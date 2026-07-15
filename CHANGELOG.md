# Changelog

## [0.6.2] - 2026-07-15

### Added

- Added post-settlement `FPRResolvedDamage` reporting with exact shield/health splits, shield break, lethal state, preserved hit context, and original-instigator attribution.
- Added declaration-driven Light/Heavy hit reactions, `State.HitStaggered`, refresh/priority rules, stun supersession, movement restoration, and lifecycle cleanup.
- Added four replicated impact GameplayCues, three persistent status GameplayCues, ProjectRift-owned Cue notify assets, a tint overlay material, a lightweight impact particle, and shared player/enemy combat feedback presentation.
- Added owner-only unreliable hit confirmation, 0.15-second hit markers, 0.65-second shield/health damage numbers, local hit-direction feedback, and camera shake honoring the v4 reduced-shake setting.
- Added focused automation for result splitting, hit context, stagger timing/priority, Cue lifecycle and assets, hit-confirmation isolation, invalid post-settlement dispatch, and rapid Cue sequence rollover.

### Changed

- Rifle hits now request Light 0.12-second reactions, enemy melee requests Heavy 0.30-second reactions, assault blast reports Heavy presentation while its existing stun owns control, and pollution remains reaction/number-free.
- All ProjectRift abilities are blocked by both stun and hit stagger; hit stagger cancels active abilities, ADS, and reload without changing authoritative damage or ammo rules.
- GAS and enemy debug text now expose stagger, Cue, and source hit-confirmation state.
- `GameplayCueNotifyPaths` is narrowed to `/Game/ProjectRift/GameplayCues`, resolving the earlier broad GameplayCue search-path warning.
- Updated the displayed project version to `0.6.2`; the player save schema remains v4.

### Verification

- Focused evidence and the final build/package matrix are recorded in `docs/projectrift/v0.6.2-test-record.md`.

## [0.6.1] - 2026-07-15

### Added

- Added a data-driven semi-automatic hitscan test rifle, rifle ammunition, a replicated PlayerState-owned weapon component, and a replicated native placeholder weapon actor.
- Added server-authoritative magazine/reserve ammo, fire-rate limiting, camera-to-aim-point-to-muzzle tracing, manual partial reload, local ADS presentation, native weapon HUD, and inventory equip/unequip controls.
- Added `IA_Aim` and `IA_Reload`, right-mouse/left-trigger ADS, R/gamepad-X reload, and right-trigger primary fire mappings.
- Added focused weapon, hitscan, equipment, persistence, input, UI, asset, and multiplayer replacement automation coverage.

### Changed

- `UGA_PrimaryAttack` now delegates to the equipped weapon instead of the legacy spherical melee query while retaining its class, input tag, and public character input functions.
- v4 snapshots, multiplayer projections, settlement receipts, and lobby application-exit capture now preserve `Slot.Primary`, merge the loaded magazine into total persisted ammo, and refill the magazine when applying a profile.
- Empty or old profiles receive the test rifle exactly once; an unknown occupied primary slot is preserved while the rifle and its ammunition are placed in the backpack.
- The inventory rejects drops for data assets with `bCanDrop=false`; existing items remain droppable by default.
- GAS debug text now includes the equipped weapon, magazine, reserve, aiming, and reload state.
- Updated the displayed project version to `0.6.1`; the player save schema remains v4.

### Verification

- Focused verification evidence and final package hashes are recorded in `docs/projectrift/v0.6.1-test-record.md`.

## [0.6.0] - 2026-07-14

### Added

- Added `UPRDamageExecutionCalculation` with AttackPower scaling, pollution resistance, typed damage tags, legacy physical fallback, and fail-closed invalid input handling.
- Added declaration-driven pollution, slow, and stun GameplayEffects plus the shared `UPRCombatEffectLibrary` for hostile validation, damage, status replacement, and lifecycle cleanup.
- Added a server-only enemy melee GameplayAbility and migrated rift enemies to a self-owned Minimal-replication ASC with the shared AttributeSet.
- Added automation coverage for damage formulas, status timing and replacement, friendly-fire rejection, enemy GAS combat, lifecycle cleanup, and ability-defined debuffs.

