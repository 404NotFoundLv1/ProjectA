# ProjectRift v0.8.0 Mission Contracts and Rift Selection Implementation Plan

> **For agentic workers:** Execute inline with `superpowers:executing-plans`; this project explicitly requires one primary agent and the existing `main` workspace.

**Goal:** Make the ship lobby select, replicate, travel with, validate and record deterministic data-driven mission contracts.

**Architecture:** Extend the existing `ProjectRiftMission` asset with authored contract data; build an immutable scalar-only mission definition from a server-owned seed. Replicate that definition from `APRGameState`, serialize only the compact travel context through the existing URL, reconstruct it in `APRRiftGameMode`, and record its version in settlement receipts.

**Tech Stack:** Unreal Engine 5.8 C++, Gameplay Framework replication, Primary Assets, UMG/native Slate fallback, Automation Framework, ProjectRift local build pipeline.

## Global Constraints

- Work only in `E:\MyWork\ProjectA`; no other project, process, port or asset service is in scope.
- Work directly on existing `main`; do not create branches, worktrees, commits, or other Git writes.
- Use `apply_patch` for edits and one primary agent for every write.
- Keep `UPRMissionProgressionDataAsset`, `ProjectRiftMission`, `MissionId`, current lobby/session flow and profile schema v9 compatible.
- Travel URL contains only ContractId, ContractVersion and Seed; no UObject paths or generated reward instances.
- Formal WBP mission-table layout and three production mission assets remain manual editor work.
- Project version becomes 0.8.0; no save-format upgrade.

---

### Task 1: Contract value types and Primary Asset definition

**Files:**
- Create: `Source/ProjectA/Progression/PRMissionContractTypes.h`
- Modify: `Source/ProjectA/Progression/PRMissionProgressionDataAsset.h`
- Modify: `Source/ProjectA/Progression/PRMissionProgressionDataAsset.cpp`
- Modify: `Source/ProjectA/Core/PRAssetManager.h`
- Modify: `Source/ProjectA/Core/PRAssetManager.cpp`
- Create: `Source/ProjectA/Tests/PRMissionContractTest.cpp`

**Interfaces:**
- Produces `FPRMissionRewardPreview`, `FPRMissionContract`, `FPRMissionDefinition`, `FPRMissionTravelContext`.
- Produces `UPRMissionProgressionDataAsset::ValidateMissionContract`, `BuildMissionDefinition(int32)` and AssetManager mission catalog loading.

- [ ] **Step 1: Write RED contract tests**

Add reflection assertions for all four structs and exact fields. Build a transient mission with `ContractVersion=1`, `BiomeId=Biome.Test`, `DifficultyId=Difficulty.Standard`, sorted/unsorted modifier input and a valid reward range. Assert validation succeeds, equal Seed produces identical definition/signature, changed Seed/modifier changes signature, zero Seed/duplicate modifier/invalid reward range fail.

- [ ] **Step 2: Run RED verification**

Run: `powershell -ExecutionPolicy Bypass -File Scripts/ProjectRift/Invoke-ProjectRiftLocal.ps1 -Mode Quick -AutomationFilter ProjectRift.MissionContracts -NoReopenEditor`

Expected: build/test failure because the reflected types and methods do not exist.

- [ ] **Step 3: Implement scalar-only types and validation**

Define the public structs with `BlueprintType`/`BlueprintReadOnly` fields. `FPRMissionTravelContext::ToTravelOptions()` emits `ContractId`, `ContractVersion` and `Seed`; parsing accepts legacy `MissionId` only if ContractId is absent. Validate all identifiers, version, ranges and duplicate modifiers. Build a normalized definition and calculate a versioned CRC signature from sorted modifiers and seed.

- [ ] **Step 4: Extend the existing mission asset and AssetManager catalog**

Add authored `FPRMissionContract Contract` to `UPRMissionProgressionDataAsset`; make `IsContractValid` require both legacy progression and contract validity. Add `BuildMissionDefinition`. Add an AssetManager `LoadMissionCatalog` helper returning valid registered `ProjectRiftMission` assets without changing the PrimaryAsset type.

- [ ] **Step 5: Run GREEN verification**

Run the focused command from Step 2. Expected: `ProjectRift.MissionContracts` has zero failures.

### Task 2: Authoritative lobby selection and replicated definition

