# Unreal Engine MCP Server

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![NPM Package](https://img.shields.io/npm/v/unreal-engine-mcp-server)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![MCP SDK](https://img.shields.io/badge/MCP%20SDK-TypeScript-blue)](https://github.com/modelcontextprotocol/sdk)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.7-orange)](https://www.unrealengine.com/)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-Published-green)](https://registry.modelcontextprotocol.io/)
[![Tools](https://img.shields.io/badge/Tools-30-purple)](docs/handler-mapping.md)
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
| **Procedural Content** | PCG graphs, samplers, filters, spawners, splines, mesh scattering |
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
| **PCG** | `manage_pcg` operations (procedural content generation) |
| **Water** (Experimental) | `build_environment` water operations (oceans, lakes, rivers) |
| **HairStrands** (Optional) | `manage_character_avatar` groom/hair operations |
| **Mutable** (Optional) | `manage_character_avatar` customizable object operations |
| **Interchange** (Built-in) | `manage_asset_plugins` Interchange Framework import/export |
| **USD Importer** (Optional) | `manage_asset_plugins` USD stage and prim operations |
| **Alembic Importer** (Optional) | `manage_asset_plugins` Alembic geometry cache import |
| **glTF Exporter** (Optional) | `manage_asset_plugins` glTF/GLB import/export |
| **Datasmith** (Optional) | `manage_asset_plugins` CAD and scene import |
| **Houdini Engine** (External) | `manage_asset_plugins` HDA operations (requires SideFX plugin) |
| **Substance** (External) | `manage_asset_plugins` SBSAR procedural textures (requires Adobe plugin) |
| **Bink Media** (Built-in) | `manage_audio_middleware` Bink Video playback |
| **Wwise** (External) | `manage_audio_middleware` Audiokinetic audio middleware (requires Wwise plugin) |
| **FMOD** (External) | `manage_audio_middleware` FMOD Studio audio middleware (requires FMOD plugin) |
| **Live Link** (Built-in) | `manage_livelink` Motion capture and live data streaming (sources, subjects, presets, face tracking) |

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

**30 Consolidated Tools** with 1,600+ actions, organized by category:

### Core Tools
| Tool | Description |
|------|-------------|
| `manage_pipeline` | Filter tools by category (core, world, authoring, gameplay, utility) |
| `manage_asset` | Assets, Materials, Blueprints (SCS, graph nodes) |
| `control_actor` | Spawn actors, transforms, physics, components, tags |
| `control_editor` | PIE, viewport, console, screenshots, CVars, UBT, input |
| `manage_level` | Levels, streaming, World Partition, HLOD; PCG graphs |

### World Building
| Tool | Description |
|------|-------------|
| `manage_lighting` | Lights, GI, shadows, volumetric fog, post-processing |
| `build_environment` | Landscapes, foliage, terrain, sky/fog, water, weather |
| `manage_volumes` | Volumes (trigger, physics, audio, nav) and splines |

### Authoring Tools
| Tool | Description |
|------|-------------|
| `manage_material_authoring` | Materials, expressions, landscape layers, textures |
| `manage_geometry` | Procedural meshes via Geometry Script |
| `manage_skeleton` | Skeletal meshes, sockets, physics assets; media |
| `manage_audio` | Audio playback, mixes, MetaSounds + Wwise/FMOD/Bink |
| `manage_sequence` | Sequencer cinematics, keyframes, MRQ renders |
| `manage_widget_authoring` | UMG widgets, layouts, bindings, HUDs |

### Gameplay Systems
| Tool | Description |
|------|-------------|
| `animation_physics` | Animation BPs, IK, retargeting + Chaos destruction/vehicles |
| `manage_effect` | Niagara/Cascade particles, debug shapes, VFX authoring |
| `manage_character` | Characters, movement, locomotion + Inventory (items, equipment) |
| `manage_combat` | Weapons, projectiles, damage, melee; GAS abilities |
| `manage_ai` | AI Controllers, BT, EQS, perception, State Trees, NPCs |
| `manage_networking` | Replication, RPCs, prediction, sessions; GameModes |
| `manage_gameplay_systems` | Targeting, checkpoints, objectives, photo mode, dialogue |

### Utility & Plugins
| Tool | Description |
|------|-------------|
| `manage_data` | Data assets, tables, save games, tags; modding/PAK/UGC |
| `manage_build` | UBT, cook/package, plugins, DDC; tests, validation |
| `manage_editor_utilities` | Editor modes, content browser, selection, subsystems |
| `manage_performance` | Profiling, benchmarks, scalability, LOD, Nanite |
| `manage_character_avatar` | MetaHuman, Groom/Hair, Mutable, Ready Player Me |
| `manage_asset_plugins` | Import plugins (USD, Alembic, glTF, Datasmith, Houdini) |
| `manage_livelink` | Live Link motion capture: sources, subjects, face tracking |
| `manage_xr` | XR (VR/AR/MR) + Virtual Production (nDisplay, DMX) |
| `manage_accessibility` | Accessibility: colorblind, subtitles, audio, motor, cognitive |

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

## Security

### WebSocket Communication
The automation bridge uses WebSocket for IPC with the Unreal Editor plugin.
The default host binding is `127.0.0.1` (configurable via `MCP_AUTOMATION_HOST`).

### GraphQL API
The GraphQL API is disabled by default (`GRAPHQL_ENABLED` must be set to `true`).
When enabled, it applies query depth limiting and per-IP rate limiting.

### Command Validation
Console commands pass through `src/utils/command-validator.ts` before execution.
The validator applies pattern-based filtering.

### Path Sanitization
User-provided paths are processed by `sanitizePathSafe()` in `src/utils/validation.ts`.
This function rejects directory traversal patterns and normalizes path separators.

---

## Community

| Resource | Description |
|----------|-------------|
| [Project Roadmap](https://github.com/users/ChiR24/projects/3) | Track development progress across 59 phases |
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