### Changed

- Player attacks, assault abilities, and enemy melee now use the same server-authoritative GAS damage and status pipeline; player friendly fire and friendly debuffs are rejected.
- Assault charge applies a three-second 0.70 slow, assault blast applies a 1.25-second stun, and enemy melee applies five seconds of pollution at two base damage per tick.
- Player and enemy movement now follows aggregated MoveSpeed and State.Stunned; downed/dead combatants clear negative states and reject new combat effects.
- GAS debug text now shows AttackPower, MoveSpeed, PollutionResistance, and active states; enemy health bars show active state text.
- ProjectRift packaging now forwards `-nozenstore` to the Cook commandlet and waits for the UAT mutex, keeping local verification independent of shared Zen capacity without controlling another project process.
- Updated the displayed project version to `0.6.0`; the player save schema remains v4.

### Verified

- All 64 ProjectRift automation leaf tests pass, including the 10-test GAS group and all new damage/status/enemy coverage.
- Default Quick, Win64 Development BuildCookRun, and `ProjectRift.LocalSmoke` pass.
- Win64 Shipping BuildCookRun passes, excludes test modules, and passes the bounded NullRHI launch check.
- The shared `UnrealEditor.modules` remains byte-for-byte unchanged, and no ProjectRift content asset was modified.

## [0.5.6] - 2026-07-14

### Added

- Added the Development-only `ProjectRiftSmokeTests` plugin with a staged `UPRLocalSmokeGauntletController` and the project-level `ProjectRift.LocalSmoke` AutomationTool node.
- Added `ProjectRift.Smoke.Content`, `ProfileIsolation`, and `GauntletContract` coverage for maps, Primary Assets, isolated profiles, controller transitions, timeout constants, and Shipping exclusions.
- Added a PowerShell 5.1 local pipeline with Quick, Full, and Build modes, fixed exit codes, exact ProjectA editor ownership checks, structured run summaries, and dependency-free self-tests.
- Added transactional Development/Shipping package candidates and safe latest-package promotion beneath `Saved/StagedBuilds/Local`.
- Added a shared UE module-manifest guard that validates `UnrealEditor.modules`, stores a hash-verified backup inside each ProjectA run, and stops if any ProjectA stage changes it.

### Changed

- Gauntlet smoke runs now fail closed unless they receive a valid run GUID and an absolute engine-owned `-userdir`; profile persistence is fixed to `Saved/Automation/Gauntlet/<RunId>/Profiles` and never scans the normal player profile root.
- Full local verification now performs the real authoritative lobby, ServerTravel, deterministic loot, objective, extraction, settlement, safe-save, acknowledgement, and lobby-return loop without input simulation.
- The project descriptor excludes both the custom Smoke plugin and engine Gauntlet plugin from Shipping configurations.
- Updated the displayed project version to `0.5.6`; the player save schema remains v4.

### Verified

- PowerShell 5.1 self-test passes all 31 assertions, and default Quick completes the Editor Development build plus all three Smoke tests.
- All 62 ProjectRift automation leaf tests complete with 0 failures (46 clean successes and 16 existing warning successes).
- Win64 Development BuildCookRun and `ProjectRift.LocalSmoke` pass with `PROJECTRIFT_SMOKE_RESULT=PASS`.
- Win64 Shipping BuildCookRun passes, contains no Gauntlet or `ProjectRiftSmokeTests` entry, and passes a bounded NullRHI launch probe.

## [0.5.5] - 2026-07-13

### Added

- Added a local-only diagnostics subsystem with a configurable 500-entry structured event ring buffer, runtime snapshots, filtering, health summaries, and validated UTF-8 JSON export under `Saved/Diagnostics`.
- Added a unified native diagnostics HUD with Overview, Player, Team & Rift, Events, and Tools tabs; F1 toggles it and Escape closes it outside Shipping builds.
- Added local developer actions for profile lifecycle, checkpoint saving, legacy-v1 samples, repair acceptance preparation, test loot, ready/start flow, and isolated fault injection.
- Added dedicated Gameplay, Network, Save, Assets, UI, and Diagnostics log categories while retaining the existing Flow category.

