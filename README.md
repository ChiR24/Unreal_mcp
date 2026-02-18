# Unreal Engine MCP Server

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![NPM Package](https://img.shields.io/npm/v/unreal-engine-mcp-server)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![MCP SDK](https://img.shields.io/badge/MCP%20SDK-TypeScript-blue)](https://github.com/modelcontextprotocol/sdk)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.7-orange)](https://www.unrealengine.com/)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-Published-green)](https://registry.modelcontextprotocol.io/)
[![Project Board](https://img.shields.io/badge/Project-Roadmap-blueviolet?logo=github)](https://github.com/users/ChiR24/projects/3)
[![Discussions](https://img.shields.io/badge/Discussions-Join-brightgreen?logo=github)](https://github.com/ChiR24/Unreal_mcp/discussions)

A comprehensive Model Context Protocol (MCP) server that enables AI assistants to control Unreal Engine through a native C++ Automation Bridge plugin. Built with TypeScript, C++, and Rust (WebAssembly).

---

## Table of Contents

- [Features](#features)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Available Tools](#available-tools)
- [WebAssembly Acceleration](#webassembly-acceleration)
- [GraphQL API](#graphql-api)
- [Docker](#docker)
- [Documentation](#documentation)
- [Community](#community)
- [Development](#development)
- [Contributing](#contributing)

---

## Features

| Category | Capabilities |
|----------|-------------|
| **Asset Management** | Browse, import, duplicate, rename, delete assets; create materials |
| **Actor Control** | Spawn, delete, transform, physics, tags, components |
| **Editor Control** | PIE sessions, camera, viewport, screenshots, bookmarks |
| **Level Management** | Load/save levels, streaming, World Partition, data layers |
| **Animation & Physics** | Animation BPs, state machines, ragdolls, vehicles, constraints |
| **Visual Effects** | Niagara particles, GPU simulations, procedural effects, debug shapes |
| **Sequencer** | Cinematics, timeline control, camera animations, keyframes |
| **Graph Editing** | Blueprint, Niagara, Material, and Behavior Tree graph manipulation |
| **Audio** | Sound cues, audio components, sound mixes, ambient sounds |
| **System** | Console commands, UBT, tests, logs, project settings, CVars |

### Architecture

- **Native C++ Automation** — All operations route through the MCP Automation Bridge plugin
- **Dynamic Type Discovery** — Runtime introspection for lights, debug shapes, and sequencer tracks
- **Graceful Degradation** — Server starts even without an active Unreal connection
- **On-Demand Connection** — Retries automation handshakes with exponential backoff
- **Command Safety** — Blocks dangerous console commands with pattern-based validation
- **Asset Caching** — 10-second TTL for improved performance
- **Metrics Rate Limiting** — Per-IP rate limiting (60 req/min) on Prometheus endpoint
- **Centralized Configuration** — Unified class aliases and type definitions

---

## Getting Started

### Prerequisites

- **Node.js** 18+
- **Unreal Engine** 5.0–5.7

### Step 1: Install MCP Server

**Option A: NPX (Recommended)**
```bash
npx unreal-engine-mcp-server
```

**Option B: Clone & Build**
```bash
git clone https://github.com/ChiR24/Unreal_mcp.git
cd Unreal_mcp
npm install
npm run build
node dist/cli.js
```

### Step 2: Install Unreal Plugin

The MCP Automation Bridge plugin is included at `Unreal_mcp/plugins/McpAutomationBridge`.

**Method 1: Copy Folder**
```
Copy:  Unreal_mcp/plugins/McpAutomationBridge/
To:    YourUnrealProject/Plugins/McpAutomationBridge/
```
Regenerate project files after copying.

**Method 2: Add in Editor**
1. Open Unreal Editor → **Edit → Plugins**
2. Click **"Add"** → Browse to `Unreal_mcp/plugins/`
3. Select the `McpAutomationBridge` folder

**Video Guide:**

https://github.com/user-attachments/assets/d8b86ebc-4364-48c9-9781-de854bf3ef7d

> ⚠️ **First-Time Project Open:** When opening the project directly (double-click `.uproject`) for the first time, UE will prompt *"Would you like to rebuild them now?"* for missing modules. Click **Yes** to rebuild. After the rebuild completes, you may still see: *"Plugin 'McpAutomationBridge' failed to load because module could not be loaded."* This is expected — UE rebuilds successfully but doesn't reload the plugin in the same session. **Simply close and reopen the project** and the plugin will load correctly. Alternatively, build via Visual Studio first to avoid this.

### Step 3: Enable Required Plugins

Enable via **Edit → Plugins**, then restart the editor:

| Plugin | Required For |
|--------|--------------|
| **MCP Automation Bridge** | All automation operations |
| **Editor Scripting Utilities** | Asset/Actor subsystem operations |
| **Sequencer** | Sequencer tools |
| **Level Sequence Editor** | `manage_sequence` operations |
| **Control Rig** | `animation_physics` operations |
| **Subobject Data Interface** | Blueprint components (UE 5.7+) |
| **Geometry Script** | `manage_geometry` operations (procedural mesh) |

### Step 4: Configure MCP Client

Add to your Claude Desktop / Cursor config file:

**Using Clone/Build:**
```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "node",
      "args": ["path/to/Unreal_mcp/dist/cli.js"],
      "env": {
        "UE_PROJECT_PATH": "C:/Path/To/YourProject",
        "MCP_AUTOMATION_PORT": "8091"
      }
    }
  }
}
```

**Using NPX:**
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

---

## Configuration

### Environment Variables

```env
# Required
UE_PROJECT_PATH="C:/Path/To/YourProject"

# Automation Bridge
MCP_AUTOMATION_HOST=127.0.0.1
MCP_AUTOMATION_PORT=8091

# LAN Access (optional)
# SECURITY: Set to true to allow binding to non-loopback addresses (e.g., 0.0.0.0)
# Only enable if you understand the security implications.
MCP_AUTOMATION_ALLOW_NON_LOOPBACK=false

# Logging
LOG_LEVEL=info  # debug | info | warn | error

# Optional
WASM_ENABLED=true
MCP_AUTOMATION_REQUEST_TIMEOUT_MS=120000
ASSET_LIST_TTL_MS=10000
```

### LAN Access Configuration

By default, the automation bridge only binds to loopback addresses (127.0.0.1) for security. To enable access from other machines on your network:

**TypeScript (MCP Server):**
```env
MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true
MCP_AUTOMATION_HOST=0.0.0.0
```

**Unreal Engine Plugin:**
1. Go to **Edit → Project Settings → Plugins → MCP Automation Bridge**
2. Under **Security**, enable **"Allow Non Loopback"**
3. Under **Connection**, set **"Listen Host"** to `0.0.0.0`
4. Restart the editor

⚠️ **Security Warning:** Enabling LAN access exposes the automation bridge to your local network. Only use on trusted networks with appropriate firewall rules.

---

## Available Tools

| Tool | Description |
|------|-------------|
| `manage_asset` | Assets, Materials, Render Targets, Behavior Trees |
| `control_actor` | Spawn, delete, transform, physics, tags |
| `control_editor` | PIE, Camera, viewport, screenshots |
| `manage_level` | Load/Save, World Partition, streaming |
| `manage_lighting` | Spawn lights, GI, shadows, build lighting, **list_light_types** |
| `manage_performance` | Profiling, optimization, scalability |
| `animation_physics` | Animation BPs, Vehicles, Ragdolls, Control Rig, IK, Blend Spaces |
| `manage_effect` | Niagara, Particles, Debug Shapes, Niagara authoring, GPU sim |
| `manage_blueprint` | Create, SCS, Graph Editing, Node manipulation |
| `build_environment` | Landscape, Foliage, Procedural |
| `system_control` | UBT, Tests, Logs, Project Settings, CVars |
| `manage_sequence` | Sequencer / Cinematics, **list_track_types** |
| `inspect` | Object Introspection |
| `manage_audio` | Audio Assets, Components, Sound Cues, MetaSounds, Attenuation |
| `manage_behavior_tree` | Behavior Tree Graph Editing |
| `manage_input` | Enhanced Input Actions & Contexts |
| `manage_geometry` | Procedural mesh creation (Geometry Script) |
| `manage_skeleton` | Skeleton, sockets, physics assets, cloth binding |
| `manage_material_authoring` | Material creation, expressions, landscape layers |
| `manage_texture` | Texture creation, modification, compression settings |
| `manage_gas` | Gameplay Ability System: abilities, effects, attributes |
| `manage_character` | Character creation, movement, advanced locomotion |
| `manage_combat` | Weapons, projectiles, damage, melee combat |
| `manage_ai` | AI controllers, EQS, perception, State Trees, Smart Objects |
| `manage_inventory` | Items, equipment, loot tables, crafting |
| `manage_interaction` | Interactables, destructibles, triggers |
| `manage_widget_authoring` | UMG widget creation, layout, styling, animations |
| `manage_networking` | Replication, RPCs, network prediction |
| `manage_game_framework` | Game modes, game states, player controllers, match flow |
| `manage_sessions` | Sessions, split-screen, LAN, voice chat |
| `manage_level_structure` | Level creation, sublevels, World Partition, data layers, HLOD |
| `manage_volumes` | Trigger volumes, blocking, physics, audio, navigation volumes |
| `manage_navigation` | NavMesh settings, nav modifiers, nav links, smart links, pathfinding |

### Supported Asset Types

Blueprints • Materials • Textures • Static Meshes • Skeletal Meshes • Levels • Sounds • Particles • Niagara Systems • Behavior Trees

---

## WebAssembly Acceleration

Optional WASM acceleration for computationally intensive operations. **Enabled by default** when available, falls back to TypeScript automatically.

| Operation | Speedup |
|-----------|---------|
| JSON parsing | 5–8x |
| Transform calculations | 5–10x |
| Vector/matrix math | 5x |
| Dependency resolution | 3–5x |

### Building WASM (Optional)

```bash
cargo install wasm-pack  # Once per machine
npm run build:wasm       # Builds  WASM
```

To disable: `WASM_ENABLED=false`

---

## GraphQL API

Optional GraphQL endpoint for complex queries. **Disabled by default.**

```env
GRAPHQL_ENABLED=true
GRAPHQL_PORT=4000
```

See [GraphQL API Documentation](docs/GraphQL-API.md).

---

## Docker

```bash
docker build -t unreal-mcp .
docker run -it --rm -e UE_PROJECT_PATH=/project unreal-mcp
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [Handler Mappings](docs/handler-mapping.md) | TypeScript to C++ routing |
| [GraphQL API](docs/GraphQL-API.md) | Query and mutation reference |
| [WebAssembly Integration](docs/WebAssembly-Integration.md) | WASM performance guide |
| [Plugin Extension](docs/editor-plugin-extension.md) | C++ plugin architecture |
| [Testing Guide](docs/testing-guide.md) | How to run and write tests |
| [Migration Guide v0.5.0](docs/Migration-Guide-v0.5.0.md) | Upgrade to v0.5.0 |
| [Roadmap](docs/Roadmap.md) | Development phases |
| [Automation Progress](docs/native-automation-progress.md) | Implementation status |

---

## Development

```bash
npm run build       # Build TypeScript + WASM
npm run lint        # Run ESLint
npm run test:unit   # Run unit tests
npm run test:all    # Run all tests
```

---

## Community

| Resource | Description |
|----------|-------------|
| [Project Roadmap](https://github.com/users/ChiR24/projects/3) | Track development progress across 47 phases |
| [Discussions](https://github.com/ChiR24/Unreal_mcp/discussions) | Ask questions, share ideas, get help |
| [Issues](https://github.com/ChiR24/Unreal_mcp/issues) | Report bugs and request features |

---

## Contributing

Contributions welcome! Please:
- Include reproduction steps for bugs
- Keep PRs focused and small
- Follow existing code style

---

## License

MIT — See [LICENSE](LICENSE)
