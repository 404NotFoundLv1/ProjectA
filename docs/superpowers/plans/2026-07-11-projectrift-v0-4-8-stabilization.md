# ProjectRift v0.4.8 Stabilization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Do not dispatch subagents. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert the existing v0.4.7 lobby-to-rift-to-lobby vertical slice into a repeatable, diagnosable, idempotent, Development-packaged v0.4.8 build without adding new gameplay systems.

**Architecture:** Keep the existing `APRRiftGameMode`, `APRRiftGameState`, `APRSpawnManager`, objective, extraction, inventory, settlement, and ServerTravel architecture. Add per-run identifiers and narrow idempotency guards at the settlement boundary, explicit teardown cleanup at existing timer/delegate owners, focused automation coverage, deterministic packaging configuration, and version evidence documents.

**Tech Stack:** Unreal Engine 5.8 C++, Gameplay Ability System, replicated Game Framework classes, UMG/Slate, UE Automation Tests, Unreal MCP editor toolsets, UBT, UAT BuildCookRun, PowerShell.

## Global Constraints

- Start only after the user explicitly says `开始 v0.4.8` and the ProjectA MCP isolation plan has passed on port `8001`.
- Operate only in `E:\MyWork\ProjectA`; never inspect or interact with another project.
- Work only on `main`; do not stage, commit, tag, push, create branches, stash, reset, restore, checkout, or create a PR.
- Preserve the v0.4.7 public interfaces, Blueprint references, module name `ProjectA`, and reflected class names.
- Do not add roles, enemies, equipment, crafting, chapters, formal UI, Steam functionality, or v0.5.x systems.
- Keep the current ServerTravel/seamless-travel behavior unless a reproducible packaged-build failure proves a narrow fix is required.
- Server remains authoritative for mission, kills, loot, extraction, settlement, and persistent ship resources.
- All code and asset changes stop after v0.4.8 verification for user acceptance and user-managed commit.

---

## File Map

- Modify `Source/ProjectA/ProjectA.h` and `.cpp`: add the focused `LogProjectRiftFlow` category.
- Modify `Source/ProjectA/Core/PRRiftSettlementTypes.h`: add `MissionId`, `RunId`, and `SettlementId` to replicated settlement data.
- Modify `Source/ProjectA/Core/PRRiftGameMode.h` and `.cpp`: create run identifiers, suppress duplicate settlement/kill events, add phase logs, and clear travel state during teardown.
- Modify `Source/ProjectA/Core/PRSpawnManager.h` and `.cpp`: explicit timer and spawned-actor delegate cleanup.
- Modify `Source/ProjectA/Tests/PRExtractedResourceRulesTest.cpp`: prove settlement/resource idempotency.
- Modify `Source/ProjectA/Tests/PRRiftGameModeStateTest.cpp`: prove run identifiers, duplicate kill suppression, and packaging configuration.
- Modify `Source/ProjectA/Tests/PRRiftSpawnManagerTest.cpp`: prove explicit stop clears active timer state.
- Modify `Config/DefaultGame.ini`: ProjectRift name/version and deterministic maps-to-cook.
- Inspect/save via MCP: `Content/ProjectRift/Maps/L_ShipLobby.umap`, `L_Rift_Test.umap`, `L_MainMenu.umap`, and directly referenced ProjectRift assets.
- Create/update `CHANGELOG.md`, `docs/projectrift/v0.4.7-baseline.md`, `docs/projectrift/known-issues.md`, and `docs/projectrift/v0.4.8-test-record.md`.

### Task 1: Capture the v0.4.7 baseline and add failing stabilization assertions

**Files:**
- Modify: `Source/ProjectA/Tests/PRExtractedResourceRulesTest.cpp`
- Modify: `Source/ProjectA/Tests/PRRiftGameModeStateTest.cpp`
- Modify: `Source/ProjectA/Tests/PRRiftSpawnManagerTest.cpp`

