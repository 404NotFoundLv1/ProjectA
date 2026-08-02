# ProjectRift Ship Hub Task 6 SVG and Handoff Fix Design

Date: 2026-08-03
Status: Awaiting user review
Scope: Task 6 repair only; Task 7 and detailed asset modeling remain out of scope.

## Problem statement

Task 6 has two distinct delivery defects:

1. The generated SVG dimension chains are placed by a generic grid. They cross the model, use horizontal lines for values that describe vertical depth, duplicate notes, and do not reliably associate a value with the geometry it measures.
2. The authoritative PDF and contact-sheet PNG are structurally valid, but reopening a regenerated file through Codex/WPS under a previously used filename can return a cached or otherwise stale handoff. A uniquely named compatibility PDF opened successfully in the user's WPS installation, while reused PDF and PNG filenames did not.

## Approved direction

The user approved:

- SVG option A: perimeter overall dimensions plus object-local dimensions and keyed schedules.
- Handoff option A: preserve frozen authoritative filenames while creating uniquely named delivery copies containing a revision and a short content hash.

## Authoritative and handoff outputs

The authoritative Task 6 paths remain unchanged:

- `Drawings/ProjectRift_ShipHub_CompleteDesign_v1.pdf`
- `Drawings/ProjectRift_ShipHub_ContactSheet_v1.png`
- `Drawings/SVG/*.svg`
- `Drawings/FinalPNG/*.png`

The publisher additionally creates exactly two files under `Drawings/Handoff/`:

- `ProjectRift_ShipHub_CompleteDesign_v1r2_<pdf-sha8>.pdf`
- `ProjectRift_ShipHub_ContactSheet_v1r2_<png-sha8>.png`

`<pdf-sha8>` and `<png-sha8>` are the first eight lowercase hexadecimal characters of the SHA-256 of the authoritative file bytes. The handoff copies must be byte-identical to their authoritative sources. Any byte change produces a new filename, preventing filename-based cache reuse.

The authoritative PDF is emitted in ReportLab invariant mode so identical accepted inputs produce byte-identical PDF bytes and a stable handoff filename. Publication-time metadata must not introduce hash churn. A repeated publish without input changes must reproduce the same authoritative hashes and the same two handoff names.

Publishing is transactional. The staged `Handoff` directory is committed with `SVG`, `FinalPNG`, the authoritative PDF, and the authoritative contact sheet. The committed `Handoff` directory contains only the current pair. If any replacement fails, the previous complete package is restored. Superseded handoff copies are recoverable by republishing the corresponding source revision and are not retained in the active output tree.

## SVG drawing zones

Every A3 SVG uses four explicit zones:

- Plot zone: the raster drawing and only object-local dimensions.
- Perimeter dimension zone: overall width, depth, and height dimensions outside the model silhouette.
- Keyed note zone: coordinates, sequences, counts, angles, and operational envelopes that are not linear measurements.
- Title block: sheet identity, scale, units, section mark, and brief hash only.

The title block does not repeat dimension values already displayed in the plot or perimeter zones.

## Dimension semantics

Each dimension is an explicit per-sheet specification, not an index-derived grid position. A specification records:

- stable identifier;
- label;
- semantic kind: `overall`, `object`, `angular`, or `schedule`;
- orientation: `horizontal`, `vertical`, `diameter`, `angular`, or `note`;
- measured endpoints or object anchor;
- dimension-line position;
- label position;
- optional schedule key.

Rules:

- Horizontal values use horizontal dimension lines; vertical values use vertical lines.
- Overall dimensions sit outside the plot silhouette with extension lines returning to the measured extents.
- Diameter dimensions cross or closely bracket the measured circular object and use a diameter prefix.
- Angular values use an arc or keyed note, never a generic horizontal dimension line.
- Coordinates, ordered center positions, counts, and operational descriptions use keyed schedules rather than arrowed dimension chains.
- No overall dimension line crosses the model silhouette.
- Labels do not overlap each other, the north arrow, scale bar, model silhouette, or title block.
- A total plan/elevation sheet uses at most two perimeter chains. A detail sheet uses at most four direct dimensions. Remaining data moves to the keyed note zone.

