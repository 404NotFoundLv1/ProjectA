# ProjectRift v0.5.1 GameplayTag Contract and Project Tuning Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. ProjectA policy forbids subagent delegation and all Git write operations for this delivery.

**Goal:** Replace ProjectRift's string/INI GameplayTag declarations with one native C++ contract and move the approved Vertical Slice mission, objective, spawn, and extraction tuning values into one project-level `UDeveloperSettings` source.

**Architecture:** `ProjectRiftGameplayTags` owns all 15 native tags and production consumers reference those constants directly. `UPRProjectSettings` is the unique config-backed tuning source; runtime actors read its class default object and apply defensive clamps without local Blueprint/Actor overrides.

**Tech Stack:** Unreal Engine 5.8 C++, Native GameplayTags, `UDeveloperSettings`, Config system, Automation Framework, ProjectA Unreal MCP on port 8001, UAT BuildCookRun.

## Global Constraints

- Operate only in `E:\MyWork\ProjectA` and on processes unambiguously owned by `ProjectA.uproject`.
- Use only ProjectA Unreal MCP `http://127.0.0.1:8001/mcp`; never probe or use port 8000.
- Work directly on existing `main`; do not create or switch branches.
- Do not run `git add`, `git commit`, `git tag`, `git push`, stash, reset, restore, checkout, merge, or history-rewrite commands.
- Implement only v0.5.1; do not begin any later roadmap version.
- Do not rename the module, reflected classes, maps, assets, Blueprints, or existing interfaces.
- Preserve all pre-existing user changes and stop if they overlap this version.
- Use RED-to-GREEN for each production behavior and preserve all existing ProjectRift tests.
- The user performs acceptance, staging, commit, tag, and push.

---

## File Map

- Create `Source/ProjectA/Core/PRGameplayTags.h` and `.cpp`: native declarations/definitions for all 15 tags.
- Create `Source/ProjectA/Settings/PRProjectSettings.h` and `.cpp`: the unique project-level tuning source.
- Create `Source/ProjectA/Tests/PRGameplayContractTest.cpp`: native-tag and config-source contract coverage.
- Create `Source/ProjectA/Tests/PRProjectSettingsTest.cpp`: settings metadata, defaults, clamps, and config coverage.
- Create `Source/ProjectA/Tests/PRProjectSettingsTestUtils.h`: scoped in-memory settings override/restore for runtime tests.
- Modify the six production tag consumers under `Abilities`, `Characters`, and `Enemies`.
- Modify `PRRiftGameMode`, `PRRiftObjectiveActor`, `PRHoldObjectiveActor`, `PRSpawnManager`, and `PRExtractionZone` to consume settings.
- Modify the affected runtime automation tests to configure the settings CDO rather than removed Actor properties.
- Modify `ProjectA.Build.cs`, `Config/DefaultGame.ini`, `CHANGELOG.md`, and `docs/projectrift/known-issues.md`.
- Delete `Config/Tags/ProjectRiftGameplayTags.ini` after native registration is green.
- Create `docs/projectrift/v0.5.1-test-record.md` for delivery evidence.

---

### Task 1: Capture the v0.5.0 baseline and add the failing native GameplayTag contract

**Files:**
- Create: `Source/ProjectA/Tests/PRGameplayContractTest.cpp`
- Read: `Config/Tags/ProjectRiftGameplayTags.ini`
- Read: `Config/DefaultGame.ini`

- [ ] **Step 1: Confirm scope, branch, and worktree**

Run read-only checks:

```powershell
git status --short
git branch --show-current
git rev-parse HEAD
```

Expected: branch `main`, base commit `5228850`, and only the approved v0.5.1 spec/plan are untracked. If unrelated changes overlap any file in this plan, stop and report them.

- [ ] **Step 2: Close only ProjectA if a full build requires it**

Identify only the editor process whose command line contains `E:\MyWork\ProjectA\ProjectA.uproject`. Save only intended ProjectA changes and close only that editor. Never inspect or act on another Unreal process.

