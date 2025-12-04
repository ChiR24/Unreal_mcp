# Unreal Engine MCP Server

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![NPM Package](https://img.shields.io/npm/v/unreal-engine-mcp-server)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![MCP SDK](https://img.shields.io/badge/MCP%20SDK-TypeScript-blue)](https://github.com/modelcontextprotocol/sdk)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.7-orange)](https://www.unrealengine.com/)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-Published-green)](https://registry.modelcontextprotocol.io/)

A comprehensive Model Context Protocol (MCP) server that enables AI assistants to control Unreal Engine through the native C++ Automation Bridge plugin. Built with TypeScript, C++, and Rust (WebAssembly) for ultra high-performance game development automation.

## Features

### Core Capabilities
- **Asset Management** - Browse, import, and create materials
- **Actor Control** - Spawn, delete, and manipulate actors with physics
- **Editor Control** - PIE sessions, camera, and viewport management
- **Level Management** - Load/save levels, lighting, and environment building
- **Animation & Physics** - Blueprints, state machines, ragdolls, constraints, vehicle setup
- **Visual Effects** - Niagara particles, GPU simulations, procedural effects
- **Sequencer** - Cinematics, camera animations, and timeline control
- **Graph Editing** - Blueprint, Niagara, Material, and Behavior Tree graph manipulation
- **World Partition** - Load cells, manage data layers
- **Render Management** - Render targets, Nanite, Lumen
- **Pipeline & Testing** - Run UBT, automated tests
- **Observability** - Log streaming, gameplay debugger, insights, asset queries
- **Console Commands** - Safe execution with dangerous command filtering
- **GraphQL API** - Flexible data querying for assets, actors, and blueprints

### High-Performance WebAssembly
- **5-8x faster JSON parsing** - WASM-accelerated property parsing
- **5-10x faster transform math** - Optimized 3D vector/matrix operations
- **3-5x faster dependency resolution** - Rapid asset graph traversal
- **Automatic fallbacks** - Works perfectly without WASM binary
- **Performance monitoring** - Built-in metrics tracking

## Quick Start

### Prerequisites
- Node.js 18+
- Unreal Engine 5.0-5.6
- Required UE Plugins (enable via **Edit ▸ Plugins**):
  - **MCP Automation Bridge** – Native C++ WebSocket automation transport (ships inside `plugins/McpAutomationBridge`)
  - **Editor Scripting Utilities** – Unlocks Editor Actor/Asset subsystems for native tool operations
  - **Sequencer** *(built-in)* – Keep enabled for cinematic tools
  - **Level Sequence Editor** – Required for `manage_sequence` operations

> 💡 After toggling any plugin, restart the editor to finalize activation. The MCP Automation Bridge provides all automation capabilities through native C++ handlers.

### Plugin feature map

| Plugin | Location | Used By | Notes |
|--------|----------|---------|-------|
| MCP Automation Bridge | Project Plugins ▸ MCP Automation Bridge | All automation operations | Primary C++ automation transport with native handlers |
| Editor Scripting Utilities | Scripting | Asset/Actor subsystem operations | Supplies Editor Actor/Asset subsystems |
| Sequencer | Built-in | Sequencer tools | Ensure not disabled in project settings |
| Level Sequence Editor | Animation | Sequencer tools | Required for `manage_sequence` operations |

> Tools such as `physics.configureVehicle` accept an optional `pluginDependencies` array so you can list the specific Unreal plugins your project relies on (for example, Chaos Vehicles or third-party vehicle frameworks). When provided, the server will verify those plugins are active before running the configuration.

### MCP Automation Bridge plugin
- Location: `plugins/McpAutomationBridge`
- Installation: copy the folder into your project's `Plugins/` directory and regenerate project files.
- Sync helper: run `npm run automation:sync -- --engine "X:/Unreal_Engine/UE_5.6/Engine/Plugins" --project "X:/Newfolder(2)/Game/Unreal/Trial/Plugins" --clean-engine --clean-project` after repo updates to copy the latest bridge build into both plugin folders and strip legacy entries (such as `SupportedTargetPlatforms: ["Editor"]`) that trigger startup warnings.
- Verification: run `node scripts/verify-automation-bridge.js --project "C:/Path/To/YourProject/Plugins" --config "C:/Path/To/YourProject/Config/DefaultEngine.ini"` to confirm the plugin files and automation bridge environment variables are in place before launching Unreal.
- Configuration: enable **MCP Automation Bridge** in **Edit ▸ Plugins**, restart the editor, then set the endpoint/token under **Edit ▸ Project Settings ▸ Plugins ▸ MCP Automation Bridge**. The bridge ships with its own lightweight WebSocket client, so you no longer need the engine's WebSockets plugin enabled.
- Startup: after configuration, the Output Log should show a successful connection and the `bridge_started` broadcast; `SendRawMessage` becomes available to Blueprint and C++ callers for manual testing.
- Current scope: manages a WebSocket session to the Node MCP server (`ws://127.0.0.1:8090` by default), performs optional capability-token handshakes, dispatches inbound JSON to native C++ handlers, implements reconnect backoff, and responds to editor functions, property operations, blueprint actions, and more through native implementations.
- Usage: all consolidated tools use the automation bridge for native C++ execution. Keep the plugin enabled for all workflows.
- Diagnostics: the `ue://automation-bridge` MCP resource surfaces handshake timestamps, recent disconnects, pending automation requests, and whether the Node listener is running—handy when validating editor connectivity from a client.
- Roadmap: expand the bridge with elevated actions (SCS authoring, typed property marshaling, modal mediation, asset workflows).

### WebAssembly Performance (Optional)

The MCP server includes WebAssembly acceleration for computationally intensive operations. WASM is **automatically used when available** and **gracefully falls back** to pure TypeScript when the bundle or toolchain is missing.

**To enable full WASM acceleration (5–8x faster operations):**

```bash
# 1. Install Rust toolchain and wasm-pack (once per machine)
#    See https://rustup.rs for installing Rust.
cargo install wasm-pack

# 2. Build TypeScript + WASM bundle
#    The build script always runs the TypeScript compiler and then
#    optionally builds the WASM bundle. If wasm-pack is missing the
#    build will succeed with a warning and the server will use
#    TypeScript fallbacks.
npm run build

# 3. Ensure WASM is enabled (default is enabled if WASM_ENABLED is unset)
#    In .env or your process environment:
#    WASM_ENABLED=true

# 4. Start the server or run tests – the logs will include a
#    "WebAssembly module initialized successfully" message when the
#    bundle is present and loaded.
npm start
```

**Without WASM (still fully functional):**

```bash
# Disable WASM explicitly (optional)
# In .env or environment:
#   WASM_ENABLED=false

npm start
# Server will always use TypeScript implementations only.
```

When the WASM bundle is not present or `wasm-pack` is not installed:

- `npm run build` prints a concise message:
  > WASM build failed or wasm-pack missing; continuing with TypeScript-only build.
- At runtime, the server attempts to load the bundle once. On `ERR_MODULE_NOT_FOUND`
  it logs a single warning suggesting `npm run build`/`cargo install wasm-pack` and
  permanently falls back to TypeScript for that process.

WASM acceleration applies to:
- JSON property parsing (5-8x faster)
- Transform calculations (5-10x faster)
- Vector/matrix math (5x faster)
- Asset dependency resolution (3-5x faster)

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

No additional engine configuration required. The MCP Automation Bridge plugin handles all automation through native C++ code.

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
        "UE_PROJECT_PATH": "C:/Users/YourName/Documents/Unreal Projects/YourProject",
        "MCP_AUTOMATION_WS_PORT": "8090"
      }
    }
  }
}
```

## Available Tools (12)

| Tool | Description |
|------|-------------|
| `manage_asset` | Assets, Materials, Render Targets, Behavior Trees |
| `control_actor` | Spawn, delete, modify, physics |
| `control_editor` | PIE, Camera, UI Input |
| `manage_level` | Load/Save, Lighting, World Partition |
| `animation_physics` | Animation BPs, Vehicles, Ragdolls |
| `manage_effect` | Niagara, Particles, Debug Shapes |
| `manage_blueprint` | Create, SCS, Graph Editing |
| `build_environment` | Landscape, Foliage, Procedural |
| `system_control` | Profiling, Quality, UBT, Tests, Logs, Debug, Insights |
| `manage_sequence` | Sequencer/Cinematics |
| `inspect` | Object Introspection |
| `manage_audio` | Audio Assets & Components |

## Documentation

- [Handler Mappings](docs/handler-mapping.md) - TypeScript to C++ routing guide
- [GraphQL API](docs/GraphQL-API.md) - Query and mutation reference
- [Automation Progress](docs/native-automation-progress.md) - Implementation status

## Key Features

- **Native C++ Automation** - All operations route through the MCP Automation Bridge plugin's native C++ handlers
- **Graceful Degradation** - Server starts even without an active Unreal connection
- **On-Demand Connection** - Retries automation handshakes with backoff instead of persistent polling
- **Non-Intrusive Health Checks** - Uses echo commands every 30 seconds while connected
- **Command Safety** - Blocks dangerous console commands
- **Input Flexibility** - Vectors/rotators accept object or array format
- **Asset Caching** - 10-second TTL for improved performance
- **GraphQL** - Specialized endpoint for complex queries

### Native C++ Architecture

The server uses a 100% native C++ approach: all automation operations are implemented as native C++ handlers in the MCP Automation Bridge plugin (under `plugins/McpAutomationBridge/Source/`). This eliminates dependencies and provides better performance, reliability, and type safety.

Configuration and runtime defaults are centralized in `src/constants.ts`. All operations route through the automation bridge's WebSocket protocol to native plugin handlers.

## Supported Asset Types

Blueprints, Materials, Textures, Static/Skeletal Meshes, Levels, Sounds, Particles, Niagara Systems, Behavior Trees

## Example Console Commands

- **Statistics**: `stat fps`, `stat gpu`, `stat memory`
- **View Modes**: `viewmode wireframe`, `viewmode unlit`
- **Gameplay**: `slomo 0.5`, `god`, `fly`
- **Rendering**: `r.screenpercentage 50`, `r.vsync 0`

### Configuration

### Environment Variables

```env
UE_PROJECT_PATH="C:/Users/YourName/Documents/Unreal Projects/YourProject"  # Absolute path to your .uproject file
LOG_LEVEL=info                        # debug | info | warn | error