**Files:**
- Modify: `Source/ProjectA/Core/PRGameState.h`
- Modify: `Source/ProjectA/Core/PRGameState.cpp`
- Modify: `Source/ProjectA/Core/PRShipLobbyGameMode.h`
- Modify: `Source/ProjectA/Core/PRShipLobbyGameMode.cpp`
- Modify: `Source/ProjectA/Player/PRPlayerController.h`
- Modify: `Source/ProjectA/Player/PRPlayerController.cpp`
- Modify: `Source/ProjectA/Tests/PRLobbyServerTravelTest.cpp`
- Test: `Source/ProjectA/Tests/PRMissionContractTest.cpp`

**Interfaces:**
- Produces `APRGameState::GetSelectedTeamMissionDefinition` and atomic `SetTeamMissionDefinition`.
- Produces `APRShipLobbyGameMode::SelectMissionContract` and `APRPlayerController::ServerSelectMissionContract`.

- [ ] **Step 1: Write RED runtime tests**

Create a test lobby world with `APRShipLobbyGameMode`, a bound host profile and a locked profile story. Assert only an authority host can select, locked contract selection leaves the previous definition intact, successful selection uses a non-zero server seed, updates GameState's whole definition and resets every player Ready flag.

- [ ] **Step 2: Run RED verification**

Run: `powershell -ExecutionPolicy Bypass -File Scripts/ProjectRift/Invoke-ProjectRiftLocal.ps1 -Mode Quick -AutomationFilter ProjectRift.MissionContracts.Lobby -NoReopenEditor`

Expected: failure because selection APIs and replicated definition do not exist.

- [ ] **Step 3: Implement replication and server-only selection**

Replicate `FPRMissionDefinition` on `APRGameState`; preserve `GetSelectedTeamMissionId()` as `Definition.ContractId`. Add a host-only lobby method that loads the requested asset, checks contract and host story eligibility, creates a non-zero server seed, builds a definition, sets it atomically and clears readiness. Add a reliable controller RPC that forwards only ContractId. Rejection returns an owner diagnostic without state/seed/readiness mutation.

- [ ] **Step 4: Route start and default selection through the definition**

Use the same validation path for lobby default setup and start eligibility. `BuildRiftTravelURL` serializes the selected definition's compact context instead of raw MissionId. Keep legacy URL output behavior available if no definition is present.

- [ ] **Step 5: Run GREEN verification**

Run the focused command from Step 2. Expected: all lobby selection cases pass.

### Task 3: Travel reconstruction, Rift state and versioned settlement

**Files:**
- Modify: `Source/ProjectA/Core/PRRiftGameMode.h`
- Modify: `Source/ProjectA/Core/PRRiftGameMode.cpp`
- Modify: `Source/ProjectA/Core/PRRiftGameState.h`
- Modify: `Source/ProjectA/Core/PRRiftGameState.cpp`
- Modify: `Source/ProjectA/Core/PRRiftSettlementTypes.h`
- Modify: `Source/ProjectA/Multiplayer/PRMultiplayerProfileTypes.h`
- Modify: `Source/ProjectA/Multiplayer/PRMultiplayerProfileTypes.cpp`
- Modify: `Source/ProjectA/Persistence/PRSaveSubsystem.cpp`
- Test: `Source/ProjectA/Tests/PRRiftGameModeStateTest.cpp`
- Test: `Source/ProjectA/Tests/PRRewardBudgetTest.cpp`

**Interfaces:**
- Produces `APRRiftGameMode::GetMissionDefinition` and compact-context parser validation.
- Produces replicated Rift definition and ContractVersion in settlement/receipt data.

- [ ] **Step 1: Write RED travel/result tests**

Assert URL round-trip reproduces ContractId/version/seed and rebuilds the same deterministic signature, malformed zero Seed and version mismatch reject before start, legacy MissionId URL remains valid, `CurrentRunSeed` equals the authorized travel seed, and generated settlement/personal receipt record ContractVersion.

- [ ] **Step 2: Run RED verification**

Run: `powershell -ExecutionPolicy Bypass -File Scripts/ProjectRift/Invoke-ProjectRiftLocal.ps1 -Mode Quick -AutomationFilter ProjectRift.MissionContracts.Travel -NoReopenEditor`

