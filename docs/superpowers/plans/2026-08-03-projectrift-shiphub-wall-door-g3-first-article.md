# ProjectRift ShipHub Wall-Door G3 First-Article Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce `SM_ShipHub_WallDoor_400_A` as ProjectRift's first complete, production-ready modular environment asset, including appearance lock, Blender production source, UV/PBR, state overlays, FBX, UE 5.8 assets, an independent validation map, and acceptance evidence.

**Architecture:** Drive the asset from one versioned JSON contract and one authoritative Blender 5.2 LTS file. Generate and validate DCC outputs under a dedicated `SourceArt` root, then import the same approved artifacts into a new `/Game/ProjectRift/ShipHub` UE subtree; three visual states share the base mesh and collision. Stop at the appearance, DCC, and UE user gates instead of advancing automatically.

**Tech Stack:** Blender 5.2 LTS at `D:\Blender5.2\blender.exe`, Blender Python, Python 3/unittest, Krita-compatible PNGs, FBX Static Mesh Pipeline, Unreal Engine 5.8, ProjectA Unreal MCP at `http://127.0.0.1:8001/mcp`, ProjectRift `PRProductionValidation`.

## Global Constraints

- Operate only in `E:\MyWork\ProjectA` and ProjectA-owned processes; never inspect or control another project.
- Unreal MCP is ProjectA-only at `http://127.0.0.1:8001/mcp`; port 8000 must not be probed.
- Work directly on existing `main`; do not create a branch or worktree.
- The user performs all Git staging, commits, tags, pushes, and pull requests; each task reports a suggested commit message only.
- Preserve `Content/ProjectRift/Maps/L_ShipLobby.umap`; it is a gameplay DEMO and is not an art input.
- Blender version must match `Blender 5.2.x LTS`; do not silently substitute another version.
- Frozen module dimensions are 400 × 30 × 400 cm with a 240 × 280 cm clear opening, 50 cm snapping, bottom-left-back pivot, and UE scale `1,1,1`.
- The base structure has at most two material slots; textures are at most 4096, with 2048 as the first-article default and about 512 px/m effective density.
- Damage, patch, and online states share the base mesh, module bounds, pivot, door opening, and collision.
- No opening door leaf, door animation, interaction logic, multiplayer logic, kit batch production, production lobby replacement, or Interchange automation.
- Purple emissive is prohibited on this asset; amber is patch/warning only and cyan is online only.
- The free Blender + Krita + UE route remains authoritative unless a separate comparison proves a paid tool saves at least 30% at equal quality.
- At each user Gate, stop and wait for explicit acceptance before beginning the next task.

---

## File Structure

### Pipeline code and tests

- `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubWallDoor.ps1` — stage runner and ProjectA path/version guard.
- `Scripts/ProjectRift/ArtPipeline/shiphub/wall_door_contract.py` — typed contract loading and invariant validation.
- `Scripts/ProjectRift/ArtPipeline/shiphub/build_wall_door_first_article.py` — Blender scene, production geometry, UV, materials, state overlays, collision, renders, bake, and export.
- `Scripts/ProjectRift/ArtPipeline/shiphub/validate_wall_door_blender.py` — independent `.blend` and FBX reimport inspection.
- `Scripts/ProjectRift/ArtPipeline/shiphub/validate_wall_door_package.py` — file, image, hash, manifest, and approval-gate validation.
- `Scripts/ProjectRift/Tests/test_shiphub_wall_door_contract.py` — pure Python contract tests.
- `Scripts/ProjectRift/Tests/test_shiphub_wall_door_package.py` — mutation-based package validator tests.
- `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubWallDoor.ps1` — Windows integration and generated-artifact assertions.

### Source art

- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.asset.json` — sole machine-readable asset contract.
- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.approval.json` — appearance, DCC, and UE gate ledger.
- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.generation-ledger.json` — image-generation provenance and exact prompts.
- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Concept/` — deterministic orthographic references, candidate paintovers, approved appearance sheet.
- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Blender/SM_ShipHub_WallDoor_400_A.blend` — authoritative production file.
- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Textures/` — 2048 Base Color, Normal, ORM, state mask, and editable source images.
- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Exports/` — base FBX and state-overlay FBXs.
- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/` — Blender, export, package, UE, performance, and SHA-256 evidence.
- `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/License/ProjectOwnedDeclaration.md` — first-article source/license declaration.

### Unreal assets

- `Content/ProjectRift/ShipHub/Meshes/Structure/SM_ShipHub_WallDoor_400_A.uasset`.
- `Content/ProjectRift/ShipHub/Meshes/Structure/SM_ShipHub_WallDoor_400_A_Overlay_Damaged.uasset`.
- `Content/ProjectRift/ShipHub/Meshes/Structure/SM_ShipHub_WallDoor_400_A_Overlay_Patched.uasset`.
- `Content/ProjectRift/ShipHub/Textures/T_ShipHub_WallDoor_400_A_BC.uasset`.
- `Content/ProjectRift/ShipHub/Textures/T_ShipHub_WallDoor_400_A_N.uasset`.
- `Content/ProjectRift/ShipHub/Textures/T_ShipHub_WallDoor_400_A_ORM.uasset`.
- `Content/ProjectRift/ShipHub/Textures/T_ShipHub_WallDoor_400_A_StateMask.uasset`.
- `Content/ProjectRift/ShipHub/Materials/M_ShipHub_Surface.uasset`.
- `Content/ProjectRift/ShipHub/Materials/M_ShipHub_Functional.uasset`.
- `Content/ProjectRift/ShipHub/Materials/MI_ShipHub_WallDoor_400_A_Damaged.uasset`.
- `Content/ProjectRift/ShipHub/Materials/MI_ShipHub_WallDoor_400_A_Patched.uasset`.
- `Content/ProjectRift/ShipHub/Materials/MI_ShipHub_WallDoor_400_A_Online.uasset`.
- `Content/ProjectRift/ShipHub/Blueprints/BP_ShipHub_WallDoor_400_A_StatePreview.uasset`.
- `Content/ProjectRift/ShipHub/Maps/L_ShipHub_ArtValidation.umap`.
- `Config/ProjectRift/AssetLicenseRegistry.json` — exact ProjectOwned record for the ShipHub subtree.

