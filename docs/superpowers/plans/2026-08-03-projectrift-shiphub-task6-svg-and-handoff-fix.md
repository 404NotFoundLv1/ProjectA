# ProjectRift Ship Hub Task 6 SVG and Handoff Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace generic, misleading SVG dimension grids with explicit sheet-owned annotations and add uniquely named, content-hashed PDF/PNG handoff copies that bypass stale filename caching.

**Architecture:** Move dimension semantics and coordinates into a focused `shiphub_dimensions.py` module that returns SVG markup and keyed schedules from explicit per-sheet specifications. Keep frozen authoritative outputs unchanged, add a transactional `Drawings/Handoff` pair whose revisioned names include source SHA-256 prefixes, and make the PDF byte-deterministic so identical inputs keep stable handoff names.

**Tech Stack:** Python 3.13, dataclasses, Pillow, ReportLab, pypdf, XML ElementTree, PowerShell 5.1, Microsoft Edge headless rendering, Poppler.

## Global Constraints

- Operate only in `E:\MyWork\ProjectA` and only on ProjectA-owned assets and processes.
- Remain on the existing `main` branch; do not run Git write operations. The user stages, commits, tags, pushes, and creates pull requests.
- Implement only this Task 6 repair. Do not start Task 7 or detailed asset modeling.
- Do not start or control UE, Unreal MCP, or Blender; accepted Task 4 and Task 5 assets are read-only inputs.
- Preserve the authoritative filenames `ProjectRift_ShipHub_CompleteDesign_v1.pdf` and `ProjectRift_ShipHub_ContactSheet_v1.png`.
- Create exactly two current handoff files under `Drawings/Handoff`, named `ProjectRift_ShipHub_CompleteDesign_v1r2_{pdf_sha8}.pdf` and `ProjectRift_ShipHub_ContactSheet_v1r2_{png_sha8}.png`, where each brace-delimited value is the first eight lowercase SHA-256 characters of its authoritative source.
- Handoff copies must be byte-identical to authoritative sources; identical inputs must reproduce identical hashes and names.
- Keep the PDF below 32 MiB with 15 A3 pages and one `/DCTDecode` image per page.
- Preserve exactly 15 SVG and 15 final PNG sheet names, A3 `viewBox="0 0 420 297"`, relative raster references, and accepted Task 5 sources.
- Use test-first red/green cycles for every production behavior change.
- Stop after verified Task 6 delivery and wait for user acceptance.

---

## File Structure

- Create `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_dimensions.py`: explicit annotation types, per-sheet layouts, SVG markup rendering, and schedule rendering.
- Modify `Scripts/ProjectRift/ArtPipeline/shiphub/publish_shiphub_drawings.py`: consume the annotation module, remove the generic grid, make PDF output invariant, stage content-hashed handoff copies, and include `Handoff` in the transaction.
- Modify `Scripts/ProjectRift/Tests/test_shiphub_publish.py`: artifact-level SVG geometry, deterministic handoff, byte identity, WPS PDF, and rollback tests.
- Modify `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubDesign.ps1`: exact handoff inventory, names, hashes, signatures, sizes, and unexpected-file validation.
- Modify `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1`: report the two handoff filenames from a bounded Publish run without changing other stage semantics.
- Regenerate `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/SVG/*.svg`, `FinalPNG/*.png`, the authoritative PDF/contact sheet, and the new `Drawings/Handoff` pair.
- Update `.superpowers/sdd/2026-08-02-projectrift-shiphub-complete-modeling-drawings/progress.md` and `task-6-report.md` after verification.

---

