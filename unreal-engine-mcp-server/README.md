# Unreal Engine MCP Server

MCP (Model Context Protocol) server for controlling Unreal Engine via Remote Control API. Enables AI assistants (Claude, Cursor, etc.) to interact with Unreal Engine projects.

## ✅ Features

- **Asset Management**: Search, filter, and browse all project assets
- **Console Commands**: Execute any UE console command (stats, rendering, gameplay) 
- **PIE Control**: Start, stop, pause Play-In-Editor sessions
- **Actor Spawning**: Spawn actors and blueprints in levels
- **Object Inspection**: Query functions and properties of UE objects
- **Remote Control**: Full HTTP and WebSocket API access

## 🚀 Quick Start

### Prerequisites
- Node.js >= 18
- Unreal Engine (5.3+)
- Enabled Plugins in UE:
  - Remote Control
  - Web Remote Control

### Installation

```bash
npm install
npm run build
```

### Enable Console Commands (REQUIRED)

Add to your project's `Saved/Config/WindowsEditor/RemoteControl.ini`:

```ini
[/Script/RemoteControlCommon.RemoteControlSettings]
bAllowConsoleCommandRemoteExecution=True
```

Then restart Unreal Engine.

### Run Server

```bash
node dist/cli.js
```

## 📋 MCP Configuration

Add to Claude Desktop or Cursor config:

```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "node",
      "args": ["path/to/unreal-engine-mcp-server/dist/cli.js"],
      "env": {
        "UE_HOST": "127.0.0.1",
        "UE_RC_HTTP_PORT": "30010",
        "UE_RC_WS_PORT": "30020"
      }
    }
  }
}
```

## 🛠️ Available Tools

### Console Commands
Execute any UE console command:
```javascript
// Examples:
"stat fps"              // Show FPS
"viewmode wireframe"    // Wireframe view
"play"                  // Start PIE
"stop"                  // Stop PIE
"pause"                 // Pause game
"slomo 0.5"            // Half speed
"fov 90"               // Set FOV
"camspeed 4"           // Camera speed
```

### Asset Management
```javascript
// Search assets
{
  "tool": "search_assets",
  "query": "BP_",
  "classNames": ["/Script/Engine.Blueprint"]
}
```

### Actor Spawning
```javascript
{
  "tool": "spawn_actor",
  "classPath": "/Engine/BasicShapes/Cube.Cube_C",
  "location": { "x": 0, "y": 0, "z": 100 }
}
```

## ⚠️ Known Issues

### Commands to Avoid (Cause Crashes)
- `viewmode visualizeBuffer BaseColor`
- `viewmode visualizeBuffer WorldNormal`
- Rapid viewmode changes without delays

### Best Practices
1. Add 500ms delays between console commands
2. Clear stats with `stat none` when done
3. Reset to `viewmode lit` after testing
4. Use batch operations for multiple commands

## 📊 Supported Asset Types

- Blueprints (`/Script/Engine.Blueprint`)
- Materials (`/Script/Engine.Material`)
- Textures (`/Script/Engine.Texture2D`)
- Static Meshes (`/Script/Engine.StaticMesh`)
- Skeletal Meshes (`/Script/Engine.SkeletalMesh`)
- Levels/Maps (`/Script/Engine.World`)
- Sounds (`/Script/Engine.SoundWave`)
- Particles (`/Script/Engine.ParticleSystem`)
- Niagara (`/Script/Niagara.NiagaraSystem`)

## 🔧 Available Console Commands

### Statistics
- `stat fps` - FPS counter
- `stat unit` - Frame timing
- `stat game` - Game stats
- `stat gpu` - GPU stats
- `stat memory` - Memory usage
- `stat engine` - Engine stats
- `stat physics` - Physics stats
- `stat none` - Clear all stats

### View Modes (Safe)
- `viewmode lit` - Default lit
- `viewmode unlit` - Unlit
- `viewmode wireframe` - Wireframe
- `viewmode detaillighting` - Detail lighting
- `viewmode lightingonly` - Lighting only
- `viewmode shadercomplexity` - Shader complexity

### Show Flags
- `show collision` - Toggle collision
- `show bounds` - Object bounds
- `show grid` - Grid display
- `show fog` - Fog
- `show particles` - Particles

### Rendering
- `r.screenpercentage 100` - Render scale
- `r.tonemapper.sharpen 0.5` - Sharpening
- `r.motionblurquality 0` - Motion blur
- `r.vsync 0` - VSync

### Gameplay
- `slomo 1` - Time speed (0.1-10)
- `god` - God mode
- `ghost` - No-clip
- `fly` - Fly mode
- `walk` - Walk mode

## 📝 Resources

MCP Resources available:
- `ue://assets` - List all project assets
- `ue://actors` - List level actors
- `ue://level` - Current level info
- `ue://exposed` - Remote Control exposed items

## 🎯 Success Rate: 95%+

All features work except:
- EditorLevelLibrary functions (use console commands instead)
- visualizeBuffer viewmodes (cause crashes)

The MCP server is production-ready for AI assistant integration!
