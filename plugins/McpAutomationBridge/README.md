# MCP Automation Bridge

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.8-orange)](https://www.unrealengine.com/)
[![GitHub](https://img.shields.io/badge/GitHub-ChiR24/Unreal__mcp-blueviolet?logo=github)](https://github.com/ChiR24/Unreal_mcp)

An Unreal Editor plugin that lets AI assistants (Claude, Cursor, VS Code, Windsurf and any other MCP client) drive the editor through the Model Context Protocol. It exposes **one** MCP tool, `unreal`, that reaches nearly 400 editor capabilities, and it does all the work itself, on the editor's game thread.

Clients reach it in one of two ways:

- **Native HTTP:** the plugin's own Streamable HTTP server at `http://127.0.0.1:3000/mcp`. No Node.js.
- **stdio:** the `unreal-engine-mcp-server` Node.js package, which talks to the plugin over a WebSocket on `127.0.0.1:8090`.

📖 Full documentation: the [wiki](https://github.com/ChiR24/Unreal_mcp/wiki), starting with [Quick Start](https://github.com/ChiR24/Unreal_mcp/wiki/Quick-Start).

---

## Features

| Area | Capabilities |
|------|-------------|
| **Levels and actors** | Spawn, place, attach and inspect actors, in batches; load, stream and save levels |
| **Blueprints and UI** | Blueprints, variables, components and whole event graphs; UMG widgets with preview images |
| **Materials and worlds** | Material graphs and instances, textures, lighting, landscapes, foliage, Niagara, PCG |
| **Gameplay** | Characters, animation, Control Rig, Gameplay Ability System, AI (Behavior Trees, State Trees, EQS), Enhanced Input, networking |
| **Cinematics and audio** | Level Sequences, cameras, Movie Render Queue, Take Recorder, Sound Cues, MetaSounds |
| **Play and verify** | Play-In-Editor with synthetic input, screenshots, logs, profiling, Python, packaging |

The full list is the generated [Action Reference](https://github.com/ChiR24/Unreal_mcp/blob/dev/docs/action-reference.generated.md).

---

## Requirements

- **Unreal Engine** 5.0 to 5.8
- **Platforms:** Win64, macOS and Linux editors. The plugin is editor-only and never ships in a packaged game.
- **A project with C++ code**, so the plugin can compile. Blueprint-only projects can use [prebuilt binaries](https://github.com/ChiR24/Unreal_mcp/wiki/Installation#option-c-prebuilt-binaries).
- **Node.js 20.19+**, only for the stdio route

---

## Installation

1. Copy this `McpAutomationBridge` folder into your project:

   ```text
   YourProject/Plugins/McpAutomationBridge/
   ```

   Using a clone of the repository instead? You can reference the plugin in place, so `git pull` updates it, by adding the repository's `plugins` folder to your `.uproject`:

   ```json
   "AdditionalPluginDirectories": ["C:/Path/To/Unreal_mcp/plugins"]
   ```

2. Open the project. When Unreal asks to rebuild the missing modules, answer **Yes**. If it says *"Engine modules cannot be compiled at runtime"*, generate project files (right-click the `.uproject`), build the Editor target once in Visual Studio, Rider or Xcode, and reopen.
3. Check the status bar at the bottom-right of the level editor: **`MCP off`** means the plugin is loaded, with the native HTTP server off.

The plugin declares the engine plugins it uses, so Unreal enables them with it:

- **Always enabled:** Python Editor Script Plugin, Editor Scripting Utilities, Niagara, Gameplay Abilities, Smart Objects
- **Optional**, used when present: Sequencer, Movie Render Queue, Takes, Control Rig, IK Rig, Niagara Editor, Behavior Tree and EQS editors, StateTree, MetaSound, Enhanced Input, Geometry Scripting, PCG, Interchange, Online Subsystem, Fab and others. The full table is in [Installation](https://github.com/ChiR24/Unreal_mcp/wiki/Installation#engine-plugins-it-uses).

A capability whose engine plugin is missing returns an error; everything else keeps working.

---

## Connect a client

### Route A: native HTTP (no Node.js)

1. In **Edit › Project Settings › Plugins › MCP Automation Bridge**, tick **Enable Native MCP Server** (port `3000` by default) and restart the editor. The status bar now reads **`MCP :3000 (0)`**.
2. Read the capability token the plugin generated at `<YourProject>/Saved/MCP/capability-token`. Treat it like a password.
3. Add the server to your client, sending the token in the `X-MCP-Capability-Token` header.

   **Claude Code:**

   ```bash
   claude mcp add --transport http unreal-engine http://127.0.0.1:3000/mcp --header "X-MCP-Capability-Token: <token>"
   ```

   **Cursor** (`.cursor/mcp.json`):

   ```json
   {
     "mcpServers": {
       "unreal-engine": {
         "url": "http://127.0.0.1:3000/mcp",
         "headers": { "X-MCP-Capability-Token": "<token>" }
       }
     }
   }
   ```

   Other clients: [Connecting Clients](https://github.com/ChiR24/Unreal_mcp/wiki/Connecting-Clients).

4. When the client connects, the count in the status bar goes up: **`MCP :3000 (1)`**.

### Route B: stdio (Node.js)

Add this to your client's MCP configuration, for example Claude Desktop's `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "npx",
      "args": ["-y", "unreal-engine-mcp-server@beta"],
      "env": {
        "UE_PROJECT_PATH": "C:/Path/To/YourProject"
      }
    }
  }
}
```

`UE_PROJECT_PATH` (the project folder or its `.uproject`) is how the server finds the capability token and the WebSocket port. The npm package and this plugin must come from the same release; the `@beta` tag is the 0.6 line.

### Try it

With the editor open, ask your assistant to *"list the actors in the current level"* or *"spawn a point light 300 units above the origin"*.

---

## The `unreal` tool

Both routes expose the same single tool with four operations: `search` finds capabilities from a few plain words, `describe` returns one capability's exact contract, `execute` runs it, and `configure` enables or disables groups of internal tools. Only the contract the model is about to use gets loaded, instead of hundreds of tool schemas. A direct `tools/call` to one of the 0.5 tool names returns `DIRECT_TOOL_CALL_REMOVED` with a `nextCall` that runs the same request through `unreal`.

Native HTTP negotiates MCP protocol versions `2025-11-25`, `2025-06-18` and `2025-03-26`; the stdio server also accepts `2024-11-05` and `2024-10-07`. Details: [Using the Gateway](https://github.com/ChiR24/Unreal_mcp/wiki/Using-the-Gateway) and [protocol.md](https://github.com/ChiR24/Unreal_mcp/blob/dev/docs/protocol.md).

---

## Configuration

### Plugin settings

**Edit › Project Settings › Plugins › MCP Automation Bridge**, saved in `Config/DefaultGame.ini`. Restart the editor after changing connection or security settings.

| Setting | Default | |
|---------|---------|---|
| **Enable Native MCP Server** | off | Serve MCP over Streamable HTTP at `/mcp` |
| **Native MCP Port** | `3000` | Must differ from the WebSocket ports. The `MCP_NATIVE_PORT` environment variable of the editor process overrides it, for example to run several editors at once. |
| **Load All Tools on Start** | on | When off, only the `core` internal tools start enabled; `configure` can enable the rest |
| **Server Instructions** | empty | Text appended to the instructions native clients receive when they connect |
| **Listen Host** | `127.0.0.1` | Bind address for both listeners |
| **Listen Ports** | `8090,8091` | WebSocket ports; the stdio server dials the first |
| **Require Capability Token** | on | Both routes refuse clients without the token |
| **Capability Token** | empty | Your own token. When empty, the plugin generates one in `Saved/MCP/capability-token`. |
| **Allow Non Loopback** | off | Let **Listen Host** be a LAN address |
| **Enable TLS** | off | `wss://` for the WebSocket listener, from PEM certificate and key files |
| **Max Client Requests / Tool Calls Per Minute** | `600` / `120` | Native HTTP limits per session; `0` disables |
| **Max Messages / Automation Requests Per Minute** | `0` (off) | WebSocket limits per client |

Every setting, including the Movie Render Queue and Take Recorder limits: [Configuration](https://github.com/ChiR24/Unreal_mcp/wiki/Configuration).

### stdio server environment variables

| Variable | Default | |
|----------|---------|---|
| `UE_PROJECT_PATH` | unset | Project folder or `.uproject`; used to find the token and the port |
| `MCP_AUTOMATION_PORT` | the first **Listen Ports** entry, else `8090` | Editor WebSocket port |
| `MCP_AUTOMATION_HOST` | `127.0.0.1` | A LAN address also needs `MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true` |
| `MCP_AUTOMATION_CAPABILITY_TOKEN` | read from the token file | Token to present, when the server can't read the project folder |
| `LOG_LEVEL` | `info` | `debug`, `info`, `warn` or `error`; logs go to stderr |

---

## Security

- **Loopback by default.** Both listeners, the WebSocket bridge and the native HTTP server, bind `127.0.0.1`. A LAN address needs **Allow Non Loopback**, and neither listener binds off-loopback unless **Require Capability Token** is also on.
- **Capability token, on by default.** The plugin generates a random 32-byte token per project at `<Project>/Saved/MCP/capability-token` and compares it in constant time on both routes. A token typed into **Capability Token** overrides the file. Delete the file and restart the editor to rotate it.
- **Consent for destructive work.** Deletes and some other writes need a per-call consent grant, which the plugin checks itself.
- **Guard rails.** Asset paths are limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara` and configured prefixes; console commands that chain or quit the editor are blocked; file paths are checked for traversal and symbolic links; the plugin's own settings are out of reach of automation.
- **TLS** for the WebSocket listener, and **rate limits** on both routes.

Local file paths are checked for traversal and symbolic-link components when accepted and again immediately before media open or render start. The trusted boundary is the editor's operating-system user: a process that can change that user's project files is already able to alter editor inputs, and is outside the threat model of a remote client.

More: [Security](https://github.com/ChiR24/Unreal_mcp/wiki/Security). Report vulnerabilities privately through [GitHub security advisories](https://github.com/ChiR24/Unreal_mcp/security/advisories/new).

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| *"Plugin 'McpAutomationBridge' failed to load"* on the very first open | Close the editor and open the project again. It loads once the first build has finished. |
| *"Engine modules cannot be compiled at runtime"* | Build the Editor target once in your IDE, then reopen the project |
| Status bar reads **`MCP off`** | The native HTTP server is off: tick **Enable Native MCP Server** and restart the editor |
| **401** from the native server | The `X-MCP-Capability-Token` header is missing or wrong. Copy `Saved/MCP/capability-token` again, without a trailing newline. |
| stdio server reports *not connected* | Keep the editor open with the plugin loaded, point `UE_PROJECT_PATH` at the project, and use the same release for the npm package and the plugin |
| Build errors after an update | Close the editor, delete the plugin's `Binaries/` and `Intermediate/` folders, regenerate project files and rebuild |

More cases: [Troubleshooting](https://github.com/ChiR24/Unreal_mcp/wiki/Troubleshooting).

### Python execution crash recovery

`execute_python` writes temporary files to `<Project>/Saved/Temp/MCP_Python/` for each run:

- `mcp_exec_<executionId>.py`: the generated wrapper script
- `code_<executionId>.py`: the submitted code (inline `code` parameter only)
- `output_<executionId>.txt`, `error_<executionId>.txt`, `status_<executionId>.txt`: the captured streams

They are removed after a normal run. If the editor dies during a run (for example an engine assertion in native code called from Python), they stay behind. To find the script that was running, look in the editor log for the line written **before** execution starts:

```text
LogMcpAutomationBridgeSubsystem: execute_python begin: executionId=<guid> requestId=<id> origin=WebSocket mode=ExecuteFile scope=Private codeSha256=<sha256> codePath=<path> wrapperPath=<path>
```

`executionId` matches the leftover file names, and `codeSha256` identifies the code without logging it. Delete the leftovers from `Saved/Temp/MCP_Python/` once you're done.

---

## Fab Technical Information

Tools & Plugins
---------------------
- **Features:** Editor automation bridge for Model Context Protocol clients. Includes a native MCP Streamable HTTP server, a WebSocket bridge for the optional Node.js stdio server, a single gateway tool with search, describe, execute and configure, asset/actor/editor/level automation, Blueprint and graph authoring, Niagara/material/audio/AI/PCG/Sequencer helpers, project and system controls, and security settings for loopback binding, capability tokens, consent, TLS and rate limits.
- **Code Modules:** `McpAutomationBridge` - Editor module; `McpAutomationBridgeFab` - Editor module (delay-loaded Fab asset-store adapter, optional).
- **Number of Blueprints:** 0.
- **Network Replicated:** No. This is an editor-only automation and MCP transport plugin; it does not add gameplay replication.
- **Supported Development Platforms:** Windows: Yes. Mac: Yes. Linux: Yes.
- **Supported Target Build Platforms:** Editor-only plugin for Win64, Mac, and Linux editor targets. It is not intended to be included in packaged game runtime builds.
- **Documentation Link:** https://github.com/ChiR24/Unreal_mcp/tree/main/plugins/McpAutomationBridge#readme
- **Example Project:** Not included. The plugin can be enabled in any Unreal Engine C++ project; see the documentation link for setup steps.
- **Important/Additional Notes:** Requires Unreal Engine 5.0-5.8. Required engine plugins are `PythonScriptPlugin`, `EditorScriptingUtilities`, `Niagara`, `GameplayAbilities`, and `SmartObjects`. Other integration references are enabled but marked optional so compatible installed engine plugins can support their matching features without becoming hard distribution dependencies. These integrations include `LevelSequenceEditor`, `MovieRenderPipeline`, `MoviePipelineMaskRenderPass`, `Takes`, `ElectraPlayer`, `NiagaraEditor`, `BehaviorTreeEditor`, `EnvironmentQueryEditor`, `ControlRig`, `RigVM`, `IKRig`, `ChaosVehiclesPlugin`, `AnimationData`, `ProceduralMeshComponent`, `Interchange`, `InterchangeOpenUSD`, `DataValidation`, `EnhancedInput`, `GeometryScripting`, `GeometryProcessing`, `ChaosCloth`, `StructUtils`, `Metasound`, `StateTree`, `MassGameplay`, `OnlineSubsystem`, `OnlineSubsystemUtils`, `Synthesis`, `PCG`, `Fab`, and `Bridge`. The native MCP server does not require Node.js. The optional stdio route uses the separately distributed `unreal-engine-mcp-server` Node.js package.

---

## Documentation

- **Wiki:** [github.com/ChiR24/Unreal_mcp/wiki](https://github.com/ChiR24/Unreal_mcp/wiki)
- **Repository README:** [github.com/ChiR24/Unreal_mcp](https://github.com/ChiR24/Unreal_mcp#readme)
- **Action Reference:** [docs/action-reference.generated.md](https://github.com/ChiR24/Unreal_mcp/blob/dev/docs/action-reference.generated.md)
- **Plugin changelog:** [CHANGELOG.md](CHANGELOG.md)

---

## Support

- **Issues**: [GitHub Issues](https://github.com/ChiR24/Unreal_mcp/issues)
- **Discussions**: [GitHub Discussions](https://github.com/ChiR24/Unreal_mcp/discussions)
- **Roadmap**: [Project Board](https://github.com/users/ChiR24/projects/3)

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

## Contributing

Contributions are welcome! Please:
- Include reproduction steps for bugs
- Keep PRs focused and small
- Follow existing code style