- [ ] **Step 3: Run the clean baseline build and all ProjectRift tests**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests ProjectRift" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\baseline-v0.5.0-for-v0.5.1'
```

Expected: build exit 0 and every current ProjectRift leaf test succeeds.

- [ ] **Step 4: Write the failing native-tag contract test**

Create `PRGameplayContractTest.cpp`, include the not-yet-existing `Core/PRGameplayTags.h`, and assert that these constants are valid and have their exact names:

```text
Input.Ability.Primary
Input.Ability.Dodge
Input.Ability.Skill.Q
Input.Ability.Skill.E
Input.Ability.Skill.R
Ability.Role.Assault
Ability.Role.Engineer
Ability.Role.Medic
Data.Damage
State.Dead
State.Downed
State.Stunned
Cooldown.Skill.Q
Cooldown.Skill.E
Cooldown.Skill.R
```

For each constant, also assert equality with `UGameplayTagsManager::Get().RequestGameplayTag(ExpectedName, false)`. Assert `ProjectRiftGameplayTags.ini` is absent, the old staging allow-list line is absent, and project version is `0.5.1`; those config assertions remain red until Task 2.

- [ ] **Step 5: Build and confirm RED**

Run the Editor build. Expected: compilation fails because `Core/PRGameplayTags.h` does not exist. The failure must reference the planned header, not an unrelated baseline issue.

---

### Task 2: Implement the native GameplayTag contract and migrate every production consumer

**Files:**
- Create: `Source/ProjectA/Core/PRGameplayTags.h`
- Create: `Source/ProjectA/Core/PRGameplayTags.cpp`
- Modify: `Source/ProjectA/Abilities/GA_AssaultBlast.cpp`
- Modify: `Source/ProjectA/Abilities/GA_AssaultGameplayAbility.cpp`
- Modify: `Source/ProjectA/Abilities/GA_PrimaryAttack.cpp`
- Modify: `Source/ProjectA/Abilities/PRDamageGameplayEffect.cpp`
- Modify: `Source/ProjectA/Characters/PRCharacter.cpp`
- Modify: `Source/ProjectA/Enemies/PREnemyCharacter.cpp`
- Modify: affected GameplayTag automation tests
- Modify: `Config/DefaultGame.ini`
- Delete: `Config/Tags/ProjectRiftGameplayTags.ini`

- [ ] **Step 1: Declare all native tags in one exported namespace**

Use `NativeGameplayTags.h` and this public shape:

```cpp
namespace ProjectRiftGameplayTags
{
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Ability_Primary);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Ability_Dodge);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Ability_Skill_Q);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Ability_Skill_E);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Ability_Skill_R);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Role_Assault);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Role_Engineer);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Role_Medic);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Downed);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stunned);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_Q);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_E);
    PROJECTA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_R);
}
```

Declare exactly the 15 approved identifiers; do not introduce aliases or later-version tags.

- [ ] **Step 2: Define all 15 tags with comments**

In `PRGameplayTags.cpp`, use `UE_DEFINE_GAMEPLAY_TAG_COMMENT` for every exact dotted name. Keep names stable and comments descriptive but behavior-neutral.

- [ ] **Step 3: Replace production string requests with constants**

Include `Core/PRGameplayTags.h` in the six production consumers and replace every production `RequestGameplayTag(TEXT("..."), false)` that corresponds to the contract. Preserve each ability's activation/cooldown/state behavior.

For `PRDamageGameplayEffect.cpp`, set the set-by-caller magnitude with:

```cpp
DamageMagnitude.DataName = NAME_None;
DamageMagnitude.DataTag = ProjectRiftGameplayTags::Data_Damage;
```

Migrate tests to the same constants where they assert production contract behavior; do not leave tests as an independent string-based source of truth.

- [ ] **Step 4: Remove duplicate INI registration and update the version**

Delete `Config/Tags/ProjectRiftGameplayTags.ini` with `apply_patch`. Remove its `[Staging]` `+AllowedConfigFiles` entry from `DefaultGame.ini`. Set `ProjectVersion=0.5.1`.

- [ ] **Step 5: Audit for forbidden production strings**

```powershell
rg -n 'RequestGameplayTag\s*\(\s*TEXT\(' Source\ProjectA --glob '!**/Tests/**'
rg -n 'DataName\s*=\s*TEXT\("Data\.Damage"\)' Source\ProjectA
rg -n 'ProjectRiftGameplayTags\.ini' Config Source
```

Expected: no production string requests, no string `DataName`, and no config reference to the deleted file.

- [ ] **Step 6: Build and run the focused contract tests for GREEN**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests ProjectRift.GameplayTags" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\v0.5.1-tags-green'
```

Expected: build exit 0 and the focused tag contract succeeds.

---

### Task 3: Add a failing settings contract, then implement `UPRProjectSettings`

