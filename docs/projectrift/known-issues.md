# ProjectRift known issues

## v0.4.8 acceptance-relevant items

- The packaged NullRHI smoke test logs a Gameplay Ability System warning because `GameplayCueNotifyPaths` is not explicitly narrowed. There are no ProjectRift GameplayCue assets in this slice, so this is currently a search-performance warning rather than a functional failure.
- The automated benchmark exit logs a CrowdManager warning that a RecastNavMesh cannot be found after exit has already been requested. MCP inspection confirms `L_Rift_Test` contains `NavMeshBoundsVolume_0` and `RecastNavMesh-Default`; verify normal gameplay navigation during manual acceptance.
- The packaged smoke test proves boot, map load, one-player mission start, structured IDs, and teardown. It does not replace the requested human traversal of the complete objective/extraction/settlement/return loop.
- Two-player listen-server behavior still requires the owner's manual acceptance pass. Automation covers authority guards and replicated state in test worlds, but not a full interactive two-window session.
- UAT validates SDKs for multiple platforms and reports unavailable non-Win64 SDKs. Win64 is valid and the Win64 Development package completes successfully.

## Release gate

Do not accept v0.4.8 until the manual matrix in `v0.4.8-test-record.md` passes. Do not begin the next small version before explicit user authorization.
