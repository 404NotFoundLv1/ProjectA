# ProjectRift known issues

## v0.6.2 acceptance-relevant items

- v0.6.2 uses intentionally lightweight placeholder feedback: shared Quinn hit-reaction montages, a ProjectRift tint material, and a small ProjectRift impact particle. Production audio, VFX, recoil, hit locations, and poise remain out of scope.
- Hit confirmations are owner-only unreliable presentation events. Network loss may omit a marker or number but never affects damage, ammo, status, stagger, death, or scoring authority.
- Pollution intentionally emits no hit number or stagger for periodic ticks; its persistent status Cue remains active for the GameplayEffect lifetime.
- `GameplayCueNotifyPaths` is now explicitly `/Game/ProjectRift/GameplayCues`; the earlier broad-search warning is resolved.
- Two-player PIE remains the required manual check for remote Cue visibility, source-only hit numbers, and reduced camera shake feel.
- UAT may report unavailable SDKs for non-Win64 platforms while validating platforms. Win64 is the supported verification target.

## v0.6.0 acceptance-relevant items

- Status feedback is intentionally text-only in this slice; production icons, VFX, audio, GameplayCues, stacking, immunity, armor, critical hits, and persistence are out of scope.
- Pollution uses GAS periodic scheduling with a minimal boundary tolerance so the five-second effect executes exactly five times, including the first application frame.
- Two-player PIE remains the required manual check for replicated status tags and player friendly-fire rejection.
- The existing GameplayCue search-path warning remains unrelated; v0.6.0 adds no GameplayCue assets.
- UAT may report unavailable SDKs for non-Win64 platforms while validating platforms. Win64 is the supported verification target for this slice.

## v0.5.6 acceptance-relevant items

- The Gauntlet loop is single-client and deterministic. It proves the complete local authoritative loop but does not exercise two separate client processes.
- Shipping logging and console commands are compiled out by default, so the automated Shipping probe validates a healthy bounded NullRHI lifetime and exact ProjectA process cleanup. Manual acceptance should still launch the graphical Shipping package.
- RunUnreal owns its device `-userdir` beneath the UE Gauntlet cache. ProjectRift profile files remain fixed beneath the ProjectA `Saved/Automation/Gauntlet/<RunId>/Profiles` root.
- UAT reports unavailable SDKs for non-Win64 platforms while validating platforms. Win64 remains valid, and both Development and Shipping packages succeed.
- ProjectA local runs now guard the official shared `UnrealEditor.modules` by hash and keep a ProjectA-local backup. A detected change stops the run but is intentionally not auto-restored into the shared UE installation.

## v0.5.1 acceptance-relevant items

- The project settings values are now global and unique. Actor and Blueprint instance overrides for the 11 migrated fields no longer exist; acceptance should change values through Project Settings > ProjectRift > Gameplay and restore the approved defaults before committing.
- The packaged NullRHI smoke test still logs the existing Gameplay Ability System warning because `GameplayCueNotifyPaths` is not explicitly narrowed. v0.5.1 adds no GameplayCue assets and does not change this unrelated configuration.
- A graphical packaged smoke reached `L_ShipLobby`, then logged D3D12 compute-PSO allocation errors under current machine memory pressure. The follow-up NullRHI smoke was clean. Manual acceptance must include a normal graphical launch on the target machine.
- Automation validates the configured values through CDO overrides and restores them after each test. It does not replace a human check that Project Settings edits persist after an editor restart.
- UAT reports unavailable SDKs for non-Win64 platforms while validating platforms. Win64 remains valid and the Win64 Development package succeeds.

## v0.5.0 acceptance-relevant items

- The packaged NullRHI smoke test still logs the existing Gameplay Ability System warning because `GameplayCueNotifyPaths` is not explicitly narrowed. v0.5.0 does not add GameplayCue assets and does not change this unrelated configuration.
- The automated benchmark exit still logs the existing CrowdManager warning after exit has been requested. MCP inspection confirms the Rift test map retains its navigation data; verify normal navigation during manual gameplay acceptance.
- The startup AssetManager validation logs counts for configured types. A zero count is a warning and does not abort editor startup, so future asset-directory changes must keep the automated discovery and package tests.
- Synchronous loading remains in the existing Inventory fallback path for compatibility. v0.5.0 provides an asynchronous foundation but does not convert Inventory or UI to an all-async lifecycle.
- UAT reports unavailable SDKs for non-Win64 platforms while validating platforms. Win64 remains valid and the Win64 Development package succeeds.

## v0.4.8 acceptance-relevant items

- The packaged NullRHI smoke test logs a Gameplay Ability System warning because `GameplayCueNotifyPaths` is not explicitly narrowed. There are no ProjectRift GameplayCue assets in this slice, so this is currently a search-performance warning rather than a functional failure.
- The automated benchmark exit logs a CrowdManager warning that a RecastNavMesh cannot be found after exit has already been requested. MCP inspection confirms `L_Rift_Test` contains `NavMeshBoundsVolume_0` and `RecastNavMesh-Default`; verify normal gameplay navigation during manual acceptance.
- The packaged smoke test proves boot, map load, one-player mission start, structured IDs, and teardown. It does not replace the requested human traversal of the complete objective/extraction/settlement/return loop.
- Two-player listen-server behavior still requires the owner's manual acceptance pass. Automation covers authority guards and replicated state in test worlds, but not a full interactive two-window session.
- UAT validates SDKs for multiple platforms and reports unavailable non-Win64 SDKs. Win64 is valid and the Win64 Development package completes successfully.

## Release gate

Do not accept v0.6.2 until the manual matrix in `v0.6.2-test-record.md` passes. Do not begin the next small version before explicit user authorization.