# Automation bridge WebSocket client (Node -> Unreal editor)
MCP_AUTOMATION_WS_HOST=127.0.0.1      # Host/interface for the automation bridge connection
MCP_AUTOMATION_WS_PORT=8090           # Primary bridge port (must match the plugin's port)
MCP_AUTOMATION_WS_PORTS=8090,8091     # Optional comma-separated list of additional ports
MCP_AUTOMATION_WS_PROTOCOLS=mcp-automation
MCP_AUTOMATION_CAPABILITY_TOKEN=      # Optional capability token for handshake security
MCP_AUTOMATION_BRIDGE_ENABLED=true    # Set to false to disable the automation bridge client

# WebAssembly acceleration
WASM_ENABLED=true                     # Default: enabled if unset; set false to force TS-only
# Optional override when hosting the WASM bundle elsewhere:
# WASM_PATH=file:///absolute/path/to/unreal_mcp_wasm.js

# Timeouts / caching (advanced – safe defaults are baked in)
MCP_AUTOMATION_REQUEST_TIMEOUT_MS=120000
MCP_AUTOMATION_EVENT_TIMEOUT_MS=0
ASSET_LIST_TTL_MS=10000
```

Note on configuration precedence
- The server uses `dotenv` to load `.env` for local development. A `.env.production` file is included as a reference for production deployments; copy values into your real environment or secret store as appropriate.
- Environment variables (.env / system env) override the internal runtime defaults (for example, defaults in `src/index.ts`, `src/automation-bridge.ts`, and individual tool implementations). This lets you tune timeouts, logging, and WASM behavior without modifying source code. Keep secrets in `.env` or a secret manager — do not store secrets in source files.

Mock mode
- For offline development or CI without an Editor, set `UNREAL_MCP_MOCK_MODE=1` to run tests and tools against a local mock bridge. This is useful for unit tests and continuous integration where launching the Editor is impractical.

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
npm run build          # Build TypeScript and (optionally) the WebAssembly bundle
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