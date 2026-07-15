Set-StrictMode -Version 2.0

function Get-ProjectRiftCanonicalPath {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Path)

    return [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
}

function Test-ProjectRiftContainedPath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Candidate,
        [Parameter(Mandatory = $true)][string]$AllowedRoot
    )

    try {
        $candidatePath = Get-ProjectRiftCanonicalPath $Candidate
        $rootPath = Get-ProjectRiftCanonicalPath $AllowedRoot
    } catch {
        return $false
    }

    if ($candidatePath.Equals($rootPath, [StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }
    $rootPrefix = $rootPath + [IO.Path]::DirectorySeparatorChar
    if (-not $candidatePath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }

    $current = $candidatePath
    while ($current.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -or $current.Equals($rootPath, [StringComparison]::OrdinalIgnoreCase)) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -Force -LiteralPath $current
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                return $false
            }
        }
        if ($current.Equals($rootPath, [StringComparison]::OrdinalIgnoreCase)) {
            break
        }
        $parent = Split-Path -Parent $current
        if ([string]::IsNullOrWhiteSpace($parent) -or $parent.Equals($current, [StringComparison]::OrdinalIgnoreCase)) {
            return $false
        }
        $current = Get-ProjectRiftCanonicalPath $parent
    }
    return $true
}

function Test-ProjectRiftEngineRoot {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Root)

    $rootPath = Get-ProjectRiftCanonicalPath $Root
    $required = @(
        'Engine\Build\Build.version',
        'Engine\Build\BatchFiles\Build.bat',
        'Engine\Build\BatchFiles\RunUAT.bat',
        'Engine\Binaries\Win64\UnrealEditor.exe',
        'Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
        'Engine\Binaries\Win64\UnrealEditor.modules'
    )
    foreach ($relative in $required) {
        if (-not (Test-Path -LiteralPath (Join-Path $rootPath $relative) -PathType Leaf)) {
            throw "EngineRoot is missing required UE tooling: $relative"
        }
    }

    $version = Get-Content -Raw -LiteralPath (Join-Path $rootPath 'Engine\Build\Build.version') | ConvertFrom-Json
    if ([int]$version.MajorVersion -ne 5 -or [int]$version.MinorVersion -ne 8) {
        throw "ProjectRift local builds require UE 5.8; '$rootPath' is UE $($version.MajorVersion).$($version.MinorVersion)."
    }

    return [pscustomobject]@{
        Root = $rootPath
        Version = '{0}.{1}.{2}' -f $version.MajorVersion, $version.MinorVersion, $version.PatchVersion
    }
}

function New-ProjectRiftEngineManifestGuard {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$EngineRoot,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [Parameter(Mandatory = $true)][string]$AllowedProjectRoot
    )

    $manifestPath = Join-Path (Get-ProjectRiftCanonicalPath $EngineRoot) 'Engine\Binaries\Win64\UnrealEditor.modules'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "UE 5.8 shared module manifest is missing: $manifestPath"
    }
    if (-not (Test-ProjectRiftContainedPath -Candidate $BackupRoot -AllowedRoot $AllowedProjectRoot)) {
        throw "Engine metadata backup must remain inside ProjectA: $BackupRoot"
    }

    New-Item -ItemType Directory -Force -Path $BackupRoot | Out-Null
    $backupPath = Join-Path $BackupRoot 'UnrealEditor.modules'
    Copy-Item -LiteralPath $manifestPath -Destination $backupPath -Force
    $sourceHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    $backupHash = (Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash
    if ($sourceHash -ne $backupHash) {
        throw 'UE 5.8 module manifest backup failed its readback hash check.'
    }

    return [pscustomobject]@{
        ManifestPath = $manifestPath
        BackupPath = $backupPath
        Sha256 = $sourceHash
        Length = (Get-Item -LiteralPath $manifestPath).Length
    }
}

function Test-ProjectRiftEngineManifestGuard {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)]$Guard)

    if (-not (Test-Path -LiteralPath $Guard.ManifestPath -PathType Leaf)) {
        return [pscustomobject]@{ IsValid = $false; Diagnostic = "Shared UE manifest disappeared: $($Guard.ManifestPath)" }
    }
    if (-not (Test-Path -LiteralPath $Guard.BackupPath -PathType Leaf)) {
        return [pscustomobject]@{ IsValid = $false; Diagnostic = "ProjectA manifest backup disappeared: $($Guard.BackupPath)" }
    }
    $currentHash = (Get-FileHash -LiteralPath $Guard.ManifestPath -Algorithm SHA256).Hash
    $backupHash = (Get-FileHash -LiteralPath $Guard.BackupPath -Algorithm SHA256).Hash
    if ($currentHash -ne $Guard.Sha256 -or $backupHash -ne $Guard.Sha256) {
        return [pscustomobject]@{ IsValid = $false; Diagnostic = 'Shared UE manifest or its ProjectA backup changed during the pipeline.' }
    }
    return [pscustomobject]@{ IsValid = $true; Diagnostic = 'Shared UE manifest is unchanged.' }
}

