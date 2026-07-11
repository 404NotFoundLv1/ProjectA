# Changelog

## [0.5.1] - 2026-07-11

### Added

- Added a native `ProjectRiftGameplayTags` contract for all 15 current input, role, damage, state, and cooldown tags.
- Added `UPRProjectSettings` under Project Settings > ProjectRift > Gameplay as the unique source for 11 Vertical Slice mission, objective, spawn, and extraction tuning values.
- Added native-tag, settings metadata/config, runtime settings-consumer, and removed-Actor-property regression coverage.

### Changed

- Replaced production string-based GameplayTag requests with native constants, including the `Data.Damage` SetByCaller contract.
- Removed the duplicate `ProjectRiftGameplayTags.ini` declarations and their staging allow-list entry.
- Routed Rift GameMode, objective, hold duration, spawn scaling/cap/interval, and extraction radius through project settings with defensive runtime clamps.
- Removed the corresponding Actor/Blueprint tuning storage while preserving the existing public getters, authority rules, replication, and mission flow.
- Updated the displayed project version to `0.5.1`.

### Verified

- ProjectAEditor and ProjectA Win64 Development targets build successfully.
- All 35 ProjectRift automation leaf tests pass; 28 without warnings and 7 with existing warnings.
- Ten affected Blueprints compile with warnings treated as errors, all inspected assets/maps are clean, and `/Game/ProjectRift` contains no redirectors.
- Win64 Development Build/Cook/Stage/Pak/Archive completes with AutomationTool exit code 0.
- The packaged NullRHI smoke reaches `L_ShipLobby` without fatal, assertion, GameplayTag, settings, or RHI errors.

## [0.5.0] - 2026-07-11

### Added

- Added the project-owned `UPRAssetManager` as the global Unreal AssetManager.
- Added canonical PrimaryAssetId construction and type-safe synchronous/asynchronous loading for `ProjectRiftItem` and `ProjectRiftLootTable`.
- Added startup validation that reports registered Primary Asset counts.
- Added AssetManager, async loading, configuration, Cook-rule, and Inventory-routing regression tests.

### Changed

- Registered the existing Item and LootTable Primary DataAssets through `PrimaryAssetTypesToScan` with `AlwaysCook` rules.
- Removed the `/Game/ProjectRift/Items` `DirectoriesToAlwaysCook` fallback so AssetManager owns the runtime data Cook contract.
- Routed Inventory fallback lookup through canonical `ProjectRiftItem:<ItemId>` identifiers instead of constructed object paths.
- Updated the displayed project version to `0.5.0`.

### Verified

- All 33 ProjectRift automation leaf tests pass.
- Seven affected Blueprints compile with warnings treated as errors.
- MCP registry tags report four `ProjectRiftItem` assets and one `ProjectRiftLootTable` asset.
- Win64 Development Build/Cook/Stage/Pak/Archive completes with AutomationTool exit code 0.
- The packaged game reports both Primary Asset types with nonzero counts, loads `L_Rift_Test`, and exits without fatal errors or Primary Asset failures.

## [0.4.8] - 2026-07-11

### Added

- Added stable `MissionId`, `RunId`, and `SettlementId` values to Rift settlement data.
- Added `LogProjectRiftFlow` structured lifecycle logging for mission, objective, extraction, settlement, return travel, and teardown phases.
- Added regression coverage for duplicate settlement, duplicate failure requests, duplicate enemy kill registration, timer teardown, project identity, and Cook inputs.

### Changed

- Made Rift settlement finalization idempotent per run so repeated completion/failure requests cannot grant resources twice.
- Made enemy kill registration idempotent per enemy instance.
- Cleared Rift return timers, spawn timers, delegates, runtime references, and tracked enemies during stop/end-play paths.
- Frozen the Development packaging baseline to the three ProjectRift maps and `/Game/ProjectRift/Items` runtime data.
- Set the displayed project identity to `ProjectRift` version `0.4.8` and explicitly staged ProjectRift gameplay-tag configuration.

### Verified

- ProjectA Editor and Win64 Development targets build successfully with Unreal Engine 5.8.
- All 31 ProjectRift automation leaf tests pass.
- Eight critical Blueprints compile with warnings treated as errors.
- Win64 Development Build/Cook/Stage/Pak/Archive completes with AutomationTool exit code 0.
- The packaged game loads `L_Rift_Test`, starts a one-player Rift mission, emits stable flow IDs, and exits cleanly without fatal errors or missing ProjectRift item assets.