**Files:**
- Create: `Source/ProjectA/Tests/PRProjectSettingsTest.cpp`
- Create: `Source/ProjectA/Settings/PRProjectSettings.h`
- Create: `Source/ProjectA/Settings/PRProjectSettings.cpp`
- Modify: `Source/ProjectA/ProjectA.Build.cs`
- Modify: `Config/DefaultGame.ini`

- [ ] **Step 1: Write the failing settings metadata/default test**

Create `ProjectRift.Settings.ProjectTuning`. Include the not-yet-existing settings header and assert:

- the class derives from `UDeveloperSettings`, uses `Config=Game`, and its default object exists;
- editor placement is category `ProjectRift`, section `Gameplay`;
- every approved property is `Config`, `EditAnywhere`, and `BlueprintReadOnly`;
- no property is Actor-instance or Blueprint-instance configurable;
- exact CDO defaults and metadata clamps match the table below;
- `DefaultGame.ini` contains an explicit `/Script/ProjectA.PRProjectSettings` section with all 11 values.

| Property | Default | Minimum | Maximum |
|---|---:|---:|---:|
| `InitialRiftStability` | 100.0 | 0.0 | 100.0 |
| `RiftStabilityDrainPerSecond` | 1.0 | 0.0 | N/A |
| `FailedResourceRetentionRate` | 0.5 | 0.0 | 1.0 |
| `ReturnToLobbyDelayAfterSettlement` | 4.0 | 0.0 | N/A |
| `ObjectiveHoldDuration` | 30.0 | 0.1 | N/A |
| `ObjectiveInteractionRadius` | 250.0 | 1.0 | N/A |
| `BaseEnemiesPerWave` | 2 | 0 | N/A |
| `EnemiesPerAdditionalPlayer` | 1 | 0 | N/A |
| `MaxAliveEnemies` | 8 | 1 | N/A |
| `WaveInterval` | 6.0 | 0.1 | N/A |
| `ExtractionRadius` | 320.0 | 1.0 | N/A |

- [ ] **Step 2: Build and confirm RED**

Expected: compilation fails because `Settings/PRProjectSettings.h` does not exist.

- [ ] **Step 3: Add the DeveloperSettings module dependency and class**

Add `DeveloperSettings` to `ProjectA.Build.cs`. Implement:

```cpp
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="ProjectRift Gameplay"))
class PROJECTA_API UPRProjectSettings : public UDeveloperSettings
```

Expose exactly the 11 properties as `UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category=...)`, with `ClampMin`, optional `ClampMax`, and matching `UIMin`/`UIMax`. Initialize the C++ defaults listed above. Override category and section naming only as needed to display `ProjectRift / Gameplay`.

- [ ] **Step 4: Add the explicit config section**

Add to `DefaultGame.ini`:

```ini
[/Script/ProjectA.PRProjectSettings]
InitialRiftStability=100.000000
RiftStabilityDrainPerSecond=1.000000
FailedResourceRetentionRate=0.500000
ReturnToLobbyDelayAfterSettlement=4.000000
ObjectiveHoldDuration=30.000000
ObjectiveInteractionRadius=250.000000
BaseEnemiesPerWave=2
EnemiesPerAdditionalPlayer=1
MaxAliveEnemies=8
WaveInterval=6.000000
ExtractionRadius=320.000000
```

- [ ] **Step 5: Build and run the focused settings contract for GREEN**

Run the Editor build, then `Automation RunTests ProjectRift.Settings.ProjectTuning`. Expected: all metadata, default, clamp, and config assertions succeed.

---

### Task 4: Migrate GameMode settlement and stability tuning to the unique settings source

**Files:**
- Create: `Source/ProjectA/Tests/PRProjectSettingsTestUtils.h`
- Modify: `Source/ProjectA/Tests/PRRiftGameModeStateTest.cpp`
- Modify: `Source/ProjectA/Tests/PRRiftSettlementTest.cpp`
- Modify: `Source/ProjectA/Core/PRRiftGameMode.h`
- Modify: `Source/ProjectA/Core/PRRiftGameMode.cpp`

- [ ] **Step 1: Add a scoped in-memory settings override helper**

The helper obtains `GetMutableDefault<UPRProjectSettings>()`, snapshots all 11 values, and restores them in its destructor. It must never call `SaveConfig` and must not modify disk config. Tests use it before worlds/actors are created so no prior CDO value leaks between tests.

- [ ] **Step 2: Change GameMode tests first and confirm RED**

Configure non-default values through the scoped settings helper for initial stability, drain, failed retention, and return delay. Assert the spawned GameMode/GameState behavior reflects those values. Also assert the four old GameMode UProperties no longer exist while `GetReturnToLobbyDelayAfterSettlement` remains callable.

