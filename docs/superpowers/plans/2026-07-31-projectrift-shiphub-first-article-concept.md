# ProjectRift Ship Hub First-Article Concept Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Use `superpowers:test-driven-development` for repository scripts. Do not use parallel editor or Blender workers because ProjectA has one authoritative editor/tool state.

**Goal:** First complete Gate G-1 for the overall ProjectRift ship design, then complete Gates G0–G2 for `SM_ShipHub_WallDoor_400_A`: establish the source-art/provenance foundation, generate and validate a dimensionally correct Blender white model, and deliver an approved concept package without importing or modifying any Unreal asset.

**Architecture:** A user-approved G-1 overall-ship concept is the upstream visual authority for silhouette, structural language, module relationships, and the damage/repair story. Only after G-1 approval do project-owned JSON briefs lock first-article dimensions and policy. A small PowerShell module resolves a user-installed Blender 4.5 LTS executable and invokes project-local Blender Python scripts. Blender remains the geometry authority and emits a `.blend`, FBX white model, validation JSON, and deterministic view renders; built-in ImageGen edits those renders for surface-treatment concepts while preserving geometry.

**Tech Stack:** PowerShell 5.1, Git LFS 3.5.1, Blender 4.5 LTS + Python, built-in ImageGen, JSON, FBX, PNG, ProjectA UE5.8 read-only reference data.

## Global Constraints

- Operate only within `E:\MyWork\ProjectA` and ProjectA-owned tool processes.
- Work directly on existing `main`; do not create/switch branches or worktrees.
- Do not run `git add`, `git commit`, `git tag`, `git push`, stash, reset, restore, checkout, or history-rewrite commands. The user handles Git operations.
- Do not modify `/Game/ProjectRift/Maps/L_ShipLobby` or any `.uasset`/`.umap` in this version.
- Do not create `L_ShipHub_ArtValidation` yet; that belongs to the next G3–G6 version.
- Do not import the white model into UE in this version.
- Do not use the current UE lobby as an image, geometry, dimension, or layout reference.
- Unreal MCP remains restricted to `http://127.0.0.1:8001/mcp`; this plan does not need MCP mutation.
- Use only built-in ImageGen. If it fails or is unavailable, stop at the documented image-generation blocker; do not use CLI/API fallback without new explicit user approval.
- Blender is not currently callable from PATH. Installation is deliberately deferred until the G-1 overall-ship design is approved; Blender 4.5 LTS or an existing `blender.exe` path is required only before Task 3.
- The first-article dimensions below become authoritative only at G0, after G-1 confirms that the door-wall belongs to the approved overall ship language: module 400×30×400 cm, door opening 240×280 cm, snap 50 cm, pivot at bottom-left-back.
- Visual direction is “断脊远征舰” with no rift-purple surface language on this ordinary structural module.
- Quality target is grounded realistic PBR at medium detail density, but this version stops at concept approval before production PBR work.
- Every generated or project-owned source artifact records provenance. No license-registry entry is added until a UE asset is actually imported.
- If G-1 is rejected, stop or revise the overall ship concept; do not proceed to G0, Blender, or the door-wall first article.
- End this version after the user approves or rejects the G2 concept package. Do not start production modeling automatically.

---

### Task 0: Approve the G-1 Overall Ship Concept Before Asset Production

**Inputs:**
- `docs/superpowers/specs/2026-07-31-projectrift-ship-hub-art-pipeline-design.md`
- ProjectRift's approved game premise and the established “断脊远征舰 + limited rift symbiosis” direction

**Produces:**
- A user-approved or user-rejected overall-ship concept decision.
- After approval and Task 1's provenance structure exists, the selected image and prompt are recorded under `SourceArt/ProjectRift/ShipHub/Concept/ShipOverall/` and the generation ledger. An unapproved preview is not a modeling authority.

- [ ] **Step 1: Generate one overall-ship design board without using the UE demo map**

The board must include a dominant exterior three-quarter view, readable side/top silhouettes, and a partial cutaway that communicates the central hub plus reactor, medical bay, communications array, armory, and jump-core relationships. It must show a small-squad expedition ship rather than a fighter or capital ship.

- [ ] **Step 2: Check the approved art constraints**

Confirm grounded realistic PBR, medium detail density, strong human engineering, a readable broken/reinforced central spine, staged crash damage and field repairs, and rift-violet restricted to the contained jump core. Reject luxury-yacht, clean-white-utopian, gothic, fully organic, city-ship, or purple-everywhere drift.

- [ ] **Step 3: Present G-1 to the user**

State explicitly that this is an overall visual and spatial-direction gate, not a dimensioned orthographic engineering drawing. Ask the user to approve the direction or identify changes to silhouette, massing, damage level, module arrangement, or color/material language.

- [ ] **Step 4: Enforce the gate**

Do not install/launch Blender, create the door-wall brief, or start Task 1 until the user explicitly approves G-1. If built-in ImageGen is unavailable, stop and report the image-generation blocker; a CLI/API fallback requires separate explicit approval because it may require an API key and incur cost.

---

## Planned File Structure

### Repository files created or modified by this version