**Interfaces:**
- Consumes: current v0.4.7 runtime classes and `ProjectRift.*` automation namespace.
- Produces: red tests for identifiers, duplicate event protection, teardown state, and packaging configuration.

- [ ] **Step 1: Record the immutable starting evidence**

Run:

```powershell
git branch --show-current
git rev-parse HEAD
git status --short --untracked-files=all
```

Expected: `main`; record the exact commit in `docs/projectrift/v0.4.7-baseline.md` later. If user changes overlap files in this plan, stop before editing.

- [ ] **Step 2: Extend resource rules assertions before implementation**

After the existing successful settlement assertions in `PRExtractedResourceRulesTest.cpp`, add:

```cpp
const FGuid SuccessfulRunId = SuccessSettlement.RunId;
const FGuid SuccessfulSettlementId = SuccessSettlement.SettlementId;
TestTrue(TEXT("Successful settlement has a run id"), SuccessfulRunId.IsValid());
TestTrue(TEXT("Successful settlement has a settlement id"), SuccessfulSettlementId.IsValid());
TestFalse(TEXT("Successful settlement has a mission id"), SuccessSettlement.MissionId.IsNone());

RiftGameMode->FinalizeRiftSettlement(EPRRiftMissionResult::Success);
const FPRRiftSettlementData DuplicateSuccessSettlement = RiftGameState->GetSettlementData();
TestEqual(TEXT("Duplicate settlement keeps the same run id"), DuplicateSuccessSettlement.RunId, SuccessfulRunId);
TestEqual(TEXT("Duplicate settlement keeps the same settlement id"), DuplicateSuccessSettlement.SettlementId, SuccessfulSettlementId);
TestEqual(TEXT("Duplicate settlement does not grant resources twice"), SuccessPlayer.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), 5);
```

After the failed settlement assertions, add:

```cpp
const int32 EnergyAfterFailure = FailurePlayer.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal"));
const int32 ChipAfterFailure = FailurePlayer.PlayerState->GetShipResourceCount(TEXT("CommonChip"));
TestFalse(TEXT("Duplicate failure request is rejected"), RiftGameMode->RequestRiftFailure());
TestEqual(TEXT("Duplicate failure does not grant EnergyCrystal twice"), FailurePlayer.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), EnergyAfterFailure);
TestEqual(TEXT("Duplicate failure does not grant CommonChip twice"), FailurePlayer.PlayerState->GetShipResourceCount(TEXT("CommonChip")), ChipAfterFailure);
```

- [ ] **Step 3: Add run and kill-idempotency assertions**

In `PRRiftGameModeStateTest.cpp`, assert reflected getters exist:

```cpp
TestNotNull(TEXT("APRRiftGameMode exposes GetCurrentRunId"), RiftGameModeClass->FindFunctionByName(TEXT("GetCurrentRunId")));
TestNotNull(TEXT("APRRiftGameMode exposes GetCurrentSettlementId"), RiftGameModeClass->FindFunctionByName(TEXT("GetCurrentSettlementId")));
TestNotNull(TEXT("APRRiftGameMode exposes GetMissionId"), RiftGameModeClass->FindFunctionByName(TEXT("GetMissionId")));
```

In the test world, capture the current run id, restart, and assert rotation:

```cpp
const FGuid FirstRunId = StabilityGameMode->GetCurrentRunId();
TestTrue(TEXT("BeginPlay creates a valid run id"), FirstRunId.IsValid());
TestTrue(TEXT("Mission owns a valid settlement id"), StabilityGameMode->GetCurrentSettlementId().IsValid());
TestFalse(TEXT("Mission id is configured"), StabilityGameMode->GetMissionId().IsNone());
TestTrue(TEXT("Restarting the mission succeeds"), StabilityGameMode->StartRiftMission());
TestNotEqual(TEXT("Restarting rotates the run id"), StabilityGameMode->GetCurrentRunId(), FirstRunId);
```