### Task 1: Explicit Annotation Model and A01 Layout

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_dimensions.py`
- Modify: `Scripts/ProjectRift/ArtPipeline/shiphub/publish_shiphub_drawings.py:49-61,179-222`
- Test: `Scripts/ProjectRift/Tests/test_shiphub_publish.py`

**Interfaces:**
- Produces: `render_sheet_annotations(sheet_id: str) -> tuple[str, str]`, where item 1 is plot/perimeter SVG markup and item 2 is keyed-note SVG markup.
- Produces: `annotation_specs(sheet_id: str) -> tuple[LinearDimension | AngularDimension | ScheduleNote, ...]` for artifact-contract tests.
- Consumes: the existing frozen sheet IDs and dimension labels; no Task 4/5 file is modified.

- [ ] **Step 1: Write the failing A01 artifact test**

Add a test that publishes the real synthetic package, parses `A01_FloorPlan.svg`, and independently asserts the approved visual semantics:

```python
def test_a01_dimensions_stay_outside_the_plan_and_use_keyed_schedules(self) -> None:
    with tempfile.TemporaryDirectory() as temporary_directory:
        brief, manifest, drawings = self._write_inputs(Path(temporary_directory))
        publish_package(brief, manifest, drawings)
        root = ElementTree.parse(drawings / "SVG" / "A01_FloorPlan.svg").getroot()
        ns = "{http://www.w3.org/2000/svg}"

        width = root.find(f".//{ns}g[@data-dimension-id='room-width']")
        depth = root.find(f".//{ns}g[@data-dimension-id='room-depth']")
        table = root.find(f".//{ns}g[@data-dimension-id='table-diameter']")
        self.assertIsNotNone(width)
        self.assertIsNotNone(depth)
        self.assertIsNotNone(table)

        width_line = width.find(f"{ns}line[@class='dimension-line']")
        depth_line = depth.find(f"{ns}line[@class='dimension-line']")
        self.assertEqual(width_line.attrib["y1"], width_line.attrib["y2"])
        self.assertGreater(float(width_line.attrib["y1"]), 233.0)
        self.assertEqual(depth_line.attrib["x1"], depth_line.attrib["x2"])
        self.assertLess(float(depth_line.attrib["x1"]), 48.0)
        self.assertEqual("diameter", table.attrib["data-orientation"])

        chains = " ".join(group.attrib.get("data-label", "") for group in root.findall(f".//{ns}g[@class='dimension-chain']"))
        self.assertNotIn("Pod centers", chains)
        self.assertNotIn("Dock coordinates", chains)
        schedule_text = "".join(root.find(f".//{ns}g[@id='keyed-notes']").itertext())
        self.assertIn("P1-P5", schedule_text)
        self.assertIn("D1-D4", schedule_text)
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
& 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest Scripts.ProjectRift.Tests.test_shiphub_publish.ShipHubPublishTests.test_a01_dimensions_stay_outside_the_plan_and_use_keyed_schedules -v
```

Expected: FAIL because the current generic grid has no `room-width`, `room-depth`, `table-diameter`, or `keyed-notes` elements.

- [ ] **Step 3: Implement the annotation types and exact A01 specification**

Create the focused module with these stable interfaces:

```python
from __future__ import annotations

from dataclasses import dataclass
from html import escape
from typing import Literal

Point = tuple[float, float]

@dataclass(frozen=True)
class LinearDimension:
    dimension_id: str
    label: str
    kind: Literal["overall", "object"]
    orientation: Literal["horizontal", "vertical", "diameter"]
    measured_start: Point
    measured_end: Point
    line_start: Point
    line_end: Point
    text_at: Point
    rotate_text: bool = False

@dataclass(frozen=True)
class AngularDimension:
    dimension_id: str
    label: str
    center: Point
    radius: float
    start_degrees: float
    end_degrees: float
    text_at: Point

@dataclass(frozen=True)
class ScheduleNote:
    note_id: str
    key: str
    label: str
    anchor: Point | None
    text_at: Point

