[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { $failures.Add($Message) }
}

function Assert-Equal {
    param($Actual, $Expected, [string]$Message)
    if ($Actual -ne $Expected) { $failures.Add("$Message Expected '$Expected'; got '$Actual'.") }
}

function Invoke-WallDoorRunner {
    param(
        [Parameter(Mandatory)][string]$RunnerPath,
        [Parameter(Mandatory)][string]$Stage,
        [Parameter(Mandatory)][string]$BlenderExe,
        [string]$PythonExe
    )

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $RunnerPath, '-Stage', $Stage, '-BlenderExe', $BlenderExe)
        if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
            $arguments += @('-PythonExe', $PythonExe)
        }
        $output = @(& powershell @arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Text = ($output | ForEach-Object { $_.ToString() }) -join "`n"
    }
}

try {
    $projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path
    $runnerPath = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubWallDoor.ps1'
    $previewPath = Join-Path $projectRoot 'Saved\Automation\ProjectRiftShipHubWallDoor\contract-preview.json'
    $blenderExe = 'D:\Blender5.2\blender.exe'
    $pythonExe = 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'

    Assert-True (Test-Path -LiteralPath $runnerPath -PathType Leaf) "Wall-door runner is missing: $runnerPath"
    if (Test-Path -LiteralPath $runnerPath -PathType Leaf) {
        $preflight = Invoke-WallDoorRunner -RunnerPath $runnerPath -Stage 'Preflight' -BlenderExe $blenderExe -PythonExe $pythonExe
        Assert-Equal $preflight.ExitCode 0 "Preflight must succeed. $($preflight.Text)"

        Push-Location -LiteralPath ([IO.Path]::GetTempPath())
        try {
            $outsideProjectPreflight = Invoke-WallDoorRunner -RunnerPath $runnerPath -Stage 'Preflight' -BlenderExe $blenderExe -PythonExe $pythonExe
        }
        finally {
            Pop-Location
        }
        Assert-Equal $outsideProjectPreflight.ExitCode 0 "Preflight must not depend on the caller working directory. $($outsideProjectPreflight.Text)"

        $validation = Invoke-WallDoorRunner -RunnerPath $runnerPath -Stage 'ValidateContract' -BlenderExe $blenderExe -PythonExe $pythonExe
        Assert-Equal $validation.ExitCode 0 "ValidateContract must succeed. $($validation.Text)"
        $defaultPythonValidation = Invoke-WallDoorRunner -RunnerPath $runnerPath -Stage 'ValidateContract' -BlenderExe $blenderExe
        Assert-Equal $defaultPythonValidation.ExitCode 0 "ValidateContract must resolve its optional Python executable. $($defaultPythonValidation.Text)"
        Assert-True (Test-Path -LiteralPath $previewPath -PathType Leaf) "ValidateContract must write only its deterministic preview: $previewPath"
        if (Test-Path -LiteralPath $previewPath -PathType Leaf) {
            $preview = Get-Content -LiteralPath $previewPath -Raw -Encoding UTF8 | ConvertFrom-Json
            Assert-Equal $preview.AssetId 'SM_ShipHub_WallDoor_400_A' 'Preview asset identifier mismatch.'
            Assert-Equal $preview.Stage 'G3' 'Preview stage mismatch.'
        }

        $futureStages = @{
            BuildAppearance = 'Task 10 (Appearance) has not run'
            BuildProduction = 'Task 11 (Production) has not run'
            BakeTextures = 'Task 12 (Texture Bake) has not run'
            Export = 'Task 13 (Export) has not run'
            ValidatePackage = 'Task 14 (Package Validation) has not run'
            AllDCC = 'Task 10 (Appearance) has not run'
        }
        foreach ($stage in $futureStages.Keys) {
            $result = Invoke-WallDoorRunner -RunnerPath $runnerPath -Stage $stage -BlenderExe $blenderExe -PythonExe $pythonExe
            Assert-True ($result.ExitCode -ne 0) "$stage must fail closed in Task 9."
            Assert-True ($result.Text -match [regex]::Escape($futureStages[$stage])) "$stage must name its unavailable owner. Output: $($result.Text)"
        }

        $firstArticleRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\FirstArticle\WallDoor_400_A'
        $unexpectedArtifacts = @(
            Get-ChildItem -LiteralPath $firstArticleRoot -Recurse -File -Force |
                Where-Object { $_.Extension -in @('.blend', '.fbx', '.glb', '.png', '.tga', '.uasset') }
        )
        Assert-Equal $unexpectedArtifacts.Count 0 'Task 9 must not create DCC, texture, export, or UE artifacts.'
    }
}
catch {
    $failures.Add("Unexpected wall-door runner test error: $($_.Exception.Message)")
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ -ErrorAction Continue }
    Write-Output 'ProjectRift ShipHub wall-door contract runner self-test: FAIL'
    exit 1
}

Write-Output 'ProjectRift ShipHub wall-door contract runner self-test: PASS'
exit 0
