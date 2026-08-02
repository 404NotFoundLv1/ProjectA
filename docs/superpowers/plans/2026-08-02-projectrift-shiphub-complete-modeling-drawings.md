# ProjectRift Ship Hub Complete Modeling Drawings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build one dimensionally authoritative Blender white model for the approved ProjectRift ship hub and publish a complete, verified modeling-drawing package containing plans, elevations, sections, detail sheets, exports and consistent perspective views.

**Architecture:** A versioned JSON brief is the machine-readable dimension contract. Pure Python validates the layout before Blender runs; Blender 5.2 LTS generates the single source model, exports FBX/GLB and renders deterministic orthographic bases; a separate bundled-Python publisher adds vector dimensions and assembles the A3 PDF. Every downstream artifact consumes the same manifest emitted from the Blender scene.

**Tech Stack:** PowerShell 7/Windows PowerShell, Python 3, Blender 5.2 LTS Python API, Pillow 12.2.0, ReportLab 4.4.9, pypdf 6.10.0, JSON, SVG, FBX, glTF/GLB.

## Global Constraints

- Operate only in `E:\MyWork\ProjectA` and on ProjectA-owned files and processes.
- Work directly in the existing `main` workspace; do not create a branch or worktree.
- The user performs staging and commits. Do not run `git add`, `git commit`, `git tag`, `git push`, `git stash`, `git reset`, `git restore` or `git checkout`.
- Do not modify or save any UE `.uasset` or `.umap`; `/Game/ProjectRift/Maps/L_ShipLobby` remains untouched.
- Do not probe or use Unreal MCP port 8000; Unreal is not required for this design-drawing version.
- Geometry authority is `SourceArt/ProjectRift/ShipHub/CompleteDesign/Blender/SM_ShipHub_Complete_White_v1.blend`.
- Interior clear dimensions are exactly 28 m × 24 m × 7 m; nominal structural height is 8 m.
- Structural grid is 4 m; regular snap is 0.5 m; module widths are 1 / 2 / 4 m.
- Central navigation table is exactly 8 m diameter and 1.1 m high.
- There are exactly five 18-degree backward-reclined hibernation pods and exactly four flush construct docks.
- Blender must be 5.2.x LTS. The user-provided executable is `D:\Blender5.2\blender.exe`. No system software download or installation is authorized implicitly; if preflight cannot resolve that executable, stop and report the ProjectA-local blocker.
- Existing concept images are visual references only; never infer dimensions from image pixels.
- Each task ends with a user Git checkpoint report and suggested commit message only; no Git mutation.

---

## File Structure

### Authored source and tools

- `SourceArt/ProjectRift/ShipHub/Briefs/ShipHubCompleteDesign_v1.json` — frozen machine-readable geometry and deliverable contract.
- `Scripts/ProjectRift/ArtPipeline/ProjectRift.ArtPipeline.psm1` — bounded executable/path resolution and invocation helpers.
- `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1` — one public stage runner.
- `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_contract.py` — typed contract loader and semantic validation.
- `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_layout.py` — pure layout math, clearances and sheet metadata.
- `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_blender.py` — Blender scene and object construction helpers.
- `Scripts/ProjectRift/ArtPipeline/shiphub/build_shiphub_design.py` — Blender white-model entry point.
- `Scripts/ProjectRift/ArtPipeline/shiphub/render_shiphub_drawings.py` — Blender camera, section and render entry point.
- `Scripts/ProjectRift/ArtPipeline/shiphub/publish_shiphub_drawings.py` — deterministic SVG/PNG/PDF publisher.
- `Scripts/ProjectRift/ArtPipeline/shiphub/validate_shiphub_blender.py` — Blender-native scene and export re-import validation.
- `Scripts/ProjectRift/ArtPipeline/shiphub/validate_shiphub_package.py` — final package validator and report writer.
- `Scripts/ProjectRift/Tests/test_shiphub_layout.py` — pure Python contract/layout tests.
- `Scripts/ProjectRift/Tests/test_shiphub_publish.py` — pure Python publishing tests.
- `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubDesign.ps1` — repository, resolver and output contract tests.

### Generated design authority and deliverables

- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Blender/SM_ShipHub_Complete_White_v1.blend`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Exports/SM_ShipHub_Complete_White_v1.fbx`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Exports/SM_ShipHub_Complete_White_v1.glb`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/PNG/A01_*.png` through `A10_*.png`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/PNG/D01_*.png` through `D05_*.png`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/SVG/A01_*.svg` through `A10_*.svg`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/SVG/D01_*.svg` through `D05_*.svg`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/ProjectRift_ShipHub_CompleteDesign_v1.pdf`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/ProjectRift_ShipHub_ContactSheet_v1.png`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Reports/layout-manifest.json`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Reports/export-validation.json`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Reports/validation-report.json`
- `SourceArt/ProjectRift/ShipHub/CompleteDesign/Reports/SHA256SUMS.txt`

---

### Task 1: Freeze the Machine-Readable Source Contract