A01_SPECS = (
    LinearDimension("room-width", "28 m OVERALL WIDTH", "overall", "horizontal", (66, 218), (354, 218), (66, 238), (354, 238), (174, 234)),
    LinearDimension("room-depth", "24 m OVERALL DEPTH", "overall", "vertical", (62, 48), (62, 215), (43, 48), (43, 215), (36, 160), True),
    LinearDimension("table-diameter", "Ø 8 m NAVIGATION TABLE", "object", "diameter", (174, 139), (246, 139), (174, 139), (246, 139), (181, 134)),
    ScheduleNote("main-path", "C", "MAIN PATH CLEARANCE: 5 m", (96, 171), (267, 239)),
    ScheduleNote("pod-centers", "P1-P5", "X = -4, -2, 0, 2, 4 m", (210, 62), (267, 246)),
    ScheduleNote("dock-coordinates", "D1-D4", "(-5.3,-5.3), (5.3,-5.3), (-5.3,5.3), (5.3,5.3) m", (335, 197), (267, 253)),
)
```

Render every group with `data-dimension-id`, `data-kind`, and `data-orientation`. Use extension lines only when measured endpoints differ from line endpoints. Render schedule notes under `<g id="keyed-notes">`; keys appear at anchors and full values stay in the keyed-note band.

- [ ] **Step 4: Replace A01 generic markup in the publisher**

Import `render_sheet_annotations`. In `_svg`, replace `_dimension_graphics(sheet_id)` and the duplicated title-block dimension list with:

```python
annotation_markup, schedule_markup = render_sheet_annotations(sheet_id)
```

Place `annotation_markup` immediately after the raster/perspective images and `schedule_markup` before the title-block rectangle. Remove `_dimension_graphics`; keep the title block limited to sheet title, scale, units, section mark, and brief hash.

- [ ] **Step 5: Run the focused test and verify GREEN**

Run the Step 2 command. Expected: PASS.

- [ ] **Step 6: Run all publisher tests**

Run:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
& 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest Scripts.ProjectRift.Tests.test_shiphub_publish -v
```

Expected: all publisher tests pass. Do not perform a Git write operation.

---

### Task 2: Explicit Layouts for A02-A10 and D01-D05

**Files:**
- Modify: `Scripts/ProjectRift/ArtPipeline/shiphub/shiphub_dimensions.py`
- Modify: `Scripts/ProjectRift/Tests/test_shiphub_publish.py`

**Interfaces:**
- Consumes: `LinearDimension`, `AngularDimension`, `ScheduleNote`, `render_sheet_annotations`, and `annotation_specs` from Task 1.
- Produces: complete explicit `SHEET_ANNOTATIONS` coverage for all 15 frozen sheet IDs.

- [ ] **Step 1: Write failing parameterized semantic tests**

Add literal expectations that catch the current misleading grid behavior:

```python
def test_all_sheets_use_semantically_oriented_annotations(self) -> None:
    expected = {
        "A02_ReflectedCeilingPlan": (("ring-outer", "diameter"), ("ring-inner", "diameter")),
        "A03_NorthElevation": (("overall-width", "horizontal"), ("clear-height", "vertical")),
        "A04_SouthElevation": (("overall-width", "horizontal"), ("clear-height", "vertical")),
        "A05_WestElevation": (("overall-depth", "horizontal"), ("clear-height", "vertical")),
        "A06_EastElevation": (("overall-depth", "horizontal"), ("clear-height", "vertical")),
        "A07_LongitudinalSection": (("section-length", "horizontal"), ("clear-height", "vertical")),
        "A08_TransverseSection": (("section-length", "horizontal"), ("clear-height", "vertical")),
        "A09_ExplodedModulePlan": (("ring-lift", "vertical"), ("structural-bay", "horizontal")),
        "A10_PerspectiveSheet": (),
        "D02_NavigationTable": (("table-diameter", "diameter"), ("display-diameter", "diameter"), ("table-height", "vertical")),
        "D03_MainAirlock": (("opening-width", "horizontal"), ("opening-height", "vertical"), ("airlock-depth", "horizontal")),
        "D04_ConstructDock": (("dock-diameter", "diameter"), ("recess-depth", "vertical")),
        "D05_WallBayInterface": (("bay-width-chain", "horizontal"), ("wall-thickness", "horizontal")),
    }
    for sheet_id, wanted in expected.items():
        actual = tuple(
            (spec.dimension_id, spec.orientation)
            for spec in annotation_specs(sheet_id)
            if isinstance(spec, LinearDimension)
        )
        self.assertEqual(wanted, actual, sheet_id)

def test_d01_angles_are_arcs_and_a10_has_no_dimension_chains(self) -> None:
    d01 = annotation_specs("D01_Cryopod")
    self.assertEqual({"recline-angle", "door-angle"}, {item.dimension_id for item in d01 if isinstance(item, AngularDimension)})
    self.assertFalse(annotation_specs("A10_PerspectiveSheet"))
```

