# ProjectRift v0.5.0 Primary DataAsset and AssetManager Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. ProjectA policy forbids subagent delegation and all Git write operations for this delivery.

**Goal:** Make a project-owned `UPRAssetManager` discover, synchronously load, asynchronously load, and Cook the existing ProjectRift Item and LootTable Primary DataAssets without hard-coded object paths.

**Architecture:** Configure UE 5.8 to instantiate `UPRAssetManager`, scan `ProjectRiftItem` and `ProjectRiftLootTable` from `/Game/ProjectRift/Items`, and apply `AlwaysCook` rules. The custom manager is the single typed loading boundary; Inventory retains injected-data precedence and otherwise resolves by PrimaryAssetId.

**Tech Stack:** Unreal Engine 5.8 C++, `UAssetManager`, `UPrimaryDataAsset`, `FPrimaryAssetId`, `FStreamableHandle`, Automation Framework, ProjectA Unreal MCP on port 8001, UAT BuildCookRun.

## Global Constraints

- Operate only in `E:\MyWork\ProjectA` and on processes unambiguously owned by `ProjectA.uproject`.
- Use only ProjectA Unreal MCP `http://127.0.0.1:8001/mcp`; never probe or use port 8000.
- Work directly on existing `main`; do not create or switch branches.
- Do not run `git add`, `git commit`, `git tag`, `git push`, stash, reset, restore, checkout, merge, or history-rewrite commands.
- Implement only v0.5.0; do not add Mission, Enemy, Ability, or later-version Primary Asset types.
- Do not rename the ProjectA module, reflected classes, maps, assets, Blueprints, or existing interfaces.
- Use RED→GREEN for each production behavior and preserve all existing ProjectRift tests.
- The user performs acceptance, staging, commit, tag, and push.

---

## File map

- Create `Source/ProjectA/Core/PRAssetManager.h`: typed AssetManager API and native async completion delegates.
- Create `Source/ProjectA/Core/PRAssetManager.cpp`: ID construction, startup validation, sync/async loading, and error logging.
- Create `Source/ProjectA/Tests/PRAssetManagerTest.cpp`: config, discovery, sync, invalid-ID, type, and async regression coverage.
- Modify `Source/ProjectA/Tests/PRInventoryComponentTest.cpp`: prove Inventory can resolve a dynamically registered PrimaryAssetId whose name has no matching filename.
- Modify `Source/ProjectA/Items/PRInventoryComponent.cpp`: remove constructed object path and call `UPRAssetManager`.
- Modify `Config/DefaultEngine.ini`: install `/Script/ProjectA.PRAssetManager` as the project AssetManager.
- Modify `Config/DefaultGame.ini`: register two Primary Asset scans with `AlwaysCook`, remove `DirectoriesToAlwaysCook`, and set version `0.5.0`.
- Modify `CHANGELOG.md`, `docs/projectrift/known-issues.md`, and create `docs/projectrift/v0.5.0-test-record.md`: delivery evidence.

---

### Task 1: Capture v0.4.8 baseline and add the failing AssetManager contract

**Files:**
- Create: `Source/ProjectA/Tests/PRAssetManagerTest.cpp`
- Read: `Config/DefaultEngine.ini`
- Read: `Config/DefaultGame.ini`

**Interfaces:**
- Consumes: existing `UPRItemDataAsset::ItemPrimaryAssetType`, `UPRLootTableDataAsset::LootTablePrimaryAssetType`.
- Produces: failing expectations for `UPRAssetManager`, its sync API, the global manager class, scan rules, and Cook replacement.

- [ ] **Step 1: Confirm the worktree and baseline commit**

Run:

```powershell
git status --short
git branch --show-current
git rev-parse HEAD
```

Expected: only the approved v0.5.0 spec and plan are untracked, branch is `main`, and the base commit is the user's accepted v0.4.8 commit.

- [ ] **Step 2: Save and close only ProjectA if it is running**

Use MCP 8001 to check open/dirty ProjectRift assets. Save only validated ProjectA assets, then close only the editor process whose command line contains `E:\MyWork\ProjectA\ProjectA.uproject`.

