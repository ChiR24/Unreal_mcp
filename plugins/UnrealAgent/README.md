# Unreal Agent Plugin

Editor-only Unreal plugin for an in-editor OpenCode ACP assistant panel.

The plugin gives the editor a focused OpenCode chat surface and can pass the configured `unreal-engine` MCP server into OpenCode sessions. The goal is not to pretend an autonomous swarm is running. The goal is a practical in-editor game-production assistant that discovers available MCP tools, inspects the current project/editor state, executes through safe tool domains, and verifies the result before claiming success.

## Current Surface

- `Window > Unreal Agent` opens the Slate panel.
- `Connect` starts `opencode acp` in the current Unreal project directory.
- On connect, the plugin writes a managed OpenCode Studio Kit into `.opencode/` and `Saved/UnrealAgent/`.
- Prompts can include a compact redacted editor context envelope with project, map, PIE, selection, dirty-package, and evidence-ledger facts.
- `session/new` receives the current project directory and any configured `unreal-engine` MCP server.
- The model, thinking, and agent selectors are populated from ACP config options when OpenCode advertises them.
- If OpenCode exposes an `unreal-agent` mode, the panel selects it automatically.
- `Send` creates an ACP prompt, attaches editor context when enabled, and streams assistant, thought, tool, plan, permission, and error activity into the panel.
- The cockpit row can refresh context and run a lightweight validation/evidence pass without asking OpenCode to guess.
- `Cancel turn` sends ACP cancellation for the active prompt without killing the process.
- Permission requests can be allowed once, allowed always when ACP exposes that option, or rejected.

## Runtime Prompt

The generated primary prompt is owned by `FUnrealAgentStudioKit::MakePrimaryAgentMarkdown()` and carries version markers:

```yaml
unreal_agent_prompt_version: 2
unreal_agent_studio_kit_version: 1
```

The prompt tells OpenCode to work like a compact Unreal game studio:

- Establish the production stage: concept, prototype, vertical slice, production, polish, release, or live support.
- Inspect before acting. Use `manage_tools` and `inspect` when MCP is connected before claiming facts about assets, actors, levels, Blueprints, settings, tests, logs, screenshots, the viewport, or PIE state.
- Offer 2-4 concrete options for vague or high-impact work, then execute after the user's direction is clear.
- Prefer small, reversible, Unreal-safe changes that fit existing project conventions.
- Validate work through MCP inspection, asset compilation, PIE/editor checks, screenshots, automation tests, logs, profiling, or build output.
- Improve prompts and workflows with a baseline-test, targeted-fix, retest loop.

## Studio Kit

`FUnrealAgentStudioKit` generates managed project-local OpenCode files:

- `.opencode/agents/unreal-agent.md` plus specialist agents for technical direction, gameplay, Blueprints, level/world building, UI/audio/VFX, networking/GAS, and QA/release.
- `.opencode/skills/*/SKILL.md` for MCP tool selection, project bootstrap, prototype, validation, release readiness, and debug/fix loops.
- `.opencode/commands/unreal-*.md` for start, inspect, prototype, validate, ship-check, and fix-errors workflows.
- `.opencode/plugins/unreal-agent-guardrails.ts` for local OpenCode hook guardrails and redaction.
- `.opencode/opencode.json` with conservative default permissions.
- `Saved/UnrealAgent/` for `state.json`, `decisions.md`, and evidence records.

Managed Studio Kit files carry `unreal_agent_studio_kit_version: 1` where the target format permits plugin metadata. `.opencode/opencode.json` uses a JSONC comment marker so it remains upgradeable without adding unknown OpenCode config keys. Existing unmarked user-authored files are preserved.

## Editor Context And Evidence

`FUnrealAgentEditorContext` captures a bounded, redacted prompt envelope. It is a fast starting snapshot, not a replacement for MCP `inspect`; the runtime prompt tells the agent to confirm stale or high-impact facts through MCP before acting.

`FUnrealAgentEvidenceLedger` records validation events under `Saved/UnrealAgent/evidence/` and maintains a compact `state.json` plus `decisions.md`. `FUnrealAgentValidationRunner` checks the Studio Kit, evidence writability, and current editor context, then records an evidence artifact.

## MCP Tool Playbook

The runtime prompt names the MCP tool domains the agent should reach for:

- `manage_tools`: discover canonical tools, enabled categories, and missing capabilities.
- `inspect`: read project, world, actor, Blueprint CDO, class, component, viewport, selection, and runtime facts.
- `manage_asset`, `manage_blueprint`, `control_actor`, `manage_level`, `manage_level_structure`, `build_environment`, `control_editor`: build the playable world, assets, Blueprints, actors, levels, lighting, landscape, viewport, screenshots, and PIE/editor flow.
- `animation_physics`, `manage_character`, `manage_combat`, `manage_ai`, `manage_gas`, `manage_networking`, `manage_inventory`, `manage_interaction`: implement player, combat, abilities, AI, multiplayer, inventory, and interaction systems.
- `manage_audio`, `manage_effect`, `manage_sequence`, `manage_geometry`, `system_control`: author audio, VFX, cinematics, geometry, project settings, profiling, validation, console/Python automation, tests, and build checks.

If a required MCP server or tool is unavailable, the agent should say exactly what is missing and continue from source, config, docs, or logs instead of inventing live editor state.

## Full Game Workflow

For a request like "make a complete game", the agent should work in staged increments:

- **Concept**: clarify genre, player fantasy, core loop, audience, platforms, production constraints, and the smallest fun prototype.
- **Design**: maintain a compact GDD, feature list, input scheme, UX flow, content needs, acceptance criteria, and non-goals.
- **Architecture**: define module ownership, GameMode/GameState/Pawn/Controller/HUD/GameInstance responsibilities, subsystem boundaries, C++ versus Blueprint boundaries, save/load, networking, data assets, and content folder conventions.
- **Prototype**: create the smallest playable loop and verify it in editor/PIE.
- **Vertical slice**: add representative art/audio/UI/VFX/AI/gameplay quality and prove the pipeline.
- **Production**: expand content and systems through the right MCP domains.
- **Polish**: profile, fix bugs, improve UX, tune controls, add accessibility/localization readiness, and remove placeholder content.
- **Release**: verify packaging readiness, platform settings, scalability, input, save migration, logs, crash risk, source-control state, documentation, changelog, and known issues.

The Claude-Code-Game-Studios repo is useful as a reference for studio structure, specialist coverage, stage gates, QA sign-off, release checklists, and improvement loops. Do not copy its Claude Code mechanics directly; translate useful habits into OpenCode ACP plus `unreal-engine` MCP behavior.

## Built-In Quick Prompts

- **Architecture review**: reviews shippable-game architecture, ownership boundaries, content conventions, and risks.
- **Gameplay plan**: turns a concept into prototype, vertical-slice, production, polish, and release-readiness phases mapped to MCP tool domains.
- **QA risk pass**: defines ship-readiness criteria, regression risks, deterministic verification, and release blockers.
- **Editor tooling**: designs or executes reversible MCP-backed automation workflows with explicit validation.

Quick prompts may use source, config, docs, and logs. They may use live editor state only when OpenCode is configured with the `unreal-engine` MCP server.

## OpenCode Resolution

Executable lookup order:

1. Absolute `OPENCODE_ACP_COMMAND`
2. `~/.opencode/bin/opencode`
3. Absolute `PATH` entries outside the current project directory

Project-relative executables are intentionally rejected.

## MCP Configuration

The panel does not expose MCP tools itself. It injects the bridge's native MCP endpoint into `session/new` when Automation Bridge settings enable native MCP:

- Settings section: `/Script/McpAutomationBridge.McpAutomationBridgeSettings`
- Required flag: `bEnableNativeMCP=true`
- URL shape: `http://<ListenHost>:<NativeMCPPort>/mcp`
- Server name: `unreal-engine`
- Capability token header: `X-MCP-Capability-Token` when `bRequireCapabilityToken=true`

Non-loopback and token policy live in `plugins/McpAutomationBridge`; keep this panel as the ACP client and UI layer.

## File Map

| Task | Location |
| --- | --- |
| Register/open panel | `Source/UnrealAgent/Private/UnrealAgentModule.cpp` |
| ACP process, JSON-RPC, context attachment, MCP injection | `Source/UnrealAgent/Private/Acp/McpOpenCodeAcpClient.*` |
| Studio Kit generation, context, evidence, validation | `Source/UnrealAgent/Private/Acp/UnrealAgent*.*` |
| Slate panel, quick prompts, model/agent menus, transcript | `Source/UnrealAgent/Private/UI/SUnrealAgentPanel.*` |
| Automation coverage | `Source/UnrealAgent/Private/UnrealAgentAutomationTests.cpp` |
| Module deps | `Source/UnrealAgent/UnrealAgent.Build.cs` |
| Packaging filter | `Config/FilterPlugin.ini` |

## Transcript Rules

- Raw JSON-RPC frames are not shown as chat rows.
- Normal process stdout/stderr noise is not shown as chat rows.
- Startup, timeout, and exit failures show actionable diagnostics.
- Tool/status events are grouped as activity rows.
- Stable Slate tags prefixed `UnrealAgent.*` are part of the automation-test contract.

## Verification

Compile the plugin module:

```bash
/data/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh UnrealEditor Linux Development -Plugin="/data/GitHub/Unreal_mcp_main/plugins/UnrealAgent/UnrealAgent.uplugin" -NoHotReloadFromIDE
```

Package the plugin:

```bash
/data/UnrealEngine/Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="/data/GitHub/Unreal_mcp_main/plugins/UnrealAgent/UnrealAgent.uplugin" -Package="/tmp/opencode/UnrealAgentPackage-final" -TargetPlatforms=Linux -Rocket -WaitForUATMutex
```

Run ACP automation tests from a host project with the plugin enabled:

```bash
/data/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd "/path/to/HostProject.uproject" -nosplash -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests UnrealAgent.Acp" -TestExit="Automation Test Queue Empty" -ReportExportPath="/tmp/opencode/unreal-agent-report-final"
```

Expected report: `/tmp/opencode/unreal-agent-report-final/index.json` with `UnrealAgent.Acp.ClientProtocol`, `UnrealAgent.Acp.PanelOpens`, and `UnrealAgent.Acp.StudioKitAndContext` passing.

## Safety

- Do not claim live editor state without MCP output.
- Do not hide destructive or bulk operations behind broad prompts.
- Do not spawn project-relative executables or trust `PATH` entries inside the current project.
- Do not block Slate/UI code on ACP process IO.
- Do not move protocol parsing into UI layout code.
- Do not edit generated `Binaries/`, `Intermediate/`, `Saved/`, packaged zips, or temporary host projects.
