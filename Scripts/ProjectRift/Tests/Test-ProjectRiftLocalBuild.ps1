[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$script:FailureCount = 0
$script:AssertionCount = 0
$script:ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$modulePath = Join-Path $PSScriptRoot '..\ProjectRift.LocalBuild.psm1'
$testRoot = Join-Path $script:ProjectRoot 'Saved\Automation\PowerShellSelfTest'

function Assert-True {
    param([bool]$Condition, [string]$Message)
    $script:AssertionCount++
    if (-not $Condition) {
        $script:FailureCount++
        Write-Error "ASSERTION FAILED: $Message" -ErrorAction Continue
    }
}

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)
    Assert-True -Condition ($Expected -eq $Actual) -Message "$Message (expected '$Expected', actual '$Actual')"
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Message)
    $threw = $false
    try { & $Action } catch { $threw = $true }
    Assert-True -Condition $threw -Message $Message
}

Import-Module -Force -Name $modulePath

if (Test-Path -LiteralPath $testRoot) {
    Remove-Item -LiteralPath $testRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

try {
    $fakeEngine = Join-Path $testRoot 'UE_5.8'
    $requiredFiles = @(
        'Engine\Build\Build.version',
        'Engine\Build\BatchFiles\Build.bat',
        'Engine\Build\BatchFiles\RunUAT.bat',
        'Engine\Binaries\Win64\UnrealEditor.exe',
        'Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
        'Engine\Binaries\Win64\UnrealEditor.modules'
    )
    foreach ($relative in $requiredFiles) {
        $path = Join-Path $fakeEngine $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
        if ($relative -eq 'Engine\Build\Build.version') {
            Set-Content -LiteralPath $path -Encoding UTF8 -Value '{"MajorVersion":5,"MinorVersion":8,"PatchVersion":0}'
        } else {
            Set-Content -LiteralPath $path -Encoding Ascii -Value 'self-test placeholder'
        }
    }

    $resolved = Resolve-ProjectRiftEngineRoot -ExplicitRoot $fakeEngine -ProjectRoot $script:ProjectRoot
    Assert-Equal ([IO.Path]::GetFullPath($fakeEngine)) $resolved.Root 'Explicit EngineRoot should resolve first.'
    Assert-Equal '5.8.0' $resolved.Version 'Engine version should be read from Build.version.'

    $guard = New-ProjectRiftEngineManifestGuard -EngineRoot $fakeEngine -BackupRoot (Join-Path $testRoot 'ManifestBackup') -AllowedProjectRoot $testRoot
    $guardState = Test-ProjectRiftEngineManifestGuard -Guard $guard
    Assert-True $guardState.IsValid 'A readback-verified ProjectA backup should guard the shared UE manifest.'

    $fakeManifest = Join-Path $fakeEngine 'Engine\Binaries\Win64\UnrealEditor.modules'
    Remove-Item -LiteralPath $fakeManifest -Force
    Assert-Throws { Resolve-ProjectRiftEngineRoot -ExplicitRoot $fakeEngine -ProjectRoot $script:ProjectRoot } 'A missing shared UnrealEditor.modules manifest must fail before any build starts.'
    Set-Content -LiteralPath $fakeManifest -Encoding UTF8 -Value '{"BuildId":"self-test","Modules":{}}'
    $mutatedGuardState = Test-ProjectRiftEngineManifestGuard -Guard $guard
    Assert-True (-not $mutatedGuardState.IsValid) 'A changed shared UE manifest must be detected before another stage runs.'

    $oldEngineRoot = $env:UE_ENGINE_ROOT
    try {
        $env:UE_ENGINE_ROOT = $fakeEngine
        $resolvedFromEnvironment = Resolve-ProjectRiftEngineRoot -ProjectRoot $script:ProjectRoot -SolutionPath (Join-Path $testRoot 'missing.sln')
        Assert-Equal ([IO.Path]::GetFullPath($fakeEngine)) $resolvedFromEnvironment.Root 'UE_ENGINE_ROOT should be the second engine source.'
    } finally {
        $env:UE_ENGINE_ROOT = $oldEngineRoot
    }

    $fakeSolution = Join-Path $testRoot 'ProjectA.sln'
    Set-Content -LiteralPath $fakeSolution -Encoding UTF8 -Value ("Project = `"$fakeEngine\Engine\Source\Programs\UnrealBuildTool\UnrealBuildTool.csproj`"")
    try {
        $env:UE_ENGINE_ROOT = $null
        $resolvedFromSolution = Resolve-ProjectRiftEngineRoot -ProjectRoot $script:ProjectRoot -SolutionPath $fakeSolution
        Assert-Equal ([IO.Path]::GetFullPath($fakeEngine)) $resolvedFromSolution.Root 'ProjectA.sln should be the final bounded engine source.'
    } finally {
        $env:UE_ENGINE_ROOT = $oldEngineRoot
    }

    $badEngine = Join-Path $testRoot 'UE_5.7'
    New-Item -ItemType Directory -Force -Path (Join-Path $badEngine 'Engine\Build') | Out-Null
    Set-Content -LiteralPath (Join-Path $badEngine 'Engine\Build\Build.version') -Encoding UTF8 -Value '{"MajorVersion":5,"MinorVersion":7,"PatchVersion":0}'
    Assert-Throws { Resolve-ProjectRiftEngineRoot -ExplicitRoot $badEngine -ProjectRoot $script:ProjectRoot } 'A non-5.8 EngineRoot must be rejected.'

    $projectFile = Join-Path $script:ProjectRoot 'ProjectA.uproject'
    $quotedCommandLine = "`"D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe`" `"$projectFile`""
    Assert-True (Test-ProjectRiftEditorProcess -Name 'UnrealEditor.exe' -CommandLine $quotedCommandLine -ProjectFile $projectFile) 'Exact ProjectA editor command line should match.'
    Assert-True (-not (Test-ProjectRiftEditorProcess -Name 'UnrealEditor.exe' -CommandLine 'UnrealEditor.exe E:\Other\Other.uproject' -ProjectFile $projectFile)) 'Another project editor must not match.'
    Assert-True (-not (Test-ProjectRiftEditorProcess -Name 'UnrealEditor-Cmd.exe' -CommandLine $quotedCommandLine -ProjectFile $projectFile)) 'Commandlet process must not be treated as the interactive editor.'
    $editorFilterCommand = Get-Command New-ProjectRiftEditorProcessFilter -ErrorAction SilentlyContinue
    Assert-True ($null -ne $editorFilterCommand) 'Editor discovery must build a ProjectA-path WMI filter instead of enumerating every UnrealEditor process.'
    if ($null -ne $editorFilterCommand) {
        $editorFilter = New-ProjectRiftEditorProcessFilter -ProjectFile $projectFile
        Assert-True ($editorFilter -match "CommandLine LIKE '%E:\\\\MyWork\\\\ProjectA\\\\ProjectA\.uproject%'") 'The WMI filter must contain the escaped canonical ProjectA path.'
        Assert-True ($editorFilter -match "CommandLine LIKE '%E:/MyWork/ProjectA/ProjectA\.uproject%'") 'The WMI filter must also recognize canonical ProjectA paths that use forward slashes.'
    }

    $allowedRoot = Join-Path $testRoot 'Allowed'
    $childPath = Join-Path $allowedRoot 'Development\Candidate'
    New-Item -ItemType Directory -Force -Path $childPath | Out-Null
    Assert-True (Test-ProjectRiftContainedPath -Candidate $childPath -AllowedRoot $allowedRoot) 'A normal child path should be accepted.'
    Assert-True (-not (Test-ProjectRiftContainedPath -Candidate $allowedRoot -AllowedRoot $allowedRoot)) 'The allowed root itself must be rejected.'
    Assert-True (-not (Test-ProjectRiftContainedPath -Candidate (Join-Path $testRoot 'Outside') -AllowedRoot $allowedRoot)) 'A sibling path must be rejected.'

    Assert-Equal 2 (Get-ProjectRiftExitCode -Stage 'Prerequisite') 'Prerequisite exit code contract.'
    Assert-Equal 3 (Get-ProjectRiftExitCode -Stage 'EditorClose') 'Editor close exit code contract.'
    Assert-Equal 10 (Get-ProjectRiftExitCode -Stage 'Build') 'Build exit code contract.'
    Assert-Equal 11 (Get-ProjectRiftExitCode -Stage 'Automation') 'Automation exit code contract.'
    Assert-Equal 12 (Get-ProjectRiftExitCode -Stage 'Package') 'Package exit code contract.'
    Assert-Equal 13 (Get-ProjectRiftExitCode -Stage 'Gauntlet') 'Gauntlet exit code contract.'

    $reportRoot = Join-Path $testRoot 'Report'
    New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $reportRoot 'index.json') -Encoding UTF8 -Value '{"succeeded":4,"succeededWithWarnings":1,"failed":0,"notRun":0}'
    $report = Read-ProjectRiftAutomationReport -ReportRoot $reportRoot
    Assert-Equal 5 $report.Passed 'Automation report passed count should include warning successes.'
    Assert-Equal 0 $report.Failed 'Automation report failed count.'
    Assert-True $report.IsSuccess 'Zero failed and not-run tests should be successful.'

    $buildArguments = New-ProjectRiftBuildArguments -ProjectFile $projectFile -Target 'Editor' -Configuration 'Development'
    Assert-True ($buildArguments -is [Array]) 'Native build arguments must be returned as an array.'
    Assert-True ($buildArguments -contains '-WaitMutex') 'Native build arguments should include WaitMutex.'
    Assert-True (($buildArguments -join ' ') -notmatch 'Invoke-Expression') 'Native argument generation must not use Invoke-Expression.'

    $shippingCandidate = Join-Path $testRoot 'ShippingCandidate'
    New-Item -ItemType Directory -Force -Path $shippingCandidate | Out-Null
    $shippingCheck = Test-ProjectRiftShippingExclusions -PackageRoot $shippingCandidate
    Assert-True $shippingCheck.IsValid 'An empty Shipping candidate has no Development test modules.'
    Assert-Equal 0 $shippingCheck.ForbiddenPaths.Count 'An empty Shipping exclusion list must remain a valid empty array in strict mode.'
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

if ($script:FailureCount -gt 0) {
    Write-Host "ProjectRift PowerShell self-test: FAIL ($script:FailureCount/$script:AssertionCount assertions failed)."
    exit 1
}

Write-Host "ProjectRift PowerShell self-test: PASS ($script:AssertionCount assertions)."
exit 0