function Resolve-ProjectRiftEngineRoot {
    [CmdletBinding()]
    param(
        [string]$ExplicitRoot,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [string]$SolutionPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitRoot)) {
        return Test-ProjectRiftEngineRoot -Root $ExplicitRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($env:UE_ENGINE_ROOT)) {
        return Test-ProjectRiftEngineRoot -Root $env:UE_ENGINE_ROOT
    }

    if ([string]::IsNullOrWhiteSpace($SolutionPath)) {
        $SolutionPath = Join-Path $ProjectRoot 'ProjectA.sln'
    }
    if (-not (Test-Path -LiteralPath $SolutionPath -PathType Leaf)) {
        throw 'UE 5.8 EngineRoot was not provided, UE_ENGINE_ROOT is unset, and ProjectA.sln is unavailable.'
    }

    $solutionText = Get-Content -Raw -LiteralPath $SolutionPath
    $match = [regex]::Match($solutionText, '(?im)([A-Z]:\\[^\r\n"]*?)\\Engine\\Source\\Programs\\UnrealBuildTool\\UnrealBuildTool\.csproj')
    if (-not $match.Success) {
        throw 'ProjectA.sln does not contain an UnrealBuildTool path from which UE 5.8 can be resolved.'
    }
    return Test-ProjectRiftEngineRoot -Root $match.Groups[1].Value
}

function Test-ProjectRiftEditorProcess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$CommandLine,
        [Parameter(Mandatory = $true)][string]$ProjectFile
    )

    if (-not $Name.Equals('UnrealEditor.exe', [StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }
    $expected = Get-ProjectRiftCanonicalPath $ProjectFile
    $tokens = [regex]::Matches($CommandLine, '"[^"]*"|\S+')
    foreach ($match in $tokens) {
        $token = $match.Value.Trim('"')
        if ($token.StartsWith('-project=', [StringComparison]::OrdinalIgnoreCase)) {
            $token = $token.Substring(9).Trim('"')
        }
        if ($token.EndsWith('.uproject', [StringComparison]::OrdinalIgnoreCase)) {
            try {
                if ((Get-ProjectRiftCanonicalPath $token).Equals($expected, [StringComparison]::OrdinalIgnoreCase)) {
                    return $true
                }
            } catch {
                continue
            }
        }
    }
    return $false
}

function ConvertTo-ProjectRiftWqlLikeLiteral {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Value)

    return $Value.Replace('[', '[[]').Replace('%', '[%]').Replace('_', '[_]').Replace('\', '\\').Replace("'", "''")
}

function New-ProjectRiftEditorProcessFilter {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$ProjectFile)

    $canonicalProject = Get-ProjectRiftCanonicalPath $ProjectFile
    $windowsProject = ConvertTo-ProjectRiftWqlLikeLiteral -Value $canonicalProject
    $forwardProject = ConvertTo-ProjectRiftWqlLikeLiteral -Value $canonicalProject.Replace('\', '/')
    return "Name = 'UnrealEditor.exe' AND (CommandLine LIKE '%$windowsProject%' OR CommandLine LIKE '%$forwardProject%')"
}

function Get-ProjectRiftEditorProcesses {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$ProjectFile)

    $filter = New-ProjectRiftEditorProcessFilter -ProjectFile $ProjectFile
    return @(Get-CimInstance Win32_Process -Filter $filter | Where-Object {
        $_.CommandLine -and (Test-ProjectRiftEditorProcess -Name $_.Name -CommandLine $_.CommandLine -ProjectFile $ProjectFile)
    })
}