**Files:**
- Create: `SourceArt/ProjectRift/ShipHub/Briefs/ShipHubCompleteDesign_v1.json`
- Create: `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubDesign.ps1`

**Interfaces:**
- Consumes: approved design spec `docs/superpowers/specs/2026-08-01-projectrift-shiphub-complete-modeling-drawings-design.md`.
- Produces: schema `projectrift.shiphub.complete-design.v1` consumed by every later task.

- [ ] **Step 1: Create the failing repository contract test**

Create `Test-ProjectRiftShipHubDesign.ps1` with the existing ProjectRift assertion pattern and these initial checks:

```powershell
[CmdletBinding()]
param([switch]$RequireGeneratedArtifacts)

$ErrorActionPreference = 'Stop'
$script:FailureCount = 0
$script:AssertionCount = 0
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$briefPath = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub\Briefs\ShipHubCompleteDesign_v1.json'

function Assert-True([bool]$Condition, [string]$Message) {
    $script:AssertionCount++
    if (-not $Condition) {
        $script:FailureCount++
        Write-Error "ASSERTION FAILED: $Message" -ErrorAction Continue
    }
}

function Assert-Equal($Expected, $Actual, [string]$Message) {
    Assert-True ($Expected -eq $Actual) "$Message (expected '$Expected', actual '$Actual')"
}

Assert-True (Test-Path -LiteralPath $briefPath -PathType Leaf) 'Complete-design brief must exist.'
if (Test-Path -LiteralPath $briefPath -PathType Leaf) {
    $brief = Get-Content -Raw -Encoding UTF8 $briefPath | ConvertFrom-Json
    Assert-Equal 'projectrift.shiphub.complete-design.v1' $brief.Schema 'Brief schema.'
    Assert-Equal 28.0 $brief.Room.ClearWidthM 'Room clear width.'
    Assert-Equal 24.0 $brief.Room.ClearDepthM 'Room clear depth.'
    Assert-Equal 7.0 $brief.Room.ClearHeightM 'Room clear height.'
    Assert-Equal 8.0 $brief.NavigationTable.DiameterM 'Navigation-table diameter.'
    Assert-Equal 5 $brief.Cryopods.Count 'Cryopod count.'
    Assert-Equal 18.0 $brief.Cryopods.ReclineDegrees 'Cryopod recline.'
    Assert-Equal 4 $brief.ConstructDocks.Count 'Construct-dock count.'
    Assert-Equal 15 $brief.Deliverables.Sheets.Count 'Sheet count.'
}

if ($script:FailureCount -gt 0) {
    Write-Host "ProjectRift ship-hub design self-test: FAIL ($script:FailureCount/$script:AssertionCount)."
    exit 1
}
Write-Host "ProjectRift ship-hub design self-test: PASS ($script:AssertionCount assertions)."
exit 0
```

