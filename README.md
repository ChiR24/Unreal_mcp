# Unreal Engine MCP Server

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![NPM Package](https://img.shields.io/npm/v/unreal-engine-mcp-server)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![MCP SDK](https://img.shields.io/badge/MCP%20SDK-TypeScript-blue)](https://github.com/modelcontextprotocol/sdk)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.7-orange)](https://www.unrealengine.com/)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-Published-green)](https://registry.modelcontextprotocol.io/)

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

# Logging
LOG_LEVEL=info  # debug | info | warn | error

# Optional
WASM_ENABLED=true
MCP_AUTOMATION_REQUEST_TIMEOUT_MS=120000
ASSET_LIST_TTL_MS=10000
```

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
| `animation_physics` | Animation BPs, Vehicles (custom types), Ragdolls |
| `manage_effect` | Niagara, Particles, Debug Shapes, **list_debug_shapes** |
| `manage_blueprint` | Create, SCS, Graph Editing |
| `manage_blueprint_graph` | Direct Blueprint Graph Manipulation |
| `build_environment` | Landscape, Foliage, Procedural |
| `system_control` | UBT, Tests, Logs, Project Settings, CVars |
| `manage_sequence` | Sequencer / Cinematics, **list_track_types** |
| `inspect` | Object Introspection |
| `manage_audio` | Audio Assets & Components |
| `manage_behavior_tree` | Behavior Tree Graph Editing |
| `manage_input` | Enhanced Input Actions & Contexts |
| `manage_geometry` | Procedural mesh creation (Geometry Script) |
| `manage_skeleton` | Skeleton, sockets, physics assets, cloth binding |
| `manage_material_authoring` | Material creation, expressions, landscape layers |
| `manage_texture` | Texture creation, modification, compression settings |
| `manage_animation_authoring` | AnimBP, Control Rig, IK Rig, IK Retargeter |
| `manage_niagara_authoring` | Niagara system/emitter creation, modules, parameters |
| `manage_gas` | Gameplay Ability System: abilities, effects, attributes |
| `manage_character` | Character creation, movement, advanced locomotion |
| `manage_combat` | Weapons, projectiles, damage, melee combat |
| `manage_ai` | AI controllers, EQS, perception, State Trees, Smart Objects |
| `manage_inventory` | Items, equipment, loot tables, crafting |
| `manage_interaction` | Interactables, destructibles, triggers |
| `manage_widget_authoring` | UMG widget creation, layout, styling, animations |
| `manage_networking` | Replication, RPCs, network prediction |
| `manage_audio_authoring` | MetaSounds, sound classes, attenuation, dialogue |

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

## Contributing

Contributions welcome! Please:
- Include reproduction steps for bugs
- Keep PRs focused and small
- Follow existing code style

---

## License

MIT — See [LICENSE](LICENSE)
