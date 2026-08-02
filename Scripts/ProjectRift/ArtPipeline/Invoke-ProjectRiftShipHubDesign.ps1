[CmdletBinding()]
param(
    [ValidateSet('Preflight','ValidateContract','BuildWhiteModel','RenderDrawings','Publish','Validate','All')]
    [string]$Stage = 'Preflight',
    [string]$BlenderExe,
    [string]$PythonExe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path
$modulePath = Join-Path $PSScriptRoot 'ProjectRift.ArtPipeline.psm1'
Import-Module -Name $modulePath -Force -ErrorAction Stop

$generatedOutputRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\CompleteDesign'
$generatedOutputProbe = Join-Path $generatedOutputRoot '_ProjectRiftShipHubDesignProbe'
if (-not (Test-ProjectRiftContainedArtPath -Candidate $generatedOutputProbe -AllowedRoot $generatedOutputRoot)) {
    throw "Generated output path is outside the approved root: $generatedOutputRoot"
}

$automationOutputRoot = Join-Path $projectRoot 'Saved\Automation\ProjectRiftShipHubDesign'
$automationOutputProbe = Join-Path $automationOutputRoot '_ProjectRiftShipHubDesignProbe'
if (-not (Test-ProjectRiftContainedArtPath -Candidate $automationOutputProbe -AllowedRoot $projectRoot)) {
    throw "Automation output path is outside ProjectA: $automationOutputRoot"
}

function Resolve-ProjectRiftValidatedBlender {
    param([Parameter(Mandatory)][string]$StageName)

    $blender = Resolve-ProjectRiftBlenderExecutable -ExplicitPath $BlenderExe
    $blenderVersionOutput = @(& $blender --version 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "$StageName Blender version check failed with code $LASTEXITCODE."
    }
    if ($blenderVersionOutput.Count -eq 0 -or -not ([string]$blenderVersionOutput[0] -match '^Blender 5\.2\.\d+ LTS')) {
        throw "$StageName requires Blender 5.2.x LTS."
    }
    return $blender
}

function Invoke-ProjectRiftPreflight {
    $blender = Resolve-ProjectRiftValidatedBlender -StageName 'Preflight'
    $python = Resolve-ProjectRiftPythonExecutable -ExplicitPath $PythonExe

    & $python -c "import PIL, reportlab, pypdf; print('ProjectRift publishing dependencies: OK')"
    if ($LASTEXITCODE -ne 0) {
        throw 'Publishing dependency preflight failed.'
    }
}

function Invoke-ProjectRiftContractValidation {
    $python = Resolve-ProjectRiftPythonExecutable -ExplicitPath $PythonExe
    $contractScript = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\shiphub\shiphub_contract.py'
    $briefPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\ShipHubCompleteDesign_v1.json'
    $layoutPreviewPath = Join-Path $automationOutputRoot 'layout-preview.json'

    if (-not (Test-Path -LiteralPath $contractScript -PathType Leaf)) {
        throw "ValidateContract stage is missing its contract script: $contractScript"
    }
    if (-not (Test-Path -LiteralPath $briefPath -PathType Leaf)) {
        throw "ValidateContract stage is missing its design brief: $briefPath"
    }

    New-Item -ItemType Directory -Path $automationOutputRoot -Force | Out-Null
    & $python $contractScript --brief $briefPath --out $layoutPreviewPath
    if ($LASTEXITCODE -ne 0) {
        throw "ValidateContract stage failed: contract validation exited with code $LASTEXITCODE."
    }
}

function Invoke-ProjectRiftBuildWhiteModel {
    $blender = Resolve-ProjectRiftValidatedBlender -StageName 'BuildWhiteModel'
    $helperScript = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\shiphub\shiphub_blender.py'
    $buildScript = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\shiphub\build_shiphub_design.py'
    $briefPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\ShipHubCompleteDesign_v1.json'
    $outputRoot = $generatedOutputRoot

    foreach ($requiredScript in @($helperScript, $buildScript)) {
        if (-not (Test-Path -LiteralPath $requiredScript -PathType Leaf)) {
            throw "BuildWhiteModel is missing its required script: $requiredScript"
        }
    }
    if (-not (Test-Path -LiteralPath $briefPath -PathType Leaf)) {
        throw "BuildWhiteModel is missing its design brief: $briefPath"
    }

    $outputPaths = @(
        (Join-Path $outputRoot 'Blender\SM_ShipHub_Complete_White_v1.blend'),
        (Join-Path $outputRoot 'Exports\SM_ShipHub_Complete_White_v1.fbx'),
        (Join-Path $outputRoot 'Exports\SM_ShipHub_Complete_White_v1.glb'),
        (Join-Path $outputRoot 'Reports\layout-manifest.json')
    )
    foreach ($outputPath in $outputPaths) {
        if (-not (Test-ProjectRiftContainedArtPath -Candidate $outputPath -AllowedRoot $outputRoot)) {
            throw "BuildWhiteModel output path is outside the approved root: $outputPath"
        }
    }

    & $blender --background --factory-startup --python $buildScript -- --project-root $projectRoot --brief $briefPath --output-root $outputRoot
    $buildExitCode = $LASTEXITCODE
    if ($buildExitCode -ne 0) {
        throw "BuildWhiteModel failed with exit code $buildExitCode."
    }
}

function Invoke-ProjectRiftRenderDrawings {
    $blender = Resolve-ProjectRiftValidatedBlender -StageName 'RenderDrawings'
    $renderScript = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\shiphub\render_shiphub_drawings.py'
    $blendPath = Join-Path $generatedOutputRoot 'Blender\SM_ShipHub_Complete_White_v1.blend'
    $manifestPath = Join-Path $generatedOutputRoot 'Reports\layout-manifest.json'
    $outputRoot = Join-Path $generatedOutputRoot 'Drawings\PNG'

    foreach ($requiredPath in @($renderScript, $blendPath, $manifestPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "RenderDrawings is missing its required input: $requiredPath"
        }
    }
    foreach ($validatedPath in @($blendPath, $manifestPath, $outputRoot)) {
        if (-not (Test-ProjectRiftContainedArtPath -Candidate $validatedPath -AllowedRoot $generatedOutputRoot)) {
            throw "RenderDrawings path is outside the approved root: $validatedPath"
        }
    }

    & $blender --background --factory-startup --python $renderScript -- --project-root $projectRoot --blend $blendPath --manifest $manifestPath --output-root $outputRoot
    $renderExitCode = $LASTEXITCODE
    if ($renderExitCode -ne 0) {
        throw "RenderDrawings failed with exit code $renderExitCode."
    }
}

function Invoke-ProjectRiftPublish {
    $python = Resolve-ProjectRiftPythonExecutable -ExplicitPath $PythonExe
    $publishScript = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\shiphub\publish_shiphub_drawings.py'
    $briefPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\ShipHubCompleteDesign_v1.json'
    $manifestPath = Join-Path $generatedOutputRoot 'Reports\layout-manifest.json'
    $drawingsRoot = Join-Path $generatedOutputRoot 'Drawings'
    $pngRoot = Join-Path $drawingsRoot 'PNG'

    foreach ($requiredPath in @($publishScript, $briefPath, $manifestPath, $pngRoot)) {
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "Publish is missing its required input: $requiredPath"
        }
    }
    foreach ($validatedPath in @($manifestPath, $drawingsRoot, $pngRoot)) {
        if (-not (Test-ProjectRiftContainedArtPath -Candidate $validatedPath -AllowedRoot $generatedOutputRoot)) {
            throw "Publish path is outside the approved root: $validatedPath"
        }
    }

    $publishOutput = @(& $python $publishScript --brief $briefPath --manifest $manifestPath --drawings-root $drawingsRoot 2>&1)
    $publishExitCode = $LASTEXITCODE
    $publishOutput | ForEach-Object { Write-Output $_ }
    if ($publishExitCode -ne 0) {
        throw "Publish stage failed with exit code $publishExitCode."
    }
    $publishSuccess = @(
        $publishOutput |
            ForEach-Object { $_.ToString() } |
            Where-Object { $_ -match '^ShipHub published .+ Committed Handoff files: [^;]+; .+\.$' }
    )
    if ($publishSuccess.Count -ne 1) {
        throw 'Publish stage did not report its two committed Handoff filenames.'
    }
    $publishSuccess[0] -match '^ShipHub published .+ Committed Handoff files: (?<Pdf>[^;]+); (?<Png>.+)\.$' | Out-Null
    Write-Output "Publish stage committed Handoff filenames: $($Matches.Pdf); $($Matches.Png)"
}