- [ ] **Step 3: Run the v0.4.8 baseline build and all ProjectRift tests**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests ProjectRift" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\baseline-v0.4.8-for-v0.5.0'
```

Expected: build exit 0 and all current ProjectRift leaf tests successful.

- [ ] **Step 4: Write the failing manager/config/sync test**

Create `PRAssetManagerTest.cpp` with a test named `ProjectRift.Assets.Manager.Sync` that includes `Core/PRAssetManager.h` and asserts:

```cpp
TestTrue(TEXT("Global manager is UPRAssetManager"), UAssetManager::Get().IsA<UPRAssetManager>());
TestEqual(TEXT("Health ID"), UPRAssetManager::MakeItemPrimaryAssetId(TEXT("HealthInjector")).ToString(), FString(TEXT("ProjectRiftItem:HealthInjector")));
TestEqual(TEXT("Loot ID"), UPRAssetManager::MakeLootTablePrimaryAssetId(TEXT("DA_TestLootTable")).ToString(), FString(TEXT("ProjectRiftLootTable:DA_TestLootTable")));
TestFalse(TEXT("Empty item ID is invalid"), UPRAssetManager::MakeItemPrimaryAssetId(NAME_None).IsValid());

UPRAssetManager* Manager = UPRAssetManager::Get();
TestNotNull(TEXT("Typed manager exists"), Manager);
TestNotNull(TEXT("HealthInjector loads by PrimaryAssetId"), Manager ? Manager->LoadItemDataSync(TEXT("HealthInjector")) : nullptr);
TestNotNull(TEXT("Test loot table loads by PrimaryAssetId"), Manager ? Manager->LoadLootTableSync(TEXT("DA_TestLootTable")) : nullptr);
TestNull(TEXT("Unknown item returns null"), Manager ? Manager->LoadItemDataSync(TEXT("MissingItem")) : nullptr);
TestNull(TEXT("Loot table cannot load as item"), Manager ? Manager->LoadItemDataSync(TEXT("DA_TestLootTable")) : nullptr);
```

Read config arrays through `GConfig` and assert both `PrimaryAssetTypesToScan` entries contain their exact type, base class, `/Game/ProjectRift/Items`, and `CookRule=AlwaysCook`. Assert ProjectPackagingSettings no longer includes `/Game/ProjectRift/Items` under `DirectoriesToAlwaysCook`, and GeneralProjectSettings reports `0.5.0`.

- [ ] **Step 5: Run the build and verify RED**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: compile fails because `Core/PRAssetManager.h` and the planned APIs do not exist. The failure must not be caused by a typo in an existing symbol.

---

### Task 2: Implement global registration, scanning, and synchronous typed loading

**Files:**
- Create: `Source/ProjectA/Core/PRAssetManager.h`
- Create: `Source/ProjectA/Core/PRAssetManager.cpp`
- Modify: `Config/DefaultEngine.ini`
- Modify: `Config/DefaultGame.ini`
- Test: `Source/ProjectA/Tests/PRAssetManagerTest.cpp`

**Interfaces:**
- Consumes: `UPRItemDataAsset::ItemPrimaryAssetType`, `UPRLootTableDataAsset::LootTablePrimaryAssetType`, UE `GetPrimaryAssetPath` and `GetPrimaryAssetObject`.
- Produces: `UPRAssetManager::Get`, ID factories, loaded-object queries, and synchronous typed loaders.

- [ ] **Step 1: Create the minimal header required by the failing test**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "PRAssetManager.generated.h"

class UPRItemDataAsset;
class UPRLootTableDataAsset;

UCLASS()
class PROJECTA_API UPRAssetManager : public UAssetManager
{
    GENERATED_BODY()

public:
    static UPRAssetManager* Get();
    static FPrimaryAssetId MakeItemPrimaryAssetId(FName ItemId);
    static FPrimaryAssetId MakeLootTablePrimaryAssetId(FName AssetName);

    virtual void StartInitialLoading() override;

    UPRItemDataAsset* GetLoadedItemData(FName ItemId) const;
    UPRLootTableDataAsset* GetLoadedLootTable(FName AssetName) const;
    UPRItemDataAsset* LoadItemDataSync(FName ItemId);
    UPRLootTableDataAsset* LoadLootTableSync(FName AssetName);

private:
    UObject* LoadPrimaryAssetSync(const FPrimaryAssetId& AssetId, UClass* ExpectedClass);
    void ValidatePrimaryAssetType(const FPrimaryAssetType& AssetType) const;
};
```

- [ ] **Step 2: Implement ID construction and safe typed manager access**

```cpp
UPRAssetManager* UPRAssetManager::Get()
{
    if (!UAssetManager::IsInitialized())
    {
        UE_LOG(LogProjectA, Error, TEXT("ProjectRift AssetManager is not initialized."));
        return nullptr;
    }

    UPRAssetManager* Manager = Cast<UPRAssetManager>(&UAssetManager::Get());
    if (!Manager)
    {
        UE_LOG(LogProjectA, Error, TEXT("ProjectRift requires UPRAssetManager but active manager is %s."), *GetNameSafe(&UAssetManager::Get()));
    }
    return Manager;
}

FPrimaryAssetId UPRAssetManager::MakeItemPrimaryAssetId(const FName ItemId)
{
    return ItemId.IsNone() ? FPrimaryAssetId() : FPrimaryAssetId(UPRItemDataAsset::ItemPrimaryAssetType, ItemId);
}
```

