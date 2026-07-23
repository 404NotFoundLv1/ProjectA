# ProjectRift First-Production Performance Budget

These are production-entry ceilings, not final platform profiling targets.

| Category | Blocking rule | Review guidance |
|---|---|---|
| Texture | Maximum 4096px; UI may use non-power-of-two dimensions | Prefer the smallest resolution that preserves the silhouette/readability target. |
| Static mesh | Simple collision plus Nanite or an authored LOD chain | Keep material slots and shadow-casting surfaces deliberately small. |
| Material | Runtime instances use `MI_` rather than editing shared base materials | Review texture samplers and translucent surfaces per map. |
| Skeletal mesh | Asset-specific review before import | Record bone/material counts in the import review. |
| Audio | ProjectRift sound class required | Review duration, compression, concurrency, and routing before final mix. |
| Map | Actor, light, shadow, decal, Niagara, and always-loaded references reviewed per chapter | Use the production validation report as the baseline inventory. |

Any exception requires an explicit policy change and a documented performance rationale; do not bypass a failed validation rule by moving the asset to an unscanned folder.