function Close-ProjectRiftEditor {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$ProjectFile,
        [int]$TimeoutSeconds = 30
    )

    $owned = @(Get-ProjectRiftEditorProcesses -ProjectFile $ProjectFile)
    if ($owned.Count -eq 0) {
        return [pscustomobject]@{ WasOpen = $false; Closed = $true; ProcessIds = @() }
    }
    $ids = @($owned | ForEach-Object { [int]$_.ProcessId })
    foreach ($id in $ids) {
        try {
            $process = [Diagnostics.Process]::GetProcessById($id)
            if (-not $process.CloseMainWindow()) {
                return [pscustomobject]@{ WasOpen = $true; Closed = $false; ProcessIds = $ids }
            }
        } catch {
            if (Get-Process -Id $id -ErrorAction SilentlyContinue) {
                return [pscustomobject]@{ WasOpen = $true; Closed = $false; ProcessIds = $ids }
            }
        }
    }

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $remaining = @(Get-ProjectRiftEditorProcesses -ProjectFile $ProjectFile)
        if ($remaining.Count -eq 0) {
            return [pscustomobject]@{ WasOpen = $true; Closed = $true; ProcessIds = $ids }
        }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)

    return [pscustomobject]@{ WasOpen = $true; Closed = $false; ProcessIds = $ids }
}

