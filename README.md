# Unreal Engine MCP Server

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![NPM Package](https://img.shields.io/npm/v/unreal-engine-mcp-server)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![MCP SDK](https://img.shields.io/badge/MCP%20SDK-TypeScript-blue)](https://github.com/modelcontextprotocol/sdk)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.6-orange)](https://www.unrealengine.com/)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-Published-green)](https://registry.modelcontextprotocol.io/)
[![smithery badge](https://smithery.ai/badge/@ChiR24/unreal_mcp)](https://smithery.ai/server/@ChiR24/unreal_mcp)

A comprehensive Model Context Protocol (MCP) server that enables AI assistants to control Unreal Engine via Remote Control API. Built with TypeScript and designed for game development automation.

## Features

### Core Capabilities
- **Asset Management** - Browse, import, and create materials
- **Actor Control** - Spawn, delete, and manipulate actors with physics
- **Editor Control** - PIE sessions, camera, and viewport management
- **Level Management** - Load/save levels, lighting, and environment building
- **Animation & Physics** - Blueprints, state machines, ragdolls, constraints
- **Visual Effects** - Niagara particles, GPU simulations, procedural effects
- **Sequencer** - Cinematics, camera animations, and timeline control
- **Console Commands** - Safe execution with dangerous command filtering

## Quick Start

### Prerequisites
- Node.js 18+
- Unreal Engine 5.0-5.6
- Required UE Plugins (enable via **Edit ▸ Plugins**):
  - **Remote Control API** – core Remote Control HTTP/WS endpoints
  - **Remote Control Web Interface** – enables WebSocket bridge used by this server
  - **Python Editor Script Plugin** – exposes Python runtime for automation
  - **Editor Scripting Utilities** – unlocks Editor Actor/Asset subsystems used throughout the tools
  - **Sequencer** *(built-in)* – keep enabled for cinematic tools
  - **Level Sequence Editor** – required for `manage_sequence` operations

> 💡 After toggling any plugin, restart the editor to finalize activation. Keep `Editor Scripting Utilities` and `Python Editor Script Plugin` enabled prior to connecting, otherwise many subsystem-based tools (actor spawning, audio, foliage, UI widgets) will refuse to run for safety.

### Plugin feature map

| Plugin | Location | Used By | Notes |
|--------|----------|---------|-------|
| Remote Control API | Developer Tools ▸ Remote Control | All tools | Provides HTTP/WS endpoints consumed by the MCP bridge |
| Remote Control Web Interface | Developer Tools ▸ Remote Control | All tools | Enables persistent WebSocket session |
| Python Editor Script Plugin | Scripting | Landscapes, lighting, audio, physics, sequences, UI | Required for every Python execution path |
| Editor Scripting Utilities | Scripting | Actors, foliage, assets, landscapes, UI | Supplies Editor Actor/Asset subsystems in UE5.6 |
| Sequencer | Built-in | Sequencer tools | Ensure not disabled in project settings |
| Level Sequence Editor | Animation | Sequencer tools | Activate before calling `manage_sequence` operations |

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
        "UE_RC_HTTP_PORT": "30010",
        "UE_RC_WS_PORT": "30020",
        "UE_PROJECT_PATH": "C:/Users/YourName/Documents/Unreal Projects/YourProject"
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
        "UE_RC_HTTP_PORT": "30010",
        "UE_RC_WS_PORT": "30020",
        "UE_PROJECT_PATH": "C:/Users/YourName/Documents/Unreal Projects/YourProject"
      }
    }
  }
}
```

## Available Tools (13)

| Tool | Description |
|------|-------------|
| `manage_asset` | List, create materials, import assets |
| `control_actor` | Spawn, delete actors, apply physics |
| `control_editor` | PIE control, camera, view modes |
| `manage_level` | Load/save levels, lighting |
| `animation_physics` | Animation blueprints, ragdolls |
| `create_effect` | Particles, Niagara, debug shapes |
| `manage_blueprint` | Create blueprints, add components |
| `build_environment` | Landscapes, terrain, foliage |
| `system_control` | Profiling, quality, UI, screenshots |
| `console_command` | Direct console command execution |
| `manage_rc` | Remote Control presets |
| `manage_sequence` | Sequencer/cinematics |
| `inspect` | Object introspection |

## Key Features

- **Graceful Degradation** - Server starts even without UE connection
- **Auto-Reconnection** - Attempts reconnection every 10 seconds
- **Connection Timeout** - 5-second timeout with configurable retries
- **Non-Intrusive Health Checks** - Uses echo commands every 30 seconds
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
UE_RC_HTTP_PORT=30010          # Remote Control HTTP port
UE_RC_WS_PORT=30020            # Remote Control WebSocket port
UE_PROJECT_PATH="C:/Users/YourName/Documents/Unreal Projects/YourProject"  # Absolute path to your .uproject file
LOG_LEVEL=info                 # debug | info | warn | error
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