- [ ] **Step 2: Run the test and verify the missing brief fails**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubDesign.ps1
```

Expected: exit `1` and `Complete-design brief must exist`.

- [ ] **Step 3: Create the exact JSON brief**

Create `ShipHubCompleteDesign_v1.json` with this structure and exact values:

```json
{
  "Schema": "projectrift.shiphub.complete-design.v1",
  "Units": { "Blender": "m", "UnrealCentimetersPerMeter": 100, "SnapM": 0.5, "StructuralBayM": 4.0 },
  "Axes": { "East": "+X", "NorthPods": "+Y", "Up": "+Z", "Origin": "Deck center" },
  "Room": {
    "ClearWidthM": 28.0, "ClearDepthM": 24.0, "ClearHeightM": 7.0,
    "NominalHeightM": 8.0, "FloorThicknessM": 0.4, "WallThicknessM": 0.4,
    "InnerVerticesM": [[-10,-12],[10,-12],[14,-8],[14,8],[10,12],[-10,12],[-14,8],[-14,-8]],
    "MainPathMinWidthM": 5.0
  },
  "NavigationTable": { "CenterM": [0,0,0], "DiameterM": 8.0, "HeightM": 1.1, "DisplayDiameterM": 6.0 },
  "Cryopods": {
    "Count": 5, "CentersXM": [-4,-2,0,2,4], "BaseYM": 9.8,
    "BoundsM": [1.6,1.6,3.0], "ReclineDegrees": 18.0,
    "DoorOpenDegrees": 75.0, "DoorEnvelopeSouthM": 1.2,
    "InteractionClearanceM": [2.0,2.5], "ExpansionInterfaceWidthM": 2.0
  },
  "Airlock": { "CenterM": [0,-12,0], "ClearOpeningM": [4.0,3.5], "DepthM": 1.2, "MusterAreaM": [8.0,5.0] },
  "WestBays": { "PreparationYRangeM": [-8,0], "RepairCenterM": [-14,6,1.4] },
  "EastBays": { "MedicalYRangeM": [-8,-4], "RoleYRangeM": [-4,-2], "JumpCoreCenterM": [14,6,3.0] },
  "ConstructDocks": { "Count": 4, "DiameterM": 1.0, "RecessM": 0.08, "CentersM": [[-5.3,-5.3,0],[5.3,-5.3,0],[-5.3,5.3,0],[5.3,5.3,0]] },
  "CeilingRing": { "InnerDiameterM": 10.0, "OuterDiameterM": 16.0, "LowestZM": 6.2, "HighestZM": 8.0, "Walkable": false },
  "CharacterReference": { "CapsuleRadiusM": 0.42, "CapsuleHalfHeightM": 0.96, "CameraArmM": 4.0 },
  "Deliverables": {
    "Sheets": [
      "A01_FloorPlan","A02_ReflectedCeilingPlan","A03_NorthElevation","A04_SouthElevation","A05_WestElevation","A06_EastElevation","A07_LongitudinalSection","A08_TransverseSection","A09_ExplodedModulePlan","A10_PerspectiveSheet","D01_Cryopod","D02_NavigationTable","D03_MainAirlock","D04_ConstructDock","D05_WallBayInterface"
    ],
    "Formats": ["blend","fbx","glb","pdf","png","svg","json","txt"]
  }
}
```

- [ ] **Step 4: Run the repository contract test**

Run the command from Step 2.

Expected: exit `0` and `ProjectRift ship-hub design self-test: PASS`.

- [ ] **Step 5: User Git checkpoint**

Report the two created files and suggest commit message `art: freeze ship hub complete-design contract`. Do not stage or commit.

---

### Task 2: Add the Bounded Tool Resolver and Stage Runner

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/ProjectRift.ArtPipeline.psm1`
- Create: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1`
- Modify: `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubDesign.ps1`

**Interfaces:**
- Consumes: explicit `-BlenderExe`, explicit `-PythonExe`, or task-specific environment variables `PROJECTRIFT_BLENDER_EXE` and `PROJECTRIFT_PYTHON_EXE`.
- Produces: `Resolve-ProjectRiftBlenderExecutable`, `Resolve-ProjectRiftPythonExecutable`, `Test-ProjectRiftContainedArtPath`, and stages `Preflight`, `ValidateContract`, `BuildWhiteModel`, `RenderDrawings`, `Publish`, `Validate`, `All`.

- [ ] **Step 1: Add failing resolver and containment assertions**

Before the failure-count block in the PowerShell test, add:

```powershell
$modulePath = Join-Path $projectRoot 'Scripts\ProjectRift\ArtPipeline\ProjectRift.ArtPipeline.psm1'
Assert-True (Test-Path -LiteralPath $modulePath -PathType Leaf) 'Art pipeline module must exist.'
if (Test-Path -LiteralPath $modulePath -PathType Leaf) {
    Import-Module -Force $modulePath
    $sourceRoot = Join-Path $projectRoot 'SourceArt\ProjectRift\ShipHub'
    Assert-True (Test-ProjectRiftContainedArtPath -Candidate (Join-Path $sourceRoot 'CompleteDesign\candidate') -AllowedRoot $sourceRoot) 'A SourceArt child path must pass.'
    Assert-True (-not (Test-ProjectRiftContainedArtPath -Candidate $sourceRoot -AllowedRoot $sourceRoot)) 'The SourceArt root itself must fail.'
    Assert-True (-not (Test-ProjectRiftContainedArtPath -Candidate (Join-Path $projectRoot 'Content\ProjectRift') -AllowedRoot $sourceRoot)) 'Content must remain outside the art-output boundary.'
    try { Resolve-ProjectRiftBlenderExecutable -ExplicitPath (Join-Path $projectRoot 'missing\blender.exe') | Out-Null; Assert-True $false 'Missing Blender must throw.' } catch { Assert-True $true 'Missing Blender fails closed.' }
}
```

- [ ] **Step 2: Run the PowerShell test and verify it fails**

Expected: exit `1` because the module does not exist.

- [ ] **Step 3: Implement the bounded module**

Create `ProjectRift.ArtPipeline.psm1` with strict mode and these signatures:

```powershell
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-ProjectRiftContainedArtPath {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Candidate,[Parameter(Mandatory)][string]$AllowedRoot)
    $root = [IO.Path]::GetFullPath($AllowedRoot).TrimEnd('\')
    $path = [IO.Path]::GetFullPath($Candidate)
    return $path.StartsWith($root + '\',[StringComparison]::OrdinalIgnoreCase)
}

function Resolve-ProjectRiftBlenderExecutable {
    [CmdletBinding()]
    param([string]$ExplicitPath)
    $candidates = @($ExplicitPath,$env:PROJECTRIFT_BLENDER_EXE)
    $command = Get-Command blender -ErrorAction SilentlyContinue
    if ($command) { $candidates += $command.Source }
    foreach ($candidate in $candidates) {
        if (-not $candidate) { continue }
        $full = [IO.Path]::GetFullPath($candidate)
        if ((Test-Path -LiteralPath $full -PathType Leaf) -and [IO.Path]::GetFileName($full) -ieq 'blender.exe') { return $full }
    }
    throw 'Blender 5.2 LTS is unavailable. Pass -BlenderExe or set PROJECTRIFT_BLENDER_EXE.'
}

function Resolve-ProjectRiftPythonExecutable {
    [CmdletBinding()]
    param([string]$ExplicitPath)
    foreach ($candidate in @($ExplicitPath,$env:PROJECTRIFT_PYTHON_EXE)) {
        if (-not $candidate) { continue }
        $full = [IO.Path]::GetFullPath($candidate)
        if (Test-Path -LiteralPath $full -PathType Leaf) { return $full }
    }
    throw 'A Python executable with Pillow, ReportLab and pypdf is required. Pass -PythonExe or set PROJECTRIFT_PYTHON_EXE.'
}

Export-ModuleMember -Function Test-ProjectRiftContainedArtPath,Resolve-ProjectRiftBlenderExecutable,Resolve-ProjectRiftPythonExecutable
```

Do not scan Program Files, registries, user profiles or other project directories.

- [ ] **Step 4: Implement the public stage runner**

`Invoke-ProjectRiftShipHubDesign.ps1` accepts:

```powershell
[CmdletBinding()]
param(
    [ValidateSet('Preflight','ValidateContract','BuildWhiteModel','RenderDrawings','Publish','Validate','All')]
    [string]$Stage = 'Preflight',
    [string]$BlenderExe,
    [string]$PythonExe
)
```

The runner resolves all paths from `$PSScriptRoot`, rejects any output outside `SourceArt\ProjectRift\ShipHub\CompleteDesign`, executes tools with argument arrays, and never uses `Invoke-Expression`. `Preflight` must run `blender.exe --version`, require `^Blender 5\.2\.\d+ LTS`, then run:

```powershell
& $python -c "import PIL, reportlab, pypdf; print('ProjectRift publishing dependencies: OK')"
if ($LASTEXITCODE -ne 0) { throw 'Publishing dependency preflight failed.' }
```

`All` recursively invokes stages in this order: `Preflight`, `ValidateContract`, `BuildWhiteModel`, `RenderDrawings`, `Publish`, `Validate`, forwarding the same explicit `-BlenderExe` and `-PythonExe` values to every stage. `BuildWhiteModel`, `RenderDrawings`, and `Validate` each resolve Blender and independently require first-line version output matching `^Blender 5\.2\.\d+ LTS`; `Publish` and every other Python-consuming stage independently resolve the supplied Python and propagate nonzero exits.

- [ ] **Step 5: Run repository tests**

Expected: PowerShell self-test passes without requiring real Blender because its ProjectA-local temporary executable probes verify resolver precedence, individual-stage version gates, recursive argument forwarding, fail-closed deferred scripts, and containment, then clean themselves up.

- [ ] **Step 6: Run real preflight and stop at the exact prerequisite**

Run with the bundled Python discovered in this desktop session:

```powershell
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubDesign.ps1 `
  -Stage Preflight `
  -PythonExe 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
```

Expected result with `-BlenderExe 'D:\Blender5.2\blender.exe'`: Blender reports `5.2.x LTS`, publishing dependencies report `OK`, and the stage exits `0`. Do not substitute another project's Blender process or tool.

- [ ] **Step 7: User Git checkpoint**

Suggest commit message `tools: add bounded ship hub design runner`. Do not stage or commit.

---

### Task 3: Implement and Test Pure Layout Geometry

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_contract.py`
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_layout.py`
- Create: `Scripts/ProjectRift/Tests/test_shiphub_layout.py`
- Modify: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1`

**Interfaces:**
- Consumes: `ShipHubCompleteDesign_v1.json`.
- Produces: `ShipHubContract`, `load_contract(path)`, `validate_contract(contract)`, `build_layout(contract)`, `validate_layout(layout)` and serializable layout data.

- [ ] **Step 1: Write failing pure-Python tests**

Create tests using `unittest` with assertions for exact vertices, room bounds, pod centers/count/recline, dock coordinates, table clearance, airlock muster area, service-ring heights and fifteen unique sheet IDs:

```python
from pathlib import Path
import sys, unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "Scripts/ProjectRift/ArtPipeline/shiphub"))
from shiphub_contract import load_contract
from shiphub_layout import build_layout, validate_layout

class ShipHubLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        brief = ROOT / "SourceArt/ProjectRift/ShipHub/Briefs/ShipHubCompleteDesign_v1.json"
        cls.contract = load_contract(brief)
        cls.layout = build_layout(cls.contract)

    def test_room_contract(self):
        self.assertEqual((28.0, 24.0, 7.0), self.layout.clear_dimensions_m)
        self.assertEqual(8, len(self.layout.inner_vertices_m))

    def test_hero_counts(self):
        self.assertEqual(5, len(self.layout.cryopods))
        self.assertTrue(all(p.recline_degrees == 18.0 for p in self.layout.cryopods))
        self.assertEqual(4, len(self.layout.construct_docks))

    def test_navigation_clearance(self):
        self.assertGreaterEqual(self.layout.minimum_main_path_width_m(), 5.0)

    def test_validation_is_clean(self):
        self.assertEqual([], validate_layout(self.layout))

if __name__ == "__main__": unittest.main()
```

- [ ] **Step 2: Run the tests and verify imports fail**

Run:

```powershell
& 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest Scripts.ProjectRift.Tests.test_shiphub_layout -v
```

Expected: import failure for `shiphub_contract`.

- [ ] **Step 3: Implement typed contract loading**

Use frozen dataclasses for `RoomContract`, `NavigationTableContract`, `CryopodContract`, `AirlockContract`, `DockContract`, `CeilingRingContract` and `ShipHubContract`. `load_contract(Path)` must reject wrong schema, missing keys, non-finite numbers, wrong counts and duplicate sheet IDs with explicit `ValueError` messages.

Also implement `main(argv: list[str] | None = None) -> int` in `shiphub_contract.py`. It accepts `--brief` and `--out`, calls `load_contract`, `build_layout` and `validate_layout`, returns `2` for invalid contracts, and writes the serializable layout preview only when there are zero issues.

- [ ] **Step 4: Implement pure layout objects and clearance math**

`shiphub_layout.py` defines frozen `LayoutObject`, `CryopodPlacement`, `ShipHubLayout` and:

```python
def build_layout(contract: ShipHubContract) -> ShipHubLayout: ...
def validate_layout(layout: ShipHubLayout) -> list[str]: ...
```

Compute the table-to-wall clear annulus analytically using the octagonal inner boundary and the 4 m table radius. Treat flush docks as non-obstructing. Validate exact pod X positions, 18-degree rotation, airlock opening, wall-bay ranges and ceiling-ring Z bounds.

- [ ] **Step 5: Run pure layout tests**

Expected: all tests pass and `validate_layout` returns no issues.

- [ ] **Step 6: Wire `ValidateContract` into the stage runner**

Invoke:

```powershell
& $python $contractScript --brief $briefPath --out $layoutPreviewPath
```

Write only `Saved\Automation\ProjectRiftShipHubDesign\layout-preview.json` during this pre-Blender validation stage.

- [ ] **Step 7: User Git checkpoint**

Suggest commit message `tools: validate ship hub layout contract`. Do not stage or commit.

---

### Task 4: Generate the Dimensionally Authoritative Blender White Model

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_blender.py`
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/build_shiphub_design.py`
- Modify: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1`
- Modify: `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubDesign.ps1`
- Generate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Blender/SM_ShipHub_Complete_White_v1.blend`
- Generate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Exports/SM_ShipHub_Complete_White_v1.fbx`
- Generate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Exports/SM_ShipHub_Complete_White_v1.glb`
- Generate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Reports/layout-manifest.json`

**Interfaces:**
- Consumes: `build_layout(load_contract(brief))`.
- Produces: Blender collections named `00_REFERENCE` through `90_CAMERAS`, exported meshes and a measured manifest.

- [ ] **Step 1: Add failing generated-artifact assertions**

Under `-RequireGeneratedArtifacts`, assert that BLEND/FBX/GLB/manifest exist, are non-empty, and the manifest reports room bounds, five pod objects, four dock objects, one table and one airlock.

- [ ] **Step 2: Run strict PowerShell test and verify generated artifacts are absent**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubDesign.ps1 -RequireGeneratedArtifacts
```

Expected: exit `1` for missing generated files.

- [ ] **Step 3: Implement deterministic Blender helpers**

`shiphub_blender.py` must expose:

```python
def reset_scene() -> None: ...
def ensure_collection(name: str): ...
def create_id_material(name: str, rgba: tuple[float,float,float,float]): ...
def create_box(name: str, size_m, location_m, collection, material): ...
def create_cylinder(name: str, radius_m, depth_m, location_m, collection, material, vertices=64): ...
def create_prism_from_polygon(name: str, vertices_xy_m, z_min_m, z_max_m, collection, material): ...
def apply_object_transforms(obj) -> None: ...
def measured_bounds(obj) -> dict[str, list[float]]: ...
```

Use only deterministic primitive/mesh construction; do not use Geometry Nodes, add-ons, external asset libraries or random values.

- [ ] **Step 4: Build the structural shell and reference objects**

Create the eight-vertex floor, 0.4 m structural wall extrusion, 0.4 m floor thickness, 4 m-bay ribs, ceiling shell and a character capsule reference. Keep the 28 × 24 × 7 m clear envelope visible and the nominal structure within Z = 8 m.

- [ ] **Step 5: Build the functional white-model objects**

Create:

- one 8 m × 1.1 m central table split into four named quadrant sectors;
- five pods named `SM_ShipHub_Cryopod_01` through `_05`, each 1.6 × 1.6 × 3 m and rotated 18 degrees about its base axis;
- two 2 m sealed expansion-interface blocks;
- one 4 × 3.5 m airlock opening and 1.2 m deep door frame;
- west preparation and repair bays at the contract ranges;
- east medical, role and jump-core bays at the contract ranges;
- four 1 m diameter, 0.08 m recessed docks at exact coordinates;
- one 10/16 m inner/outer-diameter ceiling ring between Z = 6.2 and 8.0 m.

- [ ] **Step 6: Configure white-model materials and collections**

Use exactly five ID materials: `MAT_Structure`, `MAT_Interactable`, `MAT_Glass`, `MAT_Door`, `MAT_NonWalkable`. Place objects into the collection contract from the spec. Apply transforms and ensure no object has non-unit scale.

- [ ] **Step 7: Save, export and emit the measured manifest**

`build_shiphub_design.py` accepts `--project-root`, `--brief` and `--output-root`. Save the BLEND, export selected production collections to FBX and GLB, and write `layout-manifest.json` containing Blender version, source brief hash, object names, collections, transforms, bounds, sheet IDs and total counts.

- [ ] **Step 8: Run `BuildWhiteModel`**

`BuildWhiteModel` independently resolves Blender and repeats the Blender 5.2 LTS version gate before it checks or runs the build script:

```powershell
$env:PROJECTRIFT_BLENDER_EXE = 'D:\Blender5.2\blender.exe'
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubDesign.ps1 -Stage BuildWhiteModel -BlenderExe $env:PROJECTRIFT_BLENDER_EXE -PythonExe 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
```

Expected: the stage first confirms Blender `5.2.x LTS`, then Blender exits `0`; BLEND, FBX, GLB and manifest are created.

- [ ] **Step 9: Run strict generated-artifact tests**

Expected: PowerShell `-RequireGeneratedArtifacts` passes.

- [ ] **Step 10: User Git checkpoint**

Suggest commit message `art: generate ship hub dimensional white model`. Do not stage or commit.

---

### Task 5: Render Orthographic, Section and Perspective Bases from the Same Scene

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/render_shiphub_drawings.py`
- Modify: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1`
- Generate: fifteen base PNGs under `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/PNG/`

**Interfaces:**
- Consumes: authoritative BLEND and the fifteen sheet IDs in the manifest.
- Produces: 4961 × 3508 px A3-landscape base PNGs with deterministic camera and section settings.

- [ ] **Step 1: Add render-output assertions**

Extend the strict PowerShell test to require exactly fifteen PNGs, unique filename prefixes matching every sheet ID, non-zero size and PNG signature bytes `89 50 4E 47 0D 0A 1A 0A`.

- [ ] **Step 2: Run the strict test and verify drawing PNGs are absent**

Expected: missing A01–A10 and D01–D05 errors.

- [ ] **Step 3: Implement render configuration**

Open the authoritative BLEND, use EEVEE with transparent world for line/ID bases and a neutral studio world for A10. Set resolution to 4961 × 3508, 300 dpi metadata, Filmic/AgX medium-high contrast, 64 samples and deterministic ambient occlusion. Do not use external textures.

- [ ] **Step 4: Create exact orthographic cameras**

Define:

```python
ORTHO_CAMERAS = {
    "A01_FloorPlan": ((0,0,30), (0,0,0), 34.0),
    "A02_ReflectedCeilingPlan": ((0,0,-12), (0,0,7), 34.0),
    "A03_NorthElevation": ((0,-32,3.5), (0,10,3.5), 32.0),
    "A04_SouthElevation": ((0,32,3.5), (0,-10,3.5), 32.0),
    "A05_WestElevation": ((32,0,3.5), (-14,0,3.5), 28.0),
    "A06_EastElevation": ((-32,0,3.5), (14,0,3.5), 28.0)
}
```

Orient the cameras through a `look_at(camera, target)` helper and use orthographic scale in meters. Verify North/South/East/West labels against the coordinate contract.

- [ ] **Step 5: Implement section views**

For A07 Longitudinal Section, cut on the YZ plane `X = 0`, view along the X axis with an orthographic camera at `(32,0,3.5)`, and show the north-south Y/Z relationship. For A08 Transverse Section, cut on the XZ plane `Y = 0`, view along the Y axis with an orthographic camera at `(0,-32,3.5)`, and show the east-west X/Z relationship. Hide the camera-side half only through per-render visibility flags. Add a red ID material only to cut-cap duplicates, not to source geometry. Restore visibility after each render.

- [ ] **Step 6: Implement module and detail views**

- A09: exploded collections offset along +Z by 1.5 m increments, rendered isometrically.
- D01: isolated cryopod orthographic front/side/top triptych.
- D02: isolated navigation table top/side and quadrant breakdown.
- D03: isolated airlock front/section and door envelope.
- D04: isolated construct dock plan/section.
- D05: isolated 1/2/4 m wall-bay interfaces front/side.

Use linked duplicates in a temporary `DRAWING_TEMP` collection and delete only that collection after render.

- [ ] **Step 7: Implement A10 consistent perspectives**

Render six camera views from the same white model: front, reverse, west oblique, east oblique, high overview and ceiling low-angle. Arrange them later in Task 6; this step writes six source PNGs under `Drawings/PNG/Perspectives/`.

- [ ] **Step 8: Run `RenderDrawings` and strict tests**

Expected: the stage independently confirms Blender `5.2.x LTS`, then Blender exits `0`; exactly fifteen base sheets plus six perspective sources exist; strict PNG checks pass.

- [ ] **Step 9: User Git checkpoint**

Suggest commit message `art: render ship hub orthographic drawing bases`. Do not stage or commit.

---

### Task 6: Publish Dimensioned SVG, PNG and the A3 PDF Book

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/publish_shiphub_drawings.py`
- Create: `Scripts/ProjectRift/Tests/test_shiphub_publish.py`
- Modify: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1`
- Generate: fifteen SVGs, fifteen final PNGs, one fifteen-page PDF and one contact sheet.

**Interfaces:**
- Consumes: brief, measured manifest and Blender base renders.
- Produces: deterministic labels/dimensions; `publish_package(brief_path, manifest_path, drawings_root)`.

- [ ] **Step 1: Write failing publisher tests**

Use a temporary directory, a synthetic 1600 × 900 PNG from Pillow and a minimal manifest. Assert that:

```python
from pypdf import PdfReader
from xml.etree import ElementTree

self.assertEqual(15, len(result.svg_paths))
self.assertEqual(15, len(result.png_paths))
self.assertEqual(15, len(PdfReader(result.pdf_path).pages))
for path in result.svg_paths:
    self.assertEqual("{http://www.w3.org/2000/svg}svg", ElementTree.parse(path).getroot().tag)
```

- [ ] **Step 2: Run tests and verify publisher import fails**

Run:

```powershell
& 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest Scripts.ProjectRift.Tests.test_shiphub_publish -v
```

Expected: import failure for `publish_shiphub_drawings`.

- [ ] **Step 3: Implement deterministic sheet metadata**

Define `SHEET_TITLES`, `SHEET_SCALES`, `DIMENSION_CHAINS` and `SECTION_MARKS` keyed by all fifteen exact sheet IDs. A01 must include 28 m, 24 m, 8 m table diameter, 5 m main path, pod centers and dock coordinates. D01 must include 1.6 × 1.6 × 3 m, 18 degrees, 75-degree door angle and 1.2 m envelope.

- [ ] **Step 4: Implement SVG output**

Write standards-compliant UTF-8 SVG with A3 landscape viewBox `0 0 420 297`, an embedded relative base-raster reference, vector title block, north arrow, scale bar, extension lines, arrowheads and millimeter-based labels. Escape all XML text through `html.escape`.

- [ ] **Step 5: Implement final PNG compositing**

Use Pillow to composite the base render into an A3 border, then draw deterministic lines and labels. Resolve fonts only from the explicit ordered list `C:\Windows\Fonts\msyh.ttc`, `C:\Windows\Fonts\arial.ttf`; if neither exists, fail publishing with an actionable message. Use Chinese titles only with `msyh.ttc`; use ASCII sheet titles with Arial. Never rely on AI-generated text.

- [ ] **Step 6: Implement the PDF book**

Use ReportLab `landscape(A3)`. Add one final PNG per page, title, scale, units, source brief hash and sheet number. Set PDF metadata title `ProjectRift Ship Hub Complete Modeling Drawings v1` and author `ProjectRift Project-Owned Art Pipeline`.

- [ ] **Step 7: Implement A10 and the contact sheet**

Arrange the six Blender perspective sources in a 3 × 2 grid with deterministic captions. The contact sheet is 4961 × 3508 PNG; A10 embeds the same grid inside the A3 title block.

- [ ] **Step 8: Run publisher unit tests**

Expected: all publisher tests pass; temporary PDF has exactly fifteen pages and all SVGs parse.

- [ ] **Step 9: Run `Publish` on real renders**

Expected: fifteen final PNGs, fifteen SVGs, one fifteen-page PDF and one contact sheet are created.

- [ ] **Step 10: User Git checkpoint**

Suggest commit message `art: publish ship hub complete drawing book`. Do not stage or commit.

---

### Task 7: Validate the Complete Package and Produce Evidence

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/validate_shiphub_blender.py`
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/validate_shiphub_package.py`
- Modify: `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubDesign.ps1`
- Modify: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1`
- Generate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Reports/validation-report.json`
- Generate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Reports/export-validation.json`
- Generate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Reports/SHA256SUMS.txt`

**Interfaces:**
- Consumes: full design package.
- Produces: exit `0` only when geometry, counts, formats and document integrity pass.

- [ ] **Step 1: Add failing final-package assertions**

Under `-RequireGeneratedArtifacts`, require `validation-report.json` with `Passed: true`, `IssueCount: 0`, `SheetCount: 15`, `PdfPageCount: 15`, `CryopodCount: 5`, `ConstructDockCount: 4`, `NavigationTableCount: 1`, and exact dimensions.

- [ ] **Step 2: Run strict tests and verify the missing report fails**

Expected: exit `1` for missing validation evidence.

- [ ] **Step 3: Implement Blender-native scene and export validation**

`validate_shiphub_blender.py` must open the authoritative BLEND, verify collection and object counts, then import the generated FBX and GLB into isolated temporary collections. Measure the imported bounds with Blender's evaluated dependency graph, compare them to the source export-selection bounds within 0.01 m, delete only the temporary validation collections, and write `export-validation.json` with `Passed`, source/import bounds, object counts and Blender version. It must never save the BLEND during validation.

- [ ] **Step 4: Implement file and document validation**

`validate_shiphub_package.py` must:

- verify required file names and non-zero lengths;
- decode every PNG through Pillow and require 4961 × 3508;
- parse every SVG through `ElementTree`;
- parse PDF through pypdf and require fifteen pages;
- load BLEND manifest counts and measured bounds;
- require `export-validation.json` to report `Passed: true` for both FBX and GLB re-import checks;
- verify brief SHA-256 equals manifest source hash;
- verify FBX/GLB are non-empty and their Blender-reimported bounds match the manifest within 0.01 m;
- reject duplicate sheet IDs, unrecognized files and unresolved marker strings;
- write SHA-256 for every final artifact in sorted relative-path order.

- [ ] **Step 5: Implement semantic geometry validation**

Require:

```python
expected = {
    "room_clear_m": [28.0, 24.0, 7.0],
    "nominal_height_m": 8.0,
    "navigation_table_diameter_m": 8.0,
    "cryopod_count": 5,
    "cryopod_recline_degrees": 18.0,
    "construct_dock_count": 4,
    "minimum_main_path_width_m": 5.0,
    "ceiling_ring_lowest_z_m": 6.2
}
```

Any mismatch produces a structured issue with `RuleId`, `Expected`, `Actual` and `Artifact`.

- [ ] **Step 6: Run `Validate`**

The PowerShell stage first invokes Blender with `validate_shiphub_blender.py`, then invokes bundled Python with `validate_shiphub_package.py`. Expected: exit `0`, both reports show `Passed: true`, zero issues and deterministic SHA file.

- [ ] **Step 7: Run all automated verification**

Run:

```powershell
& 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest Scripts.ProjectRift.Tests.test_shiphub_layout Scripts.ProjectRift.Tests.test_shiphub_publish -v
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubDesign.ps1 -RequireGeneratedArtifacts
```

Expected: all Python tests pass; PowerShell self-test exits `0`.

- [ ] **Step 8: Visually inspect the PDF and all fifteen sheets**

Render the PDF to page PNGs using the PDF skill workflow. Check clipped dimensions, illegible labels, wrong compass orientation, missing section marks, perspective inconsistency and title-block overflow. Correct the publisher or Blender camera source, rerun Publish and Validate, then re-render until clean.

- [ ] **Step 9: User Git checkpoint**

Suggest commit message `test: validate ship hub complete design package`. Do not stage or commit.

---

### Task 8: Execute the Full Pipeline and Hand Off the Modeling Authority

**Files:**
- Read: all authored and generated files from Tasks 1–7.
- Modify only when verification identifies a concrete defect.

**Interfaces:**
- Consumes: approved Blender 5.2 LTS executable and bundled Python executable.
- Produces: user-reviewable geometry authority and drawing package; no UE changes.

- [ ] **Step 1: Run final preflight**

Expected: Blender reports 5.2.x LTS; Pillow, ReportLab and pypdf import successfully.

- [ ] **Step 2: Run the complete bounded pipeline**

```powershell
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubDesign.ps1 `
  -Stage All `
  -BlenderExe $env:PROJECTRIFT_BLENDER_EXE `
  -PythonExe 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
```

Expected: `All` begins with `Preflight`, forwards the same explicit Blender/Python paths to each recursive stage, and every stage exits `0`; each Blender-consuming stage repeats the Blender `5.2.x LTS` gate, and no file is written outside the approved SourceArt output root or ProjectA-local automation root.

- [ ] **Step 3: Re-run fresh verification commands**

Run Task 7 Step 6 after the complete pipeline, not from cached results.

- [ ] **Step 4: Review geometric authority in Blender**

Open only `SM_ShipHub_Complete_White_v1.blend`. Inspect collection names, applied transforms, five pod angles, four dock coordinates, airlock opening, main-path width, ceiling-ring clearance and camera framing. Close Blender without modifying any unrelated file.

- [ ] **Step 5: Deliver the handoff**

Report:

- authored scripts and brief;
- generated BLEND/FBX/GLB;
- PDF, PNG, SVG and contact-sheet paths;
- exact test outputs and validation-report summary;
- the prerequisite or residual risks;
- confirmation that no UE asset/map and no Git state was changed;
- suggested commit message `art: deliver ship hub complete modeling drawings`.

- [ ] **Step 6: Stop at the version gate**

Wait for user acceptance. Do not begin high-poly modeling, UVs, PBR texturing, UE import or map replacement until the user explicitly starts the next version.
