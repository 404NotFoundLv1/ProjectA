[CmdletBinding()]
param(
    [ValidateSet('Preflight', 'ValidateContract', 'BuildAppearance', 'BuildProduction', 'BakeTextures', 'Export', 'ValidatePackage', 'AllDCC')]
    [string]$Stage = 'Preflight',
    [string]$BlenderExe,
    [string]$PythonExe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path
$modulePath = Join-Path $PSScriptRoot 'ProjectRift.ArtPipeline.psm1'
Import-Module -Name $modulePath -Force -ErrorAction Stop

$firstArticleRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\FirstArticle\WallDoor_400_A'
$briefsRoot = Join-Path $firstArticleRoot 'Briefs'
$contractPath = Join-Path $briefsRoot 'SM_ShipHub_WallDoor_400_A.asset.json'
$approvalPath = Join-Path $briefsRoot 'SM_ShipHub_WallDoor_400_A.approval.json'
$ledgerPath = Join-Path $briefsRoot 'SM_ShipHub_WallDoor_400_A.generation-ledger.json'
$contractScript = Join-Path $PSScriptRoot 'shiphub\wall_door_contract.py'
$appearanceBuilder = Join-Path $PSScriptRoot 'shiphub\build_wall_door_first_article.py'
$productionValidator = Join-Path $PSScriptRoot 'shiphub\validate_wall_door_production.py'
$productionBlend = Join-Path $firstArticleRoot 'Blender\SM_ShipHub_WallDoor_400_A.blend'
$geometryReport = Join-Path $firstArticleRoot 'Reports\geometry-validation.json'
$referencePaths = @(
    (Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\CompleteDesign\Blender\SM_ShipHub_Complete_White_v1.blend'),
    (Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\CompleteDesign\Drawings\FinalPNG\D05_WallBayInterface.png')
)
$automationRoot = Join-Path $projectRoot 'Saved\Automation\ProjectRiftShipHubWallDoor'
$previewPath = Join-Path $automationRoot 'contract-preview.json'

function Assert-ProjectRiftContainedPath {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$AllowedRoot,
        [Parameter(Mandatory)][string]$Label
    )

    if (-not (Test-ProjectRiftContainedArtPath -Candidate $Path -AllowedRoot $AllowedRoot)) {
        throw "$Label is outside its approved root: $Path"
    }
}

function Resolve-ProjectRiftWallDoorBlender {
    $candidate = if ([string]::IsNullOrWhiteSpace($BlenderExe)) { 'D:\Blender5.2\blender.exe' } else { $BlenderExe }
    try {
        $fullPath = [IO.Path]::GetFullPath($candidate)
    }
    catch {
        throw "Blender path is invalid: $candidate"
    }
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf) -or ([IO.Path]::GetFileName($fullPath) -ine 'blender.exe')) {
        throw "Blender 5.2 LTS is unavailable at: $fullPath"
    }
    return $fullPath
}

function Resolve-ProjectRiftWallDoorPython {
    if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
        return Resolve-ProjectRiftPythonExecutable -ExplicitPath $PythonExe
    }

    $workspacePython = 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $workspacePython -PathType Leaf) {
        return Resolve-ProjectRiftPythonExecutable -ExplicitPath $workspacePython
    }
    return Resolve-ProjectRiftPythonExecutable
}

function Test-ProjectRiftWallDoorInputs {
    foreach ($sourcePath in @($contractPath, $approvalPath, $ledgerPath, $contractScript, $appearanceBuilder, $productionValidator) + $referencePaths) {
        Assert-ProjectRiftContainedPath -Path $sourcePath -AllowedRoot $projectRoot -Label 'Wall-door source path'
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Required wall-door source is missing: $sourcePath"
        }
    }
    Assert-ProjectRiftContainedPath -Path $automationRoot -AllowedRoot $projectRoot -Label 'Wall-door automation output root'
    Assert-ProjectRiftContainedPath -Path $previewPath -AllowedRoot $automationRoot -Label 'Wall-door contract preview'
}

