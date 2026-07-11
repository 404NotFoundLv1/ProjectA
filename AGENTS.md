# ProjectA Codex Instructions

## Scope boundary

- Operate only on `E:\MyWork\ProjectA` and processes that are unambiguously owned by `ProjectA.uproject`.
- Do not read, inspect, stop, modify, control, or reuse any other project, editor, MCP server, log, process, or asset.
- If ProjectA tooling is unavailable, stop and report the ProjectA-local blocker. Never fall back to another project's running service.

## Unreal MCP

- ProjectA Unreal MCP uses only `http://127.0.0.1:8001/mcp`.
- Port `8000` is outside ProjectA scope. Do not probe it or send requests to it.
- Unreal MCP is an editor-development tool, not a runtime game dependency.

## Version gate

- Implement only the small version explicitly started by the user.
- After that version is implemented and verified, stop and wait for the user's acceptance.
- Do not plan or begin the next version until the user explicitly says to start it.

## Git boundary

- Work directly on the existing `main` branch until the user changes this rule.
- The user performs staging, commits, tags, pushes, and pull requests.
- Do not create or switch branches. Do not run `git add`, `git commit`, `git tag`, `git push`, stash, reset, restore, checkout, or history-rewrite commands.
- Read-only Git inspection is allowed. Preserve all pre-existing user changes; stop if they overlap the current task.

## Unreal workflow

- Treat repository code, real class names, Blueprint references, maps, and assets as authoritative over example names in design documents.
- Do not batch-rename the module, reflected classes, assets, or existing interfaces.
- Save and close only ProjectA when a full UHT/C++ build requires it; never interact with another project.
- Report changed files/assets, verification evidence, manual acceptance steps, risks, and a suggested commit message at handoff.
