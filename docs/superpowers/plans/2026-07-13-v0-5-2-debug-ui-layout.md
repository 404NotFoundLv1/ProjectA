# ProjectRift v0.5.2 Debug UI Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Separate the v0.5.2 native profile panel, GAS diagnostics, and lobby Ready diagnostics into readable right-top, left-top, and left-bottom screen regions.

**Architecture:** Centralize viewport geometry in a small pure C++ layout contract used by runtime placement and automation tests. Keep the profile UI owned by `UPRGameInstance`; replace the unpositionable engine Ready message with a focused native widget owned by `APRPlayerController`.

**Tech Stack:** Unreal Engine 5.8 C++, UMG viewport slots, Slate native widgets, Unreal Automation Framework, ProjectA MCP at `http://127.0.0.1:8001/mcp`.

## Global Constraints

- Operate only in `E:\MyWork\ProjectA` and only on ProjectA-owned processes.
- Remain on existing `main`; do not stage, commit, tag, push, stash, reset, restore, checkout, or rewrite history.
- This is a v0.5.2 acceptance correction; do not begin v0.5.3.
- Do not add Blueprint or content assets; Shipping must not expose the verification panel.
- Never access port 8000; Unreal MCP verification uses only port 8001.

---

### Task 1: Add the debug viewport layout contract with a RED test

**Files:**
- Create: `Source/ProjectA/UI/PRDebugUILayout.h`
- Create: `Source/ProjectA/Tests/PRDebugUILayoutTest.cpp`

**Interfaces:**
- Produces: `FPRDebugUILayout::{Profile,GAS,LobbyReady}{Anchors,Alignment,Position,Size}()`.
- Consumes: Unreal `FAnchors` and `FVector2D` value types only.

- [ ] **Step 1: Write the failing layout contract test before creating the production header**

```cpp
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/PRDebugUILayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPRDebugUILayoutTest,
    "ProjectRift.UI.DebugLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRDebugUILayoutTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Profile anchor is right-top"), FPRDebugUILayout::ProfileAnchors().Minimum, FVector2D(1.0f, 0.0f));
    TestEqual(TEXT("Profile aligns from right-top"), FPRDebugUILayout::ProfileAlignment(), FVector2D(1.0f, 0.0f));
    TestTrue(TEXT("Profile remains inside right edge"), FPRDebugUILayout::ProfilePosition().X < 0.0f);
    TestEqual(TEXT("GAS anchor is left-top"), FPRDebugUILayout::GASAnchors().Minimum, FVector2D::ZeroVector);
    TestEqual(TEXT("Ready anchor is left-bottom"), FPRDebugUILayout::LobbyReadyAnchors().Minimum, FVector2D(0.0f, 1.0f));
    TestEqual(TEXT("Ready aligns from left-bottom"), FPRDebugUILayout::LobbyReadyAlignment(), FVector2D(0.0f, 1.0f));
    TestTrue(TEXT("Top panels use distinct horizontal regions"), FPRDebugUILayout::ProfileAnchors().Minimum.X > FPRDebugUILayout::GASAnchors().Minimum.X);
    TestTrue(TEXT("Ready is below GAS"), FPRDebugUILayout::LobbyReadyAnchors().Minimum.Y > FPRDebugUILayout::GASAnchors().Minimum.Y);
    return true;
}
#endif
```

- [ ] **Step 2: Close only ProjectA and run the Editor build to verify RED**

Run:

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectAEditor Win64 Development 'E:\MyWork\ProjectA\ProjectA.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: compilation fails because `UI/PRDebugUILayout.h` does not exist.

- [ ] **Step 3: Add the minimal layout contract**

```cpp
#pragma once

#include "Components/CanvasPanelSlot.h"
#include "Math/Vector2D.h"

struct FPRDebugUILayout final
{
    static FAnchors ProfileAnchors() { return FAnchors(1.0f, 0.0f); }
    static FVector2D ProfileAlignment() { return FVector2D(1.0f, 0.0f); }
    static FVector2D ProfilePosition() { return FVector2D(-24.0f, 24.0f); }
    static FVector2D ProfileSize() { return FVector2D(560.0f, 620.0f); }

    static FAnchors GASAnchors() { return FAnchors(0.0f, 0.0f); }
    static FVector2D GASAlignment() { return FVector2D(0.0f, 0.0f); }
    static FVector2D GASPosition() { return FVector2D(24.0f, 24.0f); }
    static FVector2D GASSize() { return FVector2D(420.0f, 250.0f); }

    static FAnchors LobbyReadyAnchors() { return FAnchors(0.0f, 1.0f); }
    static FVector2D LobbyReadyAlignment() { return FVector2D(0.0f, 1.0f); }
    static FVector2D LobbyReadyPosition() { return FVector2D(24.0f, -24.0f); }
    static FVector2D LobbyReadySize() { return FVector2D(520.0f, 220.0f); }
};
```

- [ ] **Step 4: Build and run the focused test to verify GREEN**

Run the Editor build above, then:

```powershell
& 'D:\Unreal Engine 5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'E:\MyWork\ProjectA\ProjectA.uproject' -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests ProjectRift.UI.DebugLayout; Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:\MyWork\ProjectA\Saved\Automation\Reports\v0.5.2-ui-layout-green'
```

Expected: `ProjectRift.UI.DebugLayout` succeeds with zero errors.

### Task 2: Move and reflow the profile verification panel

**Files:**
- Modify: `Source/ProjectA/Core/PRGameInstance.cpp`
- Modify: `Source/ProjectA/UI/PRProfileDebugWidget.cpp`