```text
.gitattributes                                             # Add LFS rules for SourceArt binaries.
SourceArt/ProjectRift/ShipHub/README.md                    # Source-art ownership and folder contract.
SourceArt/ProjectRift/ShipHub/Briefs/
  SM_ShipHub_WallDoor_400_A.asset.json                    # G0 authority for dimensions and policy.
  SM_ShipHub_WallDoor_400_A.generation-ledger.json        # Prompt/tool/result provenance.
  SM_ShipHub_WallDoor_400_A.approval.json                 # G1/G2 user decisions.
SourceArt/ProjectRift/ShipHub/License/
  SM_ShipHub_WallDoor_400_A.provenance.md                 # Project-owned and generated-source declaration.
Scripts/ProjectRift/ArtPipeline/
  ProjectRift.ArtPipeline.psm1                            # Safe path and Blender resolver/invocation functions.
  Invoke-ProjectRiftArtFirstArticle.ps1                   # Single entry point for preflight/build/validate/render.
  blender/
    shiphub_common.py                                     # Shared constants, object creation, reporting helpers.
    build_shiphub_wall_door.py                            # Deterministic white-model builder/exporter/renderer.
Scripts/ProjectRift/Tests/
  Test-ProjectRiftArtPipeline.ps1                         # Repository and PowerShell contract tests.
```

### Generated LFS artifacts

```text
SourceArt/ProjectRift/ShipHub/Blender/SM_ShipHub_WallDoor_400_A.blend
SourceArt/ProjectRift/ShipHub/Exports/SM_ShipHub_WallDoor_400_A.fbx
SourceArt/ProjectRift/ShipHub/Concept/SM_ShipHub_WallDoor_400_A/
  white-front.png
  white-back.png
  white-left.png
  white-right.png
  white-top.png
  white-perspective-front.png
  white-perspective-back.png
  white-perspective-detail.png
  white-contact-sheet.png
  concept-surface-a.png
  concept-surface-b.png
  concept-surface-c.png
  concept-approved-keyframe.png
  concept-state-damaged.png
  concept-state-patched.png
  concept-state-online.png
  material-callouts.md
SourceArt/ProjectRift/ShipHub/Briefs/Reports/
  SM_ShipHub_WallDoor_400_A.validation.json
```

### Explicitly untouched

```text
Content/**/*.uasset
Content/**/*.umap
Config/ProjectRift/AssetLicenseRegistry.json
ProjectA.uproject
/Game/ProjectRift/Maps/L_ShipLobby
```

---

### Task 1: Establish G0 Source-Art and Provenance Contract

**Files:**
- Modify: `.gitattributes`
- Create: `SourceArt/ProjectRift/ShipHub/README.md`
- Create: `SourceArt/ProjectRift/ShipHub/Briefs/SM_ShipHub_WallDoor_400_A.asset.json`
- Create: `SourceArt/ProjectRift/ShipHub/Briefs/SM_ShipHub_WallDoor_400_A.generation-ledger.json`
- Create: `SourceArt/ProjectRift/ShipHub/Briefs/SM_ShipHub_WallDoor_400_A.approval.json`
- Create: `SourceArt/ProjectRift/ShipHub/License/SM_ShipHub_WallDoor_400_A.provenance.md`
- Create: `Scripts/ProjectRift/Tests/Test-ProjectRiftArtPipeline.ps1`

**Interfaces:**
- Consumes: approved design spec `docs/superpowers/specs/2026-07-31-projectrift-ship-hub-art-pipeline-design.md`.
- Produces: authoritative `asset.json` consumed by Tasks 2–5; empty but schema-valid generation and approval ledgers.

- [ ] **Step 1: Write the failing source-contract tests**

Create `Scripts/ProjectRift/Tests/Test-ProjectRiftArtPipeline.ps1` with the existing ProjectRift PowerShell self-test style:

```powershell
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$script:FailureCount = 0
$script:AssertionCount = 0
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))

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
    Assert-True ($Expected -eq $Actual) "$Message (expected '$Expected', actual '$Actual')"
}

$briefPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\SM_ShipHub_WallDoor_400_A.asset.json'
$ledgerPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\SM_ShipHub_WallDoor_400_A.generation-ledger.json'
$approvalPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\SM_ShipHub_WallDoor_400_A.approval.json'
$attributesPath = Join-Path $projectRoot '.gitattributes'

Assert-True (Test-Path -LiteralPath $briefPath) 'The first-article brief must exist.'
Assert-True (Test-Path -LiteralPath $ledgerPath) 'The generation ledger must exist.'
Assert-True (Test-Path -LiteralPath $approvalPath) 'The approval ledger must exist.'

if (Test-Path -LiteralPath $briefPath) {
    $brief = Get-Content -LiteralPath $briefPath -Raw | ConvertFrom-Json
    Assert-Equal 1 $brief.SchemaVersion 'Brief schema version.'
    Assert-Equal 'SM_ShipHub_WallDoor_400_A' $brief.AssetId 'Asset ID.'
    Assert-Equal 400 $brief.DimensionsCm.Width 'Module width.'
    Assert-Equal 30 $brief.DimensionsCm.Depth 'Module depth.'
    Assert-Equal 400 $brief.DimensionsCm.Height 'Module height.'
    Assert-Equal 240 $brief.DoorOpeningCm.Width 'Door clear width.'
    Assert-Equal 280 $brief.DoorOpeningCm.Height 'Door clear height.'
    Assert-Equal 50 $brief.SnapCm 'Grid snap.'
    Assert-Equal 'BottomLeftBack' $brief.Pivot 'Pivot contract.'
    Assert-Equal 'G0' $brief.Stage 'Initial gate.'
    Assert-True (-not $brief.AllowRiftPurple) 'Ordinary structure must reject rift purple.'
}

$attributes = if (Test-Path -LiteralPath $attributesPath) { Get-Content -LiteralPath $attributesPath -Raw } else { '' }
foreach ($pattern in @('SourceArt/**/*.blend', 'SourceArt/**/*.fbx', 'SourceArt/**/*.png', 'SourceArt/**/*.exr', 'SourceArt/**/*.kra')) {
    Assert-True ($attributes.Contains("$pattern filter=lfs diff=lfs merge=lfs -text")) "LFS rule missing for $pattern"
}

if ($script:FailureCount -gt 0) {
    Write-Host "ProjectRift art-pipeline self-test: FAIL ($script:FailureCount/$script:AssertionCount assertions failed)."
    exit 1
}

Write-Host "ProjectRift art-pipeline self-test: PASS ($script:AssertionCount assertions)."
exit 0
```