Implement the LootTable factory identically with `LootTablePrimaryAssetType` and `AssetName`.

- [ ] **Step 3: Implement loaded queries and synchronous loading without a hard-coded object path**

`GetLoadedItemData` and `GetLoadedLootTable` call `GetPrimaryAssetObject` and `Cast`. `LoadPrimaryAssetSync` must:

```cpp
if (!AssetId.IsValid() || !ExpectedClass)
{
    return nullptr;
}
if (UObject* Existing = GetPrimaryAssetObject(AssetId))
{
    return Existing->IsA(ExpectedClass) ? Existing : nullptr;
}
const FSoftObjectPath AssetPath = GetPrimaryAssetPath(AssetId);
if (!AssetPath.IsValid())
{
    UE_LOG(LogProjectA, Warning, TEXT("Primary asset is not registered: %s"), *AssetId.ToString());
    return nullptr;
}
UObject* Loaded = AssetPath.TryLoad();
if (!Loaded || !Loaded->IsA(ExpectedClass))
{
    UE_LOG(LogProjectA, Warning, TEXT("Primary asset load/type validation failed. Id=%s Path=%s Expected=%s Actual=%s"),
        *AssetId.ToString(), *AssetPath.ToString(), *GetNameSafe(ExpectedClass), *GetNameSafe(Loaded ? Loaded->GetClass() : nullptr));
    return nullptr;
}
return Loaded;
```

- [ ] **Step 4: Implement startup validation**

After `Super::StartInitialLoading()`, call `ValidatePrimaryAssetType` for the Item and LootTable types. The helper calls `GetPrimaryAssetIdList`; log the count at `Log` level, and warn only when no IDs are registered.

- [ ] **Step 5: Configure the global manager and Primary Asset scans**

In `DefaultEngine.ini`, under `/Script/Engine.Engine`, set:

```ini
AssetManagerClassName=/Script/ProjectA.PRAssetManager
```

In `DefaultGame.ini`, set `ProjectVersion=0.5.0`, remove:

```ini
+DirectoriesToAlwaysCook=(Path="/Game/ProjectRift/Items")
```

and add:

```ini
[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="ProjectRiftItem",AssetBaseClass="/Script/ProjectA.PRItemDataAsset",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/ProjectRift/Items")),SpecificAssets=,Rules=(Priority=1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
+PrimaryAssetTypesToScan=(PrimaryAssetType="ProjectRiftLootTable",AssetBaseClass="/Script/ProjectA.PRLootTableDataAsset",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game/ProjectRift/Items")),SpecificAssets=,Rules=(Priority=1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
```

- [ ] **Step 6: Build and run the focused sync test for GREEN**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests ProjectRift.Assets.Manager.Sync" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\v0.5.0-manager-sync-green'
```

Expected: build exit 0 and focused test successful with zero test errors.

---

### Task 3: Add and verify typed asynchronous loading

**Files:**
- Modify: `Source/ProjectA/Tests/PRAssetManagerTest.cpp`
- Modify: `Source/ProjectA/Core/PRAssetManager.h`
- Modify: `Source/ProjectA/Core/PRAssetManager.cpp`

**Interfaces:**
- Consumes: ID factories and loaded queries from Task 2.
- Produces: `FPRItemDataLoadComplete`, `FPRLootTableLoadComplete`, `LoadItemDataAsync`, and `LoadLootTableAsync`.

- [ ] **Step 1: Add a failing async automation test**

Add `ProjectRift.Assets.Manager.Async`. Request an Item and LootTable before calling their sync loaders, retain result objects in local variables, wait on each returned handle, and assert:

```cpp
bool bItemCallback = false;
UPRItemDataAsset* AsyncItem = nullptr;
TSharedPtr<FStreamableHandle> ItemHandle = Manager->LoadItemDataAsync(
    TEXT("EnergyCrystal"),
    FPRItemDataLoadComplete::CreateLambda([&](UPRItemDataAsset* Loaded)
    {
        bItemCallback = true;
        AsyncItem = Loaded;
    }));