function Invoke-ProjectRiftDeferredStage {
    param(
        [Parameter(Mandatory)][string]$DeferredStage,
        [switch]$RequireBlender,
        [switch]$RequirePython
    )

    if ($RequireBlender) {
        Resolve-ProjectRiftValidatedBlender -StageName $DeferredStage | Out-Null
    }
    if ($RequirePython) {
        Resolve-ProjectRiftPythonExecutable -ExplicitPath $PythonExe | Out-Null
    }

    $stageScript = Join-Path $PSScriptRoot ("{0}-ProjectRiftShipHubDesign.ps1" -f $DeferredStage)
    if (-not (Test-Path -LiteralPath $stageScript -PathType Leaf)) {
        throw "$DeferredStage stage is missing its required script: $stageScript"
    }

    $stageArguments = @{}
    if (-not [string]::IsNullOrWhiteSpace($BlenderExe)) {
        $stageArguments.BlenderExe = $BlenderExe
    }
    if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
        $stageArguments.PythonExe = $PythonExe
    }
    & $stageScript @stageArguments
    if ($LASTEXITCODE -ne 0) {
        throw "$DeferredStage stage failed: required script exited with code $LASTEXITCODE."
    }
}

function Invoke-ProjectRiftRecursiveStage {
    param([Parameter(Mandatory)][string]$RecursiveStage)

    $recursiveArguments = @{ Stage = $RecursiveStage }
    if (-not [string]::IsNullOrWhiteSpace($BlenderExe)) {
        $recursiveArguments.BlenderExe = $BlenderExe
    }
    if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
        $recursiveArguments.PythonExe = $PythonExe
    }
    & $PSCommandPath @recursiveArguments
}

switch ($Stage) {
    'Preflight' { Invoke-ProjectRiftPreflight }
    'ValidateContract' { Invoke-ProjectRiftContractValidation }
    'BuildWhiteModel' { Invoke-ProjectRiftBuildWhiteModel }
    'RenderDrawings' { Invoke-ProjectRiftRenderDrawings }
    'Publish' { Invoke-ProjectRiftPublish }
    'Validate' { Invoke-ProjectRiftDeferredStage -DeferredStage 'Validate' -RequireBlender -RequirePython }
    'All' {
        Invoke-ProjectRiftRecursiveStage -RecursiveStage 'Preflight'
        Invoke-ProjectRiftRecursiveStage -RecursiveStage 'ValidateContract'
        Invoke-ProjectRiftRecursiveStage -RecursiveStage 'BuildWhiteModel'
        Invoke-ProjectRiftRecursiveStage -RecursiveStage 'RenderDrawings'
        Invoke-ProjectRiftRecursiveStage -RecursiveStage 'Publish'
        Invoke-ProjectRiftRecursiveStage -RecursiveStage 'Validate'
    }
}
