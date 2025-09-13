# Unreal Engine MCP Server

MCP (Model Context Protocol) server for controlling Unreal Engine via Remote Control API. Enables AI assistants (Claude, Cursor, etc.) to interact with Unreal Engine projects.

## ✅ Features

### Core Features
- **Asset Management**: Search, filter, and browse all project assets
- **Console Commands**: Execute any UE console command (stats, rendering, gameplay) 
- **PIE Control**: Start, stop, pause Play-In-Editor sessions
- **Actor Spawning**: Spawn actors and blueprints in levels
- **Object Inspection**: Query functions and properties of UE objects
- **Remote Control**: Full HTTP and WebSocket API access

### 🚀 Advanced Features
- **Animation System**: Create animation blueprints, state machines, montages, blend spaces
- **Physics Simulation**: Chaos physics, ragdolls, constraints, destruction, vehicles
- **Niagara Effects**: Particle systems, GPU simulations, preset effects (fire, water, explosions)
- **Blueprint Visual Scripting**: Create and manage blueprints programmatically
- **Audio System**: 3D spatial audio, sound cues, reverb zones
- **UI/UMG Widgets**: Create and manage UI elements
- **Level Sequences**: Cinematics and camera animations
- **Control Rig**: Advanced skeletal control

## 🚀 Quick Start