Add a spawned `APREnemyCharacter`, call `RegisterEnemyKilled` twice, and assert the second call is false and the count remains one.

- [ ] **Step 4: Extend existing spawn and configuration coverage**

In `PRRiftSpawnManagerTest.cpp`, assert the new timer diagnostic exists and changes with start/stop:

```cpp
TestNotNull(TEXT("APRSpawnManager exposes IsWaveTimerActive"), SpawnManagerClass->FindFunctionByName(TEXT("IsWaveTimerActive")));

TestTrue(TEXT("Wave timer runs while spawning is active"), SpawnManager->IsWaveTimerActive());
RiftGameMode->StopSpawnManagers();
TestFalse(TEXT("Wave timer is cleared when spawning stops"), SpawnManager->IsWaveTimerActive());
```

In `PRRiftGameModeStateTest.cpp`, add `#include "GeneralProjectSettings.h"` and `#include "Misc/ConfigCacheIni.h"`, then add:

```cpp
const UGameMapsSettings* Maps = GetDefault<UGameMapsSettings>();
const UGeneralProjectSettings* Project = GetDefault<UGeneralProjectSettings>();
TestEqual(TEXT("Game default map is ship lobby"), Maps->GetGameDefaultMap(), FString(TEXT("/Game/ProjectRift/Maps/L_ShipLobby.L_ShipLobby")));
TestEqual(TEXT("Project name is ProjectRift"), Project->ProjectName, FString(TEXT("ProjectRift")));
TestEqual(TEXT("Project version is v0.4.8"), Project->ProjectVersion, FString(TEXT("0.4.8")));

TArray<FString> MapsToCook;
GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("MapsToCook"), MapsToCook, GGameIni);
const auto ContainsCookMap = [&MapsToCook](const TCHAR* MapPath)
{
	return MapsToCook.ContainsByPredicate([MapPath](const FString& Entry)
	{
		return Entry.Contains(MapPath);
	});
};
TestTrue(TEXT("Cook list contains main menu"), ContainsCookMap(TEXT("/Game/ProjectRift/Maps/L_MainMenu")));
TestTrue(TEXT("Cook list contains ship lobby"), ContainsCookMap(TEXT("/Game/ProjectRift/Maps/L_ShipLobby")));
TestTrue(TEXT("Cook list contains rift test"), ContainsCookMap(TEXT("/Game/ProjectRift/Maps/L_Rift_Test")));
```

- [ ] **Step 5: Build and run the focused tests to prove red state**

Run after saving and closing only ProjectA:

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: compilation fails because the new identifier fields/getters and timer accessor do not exist yet. This is the required red state.

### Task 2: Add run identity, flow logging, and settlement idempotency

**Files:**
- Modify: `Source/ProjectA/ProjectA.h`
- Modify: `Source/ProjectA/ProjectA.cpp`
- Modify: `Source/ProjectA/Core/PRRiftSettlementTypes.h`
- Modify: `Source/ProjectA/Core/PRRiftGameMode.h`
- Modify: `Source/ProjectA/Core/PRRiftGameMode.cpp`

**Interfaces:**
- Produces: `FPRRiftSettlementData::{MissionId,RunId,SettlementId}`; `APRRiftGameMode::{GetCurrentRunId,GetCurrentSettlementId,GetMissionId}`; `LogProjectRiftFlow`.
- Preserves: every existing BlueprintCallable method and return type.

- [ ] **Step 1: Add the focused log category**

Add to `ProjectA.h`:

```cpp
DECLARE_LOG_CATEGORY_EXTERN(LogProjectRiftFlow, Log, All);
```

Add to `ProjectA.cpp`:

```cpp
DEFINE_LOG_CATEGORY(LogProjectRiftFlow)
```

- [ ] **Step 2: Add stable identifiers to settlement data**

At the start of `FPRRiftSettlementData`, add:

```cpp
UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement")
FName MissionId = NAME_None;

UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement")
FGuid RunId;

UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement")
FGuid SettlementId;
```

Keep `IsValid()` result-based for v0.4.8 compatibility.

- [ ] **Step 3: Add GameMode state without changing old public signatures**

Add public BlueprintPure getters to `PRRiftGameMode.h`:

```cpp
UFUNCTION(BlueprintPure, Category = "Rift|Diagnostics")
FGuid GetCurrentRunId() const { return CurrentRunId; }

UFUNCTION(BlueprintPure, Category = "Rift|Diagnostics")
FGuid GetCurrentSettlementId() const { return CurrentSettlementId; }

UFUNCTION(BlueprintPure, Category = "Rift|Diagnostics")
FName GetMissionId() const { return MissionId; }
```

Add private state:

```cpp
UPROPERTY(EditDefaultsOnly, Category = "Rift|Diagnostics")
FName MissionId = FName(TEXT("Mission.Rift.Test.Hold"));

FGuid CurrentRunId;
FGuid CurrentSettlementId;
FGuid FinalizedRunId;
bool bSettlementFinalizationInProgress = false;
TSet<TObjectKey<APREnemyCharacter>> CountedKilledEnemies;

void LogFlowPhase(const TCHAR* Phase, const APlayerState* PlayerState = nullptr) const;
```

- [ ] **Step 4: Initialize and rotate identifiers in `StartRiftMission`**

Before resetting mission state:

```cpp
CurrentRunId = FGuid::NewGuid();
CurrentSettlementId = FGuid::NewGuid();
FinalizedRunId.Invalidate();
bSettlementFinalizationInProgress = false;
CountedKilledEnemies.Reset();
```

Call `LogFlowPhase(TEXT("MissionStarted"));` after successful initialization.

- [ ] **Step 5: Make settlement finalization re-entry safe**

At the start of `FinalizeRiftSettlement`, reject non-authority, invalid IDs, an in-progress finalization, an already-finalized run, or a ready GameState. Use `TGuardValue<bool>` around resource application. Populate all three IDs in `GenerateSettlementData`, then set `FinalizedRunId = CurrentRunId` only after `SetSettlementData` and `SetSettlementReady(true)` succeed.

Before scheduling travel in `RequestReturnToLobbyTravel`, require `RiftGameState && RiftGameState->IsSettlementReady()`. This prevents travel from continuing after a failed settlement creation.

- [ ] **Step 6: Suppress duplicate enemy death notifications**

In `RegisterEnemyKilled`, reject an enemy already present in `CountedKilledEnemies`; otherwise add it before incrementing `KilledEnemyCount`.

- [ ] **Step 7: Add structured transition logs**

Implement:

```cpp
void APRRiftGameMode::LogFlowPhase(const TCHAR* Phase, const APlayerState* PlayerState) const
{
	UE_LOG(LogProjectRiftFlow, Log,
		TEXT("MissionId=%s RunId=%s SettlementId=%s PlayerId=%d Phase=%s NetMode=%d"),
		*MissionId.ToString(),
		*CurrentRunId.ToString(EGuidFormats::DigitsWithHyphens),
		*CurrentSettlementId.ToString(EGuidFormats::DigitsWithHyphens),
		PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE,
		Phase,
		static_cast<int32>(GetNetMode()));
}
```

Call it for objective activation/completion, extraction opened/player extracted, settlement finalized/duplicate ignored, return scheduled/executing/failed, mission failure, and EndPlay.

- [ ] **Step 8: Rebuild and run focused tests**

Expected: identifier and duplicate-settlement/kill assertions pass; timer and packaging assertions remain red until Tasks 3 and 4.

### Task 3: Close timer and spawned-enemy delegate lifecycle gaps

**Files:**
- Modify: `Source/ProjectA/Core/PRRiftGameMode.h/.cpp`
- Modify: `Source/ProjectA/Core/PRSpawnManager.h/.cpp`
- Test: `Source/ProjectA/Tests/PRRiftSpawnManagerTest.cpp`

