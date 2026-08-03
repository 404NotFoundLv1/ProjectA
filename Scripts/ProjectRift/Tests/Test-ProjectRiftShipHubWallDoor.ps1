[CmdletBinding()]
param(
    [switch]$RequireAppearance,
    [switch]$RequireProductionMesh
)

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

function Assert-NumericArrayEqual {
    param($Actual, [double[]]$Expected, [string]$Message)
    $actualValues = @($Actual)
    if ($actualValues.Count -ne $Expected.Count) {
        $failures.Add("$Message Expected $($Expected.Count) values; got $($actualValues.Count).")
        return
    }
    for ($index = 0; $index -lt $Expected.Count; $index++) {
        if ([double]$actualValues[$index] -ne $Expected[$index]) {
            $failures.Add("$Message Index $index expected '$($Expected[$index])'; got '$($actualValues[$index])'.")
        }
    }
}

function Get-PngMetadata {
    param([Parameter(Mandatory)][string]$Path)

    $bytes = [IO.File]::ReadAllBytes($Path)
    $signature = [byte[]](137, 80, 78, 71, 13, 10, 26, 10)
    Assert-True ($bytes.Length -ge 24) "PNG is too small: $Path"
    if ($bytes.Length -lt 24) {
        return [pscustomobject]@{ Width = 0; Height = 0; Length = $bytes.Length }
    }
    Assert-True (($bytes[0..7] -join ',') -eq ($signature -join ',')) "PNG signature mismatch: $Path"
    $width = ([uint32]$bytes[16] -shl 24) -bor ([uint32]$bytes[17] -shl 16) -bor ([uint32]$bytes[18] -shl 8) -bor [uint32]$bytes[19]
    $height = ([uint32]$bytes[20] -shl 24) -bor ([uint32]$bytes[21] -shl 16) -bor ([uint32]$bytes[22] -shl 8) -bor [uint32]$bytes[23]
    return [pscustomobject]@{ Width = $width; Height = $height; Length = $bytes.Length }
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

        $appearance = Invoke-WallDoorRunner -RunnerPath $runnerPath -Stage 'BuildAppearance' -BlenderExe $blenderExe -PythonExe $pythonExe
        Assert-Equal $appearance.ExitCode 0 "BuildAppearance must succeed in Task 10. $($appearance.Text)"

        $production = Invoke-WallDoorRunner -RunnerPath $runnerPath -Stage 'BuildProduction' -BlenderExe $blenderExe -PythonExe $pythonExe
        Assert-Equal $production.ExitCode 0 "BuildProduction must succeed in Task 11. $($production.Text)"

        $futureStages = @{
            BakeTextures = 'Task 12 (Texture Bake) has not run'
            Export = 'Task 13 (Export) has not run'
            ValidatePackage = 'Task 14 (Package Validation) has not run'
            AllDCC = 'Task 12 (Texture Bake) has not run'
        }
        foreach ($stage in $futureStages.Keys) {
            $result = Invoke-WallDoorRunner -RunnerPath $runnerPath -Stage $stage -BlenderExe $blenderExe -PythonExe $pythonExe
            Assert-True ($result.ExitCode -ne 0) "$stage must fail closed after Task 10."
            Assert-True ($result.Text -match [regex]::Escape($futureStages[$stage])) "$stage must name its unavailable owner. Output: $($result.Text)"
        }

        if ($RequireAppearance) {
            $firstArticleRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\FirstArticle\WallDoor_400_A'
            $blendPath = Join-Path $firstArticleRoot 'Blender\SM_ShipHub_WallDoor_400_A_Appearance.blend'
            $authorityRelativePaths = @(
                'Concept/Orthographic/Front.png',
                'Concept/Orthographic/Back.png',
                'Concept/Orthographic/Left.png',
                'Concept/Orthographic/Right.png',
                'Concept/Orthographic/Perspective.png'
            )
            $candidateRelativePaths = @(
                'Concept/Candidates/Base.png',
                'Concept/Candidates/Damaged.png',
                'Concept/Candidates/Patched.png',
                'Concept/Candidates/Online.png'
            )
            $sheetPath = Join-Path $firstArticleRoot 'Concept\SM_ShipHub_WallDoor_400_A_AppearanceLock.png'
            $reportPath = Join-Path $firstArticleRoot 'Reports\appearance-validation.json'

            Assert-True (Test-Path -LiteralPath $blendPath -PathType Leaf) "Appearance Blender file is missing: $blendPath"
            foreach ($relativePath in $authorityRelativePaths) {
                $imagePath = Join-Path $firstArticleRoot $relativePath
                Assert-True (Test-Path -LiteralPath $imagePath -PathType Leaf) "Authority PNG is missing: $imagePath"
                if (Test-Path -LiteralPath $imagePath -PathType Leaf) {
                    $metadata = Get-PngMetadata -Path $imagePath
                    Assert-True ($metadata.Length -gt 24) "Authority PNG is empty: $imagePath"
                    Assert-Equal $metadata.Width 2048 "Authority PNG width: $relativePath"
                    Assert-Equal $metadata.Height 2048 "Authority PNG height: $relativePath"
                }
            }

            $candidateMetadata = @()
            foreach ($relativePath in $candidateRelativePaths) {
                $imagePath = Join-Path $firstArticleRoot $relativePath
                Assert-True (Test-Path -LiteralPath $imagePath -PathType Leaf) "Candidate PNG is missing: $imagePath"
                if (Test-Path -LiteralPath $imagePath -PathType Leaf) {
                    $metadata = Get-PngMetadata -Path $imagePath
                    Assert-True ($metadata.Length -gt 24) "Candidate PNG is empty: $imagePath"
                    $candidateMetadata += $metadata
                }
            }
            if ($candidateMetadata.Count -eq 4) {
                foreach ($metadata in $candidateMetadata) {
                    Assert-Equal $metadata.Width 2048 'Final Blender candidate width.'
                    Assert-Equal $metadata.Height 2048 'Final Blender candidate height.'
                    Assert-Equal $metadata.Width $candidateMetadata[0].Width 'Candidate PNG widths must match.'
                    Assert-Equal $metadata.Height $candidateMetadata[0].Height 'Candidate PNG heights must match.'
                    Assert-Equal ([double]$metadata.Width / $metadata.Height) ([double]$candidateMetadata[0].Width / $candidateMetadata[0].Height) 'Candidate PNG aspect ratios must match.'
                }
            }

            Assert-True (Test-Path -LiteralPath $sheetPath -PathType Leaf) "Appearance-lock sheet is missing: $sheetPath"
            if (Test-Path -LiteralPath $sheetPath -PathType Leaf) {
                $sheetMetadata = Get-PngMetadata -Path $sheetPath
                Assert-True ($sheetMetadata.Length -gt 24) 'Appearance-lock sheet must be non-empty.'
                Assert-True ($sheetMetadata.Width -ge 4096) 'Appearance-lock sheet must be at least 4096 pixels wide.'
                Assert-True ($sheetMetadata.Height -ge 2048) 'Appearance-lock sheet must be at least 2048 pixels high.'
            }

            Assert-True (Test-Path -LiteralPath $reportPath -PathType Leaf) "Appearance report is missing: $reportPath"
            if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
                $report = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
                $approvalPath = Join-Path $firstArticleRoot 'Briefs\SM_ShipHub_WallDoor_400_A.approval.json'
                $approval = Get-Content -LiteralPath $approvalPath -Raw -Encoding UTF8 | ConvertFrom-Json
                Assert-Equal $report.Schema 'projectrift.shiphub.wall-door-appearance-validation.v1' 'Appearance report schema.'
                Assert-Equal $report.AssetId 'SM_ShipHub_WallDoor_400_A' 'Appearance asset ID.'
                Assert-True $report.Passed 'Appearance deterministic validation.'
                Assert-Equal $report.BoundsCm.Width 400 'Appearance width.'
                Assert-Equal $report.BoundsCm.Depth 30 'Appearance depth.'
                Assert-Equal $report.BoundsCm.Height 400 'Appearance height.'
                Assert-Equal $report.DoorOpeningCm.Width 240 'Appearance opening width.'
                Assert-Equal $report.DoorOpeningCm.Height 280 'Appearance opening height.'
                Assert-True $report.PivotAtBottomLeftBack 'Appearance pivot.'
                Assert-Equal $approval.Appearance.Status 'Approved' 'Appearance approval ledger must reflect the explicit user approval.'
                Assert-Equal $report.ApprovalStatus $approval.Appearance.Status 'Appearance report must reflect the approval ledger status.'
                Assert-Equal $report.ApprovalDate $approval.Appearance.Date 'Appearance report must reflect the approval date.'
                Assert-Equal ($report.ApprovalEvidence -join '|') ($approval.Appearance.Evidence -join '|') 'Appearance report must reflect approval evidence.'
                Assert-Equal $report.Method 'BlenderAuthoritativeStyleReconstruction' 'Appearance method.'
                Assert-Equal $report.ImageGenRole 'StyleReferenceOnly' 'ImageGen role.'
                Assert-Equal $report.CandidateCamera 'CAM_Perspective' 'Candidate camera.'
                Assert-Equal $report.CandidateCameraDriftPixels 0 'Candidate camera drift.'
                Assert-Equal @($report.Candidates).Count 4 'Final Blender candidate count.'
                Assert-Equal @($report.CandidateMetadata).Count 4 'Final Blender candidate metadata count.'
                if (@($report.CandidateMetadata).Count -eq 4) {
                    $candidateCameraHashes = @($report.CandidateMetadata | ForEach-Object { $_.CameraTransformLensSha256 } | Select-Object -Unique)
                    Assert-Equal $candidateCameraHashes.Count 1 'Final Blender candidates must share one camera transform/lens hash.'
                    foreach ($entry in @($report.CandidateMetadata)) {
                        Assert-Equal $entry.Width 2048 "Final candidate report width: $($entry.Path)"
                        Assert-Equal $entry.Height 2048 "Final candidate report height: $($entry.Path)"
                    }
                }
                Assert-Equal (($report.Collections | Sort-Object) -join '|') ((@('00_REFERENCE', '10_STRUCTURE', '20_DETAIL', '30_STATE_OVERLAY', '40_COLLISION', '90_EXPORT') | Sort-Object) -join '|') 'Appearance collections.'
                Assert-Equal ($report.AuthorityViews -join '|') ($authorityRelativePaths -join '|') 'Appearance authority paths.'
                Assert-Equal ($report.Candidates -join '|') ($candidateRelativePaths -join '|') 'Appearance candidate paths.'
                $ledgerPath = Join-Path $firstArticleRoot 'Briefs\SM_ShipHub_WallDoor_400_A.generation-ledger.json'
                Assert-Equal $report.GenerationLedgerSha256 ((Get-FileHash -LiteralPath $ledgerPath -Algorithm SHA256).Hash.ToLowerInvariant()) 'Generation ledger SHA-256.'
                $serializedReport = $report | ConvertTo-Json -Depth 20 -Compress
                Assert-True ($serializedReport -notmatch '(?i)(\.fbx|\.glb|\.uasset|/Content/|\\Content\\|\b(BaseColor|NormalMap|ORM|StateMask)\b)') 'Appearance report must not list later-task artifacts.'
            }

            $laterArtifacts = @(
                Get-ChildItem -LiteralPath $firstArticleRoot -Recurse -File -Force |
                    Where-Object { $_.Extension -in @('.fbx', '.glb', '.tga', '.uasset') }
            )
            Assert-Equal $laterArtifacts.Count 0 'Task 10 must not create production, texture, export, or UE artifacts.'
        }

        if ($RequireProductionMesh) {
            $firstArticleRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\FirstArticle\WallDoor_400_A'
            $blendPath = Join-Path $firstArticleRoot 'Blender\SM_ShipHub_WallDoor_400_A.blend'
            $reportPath = Join-Path $firstArticleRoot 'Reports\geometry-validation.json'
            $validatorPath = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\shiphub\validate_wall_door_production.py'

            Assert-True (Test-Path -LiteralPath $blendPath -PathType Leaf) "Authoritative production Blender file is missing: $blendPath"
            if (Test-Path -LiteralPath $blendPath -PathType Leaf) {
                Assert-True ((Get-Item -LiteralPath $blendPath).Length -gt 0) 'Authoritative production Blender file must be non-empty.'
            }
            Assert-True (Test-Path -LiteralPath $reportPath -PathType Leaf) "Independent geometry report is missing: $reportPath"
            Assert-True (Test-Path -LiteralPath $validatorPath -PathType Leaf) "Saved-blend validator is missing: $validatorPath"
            if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
                $report = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
                Assert-Equal $report.Schema 'projectrift.shiphub.wall-door-geometry-validation.v1' 'Geometry report schema.'
                Assert-Equal $report.AssetId 'SM_ShipHub_WallDoor_400_A' 'Geometry asset ID.'
                Assert-True $report.Passed 'Independent geometry validation must pass.'
                Assert-Equal $report.ValidationMethod 'IndependentSavedBlendReopen' 'Geometry validation method.'
                Assert-True $report.SavedBlendReopened 'Geometry validator must reopen the saved authoritative blend.'
                if (Test-Path -LiteralPath $validatorPath -PathType Leaf) {
                    Assert-Equal $report.ValidatorScriptSha256 ((Get-FileHash -LiteralPath $validatorPath -Algorithm SHA256).Hash.ToLowerInvariant()) 'Saved-blend validator SHA-256.'
                }
                Assert-Equal $report.InspectedBlendSha256 ((Get-FileHash -LiteralPath $blendPath -Algorithm SHA256).Hash.ToLowerInvariant()) 'Independently inspected Blender SHA-256.'
                Assert-NumericArrayEqual $report.BoundsCm @(400, 30, 400) 'Exact production bounds.'
                Assert-NumericArrayEqual $report.DoorOpeningCm @(240, 280) 'Exact door opening size.'
                Assert-NumericArrayEqual $report.DoorOpeningRangeCm.X @(80, 320) 'Exact door opening X range.'
                Assert-NumericArrayEqual $report.DoorOpeningRangeCm.Z @(0, 280) 'Exact door opening Z range.'
                Assert-NumericArrayEqual $report.PivotCm @(0, 0, 0) 'Bottom-left-back pivot.'
                Assert-NumericArrayEqual $report.AppliedScale @(1, 1, 1) 'Applied production scale.'
                Assert-True ($report.MaterialSlotCount -le 2) 'Base material slots must not exceed the contract limit.'
                Assert-Equal $report.MaterialSlotCount 2 'Production report material-slot value.'
                Assert-Equal $report.UnexpectedNonManifoldEdgeCount 0 'Unexpected non-manifold edge count.'
                Assert-Equal $report.DuplicateFaceCount 0 'Duplicate face count.'
                Assert-Equal ($report.OverlayObjects -join '|') 'SM_ShipHub_WallDoor_400_A_Overlay_Damaged|SM_ShipHub_WallDoor_400_A_Overlay_Patched' 'Separate production overlay objects.'
                Assert-Equal $report.OnlineSilhouetteOverlayCount 0 'Online must not have a silhouette overlay.'

                $collisionPieces = @($report.CollisionPieces)
                Assert-Equal $collisionPieces.Count 3 'Exact collision piece count.'
                Assert-Equal (($collisionPieces.Name | Sort-Object) -join '|') ((@(
                    'UCX_SM_ShipHub_WallDoor_400_A_LeftJamb',
                    'UCX_SM_ShipHub_WallDoor_400_A_Lintel',
                    'UCX_SM_ShipHub_WallDoor_400_A_RightJamb'
                ) | Sort-Object) -join '|') 'Exact collision piece names.'
                foreach ($piece in $collisionPieces) {
                    Assert-True (-not $piece.BlocksDoorOpening) "Collision piece must not block the door opening: $($piece.Name)"
                    Assert-Equal @($piece.BoundsCm).Count 6 "Collision piece bounds evidence: $($piece.Name)"
                }

                $expectedExportObjects = @(
                    'SM_ShipHub_WallDoor_400_A',
                    'SM_ShipHub_WallDoor_400_A_RemovableCover',
                    'SM_ShipHub_WallDoor_400_A_Overlay_Damaged',
                    'SM_ShipHub_WallDoor_400_A_Overlay_Patched'
                )
                $exportEvidence = @($report.ExportObjects)
                Assert-Equal (($exportEvidence.Name | Sort-Object) -join '|') (($expectedExportObjects | Sort-Object) -join '|') 'Export-facing object evidence.'
                foreach ($entry in $exportEvidence) {
                    Assert-NumericArrayEqual $entry.Location @(0, 0, 0) "Applied location: $($entry.Name)"
                    Assert-NumericArrayEqual $entry.RotationDegrees @(0, 0, 0) "Applied rotation: $($entry.Name)"
                    Assert-NumericArrayEqual $entry.Scale @(1, 1, 1) "Applied scale: $($entry.Name)"
                    Assert-Equal ($entry.UVLayers -join '|') 'UV0|UV1' "Exact UV layers: $($entry.Name)"
                    Assert-True $entry.ModifiersApplied "Applied modifiers: $($entry.Name)"
                    Assert-True $entry.Triangulated "Triangulated export topology: $($entry.Name)"
                    Assert-True ($entry.PolygonCount -gt 0) "Polygon evidence: $($entry.Name)"
                    Assert-True ($entry.TriangleCount -gt 0) "Triangle evidence: $($entry.Name)"
                }

                Assert-True ($report.PrimarySilhouetteBevelCm -ge 4 -and $report.PrimarySilhouetteBevelCm -le 6) 'Primary silhouette bevel must remain within 4..6 cm.'
                Assert-True ($report.SecondaryPanelBevelCm -ge 0.8 -and $report.SecondaryPanelBevelCm -le 1.5) 'Secondary panel bevel must remain within 0.8..1.5 cm.'
                Assert-True ($report.FunctionalRecessDepthCm -ge 2 -and $report.FunctionalRecessDepthCm -le 6) 'Functional recess depth must remain within 2..6 cm.'
                Assert-Equal $report.FunctionalRecessEvidence.Method 'SavedMeshCavityBackplane' 'Functional recess must be measured from saved mesh geometry.'
                Assert-Equal $report.FunctionalRecessEvidence.MeasuredDepthCm 4 'Functional recess measured depth.'
                Assert-Equal $report.FunctionalRecessEvidence.FrontSurfaceYCm 4 'Functional recess front surface.'
                Assert-Equal $report.FunctionalRecessEvidence.BackSurfaceYCm 8 'Functional recess backplane.'
                Assert-True ($report.FunctionalRecessEvidence.SampleVertexCount -gt 0) 'Functional recess saved-mesh samples.'
                Assert-True $report.OuterSnapFacesPlanar 'Outer snap faces must remain planar.'
                Assert-True (-not $report.HasDoorLeaf) 'Production geometry must not contain a door leaf.'
                Assert-Equal $report.PatchedRepairPlateCount 2 'Patched overlay repair plate count.'
                Assert-Equal $report.PatchedCableGuideCount 1 'Patched overlay cable-guide count.'
                Assert-True $report.DamagedOverlayHasAbsentCoverRim 'Damaged overlay absent-cover rim evidence.'

                $stateCompositions = @($report.StateCompositions)
                Assert-Equal (($stateCompositions.State | Sort-Object) -join '|') ((@('Base', 'Damaged', 'Online', 'Patched') | Sort-Object) -join '|') 'Exact saved-blend state compositions.'
                foreach ($state in $stateCompositions) {
                    $expectedCoverCount = if ($state.State -eq 'Damaged') { 0 } else { 1 }
                    Assert-Equal $state.VisibleCoverObjectCount $expectedCoverCount "Visible removable-cover count: $($state.State)"
                    Assert-Equal $state.CoverVisible ($state.State -ne 'Damaged') "Removable-cover visibility: $($state.State)"
                    Assert-True ($state.VisibleBaseBodyCount -eq 1) "Shared base body visibility: $($state.State)"
                    Assert-True (-not $state.UsesClonedBase) "State must not clone the base body: $($state.State)"
                }

                $renderEntries = @($report.ReviewRenders)
                Assert-Equal (($renderEntries.State | Sort-Object) -join '|') ((@('Base', 'Damaged', 'Patched') | Sort-Object) -join '|') 'Exact Task 11 review render states.'
                foreach ($entry in $renderEntries) {
                    $renderPath = Join-Path $firstArticleRoot ([string]$entry.Path)
                    Assert-True (Test-Path -LiteralPath $renderPath -PathType Leaf) "Production review render is missing: $renderPath"
                    if (Test-Path -LiteralPath $renderPath -PathType Leaf) {
                        Assert-Equal $entry.Sha256 ((Get-FileHash -LiteralPath $renderPath -Algorithm SHA256).Hash.ToLowerInvariant()) "Production review render SHA-256: $($entry.Path)"
                    }
                }
                Assert-Equal $report.BlenderVersion '5.2.0 LTS' 'Authoritative Blender version.'
                Assert-Equal $report.ApprovalDependency.Status 'Approved' 'Production must depend on approved appearance.'
                Assert-True (-not [string]::IsNullOrWhiteSpace([string]$report.SourceSha256.AppearanceBlend)) 'Appearance authority source hash is required.'
                Assert-Equal $report.OutputSha256.ProductionBlend ((Get-FileHash -LiteralPath $blendPath -Algorithm SHA256).Hash.ToLowerInvariant()) 'Production Blender SHA-256.'
            }

            if ((Test-Path -LiteralPath $validatorPath -PathType Leaf) -and (Test-Path -LiteralPath $blendPath -PathType Leaf)) {
                $mutationRoot = Join-Path $projectRoot 'Saved\Automation\ProjectRiftShipHubWallDoor\ValidatorMutation'
                $mutatedBlend = Join-Path $mutationRoot 'SM_ShipHub_WallDoor_400_A_MissingUV1.blend'
                $mutationReport = Join-Path $mutationRoot 'geometry-validation.json'
                try {
                    New-Item -ItemType Directory -Path $mutationRoot -Force | Out-Null
                    Copy-Item -LiteralPath $blendPath -Destination $mutatedBlend -Force
                    $escapedMutatedBlend = $mutatedBlend.Replace("'", "\'")
                    $mutationExpression = "import bpy; obj=bpy.data.objects['SM_ShipHub_WallDoor_400_A']; layer=obj.data.uv_layers.get('UV1'); obj.data.uv_layers.remove(layer); bpy.ops.wm.save_as_mainfile(filepath=r'$escapedMutatedBlend', check_existing=False)"
                    $previousErrorActionPreference = $ErrorActionPreference
                    try {
                        $ErrorActionPreference = 'Continue'
                        $mutationOutput = @(& $blenderExe --background $mutatedBlend --python-expr $mutationExpression 2>&1)
                        $mutationExitCode = $LASTEXITCODE
                        $validatorOutput = @(& $blenderExe --background --factory-startup --python $validatorPath -- --project-root $projectRoot --output-root $firstArticleRoot --blend $mutatedBlend --report $mutationReport --skip-renders 2>&1)
                        $validatorExitCode = $LASTEXITCODE
                    }
                    finally {
                        $ErrorActionPreference = $previousErrorActionPreference
                    }
                    Assert-Equal $mutationExitCode 0 "Saved-blend UV mutation setup must succeed. $($mutationOutput -join "`n")"
                    Assert-True ($validatorExitCode -ne 0) 'Independent validator must fail a saved blend whose base UV1 was removed.'
                    Assert-True (($validatorOutput -join "`n") -match 'UV layers') "Independent validator must name the mutated UV contract. $($validatorOutput -join "`n")"
                    Assert-True (-not (Test-Path -LiteralPath $mutationReport -PathType Leaf)) 'Failed independent validation must not publish a geometry report.'
                }
                finally {
                    if (Test-Path -LiteralPath $mutationRoot) {
                        Remove-Item -LiteralPath $mutationRoot -Recurse -Force
                    }
                }
            }

            $forbiddenArtifacts = @(
                Get-ChildItem -LiteralPath $firstArticleRoot -Recurse -File -Force |
                    Where-Object { $_.Extension -in @('.fbx', '.glb', '.tga', '.uasset') -or $_.Name -match '(?i)(BaseColor|NormalMap|ORM|StateMask)' }
            )
            Assert-Equal $forbiddenArtifacts.Count 0 'Task 11 must not create Task 12+ texture, export, or UE artifacts.'
        }
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
