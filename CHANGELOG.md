# Changelog

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
