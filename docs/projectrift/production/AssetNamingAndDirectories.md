# ProjectRift Asset Naming and Directories

All new production assets live under `/Game/ProjectRift/<Domain>`. Do not add assets directly under `/Game` or `/Game/ProjectRift`; do not place ProjectRift production assets in `Developer`, template, or `Variant_*` roots.

| Asset | Prefix | Approved domain examples |
|---|---|---|
| Map | `L_` | `Maps` |
| Blueprint | `BP_` | `Blueprints`, `Characters`, `Enemies` |
| Data asset | `DA_` | `Items`, `Bosses/Definitions`, `Missions` |
| Static / skeletal mesh | `SM_` / `SK_` | `Characters`, domain-specific `Meshes` |
| Texture | `T_` | `UI/Icons`, domain-specific `Textures` |
| Material / instance | `M_` / `MI_` | `Materials` |
| Niagara system / emitter | `NS_` / `NE_` | `VFX` |
| Sound wave / cue | `S_` / `SC_` | `SFX` |
| Widget | `WBP_` | `UI` |
| Input action / map | `IA_` / `IMC_` | `Input` |

Names use ASCII, contain no spaces, and must not end in `_New`, `_Copy`, or `_Duplicate`. Use the production validator before saving a new import. It reports problems; it never renames, moves, or deletes assets.