Run the GameMode/settlement focused tests. Expected RED: current GameMode still reads its local properties.

- [ ] **Step 3: Remove the four local properties and read settings directly**

Remove only:

```text
InitialRiftStability
RiftStabilityDrainPerSecond
FailedResourceRetentionRate
ReturnToLobbyDelayAfterSettlement
```

Keep `ReturnToLobbyMapPath` and the public getter. In runtime code, read `GetDefault<UPRProjectSettings>()` and clamp:

- initial stability to `[0, 100]`;
- drain and return delay to `>= 0`;
- failed retention to `[0, 1]`.

Do not alter authority, replication, mission-state transitions, settlement payloads, or server travel behavior.

- [ ] **Step 4: Build and run focused GameMode tests for GREEN**

Run `ProjectRift.Rift.GameMode` and `ProjectRift.Rift.Settlement` families. Expected: custom settings values drive behavior and the removed properties are absent.

---

### Task 5: Migrate objective, spawn, and extraction tuning to settings

**Files:**
- Modify: `Source/ProjectA/Tests/PRRiftObjectiveSystemTest.cpp`
- Modify: `Source/ProjectA/Tests/PRRiftSpawnManagerTest.cpp`
- Modify: `Source/ProjectA/Tests/PRRiftExtractionZoneTest.cpp`
- Modify: `Source/ProjectA/Core/PRRiftObjectiveActor.h`
- Modify: `Source/ProjectA/Core/PRRiftObjectiveActor.cpp`
- Modify: `Source/ProjectA/Core/PRHoldObjectiveActor.h`
- Modify: `Source/ProjectA/Core/PRHoldObjectiveActor.cpp`
- Modify: `Source/ProjectA/Core/PRSpawnManager.h`
- Modify: `Source/ProjectA/Core/PRSpawnManager.cpp`
- Modify: `Source/ProjectA/Core/PRExtractionZone.h`
- Modify: `Source/ProjectA/Core/PRExtractionZone.cpp`

- [ ] **Step 1: Rework focused tests to configure the settings CDO and confirm RED**

Using the scoped helper:

- set a non-default objective interaction radius and hold duration, then assert getters/component behavior use them;
- set `BaseEnemiesPerWave=1`, `EnemiesPerAdditionalPlayer=1`, `MaxAliveEnemies=3`, and `WaveInterval=0.2`, then preserve the current three-player wave/cap assertions;
- set a non-default extraction radius and assert the runtime sphere/getter uses it;
- assert the seven former Actor properties are absent from their Actor classes.

Run the three focused test families. Expected RED: current actors still use their local values.

- [ ] **Step 2: Migrate objective actors**

Remove `InteractionRadius` from `APRRiftObjectiveActor` and `HoldDuration` from `APRHoldObjectiveActor`. Keep their public getters and read settings directly. Clamp interaction radius to `>= 1` and hold duration to `>= 0.1`. Ensure collision/component initialization and progress timing use the same effective values.

- [ ] **Step 3: Migrate SpawnManager**

Remove the four local tuning UProperties. Read settings when computing wave size, the alive cap, and timer interval. Clamp base/additional enemies to `>= 0`, max alive to `>= 1`, and interval to `>= 0.1`. Preserve authority-only spawning, replication, discovery, timer lifecycle, and enemy class selection.

- [ ] **Step 4: Migrate ExtractionZone**

Remove `ExtractionRadius`, retain the getter, and read/clamp the settings value to `>= 1`. Ensure the sphere component and proximity behavior use that same value. Preserve extraction state, duplicate protection, settlement, and prompt logic.

- [ ] **Step 5: Audit removed local properties**

```powershell
rg -n 'InitialRiftStability|RiftStabilityDrainPerSecond|FailedResourceRetentionRate|ReturnToLobbyDelayAfterSettlement' Source\ProjectA\Core\PRRiftGameMode.h
rg -n 'InteractionRadius|HoldDuration|BaseEnemiesPerWave|EnemiesPerAdditionalPlayer|MaxAliveEnemies|WaveInterval|ExtractionRadius' Source\ProjectA\Core --glob '*.h'
```

Expected: only public getter/function names may remain in Actor headers; none of the 11 values remain as Actor `UPROPERTY` storage.

- [ ] **Step 6: Build and run all affected focused tests for GREEN**

Run the objective, spawn manager, extraction, GameMode, and settlement families. Expected: all succeed with scoped settings overrides fully restored between tests.