- [ ] **Step 2: Run the two focused tests and verify RED**

Run both test methods with `python -m unittest ... -v`. Expected: FAIL because only A01 exists in the explicit map.

- [ ] **Step 3: Add exact sheet specifications**

Populate `SHEET_ANNOTATIONS` using these fixed zones and positions:

| Sheets | Horizontal/perimeter line | Vertical/perimeter line | Object/schedule placement |
|---|---|---|---|
| A02 | outer Ø: `(150,135)-(270,135)`; inner Ø: `(172,145)-(248,145)` | none | labels at `(174,130)` and `(182,158)` |
| A03-A04 | `(68,238)-(352,238)` | `(43,58)-(43,218)` | width/depth label below; height text rotated left |
| A05-A06 | `(76,238)-(344,238)` | `(43,58)-(43,218)` | depth label below; height text rotated left |
| A07-A08 | `(72,238)-(348,238)` | `(43,58)-(43,218)` | section length below; height text rotated left |
| A09 | bay `(82,228)-(146,228)` | lift `(365,68)-(365,210)` | lift text rotated right; bay text below |
| A10 | none | none | perspective index only |
| D01 | pod width `(74,222)-(132,222)` and length `(158,222)-(262,222)` | pod height `(54,82)-(54,202)` | 18° arc centered `(212,164)`, 75° arc centered `(326,171)`, envelope keyed `E` |
| D02 | table Ø `(54,136)-(154,136)`, display Ø `(280,136)-(362,136)` | height `(204,114)-(204,190)` | no duplicate schedule values |
| D03 | opening width `(54,218)-(142,218)`, depth `(185,218)-(244,218)` | opening height `(42,94)-(42,196)` | muster area keyed `M` in schedule |
| D04 | dock Ø `(148,146)-(204,146)` | recess `(270,142)-(270,183)` | count/location keyed `D1-D4` |
| D05 | chained widths `(68,224)-(330,224)` | none | ticks split 1 m / 2 m / 4 m; wall thickness `(338,145)-(382,145)` |

Use the frozen labels from the design spec. A03-A08 contain exactly two perimeter chains. Direct/detail counts remain at or below four; schedules carry counts, coordinates, operational envelopes, and muster-area data.

- [ ] **Step 4: Run focused tests and verify GREEN**

Run the Step 2 methods. Expected: PASS.

- [ ] **Step 5: Add artifact-level duplicate and zone tests**

Publish a synthetic package and parse every SVG. Assert:

```python
self.assertEqual(0, len(a10.findall(f".//{ns}g[@class='dimension-chain']")))
self.assertLessEqual(len(sheet.findall(f".//{ns}g[@data-kind='overall']")), 2)
self.assertEqual(1, rendered_text.count("28 m OVERALL WIDTH"))
```

For each sheet, require the title-block group to contain no `dimension` class text. Mutating the renderer back to the generic grid or duplicating title values must fail.

- [ ] **Step 6: Run the complete publisher suite**

Run `python -m unittest Scripts.ProjectRift.Tests.test_shiphub_publish -v`. Expected: all tests pass. Do not perform a Git write operation.

---

### Task 3: Deterministic Content-Hashed Handoff Pair

**Files:**
- Modify: `Scripts/ProjectRift/ArtPipeline/shiphub/publish_shiphub_drawings.py`
- Modify: `Scripts/ProjectRift/Tests/test_shiphub_publish.py`

**Interfaces:**
- Produces: `sha256_hex(path: Path) -> str`.
- Produces: `_stage_handoff(stage: Path, pdf_path: Path, contact_path: Path) -> tuple[Path, Path]`.
- Extends: `PublishResult` with `handoff_pdf_path: Path` and `handoff_contact_sheet_path: Path`.

- [ ] **Step 1: Write the failing deterministic handoff test**