TestTrue(TEXT("Known item async request returns a handle"), ItemHandle.IsValid());
if (ItemHandle)
{
    ItemHandle->WaitUntilComplete();
}
TestTrue(TEXT("Item callback executes"), bItemCallback);
TestEqual(TEXT("Async item ID"), AsyncItem ? AsyncItem->ItemId : NAME_None, FName(TEXT("EnergyCrystal")));
```

Repeat for `DA_TestLootTable`. Also assert an empty ID returns an empty Handle and immediately invokes the callback exactly once with `nullptr`.

- [ ] **Step 2: Build and verify RED**

Run the Editor build. Expected: compile fails because async delegates and methods do not exist.

- [ ] **Step 3: Add the delegate and method declarations**

```cpp
class FStreamableHandle;
DECLARE_DELEGATE_OneParam(FPRItemDataLoadComplete, UPRItemDataAsset*);
DECLARE_DELEGATE_OneParam(FPRLootTableLoadComplete, UPRLootTableDataAsset*);

TSharedPtr<FStreamableHandle> LoadItemDataAsync(FName ItemId, FPRItemDataLoadComplete Completion);
TSharedPtr<FStreamableHandle> LoadLootTableAsync(FName AssetName, FPRLootTableLoadComplete Completion);
```

- [ ] **Step 4: Implement async loading through `LoadPrimaryAsset`**

For each method:

1. Construct and validate the PrimaryAssetId.
2. If invalid, execute the completion once with `nullptr` and return an empty Handle.
3. If already loaded, execute the completion once with the loaded object and return an empty Handle.
4. Reject unregistered IDs by checking `GetPrimaryAssetPath`; warn, complete with `nullptr`, and return an empty Handle.
5. Call `LoadPrimaryAsset` with an `FStreamableDelegate::CreateWeakLambda` that retrieves the typed loaded object and executes the captured completion once.
6. If `LoadPrimaryAsset` returns an empty Handle, warn and execute the completion once with `nullptr`.

- [ ] **Step 5: Run focused async and sync tests for GREEN**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests ProjectRift.Assets.Manager" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\v0.5.0-manager-green'
```

Expected: both manager tests successful and zero test errors.

---

### Task 4: Route Inventory through AssetManager and prove path independence

**Files:**
- Modify: `Source/ProjectA/Tests/PRInventoryComponentTest.cpp`
- Modify: `Source/ProjectA/Items/PRInventoryComponent.cpp`

**Interfaces:**
- Consumes: `UPRAssetManager::LoadItemDataSync(FName)`.
- Produces: Inventory fallback resolution by PrimaryAssetId with injected `ItemDataAssets` still taking precedence.

- [ ] **Step 1: Add a failing dynamic-ID Inventory test**

In `PRInventoryComponentTest.cpp`, obtain the global `UPRAssetManager`, dynamically register:

```cpp
const FPrimaryAssetId AliasId(UPRItemDataAsset::ItemPrimaryAssetType, TEXT("ManagerAlias"));
const FSoftObjectPath HealthPath(TEXT("/Game/ProjectRift/Items/DA_HealthInjector.DA_HealthInjector"));
TestTrue(TEXT("Dynamic alias registered"), Manager->AddDynamicAsset(AliasId, HealthPath, FAssetBundleData()));
UPRItemDataAsset* AliasData = Inventory->FindItemData(TEXT("ManagerAlias"));
TestNotNull(TEXT("Inventory resolves dynamic PrimaryAssetId without a matching filename"), AliasData);
TestEqual(TEXT("Alias resolves registered asset"), AliasData ? AliasData->ItemId : NAME_None, FName(TEXT("HealthInjector")));
```

The alias intentionally has no `DA_ManagerAlias` file, so the existing hard-coded path implementation must fail.

- [ ] **Step 2: Build and run `ProjectRift.Items.Inventory` to verify RED**

Expected: the new assertion fails because the current Inventory constructs `/Game/ProjectRift/Items/DA_ManagerAlias`.

- [ ] **Step 3: Replace only the fallback lookup**

Keep the injected `ItemDataAssets` loop unchanged. Replace the constructed `AssetPath` and `LoadObject` with:

```cpp
if (UPRAssetManager* AssetManager = UPRAssetManager::Get())
{
    return AssetManager->LoadItemDataSync(ItemId);
}
return nullptr;
```

Add `#include "Core/PRAssetManager.h"`. Do not change Inventory replication, stack, use, drop, or UI behavior.

- [ ] **Step 4: Verify GREEN and absence of the old fallback string**