### Changed

- Replaced automatically opened, overlapping profile/ready/GAS debug overlays with the centered unified HUD while preserving the legacy classes for compatibility.
- Main menu sessions without an active profile open directly to the Tools tab; Inventory and Ship Repair remain mutually exclusive with diagnostics.
- Corrupt-primary and fail-next-save actions are now rejected unless the save root is explicitly isolated beneath ProjectA `Saved/Automation`.
- Updated the displayed project version to `0.5.5`; the player save schema remains v4.

### Verified

- ProjectAEditor Win64 Development, ProjectA Win64 Development, and ProjectA Win64 Shipping targets build successfully with Unreal Engine 5.8.
- All 59 ProjectRift automation leaf tests pass after the final DPI-aware diagnostics layout fix.
- ProjectA-only 8001 MCP verified two-player PIE, isolated profile roots, removal of the scattered debug overlays, and the centered main-menu Tools panel.

## [0.5.4] - 2026-07-13

### Added

- Added the `ProjectRiftShipRepair` Primary DataAsset contract and the staged `Repair.Ship.Engine.Stage1` engine repair for `EnergyCrystal ×10` after `Story.Prologue.RiftTestHold`.
- Added authoritative lobby repair requests, owner-only repair receipts, client-owned transactional persistence, acknowledgement state, and five-second retry handling.
- Added the v4 processed-repair transaction ledger with 128-entry trimming and real v3-to-v4 migration while preserving the v1-to-v4 chain.
- Added a formal native ship repair panel available in Development and Shipping, with centered R/Esc lifecycle, inventory mutual exclusion, and a non-Shipping acceptance helper.

### Changed

- Multiplayer profile projections and seamless PlayerState travel now preserve personal ship module state.
- Repair completion deducts the exact absolute wallet cost, upgrades modules monotonically, and unlocks chapters only when both story and repair-resource gates are satisfied.
- Pending repair persistence prevents readying or another repair until the owning client saves successfully; the server never reads or writes a remote profile file.
- Profile migration diagnostics now display the actual latest save version, and the displayed project version is `0.5.4`.

### Verified

- ProjectAEditor and ProjectA Win64 Development targets build successfully with Unreal Engine 5.8.
- `ProjectRift.ShipRepair`, save/profile migration, multiplayer projection, progression, and existing ProjectRift regression coverage pass.
- ProjectA-only 8001 MCP created and read back the engine repair asset; ProjectA editor validation uses no other MCP endpoint.

## [0.5.3] - 2026-07-13

### Added

- Added `ProjectRiftMission` Primary DataAssets and the starter `Mission.Rift.Test.Hold` progression contract for `L_Rift_Test`.
- Added validated per-client multiplayer profile projections, unique GUID binding, host-led mission eligibility, and replicated team mission readiness.
- Added authoritative personal settlement receipts, client-owned transactional persistence, acknowledgement/timeout handling, and lobby retry status.
- Added the v3 processed-settlement ledger with 128-entry trimming and real v2-to-v3 migration.
- Added isolated multi-client PIE profile roots under ProjectA `Saved/Automation` for development verification.

### Changed

- Rift travel now carries and validates the authoritative MissionId, while seamless PlayerState replacement preserves profile binding, story projection, baselines, inventory, resources, and role.
- Successful extracted players retain authoritative results and can advance eligible story nodes; non-extracted and failed players lose new non-resource loot while preserving real consumption, with failure resources continuing to use the configured retention policy.
- Settlement return now waits at least four seconds and then for all connected client acknowledgements, with an eight-second maximum barrier.
- Multiplayer-bound profiles reject profile switching, deletion of the bound profile, external snapshot replacement, and checkpoint capture outside the settlement transaction.
- Updated the displayed project version and development profile panel title to `0.5.3`.

### Verified

- ProjectAEditor and ProjectA Win64 Development targets build successfully with Unreal Engine 5.8.
- All 47 ProjectRift automation leaf tests pass.
- ProjectA-only 8001 MCP validates the mission asset, isolated two-player PIE profile binding, replicated team mission state, and non-overlapping development HUD layout.

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