**Interfaces:**
- Consumes: `FPRDebugUILayout::Profile*()`.
- Produces: a right-top, fixed-size, scroll-safe profile verification panel.

- [ ] **Step 1: Apply the shared right-top viewport geometry after `AddToViewport(100)`**

```cpp
#include "UI/PRDebugUILayout.h"

ProfileDebugWidget->AddToViewport(100);
ProfileDebugWidget->SetAnchorsInViewport(FPRDebugUILayout::ProfileAnchors());
ProfileDebugWidget->SetAlignmentInViewport(FPRDebugUILayout::ProfileAlignment());
ProfileDebugWidget->SetPositionInViewport(FPRDebugUILayout::ProfilePosition(), false);
ProfileDebugWidget->SetDesiredSizeInViewport(FPRDebugUILayout::ProfileSize());
```

- [ ] **Step 2: Give the profile widget a readable panel shell and two action rows**

Use `SBorder` with `BorderBackgroundColor(FLinearColor(0, 0, 0, 0.82f))`, 16px padding, an 18pt title, two `SHorizontalBox` action rows, and a `SScrollBox` constrained with `SBox::MaxDesiredHeight(360.0f)` for profiles. Set profile and status text to 14pt and enable wrapping for GUID/status text.

- [ ] **Step 3: Rebuild and run `ProjectRift.Save.SubsystemContract` plus `ProjectRift.UI.DebugLayout`**

Expected: both tests succeed and no new Blueprint/content asset is generated.

### Task 3: Replace the lobby Ready engine message with a left-bottom native widget

**Files:**
- Create: `Source/ProjectA/UI/PRLobbyReadyDebugWidget.h`
- Create: `Source/ProjectA/UI/PRLobbyReadyDebugWidget.cpp`
- Modify: `Source/ProjectA/Player/PRPlayerController.h`
- Modify: `Source/ProjectA/Player/PRPlayerController.cpp`

**Interfaces:**
- Produces: `void UPRLobbyReadyDebugWidget::SetReadyText(const FText& InText)`.
- Consumes: Ready text already assembled by `APRPlayerController::RefreshLobbyReadyDebugDisplay()` and `FPRDebugUILayout::LobbyReady*()`.

- [ ] **Step 1: Implement the focused native widget**

```cpp
UCLASS()
class PROJECTA_API UPRLobbyReadyDebugWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetReadyText(const FText& InText);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    TSharedPtr<STextBlock> ReadyTextBlock;
    FText PendingText;
};
```

`RebuildWidget()` returns an `SBorder` with 0.72 alpha black background, 12px padding, and a wrapping 14pt `STextBlock`. `SetReadyText()` updates `PendingText` and the Slate text block when valid.

- [ ] **Step 2: Add controller ownership and cleanup**

Add `TObjectPtr<UPRLobbyReadyDebugWidget> LobbyReadyDebugWidget`, plus `CreateLobbyReadyDebugHUD()` and `DestroyLobbyReadyDebugHUD()`. `EndPlay()` must destroy it before `Super::EndPlay()`.

- [ ] **Step 3: Place the Ready widget at left-bottom and remove the engine message path**

`CreateLobbyReadyDebugHUD()` uses `AddToPlayerScreen(21)` and all four `FPRDebugUILayout::LobbyReady*()` values. `RefreshLobbyReadyDebugDisplay()` creates/updates it only in `IsShipLobbyDebugWorld(World)`; outside the lobby it calls `DestroyLobbyReadyDebugHUD()`. Remove `GEngine->AddOnScreenDebugMessage` and `RemoveOnScreenDebugMessage` for the Ready list only.

- [ ] **Step 4: Apply the shared GAS geometry**

In `CreateGASDebugHUD()`, replace literal alignment/position/size with `FPRDebugUILayout::GAS*()` and explicitly set the left-top anchors.

- [ ] **Step 5: Build and run focused UI/gameplay regression**

Run `ProjectAEditor Win64 Development`, then automate `ProjectRift.UI.DebugLayout`, `ProjectRift.Gameplay.LobbyRiftLobbyLoop`, and the six `ProjectRift.Save` tests. Expected: zero failures.

### Task 4: ProjectA-only visual and final regression verification

**Files:**
- Verify only; no additional production files expected.

**Interfaces:**
- Consumes: completed runtime layout and ProjectA MCP 8001.
- Produces: screenshot/log/build/test evidence for user acceptance.

- [ ] **Step 1: Run fresh Development builds**

Build `ProjectAEditor Win64 Development` and `ProjectA Win64 Development`; both must return `Result: Succeeded`.

- [ ] **Step 2: Run the full ProjectRift automation suite**

Export to `Saved/Automation/Reports/v0.5.2-ui-layout-final` and verify the report contains zero failed/not-run tests; all `ProjectRift.Save` and `ProjectRift.UI.DebugLayout` tests must have zero errors.

- [ ] **Step 3: Verify through ProjectA MCP only**

Open `L_MainMenu`, start in-process PIE, capture the editor image, and confirm profile=right-top, GAS=left-top, Ready=left-bottom with no overlap. Read `LogProjectA` for UI/profile errors, stop PIE, and restore `/Game/ProjectRift/Maps/L_ShipLobby`.

- [ ] **Step 4: Audit the workspace without Git writes**

Run `git status --short` and `git diff --check`. Confirm branch remains `main`, no files are staged, and no assets were created. Stop and hand off v0.5.2 for user acceptance.