```python
def test_publish_creates_stable_content_hashed_handoff_copies(self) -> None:
    with tempfile.TemporaryDirectory() as temporary_directory:
        brief, manifest, drawings = self._write_inputs(Path(temporary_directory))
        first = publish_package(brief, manifest, drawings)
        first_pdf_bytes = first.pdf_path.read_bytes()
        first_png_bytes = first.contact_sheet_path.read_bytes()
        first_names = (first.handoff_pdf_path.name, first.handoff_contact_sheet_path.name)

        second = publish_package(brief, manifest, drawings)
        self.assertEqual(first_pdf_bytes, second.pdf_path.read_bytes())
        self.assertEqual(first_png_bytes, second.contact_sheet_path.read_bytes())
        self.assertEqual(first_names, (second.handoff_pdf_path.name, second.handoff_contact_sheet_path.name))

        pdf_sha8 = hashlib.sha256(first_pdf_bytes).hexdigest()[:8]
        png_sha8 = hashlib.sha256(first_png_bytes).hexdigest()[:8]
        self.assertEqual(f"ProjectRift_ShipHub_CompleteDesign_v1r2_{pdf_sha8}.pdf", first_names[0])
        self.assertEqual(f"ProjectRift_ShipHub_ContactSheet_v1r2_{png_sha8}.png", first_names[1])
        self.assertEqual(first_pdf_bytes, first.handoff_pdf_path.read_bytes())
        self.assertEqual(first_png_bytes, first.handoff_contact_sheet_path.read_bytes())
        self.assertEqual(2, len(list((drawings / "Handoff").iterdir())))
```

- [ ] **Step 2: Run the focused test and verify RED**

Expected: ERROR/FAIL because `PublishResult` has no handoff fields and no `Handoff` directory exists.

- [ ] **Step 3: Make PDF bytes invariant**

Construct ReportLab with invariant mode while preserving the confirmed WPS encoding:

```python
document = canvas.Canvas(
    str(output_path),
    pagesize=landscape(A3),
    pageCompression=1,
    invariant=1,
)
```

Keep `rl_config.useA85 = 0`, full-resolution quality-90 4:4:4 non-progressive JPEG embedding, metadata, 15 pages, and the 32 MiB hard limit.

- [ ] **Step 4: Stage the unique pair**

Implement:

```python
def sha256_hex(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

def _stage_handoff(stage: Path, pdf_path: Path, contact_path: Path) -> tuple[Path, Path]:
    handoff = stage / "Handoff"
    handoff.mkdir()
    pdf_copy = handoff / f"ProjectRift_ShipHub_CompleteDesign_v1r2_{sha256_hex(pdf_path)[:8]}.pdf"
    png_copy = handoff / f"ProjectRift_ShipHub_ContactSheet_v1r2_{sha256_hex(contact_path)[:8]}.png"
    shutil.copyfile(pdf_path, pdf_copy)
    shutil.copyfile(contact_path, png_copy)
    return pdf_copy, png_copy
```

Call it after `_pdf_book` and before `_commit`. Extend the result with committed `drawings_root / "Handoff" / name` paths.

- [ ] **Step 5: Run the focused test and verify GREEN**

Expected: PASS with stable bytes, stable names, exact hashes, and exactly two handoff files.

- [ ] **Step 6: Re-run the WPS PDF contract test**

Run the existing compact-PDF test. Require 15 pages, strict parse, one `/DCTDecode` image per page, and size below 32 MiB. Expected: PASS.

---

### Task 4: Transaction and Repository-Strict Handoff Contract