---

### Task 6: Validate ProjectA assets and Blueprints through MCP 8001

**Files/assets:**
- Inspect only the ProjectRift maps and Blueprints that derive from the five migrated Actor classes or consume the migrated tags.

- [ ] **Step 1: Start only ProjectA with MCP on port 8001**

Launch the exact `ProjectA.uproject` with `-ModelContextProtocolStartServer -ModelContextProtocolPort=8001`. Poll only port 8001. If unavailable, report the ProjectA-local blocker; never fall back to another service.

- [ ] **Step 2: Compile affected ProjectRift Blueprints**

Compile ProjectRift Blueprints deriving from `APRRiftGameMode`, `APRRiftObjectiveActor`, `APRHoldObjectiveActor`, `APRSpawnManager`, and `APRExtractionZone`, plus character/enemy Blueprints that consume the migrated tag behavior. Use warnings-as-errors where supported.

- [ ] **Step 3: Inspect maps and instances for obsolete overrides**

Inspect only ProjectRift assets. Confirm no migrated property survives as an instance override in `L_Rift_Test` or related ProjectRift maps, and that the expected native parent classes remain intact. Do not edit unrelated Blueprint logic or assets.

- [ ] **Step 4: Audit dirty assets and redirectors**

Expected: no unintended dirty ProjectRift assets and no new redirectors. Save only an intended validated ProjectA asset if required.

- [ ] **Step 5: Close only ProjectA before final build/package verification**

Close only the editor process whose command line names `E:\MyWork\ProjectA\ProjectA.uproject`.

---

### Task 7: Full regression, packaging baseline, documentation, and handoff

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `docs/projectrift/known-issues.md`
- Create: `docs/projectrift/v0.5.1-test-record.md`

- [ ] **Step 1: Run static contract audits**

```powershell
rg -n 'RequestGameplayTag\s*\(\s*TEXT\(' Source\ProjectA --glob '!**/Tests/**'
rg -n 'ProjectRiftGameplayTags\.ini' Config Source
rg -n 'ProjectVersion=0\.5\.1' Config\DefaultGame.ini
git diff --check
git status --short
```

Expected: no production string tag request, no deleted INI reference, version is exact, no whitespace errors, and only v0.5.1 files are changed/untracked.

- [ ] **Step 2: Run a fresh full Editor build**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: exit 0 with UHT and compile/link successful.

- [ ] **Step 3: Run the full ProjectRift automation suite**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests ProjectRift" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\v0.5.1-full'
```

Expected: every discovered ProjectRift leaf test succeeds with zero test errors.

- [ ] **Step 4: Run UAT validation and package a Development baseline**

Use `BuildCookRun` for `ProjectA.uproject`, Win64 Development, Cook, Stage, Pak, and Archive into a ProjectA-local v0.5.1 directory under `Saved`. Do not include editor-only MCP configuration as a runtime dependency.

Expected: UAT exit 0, native tags resolve during Cook, and no missing-tag/config/property serialization errors occur.

- [ ] **Step 5: Smoke-launch the packaged game**

Launch the packaged ProjectA executable with a bounded timeout and ProjectA-local log. Confirm startup reaches the normal ProjectRift map flow without native-tag, settings-class, asset-manager, fatal, or assertion errors. Terminate only that packaged ProjectA process after the smoke window.

- [ ] **Step 6: Record evidence and known limitations**

Update `CHANGELOG.md` for v0.5.1. Update `known-issues.md` only with verified current limitations. Create `v0.5.1-test-record.md` containing exact commands, exit codes, test counts/results, MCP Blueprint checks, UAT result, package path, smoke result, and any non-blocking warnings.

- [ ] **Step 7: Perform the final read-only delivery audit**

```powershell
git diff --stat
git diff -- Config Source CHANGELOG.md docs
git status --short
```

Confirm there are no files from another project, no Git write action occurred, and no later-version work was started.

- [ ] **Step 8: Hand off and stop**

Report:

- changed source/config/docs files and the deleted tag INI;
- affected/validated ProjectRift assets, if any;
- fresh build, focused/full test, MCP, UAT, and package-smoke evidence;
- exact manual acceptance steps for Project Settings, native tags, mission flow, objective timing/radius, spawn scaling/cap, extraction radius, and settlement;
- residual risks and suggested commit message: `v0.5.1 GameplayTag 合同与项目调参设置`.

Do not stage or commit. End with: `已停止，未开始下一版本。`