function Invoke-ProjectRiftWallDoorAppearanceBuild {
    Test-ProjectRiftWallDoorInputs
    $blender = Resolve-ProjectRiftWallDoorBlender
    & $blender --background --factory-startup --python $appearanceBuilder -- --contract $contractPath --project-root $projectRoot --output-root $firstArticleRoot
    if ($LASTEXITCODE -ne 0) {
        throw "BuildAppearance stage failed with Blender exit code $LASTEXITCODE."
    }
    $python = Resolve-ProjectRiftWallDoorPython
    & $python $appearanceBuilder --finalize --contract $contractPath --project-root $projectRoot --output-root $firstArticleRoot
    if ($LASTEXITCODE -ne 0) {
        throw "BuildAppearance finalization failed with Python exit code $LASTEXITCODE."
    }
}

function Invoke-ProjectRiftWallDoorProductionBuild {
    Test-ProjectRiftWallDoorInputs
    $blender = Resolve-ProjectRiftWallDoorBlender
    & $blender --background --factory-startup --python $appearanceBuilder -- --production --contract $contractPath --project-root $projectRoot --output-root $firstArticleRoot
    if ($LASTEXITCODE -ne 0) {
        throw "BuildProduction stage failed with Blender exit code $LASTEXITCODE."
    }
    & $blender --background --factory-startup --python $productionValidator -- --project-root $projectRoot --output-root $firstArticleRoot --blend $productionBlend --report $geometryReport
    if ($LASTEXITCODE -ne 0) {
        throw "BuildProduction independent saved-blend validation failed with Blender exit code $LASTEXITCODE."
    }
}

function Invoke-ProjectRiftWallDoorPreflight {
    Test-ProjectRiftWallDoorInputs
    $blender = Resolve-ProjectRiftWallDoorBlender
    $blenderVersion = @(& $blender --version 2>&1)
    if ($LASTEXITCODE -ne 0 -or $blenderVersion.Count -eq 0 -or -not ([string]$blenderVersion[0] -match '^Blender 5\.2\.\d+ LTS')) {
        throw 'Preflight requires Blender 5.2.x LTS.'
    }

    $python = Resolve-ProjectRiftWallDoorPython
    & $python --version | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'Preflight Python version check failed.'
    }

    & $python -c "import sys; from pathlib import Path; sys.path.insert(0, sys.argv[3]); from Scripts.ProjectRift.ArtPipeline.shiphub.wall_door_contract import load_contract, validate_contract; contract = load_contract(Path(sys.argv[1])); issues = validate_contract(contract, Path(sys.argv[2])); print('; '.join(issues)); raise SystemExit(bool(issues))" $contractPath $projectRoot $projectRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'Preflight contract validation failed.'
    }
}

function Invoke-ProjectRiftWallDoorContractValidation {
    Test-ProjectRiftWallDoorInputs
    $python = Resolve-ProjectRiftWallDoorPython
    New-Item -ItemType Directory -Path $automationRoot -Force | Out-Null
    & $python $contractScript --contract $contractPath --project-root $projectRoot --out $previewPath
    if ($LASTEXITCODE -ne 0) {
        throw "ValidateContract stage failed with exit code $LASTEXITCODE."
    }
}

function Invoke-ProjectRiftUnavailableWallDoorStage {
    param([Parameter(Mandatory)][string]$Message)
    throw $Message
}

switch ($Stage) {
    'Preflight' { Invoke-ProjectRiftWallDoorPreflight }
    'ValidateContract' { Invoke-ProjectRiftWallDoorContractValidation }
    'BuildAppearance' { Invoke-ProjectRiftWallDoorAppearanceBuild }
    'BuildProduction' { Invoke-ProjectRiftWallDoorProductionBuild }
    'BakeTextures' { Invoke-ProjectRiftUnavailableWallDoorStage 'BakeTextures is unavailable: Task 12 (Texture Bake) has not run.' }
    'Export' { Invoke-ProjectRiftUnavailableWallDoorStage 'Export is unavailable: Task 13 (Export) has not run.' }
    'ValidatePackage' { Invoke-ProjectRiftUnavailableWallDoorStage 'ValidatePackage is unavailable: Task 14 (Package Validation) has not run.' }
    'AllDCC' {
        Invoke-ProjectRiftWallDoorPreflight
        Invoke-ProjectRiftWallDoorContractValidation
        Invoke-ProjectRiftWallDoorAppearanceBuild
        Invoke-ProjectRiftWallDoorProductionBuild
        Invoke-ProjectRiftUnavailableWallDoorStage 'BakeTextures is unavailable: Task 12 (Texture Bake) has not run.'
    }
}