function Start-ProjectRiftEditor {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$EditorExecutable,
        [Parameter(Mandatory = $true)][string]$ProjectFile
    )
    Start-Process -FilePath $EditorExecutable -ArgumentList @("`"$ProjectFile`"") -WindowStyle Hidden | Out-Null
}

function Get-ProjectRiftExitCode {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][ValidateSet('Prerequisite', 'EditorClose', 'Build', 'Automation', 'Package', 'Gauntlet')][string]$Stage)

    switch ($Stage) {
        'Prerequisite' { return 2 }
        'EditorClose' { return 3 }
        'Build' { return 10 }
        'Automation' { return 11 }
        'Package' { return 12 }
        'Gauntlet' { return 13 }
    }
}

function New-ProjectRiftBuildArguments {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$ProjectFile,
        [Parameter(Mandatory = $true)][ValidateSet('Editor', 'Game')][string]$Target,
        [Parameter(Mandatory = $true)][ValidateSet('Development', 'Shipping')][string]$Configuration
    )

    $targetName = 'ProjectA'
    if ($Target -eq 'Editor') {
        if ($Configuration -eq 'Shipping') {
            throw 'Unreal Editor does not support the Shipping configuration.'
        }
        $targetName = 'ProjectAEditor'
    }
    return @($targetName, 'Win64', $Configuration, "-Project=$ProjectFile", '-WaitMutex', '-NoHotReloadFromIDE')
}

function New-ProjectRiftPackageArguments {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$ProjectFile,
        [Parameter(Mandatory = $true)][ValidateSet('Development', 'Shipping')][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$ArchiveDirectory
    )

    return @(
        '-WaitForUATMutex',
        'BuildCookRun',
        "-project=$ProjectFile", '-nop4', '-utf8output',
        '-platform=Win64', "-clientconfig=$Configuration",
        '-build', '-cook', '-AdditionalCookerOptions=-skipzenstore', '-stage', '-pak', '-archive',
        "-archivedirectory=$ArchiveDirectory"
    )
}

function Read-ProjectRiftAutomationReport {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$ReportRoot)

    $index = Join-Path $ReportRoot 'index.json'
    if (-not (Test-Path -LiteralPath $index -PathType Leaf)) {
        throw "Automation report is missing: $index"
    }
    $json = Get-Content -Raw -LiteralPath $index | ConvertFrom-Json
    $succeeded = [int]$json.succeeded
    $warnings = [int]$json.succeededWithWarnings
    $failed = [int]$json.failed
    $notRun = [int]$json.notRun
    return [pscustomobject]@{
        Passed = $succeeded + $warnings
        Failed = $failed
        NotRun = $notRun
        IsSuccess = ($failed -eq 0 -and $notRun -eq 0 -and ($succeeded + $warnings) -gt 0)
    }
}

function Invoke-ProjectRiftNative {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][object[]]$ArgumentList,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null
    $started = [DateTime]::UtcNow
    Push-Location $WorkingDirectory
    try {
        & $FilePath @ArgumentList 2>&1 | Tee-Object -FilePath $LogPath | ForEach-Object { Write-Host $_ }
        $code = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($null -eq $code) { $code = 1 }
    return [pscustomobject]@{
        ExitCode = [int]$code
        DurationSeconds = [Math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 3)
        Log = $LogPath
    }
}

function Remove-ProjectRiftSafeDirectory {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$AllowedRoot
    )

    if (-not (Test-ProjectRiftContainedPath -Candidate $Path -AllowedRoot $AllowedRoot)) {
        throw "Refusing to delete an unsafe or reparse-point path: $Path"
    }
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Remove-ProjectRiftStaleCookStoreMarker {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $canonicalProjectRoot = Get-ProjectRiftCanonicalPath $ProjectRoot
    $marker = Join-Path $canonicalProjectRoot 'Saved\Cooked\Windows\ue.projectstore'
    if (-not (Test-ProjectRiftContainedPath -Candidate $marker -AllowedRoot $canonicalProjectRoot)) {
        throw 'Calculated ProjectA cook-store marker path is unsafe.'
    }
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        return $false
    }

    Remove-Item -LiteralPath $marker -Force
    return $true
}

function Publish-ProjectRiftPackage {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$CandidatePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][string]$AllowedRoot,
        [Parameter(Mandatory = $true)][string]$RunId
    )

    if (-not (Test-ProjectRiftContainedPath -Candidate $CandidatePath -AllowedRoot $AllowedRoot) -or
        -not (Test-ProjectRiftContainedPath -Candidate $DestinationPath -AllowedRoot $AllowedRoot)) {
        throw 'Package candidate or destination is outside ProjectA Saved/StagedBuilds/Local.'
    }
    if (-not (Test-Path -LiteralPath $CandidatePath -PathType Container)) {
        throw "Package candidate does not exist: $CandidatePath"
    }
    $backup = Join-Path $AllowedRoot ('.Previous-{0}-{1}' -f (Split-Path -Leaf $DestinationPath), $RunId)
    if (-not (Test-ProjectRiftContainedPath -Candidate $backup -AllowedRoot $AllowedRoot)) {
        throw 'Calculated package backup path is unsafe.'
    }
    if (Test-Path -LiteralPath $backup) {
        Remove-ProjectRiftSafeDirectory -Path $backup -AllowedRoot $AllowedRoot
    }

    $movedOld = $false
    try {
        if (Test-Path -LiteralPath $DestinationPath) {
            Move-Item -LiteralPath $DestinationPath -Destination $backup
            $movedOld = $true
        }
        Move-Item -LiteralPath $CandidatePath -Destination $DestinationPath
        if ($movedOld -and (Test-Path -LiteralPath $backup)) {
            Remove-ProjectRiftSafeDirectory -Path $backup -AllowedRoot $AllowedRoot
        }
    } catch {
        if (-not (Test-Path -LiteralPath $DestinationPath) -and $movedOld -and (Test-Path -LiteralPath $backup)) {
            Move-Item -LiteralPath $backup -Destination $DestinationPath
        }
        throw
    }
}

function Get-ProjectRiftPackageInfo {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$PackageRoot)

    $files = @(Get-ChildItem -LiteralPath $PackageRoot -Recurse -File)
    $executable = $files | Where-Object { $_.Name -eq 'ProjectA.exe' } | Select-Object -First 1
    if ($null -eq $executable) {
        throw "Packaged ProjectA executable was not found below $PackageRoot"
    }
    $size = ($files | Measure-Object -Property Length -Sum).Sum
    if ($null -eq $size) { $size = 0 }
    return [pscustomobject]@{
        Path = Get-ProjectRiftCanonicalPath $PackageRoot
        FileCount = $files.Count
        TotalSizeBytes = [int64]$size
        Executable = $executable.FullName
        ExecutableSha256 = (Get-FileHash -LiteralPath $executable.FullName -Algorithm SHA256).Hash
    }
}

function Test-ProjectRiftShippingExclusions {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$PackageRoot)

    $forbidden = @(Get-ChildItem -LiteralPath $PackageRoot -Recurse -Force | Where-Object {
        $_.Name -match '(?i)ProjectRiftSmokeTests|Gauntlet'
    })
    return [pscustomobject]@{
        IsValid = ($forbidden.Count -eq 0)
        ForbiddenPaths = @($forbidden | ForEach-Object { $_.FullName })
    }
}

Export-ModuleMember -Function @(
    'Close-ProjectRiftEditor',
    'Get-ProjectRiftCanonicalPath',
    'Get-ProjectRiftEditorProcesses',
    'Get-ProjectRiftExitCode',
    'Get-ProjectRiftPackageInfo',
    'Invoke-ProjectRiftNative',
    'New-ProjectRiftEditorProcessFilter',
    'New-ProjectRiftEngineManifestGuard',
    'New-ProjectRiftBuildArguments',
    'New-ProjectRiftPackageArguments',
    'Publish-ProjectRiftPackage',
    'Read-ProjectRiftAutomationReport',
    'Remove-ProjectRiftSafeDirectory',
    'Remove-ProjectRiftStaleCookStoreMarker',
    'Resolve-ProjectRiftEngineRoot',
    'Start-ProjectRiftEditor',
    'Test-ProjectRiftContainedPath',
    'Test-ProjectRiftEngineManifestGuard',
    'Test-ProjectRiftEditorProcess',
    'Test-ProjectRiftShippingExclusions'
)
