# MCP Automation Bridge

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.8-orange)](https://www.unrealengine.com/)
[![GitHub](https://img.shields.io/badge/GitHub-ChiR24/Unreal__mcp-blueviolet?logo=github)](https://github.com/ChiR24/Unreal_mcp)

An Unreal Engine editor plugin that enables AI assistants (Claude, Cursor, Windsurf, etc.) to control Unreal Engine through the Model Context Protocol (MCP).

---

## Features

| Category | Capabilities |
|----------|-------------|
| **Asset Management** | Browse, import, duplicate, rename, delete assets; create materials |
| **Actor Control** | Spawn, delete, transform, physics, tags, components |
| **Editor Control** | PIE sessions, camera, viewport, screenshots, bookmarks |
| **Level Management** | Load/save levels, streaming, lighting |
| **Animation & Physics** | Animation BPs, state machines, ragdolls, vehicles, constraints |
| **Visual Effects** | Niagara particles, GPU simulations, procedural effects |
| **Sequencer** | Cinematics, timeline control, Movie Render Queue, media, Take Recorder, replay |
| **Graph Editing** | Blueprint, Niagara, Material, Behavior Tree graphs |
| **Audio** | Sound cues, audio components, MetaSounds |
| **System** | Console commands, UBT, tests, logs, project settings, Python execution |

---

## Requirements

- **Unreal Engine**: 5.0 - 5.8 — all versions in the range are supported and working.
- **Platforms**: Win64, Mac, Linux
- **Node.js**: 20.19.0+ (only for TypeScript bridge transport — not needed for Native MCP)

---

## Installation

### Method 1: Copy to Project

1. Copy the `McpAutomationBridge` folder to your project's `Plugins/` directory:
   ```
   YourProject/Plugins/McpAutomationBridge/
   ```

2. Regenerate project files:
   - Right-click `.uproject` → "Generate Visual Studio project files"
   - Or run: `GenerateProjectFiles.bat`

3. Open your project in Unreal Editor

4. Enable required plugins in **Edit → Plugins**:

<details>
<summary><b>Core Plugins (Required)</b></summary>

   - ✅ MCP Automation Bridge
   - ✅ Python Editor Script Plugin
   - ✅ Editor Scripting Utilities
   - ✅ Niagara
   - ✅ Gameplay Abilities (for `manage_gas`)
   - ✅ Smart Objects (for AI smart objects)

</details>

<details>
<summary><b>Optional Plugins (Auto-enabled)</b></summary>

   - ✅ Level Sequence Editor (for `manage_sequence`)
   - ✅ Movie Render Pipeline (for `manage_sequence` Movie Render Queue)
   - ✅ Movie Pipeline Mask Render Pass (for the object-ID render pass)
   - ✅ Takes (for `manage_sequence` Take Recorder)
   - ✅ Electra Player (for `manage_sequence` file-backed media playback)
   - ✅ Control Rig (for `animation_physics`)
   - ✅ RigVM (for Control Rig and graph authoring support)
   - ✅ GeometryScripting (for `manage_geometry`)
   - ✅ GeometryProcessing (for geometry processing support)
   - ✅ Behavior Tree Editor (for `manage_ai` Behavior Trees)
   - ✅ Niagara Editor (for Niagara authoring)
   - ✅ MetaSound (for `manage_audio` MetaSounds)
   - ✅ StateTree (for `manage_ai` State Trees)
   - ✅ Enhanced Input (for `manage_networking` input mappings)
   - ✅ Environment Query Editor (for AI/EQS)
   - ✅ Chaos Cloth (for cloth simulation)
   - ✅ Interchange (for asset import/export)
   - ✅ Data Validation (for data validation)
   - ✅ PCG (for `manage_pcg` graph authoring and execution when enabled for the build)
   - ✅ Procedural Mesh Component (for procedural geometry)
   - ✅ OnlineSubsystem (for sessions/networking)
   - ✅ OnlineSubsystemUtils (for sessions/networking)

</details>

   > 💡 Optional plugins are auto-enabled by the MCP Automation Bridge plugin. PCG support is compiled for source projects when the project explicitly enables PCG; versioned release packages for UE 5.2+ include it.

5. Restart the editor

### Method 2: Add in Editor

1. Open Unreal Editor → **Edit → Plugins**
2. Click **"Add"** button
3. Browse to and select the `McpAutomationBridge` folder
4. Enable the plugin and restart

---

## Quick Start

### Option A: Native MCP Transport (no Node.js needed)

The plugin includes a built-in MCP Streamable HTTP server. AI clients connect directly — no TypeScript bridge required.
**Note:** the `bAllowNonLoopback` setting applies to **both** the WebSocket bridge and the native MCP transport. Enabling it binds both surfaces to non-loopback addresses. Capability token auth is on by default (0.5.31+) — the plugin auto-generates a per-install token at `<Project>/Saved/MCP/capability-token` (a manual `CapabilityToken` in Project Settings overrides it), so both transports require authentication automatically.

1. Enable in **Edit → Project Settings → Plugins → MCP Automation Bridge**:
   - Check **Enable Native MCP**
   - Set port (default: `3000`)
2. Restart the editor
3. Configure your AI client for Streamable HTTP at `http://localhost:3000/mcp`

**Claude Code:**
```bash
claude mcp add unreal-engine --transport http http://localhost:3000/mcp
```

**Cursor** (`.cursor/mcp.json`):
```json
{
  "mcpServers": {
    "unreal-engine": {
      "url": "http://localhost:3000/mcp"
    }
  }
}
```

### Native Gateway & Protocol

The native MCP transport permanently exposes a single `unreal` gateway tool on the `/mcp` surface; there is no opt-out and no legacy 23-tool listing to restore. This matches the TypeScript stdio transport, so both transports behave consistently. A direct `tools/call` for a canonical tool name returns a bounded, executable `DIRECT_TOOL_CALL_REMOVED` migration receipt whose `nextCall` re-runs the request through `unreal` (`search` → `describe` → `execute`).

Supports MCP protocol versions `2025-11-25` (latest), `2025-06-18`, and `2025-03-26`, and deliberately excludes the later `2026-07-28` RC. The TypeScript bridge transport additionally accepts the legacy `2024-11-05` and `2024-10-07` versions. See [docs/protocol.md](docs/protocol.md) (or the server README [Gateway Protocol & Transport](https://github.com/ChiR24/Unreal_mcp#gateway-protocol--transport)) for the full negotiation and transport contract.

### Option B: TypeScript Bridge (classic setup)

### Step 1: Install MCP Server

```bash
# Using npx (recommended)
npx unreal-engine-mcp-server

# Or install globally
npm install -g unreal-engine-mcp-server
```

### Step 2: Configure AI Client

Add to your Claude Desktop config (`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "npx",
      "args": ["unreal-engine-mcp-server"],
      "env": {
        "UE_PROJECT_PATH": "C:/Path/To/YourProject"
      }
    }
  }
}
```

### Step 3: Start Automating

1. Open your Unreal project
2. Start your AI client (Claude Desktop, Cursor, etc.)
3. The MCP server will automatically connect to the Automation Bridge

Example prompts:
- "List all assets in /Game/Characters"
- "Spawn a point light at (100, 200, 300)"
- "Create a new material called M_Glow"
- "Take a screenshot of the current viewport"

---

## Fab Technical Information

Tools & Plugins
---------------------
- **Features:** Editor automation bridge for Model Context Protocol clients. Includes native HTTP/SSE MCP transport, WebSocket bridge transport, dynamic MCP tool management, asset/actor/editor/level automation, Blueprint and graph authoring, Niagara/material/audio/AI/PCG/sequencer helpers, project/system controls, and security settings for loopback, TLS, capability tokens, and rate limits.
- **Code Modules:** `McpAutomationBridge` - Editor module.
- **Number of Blueprints:** 0.
- **Network Replicated:** No. This is an editor-only automation and MCP transport plugin; it does not add gameplay replication.
- **Supported Development Platforms:** Windows: Yes. Mac: Yes. Linux: Yes.
- **Supported Target Build Platforms:** Editor-only plugin for Win64, Mac, and Linux editor targets. It is not intended to be included in packaged game runtime builds.
- **Documentation Link:** https://github.com/ChiR24/Unreal_mcp/tree/main/plugins/McpAutomationBridge#readme
- **Example Project:** Not included. The plugin can be enabled in any Unreal Engine C++ project; see the documentation link for setup steps.
- **Important/Additional Notes:** Requires Unreal Engine 5.0-5.8; all versions in the range are supported and working. Required engine plugins are `PythonScriptPlugin`, `EditorScriptingUtilities`, `Niagara`, `GameplayAbilities`, and `SmartObjects`. Other integration references are enabled but marked optional so compatible installed engine plugins can support their matching features without becoming hard distribution dependencies. These integrations include `LevelSequenceEditor`, `MovieRenderPipeline`, `MoviePipelineMaskRenderPass`, `Takes`, `ElectraPlayer`, `NiagaraEditor`, `BehaviorTreeEditor`, `EnvironmentQueryEditor`, `ControlRig`, `RigVM`, `IKRig`, `ChaosVehiclesPlugin`, `AnimationData`, `ProceduralMeshComponent`, `Interchange`, `InterchangeOpenUSD`, `DataValidation`, `EnhancedInput`, `GeometryScripting`, `GeometryProcessing`, `ChaosCloth`, `StructUtils`, `Metasound`, `StateTree`, `MassGameplay`, `OnlineSubsystem`, `OnlineSubsystemUtils`, `Synthesis`, and `PCG`. Native MCP transport does not require Node.js. The optional TypeScript bridge transport uses the separately distributed `unreal-engine-mcp-server` Node.js package.

---

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `UE_PROJECT_PATH` | - | Path to your `.uproject` file |
| `MCP_AUTOMATION_HOST` | `127.0.0.1` | Bridge host address |
| `MCP_AUTOMATION_PORT` | `8091` | Bridge WebSocket port |
| `MCP_NATIVE_PORT` | (`Native MCP Port` setting) | Overrides the native MCP HTTP/SSE port at startup without editing ini — pick a per-editor port (e.g. to run several editors at once). Falls back to the project setting when unset/invalid. |
| `LOG_LEVEL` | `info` | Logging level (debug/info/warn/error) |

### Plugin Settings

Configure in **Edit → Project Settings → Plugins → MCP Automation Bridge**:

- **Listen Ports**: WebSocket ports (default: 8090, 8091)
- **Enable TLS**: Enable secure WebSocket connections
- **Allow Non-Loopback**: Enable LAN access for both the WebSocket bridge listen socket and the native MCP HTTP/SSE transport (requires `Require Capability Token`)
- **Enable Native MCP**: Enable built-in HTTP/SSE MCP server (default: off)
- **Native MCP Port**: HTTP port for native MCP transport (default: 3000; override at startup with the `MCP_NATIVE_PORT` environment variable)
- **Listen Host**: Bind address (default: 127.0.0.1)
- **Load All Tools on Start**: Load all 23 canonical tools at startup (default: on)
- **Native MCP Instructions**: Custom instructions for AI clients
- **Require Capability Token**: Enforce token authentication on WS and HTTP transports (on by default since 0.5.31; the plugin auto-generates `<Project>/Saved/MCP/capability-token` — a manual `CapabilityToken` in Project Settings overrides it)

---

## Security

- **Fail-closed listener binding** — both plugin-owned server-side listeners (the WebSocket bridge listen socket and the native MCP HTTP/SSE transport) bind loopback-first by default. Non-loopback requires explicit `bAllowNonLoopback`; the native transport additionally refuses to bind non-loopback unless `bRequireCapabilityToken` is enabled, so a LAN-exposed surface can never start without auth.
- **WebSocket loopback-only by default**; LAN binding on either surface requires explicit opt-in
- **Capability token authentication** — enforced on both WebSocket and Native MCP transports. On by default since 0.5.31: the plugin auto-generates a 32-byte secret at `<Project>/Saved/MCP/capability-token`; a manual `CapabilityToken` in Project Settings overrides it. Disabling `Require Capability Token` restores the pre-0.5.31 (insecure) default.
- **TLS/SSL support** for the WebSocket transport
- **Rate limiting** support (disabled by default; configurable via Project Settings)
- **Handshake required** before automation requests
- **Command validation** blocks dangerous console commands
- **Path sanitization** — blocks directory traversal in file operations
- **Python execution security** — 1 MB code limit, symlink resolution, temp file scope guard cleanup

Local filesystem paths are checked for traversal and symbolic-link components
when accepted and again immediately before media open or render start. The
trusted boundary is the editor's operating-system user: a process that can
mutate that user's project files concurrently is already able to alter editor
inputs and is outside the transport's remote-client threat model.

---

## Troubleshooting

### Plugin Failed to Load

If you see *"Plugin 'McpAutomationBridge' failed to load"* on first open:
1. Close Unreal Editor
2. Reopen the project
3. The plugin should load correctly

This is a known UE behavior when plugins are rebuilt on first load.

### Connection Refused

1. Verify the plugin is enabled in **Edit → Plugins**
2. Check port 8091 is not blocked by firewall
3. Ensure MCP server is running: `npx unreal-engine-mcp-server`

### Build Errors

The plugin uses `PCHUsageMode.NoPCHs` to prevent memory issues during compilation. If you encounter build errors:
1. Close Visual Studio
2. Delete `Intermediate/`, `Binaries/`, `Saved/` folders
3. Regenerate project files
4. Rebuild

### Python Execution Crash Recovery

`execute_python` writes temporary artifacts to `<Project>/Saved/Temp/MCP_Python/` before each run:

- `mcp_exec_<executionId>.py` — generated wrapper script
- `code_<executionId>.py` — user-provided code (inline `code` parameter only)
- `output_<executionId>.txt`, `error_<executionId>.txt`, `status_<executionId>.txt` — captured streams

On success these files are cleaned up automatically. If the editor is terminated by a fatal error during Python execution (e.g. an Engine-side assertion or access violation in native code called from Python), the cleanup does not run and the temp files remain on disk.

To identify the script that was active when a crash occurred, check the editor log for the `execute_python begin:` line emitted **before** native execution:

```
LogMcpAutomationBridgeSubsystem: execute_python begin: executionId=<guid> requestId=<id> origin=WebSocket mode=ExecuteFile scope=Private codeSha256=<sha256> codePath=<path> wrapperPath=<path>
```

The `executionId` matches the suffix of the leftover `mcp_exec_<executionId>.py` / `code_<executionId>.py` files. The `codeSha256` is the SHA-256 of the executed code bytes, so the active script is identifiable without logging raw source. Crash-leftover temp files can be safely deleted manually from `<Project>/Saved/Temp/MCP_Python/` once diagnosed.

---

## Documentation

- **Full Documentation**: [GitHub README](https://github.com/ChiR24/Unreal_mcp#readme)
- **Handler Mapping**: [docs/handler-mapping.md](https://github.com/ChiR24/Unreal_mcp/blob/main/docs/handler-mapping.md)

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