**Interfaces:**
- Produces: `APRSpawnManager::IsWaveTimerActive()` diagnostic getter and explicit `EndPlay` cleanup.
- Preserves: actor behavior while a mission is active.

- [ ] **Step 1: Add GameMode teardown**

Declare `virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;`. Its implementation must clear `ReturnToLobbyTravelTimerHandle`, call `StopSpawnManagers()`, reset `SpawnManagers`, `ActiveObjective`, extracted/kill sets, log `EndPlay`, then call `Super::EndPlay`.

- [ ] **Step 2: Add SpawnManager timer visibility and cleanup**

Add:

```cpp
virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
bool IsWaveTimerActive() const;
```

Implement `IsWaveTimerActive()` with `GetWorldTimerManager().IsTimerActive(WaveTimerHandle)`. In `StopSpawning`, clear the timer, remove `HandleSpawnedActorDestroyed` from every valid tracked enemy, reset `AliveEnemies`, `SpawnPoints`, and `ActiveObjective`. `EndPlay` calls `StopSpawning()` before `Super::EndPlay`.

- [ ] **Step 3: Audit self-owned component and UI callbacks without speculative edits**

Confirm `APRRiftObjectiveActor` and `APRExtractionZone` bind only their own components back to themselves, so actor/component destruction removes both sides together. Confirm `APRPlayerController::EndPlay` already clears `LobbyReadyDebugTimerHandle`, removes inventory delegates, and destroys GAS, inventory, and settlement widgets. Record this evidence in the v0.4.8 test record; change these classes only if a reproducible stale callback appears during Task 6.

- [ ] **Step 4: Rebuild and run lifecycle plus existing Rift tests**

Run `ProjectRift.Rift` automation. Expected: lifecycle cleanup, settlement, resource, objective, spawn, extraction, and GameMode tests all pass.

### Task 4: Freeze Development packaging configuration

**Files:**
- Modify: `Config/DefaultGame.ini`
- Test: `Source/ProjectA/Tests/PRRiftGameModeStateTest.cpp`

**Interfaces:**
- Produces: version `0.4.8` and explicit cook roots for the three existing ProjectRift maps.

- [ ] **Step 1: Update project identity and version**

Use:

```ini
[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=3A2023EA45DDCD89BE7179A9A06F381E
ProjectName=ProjectRift
ProjectVersion=0.4.8
```

- [ ] **Step 2: Add the deterministic map cook list**

Append:

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
BuildConfiguration=PPBC_Development
+MapsToCook=(FilePath="/Game/ProjectRift/Maps/L_MainMenu")
+MapsToCook=(FilePath="/Game/ProjectRift/Maps/L_ShipLobby")
+MapsToCook=(FilePath="/Game/ProjectRift/Maps/L_Rift_Test")
```

- [ ] **Step 3: Build and run `ProjectRift.Rift.GameModeState`**

Expected: all map, project name, and project version assertions pass.

### Task 5: Validate ProjectRift assets through the isolated Unreal MCP

**Files:**
- Inspect/save only if dirty and valid: `Content/ProjectRift/Maps/L_MainMenu.umap`, `L_ShipLobby.umap`, `L_Rift_Test.umap`, and their directly referenced ProjectRift Blueprints/DataAssets.

**Interfaces:**
- Consumes: MCP on `127.0.0.1:8001` only.
- Produces: saved, compile-clean assets with no unresolved ProjectRift redirectors.

- [ ] **Step 1: Call `list_toolsets`, then describe only AssetTools, BlueprintTools, ObjectTools, and SceneTools**

Expected: tool schemas come from ProjectA. If the call fails, stop; do not probe another endpoint.

- [ ] **Step 2: Inspect the three maps and critical placed actors**

Confirm `L_Rift_Test` contains its hold objective, spawn manager/spawn points, extraction zone, NavMeshBoundsVolume, and the configured Rift GameMode; confirm `L_ShipLobby` uses the lobby GameMode. Confirm the fixed test set references `BP_PREnemyCharacter`, `DA_TestLootTable`, one hold objective, and one extraction zone without creating replacement assets.

- [ ] **Step 3: Compile affected Blueprints and save only validated dirty assets**

Expected: zero Blueprint compile errors. Record every saved `.uasset`/`.umap` in the test record.

- [ ] **Step 4: Fix ProjectRift redirectors only after listing their referencers**

Expected: no deletion or rename of live assets; no operation outside `/Game/ProjectRift`.

### Task 6: Run the full v0.4.8 verification matrix

**Files:**
- Output only under ignored `Saved/Automation`, `Saved/StagedBuilds`, and `Saved/Logs`.

**Interfaces:**
- Produces: fresh build, automation, cook, package, and user acceptance evidence.

- [ ] **Step 1: Full editor build with ProjectA closed**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: exit code `0`.

- [ ] **Step 2: Run all ProjectRift automation tests headlessly**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests ProjectRift" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\v0.4.8' -log
```