---

### Task 9: Establish the G3 Contract and Test Harness

**Files:**
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.asset.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.approval.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.generation-ledger.json`
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/wall_door_contract.py`
- Create: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubWallDoor.ps1`
- Create: `Scripts/ProjectRift/Tests/test_shiphub_wall_door_contract.py`
- Create: `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubWallDoor.ps1`

**Interfaces:**
- Consumes: approved G3 design spec and Task 8 design package.
- Produces: `WallDoorContract load_contract(Path)` and stage runner values `Preflight`, `ValidateContract`, `BuildAppearance`, `BuildProduction`, `BakeTextures`, `Export`, `ValidatePackage`, `AllDCC`.

- [ ] **Step 1: Write failing contract tests**

Create tests that load a temporary JSON fixture and assert these exact values:

```python
self.assertEqual(contract.asset_id, "SM_ShipHub_WallDoor_400_A")
self.assertEqual(contract.bounds_cm, (400.0, 30.0, 400.0))
self.assertEqual(contract.opening_cm, (240.0, 280.0))
self.assertEqual(contract.snap_cm, 50.0)
self.assertEqual(contract.pivot, "BottomLeftBack")
self.assertEqual(contract.material_slot_limit, 2)
self.assertEqual(contract.texture_size, 2048)
self.assertEqual(contract.states, ("Damaged", "Patched", "Online"))
```

Add mutation tests for width `399`, opening width `239`, snap `25`, pivot `Center`, texture size `8192`, a third material slot, and state order changes. Each must raise `ValueError` containing the failing dotted JSON path.

- [ ] **Step 2: Run the tests and verify the expected failure**

Run:

```powershell
python -m unittest Scripts.ProjectRift.Tests.test_shiphub_wall_door_contract -v
```

Expected: FAIL because `wall_door_contract` and the contract JSON do not yet exist.

- [ ] **Step 3: Create the exact contract JSON**

Use this schema and values:

```json
{
  "Schema": "projectrift.shiphub.wall-door-first-article.v1",
  "AssetId": "SM_ShipHub_WallDoor_400_A",
  "Stage": "G3",
  "Units": "cm",
  "BoundsCm": [400, 30, 400],
  "DoorOpeningCm": [240, 280],
  "DoorOpeningMinCm": [80, 0, 0],
  "SnapCm": 50,
  "Pivot": "BottomLeftBack",
  "MaterialSlotLimit": 2,
  "TextureSize": 2048,
  "TextureChannels": ["BC", "N", "ORM", "StateMask"],
  "States": ["Damaged", "Patched", "Online"],
  "BaseMesh": "SM_ShipHub_WallDoor_400_A",
  "CollisionPieces": ["LeftJamb", "RightJamb", "Lintel"],
  "References": [
    "SourceArt/ProjectRift/ShipHub/CompleteDesign/Blender/SM_ShipHub_Complete_White_v1.blend",
    "SourceArt/ProjectRift/ShipHub/CompleteDesign/Drawings/FinalPNG/D05_WallBayInterface.png"
  ]
}
```

The opening minimum plus width yields X `80..320 cm`; Z is `0..280 cm`. The base bounds are X `0..400`, Y `0..30`, Z `0..400`.

- [ ] **Step 4: Implement the typed loader and invariant validator**

Define:

```python
@dataclass(frozen=True)
class WallDoorContract:
    asset_id: str
    bounds_cm: tuple[float, float, float]
    opening_cm: tuple[float, float]
    opening_min_cm: tuple[float, float, float]
    snap_cm: float
    pivot: str
    material_slot_limit: int
    texture_size: int
    texture_channels: tuple[str, ...]
    states: tuple[str, ...]
    base_mesh: str
    collision_pieces: tuple[str, ...]
    references: tuple[str, ...]

def load_contract(path: Path) -> WallDoorContract:
    data = json.loads(path.read_text(encoding="utf-8"))
    contract = WallDoorContract(
        asset_id=str(data["AssetId"]),
        bounds_cm=tuple(float(v) for v in data["BoundsCm"]),
        opening_cm=tuple(float(v) for v in data["DoorOpeningCm"]),
        opening_min_cm=tuple(float(v) for v in data["DoorOpeningMinCm"]),
        snap_cm=float(data["SnapCm"]),
        pivot=str(data["Pivot"]),
        material_slot_limit=int(data["MaterialSlotLimit"]),
        texture_size=int(data["TextureSize"]),
        texture_channels=tuple(str(v) for v in data["TextureChannels"]),
        states=tuple(str(v) for v in data["States"]),
        base_mesh=str(data["BaseMesh"]),
        collision_pieces=tuple(str(v) for v in data["CollisionPieces"]),
        references=tuple(str(v) for v in data["References"]),
    )
    issues = validate_contract(contract)
    if issues:
        raise ValueError("; ".join(issues))
    return contract

