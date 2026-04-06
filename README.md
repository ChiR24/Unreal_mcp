# Unreal Engine MCP Server

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![NPM Package](https://img.shields.io/npm/v/unreal-engine-mcp-server)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![MCP SDK](https://img.shields.io/badge/MCP%20SDK-TypeScript-blue)](https://github.com/modelcontextprotocol/sdk)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.7-orange)](https://www.unrealengine.com/)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-Published-green)](https://registry.modelcontextprotocol.io/)
[![Project Board](https://img.shields.io/badge/Project-Roadmap-blueviolet?logo=github)](https://github.com/users/ChiR24/projects/3)
[![Discussions](https://img.shields.io/badge/Discussions-Join-brightgreen?logo=github)](https://github.com/ChiR24/Unreal_mcp/discussions)

A comprehensive Model Context Protocol (MCP) server that enables AI assistants to control Unreal Engine through a native C++ Automation Bridge plugin. Built with TypeScript and C++.

---

## Table of Contents

- [Features](#features)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Available Tools](#available-tools)
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

#### From source (requires a project with code target)

Your project must have a code target (`.sln` or `.xcworkspace`).
Blueprint-only projects cannot compile native plugins — to convert, add any class via **Tools > New C++ Class** in the editor.

**Method 1: Copy Folder**
```
Copy:  Unreal_mcp/plugins/McpAutomationBridge/
To:    YourUnrealProject/Plugins/McpAutomationBridge/
```

**Method 2: External Plugin Directory (no copy needed)**
1. Open Unreal Editor → **Edit → Plugins**
2. Click **Plugin Directories** (bottom-left)
3. In **Additional Plugin Directories**, add the path to `Unreal_mcp/plugins/`
4. Restart the editor — the plugin will be picked up from the external location

This saves the path in your `.uproject` file so the plugin stays linked without copying.

The plugin compiles automatically when you open the project — UE detects the `.uplugin` + `Source/` and runs UnrealBuildTool.

**Video Guide:**

https://github.com/user-attachments/assets/d8b86ebc-4364-48c9-9781-de854bf3ef7d

> ⚠️ **First-Time Project Open:** UE may prompt *"Would you like to rebuild them now?"* — click **Yes**. If instead you see *"Missing Modules — McpAutomationBridge. Engine modules cannot be compiled at runtime. Please build through your IDE."* — open your project in **Visual Studio** (Win) or **Xcode** (Mac) and build from there. After that, the editor will open normally with the plugin loaded.

#### Pre-built (works with any project, including Blueprint-only)

Build the plugin once, then distribute the compiled binaries — no IDE or compilation needed on the target machine.

**1. Build:**
```bash
# macOS / Linux
./scripts/package-plugin.sh /path/to/UE_5.6

# Windows
scripts\package-plugin.bat C:\Path\To\UE_5.6
```

This produces a zip like `McpAutomationBridge-v0.6.0-UE5.6-Mac.zip`.

**2. Install:** unzip into `YourProject/Plugins/` and open the project. That's it — no compilation step.

> Note: pre-built binaries are tied to a specific UE version. A build for 5.6 won't work with 5.5 or 5.7.

### Step 3: Enable Required Plugins

Enable via **Edit → Plugins**, then restart the editor.

<details>
<summary><b>Core Plugins (Required)</b></summary>

| Plugin | Required For |
|--------|--------------|
| **MCP Automation Bridge** | All automation operations |
| **Editor Scripting Utilities** | Asset/Actor subsystem operations |
| **Niagara** | Visual effects and particle systems |

</details>

<details>
<summary><b>Optional Plugins (Auto-enabled)</b></summary>

| Plugin | Required For |
|--------|--------------|
| **Level Sequence Editor** | `manage_sequence` operations |
| **Control Rig** | `animation_physics` operations |
| **GeometryScripting** | `manage_geometry` operations |
| **Behavior Tree Editor** | `manage_behavior_tree` operations |
| **Niagara Editor** | Niagara authoring |
| **Environment Query Editor** | AI/EQS operations |
| **Gameplay Abilities** | `manage_gas` operations |
| **MetaSound** | `manage_audio` MetaSound authoring |
| **StateTree** | `manage_ai` State Tree operations |
| **Smart Objects** | AI smart object operations |
| **Enhanced Input** | `manage_input` operations |
| **Chaos Cloth** | Cloth simulation |
| **Interchange** | Asset import/export |
| **Data Validation** | Data validation |
| **Procedural Mesh Component** | Procedural geometry |
| **OnlineSubsystem** | Session/networking operations |
| **OnlineSubsystemUtils** | Session/networking operations |

</details>

> 💡 Optional plugins are auto-enabled by the MCP Automation Bridge plugin when needed.

### Step 4: Configure MCP Client

#### Option A: Native MCP Transport (Direct HTTP — no bridge needed)

The plugin includes a built-in MCP Streamable HTTP server. AI clients connect directly to the plugin over HTTP — no TypeScript bridge, no Node.js, no npm.

**Enable in Unreal:**
1. **Edit > Project Settings > Plugins > MCP Automation Bridge**
2. Check **Enable Native MCP**
3. Set port (default: `3000`)
4. Optionally set **Native MCP Instructions** for project-specific guidance
5. Restart the editor

**Configure your MCP client** to use Streamable HTTP transport at:
```
http://localhost:3000/mcp
```

**Claude Code:**
```bash
claude mcp add unreal-engine --transport http http://localhost:3000/mcp
```

Or manually in `~/.claude/settings.json` or project `.mcp.json`:
```json
{
  "mcpServers": {
    "unreal-engine": {
      "type": "url",
      "url": "http://localhost:3000/mcp"
    }
  }
}
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

**Verify it works:**
- **Status bar** — look for `● MCP :3000 (2)` in the bottom-right of the editor. Green dot = server running, number in parens = active sessions. Click it to open settings.
- **Output Log** — filter by `LogMcpNativeTransport` to see connections, tool calls, and session activity:
  ```
  LogMcpNativeTransport: Native MCP server started on http://localhost:3000/mcp
  LogMcpNativeTransport: MCP session initialized: ... (client: claude-code 2.1.92, active sessions: 1)
  LogMcpNativeTransport: tools/call: inspect (RequestId=...)
  LogMcpNativeTransport: tools/call completed: ... (tool=inspect, success=true)
  ```

Features:
- SSE streaming for real-time progress during long operations
- Multiple concurrent sessions (Cursor + Claude Code + others simultaneously)
- Dynamic tool management — only core tools (8) loaded by default, enable more via `manage_tools`

#### Option B: TypeScript Bridge (stdio — classic setup)

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
MCP_AUTOMATION_REQUEST_TIMEOUT_MS=120000
ASSET_LIST_TTL_MS=10000

# Custom content mount points (comma-separated)
# Plugins with CanContainContent register mount points beyond /Game/.
# MCP_ADDITIONAL_PATH_PREFIXES=/ProjectObject/,/ProjectAnimation/
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

**36 MCP tools** with action-based dispatch for comprehensive Unreal Engine automation.

<details>
<summary><b>Core Tools</b></summary>

| Tool | Description |
|------|-------------|
| `manage_asset` | Assets, Materials, Render Targets, Behavior Trees |
| `control_actor` | Spawn, delete, transform, physics, tags |
| `control_editor` | PIE, Camera, viewport, screenshots |
| `manage_level` | Load/Save, World Partition, streaming |
| `system_control` | UBT, Tests, Logs, Project Settings, CVars |
| `inspect` | Object Introspection |
| `manage_pipeline` | Build automation, UBT compilation, status checks |
| `manage_tools` | Dynamic tool management (enable/disable at runtime) |

</details>

<details>
<summary><b>World Building</b></summary>

| Tool | Description |
|------|-------------|
| `manage_lighting` | Spawn lights, GI, shadows, build lighting, list_light_types |
| `manage_level_structure` | Level creation, sublevels, World Partition, data layers, HLOD |
| `manage_volumes` | Trigger volumes, blocking, physics, audio, navigation volumes |
| `manage_navigation` | NavMesh settings, nav modifiers, nav links, smart links, pathfinding |
| `build_environment` | Landscape, Foliage, Procedural |
| `manage_splines` | Spline creation, spline mesh deformation |

</details>

<details>
<summary><b>Animation & Physics</b></summary>

| Tool | Description |
|------|-------------|
| `animation_physics` | Animation BPs, Vehicles, Ragdolls, Control Rig, IK, Blend Spaces |
| `manage_skeleton` | Skeleton, sockets, physics assets, cloth binding |
| `manage_geometry` | Procedural mesh creation (Geometry Script) |

</details>

<details>
<summary><b>Visuals & Effects</b></summary>

| Tool | Description |
|------|-------------|
| `manage_effect` | Niagara, Particles, Debug Shapes, GPU simulations |
| `manage_material_authoring` | Material creation, expressions, landscape layers |
| `manage_texture` | Texture creation, modification, compression settings |
| `manage_blueprint` | Create, SCS, Graph Editing, Node manipulation |
| `manage_sequence` | Sequencer / Cinematics, list_track_types |
| `manage_performance` | Profiling, optimization, scalability |

</details>

<details>
<summary><b>Audio & Input</b></summary>

| Tool | Description |
|------|-------------|
| `manage_audio` | Audio Assets, Components, Sound Cues, MetaSounds, Attenuation |
| `manage_input` | Enhanced Input Actions & Contexts |

</details>

<details>
<summary><b>Gameplay Systems</b></summary>

| Tool | Description |
|------|-------------|
| `manage_behavior_tree` | Behavior Tree Graph Editing |
| `manage_ai` | AI controllers, EQS, perception, State Trees, Smart Objects |
| `manage_gas` | Gameplay Ability System: abilities, effects, attributes |
| `manage_character` | Character creation, movement, advanced locomotion |
| `manage_combat` | Weapons, projectiles, damage, melee combat |
| `manage_inventory` | Items, equipment, loot tables, crafting |
| `manage_interaction` | Interactables, destructibles, triggers |
| `manage_widget_authoring` | UMG widget creation, layout, styling, animations |

</details>

<details>
<summary><b>Networking & Sessions</b></summary>

| Tool | Description |
|------|-------------|
| `manage_networking` | Replication, RPCs, network prediction |
| `manage_game_framework` | Game modes, game states, player controllers, match flow |
| `manage_sessions` | Sessions, split-screen, LAN, voice chat |

</details>
### Supported Asset Types

Blueprints • Materials • Textures • Static Meshes • Skeletal Meshes • Levels • Sounds • Particles • Niagara Systems • Behavior Trees

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
| [Plugin Extension](docs/editor-plugin-extension.md) | C++ plugin architecture |
| [Testing Guide](docs/testing-guide.md) | How to run and write tests |
| [Roadmap](docs/Roadmap.md) | Development phases |


---

## Development

```bash
npm run build       # Build TypeScript
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