## Sheet-specific intent

### A01 floor plan

- `28 m overall width`: horizontal chain below the room silhouette.
- `24 m overall depth`: vertical chain left of the room silhouette.
- `8 m navigation-table diameter`: object-local diameter across the table.
- `5 m main path`: keyed clearance note anchored to the main circulation lane unless the base render provides two unambiguous measured edges.
- Cryopod centers: keyed schedule `P1-P5`, preserving `X = -4, -2, 0, 2, 4 m`.
- Construct docks: keyed schedule `D1-D4`, preserving the four frozen coordinates.

### A02-A09 architectural sheets

- A02 uses local concentric diameter dimensions for the 16 m and 10 m service rings.
- A03-A08 use one horizontal overall chain and one vertical height chain outside the silhouette.
- A09 uses a vertical exploded-lift chain and a local structural-bay dimension.

### A10 perspective sheet

- No arrowed dimensions. It retains the six approved perspective sources and a perspective index only.

### D01-D05 detail sheets

- D01 uses explicit width/depth/height dimensions, an 18-degree recline arc, a 75-degree door arc, and a keyed 1.2 m operating-envelope note. Direct dimensions are limited to the four most important geometry measurements.
- D02 uses concentric diameter dimensions and one vertical table-height dimension.
- D03 uses width and height on the clear opening, one local depth dimension, and a keyed muster-area note.
- D04 uses one diameter dimension, one recess-depth dimension, and a keyed count/location note.
- D05 uses a chained 1 m / 2 m / 4 m bay-width dimension and one wall-thickness dimension.

## Rendering and style

- Preserve the current A3 `viewBox="0 0 420 297"`, dark technical-drawing palette, title block, north arrow, scale bar, and accepted raster sources.
- Use thinner extension lines than dimension lines.
- Use compact inward/outward arrowheads appropriate to available space.
- Keep annotation text horizontal except vertical overall labels, which may rotate 90 degrees.
- Use one annotation color consistently. Schedules use neutral title-block text with colored keys.
- Preserve relative raster references so SVG files remain editable and project-owned.

## Validation contract

Automated tests must exercise generated artifacts rather than source-text patterns.

### SVG tests

- Exactly 15 parseable SVGs with the frozen sheet names and A3 view box.
- A01 overall-width line is horizontal and below the plot silhouette.
- A01 overall-depth line is vertical and left of the plot silhouette.
- A01 table diameter is object-local.
- A01 pod and dock data appear once in keyed schedules and not as arrowed chains.
- A02 diameter dimensions are object-local.
- A03-A08 each contain no more than two perimeter chains with correct orientation.
- A10 contains no arrowed dimension chain.
- D01 angular values are arcs or schedule notes rather than horizontal dimension lines.
- No sheet repeats a direct dimension label in the title block.

### Handoff tests

- `Drawings/Handoff` contains exactly the current PDF and PNG pair.
- Each handoff filename contains `v1r2` and the correct eight-character source hash.
- Each handoff copy is byte-identical to its authoritative source.
- Publishing twice from identical inputs reproduces identical authoritative hashes and identical handoff filenames.
- The PDF remains below 32 MiB, has 15 A3 pages, and uses one `/DCTDecode` image per page.
- The PNG has a valid PNG signature and remains 4961 x 3508.
- Repository strict validation rejects missing, stale, extra, wrongly hashed, or non-identical handoff files.

## Visual acceptance

After publication:

- Render all 15 SVGs to preview images and inspect them as one contact sheet.
- Inspect A01, A02, D01, and D03 at full resolution.
- Render all 15 PDF pages and inspect the page contact sheet plus A01 at 300 DPI.
- Deliver the uniquely named handoff PDF and PNG for the user's WPS/Photos confirmation.

## Boundaries

- Do not modify accepted Task 4 BLEND/FBX/GLB/layout authority files.
- Do not modify accepted Task 5 base renders or perspective inputs.
- Do not start UE, MCP, Blender authoring, Task 7, or detailed asset modeling.
- Do not perform Git write operations.