def validate_contract(contract: WallDoorContract) -> list[str]:
    expected = {
        "AssetId": (contract.asset_id, "SM_ShipHub_WallDoor_400_A"),
        "BoundsCm": (contract.bounds_cm, (400.0, 30.0, 400.0)),
        "DoorOpeningCm": (contract.opening_cm, (240.0, 280.0)),
        "DoorOpeningMinCm": (contract.opening_min_cm, (80.0, 0.0, 0.0)),
        "SnapCm": (contract.snap_cm, 50.0),
        "Pivot": (contract.pivot, "BottomLeftBack"),
        "MaterialSlotLimit": (contract.material_slot_limit, 2),
        "TextureSize": (contract.texture_size, 2048),
        "TextureChannels": (contract.texture_channels, ("BC", "N", "ORM", "StateMask")),
        "States": (contract.states, ("Damaged", "Patched", "Online")),
        "CollisionPieces": (contract.collision_pieces, ("LeftJamb", "RightJamb", "Lintel")),
    }
    return [f"{path} must be exactly {wanted!r}" for path, (actual, wanted) in expected.items() if actual != wanted]
```

Validation must compare every frozen value exactly, require two existing ProjectA-relative references, reject absolute/out-of-root paths, and emit dotted-path messages such as `BoundsCm must be exactly [400, 30, 400]`.

- [ ] **Step 5: Create pending gate and provenance ledgers**

`approval.json` starts with:

```json
{
  "Schema": "projectrift.shiphub.wall-door-approval.v1",
  "AssetId": "SM_ShipHub_WallDoor_400_A",
  "Appearance": {"Status": "Pending", "Evidence": []},
  "DCC": {"Status": "Pending", "Evidence": []},
  "UE": {"Status": "Pending", "Evidence": []}
}
```

`generation-ledger.json` starts with schema, asset ID, and an empty `Entries` array. It must never contain secrets or API keys.

- [ ] **Step 6: Implement the ProjectA-contained runner**

The runner must resolve the project root from its own location, reuse `ProjectRift.ArtPipeline.psm1`, validate every output through `Test-ProjectRiftContainedArtPath`, and reject Blender output unless the first version line matches `^Blender 5\.2\.\d+ LTS`.

`AllDCC` runs only through `ValidatePackage`; it must never launch UE or modify `Content`.

- [ ] **Step 7: Run passing contract and PowerShell tests**

Run:

```powershell
python -m unittest Scripts.ProjectRift.Tests.test_shiphub_wall_door_contract -v
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubWallDoor.ps1
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubWallDoor.ps1 -Stage ValidateContract -BlenderExe 'D:\Blender5.2\blender.exe'
```

Expected: all tests PASS and the contract validator prints the exact asset ID and dimensions.

- [ ] **Step 8: User Git checkpoint**

Report changed files and test evidence. Suggest commit message `art: add ship hub wall-door G3 contract`. Do not stage or commit.

---

### Task 10: Produce and Approve the G3.0 Appearance Lock

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/build_wall_door_first_article.py`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Blender/SM_ShipHub_WallDoor_400_A_Appearance.blend`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Concept/Orthographic/*.png`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Concept/Candidates/*.png`
- Create after approval: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Concept/SM_ShipHub_WallDoor_400_A_AppearanceLock.png`
- Modify: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.generation-ledger.json`
- Modify after explicit approval: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.approval.json`

**Interfaces:**
- Consumes: validated `WallDoorContract`, Task 8 D05 reference, and approved realistic ShipHub visual language.
- Produces: geometry-bound front/back/left/right orthographic renders, one neutral perspective, three state studies, and `Appearance.Status == Approved`.

- [ ] **Step 1: Extend the integration test with appearance assertions**

When `-RequireAppearance` is passed, assert that front, back, left, right, perspective, Damaged, Patched, Online, and the final appearance-lock sheet are valid non-empty PNG files. Assert the appearance `.blend` report contains:

```powershell
Assert-Equal 400 $report.BoundsCm.Width 'Appearance width.'
Assert-Equal 30 $report.BoundsCm.Depth 'Appearance depth.'
Assert-Equal 400 $report.BoundsCm.Height 'Appearance height.'
Assert-Equal 240 $report.DoorOpeningCm.Width 'Appearance opening width.'
Assert-Equal 280 $report.DoorOpeningCm.Height 'Appearance opening height.'
Assert-True $report.PivotAtBottomLeftBack 'Appearance pivot.'
```

- [ ] **Step 2: Run the test and verify it fails for missing appearance outputs**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubWallDoor.ps1 -RequireAppearance
```

Expected: FAIL listing the missing appearance `.blend`, PNGs, and report.

- [ ] **Step 3: Build the deterministic appearance scene**

In Blender, create the 4 × 0.3 × 4 m envelope from three open-door structural regions: left jamb X `0..0.8`, right jamb X `3.2..4.0`, and lintel X `0.8..3.2`, Z `2.8..4.0`. Add only silhouette/mid-frequency language:

- 5 cm controlled outer chamfers;
- 4 cm recessed reveal around the opening;
- two 8 cm vertical service channels;
- one 12 cm lintel maintenance recess;
- asymmetric removable cover allocation without changing bounds;
- no bolts, decals, cables, dirt, door leaf, or geometry outside the envelope.

Create collections `00_REFERENCE`, `10_STRUCTURE`, `20_DETAIL`, `30_STATE_OVERLAY`, `40_COLLISION`, and `90_EXPORT`. Set the object origin to `(0,0,0)` at bottom-left-back.

- [ ] **Step 4: Render geometry-authority views**

Use 70 mm orthographic-style cameras for `Front`, `Back`, `Left`, and `Right`, plus a 50 mm three-quarter perspective. Render at 2048 × 2048 with neutral gray world and neutral three-point lighting. Hide collision and reference collections.

- [ ] **Step 5: Generate geometry-preserving surface and state studies**

Use the neutral perspective as the edit reference. Repeat this invariant text in every ImageGen call:

```text
ProjectRift UE5.8 modular ship-hub wall-door asset. Preserve the exact input camera, 400×30×400 cm silhouette, 240×280 cm open doorway, jamb widths, lintel height, module boundaries, perspective, and structural recesses. Apply grounded realistic PBR hard-surface treatment with dark load-bearing steel, cool-gray armor panels, black seals, restrained functional detail, clean readable surfaces, and medium detail density. No room, floor, character, door leaf, text, watermark, purple energy, organic growth, loose prop clutter, or geometry outside the existing envelope.
```

Generate one base treatment and three state edits:

- Damaged: power off, localized soot and abrasion, one cosmetic cover absent, no silhouette deformation;
- Patched: two restrained repair plates, a short protected cable segment, amber maintenance strips;
- Online: secured covers, small cyan functional strips, cold-white work light, not factory-new.

Record tool, mode, exact prompt, input/output paths, SHA-256, timestamp, and commercial review status in the generation ledger.

- [ ] **Step 6: Reject visual drift before user review**

Overlay each generated study at 50% opacity with the neutral perspective. Reject if the exterior silhouette, door opening, snap boundary, camera, or major recess shifts by more than 2% of image width, or if prohibited purple/text/clutter appears. Allow at most one targeted regeneration per failed state; if still invalid, stop and report the G3.0 blocker.

- [ ] **Step 7: Assemble and present the appearance-lock sheet**

Create one lossless PNG with front/back/left/right, base perspective, and the three states. Include deterministic labels added during publishing, never generated text. Present it to the user and ask only for appearance approval.

- [ ] **Step 8: Record approval and stop at the Appearance Gate**

Only after explicit approval set:

```json
"Appearance": {
  "Status": "Approved",
  "Date": "2026-08-03",
  "Evidence": [
    "Concept/SM_ShipHub_WallDoor_400_A_AppearanceLock.png",
    "Reports/appearance-validation.json"
  ]
}
```

Run the `-RequireAppearance` test again. Report evidence and suggest commit message `art: approve ship hub wall-door appearance lock`. Stop; do not begin production modeling until the user approves this Gate.

---

### Task 11: Build the Authoritative Production Mesh

**Files:**
- Modify: `Scripts/ProjectRift/ArtPipeline/shiphub/build_wall_door_first_article.py`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Blender/SM_ShipHub_WallDoor_400_A.blend`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/geometry-validation.json`
- Modify: `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubWallDoor.ps1`

**Interfaces:**
- Consumes: approved appearance sheet and immutable contract.
- Produces: bake-ready base mesh, damaged/patch overlay meshes, deterministic camera set, and geometry evidence.

- [ ] **Step 1: Add failing generated-geometry assertions**

Add `-RequireProductionMesh` and assert exact bounds/opening/pivot, applied scale, material-slot count `<=2`, three named collision intents, zero unexpected non-manifold edges, zero duplicate faces, UV layer names `UV0` and `UV1`, and the presence of separate overlay objects.

- [ ] **Step 2: Verify the new assertions fail**

Run the PowerShell test with `-RequireProductionMesh`. Expected: FAIL because the authoritative `.blend` and report are absent.

- [ ] **Step 3: Refine silhouette and construction layers**

Convert the approved appearance structure into production geometry using non-destructive Boolean, Bevel, Weighted Normal, Mirror, and Array modifiers where appropriate. Keep:

- primary silhouette edges at 4–6 cm bevel width;
- secondary panel edges at 0.8–1.5 cm bevel width;
- functional recess depth at 2–6 cm;
- all front detail inside Y `0..30 cm`;
- the opening exactly X `80..320 cm`, Z `0..280 cm`;
- the outer snap faces planar and untouched.

Do not model scratches, random bolts, grain, painted labels, or tiny repeated seams.

- [ ] **Step 4: Create state overlays without cloning the base**

Create `SM_ShipHub_WallDoor_400_A_Overlay_Damaged` for the absent-cover rim and localized damaged edge inserts, and `SM_ShipHub_WallDoor_400_A_Overlay_Patched` for two repair plates plus a protected cable guide. Neither overlay may cross the doorway or outer snap planes. Online uses no silhouette overlay.

- [ ] **Step 5: Apply transforms and stabilize topology**

Apply export-facing modifiers, triangulate only at the final export duplicate, recalculate outward normals, remove accidental doubles, and keep the editable source objects non-destructive in their working collections. Base and overlay export objects must each have location/rotation zero and scale one.

- [ ] **Step 6: Write independent geometry evidence**

The generated report must contain:

```json
{
  "Schema": "projectrift.shiphub.wall-door-geometry-validation.v1",
  "AssetId": "SM_ShipHub_WallDoor_400_A",
  "Passed": true,
  "BoundsCm": [400, 30, 400],
  "DoorOpeningCm": [240, 280],
  "PivotCm": [0, 0, 0],
  "AppliedScale": [1, 1, 1],
  "MaterialSlotCount": 2,
  "UnexpectedNonManifoldEdgeCount": 0,
  "DuplicateFaceCount": 0,
  "OverlayObjects": [
    "SM_ShipHub_WallDoor_400_A_Overlay_Damaged",
    "SM_ShipHub_WallDoor_400_A_Overlay_Patched"
  ]
}
```

- [ ] **Step 7: Run Blender build and geometry tests**

Run:

```powershell
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubWallDoor.ps1 -Stage BuildProduction -BlenderExe 'D:\Blender5.2\blender.exe'
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubWallDoor.ps1 -RequireProductionMesh
```

Expected: PASS with exact geometry evidence.

- [ ] **Step 8: User Git checkpoint**

Report renders, polygon counts, topology evidence, and remaining texture risk. Suggest commit message `art: build ship hub wall-door production mesh`. Do not stage or commit.

---

### Task 12: Author UVs, PBR Textures, and Three DCC States

**Files:**
- Modify: `Scripts/ProjectRift/ArtPipeline/shiphub/build_wall_door_first_article.py`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Textures/T_ShipHub_WallDoor_400_A_BC.png`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Textures/T_ShipHub_WallDoor_400_A_N.png`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Textures/T_ShipHub_WallDoor_400_A_ORM.png`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Textures/T_ShipHub_WallDoor_400_A_StateMask.png`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Textures/Source/SM_ShipHub_WallDoor_400_A_TextureSource.blend`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/texture-validation.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Concept/DCCReview/Damaged.png`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Concept/DCCReview/Patched.png`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Concept/DCCReview/Online.png`
- Modify: `Scripts/ProjectRift/Tests/Test-ProjectRiftShipHubWallDoor.ps1`

**Interfaces:**
- Consumes: production mesh and approved appearance/state references.
- Produces: UV0/UV1, four 2048 texture sets, material-slot assignment, and three DCC review renders.

- [ ] **Step 1: Add failing UV and texture assertions**

Assert `UV0` and `UV1` exist, all UV coordinates are finite, UV0 packing stays within 0–1, baked images are exactly 2048 × 2048, BC is RGB/sRGB, Normal is RGB/non-color, ORM and StateMask are RGB/non-color, and no image is empty or single-color.

- [ ] **Step 2: Verify missing texture outputs fail**

Run `Test-ProjectRiftShipHubWallDoor.ps1 -RequireTextures`. Expected: FAIL listing four missing textures and missing UV evidence.

- [ ] **Step 3: Unwrap with the required density and seam policy**

Use `UV0` for material textures and `UV1` for non-overlapping lightmap compatibility. Place seams on rear edges, panel breaks, reveal corners, and inaccessible recesses. Keep front jambs and lintel orientation consistent; target about 512 px/m on visible structural surfaces. Allow mirrored hidden/repeating regions only in UV0; UV1 must not overlap.

- [ ] **Step 4: Build the free procedural PBR source**

In the texture source `.blend`, create dark painted structural steel, cool-gray armor coating, exposed steel, and black seals. Bake:

- BC without lighting or AO;
- tangent-space DirectX-compatible Normal for UE;
- ORM with R=AO, G=Roughness, B=Metallic;
- StateMask with R=damage, G=patch, B=functional emissive.

Keep base roughness mainly `0.42..0.72`, exposed metal `0.28..0.52`, rubber `0.62..0.82`. Limit edge wear to high-contact or collision-damaged locations and keep broad surfaces visually calm.

- [ ] **Step 5: Assign no more than two material slots**

Slot 0 is `ShipHub_Surface`; slot 1 is `ShipHub_Functional`. Damage and patch differences are controlled by StateMask and overlay visibility, not extra base slots.

- [ ] **Step 6: Render the three DCC states from fixed cameras**

Use identical camera, exposure, and neutral lighting for Damaged, Patched, and Online. Damaged has no emissive; Patched uses amber only; Online uses cyan only. Save both full view and one close-up per state if the contact sheet cannot show bevel/roughness quality at original resolution.

- [ ] **Step 7: Run texture generation and tests**

Run:

```powershell
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubWallDoor.ps1 -Stage BakeTextures -BlenderExe 'D:\Blender5.2\blender.exe'
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubWallDoor.ps1 -RequireProductionMesh -RequireTextures
```

Expected: PASS with 2048 image metadata, UV evidence, and exactly two material slots.

- [ ] **Step 8: User Git checkpoint**

Present the three state renders and texture channel contact sheet. Suggest commit message `art: texture ship hub wall-door first article`. Do not stage or commit.

---

### Task 13: Add Collision, Export FBX, Validate the Package, and Pass the DCC Gate

**Files:**
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/validate_wall_door_blender.py`
- Create: `Scripts/ProjectRift/ArtPipeline/shiphub/validate_wall_door_package.py`
- Create: `Scripts/ProjectRift/Tests/test_shiphub_wall_door_package.py`
- Modify: `Scripts/ProjectRift/ArtPipeline/Invoke-ProjectRiftShipHubWallDoor.ps1`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Exports/SM_ShipHub_WallDoor_400_A.fbx`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Exports/SM_ShipHub_WallDoor_400_A_Overlay_Damaged.fbx`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Exports/SM_ShipHub_WallDoor_400_A_Overlay_Patched.fbx`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/export-validation.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/package-validation.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/SHA256SUMS.txt`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/License/ProjectOwnedDeclaration.md`
- Modify after explicit approval: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.approval.json`

**Interfaces:**
- Consumes: authoritative `.blend`, four textures, and approved appearance record.
- Produces: UE-ready FBXs, collision, independent reimport evidence, complete DCC package, and `DCC.Status == Approved`.

- [ ] **Step 1: Write mutation-based package tests**

Copy the package into `Saved/Automation/ProjectRiftShipHubWallDoor` using hard links where available. Mutate one item per test and require a structured failure rule:

- changed FBX bytes → `EXPORT_EVIDENCE_SHA256_FBX`;
- 4096 texture substituted for 2048 → `TEXTURE_DIMENSION`;
- opening width changed to 239 in report → `DOOR_OPENING`;
- fourth collision object → `COLLISION_COUNT`;
- material slot count 3 → `MATERIAL_SLOT_LIMIT`;
- missing project-owned declaration → `LICENSE_EVIDENCE`;
- unresolved `TODO`, `TBD`, or `placeholder` marker → `UNRESOLVED_MARKER`.

- [ ] **Step 2: Verify package tests fail before validators exist**

Run:

```powershell
python -m unittest Scripts.ProjectRift.Tests.test_shiphub_wall_door_package -v
```

Expected: FAIL because validators and exports are absent.

- [ ] **Step 3: Create three simple UCX pieces**

Name them exactly:

```text
UCX_SM_ShipHub_WallDoor_400_A_00
UCX_SM_ShipHub_WallDoor_400_A_01
UCX_SM_ShipHub_WallDoor_400_A_02
```

They cover left jamb, right jamb, and lintel respectively. No collision face may cross X `80..320 cm` below Z `280 cm`. Overlay FBXs contain no collision.

- [ ] **Step 4: Export with one frozen FBX preset**

Select only the base export mesh and three UCX objects for the base FBX. Export overlays separately. Use `axis_forward='-Y'`, `axis_up='Z'`, applied unit scale, mesh modifiers, face smoothing, no animation, and no leaf bones. Do not export cameras, lights, references, or working high-detail objects.

- [ ] **Step 5: Independently reimport and inspect every FBX**

Open each FBX in a factory-startup Blender process, measure bounds, names, material slots, normals, triangles, and collision count, and compare SHA-256 values to source evidence. The base must be 4 × 0.3 × 4 m and preserve the 2.4 × 2.8 m open passage.

- [ ] **Step 6: Validate the complete package and write hashes**

The package validator checks the contract hash, approval status, source `.blend`, four textures, three FBXs, three DCC renders, two validation reports, license declaration, and all expected SHA-256 entries. It writes `Passed`, `IssueCount`, and structured `Issues` without claiming PASS when any required artifact is missing.

- [ ] **Step 7: Run full DCC verification**

Run:

```powershell
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubWallDoor.ps1 -Stage Export -BlenderExe 'D:\Blender5.2\blender.exe'
.\Scripts\ProjectRift\ArtPipeline\Invoke-ProjectRiftShipHubWallDoor.ps1 -Stage ValidatePackage -BlenderExe 'D:\Blender5.2\blender.exe'
python -m unittest Scripts.ProjectRift.Tests.test_shiphub_wall_door_contract Scripts.ProjectRift.Tests.test_shiphub_wall_door_package -v
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftShipHubWallDoor.ps1 -RequireAppearance -RequireProductionMesh -RequireTextures -RequireExports
```

Expected: all PASS and `package-validation.json` has `Passed: true`.

- [ ] **Step 8: Present DCC evidence and stop at the DCC Gate**

Show fixed-camera Damaged/Patched/Online renders, close-ups, wireframe/UV contact sheet, collision view, and validation summary. Only after explicit approval set `DCC.Status` to `Approved` with exact evidence paths, rerun package validation, and suggest commit message `art: approve ship hub wall-door DCC first article`. Stop before UE import.

---

### Task 14: Import the First Article and Build UE Materials

**Files:**
- Create: `Content/ProjectRift/ShipHub/Meshes/Structure/SM_ShipHub_WallDoor_400_A.uasset`
- Create: `Content/ProjectRift/ShipHub/Meshes/Structure/SM_ShipHub_WallDoor_400_A_Overlay_Damaged.uasset`
- Create: `Content/ProjectRift/ShipHub/Meshes/Structure/SM_ShipHub_WallDoor_400_A_Overlay_Patched.uasset`
- Create: four texture assets under `Content/ProjectRift/ShipHub/Textures/`
- Create: `Content/ProjectRift/ShipHub/Materials/M_ShipHub_Surface.uasset`
- Create: `Content/ProjectRift/ShipHub/Materials/M_ShipHub_Functional.uasset`
- Create: three state material instances under `Content/ProjectRift/ShipHub/Materials/`
- Modify: `Config/ProjectRift/AssetLicenseRegistry.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/ue-import-validation.json`

**Interfaces:**
- Consumes: DCC-approved FBXs/textures and only ProjectA's UE editor/MCP endpoint.
- Produces: correctly named UE meshes, simple collision, Nanite or documented LOD route, textures, reusable master materials, state material instances, and import evidence.

- [ ] **Step 1: Confirm the DCC Gate and ProjectA editor identity**

Read `approval.json`; refuse import unless `DCC.Status` is exactly `Approved`. Confirm the editor project file resolves to `E:\MyWork\ProjectA\ProjectA.uproject`. If MCP is used, connect only to `127.0.0.1:8001/mcp`.

- [ ] **Step 2: Import the base FBX through the legacy Static Mesh pipeline**

Import at scale 1 with combined mesh disabled where collision naming requires it, import normals/tangents, generate lightmap UV disabled because UV1 already exists, and auto-generate collision disabled. Confirm the three UCX pieces are recognized as simple collision and are not separate render meshes.

- [ ] **Step 3: Import overlay FBXs and textures**

Import the two overlays without collision. Configure BC as sRGB, Normal as Normal Map/non-sRGB, and ORM/StateMask as Masks/non-sRGB. All textures remain 2048 and must not acquire unexpected alpha or virtual-texture settings.

- [ ] **Step 4: Create reusable materials and state instances**

`M_ShipHub_Surface` samples BC, Normal, and ORM; connects ORM R→AO, G→Roughness, B→Metallic; exposes scalar roughness multiplier and vector tint. `M_ShipHub_Functional` uses StateMask B for emissive and exposes emissive color/intensity.

Create instances:

- Damaged: emissive intensity 0, damage mask enabled;
- Patched: amber `#D88A32`, restrained intensity, patch mask enabled;
- Online: cyan `#58C7D9`, restrained intensity, functional mask enabled.

- [ ] **Step 5: Enable and verify the required geometry performance route**

Enable Nanite for the three meshes if UE accepts the geometry and simple collision remains correct. If Nanite is rejected, create an explicit LOD1 and record the reason and triangle ratio; do not leave the asset with neither Nanite nor LOD.

- [ ] **Step 6: Add exact license coverage**

Append a ProjectOwned folder record for `/Game/ProjectRift/ShipHub` pointing to `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/License/ProjectOwnedDeclaration.md`, reviewed by ProjectA on the actual execution date. Do not weaken or remove existing registry records.

- [ ] **Step 7: Capture import evidence and test reimport**

Record asset paths, imported source hashes, bounds, pivot, material slot count, UV channel count, simple collision count, Nanite/LOD status, texture dimensions/compression, and actor scale. Reimport the unchanged base FBX and textures; verify package paths, material assignments, collision, and references are unchanged.

- [ ] **Step 8: Run production validation**

With ProjectA editor closed only if the commandlet requires it, run:

```powershell
UnrealEditor-Cmd.exe E:\MyWork\ProjectA\ProjectA.uproject -run=PRProductionValidation -unattended -nop4 -ProjectRiftProductionReportDir=Saved/Automation/ProjectRiftShipHubWallDoor/ProductionValidation
```

Expected: exit `0`, `production-validation.json` and `.md` are written, and no new ShipHub asset has an Error issue. Existing unrelated warnings are reported separately and not silently deleted.

- [ ] **Step 9: User Git checkpoint**

Report created `.uasset` files, reimport evidence, commandlet result, and suggested commit message `art: import ship hub wall-door first article`. Do not stage or commit.

---

### Task 15: Build the State Preview Blueprint and Art Validation Map

**Files:**
- Create: `Content/ProjectRift/ShipHub/Blueprints/BP_ShipHub_WallDoor_400_A_StatePreview.uasset`
- Create: `Content/ProjectRift/ShipHub/Maps/L_ShipHub_ArtValidation.umap`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/ue-map-validation.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/UEReview/*.png`

**Interfaces:**
- Consumes: imported base/overlay meshes and three material instances.
- Produces: visual-only state switching, a 2×2 modular test bay, traversal/collision test area, fixed cameras, and map evidence.

- [ ] **Step 1: Create a visual-only preview Blueprint**

Add one base StaticMeshComponent, one Damaged overlay component, one Patched overlay component, and functional light components. Expose an editor instance integer `PreviewState` with validated values `0=Damaged`, `1=Patched`, `2=Online`. Construction Script behavior is exact:

- 0: Damaged material, Damaged overlay visible, Patched overlay hidden, all functional lights off;
- 1: Patched material, Patched overlay visible, Damaged overlay hidden, amber maintenance lights on;
- 2: Online material, both overlays hidden, cyan functional lights on.

The Blueprint contains no Tick, input, door movement, interaction interface, networking, or gameplay state replication.

- [ ] **Step 2: Create the independent validation map**

Create `/Game/ProjectRift/ShipHub/Maps/L_ShipHub_ArtValidation`. Do not duplicate or modify `L_ShipLobby`. Place:

- one 100 cm reference cube;
- one actual `BP_PRCharacter` reference or the authoritative character capsule if spawning the Blueprint has side effects;
- one intelligent-construct scale proxy using the approved Task 8 reference;
- three single preview assets, one per state;
- a 2×2 wall-door snap arrangement on the 50 cm grid;
- neutral, cold-ship, and emergency lighting zones;
- fixed cameras for front, perspective, close-up, collision, and seam views.

- [ ] **Step 3: Verify snapping, bounds, and actor transforms**

All base module actors use scale `1,1,1`, 90° rotations, and 50 cm-aligned locations. The 2×2 arrangement must have no visible gap beyond the designed seam and no overlap at the outer snap faces.

- [ ] **Step 4: Verify traversal and collision in PIE**

Possess the real ProjectRift character and walk through every door orientation. Confirm the 240 × 280 cm opening remains passable, the camera does not sustain unacceptable clipping, and collision does not catch on the threshold. Also move the construct proxy through the opening or validate its approved collision envelope against the opening.

- [ ] **Step 5: Verify all three states under three lighting conditions**

Check that Damaged is unpowered, Patched is amber, Online is cyan, purple never appears, overlays never affect collision, and broad surfaces remain readable rather than noisy or uniformly dirty.

- [ ] **Step 6: Save fixed-camera evidence**

Capture lossless PNGs for front, rear/perspective, bevel close-up, 2×2 seam, collision visualization, and the three states. Record map path, actor transforms, state component visibility, passage result, and screenshot hashes in `ue-map-validation.json`.

- [ ] **Step 7: Reopen ProjectA and revalidate persistence**

Close and reopen only ProjectA, load `L_ShipHub_ArtValidation`, and confirm map references, materials, Blueprint state, mesh collision, and fixed cameras persist. Never interact with another editor process.

- [ ] **Step 8: User Git checkpoint**

Report map/Blueprint assets, PIE evidence, screenshots, and suggested commit message `art: add ship hub wall-door validation scene`. Do not stage or commit.

---

### Task 16: Run Final G3 Validation and Pass the UE Gate

**Files:**
- Modify after explicit approval: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Briefs/SM_ShipHub_WallDoor_400_A.approval.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/g3-final-validation.json`
- Create: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/G3_Acceptance.md`
- Modify: `SourceArt/ProjectRift/ShipHub/FirstArticle/WallDoor_400_A/Reports/SHA256SUMS.txt`

**Interfaces:**
- Consumes: approved appearance, approved DCC package, UE import evidence, validation map evidence, production validation reports.
- Produces: one auditable G3 acceptance package and `UE.Status == Approved`.

- [ ] **Step 1: Run a clean full DCC verification**

Run `AllDCC`, both Python test modules, and the PowerShell test with every required-artifact switch. Expected: all PASS without regenerating different hashes for unchanged inputs.

- [ ] **Step 2: Run ProjectRift production validation again**

Run `PRProductionValidation` to a fresh `Saved/Automation/ProjectRiftShipHubWallDoor/FinalProductionValidation` directory. Require zero Error issues for all `/Game/ProjectRift/ShipHub` assets and record any unrelated pre-existing warnings separately.

- [ ] **Step 3: Repeat reimport and PIE smoke checks**

Reimport unchanged FBX/textures, reopen `L_ShipHub_ArtValidation`, test all three states, traverse the doorway, and confirm `L_ShipLobby` timestamp/hash is unchanged from the pre-G3 baseline.

- [ ] **Step 4: Write the final machine-readable report**

The report must include:

```json
{
  "Schema": "projectrift.shiphub.wall-door-g3-final-validation.v1",
  "AssetId": "SM_ShipHub_WallDoor_400_A",
  "Passed": true,
  "AppearanceApproved": true,
  "DCCApproved": true,
  "BoundsCm": [400, 30, 400],
  "DoorOpeningCm": [240, 280],
  "ActorScale": [1, 1, 1],
  "CollisionPieceCount": 3,
  "MaterialSlotCount": 2,
  "TextureSize": 2048,
  "States": ["Damaged", "Patched", "Online"],
  "ReimportStable": true,
  "TraversalPassed": true,
  "ProductionValidationErrors": 0,
  "ShipLobbyModified": false
}
```

Do not set `Passed: true` until each evidence source has been independently read during the final run.

- [ ] **Step 5: Present the UE Gate package to the user**

Show fixed cameras, three states, collision/traversal, 2×2 seams, and concise DCC/UE validation summaries. Provide exact paths for the authoritative `.blend`, FBXs, textures, map, Blueprint, and reports.

- [ ] **Step 6: Record UE approval only after explicit acceptance**

Set `UE.Status` to `Approved`, record the actual date and exact evidence paths, refresh package hashes, and rerun only the non-mutating final validators. If rejected, preserve `Pending`, record exact rejection notes, and return only to the responsible Gate.

- [ ] **Step 7: Stop at the version gate**

Report all changed files/assets, verification evidence, manual acceptance result, residual risks, and suggested commit message `art: complete ship hub wall-door G3 first article`. Do not stage or commit. Do not plan or begin G4 kit batch production until the user explicitly starts it.

---

## Final Acceptance Checklist

- [ ] Appearance orthographic/state sheet explicitly approved.
- [ ] Authoritative Blender source opens in Blender 5.2 LTS.
- [ ] Base bounds, opening, pivot, scale, UVs, normals, and two-slot material limit pass.
- [ ] Damage and patch overlays are independent; Online uses the base silhouette.
- [ ] Four 2048 PBR/state textures pass metadata and content validation.
- [ ] Three FBXs reimport with stable bounds, names, materials, and hashes.
- [ ] Three UCX pieces leave the door opening clear.
- [ ] UE assets use correct paths, names, texture settings, simple collision, and Nanite or LOD.
- [ ] State preview Blueprint contains no gameplay interaction or Tick.
- [ ] `L_ShipHub_ArtValidation` passes 2×2 seams, actual character traversal, camera, and three lighting/state checks.
- [ ] `PRProductionValidation` reports no ShipHub errors.
- [ ] `L_ShipLobby` remains unchanged.
- [ ] License record, reports, screenshots, and SHA-256 manifest are complete.
- [ ] UE Gate explicitly approved, then work stops.

## Reference Documents

- `docs/superpowers/specs/2026-08-03-projectrift-shiphub-wall-door-g3-first-article-design.md`
- `docs/superpowers/specs/2026-07-31-projectrift-ship-hub-art-pipeline-design.md`
- `docs/superpowers/specs/2026-08-01-projectrift-shiphub-complete-modeling-drawings-design.md`
- `docs/projectrift/production/AssetNamingAndDirectories.md`
- `docs/projectrift/production/AssetImportAndLicenseChecklist.md`
- `docs/projectrift/production/PerformanceBudget.md`
- `Config/ProjectRift/ProductionValidation.json`
- `Config/ProjectRift/AssetLicenseRegistry.json`
