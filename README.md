# Unreal Engine MCP Server

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![NPM Package](https://img.shields.io/npm/v/unreal-engine-mcp-server)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![MCP SDK](https://img.shields.io/badge/MCP%20SDK-TypeScript-blue)](https://github.com/modelcontextprotocol/sdk)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.6-orange)](https://www.unrealengine.com/)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-Published-green)](https://registry.modelcontextprotocol.io/)

A comprehensive Model Context Protocol (MCP) server that enables AI assistants to control Unreal Engine through the Automation Bridge plugin and Python subsystem APIs. Built with TypeScript and designed for game development automation.

## Features

### Core Capabilities
- **Asset Management** - Browse, import, and create materials
- **Actor Control** - Spawn, delete, and manipulate actors with physics
- **Editor Control** - PIE sessions, camera, and viewport management
- **Level Management** - Load/save levels, lighting, and environment building
- **Animation & Physics** - Blueprints, state machines, ragdolls, constraints, vehicle setup
- **Visual Effects** - Niagara particles, GPU simulations, procedural effects
- **Sequencer** - Cinematics, camera animations, and timeline control
- **Console Commands** - Safe execution with dangerous command filtering

## Quick Start

### Prerequisites
- Node.js 18+
- Unreal Engine 5.0-5.6
- Required UE Plugins (enable via **Edit ▸ Plugins**):
  - **MCP Automation Bridge** – WebSocket automation transport (ships inside `Public/McpAutomationBridge`)
  - **Python Editor Script Plugin** – exposes Python runtime for automation
  - **Editor Scripting Utilities** – unlocks Editor Actor/Asset subsystems used throughout the tools
  - **Remote Control API** – required for Remote Control preset tooling
  - **Sequencer** *(built-in)* – keep enabled for cinematic tools
  - **Level Sequence Editor** – required for `manage_sequence` operations

> 💡 After toggling any plugin, restart the editor to finalize activation. Keep `Editor Scripting Utilities` and `Python Editor Script Plugin` enabled prior to connecting, otherwise many subsystem-based tools (actor spawning, audio, foliage, UI widgets) will refuse to run for safety.

### Plugin feature map

| Plugin | Location | Used By | Notes |
|--------|----------|---------|-------|
| MCP Automation Bridge | Project Plugins ▸ MCP Automation Bridge | All transport, console, Python tools | Primary automation transport consumed by the MCP server |
| Python Editor Script Plugin | Scripting | Landscapes, lighting, audio, physics, sequences, UI | Required for every Python execution path |
| Editor Scripting Utilities | Scripting | Actors, foliage, assets, landscapes, UI | Supplies Editor Actor/Asset subsystems in UE5.6 |
| Remote Control API | Developer Tools ▸ Remote Control | `manage_rc` tools | Required for Remote Control preset workflows |
| Sequencer | Built-in | Sequencer tools | Ensure not disabled in project settings |
| Level Sequence Editor | Animation | Sequencer tools | Activate before calling `manage_sequence` operations |

> Tools such as `physics.configureVehicle` accept an optional `pluginDependencies` array so you can list the specific Unreal plugins your project relies on (for example, Chaos Vehicles or third-party vehicle frameworks). When provided, the server will verify those plugins are active before running the configuration.

### Optional MCP Automation Bridge plugin
- Location: `Public/McpAutomationBridge`
- Installation: copy the folder into your project's `Plugins/` directory and regenerate project files.
- Sync helper: run `npm run automation:sync -- --engine "X:/Unreal_Engine/UE_5.6/Engine/Plugins" --project "X:/Newfolder(2)/Game/Unreal/Trial/Plugins" --clean-engine --clean-project` after repo updates to copy the latest bridge build into both plugin folders and strip legacy entries (such as `SupportedTargetPlatforms: ["Editor"]`) that trigger startup warnings.
- Verification: run `npm run automation:verify -- --project "C:/Path/To/YourProject/Plugins" --config "C:/Path/To/YourProject/Config/DefaultEngine.ini"` to confirm the plugin files and automation bridge environment variables are in place before launching Unreal.
- Configuration: enable **MCP Automation Bridge** in **Edit ▸ Plugins**, restart the editor, then set the endpoint/token under **Edit ▸ Project Settings ▸ Plugins ▸ MCP Automation Bridge**. The bridge ships with its own lightweight WebSocket client, so you no longer need the engine’s WebSockets plugin enabled.
- Startup: after configuration, the Output Log should show a successful connection and the `bridge_started` broadcast; `SendRawMessage` becomes available to Blueprint and C++ callers for manual testing.
- Current scope: manages a WebSocket session to the Node MCP server (`ws://127.0.0.1:8090` by default), performs optional capability-token handshakes, dispatches inbound JSON to the subsystem delegate, implements reconnect backoff, and now responds to `execute_editor_python`, `get_object_property`, and `set_object_property` automation requests.
- Usage: the consolidated tools require the automation bridge for Python execution and property access. Keep the plugin enabled for all supported workflows.
- Diagnostics: the `ue://automation-bridge` MCP resource surfaces handshake timestamps, recent disconnects, pending automation requests, and whether the Node listener is running—handy when validating editor connectivity from a client.
- Roadmap: expand the bridge with the elevated actions defined in `docs/editor-plugin-extension.md` (SCS authoring, typed property marshaling, modal mediation, asset workflows).

### Installation

#### Option 1: NPM Package (Recommended)

```bash
# Install globally
npm install -g unreal-engine-mcp-server

# Or install locally in your project
npm install unreal-engine-mcp-server
```

#### Option 2: Clone and Build

```bash
# Clone the repository
git clone https://github.com/ChiR24/Unreal_mcp.git
cd Unreal_mcp

# Install dependencies and build
npm install
npm run build

# Run directly
node dist/cli.js
```

### Unreal Engine Configuration

Add to your project's `Config/DefaultEngine.ini`:

```ini
[/Script/PythonScriptPlugin.PythonScriptPluginSettings]
bRemoteExecution=True
bAllowRemotePythonExecution=True

[/Script/RemoteControl.RemoteControlSettings]
bAllowRemoteExecutionOfConsoleCommands=True
bEnableRemoteExecution=True
bAllowPythonExecution=True
```

Then enable Python execution in: Edit > Project Settings > Plugins > Remote Control

## MCP Client Configuration

### Claude Desktop / Cursor

#### For NPM Installation (Local)

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "npx",
      "args": ["unreal-engine-mcp-server"],
      "env": {
        "UE_HOST": "127.0.0.1",
        "UE_PROJECT_PATH": "C:/Users/YourName/Documents/Unreal Projects/YourProject",
        "MCP_AUTOMATION_WS_PORT": "8090"
      }
    }
  }
}
```

#### For Clone/Build Installation

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "node",
      "args": ["path/to/Unreal_mcp/dist/cli.js"],
      "env": {
        "UE_HOST": "127.0.0.1",
        "UE_PROJECT_PATH": "C:/Users/YourName/Documents/Unreal Projects/YourProject",
        "MCP_AUTOMATION_WS_PORT": "8090"
      }
    }
  }
}
```

