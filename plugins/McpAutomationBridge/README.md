# MCP Automation Bridge

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.7-orange)](https://www.unrealengine.com/)
[![GitHub](https://img.shields.io/badge/GitHub-ChiR24/Unreal__mcp-blueviolet?logo=github)](https://github.com/ChiR24/Unreal_mcp)

An Unreal Engine editor plugin that enables AI assistants (Claude, Cursor, Windsurf, etc.) to control Unreal Engine through the Model Context Protocol (MCP).

---

## Features

| Category | Capabilities |
|----------|-------------|
| **Asset Management** | Browse, import, duplicate, rename, delete assets; create materials |
| **Actor Control** | Spawn, delete, transform, physics, tags, components |
| **Editor Control** | PIE sessions, camera, viewport, screenshots, bookmarks |
| **Level Management** | Load/save levels, streaming, World Partition, data layers |
| **Animation & Physics** | Animation BPs, state machines, ragdolls, vehicles, constraints |
| **Visual Effects** | Niagara particles, GPU simulations, procedural effects |
| **Sequencer** | Cinematics, timeline control, camera animations |
| **Graph Editing** | Blueprint, Niagara, Material, Behavior Tree graphs |
| **Audio** | Sound cues, audio components, MetaSounds |
| **System** | Console commands, UBT, tests, logs, project settings |

**200+ automation actions** across 36 MCP tools.

---

## Requirements

- **Unreal Engine**: 5.0 - 5.7
- **Platforms**: Win64, Mac, Linux
- **Node.js**: 18+ (for MCP server)

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
   - ✅ Editor Scripting Utilities
   - ✅ Niagara

</details>

<details>
<summary><b>Optional Plugins (Auto-enabled)</b></summary>

   - ✅ Level Sequence Editor (for `manage_sequence`)
   - ✅ Control Rig (for `animation_physics`)
   - ✅ GeometryScripting (for `manage_geometry`)
   - ✅ Behavior Tree Editor (for `manage_behavior_tree`)
   - ✅ Niagara Editor (for Niagara authoring)
   - ✅ Gameplay Abilities (for `manage_gas`)
   - ✅ MetaSound (for `manage_audio` MetaSounds)
   - ✅ StateTree (for `manage_ai` State Trees)
   - ✅ Enhanced Input (for `manage_input`)
   - ✅ Environment Query Editor (for AI/EQS)
   - ✅ Smart Objects (for AI smart objects)
   - ✅ Chaos Cloth (for cloth simulation)
   - ✅ Interchange (for asset import/export)
   - ✅ Data Validation (for data validation)
   - ✅ Procedural Mesh Component (for procedural geometry)
   - ✅ OnlineSubsystem (for sessions/networking)
   - ✅ OnlineSubsystemUtils (for sessions/networking)

</details>

   > 💡 Optional plugins are auto-enabled by the MCP Automation Bridge plugin. Only the core plugins require manual verification.

5. Restart the editor

### Method 2: Add in Editor

1. Open Unreal Editor → **Edit → Plugins**
2. Click **"Add"** button
3. Browse to and select the `McpAutomationBridge` folder
4. Enable the plugin and restart

---

## Quick Start

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

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `UE_PROJECT_PATH` | - | Path to your `.uproject` file |
| `MCP_AUTOMATION_HOST` | `127.0.0.1` | Bridge host address |
| `MCP_AUTOMATION_PORT` | `8091` | Bridge WebSocket port |
| `LOG_LEVEL` | `info` | Logging level (debug/info/warn/error) |

### Plugin Settings

Configure in **Edit → Project Settings → Plugins → MCP Automation Bridge**:

- **Listen Ports**: WebSocket ports (default: 8090, 8091)
- **Enable TLS**: Enable secure WebSocket connections
- **Allow Non-Loopback**: Enable LAN access (security consideration)

---

## Security

- **Loopback-only binding** by default (127.0.0.1)
- **TLS/SSL support** for secure connections
- **Rate limiting** (600 messages/min, 120 automation requests/min)
- **Handshake required** before automation requests
- **Command validation** blocks dangerous console commands

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

---

## Documentation

- **Full Documentation**: [GitHub README](https://github.com/ChiR24/Unreal_mcp#readme)
- **Handler Mapping**: [docs/handler-mapping.md](https://github.com/ChiR24/Unreal_mcp/blob/main/docs/handler-mapping.md)
- **Migration Guide**: [docs/Migration-Guide-v0.5.0.md](https://github.com/ChiR24/Unreal_mcp/blob/main/docs/Migration-Guide-v0.5.0.md)

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
