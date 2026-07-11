# ProjectA MCP Isolation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Do not dispatch subagents. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bind only ProjectA's Unreal MCP server and Codex client configuration to `127.0.0.1:8001` without inspecting or affecting any other project.

**Architecture:** ProjectA stores a tracked default UE editor setting, a project-local Codex endpoint, and a repository-level `AGENTS.md` policy. The current ProjectA editor and Codex task are restarted once so both sides reload port `8001`; no global Codex configuration and no other process are changed.

**Tech Stack:** Unreal Engine 5.8 Editor Preferences config, Unreal MCP, Codex project config, Markdown repository guidance, PowerShell read-only verification.

## Global Constraints

- Operate only inside `E:\MyWork\ProjectA` and ProjectA-owned processes.
- Never inspect, stop, modify, or reuse any other project's editor, MCP server, files, logs, or processes.
- Port `8000` is outside ProjectA scope; ProjectA uses only `http://127.0.0.1:8001/mcp`.
- Work only on the existing `main` branch.
- Do not run `git add`, `git commit`, `git tag`, `git push`, branch commands, stash, reset, restore, checkout, or PR commands.
- Stop after this isolation setup is verified. Do not start v0.4.8 without a separate explicit user instruction.

---

## File Map

- Create `AGENTS.md`: durable ProjectA-only scope, Git, MCP, and version-gate rules.
- Modify `.codex/config.toml`: point this workspace's Codex MCP client to ProjectA port `8001`.
- Modify `.mcp.json`: point ProjectA's generated JSON MCP client entry to port `8001`.
- Modify `Config/DefaultEditorPerProjectUserSettings.ini`: tracked ProjectA default for MCP auto-start on port `8001`.
- Modify `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`: current local ProjectA editor override, ignored by Git, updated so it does not continue overriding the tracked default with port `8000`.

### Task 1: Install durable ProjectA operating rules

**Files:**
- Create: `AGENTS.md`
- Reference: `docs/superpowers/specs/2026-07-11-projectrift-strict-version-handoff-design.md`

**Interfaces:**
- Consumes: the approved strict-version-handoff specification.
- Produces: repository instructions automatically loaded by future Codex tasks.

- [ ] **Step 1: Recheck scope and worktree without changing Git state**

Run:

```powershell
git branch --show-current
git status --short --untracked-files=all
```

Expected: branch is `main`; existing user changes are preserved and reported before editing.

- [ ] **Step 2: Create `AGENTS.md` with the exact durable rules**

```markdown
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
```

- [ ] **Step 3: Confirm the policy contains every non-negotiable boundary**

Run:

```powershell
rg -n "E:\\MyWork\\ProjectA|8001|8000|main|Do not create or switch branches|Do not plan or begin the next version" AGENTS.md
```

Expected: all six rules are present.

### Task 2: Pin ProjectA's UE and Codex endpoints to port 8001

**Files:**
- Modify: `.codex/config.toml`
- Modify: `.mcp.json`
- Modify: `Config/DefaultEditorPerProjectUserSettings.ini`
- Modify: `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`

**Interfaces:**
- Consumes: ProjectA's enabled `ModelContextProtocol` and `EditorToolset` plugins in `ProjectA.uproject`.
- Produces: one ProjectA-local Streamable HTTP endpoint at `http://127.0.0.1:8001/mcp`.

- [ ] **Step 1: Change only the ProjectA Codex URL**

Replace `.codex/config.toml` with:

```toml
[mcp_servers.unreal-mcp]
url = "http://127.0.0.1:8001/mcp"
enabled = true
startup_timeout_sec = 20
tool_timeout_sec = 120
```

- [ ] **Step 2: Add the tracked ProjectA UE default**

Update `.mcp.json` so its `unreal-mcp.url` is exactly `http://127.0.0.1:8001/mcp`.

Then append this section to `Config/DefaultEditorPerProjectUserSettings.ini`:

```ini
[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]
bAutoStartServer=True
ServerPortNumber=8001
ServerUrlPath=/mcp
```

- [ ] **Step 3: Update the existing ProjectA-local user override**

In `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`, preserve the file and its unrelated settings; change only the existing `[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]` section to:

```ini
[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]
ServerUrlPath=/mcp
ServerPortNumber=8001
bAutoStartServer=True
```

- [ ] **Step 4: Verify no ProjectA MCP config still references port 8000**

Run:

```powershell
rg -n "127\.0\.0\.1:8000|ServerPortNumber=8000" .codex .mcp.json Config Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini
```

Expected: no matches. Do not search outside `E:\MyWork\ProjectA`.

### Task 3: Reload and verify the isolated ProjectA MCP service

**Files:**
- No source changes.
- Read: `Saved/Logs/ProjectA.log`

**Interfaces:**
- Consumes: the port settings from Task 2.
- Produces: verified ProjectA toolsets available to Codex on port `8001`.

- [ ] **Step 1: Ask the user to save ProjectA, then restart only ProjectA and the current Codex task**

Expected: no request is made to close or interact with any other editor or project.

- [ ] **Step 2: Verify ProjectA owns a listener on port 8001**

Run:

```powershell
netstat -ano | Select-String '127.0.0.1:8001'
```

Expected: one `LISTENING` entry. Use the ProjectA process information already visible from ProjectA's own launch context; do not inspect unrelated processes.

- [ ] **Step 3: Call the configured Unreal MCP `list_toolsets` meta-tool**

Expected: the call succeeds and returns ProjectA editor toolsets such as ActorTools, AssetTools, BlueprintTools, DataAssetTools, ObjectTools, and SceneTools.

- [ ] **Step 4: Verify ProjectA's own log evidence**

Run:

```powershell
rg -n "Starting MCP server on port 8001|HttpListener unable to bind|toolsets discoverable" Saved\Logs\ProjectA.log
```

Expected: a successful start on `8001`, registered toolsets, and no bind failure for `8001`.

- [ ] **Step 5: Stop at the isolation handoff gate**

Run:

```powershell
git status --short --untracked-files=all
```

Expected: `AGENTS.md`, the tracked editor default, and planning/spec files may be visible; `.codex` and `Saved` remain ignored. Do not stage or commit. Report verification and wait for the user's separate `开始 v0.4.8` instruction.
