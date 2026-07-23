# ProjectRift Chapter-Level Production Interface

Every declared production chapter map is validated against stable authored interfaces:

- **SpawnGroups:** `APREnemySpawnPoint` with non-empty `SpawnGroupId`.
- **ObjectiveSockets:** graph-linked `APRRiftObjectiveActor` with an `ObjectiveNodeId`.
- **ExtractionSocket:** one `APRExtractionZone`.
- **BossArena:** one `APRBossEncounterController` when the contract requires it.
- **StreamingPartitions:** `APRProductionLevelMarker` with kind `StreamingPartition` and a stable `MarkerId`.

The required IDs are declared in `Config/ProjectRift/ProductionValidation.json`. The validator loads maps read-only and reports `PRP-LVL-000` through `PRP-LVL-005`; it does not add or repair actors. `/Game/ProjectRift/Developer/Maps/L_ProductionValidation_Test` is the isolated fixture for these five interfaces.