Expected: process exit `0`, zero failed ProjectRift tests, report exported.

- [ ] **Step 3: Build, cook, stage, package, and archive Windows Development**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun -project='E:\MyWork\ProjectA\ProjectA.uproject' -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory='E:\MyWork\ProjectA\Saved\StagedBuilds\v0.4.8' -utf8output
```

Expected: `AutomationTool exiting with ExitCode=0` and a launchable packaged executable.

- [ ] **Step 4: Codex-run single-player packaged smoke**

Launch the packaged build, traverse lobby -> rift -> objective -> extraction -> settlement -> lobby once, and capture ProjectA logs showing one RunId and one SettlementId with no duplicate grant.

- [ ] **Step 5: Prepare user acceptance matrix**

User executes five consecutive single-player loops and three consecutive two-player listen-server loops. Each run checks synchronized mission state, loot, extraction, settlement, return state, no duplicate rewards, no residual enemies/timers/UI, and no client authority escalation.

### Task 7: Write release evidence and stop

**Files:**
- Create/update: `CHANGELOG.md`
- Create: `docs/projectrift/v0.4.7-baseline.md`
- Create: `docs/projectrift/known-issues.md`
- Create: `docs/projectrift/v0.4.8-test-record.md`

**Interfaces:**
- Consumes: exact outputs from Tasks 1-6.
- Produces: user-reviewable release evidence; no Git commit.

- [ ] **Step 1: Record the baseline**

Document the exact starting commit, maps, real classes, existing save/settlement behavior, and preserved v0.4.7 interfaces.

- [ ] **Step 2: Update `CHANGELOG.md` under `[0.4.8] - 2026-07-11`**

List only implemented stabilization, diagnostics, tests, packaging, and documentation changes.

- [ ] **Step 3: Write known issues with severity and reproduction evidence**

If none remain, state `No known Blocker or Critical issues after the recorded verification matrix.` Do not invent issues or leave blank sections.

- [ ] **Step 4: Write the test record**

Include exact commands, timestamps, build exit codes, automation pass/fail counts, package location, changed assets, log evidence, and the pending five-run/three-run user acceptance checklist.

- [ ] **Step 5: Verify scope and leave everything uncommitted**

Run:

```powershell
git diff --check
git status --short --untracked-files=all
git diff --stat
```

Expected: only v0.4.8 and approved workflow files are changed; no staged files; no unrelated refactor.

- [ ] **Step 6: Deliver and stop**

Report modified files/assets, implementation summary, build/test evidence, MCP verification, manual single/2-player acceptance steps, known risks, suggested user-managed tags `baseline-v0.4.7` and `milestone-v0.4.8-vertical-slice`, and suggested commit message `v0.4.8 Stabilize the existing v0.4.7 gameplay loop`. End with: `已停止，未开始下一版本。`
