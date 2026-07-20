[CmdletBinding()]
param(
    [ValidateSet('Quick', 'Full', 'Build')][string]$Mode = 'Quick',
    [ValidateSet('Editor', 'Game', 'Package')][string]$Target = 'Editor',
    [ValidateSet('Development', 'Shipping')][string]$Configuration = 'Development',
    [string]$EngineRoot,
    [switch]$NoReopenEditor
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$projectFile = Join-Path $projectRoot 'ProjectA.uproject'
$runId = [Guid]::NewGuid().ToString('D')
$runStarted = [DateTime]::UtcNow
$timestamp = $runStarted.ToString('yyyyMMddTHHmmssZ')
$runRoot = Join-Path $projectRoot ("Saved\Automation\LocalBuildRuns\$timestamp-$runId")
$reportsRoot = Join-Path $runRoot 'reports'
$summaryPath = Join-Path $runRoot 'summary.json'
New-Item -ItemType Directory -Force -Path $reportsRoot | Out-Null

$modulePath = Join-Path $PSScriptRoot 'ProjectRift.LocalBuild.psm1'
Import-Module -Force -Name $modulePath

$summary = [ordered]@{
    SchemaVersion = 1
    ProjectVersion = '0.7.2'
    RunId = $runId
    Mode = $Mode
    Target = $Target
    Configuration = $Configuration
    UnrealEngineVersion = $null
    EngineManifest = $null
    StartedUtc = $runStarted.ToString('o')
    EndedUtc = $null
    DurationSeconds = 0
    OverallStatus = 'Running'
    FailedStage = $null
    Stages = @()
    Package = $null
    Gauntlet = [ordered]@{ Stage = $null; RunId = $runId; Result = $null }
}

$exitCode = 0
$editorWasOpen = $false
$resolvedEngine = $null
$fallbackStage = 'Prerequisite'
$fallbackExitCode = 2
$activeCandidate = $null
$activeCandidateRoot = $null
$engineManifestGuard = $null

function Write-LocalSummary {
    $ended = [DateTime]::UtcNow
    $summary.EndedUtc = $ended.ToString('o')
    $summary.DurationSeconds = [Math]::Round(($ended - $runStarted).TotalSeconds, 3)
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
}

function Add-LocalStage {
    param(
        [string]$Name,
        [int]$NativeExitCode,
        [double]$DurationSeconds,
        [string]$Log,
        [int]$PassedTests = 0,
        [int]$FailedTests = 0
    )
    $summary.Stages += [ordered]@{
        Name = $Name
        ExitCode = $NativeExitCode
        DurationSeconds = $DurationSeconds
        Log = $Log
        PassedTests = $PassedTests
        FailedTests = $FailedTests
    }
}

function Stop-LocalPipeline {
    param([string]$Stage, [int]$Code, [string]$Message)
    $summary.FailedStage = $Stage
    $summary.OverallStatus = 'Failed'
    $script:exitCode = $Code
    throw [InvalidOperationException]::new($Message)
}

function Assert-LocalEngineManifest {
    if ($null -eq $engineManifestGuard) {
        Stop-LocalPipeline -Stage 'Prerequisite' -Code (Get-ProjectRiftExitCode Prerequisite) -Message 'The shared UE manifest guard was not initialized.'
    }
    $state = Test-ProjectRiftEngineManifestGuard -Guard $engineManifestGuard
    $summary.EngineManifest.Preserved = [bool]$state.IsValid
    $summary.EngineManifest.Diagnostic = $state.Diagnostic
    if (-not $state.IsValid) {
        Stop-LocalPipeline -Stage 'Prerequisite' -Code (Get-ProjectRiftExitCode Prerequisite) -Message $state.Diagnostic
    }
}

function Invoke-LocalEditorBuild {
    $arguments = New-ProjectRiftBuildArguments -ProjectFile $projectFile -Target 'Editor' -Configuration 'Development'
    $result = Invoke-ProjectRiftNative -FilePath (Join-Path $resolvedEngine.Root 'Engine\Build\BatchFiles\Build.bat') -ArgumentList $arguments -LogPath (Join-Path $runRoot 'editor-build.log') -WorkingDirectory $projectRoot
    Assert-LocalEngineManifest
    Add-LocalStage -Name 'EditorBuild' -NativeExitCode $result.ExitCode -DurationSeconds $result.DurationSeconds -Log $result.Log
    if ($result.ExitCode -ne 0) {
        Stop-LocalPipeline -Stage 'EditorBuild' -Code (Get-ProjectRiftExitCode Build) -Message 'ProjectAEditor Development build failed.'
    }
}

function Invoke-LocalAutomation {
    param([string]$Filter, [string]$Name)
    $report = Join-Path $reportsRoot $Name
    New-Item -ItemType Directory -Force -Path $report | Out-Null
    $arguments = @(
        $projectFile,
        '-Unattended', '-NullRHI', '-NoSound', '-NoSplash',
        "-ExecCmds=Automation RunTests $Filter;Quit",
        '-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$report",
        '-Log'
    )
    $logPath = Join-Path $runRoot 'automation.log'
    if ($Name -ne 'quick') { $logPath = Join-Path $runRoot 'regression.log' }
    $result = Invoke-ProjectRiftNative -FilePath (Join-Path $resolvedEngine.Root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -ArgumentList $arguments -LogPath $logPath -WorkingDirectory $projectRoot
    Assert-LocalEngineManifest
    try { $parsed = Read-ProjectRiftAutomationReport -ReportRoot $report } catch {
        Add-LocalStage -Name "Automation:$Filter" -NativeExitCode $result.ExitCode -DurationSeconds $result.DurationSeconds -Log $result.Log -FailedTests 1
        Stop-LocalPipeline -Stage 'Automation' -Code (Get-ProjectRiftExitCode Automation) -Message $_.Exception.Message
    }
    Add-LocalStage -Name "Automation:$Filter" -NativeExitCode $result.ExitCode -DurationSeconds $result.DurationSeconds -Log $result.Log -PassedTests $parsed.Passed -FailedTests ($parsed.Failed + $parsed.NotRun)
    if ($result.ExitCode -ne 0 -or -not $parsed.IsSuccess) {
        Stop-LocalPipeline -Stage 'Automation' -Code (Get-ProjectRiftExitCode Automation) -Message "Automation filter '$Filter' failed."
    }
}

function Invoke-LocalPackage {
    param([string]$PackageConfiguration)
    $null = Remove-ProjectRiftStaleCookStoreMarker -ProjectRoot $projectRoot
    $localRoot = Join-Path $projectRoot 'Saved\StagedBuilds\Local'
    $candidate = Join-Path $localRoot ('.Candidate-{0}-{1}' -f $PackageConfiguration, $runId)
    New-Item -ItemType Directory -Force -Path $localRoot | Out-Null
    if (-not (Test-ProjectRiftContainedPath -Candidate $candidate -AllowedRoot $localRoot)) {
        Stop-LocalPipeline -Stage 'Package' -Code (Get-ProjectRiftExitCode Package) -Message 'Calculated package candidate path is unsafe.'
    }
    if (Test-Path -LiteralPath $candidate) {
        Remove-ProjectRiftSafeDirectory -Path $candidate -AllowedRoot $localRoot
    }
    New-Item -ItemType Directory -Force -Path $candidate | Out-Null
    $script:activeCandidate = $candidate
    $script:activeCandidateRoot = $localRoot

    $arguments = New-ProjectRiftPackageArguments -ProjectFile $projectFile -Configuration $PackageConfiguration -ArchiveDirectory $candidate
    $result = Invoke-ProjectRiftNative -FilePath (Join-Path $resolvedEngine.Root 'Engine\Build\BatchFiles\RunUAT.bat') -ArgumentList $arguments -LogPath (Join-Path $runRoot 'package.log') -WorkingDirectory $projectRoot
    Assert-LocalEngineManifest
    Add-LocalStage -Name "Package:$PackageConfiguration" -NativeExitCode $result.ExitCode -DurationSeconds $result.DurationSeconds -Log $result.Log
    if ($result.ExitCode -ne 0) {
        Stop-LocalPipeline -Stage 'Package' -Code (Get-ProjectRiftExitCode Package) -Message "ProjectA $PackageConfiguration package failed."
    }
    return [pscustomobject]@{ Candidate = $candidate; LocalRoot = $localRoot }
}

function Invoke-LocalGauntlet {
    param([string]$PackageCandidate)
    $userDir = Join-Path $projectRoot ("Saved\Automation\Gauntlet\$runId\UserDir")
    New-Item -ItemType Directory -Force -Path $userDir | Out-Null
    $testSpec = "ProjectRift.LocalSmoke(RunId='$runId',UserDir='$userDir')"
    $env:UE_ENGINE_ROOT = $resolvedEngine.Root
    $arguments = @(
        '-WaitForUATMutex',
        "-ScriptDir=$(Join-Path $PSScriptRoot 'Gauntlet')",
        'RunUnreal',
        "-project=$projectFile", '-platform=Win64', '-configuration=Development',
        "-build=$PackageCandidate", "-test=$testSpec"
    )
    $result = Invoke-ProjectRiftNative -FilePath (Join-Path $resolvedEngine.Root 'Engine\Build\BatchFiles\RunUAT.bat') -ArgumentList $arguments -LogPath (Join-Path $runRoot 'gauntlet.log') -WorkingDirectory $projectRoot
    Assert-LocalEngineManifest
    $logText = ''
    if (Test-Path -LiteralPath $result.Log) { $logText = Get-Content -Raw -LiteralPath $result.Log }
    $stageMatches = [regex]::Matches($logText, 'PROJECTRIFT_SMOKE_STAGE=([^\s]+)')
    if ($stageMatches.Count -gt 0) { $summary.Gauntlet.Stage = $stageMatches[$stageMatches.Count - 1].Groups[1].Value }
    if ($logText -match 'PROJECTRIFT_SMOKE_RESULT=PASS') { $summary.Gauntlet.Result = 'PASS' } else { $summary.Gauntlet.Result = 'FAIL' }
    Add-LocalStage -Name 'Gauntlet:ProjectRift.LocalSmoke' -NativeExitCode $result.ExitCode -DurationSeconds $result.DurationSeconds -Log $result.Log
    if ($result.ExitCode -ne 0 -or $summary.Gauntlet.Result -ne 'PASS') {
        Stop-LocalPipeline -Stage 'Gauntlet' -Code (Get-ProjectRiftExitCode Gauntlet) -Message 'ProjectRift.LocalSmoke did not produce a successful Gauntlet result.'
    }
}

function Invoke-LocalShippingLaunchCheck {
    param([string]$PackageRoot)
    $packageInfo = Get-ProjectRiftPackageInfo -PackageRoot $PackageRoot
    $log = Join-Path $runRoot 'shipping-launch.log'
    $arguments = @('-NullRHI', '-NoSound', '-Unattended', '-NoSplash')
    $started = [DateTime]::UtcNow
    "PROJECTRIFT_SHIPPING_PROBE=START Executable=$($packageInfo.Executable)" | Set-Content -LiteralPath $log -Encoding UTF8
    $process = Start-Process -FilePath $packageInfo.Executable -ArgumentList $arguments -PassThru -WindowStyle Hidden
    if ($process.WaitForExit(10000)) {
        $probeCode = $process.ExitCode
        "PROJECTRIFT_SHIPPING_PROBE=EXIT ExitCode=$probeCode" | Add-Content -LiteralPath $log -Encoding UTF8
        Add-LocalStage -Name 'ShippingLaunch' -NativeExitCode $probeCode -DurationSeconds ([Math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 3)) -Log $log
        if ($probeCode -ne 0) {
            Stop-LocalPipeline -Stage 'Package' -Code (Get-ProjectRiftExitCode Package) -Message 'Shipping NullRHI startup check exited with an error.'
        }
        return
    }

    # A Shipping build may compile out console/logging, so a healthy ten-second
    # NullRHI lifetime is the bounded startup signal. Stop only the exact probe
    # launcher and its verified child executable inside this candidate package.
    $ownedProbeIds = @([int]$process.Id)
    $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $($process.Id)")
    foreach ($child in $children) {
        if (-not $child.ExecutablePath -or -not $child.ExecutablePath.StartsWith($packageInfo.Path + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            Add-LocalStage -Name 'ShippingLaunch' -NativeExitCode 1 -DurationSeconds ([Math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 3)) -Log $log
            Stop-LocalPipeline -Stage 'Package' -Code (Get-ProjectRiftExitCode Package) -Message 'Shipping probe created an unrecognized child process; no process was stopped.'
        }
        $ownedProbeIds += [int]$child.ProcessId
    }
    foreach ($ownedId in ($ownedProbeIds | Sort-Object -Descending)) {
        Stop-Process -Id $ownedId -Force -ErrorAction Stop
    }
    "PROJECTRIFT_SHIPPING_PROBE=PASS AliveSeconds=10 StoppedIds=$($ownedProbeIds -join ',')" | Add-Content -LiteralPath $log -Encoding UTF8
    Add-LocalStage -Name 'ShippingLaunch' -NativeExitCode 0 -DurationSeconds ([Math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 3)) -Log $log
}

try {
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
        Stop-LocalPipeline -Stage 'Prerequisite' -Code (Get-ProjectRiftExitCode Prerequisite) -Message 'ProjectA.uproject is missing.'
    }
    if ($Mode -ne 'Build' -and ($Target -ne 'Editor' -or $Configuration -ne 'Development')) {
        Stop-LocalPipeline -Stage 'Prerequisite' -Code (Get-ProjectRiftExitCode Prerequisite) -Message 'Quick and Full always use Editor/Development; Target and Configuration overrides require Mode Build.'
    }
    if ($Mode -eq 'Build' -and $Target -eq 'Editor' -and $Configuration -eq 'Shipping') {
        Stop-LocalPipeline -Stage 'Prerequisite' -Code (Get-ProjectRiftExitCode Prerequisite) -Message 'Shipping Editor builds are not supported.'
    }

    try {
        $resolvedEngine = Resolve-ProjectRiftEngineRoot -ExplicitRoot $EngineRoot -ProjectRoot $projectRoot
        $summary.UnrealEngineVersion = $resolvedEngine.Version
        $engineManifestGuard = New-ProjectRiftEngineManifestGuard -EngineRoot $resolvedEngine.Root -BackupRoot (Join-Path $runRoot 'engine-metadata-backup') -AllowedProjectRoot $projectRoot
        $summary.EngineManifest = [ordered]@{
            Path = $engineManifestGuard.ManifestPath
            Backup = $engineManifestGuard.BackupPath
            Sha256 = $engineManifestGuard.Sha256
            Length = $engineManifestGuard.Length
            Preserved = $true
            Diagnostic = 'Shared UE manifest was backed up and verified.'
        }
    } catch {
        Stop-LocalPipeline -Stage 'Prerequisite' -Code (Get-ProjectRiftExitCode Prerequisite) -Message $_.Exception.Message
    }

    $fallbackStage = 'EditorClose'
    $fallbackExitCode = Get-ProjectRiftExitCode EditorClose
    $close = Close-ProjectRiftEditor -ProjectFile $projectFile -TimeoutSeconds 30
    $editorWasOpen = $close.WasOpen
    if (-not $close.Closed) {
        Stop-LocalPipeline -Stage 'EditorClose' -Code (Get-ProjectRiftExitCode EditorClose) -Message 'ProjectA editor did not close within 30 seconds; no process was force-killed.'
    }

    if ($Mode -eq 'Quick') {
        $fallbackStage = 'Build'
        $fallbackExitCode = Get-ProjectRiftExitCode Build
        Invoke-LocalEditorBuild
        $fallbackStage = 'Automation'
        $fallbackExitCode = Get-ProjectRiftExitCode Automation
        Invoke-LocalAutomation -Filter 'ProjectRift.Smoke' -Name 'quick'
    } elseif ($Mode -eq 'Full') {
        $fallbackStage = 'Build'
        $fallbackExitCode = Get-ProjectRiftExitCode Build
        Invoke-LocalEditorBuild
        $fallbackStage = 'Automation'
        $fallbackExitCode = Get-ProjectRiftExitCode Automation
        Invoke-LocalAutomation -Filter 'ProjectRift' -Name 'regression'
        $fallbackStage = 'Package'
        $fallbackExitCode = Get-ProjectRiftExitCode Package
        $package = Invoke-LocalPackage -PackageConfiguration 'Development'
        $fallbackStage = 'Gauntlet'
        $fallbackExitCode = Get-ProjectRiftExitCode Gauntlet
        Invoke-LocalGauntlet -PackageCandidate $package.Candidate
        $fallbackStage = 'Package'
        $fallbackExitCode = Get-ProjectRiftExitCode Package
        $destination = Join-Path $package.LocalRoot 'Development'
        Publish-ProjectRiftPackage -CandidatePath $package.Candidate -DestinationPath $destination -AllowedRoot $package.LocalRoot -RunId $runId
        $activeCandidate = $null
        $activeCandidateRoot = $null
        $summary.Package = Get-ProjectRiftPackageInfo -PackageRoot $destination
    } elseif ($Target -eq 'Package') {
        $fallbackStage = 'Package'
        $fallbackExitCode = Get-ProjectRiftExitCode Package
        $package = Invoke-LocalPackage -PackageConfiguration $Configuration
        if ($Configuration -eq 'Development') {
            $fallbackStage = 'Gauntlet'
            $fallbackExitCode = Get-ProjectRiftExitCode Gauntlet
            Invoke-LocalGauntlet -PackageCandidate $package.Candidate
        } else {
            $exclusions = Test-ProjectRiftShippingExclusions -PackageRoot $package.Candidate
            if (-not $exclusions.IsValid) {
                Stop-LocalPipeline -Stage 'Package' -Code (Get-ProjectRiftExitCode Package) -Message ("Shipping package contains test modules: " + ($exclusions.ForbiddenPaths -join ', '))
            }
            Invoke-LocalShippingLaunchCheck -PackageRoot $package.Candidate
        }
        $fallbackStage = 'Package'
        $fallbackExitCode = Get-ProjectRiftExitCode Package
        $destination = Join-Path $package.LocalRoot $Configuration
        Publish-ProjectRiftPackage -CandidatePath $package.Candidate -DestinationPath $destination -AllowedRoot $package.LocalRoot -RunId $runId
        $activeCandidate = $null
        $activeCandidateRoot = $null
        $summary.Package = Get-ProjectRiftPackageInfo -PackageRoot $destination
    } else {
        $fallbackStage = 'Build'
        $fallbackExitCode = Get-ProjectRiftExitCode Build
        $arguments = New-ProjectRiftBuildArguments -ProjectFile $projectFile -Target $Target -Configuration $Configuration
        $logName = if ($Target -eq 'Editor') { 'editor-build.log' } else { 'game-build.log' }
        $result = Invoke-ProjectRiftNative -FilePath (Join-Path $resolvedEngine.Root 'Engine\Build\BatchFiles\Build.bat') -ArgumentList $arguments -LogPath (Join-Path $runRoot $logName) -WorkingDirectory $projectRoot
        Assert-LocalEngineManifest
        Add-LocalStage -Name "Build:${Target}:${Configuration}" -NativeExitCode $result.ExitCode -DurationSeconds $result.DurationSeconds -Log $result.Log
        if ($result.ExitCode -ne 0) {
            Stop-LocalPipeline -Stage 'Build' -Code (Get-ProjectRiftExitCode Build) -Message "ProjectA $Target $Configuration build failed."
        }
    }

    $summary.OverallStatus = 'Succeeded'
} catch {
    if ($exitCode -eq 0) {
        $exitCode = $fallbackExitCode
        $summary.FailedStage = $fallbackStage
        $summary.OverallStatus = 'Failed'
    }
    Write-Error $_.Exception.Message -ErrorAction Continue
} finally {
    if ($exitCode -ne 0 -and -not [string]::IsNullOrWhiteSpace($activeCandidate) -and -not [string]::IsNullOrWhiteSpace($activeCandidateRoot) -and (Test-Path -LiteralPath $activeCandidate)) {
        try {
            Remove-ProjectRiftSafeDirectory -Path $activeCandidate -AllowedRoot $activeCandidateRoot
        } catch {
            Write-Warning "Failed to remove invalid ProjectA package candidate '$activeCandidate': $($_.Exception.Message)"
        }
    }
    Write-LocalSummary
    if ($exitCode -eq 0 -and $editorWasOpen -and -not $NoReopenEditor -and $null -ne $resolvedEngine) {
        Start-ProjectRiftEditor -EditorExecutable (Join-Path $resolvedEngine.Root 'Engine\Binaries\Win64\UnrealEditor.exe') -ProjectFile $projectFile
    }
    Write-Host "ProjectRift local pipeline summary: $summaryPath"
}

exit $exitCode
