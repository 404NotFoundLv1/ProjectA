# ProjectRift local build and Smoke pipeline

This Windows-only entry point targets ProjectA, PowerShell 5.1, Win64, and Unreal Engine 5.8:

```powershell
# Quick is the default: Editor Development incremental build + ProjectRift.Smoke
.\Scripts\ProjectRift\Invoke-ProjectRiftLocal.ps1

# Full: Editor build + all ProjectRift tests + Development package + Gauntlet loop
.\Scripts\ProjectRift\Invoke-ProjectRiftLocal.ps1 -Mode Full

# One Shipping package, exclusion check, and bounded NullRHI launch probe
.\Scripts\ProjectRift\Invoke-ProjectRiftLocal.ps1 `
  -Mode Build -Target Package -Configuration Shipping
```

Use `-NoReopenEditor` for unattended local verification. `-Target Editor|Game|Package` and `-Configuration Development|Shipping` apply to `-Mode Build`. A Shipping Editor build and Development-only Smoke in Shipping are rejected as prerequisites.

## Engine and editor ownership

Engine resolution is deliberately bounded:

1. `-EngineRoot`
2. `UE_ENGINE_ROOT`
3. the UnrealBuildTool path already present in ProjectA's own `ProjectA.sln`

The selected engine must expose the required UBT, UAT, Editor, and commandlet executables and report UE 5.8 in `Engine/Build/Build.version`. The script does not scan drives or other projects.

Before ProjectA closes or builds anything, the pipeline also requires `Engine/Binaries/Win64/UnrealEditor.modules`. It records its SHA-256 and copies a readback-verified backup into the current ProjectA run's `engine-metadata-backup` directory. The hash is checked after Editor build, Automation, package, and Gauntlet boundaries. A missing or changed shared manifest stops the run immediately; the script never guesses, regenerates, deletes, or automatically writes back shared engine metadata.

Only a process named `UnrealEditor.exe` whose command line contains the canonical ProjectA `.uproject` token is owned by this pipeline. It receives a normal close request and up to 30 seconds to exit. A blocked close returns code 3 and is never force-killed. If ProjectA was open before a successful run, it is reopened unless `-NoReopenEditor` was supplied. Any failed run leaves it closed.

## Modes

- `Quick`: `ProjectAEditor Win64 Development`, followed by `ProjectRift.Smoke` in `UnrealEditor-Cmd` with NullRHI, no audio, unattended execution, and a JSON Automation report.
- `Full`: Quick's Editor build, all `ProjectRift` automation tests, Win64 Development BuildCookRun, and the custom `ProjectRift.LocalSmoke` Gauntlet node.
- `Build`: one Editor or Game UBT target, or one Package BuildCookRun. A Development package must pass Gauntlet before promotion. A Shipping package must pass module exclusion and bounded NullRHI launch checks.

Gauntlet owns process/session monitoring, timeouts, role command lines, and result promotion; UBT/UAT build and package work happens before the test node, matching the [official Gauntlet responsibility boundary](https://dev.epicgames.com/documentation/unreal-engine/gauntlet-automation-framework-overview-in-unreal-engine?lang=en-US).

## Smoke contracts

`ProjectRift.Smoke` contains:

- `Content`: loads `L_MainMenu`, `L_ShipLobby`, and `L_Rift_Test`, plus item, loot-table, mission, and ship-repair Primary Assets.
- `ProfileIsolation`: creates, saves, reloads, and projects a profile only beneath `Saved/Automation`.
- `GauntletContract`: validates the controller type, 30-second stage timeout, 180-second total timeout, legal transitions, and duplicate-callback rejection.

`ProjectRift.LocalSmoke` launches one Win64 Development Client at `L_ShipLobby` with NullRHI, no audio, unattended mode, a Gauntlet-owned isolated `-userdir`, and `PRLocalSmokeGauntletController`. It uses authoritative ProjectRift APIs to create/bind a profile, start the selected mission through ServerTravel, collect one deterministic `EnergyCrystal`, complete the objective, extract, save the settlement, return to the lobby, and revalidate persisted/runtime state. The controller emits `PROJECTRIFT_SMOKE_STAGE=<Stage>` and finishes with either `PROJECTRIFT_SMOKE_RESULT=PASS` or a structured failure diagnostic.

The profile root is derived only from a valid run GUID:

```text
Saved/Automation/Gauntlet/<RunId>/Profiles
```

Missing/invalid GUIDs, missing/relative `-userdir`, filesystem-root `-userdir`, and ProjectA non-automation saved paths fail closed before catalog loading. The engine's custom user directory argument supplies session isolation; see the [UE 5.8 `FPaths` custom user directory documentation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FPaths/CustomUserDirArgument?lang=en-US).

## Reports and package promotion

Each invocation writes:

```text
Saved/Automation/LocalBuildRuns/<UtcTimestamp>-<RunId>/
  summary.json
  editor-build.log / game-build.log
  automation.log / regression.log
  package.log
  gauntlet.log
  shipping-launch.log
  reports/
```

`summary.json` records schema/project/engine versions, the guarded engine-manifest path/backup/hash/preservation result, mode, target, configuration, UTC timestamps, duration, overall/failure state, per-stage exit code/duration/log/test counts, package file count/size/executable SHA-256, and Gauntlet run/stage/result.

Packages are first archived to a candidate below `Saved/StagedBuilds/Local`. Normalized containment and reparse-point checks run before recursive deletion, replacement, or rollback. Only a validated candidate replaces `Development` or `Shipping`; replacement failures restore the previous valid directory. Shipping's bounded probe treats a healthy ten-second NullRHI lifetime as startup success, then stops only the launcher and verified child executable inside that candidate package.

Exit codes:

| Code | Meaning |
| ---: | --- |
| 0 | Success |
| 2 | Prerequisite failure |
| 3 | ProjectA editor close failure |
| 10 | UBT build failure |
| 11 | Automation failure |
| 12 | Package, exclusion, promotion, or Shipping probe failure |
| 13 | Gauntlet Smoke failure |

Run the dependency-free PowerShell contract suite with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Scripts\ProjectRift\Tests\Test-ProjectRiftLocalBuild.ps1
```