**Files:**
- Modify: `Scripts/ProjectRift/ArtPipeline/shiphub/publish_shiphub_drawings.py`
- Modify: `Scripts/ProjectRift/Tests/test_shiphub_publish.py`
- Modify: `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubDesign.ps1`
- Modify: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubDesign.ps1`

**Interfaces:**
- Consumes: staged `Handoff` directory and extended `PublishResult` from Task 3.
- Produces: one atomic six-target publication transaction and strict validation of the active handoff pair.

- [ ] **Step 1: Extend the rollback test and verify RED**

Add pre-existing `Handoff/old.pdf` and `Handoff/old.png` fixtures to `test_commit_failure_restores_every_preexisting_output`. Stage a new `Handoff` but omit a later required target so replacement begins and fails. Assert both old handoff files are restored and no backup directory remains.

Expected RED: the current `_commit` target tuple ignores `Handoff`, so the fixture does not participate in rollback.

- [ ] **Step 2: Add `Handoff` to the transaction and verify GREEN**

Change the target tuple to:

```python
targets = (
    "SVG",
    "FinalPNG",
    "ProjectRift_ShipHub_CompleteDesign_v1.pdf",
    "ProjectRift_ShipHub_ContactSheet_v1.png",
    "Handoff",
)
```

Run the rollback test. Expected: PASS.

- [ ] **Step 3: Add strict generated-artifact checks**

In the PowerShell validator:

1. Require `Drawings\Handoff`.
2. Compute authoritative SHA-256 values with `Get-FileHash`.
3. Require exact lowercase filenames containing the first eight hash characters.
4. Require exactly two files and reject any subdirectory or extra file.
5. Compare authoritative and handoff hashes for byte identity.
6. Apply `%PDF`, `<32MB`, PNG signature, and `4961 x 3508` checks to handoff copies as well as authoritative sources.

Use literal expectations derived from authoritative file hashes; do not grep publisher source.

- [ ] **Step 4: Verify strict RED before regeneration**

Run:

```powershell
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File 'Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubDesign.ps1' -RequireGeneratedArtifacts
```

Expected: FAIL because the current official output tree has no `Handoff` directory.

- [ ] **Step 5: Report handoff filenames from Publish**

Extend the Python publisher success line and the PowerShell runner's Publish-stage result so a bounded run prints both committed handoff filenames. Preserve all existing stage names, containment checks, and the rule that `All` stops at absent Task 7 Validate.

- [ ] **Step 6: Run all Python tests**

Run:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
& 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest Scripts.ProjectRift.Tests.test_shiphub_layout Scripts.ProjectRift.Tests.test_shiphub_publish -v
```

Expected: every test passes. Do not perform a Git write operation.

---

### Task 5: Bounded Publication, Visual QA, and User Handoff