```powershell
rg -n "DA_%s|/Game/ProjectRift/Items/DA_" Source\ProjectA\Items\PRInventoryComponent.cpp
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests ProjectRift.Items.InventoryComponent" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\v0.5.0-inventory-green'
```

Expected: `rg` finds no constructed item path and the Inventory test succeeds.

---

### Task 5: ProjectA MCP asset validation

**Files:**
- Inspect only: `/Game/ProjectRift/Items/DA_HealthInjector`
- Inspect only: `/Game/ProjectRift/Items/DA_ShieldPack`
- Inspect only: `/Game/ProjectRift/Items/DA_EnergyCrystal`
- Inspect only: `/Game/ProjectRift/Items/DA_CommonChip`
- Inspect only: `/Game/ProjectRift/Items/DA_TestLootTable`
- Compile only affected ProjectRift Blueprints.

**Interfaces:**
- Consumes: ProjectA MCP 8001 and final native classes/config.
- Produces: asset-class/tag evidence, Blueprint compile evidence, dirty-state and redirector audit.

- [ ] **Step 1: Start only ProjectA with MCP port 8001**

Launch the exact ProjectA editor with `-ModelContextProtocolStartServer -ModelContextProtocolPort=8001`. Poll only port 8001.

- [ ] **Step 2: Inspect the five DataAssets**

Use AssetTools `get_asset_class`, `get_asset_tags`, and ObjectTools `get_properties`. Confirm four Item assets expose the expected ItemId and the loot table exposes four entries. Confirm registry tags/manager tests report the expected PrimaryAssetIds.

- [ ] **Step 3: Compile affected Blueprints**

Compile `BP_PRCharacter`, `BP_PRPlayerController`, `BP_PRPickupActor`, and UI Blueprints that consume Item data with `warnings_as_errors=true`. Do not modify graph logic.

- [ ] **Step 4: Audit dirty assets and redirectors**

Confirm the inspected assets and three ProjectRift maps are not dirty. Search `/Game/ProjectRift` for `ObjectRedirector`; expected result is empty. Save nothing unless an intended validated ProjectA asset is dirty.

- [ ] **Step 5: Close only ProjectA**

Close the editor process whose command line names `E:\MyWork\ProjectA\ProjectA.uproject`. Confirm it exits before full build/package verification.

---

### Task 6: Full verification, package smoke, and documentation

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `docs/projectrift/known-issues.md`
- Create: `docs/projectrift/v0.5.0-test-record.md`

**Interfaces:**
- Consumes: all v0.5.0 implementation and tests.
- Produces: final evidence and strict-version handoff.

- [ ] **Step 1: Run the final Editor build**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: exit 0.

- [ ] **Step 2: Run all ProjectRift tests**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests ProjectRift" -TestExit="Automation Test Queue Empty" -ReportExportPath='E:\MyWork\ProjectA\Saved\Automation\Reports\v0.5.0-final'
```

Expected: every leaf test is successful, with 0 failed and 0 not run.

- [ ] **Step 3: Build/Cook/Stage/Pak/Archive Win64 Development**

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun -project='E:\MyWork\ProjectA\ProjectA.uproject' -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory='E:\MyWork\ProjectA\Saved\StagedBuilds\v0.5.0' -utf8output
```

Expected: `BUILD SUCCESSFUL` and AutomationTool exit code 0.

- [ ] **Step 4: Run the packaged ProjectA smoke**

Launch the archived `ProjectA.exe` directly into `/Game/ProjectRift/Maps/L_Rift_Test?listen` with `-benchmark -fps=60 -seconds=10 -nullrhi -nosound -unattended -NoSplash` and an `-abslog` under `E:\MyWork\ProjectA\Saved\Logs`.

Expected: map loads, ProjectRift mission starts, AssetManager startup reports both types and nonzero counts, and the log has zero Fatal/Error/missing Item/LootTable/type-mismatch lines.

- [ ] **Step 5: Write delivery documentation**

Record the exact RED failures, GREEN reports, final leaf-test counts, MCP audit, UAT exit code, archive metrics/hashes, smoke-log evidence, warnings, changed files/assets, manual acceptance steps, and suggested commit message. Update CHANGELOG with `0.5.0` only.

- [ ] **Step 6: Run the completion audit without Git writes**

```powershell
git diff --check
git status --short
git diff --cached --name-only
```

Expected: diff check exit 0, all v0.5.0 changes visible, and no staged files. Confirm no ProjectA editor/game process remains.

- [ ] **Step 7: Hand off and stop**

Report changed files/assets, verification evidence, package path, manual acceptance matrix, known risks, suggested commit message/tag, and exact final status: `已停止，未开始下一版本。`
