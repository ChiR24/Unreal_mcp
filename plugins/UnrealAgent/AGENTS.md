# Plugins/UnrealAgent

Editor-only Unreal plugin for an in-editor OpenCode ACP assistant panel. This is separate from `plugins/McpAutomationBridge`; it does not expose MCP tools itself, but it can pass the configured `unreal-engine` MCP server to OpenCode ACP sessions.

## STRUCTURE
```text
UnrealAgent/
|-- UnrealAgent.uplugin          # plugin metadata, version `0.1.0`
|-- Config/FilterPlugin.ini      # package filter for Studio Kit resources
|-- Resources/OpenCodeStudioKit/ # packaged Studio Kit reference artifact
`-- Source/UnrealAgent/
    |-- UnrealAgent.Build.cs     # Slate/Json/LevelEditor/ToolMenus deps
    `-- Private/
        |-- UnrealAgentModule.cpp # Window menu + Level Editor tab spawner
        |-- Acp/                  # client, Studio Kit, context, evidence, validation
        |-- Tests/                # Slate/ACP automation coverage
        `-- UI/                   # core panel, composer, transcript, history, permissions
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Register/open panel | `Source/UnrealAgent/Private/UnrealAgentModule.cpp` | `Window > Unreal Agent`, `UnrealAgent` tab spawner |
| Change ACP protocol/process | `Source/UnrealAgent/Private/Acp/Client/` | See nested AGENTS for `opencode acp` |
| Change Studio Kit/context/evidence | `Source/UnrealAgent/Private/Acp/` | Use the matching responsibility folder |
| Change Slate panel | `Source/UnrealAgent/Private/UI/` | Use the matching panel responsibility folder |
| Change module deps | `Source/UnrealAgent/UnrealAgent.Build.cs` | Keep editor-only Slate/Json/ToolMenus deps scoped here |
| Verify UI/protocol | `Source/UnrealAgent/Private/Tests/` | Wrapper registrations plus split layout, history, transcript, Studio Kit, and protocol checks |
| User-facing docs | `README.md` | Keep runtime prompt, MCP playbook, quick prompts, and verification guidance in one place |

## CONVENTIONS
- Connect starts `opencode acp` in the current Unreal project directory.
- Executable lookup order: absolute `OPENCODE_ACP_COMMAND`, `~/.opencode/bin/opencode`, then absolute PATH entries outside the project directory.
- Built-in quick prompts are production-workflow prompts. They may use source/config/docs/log context, and they may use live editor state only when OpenCode is configured with the `unreal-engine` MCP server.
- `FUnrealAgentStudioKit` owns generated `.opencode/` agents, skills, commands, plugin hooks, config, and `Saved/UnrealAgent/` evidence scaffolding. Keep it aligned with the MCP tool surface, first-playable/editor-onboarding workflows, and `README.md`.
- `FUnrealAgentEditorContext` owns the redacted editor context envelope attached to prompts by default; treat it as a starting snapshot and confirm high-impact facts with MCP `inspect`.
- `FUnrealAgentEvidenceLedger` and `FUnrealAgentValidationRunner` own local evidence and validation status. Keep validation fast, deterministic, and contract-aware for panel use: generated Studio Kit files, MCP-first guardrails, route cards, surface inventory, mutation safety, runtime validation markers, and evidence scaffolding should be checked without claiming live editor proof.
- Model and agent selectors are populated from ACP `session/new` config options.
- Permission requests are resolved from the panel: allow once, allow always only for non-Unreal requests when ACP offers it, or reject. Unreal MCP and editor-state requests must remain one-shot.
- Startup, timeout, and exit failures show recent diagnostics; normal JSON-RPC/process noise is not a transcript row.
- Widget tags prefixed `UnrealAgent.*` are an automation-test contract, including cockpit tags under `UnrealAgent.Cockpit.*`.

## ANTI-PATTERNS
- Claiming hidden role routing, autonomous swarm behavior, or live editor facts without implemented MCP/tool evidence.
- Spawning project-relative executables or trusting PATH entries inside the current project directory.
- Blocking editor UI code on the ACP process; lifecycle and pipe draining belong in the ACP client.
- Showing raw JSON-RPC/tool/status events as normal chat rows.
- Editing generated `Binaries/`, `Intermediate/`, `Saved/`, packaged zips, or temporary host projects.

## COMMANDS
```bash
npm run test:unreal-agent-guardrails
SESSION="unreal-agent-build-$(date +%Y%m%d-%H%M%S)"; tmux new-session -d -s "$SESSION" "/data/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh UnrealEditor Linux Development -Plugin=/data/GitHub/Unreal_mcp_main/plugins/UnrealAgent/UnrealAgent.uplugin -NoHotReloadFromIDE 2>&1 | tee /tmp/$SESSION.log"
SESSION="unreal-agent-package-$(date +%Y%m%d-%H%M%S)"; tmux new-session -d -s "$SESSION" "/data/UnrealEngine/Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin=/data/GitHub/Unreal_mcp_main/plugins/UnrealAgent/UnrealAgent.uplugin -Package=/tmp/opencode/UnrealAgentPackage-final -TargetPlatforms=Linux -Rocket -WaitForUATMutex 2>&1 | tee /tmp/$SESSION.log"
SESSION="unreal-agent-tests-$(date +%Y%m%d-%H%M%S)"; tmux new-session -d -s "$SESSION" "/data/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd /path/to/HostProject.uproject -nosplash -unattended -nop4 -NullRHI -ExecCmds='Automation RunTests UnrealAgent.Acp' -TestExit='Automation Test Queue Empty' -ReportExportPath=/tmp/opencode/unreal-agent-report-final 2>&1 | tee /tmp/$SESSION.log"
```

## NOTES
- Run every Unreal build, package, test, or editor process in a uniquely named tmux session. Inspect it with `tmux attach-session -t "$SESSION"` while active and keep the captured log as verification evidence.
- Expected automation report: `/tmp/opencode/unreal-agent-report-final/index.json` with `UnrealAgent.Acp.ClientProtocol`, `UnrealAgent.Acp.PanelOpens`, `UnrealAgent.Acp.PermissionSafety`, `UnrealAgent.Acp.PermissionSources`, `UnrealAgent.Acp.PermissionSyntax`, `UnrealAgent.Acp.Security`, `UnrealAgent.Acp.StudioKitAndContext`, and `UnrealAgent.Acp.StudioKitGuardrails` passing.
- This plugin is experimental/beta per `UnrealAgent.uplugin`; keep user-visible claims narrow.