### Prerequisites
- Node.js >= 18
- Unreal Engine (5.0 - 5.6)
- Enabled Plugins in UE:
  - Remote Control
  - Web Remote Control
  - Python Script Plugin (for material creation and advanced features)
  - Editor Scripting Utilities
  - Optional: [UnrealEnginePython](https://github.com/20tab/UnrealEnginePython) for enhanced Blueprint component manipulation

### Installation

```bash
npm install
npm run build
```

### Configuration (REQUIRED)

#### 1. Enable Remote Execution
Add to your project's `Config/DefaultEngine.ini`:

```ini
[/Script/PythonScriptPlugin.PythonScriptPluginSettings]
bRemoteExecution=True
bAllowRemotePythonExecution=True

[/Script/RemoteControl.RemoteControlSettings]
bAllowRemoteExecutionOfConsoleCommands=True
bEnableRemoteExecution=True
bAllowPythonExecution=True
+WhitelistedClasses=/Script/Engine.Default__PythonScriptLibrary
+WhitelistedClasses=/Script/EditorScriptingUtilities.Default__EditorAssetLibrary
```

#### 2. Enable Python in Project Settings
- Go to Edit > Project Settings > Plugins > Remote Control
- Check "Enable Remote Python Execution"
- Click "Set as Defaults"

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

### Consolidated Mode (DEFAULT)

The server uses 10 consolidated tools that provide comprehensive control:

1. **manage_asset** - List, create materials, import assets
2. **control_actor** - Spawn, delete actors, apply physics forces
3. **control_editor** - PIE control, camera, view modes
4. **manage_level** - Load/save levels, create lights, build lighting
5. **animation_physics** - Animation blueprints, montages, ragdoll setup
6. **create_effect** - Particle effects, Niagara systems, debug shapes
7. **manage_blueprint** - Create blueprints, add components
8. **build_environment** - Landscapes, terrain sculpting, foliage
9. **system_control** - Profiling, quality settings, sound, UI
10. **console_command** - Direct console command execution

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

## 🔧 Recent Improvements

### Version 1.2.0 (Latest)
- ✅ **Connection Improvements**:
  - Added connection timeout (5 seconds by default)
  - Server no longer hangs if Unreal Engine isn't running
  - Automatic retry with configurable attempts
  - Graceful degradation - server starts even without UE connection
  - Automatic reconnection attempts every 10 seconds
- ✅ **Tool 2 (control_actor) Fixes**:
  - Spawn now uses Python API with proper actor labeling
  - Delete checks both actor name and label
  - Apply force enables physics and checks both name/label
- ✅ **Tool 7 (manage_blueprint) UE 5.6 Enhancements**:
  - Added multi-tier component addition approach
  - Fast-path support for UnrealEnginePython plugin
  - Graceful fallback with manual instructions when API limited
  - Full compatibility with UE 5.0 - 5.6
- ✅ Fixed Python execution for multi-line scripts
- ✅ Fixed deprecated EditorLevelLibrary calls (now uses EditorActorSubsystem)
- ✅ Fixed apply_force parameter mismatch
- ✅ Added missing .js extensions for ESM compatibility
- ✅ Improved material creation with proper validation
- ✅ Enhanced error handling and retry logic

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


## ⚙️ Environment (.env)

Create a .env file in the project root to configure connection and behavior:

```env
# Unreal Remote Control connection (defaults shown)
UE_HOST=127.0.0.1
UE_RC_HTTP_PORT=30010
UE_RC_WS_PORT=30020

# Tool mode: true = 10 consolidated tools, false = individual tools
USE_CONSOLIDATED_TOOLS=true

# Asset listing cache TTL (ms)
ASSET_LIST_TTL_MS=10000

# Logging level: debug | info | warn | error
LOG_LEVEL=info
```

Notes:
- You must enable Remote Control and Python Script Plugin in Unreal for full functionality.
- See the Configuration section above for the required project settings to allow remote Python execution.

## 🩺 Health & Diagnostics

The server exposes a health resource that includes connection, performance, and feature information:

- Resource: `ue://health`
- Example payload:

```json
{
  "status": "connected",
  "uptime": 1234,
  "performance": {
    "totalRequests": 42,
    "successfulRequests": 40,
    "failedRequests": 2,
    "successRate": "95.24%",
    "averageResponseTime": "12ms"
  },
  "lastHealthCheck": "2025-09-13T06:31:15.000Z",
  "unrealConnection": {
    "status": "connected",
    "host": "127.0.0.1",
    "httpPort": 30010,
    "wsPort": 30020,
    "engineVersion": {
      "version": "5.6.1-44394996+++UE5+Release-5.6",
      "major": 5,
      "minor": 6,
      "patch": 1,
      "isUE56OrAbove": true
    },
    "features": {
      "pythonEnabled": true,
      "subsystems": {
        "unrealEditor": true,
        "levelEditor": true,
        "editorActor": true
      },
      "rcHttpReachable": true
    }
  },
  "recentErrors": [
    {
      "time": "2025-09-13T06:30:00.000Z",
      "scope": "tool-call/list_assets",
      "type": "CONNECTION",
      "message": "HTTP timeout",
      "retriable": true
    }
  ]
}
```

Also available: `ue://version` returns the same engine version structure.

## 📦 Asset Listing Cache

- Asset listing now uses an in-memory cache keyed by `directory + recursive` with a TTL.
- Configure TTL via `ASSET_LIST_TTL_MS` (default 10000 ms).
- First call fills the cache; subsequent calls within TTL return instantly.

## 🎛️ Input Normalization (Vectors & Rotators)

Most tools that accept vectors and rotations now accept either object or array shapes:

- Vector: `{ x, y, z }` or `[x, y, z]`
- Rotator: `{ pitch, yaw, roll }` or `[pitch, yaw, roll]`

Examples:
```json
{
  "tool": "spawn_actor",
  "classPath": "/Engine/BasicShapes/Cube.Cube_C",
  "location": [0, 0, 100],
  "rotation": { "pitch": 0, "yaw": 45, "roll": 0 }
}
```

```json
{
  "tool": "create_light",
  "lightType": "Directional",
  "name": "KeyLight",
  "rotation": [ -45, 30, 0 ]
}
```

## 🧰 Logging & Safety

- Adjust verbosity via `LOG_LEVEL` (`debug`, `info`, `warn`, `error`).
- The server blocks known dangerous console commands (e.g., `buildpaths`, `rebuildnavigation`, certain `visualizeBuffer` modes) to prevent UE crashes.

## 🧪 Development

- Build: `npm run build`
- Lint: `npm run lint` / `npm run lint:fix`

Suggested CI (GitHub Actions):
- Run `npm ci`, `npm run build`, `npm run lint`, and `npm test` on pull requests.

## 🤝 Contributing

Issues and PRs are welcome! Please include reproduction steps for bugs. For new tools or UE features, prefer small, focused PRs.

## 📄 License

MIT © Unreal Engine MCP Team. See `LICENSE` for details.
