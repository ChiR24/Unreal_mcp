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
        |-- UnrealAgentModule.cpp          # Window menu + Level Editor tab spawner
        |-- UnrealAgentAutomationTests.cpp # Slate/ACP smoke tests
        |-- Acp/                          # OpenCode ACP process + JSON-RPC client
        `-- UI/                           # Slate chat panel
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Register/open panel | `Source/UnrealAgent/Private/UnrealAgentModule.cpp` | `Window > Unreal Agent`, `UnrealAgent` tab spawner |
| Change ACP protocol/process | `Source/UnrealAgent/Private/Acp/` | See nested AGENTS for `opencode acp`, Studio Kit, context, evidence |
| Change Slate panel | `Source/UnrealAgent/Private/UI/` | See nested AGENTS for widget tags and transcript rules |
| Change module deps | `Source/UnrealAgent/UnrealAgent.Build.cs` | Keep editor-only Slate/Json/ToolMenus deps scoped here |
| Verify UI/protocol | `Source/UnrealAgent/Private/UnrealAgentAutomationTests.cpp` | Test names under `UnrealAgent.Acp.*` |
| User-facing docs | `README.md` | Keep runtime prompt, MCP playbook, quick prompts, and verification guidance in one place |

## CONVENTIONS
- Connect starts `opencode acp` in the current Unreal project directory.
- Executable lookup order: absolute `OPENCODE_ACP_COMMAND`, `~/.opencode/bin/opencode`, then absolute PATH entries outside the project directory.
- Built-in quick prompts are production-workflow prompts. They may use source/config/docs/log context, and they may use live editor state only when OpenCode is configured with the `unreal-engine` MCP server.
- `FUnrealAgentStudioKit` owns generated `.opencode/` agents, skills, commands, plugin hooks, config, and `Saved/UnrealAgent/` evidence scaffolding. Keep it aligned with the MCP tool surface and `README.md`.
- `FUnrealAgentEditorContext` owns the redacted editor context envelope attached to prompts by default; treat it as a starting snapshot and confirm high-impact facts with MCP `inspect`.
- `FUnrealAgentEvidenceLedger` and `FUnrealAgentValidationRunner` own local evidence and validation status. Keep them lightweight enough for panel use.
- Model and agent selectors are populated from ACP `session/new` config options.
- Permission requests are resolved from the panel: allow once, allow always when ACP offers it, or reject.
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
/data/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh UnrealEditor Linux Development -Plugin="/data/GitHub/Unreal_mcp_main/plugins/UnrealAgent/UnrealAgent.uplugin" -NoHotReloadFromIDE
/data/UnrealEngine/Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="/data/GitHub/Unreal_mcp_main/plugins/UnrealAgent/UnrealAgent.uplugin" -Package="/tmp/opencode/UnrealAgentPackage-final" -TargetPlatforms=Linux -Rocket
/data/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd "/path/to/HostProject.uproject" -nosplash -unattended -nop4 -NullRHI -ExecCmds="Automation RunTests UnrealAgent.Acp" -TestExit="Automation Test Queue Empty" -ReportExportPath="/tmp/opencode/unreal-agent-report-final"
```

## NOTES
- Expected automation report: `/tmp/opencode/unreal-agent-report-final/index.json` with `UnrealAgent.Acp.ClientProtocol`, `UnrealAgent.Acp.PanelOpens`, and `UnrealAgent.Acp.StudioKitAndContext` passing.
- This plugin is experimental/beta per `UnrealAgent.uplugin`; keep user-visible claims narrow.