Expected: failure because context/version fields are absent.

- [ ] **Step 3: Implement compact-context parsing and authoritative reconstruction**

In `InitGame`, parse `FPRMissionTravelContext`, load the registered mission, require matching contract version, rebuild the definition and reject invalid/tampered context. Preserve legacy `MissionId` parsing only where the new ContractId option is absent. Store the reconstructed definition in Rift GameState and use its Seed as `CurrentRunSeed`.

- [ ] **Step 4: Record and validate contract version**

Add ContractVersion to settlement data and personal settlement receipt; fill it from the resolved definition. Before receipt application, load the mission and reject a version mismatch. Do not alter profile schema or persist a lobby selection.

- [ ] **Step 5: Run GREEN verification**

Run the focused command from Step 2. Expected: travel, determinism, settlement and compatibility tests pass.

### Task 4: Native mission-selection fallback presentation

**Files:**
- Create: `Source/ProjectA/UI/PRMissionSelectionWidget.h`
- Create: `Source/ProjectA/UI/PRMissionSelectionWidget.cpp`
- Modify: `Source/ProjectA/Player/PRPlayerController.h`
- Modify: `Source/ProjectA/Player/PRPlayerController.cpp`
- Create: `Source/ProjectA/Tests/PRMissionSelectionUITest.cpp`

**Interfaces:**
- Produces Blueprint-facing `FPRMissionSelectionRow` and `UPRMissionSelectionWidget` catalog/selection/focus APIs.
- Consumes replicated `FPRMissionDefinition` and controller ContractId request API.

- [ ] **Step 1: Write RED UI contract tests**

Assert the widget exposes catalog rows with display/lock/biome/difficulty/modifiers/reward preview, selection state, next/previous focus and select request. Assert all requested selection is forwarded as ContractId, not a client-provided Seed/definition.

- [ ] **Step 2: Run RED verification**

Run: `powershell -ExecutionPolicy Bypass -File Scripts/ProjectRift/Invoke-ProjectRiftLocal.ps1 -Mode Quick -AutomationFilter ProjectRift.MissionContracts.UI -NoReopenEditor`

Expected: build/test failure because widget and row contract are absent.

- [ ] **Step 3: Implement text-first native fallback**

Build rows from AssetManager mission catalog and local profile eligibility; bind display to replicated GameState definition; expose controller focus helpers and host-only select action. Do not save a WBP asset or generate reward instances. Missing optional assets render safe text.

- [ ] **Step 4: Bind controller lifecycle**

Create/destroy the widget with existing lobby-panel lifecycle without conflicting with inventory, role loadout, repair or debug widgets. Expose a Blueprint callable toggle but do not add a new mandatory input binding.

- [ ] **Step 5: Run GREEN verification**

Run the focused command from Step 2. Expected: native fallback contract passes with no WBP asset dependency.

### Task 5: Versioning, regression tests and evidence

**Files:**
- Modify: `Config/DefaultGame.ini`
- Modify: `Scripts/ProjectRift/Invoke-ProjectRiftLocal.ps1`
- Modify: existing version assertion tests and debug titles found by `rg '0.7.6' Source/ProjectA`
- Modify: `CHANGELOG.md`
- Create: `docs/projectrift/v0.8.0-test-record.md`

- [ ] **Step 1: Add RED version/compatibility assertions**

Update test expectations to 0.8.0 while asserting the profile save version remains 9 and legacy MissionId travel remains valid.

- [ ] **Step 2: Apply version/docs changes**

Set project/version-script/debug text to 0.8.0, document contract selection and manual editor assets, and record exact focused/full/package evidence in the test record.

- [ ] **Step 3: Run targeted and full verification**

Run focused `ProjectRift.MissionContracts`, Quick/Smoke, full `ProjectRift`, Development package plus `ProjectRift.LocalSmoke`, Shipping package, exclusion and NullRHI checks. Confirm the shared `UnrealEditor.modules` SHA-256 is `BBE466E4330F3FDDEA1120A9070A7402F5A9655404223F55C9E0D1D322568747` and length is 32290.

- [ ] **Step 4: Record manual acceptance**

Document 8001 two-player PIE checks: shared definition/reward preview, non-host/locked RPC rejection, same-seed reproduction, return-lobby reselection, and no map/Blueprint asset edits.