**Files:**
- Regenerate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/SVG/*.svg`
- Regenerate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/FinalPNG/*.png`
- Regenerate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/ProjectRift_ShipHub_CompleteDesign_v1.pdf`
- Regenerate: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/ProjectRift_ShipHub_ContactSheet_v1.png`
- Create: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/Handoff/ProjectRift_ShipHub_CompleteDesign_v1r2_{pdf_sha8}.pdf`
- Create: `SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/Handoff/ProjectRift_ShipHub_ContactSheet_v1r2_{png_sha8}.png`
- Modify: `.superpowers/sdd/2026-08-02-projectrift-shiphub-complete-modeling-drawings/progress.md`
- Modify: `.superpowers/sdd/2026-08-02-projectrift-shiphub-complete-modeling-drawings/task-6-report.md`

**Interfaces:**
- Consumes: the complete tested publisher and validator from Tasks 1-4.
- Produces: the user-testable Task 6 r2 handoff pair and final acceptance evidence.

- [ ] **Step 1: Record pre-publication Task 4 hashes**

Run SHA-256 on the accepted BLEND, FBX, GLB, and layout manifest. Require the frozen values:

```text
BLEND  BD29AE2833C5E728CA94AECFE8542D3F61D50BC122F3C68332880727B4D1DBCD
FBX    DF8BC36E55CA8DDB1BA062BAC5F23CC453AED5346F17A23038F1F9EE5B4883C3
GLB    AB48B11D98678871B3DE37BD278B4A0AC5B88A9C6D039F393F0E5E92F4803862
LAYOUT 3DF4F054F7DDF8F2C5523ECD673C4B3A2E4F6E7B4DB7C27D0A23EE0C412A30D8
```

- [ ] **Step 2: Run bounded Publish**

```powershell
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File 'Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubDesign.ps1' -Stage Publish -PythonExe 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
```

Expected: exit `0`, 15 SVGs, 15 final PNGs, the authoritative PDF/contact sheet, and two printed `v1r2_<sha8>` handoff names.

- [ ] **Step 3: Run repository strict validation**

Run the Task 4 Step 4 command. Expected: `ProjectRift ship-hub design self-test: PASS`.

- [ ] **Step 4: Render all SVGs with ProjectA-owned Edge invocations**

Create `tmp\shiphub-task6-r2-svg-qa`. Render the 15 exact SVG paths with this bounded loop and require every Edge invocation to exit `0`:

```powershell
$edge = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
$svgRoot = 'E:\MyWork\ProjectA\SourceArt\ProjectRift\ShipHub\CompleteDesign\Drawings\SVG'
$qaRoot = 'E:\MyWork\ProjectA\tmp\shiphub-task6-r2-svg-qa'
New-Item -ItemType Directory -Path $qaRoot | Out-Null
Get-ChildItem -LiteralPath $svgRoot -Filter '*.svg' -File | Sort-Object Name | ForEach-Object {
    $output = Join-Path $qaRoot ($_.BaseName + '.png')
    $uri = 'file:///' + ($_.FullName -replace '\\', '/')
    & $edge --headless --disable-gpu --hide-scrollbars --window-size=1680,1188 "--screenshot=$output" $uri
    if ($LASTEXITCODE -ne 0) { throw "Edge failed to render $($_.FullName)" }
}
```

Build one 3x5 contact sheet from the 15 screenshots with bundled Pillow. Inspect the contact sheet, then inspect A01, A02, D01, and D03 at original screenshot resolution. Reject any crossing overall line, incorrect orientation, overlap, clipping, duplicate value, or unreadable schedule.

- [ ] **Step 5: Render and inspect the final PDF**

Use bundled Poppler:

```powershell
& 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\poppler\Library\bin\pdftoppm.exe' -png -r 72 '<authoritative PDF>' 'tmp\pdfs\task6-r2\page'
& 'C:\Users\10144\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\poppler\Library\bin\pdftoppm.exe' -f 1 -singlefile -png -r 300 '<authoritative PDF>' 'tmp\pdfs\task6-r2\a01-300dpi'
```

Inspect all 15 pages as a contact sheet and A01 at 300 DPI. Require no missing page, clipping, overlap, missing-glyph box, title overflow, or visible compression defect.

- [ ] **Step 6: Verify deterministic republish**

Record authoritative and handoff hashes/names, run bounded Publish a second time, and require all four hashes and both handoff names to remain identical. Re-run repository strict validation. Expected: PASS.

- [ ] **Step 7: Clean exact QA and cache residue**

Resolve and verify the two exact QA directories are inside `E:\MyWork\ProjectA\tmp`, delete only files created by Steps 4-5, remove the now-empty exact directories, and remove test-generated `.pyc` files plus their empty `__pycache__` directories. Require zero `.shiphub-*`, `.tmp`, `.bak`, `.pyc`, or QA residue under the scoped paths.

- [ ] **Step 8: Final read-only verification and documentation**

Run `git -c safe.directory=E:/MyWork/ProjectA status --short` and `git -c safe.directory=E:/MyWork/ProjectA diff --check`. Confirm only Task 6 expected paths are present, Task 4 hashes remain frozen, and no Task 7 file exists. Append the red/green evidence, final hashes, exact handoff names, visual QA results, deletion disclosure, boundaries, and suggested commit message to the Task 6 progress/report files.

- [ ] **Step 9: User acceptance checkpoint**

Deliver the uniquely named handoff PDF with one PDF output citation and the uniquely named handoff PNG with one concrete file link. Ask the user to open both in WPS/Photos and visually review A01/A02/D01/D03 SVG previews. Stop and wait for explicit Task 6 acceptance; do not begin Task 7.

---

## Spec Coverage Matrix

- Authoritative filenames, byte-identical handoff copies, deterministic invariant PDF bytes, and transactional publication: Tasks 3-4.
- A01 floor plan perimeter/object/schedule layout: Task 1.
- A02-A09 architectural annotation semantics: Task 2.
- A10 perspective sheet with no arrowed dimensions: Task 2.
- D01-D05 detail annotation semantics: Task 2.
- PDF below 32 MiB with single-DCT pages: Tasks 3 and 5.
- Repository strict validation and exact output inventory: Task 4.
- Visual acceptance for all SVGs, all PDF pages, and full-resolution representative sheets: Task 5.
- Project boundaries, Git prohibition, and the Task 7 version gate: Global Constraints and Task 5.