- [ ] **Step 2: Run the source-contract test and verify it fails**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftArtPipeline.ps1
```

Expected: exit `1` with missing brief, ledger, approval, and LFS rule assertions.

- [ ] **Step 3: Add exact SourceArt LFS rules**

Append to `.gitattributes` using `apply_patch`:

```gitattributes
# ProjectRift source-art binaries
SourceArt/**/*.blend filter=lfs diff=lfs merge=lfs -text
SourceArt/**/*.blend1 filter=lfs diff=lfs merge=lfs -text
SourceArt/**/*.fbx filter=lfs diff=lfs merge=lfs -text
SourceArt/**/*.png filter=lfs diff=lfs merge=lfs -text
SourceArt/**/*.exr filter=lfs diff=lfs merge=lfs -text
SourceArt/**/*.kra filter=lfs diff=lfs merge=lfs -text
```

Do not run `git lfs track`, `git add`, or `git commit`; the text rules are edited directly and the user performs Git actions later.

- [ ] **Step 4: Create the authoritative first-article brief**

Create `SourceArt/ProjectRift/ShipHub/Briefs/SM_ShipHub_WallDoor_400_A.asset.json`:

```json
{
  "SchemaVersion": 1,
  "AssetId": "SM_ShipHub_WallDoor_400_A",
  "Stage": "G0",
  "SourceKind": "ProjectOwnedGenerated",
  "IntendedUePath": "/Game/ProjectRift/ShipHub/Meshes/Structure/SM_ShipHub_WallDoor_400_A",
  "DimensionsCm": { "Width": 400, "Depth": 30, "Height": 400 },
  "DoorOpeningCm": { "Width": 240, "Height": 280 },
  "SnapCm": 50,
  "Pivot": "BottomLeftBack",
  "CameraBaselineCm": { "CapsuleRadius": 42, "CapsuleHalfHeight": 96, "SpringArmLength": 400 },
  "ArtDirection": "BrokenSpineExpeditionShip",
  "QualityTarget": "GroundedRealisticPBRMediumDetail",
  "AllowedStates": [ "Damaged", "Patched", "Online" ],
  "AllowRiftPurple": false,
  "MaterialSlotLimit": 2,
  "ConceptIterationLimit": 3,
  "CurrentMapIsReference": false,
  "UeImportAllowedInThisVersion": false
}
```

- [ ] **Step 5: Create empty, schema-valid ledgers**

Create generation ledger:

```json
{
  "SchemaVersion": 1,
  "AssetId": "SM_ShipHub_WallDoor_400_A",
  "Entries": []
}
```

Create approval ledger:

```json
{
  "SchemaVersion": 1,
  "AssetId": "SM_ShipHub_WallDoor_400_A",
  "G0": { "Status": "Approved", "Date": "2026-07-31", "Notes": "Approved implementation scope and exact first-article dimensions." },
  "G1": { "Status": "Pending" },
  "G2": { "Status": "Pending" }
}
```

- [ ] **Step 6: Create source-art and provenance documentation**

`README.md` must state folder responsibilities, Blender as geometry authority, ImageGen as non-authoritative surface exploration, and the prohibition on importing before G2 approval.

`SM_ShipHub_WallDoor_400_A.provenance.md` must record:

```markdown
# SM_ShipHub_WallDoor_400_A provenance

- Geometry owner: ProjectA
- Geometry method: deterministic project-local Blender Python scripts
- Concept method: built-in OpenAI ImageGen edits of ProjectA-owned white-model renders
- Current use: internal concept and first-article development only
- UE import: prohibited in this version
- Third-party model or texture inputs: none
- Final commercial/legal review: required before first UE import
```

- [ ] **Step 7: Run the source-contract test and verify it passes**

Run the same PowerShell command.

Expected: `ProjectRift art-pipeline self-test: PASS` and exit `0`.

- [ ] **Step 8: User Git checkpoint**

Report changed files and suggest commit message `art: establish ship hub first-article source contract`. Do not stage or commit.

---

### Task 2: Add a Bounded Blender Resolver and Invocation Entry Point

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/ProjectRift.ArtPipeline.psm1`
- Create: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftArtFirstArticle.ps1`
- Modify: `Scripts/ProjectRift/Tests/Test-ProjectRiftArtPipeline.ps1`

**Interfaces:**
- Consumes: explicit `-BlenderExe` or task-specific `PROJECTRIFT_BLENDER_EXE`.
- Produces: `Resolve-ProjectRiftBlenderExecutable`, `Test-ProjectRiftContainedArtPath`, and a single first-article CLI.

- [ ] **Step 1: Extend tests with failing resolver and containment assertions**

Add before the final failure-count block:

```powershell
$modulePath = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\ProjectRift.ArtPipeline.psm1'
Assert-True (Test-Path -LiteralPath $modulePath) 'Art pipeline module must exist.'