## Available Tools (14)

| Tool | Description |
|------|-------------|
| `manage_asset` | List, create materials, import assets |
| `control_actor` | Spawn, delete actors, apply physics |
| `control_editor` | PIE control, camera, view modes |
| `manage_level` | Load/save levels, lighting |
| `animation_physics` | Animation blueprints, ragdolls, vehicle setup |
| `create_effect` | Particles, Niagara, debug shapes |
| `manage_blueprint` | Create blueprints, add components, edit defaults |
| `build_environment` | Landscapes, terrain, foliage |
| `system_control` | Profiling, quality, UI, screenshots |
| `console_command` | Direct console command execution |
| `execute_python` | Run bespoke Python scripts or bridge templates |
| `manage_rc` | Remote Control presets |
| `manage_sequence` | Sequencer/cinematics |
| `inspect` | Object introspection |

## Key Features

- **Automation Bridge Transport** - All console and Python execution routes through the MCP Automation Bridge plugin
- **Graceful Degradation** - Server starts even without an active Unreal connection
- **On-Demand Connection** - Retries automation handshakes with backoff instead of persistent polling
- **Non-Intrusive Health Checks** - Uses echo commands every 30 seconds while connected
- **Command Safety** - Blocks dangerous console commands
- **Input Flexibility** - Vectors/rotators accept object or array format
- **Asset Caching** - 10-second TTL for improved performance

## Supported Asset Types

Blueprints, Materials, Textures, Static/Skeletal Meshes, Levels, Sounds, Particles, Niagara Systems

## Example Console Commands

- **Statistics**: `stat fps`, `stat gpu`, `stat memory`
- **View Modes**: `viewmode wireframe`, `viewmode unlit`
- **Gameplay**: `slomo 0.5`, `god`, `fly`
- **Rendering**: `r.screenpercentage 50`, `r.vsync 0`

## Configuration

### Environment Variables

```env
UE_HOST=127.0.0.1              # Unreal Engine host
UE_PROJECT_PATH="C:/Users/YourName/Documents/Unreal Projects/YourProject"  # Absolute path to your .uproject file
LOG_LEVEL=info                 # debug | info | warn | error
MCP_AUTOMATION_WS_HOST=127.0.0.1     # (Optional) Host interface for the automation bridge WebSocket server
MCP_AUTOMATION_WS_PORT=8090          # (Optional) Port for the automation bridge WebSocket server
MCP_AUTOMATION_WS_PORTS=8090,8091    # (Optional) Comma-separated list of ports to listen on simultaneously
MCP_AUTOMATION_WS_PROTOCOLS=mcp-automation # (Optional) Preferred WebSocket subprotocols, comma-separated
MCP_AUTOMATION_CAPABILITY_TOKEN=     # (Optional) Capability token the editor plugin must echo back during handshake
MCP_AUTOMATION_BRIDGE_ENABLED=true   # Set to false to disable the automation bridge listener entirely
```

### Docker

```bash
docker build -t unreal-mcp .
docker run -it --rm unreal-mcp
```

Pull from Docker Hub

```bash
docker pull mcp/server/unreal-engine-mcp-server:latest
docker run --rm -it mcp/server/unreal-engine-mcp-server:latest
```

## Development

```bash
npm run build          # Build TypeScript
npm run lint           # Run ESLint
npm run lint:fix       # Fix linting issues
```

## Contributing

Contributions welcome! Please:
- Include reproduction steps for bugs
- Keep PRs focused and small
- Follow existing code style

## License

MIT - See [LICENSE](LICENSE) file