if (Test-Path -LiteralPath $modulePath) {
    Import-Module -Force $modulePath
    $sourceRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub'
    Assert-True (Test-ProjectRiftContainedArtPath -Candidate (Join-Path $sourceRoot 'Concept\a.png') -AllowedRoot $sourceRoot) 'A SourceArt child path must be accepted.'
    Assert-True (-not (Test-ProjectRiftContainedArtPath -Candidate $sourceRoot -AllowedRoot $sourceRoot)) 'The SourceArt root itself must be rejected as a destructive target.'
    Assert-True (-not (Test-ProjectRiftContainedArtPath -Candidate (Join-Path $projectRoot 'Content\ProjectRift') -AllowedRoot $sourceRoot)) 'Content must not be accepted as SourceArt.'
    try {
        Resolve-ProjectRiftBlenderExecutable -ExplicitPath (Join-Path $projectRoot 'missing\blender.exe') | Out-Null
        Assert-True $false 'Missing Blender path must throw.'
    } catch {
        Assert-True $true 'Missing Blender path fails closed.'
    }
}
```

- [ ] **Step 2: Run tests and verify resolver tests fail**

Expected: exit `1` because the module/functions do not exist.

- [ ] **Step 3: Implement bounded resolver functions**

Create `ProjectRift.ArtPipeline.psm1` with these public contracts:

```powershell
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-ProjectRiftContainedArtPath {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Candidate, [Parameter(Mandatory)][string]$AllowedRoot)
    $root = [IO.Path]::GetFullPath($AllowedRoot).TrimEnd('\')
    $path = [IO.Path]::GetFullPath($Candidate)
    return $path.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Resolve-ProjectRiftBlenderExecutable {
    [CmdletBinding()]
    param([string]$ExplicitPath)
    $candidates = @()
    if ($ExplicitPath) { $candidates += $ExplicitPath }
    if ($env:PROJECTRIFT_BLENDER_EXE) { $candidates += $env:PROJECTRIFT_BLENDER_EXE }
    $pathCommand = Get-Command blender -ErrorAction SilentlyContinue
    if ($pathCommand) { $candidates += $pathCommand.Source }

    foreach ($candidate in $candidates) {
        if (-not $candidate) { continue }
        $full = [IO.Path]::GetFullPath($candidate)
        if ((Test-Path -LiteralPath $full -PathType Leaf) -and [IO.Path]::GetFileName($full) -ieq 'blender.exe') {
            return $full
        }
    }
    throw 'Blender 4.5 LTS is unavailable. Install it and pass -BlenderExe, or set PROJECTRIFT_BLENDER_EXE locally.'
}

Export-ModuleMember -Function Test-ProjectRiftContainedArtPath, Resolve-ProjectRiftBlenderExecutable
```

Do not scan `C:\Program Files`, user profiles, registries, or other project directories.

- [ ] **Step 4: Implement the single entry point**

`Invoke-ProjectRiftArtFirstArticle.ps1` must accept:

```powershell
[CmdletBinding()]
param(
    [ValidateSet('Preflight','BuildWhiteModel')]
    [string]$Stage = 'Preflight',
    [string]$BlenderExe
)
```

It must:

1. Resolve ProjectA root from `$PSScriptRoot`.
2. Resolve Blender using the module.
3. Execute `blender.exe --version` and reject any first line not matching `^Blender 4\.5\.`.
4. For `Preflight`, print the canonical executable and version, then exit `0`.
5. For `BuildWhiteModel`, invoke Task 3's script with an argument array, never string evaluation:

```powershell
$arguments = @(
    '--background',
    '--factory-startup',
    '--python', $builderScript,
    '--',
    '--project-root', $projectRoot,
    '--brief', $briefPath
)
& $resolvedBlender @arguments
if ($LASTEXITCODE -ne 0) { throw "Blender first-article build failed with exit code $LASTEXITCODE." }
```

- [ ] **Step 5: Run PowerShell tests**

Expected: all repository-level assertions pass without requiring a real Blender installation.

- [ ] **Step 6: Run real preflight or stop at the exact prerequisite**

Run:

```powershell
$env:PROJECTRIFT_BLENDER_EXE = 'C:\Program Files\Blender Foundation\Blender 4.5\blender.exe'
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftArtFirstArticle.ps1 -Stage Preflight
```

Expected: first output line identifies Blender `4.5.x` and exit `0`.

If the user has not yet installed Blender, stop here and ask them to install Blender 4.5 LTS from the official site, then provide/confirm the `blender.exe` path. Do not install system software outside ProjectA automatically.

- [ ] **Step 7: User Git checkpoint**

Suggest commit message `tools: add bounded Blender first-article runner`. Do not stage or commit.

---

### Task 3: Generate and Validate the Dimensionally Correct White Model

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/blender/shiphub_common.py`
- Create: `Scripts/ProjectRift/ArtPipeline/blender/build_shiphub_wall_door.py`
- Modify: `Scripts/ProjectRift/Tests/Test-ProjectRiftArtPipeline.ps1`
- Generate: `SourceArt/ProjectRift/ShipHub/Blender/SM_ShipHub_WallDoor_400_A.blend`
- Generate: `SourceArt/ProjectRift/ShipHub/Exports/SM_ShipHub_WallDoor_400_A.fbx`
- Generate: white-model PNGs and validation JSON listed above.

**Interfaces:**
- Consumes: exact G0 JSON brief and Blender 4.5 LTS.
- Produces: deterministic geometry, render views, FBX, and validation report consumed by Task 4 and ImageGen in Task 5.

- [ ] **Step 1: Add failing output/report assertions**

Extend the PowerShell test so that, when the validation report exists, it verifies exact values:

```powershell
$reportPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\Reports\SM_ShipHub_WallDoor_400_A.validation.json'
if (Test-Path -LiteralPath $reportPath) {
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    Assert-Equal 'PASS' $report.Result 'Blender validation result.'
    Assert-Equal 400 $report.BoundsCm.Width 'Generated width.'
    Assert-Equal 30 $report.BoundsCm.Depth 'Generated depth.'
    Assert-Equal 400 $report.BoundsCm.Height 'Generated height.'
    Assert-Equal 240 $report.DoorOpeningCm.Width 'Generated door width.'
    Assert-Equal 280 $report.DoorOpeningCm.Height 'Generated door height.'
    Assert-Equal 3 $report.CollisionPieceCount 'Door collision piece count.'
    Assert-Equal 1 $report.RenderMaterialSlotCount 'White-model material slot count.'
    Assert-True $report.PivotAtOrigin 'Pivot must be at the bottom-left-back origin.'
}
```

Change the test parameter block to:

```powershell
[CmdletBinding()]
param(
    [switch]$RequireGeneratedArtifacts,
    [switch]$RequireConceptPackage
)
```

When `-RequireGeneratedArtifacts` is set, assert all `.blend`, FBX, nine white renders, contact sheet, and report exist. This keeps ordinary repository tests runnable before Blender output is generated.

- [ ] **Step 2: Implement shared Blender helpers**

`shiphub_common.py` must define these exact constants and helpers:

```python
ASSET_ID = "SM_ShipHub_WallDoor_400_A"
MODULE_WIDTH_M = 4.0
MODULE_DEPTH_M = 0.30
MODULE_HEIGHT_M = 4.0
DOOR_WIDTH_M = 2.40
DOOR_HEIGHT_M = 2.80

def create_box(name: str, dimensions_m: tuple[float, float, float], location_m: tuple[float, float, float]): ...
def apply_all_transforms(obj) -> None: ...
def ensure_collection(name: str): ...
def add_white_material(): ...
def join_render_geometry(objects, asset_id: str): ...
def write_json(path: str, payload: dict) -> None: ...
```

Scene settings:

```python
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.render.resolution_x = 1024
scene.render.resolution_y = 1024
scene.render.resolution_percentage = 100
```

- [ ] **Step 3: Implement deterministic wall-door geometry**

The render mesh comprises three boxes with no geometry across the door opening:

```python
left_jamb = create_box("Wall_Left", (0.80, 0.30, 4.00), (0.40, 0.15, 2.00))
right_jamb = create_box("Wall_Right", (0.80, 0.30, 4.00), (3.60, 0.15, 2.00))
lintel = create_box("Wall_Lintel", (2.40, 0.30, 1.20), (2.00, 0.15, 3.40))
render_mesh = join_render_geometry([left_jamb, right_jamb, lintel], ASSET_ID)
```

This yields:

- bounds: X `0.0–4.0 m`, Y `0.0–0.3 m`, Z `0.0–4.0 m`;
- door opening: X `0.8–3.2 m`, Z `0.0–2.8 m`;
- pivot/object origin: `(0, 0, 0)`;
- one white render material slot.

Create three hidden collision boxes named:

```text
UCX_SM_ShipHub_WallDoor_400_A_00
UCX_SM_ShipHub_WallDoor_400_A_01
UCX_SM_ShipHub_WallDoor_400_A_02
```

They must match the left jamb, right jamb, and lintel and must never span the doorway.

- [ ] **Step 4: Add first-article structural language without changing the envelope**

Use non-destructive or generated secondary geometry inside the 4×0.3×4 m envelope:

- 5 cm front-face chamfers on the outer jamb edges;
- a 4 cm recessed reveal around the 240×280 cm opening;
- two vertical 8 cm service channels, one on each jamb;
- one 12 cm lintel maintenance recess;
- no bolts, decals, damage, loose cables, or rift growth at G1.

The doorway and outer snap boundaries must remain exact after bevels.

- [ ] **Step 5: Add deterministic review cameras and lighting**

Create cameras named:

```text
CAM_Front, CAM_Back, CAM_Left, CAM_Right, CAM_Top,
CAM_PerspectiveFront, CAM_PerspectiveBack, CAM_PerspectiveDetail
```

Orthographic views use an orthographic scale of `5.2 m`; perspective views use a 50 mm lens. Use a neutral three-light studio with no colored lighting. Render transparent backgrounds off; use a uniform 18% gray world so edges remain readable.

- [ ] **Step 6: Export source, FBX, renders, contact sheet, and report**

Save the `.blend`, then export selected render mesh plus collision with:

```python
bpy.ops.export_scene.fbx(
    filepath=fbx_path,
    use_selection=True,
    object_types={'MESH'},
    apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_UNITS',
    axis_forward='-Y',
    axis_up='Z',
    use_mesh_modifiers=True,
    mesh_smooth_type='FACE',
    add_leaf_bones=False,
    bake_anim=False,
)
```

Write the validation JSON from this Python payload so the brief hash is always the actual source hash:

```python
payload = {
    "SchemaVersion": 1,
    "AssetId": "SM_ShipHub_WallDoor_400_A",
    "Result": "PASS",
    "BoundsCm": {"Width": 400, "Depth": 30, "Height": 400},
    "DoorOpeningCm": {"Width": 240, "Height": 280},
    "PivotAtOrigin": True,
    "CollisionPieceCount": 3,
    "RenderMaterialSlotCount": 1,
    "NonManifoldEdgeCount": 0,
    "GeneratedBy": "Blender 4.5 LTS project-local Python",
    "BriefSha256": hashlib.sha256(Path(brief_path).read_bytes()).hexdigest(),
}
```

Build the contact sheet using Blender compositor or Pillow only if Pillow is already available. If Pillow is unavailable, render a ninth Blender scene containing the eight view planes; do not add a new package dependency silently.

- [ ] **Step 7: Run headless generation**

Run:

```powershell
$env:PROJECTRIFT_BLENDER_EXE = 'C:\Program Files\Blender Foundation\Blender 4.5\blender.exe'
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftArtFirstArticle.ps1 -Stage BuildWhiteModel
```

Expected: exit `0`, one `.blend`, one FBX, nine PNGs, and validation JSON with `Result=PASS`.

- [ ] **Step 8: Run generated-artifact tests**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftArtPipeline.ps1 -RequireGeneratedArtifacts
```

Expected: PASS with exact dimension, collision, pivot, and artifact assertions.

- [ ] **Step 9: User Git checkpoint**

Suggest commit message `art: generate ship hub door-wall white model`. Do not stage or commit.

---

### Task 4: Perform G1 White-Model Review

**Files:**
- Inspect: `SourceArt/ProjectRift/ShipHub/Concept/SM_ShipHub_WallDoor_400_A/white-contact-sheet.png`
- Inspect: `SourceArt/ProjectRift/ShipHub/Briefs/Reports/SM_ShipHub_WallDoor_400_A.validation.json`
- Modify after user decision: `SourceArt/ProjectRift/ShipHub/Briefs/SM_ShipHub_WallDoor_400_A.approval.json`

**Interfaces:**
- Consumes: deterministic Task 3 outputs.
- Produces: explicit G1 approval required before ImageGen calls.

- [ ] **Step 1: Inspect images at original resolution**

Use `view_image` on the contact sheet and the front, perspective-front, perspective-back, and perspective-detail renders.

Reject before user review if:

- the doorway is not visibly open;
- left/right jamb widths differ without design intent;
- the 4 cm reveal or service channels break snap boundaries;
- the object clips the render frame;
- collision geometry is visible in beauty renders;
- the result resembles a flat Cube cutout rather than a designed structural module.

- [ ] **Step 2: Verify the report independently**

Run the generated-artifact PowerShell test and also inspect the JSON. The report must show 400×30×400 cm, 240×280 cm opening, three collision pieces, pivot true, and zero non-manifold edges.

- [ ] **Step 3: Present the G1 contact sheet to the user**

Explain that this is a real 3D asset white model, not the current map and not final concept art. Ask only whether the structure and proportions are approved.

- [ ] **Step 4: Record the decision**

On approval, update only the G1 object:

```json
"G1": {
  "Status": "Approved",
  "Date": "2026-07-31",
  "Evidence": [
    "Concept/SM_ShipHub_WallDoor_400_A/white-contact-sheet.png",
    "Briefs/Reports/SM_ShipHub_WallDoor_400_A.validation.json"
  ]
}
```

If rejected, keep `Status: Pending`, append a `RejectionNotes` array with the user's exact requested changes, return to Task 3, and rerun all Task 3 verification.

---

### Task 5: Generate Three Geometry-Preserving Surface Concepts

**Files:**
- Reference: `white-perspective-front.png`
- Create: `concept-surface-a.png`
- Create: `concept-surface-b.png`
- Create: `concept-surface-c.png`
- Modify: `SM_ShipHub_WallDoor_400_A.generation-ledger.json`

**Interfaces:**
- Consumes: G1-approved white-model render.
- Produces: three surface-treatment choices within the already approved art direction.

- [ ] **Step 1: Confirm G1 is approved and load the reference image**

Read `approval.json`; do not proceed unless `G1.Status` is exactly `Approved`. Load `white-perspective-front.png` with `view_image` so it is visible to built-in ImageGen.

- [ ] **Step 2: Generate Surface A — Fleet-maintained survivor**

Use built-in ImageGen edit mode with this prompt:

```text
Use case: sketch-to-render
Asset type: ProjectRift UE5.8 modular ship-hub first-article concept
Input image: the ProjectA-owned Blender white-model render is the geometry authority and edit target
Primary request: paint over the existing 400 cm wide modular bulkhead door-wall as a grounded realistic PBR hard-surface game asset, using the approved Broken-Spine Expedition Ship language
Subject: human-built expedition-vessel structural module with a 240×280 cm open doorway, exposed load-bearing frame, chamfered armor panels, service channels, and credible maintenance access
Surface treatment A: disciplined fleet construction that survived a crash; mostly intact dark gunmetal frame, cool-gray painted panels, restrained edge wear, a few replaced fasteners, subtle soot near the upper maintenance recess
Composition: preserve the exact camera, silhouette, opening, module boundaries, proportions, perspective, and all structural recesses from the input
Lighting: neutral studio asset-review lighting; no environment scene
Color palette: dark blue-gray frame, cool gray panels, tiny maintenance-amber identifiers; no purple
Materials: painted metal, exposed steel only at believable chips, black rubber seals, realistic roughness variation
Constraints: change surface design only; keep the doorway fully open; do not add a room, floor, ceiling, character, weapon, crate, text, logo, watermark, hanging cable, organic growth, glowing purple energy, excessive greebles, or geometry outside the existing envelope
```

- [ ] **Step 3: Generate Surface B — Field-patched utility**

Use the same invariant prompt, changing only the treatment:

```text
Surface treatment B: visibly field-patched after an alien-world crash; one cool-gray replacement armor plate on each jamb, restrained exposed fasteners, two small maintenance-amber status strips, localized soot and abrasion, still structurally organized and professionally repairable
```

- [ ] **Step 4: Generate Surface C — Structural trauma**

Use the same invariant prompt, changing only the treatment:

```text
Surface treatment C: deeper but believable structural trauma; more exposed dark load-bearing frame, one missing cosmetic cover and a repaired lintel seam, no deformation of the door opening or snap boundary, no rift contamination, no purple emissive, and no post-apocalyptic junk collage
```

Issue one built-in ImageGen call per variant. Do not use a batch or CLI fallback.

- [ ] **Step 5: Save selected outputs into SourceArt**

Copy the three generated outputs from `$CODEX_HOME/generated_images/...` into the exact concept paths. Do not overwrite an existing variant; if rerun, use `concept-surface-a-v2.png` and record the superseded result.

- [ ] **Step 6: Record generation provenance**

Append one entry per call with:

```json
{
  "Id": "surface-a-v1",
  "Tool": "OpenAI built-in ImageGen",
  "Mode": "edit",
  "Input": "Concept/SM_ShipHub_WallDoor_400_A/white-perspective-front.png",
  "Output": "Concept/SM_ShipHub_WallDoor_400_A/concept-surface-a.png",
  "PromptSha256": "computed from the exact UTF-8 prompt",
  "CreatedAt": "ISO-8601 timestamp",
  "Status": "Candidate",
  "CommercialReview": "RequiredBeforeUEImport"
}
```

Store the full exact prompt beside each entry in a `Prompt` string; do not store only the hash.

- [ ] **Step 7: Validate generated concepts**

Inspect all three at original resolution. Reject a candidate if it changes the outer envelope or doorway by more than the visible white-model edge thickness, closes the opening, invents an unseen room, adds purple/rift language, includes text/watermarks, or introduces impossible panel connections.

One targeted regeneration is allowed for a rejected candidate. If built-in ImageGen fails or all candidates drift after the approved three-round maximum, stop and report the G2 blocker. Mention that CLI fallback requires `OPENAI_API_KEY` and new explicit user approval; do not invoke it.

- [ ] **Step 8: Present only valid A/B/C concepts to the user**

Ask the user to choose A, B, C, or request one single targeted change. Do not begin production modeling in the same turn.

---

### Task 6: Build and Approve the G2 Concept Package

**Files:**
- Create: `concept-approved-keyframe.png`
- Create: `concept-state-damaged.png`
- Create: `concept-state-patched.png`
- Create: `concept-state-online.png`
- Create: `material-callouts.md`
- Modify: `SM_ShipHub_WallDoor_400_A.asset.json`
- Modify: `SM_ShipHub_WallDoor_400_A.generation-ledger.json`
- Modify: `SM_ShipHub_WallDoor_400_A.approval.json`
- Test: `Scripts/ProjectRift/Tests/Test-ProjectRiftArtPipeline.ps1`

**Interfaces:**
- Consumes: one user-selected surface concept and G1 geometry.
- Produces: approved G2 package that the later production-modeling plan can consume without guessing.

- [ ] **Step 1: Copy the selected concept non-destructively**

Copy the selected candidate to `concept-approved-keyframe.png`; retain A/B/C originals and record the selected candidate ID in the ledger.

- [ ] **Step 2: Generate the three state studies as geometry-preserving edits**

Load the approved keyframe and issue one built-in edit call per state. Repeat these invariants in every prompt:

```text
Keep the exact camera, geometry, 400 cm module envelope, 240×280 cm open doorway, panel boundaries, and approved material identity. Change only repair state indicators. No room, characters, text, watermark, purple energy, organic growth, or new silhouette geometry.
```

State-specific requirements:

- **Damaged:** power off, soot and dust, one removable cosmetic cover absent, exposed frame remains safe, no emissive;
- **Patched:** replacement plates and restrained repair hardware, maintenance-amber status strips, localized cleaning around repaired joints;
- **Online:** covers secured, cold-white work light and small cyan functional feedback, no pristine factory-new look.

- [ ] **Step 3: Write material and construction callouts**

Create `material-callouts.md` with exact sections:

```markdown
# SM_ShipHub_WallDoor_400_A material callouts

## Geometry authority
- Dimensions: 400×30×400 cm
- Door clear opening: 240×280 cm
- Pivot: bottom-left-back
- Snap: 50 cm

## Material families
- Dark painted structural steel: Metallic 0 for coating, roughness 0.48–0.68
- Exposed steel at chips: Metallic 1, roughness 0.28–0.52
- Cool-gray armor coating: Metallic 0, roughness 0.42–0.62
- Black rubber seals: Metallic 0, roughness 0.62–0.82

## Color roles
- Dark frame: approximately 70%
- Cool-gray panel/reveal surfaces: approximately 20%
- Maintenance amber: patched state only and below 5%
- Functional cyan: online state only and below 3%
- Rift purple: prohibited

## Production constraints
- Maximum two material slots
- Shared trim/detail system; no unique 4K wall texture
- Damage, repair plate, decal, and light elements remain removable overlays
- AI concepts are reference only; PBR maps must be authored and validated in the next version
```

- [ ] **Step 4: Extend self-tests for G2 package completeness**

Add `-RequireConceptPackage`. When set, assert the approved keyframe, three state studies, and material callouts exist; assert `asset.json.Stage == 'G2'` and `approval.json.G2.Status == 'Approved'`.

- [ ] **Step 5: Present the complete G2 package for user review**

Show the keyframe and three state studies together. Summarize geometry invariants and material callouts. Ask whether G2 is approved.

- [ ] **Step 6: Record G2 approval only after the user explicitly approves**

Update `asset.json`:

```json
"Stage": "G2"
```

Then set the selected concept programmatically so the recorded value matches the user's actual response:

```powershell
$selectedConceptId = @{ A = 'surface-a-v1'; B = 'surface-b-v1'; C = 'surface-c-v1' }[$userChoice]
$approval = Get-Content -LiteralPath $approvalPath -Raw | ConvertFrom-Json
$approval.G2.Status = 'Approved'
$approval.G2.Date = '2026-07-31'
$approval.G2 | Add-Member -Force -NotePropertyName SelectedConceptId -NotePropertyValue $selectedConceptId
$approval.G2 | Add-Member -Force -NotePropertyName Evidence -NotePropertyValue @(
    'Concept/SM_ShipHub_WallDoor_400_A/concept-approved-keyframe.png',
    'Concept/SM_ShipHub_WallDoor_400_A/concept-state-damaged.png',
    'Concept/SM_ShipHub_WallDoor_400_A/concept-state-patched.png',
    'Concept/SM_ShipHub_WallDoor_400_A/concept-state-online.png',
    'Concept/SM_ShipHub_WallDoor_400_A/material-callouts.md'
)
$approval | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $approvalPath -Encoding UTF8
```

- [ ] **Step 7: Run final first-version verification**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftArtPipeline.ps1 -RequireGeneratedArtifacts -RequireConceptPackage
git status --short --branch
```

Expected:

- PowerShell self-test exits `0`;
- no `.uasset` or `.umap` is changed;
- no change to `L_ShipLobby`;
- only the plan-approved repository/source-art files are present;
- G-1, G0, G1, and G2 have all received explicit user approval; G0, G1, and G2 are recorded in the first-article approval ledger, and the approved G-1 source/prompt is recorded in the generation ledger.

- [ ] **Step 8: Stop at the version gate**

Report changed files, verification evidence, remaining risks, manual review steps, and suggested commit message `art: approve ship hub door-wall first article concept`. Do not stage or commit. Do not write or start the G3 production-model/UE-import plan until the user explicitly requests it.

---

## Manual Acceptance Checklist

The user only needs to perform these decisions/actions:

1. Approve or reject the G-1 overall ship design.
2. After G-1 approval, install Blender 4.5 LTS or provide an exact existing `blender.exe` path.
3. Approve or reject the G1 white-model proportions.
4. Choose one valid A/B/C surface concept.
5. Approve or reject the final G2 keyframe and three-state concept package.

The user does not need to model, UV, texture, export, or operate Unreal Editor in this version.

## Out of Scope for This Plan

- Production bevel/topology pass beyond the approved white model;
- UV unwrap and bake-ready production mesh;
- Trim Sheet, texture maps, or UE materials;
- Substance comparison;
- Unreal import, license-registry entry, Blueprint, collision validation in UE, Nanite/LOD, or `PRProductionValidation` against a new asset;
- `L_ShipHub_ArtValidation` creation;
- full module kit or production lobby layout;
- any modification of `L_ShipLobby`.

## Reference Documents

- `docs/superpowers/specs/2026-07-31-projectrift-ship-hub-art-pipeline-design.md`
- `docs/projectrift/production/AssetNamingAndDirectories.md`
- `docs/projectrift/production/AssetImportAndLicenseChecklist.md`
- `docs/projectrift/production/PerformanceBudget.md`
- Blender 4.5 command line: <https://docs.blender.org/manual/en/4.5/advanced/command_line/arguments.html>
