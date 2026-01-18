# MCP Automation Bridge Plugin

The MCP Automation Bridge is a production-ready Unreal Editor plugin that enables direct communication between the MCP server and Unreal Engine, providing **100% native C++ implementations** for all automation tasks.

## Goals

- **Direct SCS Access** - Expose Blueprint SimpleConstructionScript mutations through a curated C++ API that the MCP server can call.
- **Typed Property Marshaling** - Relay incoming JSON payloads through Unreal's `FProperty` system so class/enum/soft object references resolve without manual string coercion.
- **Asset Lifecycle Helpers** - Wrap save/move/delete flows with redirector fix-up, source control hooks, and safety prompts suppressed via automation policies.
- **Modal Dialog Mediation** - Surface blocking dialogs to the MCP server with explicit continue/cancel channels instead of stalling automation.
- **Native-Only Architecture** - All operations implemented in C++ without Python dependencies for maximum reliability and performance.

## Architecture Sketch

- Editor plugin registers a `UMcpAutomationBridge` subsystem.
- Subsystem subscribes to a local WebSocket or named pipe opened by the Node MCP server when it needs elevated actions.
- Each elevated command includes a capability token so the plugin can enforce an allow-list (exposed through project settings) and fail gracefully if disabled.
- Results are serialized back to the MCP server with structured warnings so the client can still prompt the user when manual intervention is required.

## Plugin Architecture (Current: v0.1.0)

> **Note:** This plugin is currently in Beta/Experimental status.

### Core Components

- **Plugin Location**: `plugins/McpAutomationBridge/` (source)
- **Module Type**: Editor-only subsystem (`UEditorSubsystem`)
- **Main Class**: `UMcpAutomationBridgeSubsystem` - manages WebSocket connections, request routing, and automation execution
- **WebSocket Implementation**: `FMcpBridgeWebSocket` - custom lightweight WebSocket client (no external dependencies)
- **Settings**: `UMcpAutomationBridgeSettings` - configurable via **Project Settings > Plugins > MCP Automation Bridge**

### Connection Management

- **WebSocket Server Mode**: Plugin connects TO the MCP server's WebSocket listener (default: `ws://127.0.0.1:8091`)
- **Handshake Protocol**: `bridge_hello` -> capability token validation -> `bridge_ack`
- **Reconnection**: Automatic with exponential backoff (configurable delay, 5s default)
- **Heartbeat**: Optional heartbeat tracking for connection health monitoring
- **Capability Token**: Optional security layer for authentication
- **Multi-Port Support**: Can connect to multiple server ports simultaneously (default: 8090, 8091)

### Request Processing

- **Thread-Safe Queue**: Incoming requests queued and processed sequentially on game thread
- **Telemetry**: Tracks success/failure rates, execution times, and action statistics
- **Error Handling**: Structured error responses with error codes and retry flags
- **Timeout Management**: Configurable timeouts for long-running operations

## Server Integration

- `src/automation-bridge.ts` spins up a lightweight WebSocket server (default `ws://127.0.0.1:8091`) guarded by an optional capability token.
- Handshake flow: editor sends `bridge_hello` -> server validates capability token -> server responds with `bridge_ack` and caches the socket for future elevated commands.
- Environment flags: `MCP_AUTOMATION_HOST`, `MCP_AUTOMATION_PORT`, `MCP_AUTOMATION_CAPABILITY_TOKEN`, and `MCP_AUTOMATION_CLIENT_MODE` allow operators to relocate or disable the listener without code changes.
- Health endpoint (`ue://health`) now surfaces bridge connectivity status so MCP clients can confirm when the plugin is online.

## Tool Coverage Matrix

**36 Consolidated Tools** with native C++ implementations:

| # | Tool | Category | Bridge Status | Description |
|---|------|----------|---------------|-------------|
| 1 | `configure_tools` | Core | N/A | MCP meta-tool for filtering visible tools (not UE-related) |
| 2 | `manage_asset` | Core | Native | Assets, Materials, Blueprints (SCS, graph nodes), MetaSounds, Nanite |
| 3 | `control_actor` | Core | Native | Spawn actors, transforms, physics, components, tags, attachments |
| 4 | `control_editor` | Core | Native | PIE, viewport, console, screenshots, CVars, UBT, input |
| 5 | `manage_level` | Core | Native | Levels, streaming, World Partition, HLOD, PCG graphs |
| 6 | `manage_motion_design` | World | Native | Motion Design toolset integration |
| 7 | `animation_physics` | Gameplay | Native | Animation BPs, IK, retargeting, Chaos destruction/vehicles |
| 8 | `manage_effect` | World | Native | Niagara/Cascade particles, debug shapes, VFX authoring |
| 9 | `build_environment` | World | Native | Landscapes, foliage, terrain, sky/fog, water, weather |
| 10 | `manage_sequence` | Authoring | Native | Sequencer cinematics, keyframes, MRQ renders |
| 11 | `manage_audio` | Authoring | Native | Audio playback, mixes, MetaSounds, Wwise/FMOD/Bink |
| 12 | `manage_lighting` | World | Native | Lights, GI, shadows, volumetric fog, post-processing |
| 13 | `manage_performance` | Utility | Native | Profiling, benchmarks, scalability, LOD, Nanite |
| 14 | `manage_geometry` | Authoring | Native | Procedural meshes via Geometry Script |
| 15 | `manage_skeleton` | Authoring | Native | Skeletal meshes, sockets, physics assets, media |
| 16 | `manage_material_authoring` | Authoring | Native | Materials, expressions, landscape layers, textures |
| 17 | `manage_character` | Gameplay | Native | Characters, movement, locomotion, inventory |
| 18 | `manage_combat` | Gameplay | Native | Weapons, projectiles, damage, melee, GAS abilities |
| 19 | `manage_ai` | Gameplay | Native | AI Controllers, BT, EQS, perception, State Trees, NPCs |
| 20 | `manage_widget_authoring` | Authoring | Native | UMG widgets, layouts, bindings, HUDs |
| 21 | `manage_networking` | Gameplay | Native | Replication, RPCs, prediction, sessions, GameModes |
| 22 | `manage_volumes` | World | Native | Volumes (trigger, physics, audio, nav) and splines |
| 23 | `manage_data` | Utility | Native | Data assets, tables, save games, tags, modding/PAK/UGC |
| 24 | `manage_build` | Utility | Native | UBT, cook/package, plugins, DDC, tests, validation |
| 25 | `manage_editor_utilities` | Utility | Native | Editor modes, content browser, selection, subsystems |
| 26 | `manage_gameplay_systems` | Gameplay | Native | Targeting, checkpoints, objectives, photo mode, dialogue |
| 27 | `manage_character_avatar` | Utility | Native | MetaHuman, Groom/Hair, Mutable, Ready Player Me |
| 28 | `manage_asset_plugins` | Utility | Native | Import plugins (USD, Alembic, glTF, Datasmith, Houdini) |
| 29 | `manage_livelink` | Utility | Native | Live Link motion capture: sources, subjects, face tracking |
| 30 | `manage_xr` | Utility | Native | XR (VR/AR/MR), Virtual Production (nDisplay, DMX) |
| 31 | `manage_accessibility` | Utility | Native | Colorblind, subtitles, audio, motor, cognitive accessibility |
| 32 | `manage_ui` | Authoring | Native | UI system operations |
| 33 | `manage_gameplay_abilities` | Gameplay | Native | Gameplay Ability System (GAS) abilities |
| 34 | `manage_attribute_sets` | Gameplay | Native | GAS attribute sets |
| 35 | `manage_gameplay_cues` | Gameplay | Native | GAS gameplay cues |
| 36 | `test_gameplay_abilities` | Gameplay | Native | GAS testing utilities |

**Legend:**
- **Native** = Fully implemented in C++ plugin
- **N/A** = Not applicable (MCP-only tool)

## Handler Implementation

The plugin contains 82 handler files implementing all tool actions:

**Handler Location:** `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/*Handlers.cpp`

Key handler categories:
- **Asset Operations**: AssetHandlers, AssetWorkflowHandlers, AssetQueryHandlers, AssetPluginsHandlers
- **Actor/Editor**: ControlHandlers, EditorFunctionHandlers, EditorUtilitiesHandlers
- **Animation**: AnimationHandlers, AnimationAuthoringHandlers, ControlRigHandlers
- **Blueprints**: BlueprintHandlers, BlueprintCreationHandlers, BlueprintGraphHandlers, SCSHandlers
- **Environment**: LandscapeHandlers, FoliageHandlers, EnvironmentHandlers, WaterHandlers, WeatherHandlers
- **Effects**: EffectHandlers, NiagaraHandlers, NiagaraAuthoringHandlers, NiagaraAdvancedHandlers, NiagaraGraphHandlers
- **Sequences**: SequenceHandlers, SequencerHandlers, SequencerConsolidatedHandlers, MovieRenderHandlers
- **Audio**: AudioHandlers, AudioAuthoringHandlers, AudioMiddlewareHandlers, MetaSoundHandlers
- **Materials**: MaterialAuthoringHandlers, MaterialGraphHandlers, TextureHandlers
- **AI**: AIHandlers, BehaviorTreeHandlers, AINPCHandlers, NavigationHandlers
- **Gameplay**: CharacterHandlers, CombatHandlers, GameplaySystemsHandlers, GameplayPrimitivesHandlers, GASHandlers
- **Physics**: PhysicsDestructionHandlers, VolumeHandlers
- **Level**: LevelHandlers, LevelStructureHandlers, WorldPartitionHandlers, PCGHandlers
- **Networking**: NetworkingHandlers, SessionsHandlers
- **XR/VP**: XRPluginsHandlers, VirtualProductionHandlers, LiveLinkHandlers
- **Utility**: PropertyHandlers, DataHandlers, BuildHandlers, TestHandlers, TestingHandlers, PerformanceHandlers
- **UI**: UiHandlers, WidgetAuthoringHandlers, AccessibilityHandlers
- **Other**: LightingHandlers, PostProcessHandlers, GeometryHandlers, SkeletonHandlers, MediaHandlers, SplineHandlers, MotionDesignHandlers, CharacterAvatarHandlers, InventoryHandlers, InteractionHandlers, ModdingHandlers, InputHandlers, GameFrameworkHandlers, DebugHandlers, LogHandlers, InsightsHandlers, RenderHandlers

## Implemented Action Categories

### Asset Operations
- Import (FBX, textures via `UAssetImportTask`)
- Create materials and material instances
- Duplicate, rename, move, delete assets
- Asset Registry queries and filtering
- Dependency graph traversal
- Thumbnail generation
- Source control integration (Perforce/SVN)
- Redirector fixup

### Blueprint Operations
- Blueprint asset creation and compilation
- SimpleConstructionScript (SCS) manipulation
- Component addition and property modification
- CDO (Class Default Object) editing
- Blueprint graph node creation and connection
- Variable and function management

### Editor Control
- PIE (Play In Editor) control
- Viewport camera positioning
- Console command execution (with safety filtering)
- Screenshot capture
- View mode changes

### Level Operations
- Level creation, loading, saving
- World Partition support
- Streaming level management
- Lighting builds

### Animation and Physics
- Animation Blueprint creation
- Montage playback control
- Ragdoll physics configuration
- Chaos destruction and vehicle physics

### Environment Building
- Landscape creation and editing
- Heightmap sculpting
- Layer painting
- Foliage painting and management

### Effects
- Niagara system and emitter creation
- Niagara actor spawning
- User parameter modification
- Debug shape drawing

### Sequencer
- Level Sequence creation
- Track management (camera, animation, transform)
- Keyframe operations
- Movie Render Queue integration

### Property Operations
- JSON to FProperty conversion with type safety
- FProperty to JSON serialization
- Array, Map, and Set operations
- Support for primitives, enums, objects, soft references, structs

## Dependencies

### Required Plugin Dependencies (27 plugins)

From `McpAutomationBridge.uplugin`:

| Plugin | Required | Notes |
|--------|----------|-------|
| EditorScriptingUtilities | Yes | Asset/Actor subsystem operations |
| LevelSequenceEditor | Yes | Sequencer operations |
| Niagara | Yes | VFX system |
| ControlRig | Yes | Animation and physics tools |
| ProceduralMeshComponent | Yes | Procedural mesh generation |
| Interchange | Yes | Asset interchange framework |
| InterchangeOpenUSD | Yes | USD import/export |
| DataValidation | Yes | Asset validation |
| EnhancedInput | Yes | Input system |
| GeometryScripting | Yes | Geometry Script operations |
| ChaosCloth | Yes | Cloth simulation |
| GameplayAbilities | Yes | GAS integration |
| Metasound | Yes | MetaSound audio |
| StateTree | Yes | AI State Trees |
| SmartObjects | Yes | Smart Objects system |
| MassGameplay | Yes | Mass Entity system |
| OnlineSubsystem | Yes | Online subsystem |
| OnlineSubsystemUtils | Yes | Online utilities |
| PCG | Yes | Procedural Content Generation |
| MotionDesign | Optional | Motion Design toolset |
| IKRig | Yes | IK Rig system |
| AnimationModifierLibrary | Yes | Animation modifiers |
| SequencerScripting | Yes | Sequencer scripting |
| Paper2D | Yes | 2D sprite support |
| MotionWarping | Yes | Motion warping |
| PoseSearch | Yes | Animation pose search |
| AnimationLocomotionLibrary | Yes | Locomotion library |

### Required Unreal Engine Modules

- **Core** - Base engine functionality
- **CoreUObject** - UObject system
- **Engine** - Runtime engine
- **UnrealEd** - Editor subsystems
- **AssetTools** - Asset manipulation
- **AssetRegistry** - Asset database
- **LevelEditor** - Level editing subsystems
- **BlueprintGraph** - Blueprint editing
- **LevelSequence** - Sequencer
- **Landscape** / **LandscapeEditor** - Terrain tools
- **NiagaraEditor** - VFX editing
- **MaterialEditor** - Material editing

### Optional Modules (Auto-Detected)

- **SubobjectDataInterface** - UE 5.7+ Blueprint SCS subsystem
- **SourceControl** - Version control integration

## Installation and Configuration

### Plugin Installation

1. Copy `plugins/McpAutomationBridge/` to your project's `Plugins/` directory
2. Regenerate project files
3. Enable plugin via **Edit > Plugins > MCP Automation Bridge**
4. Restart editor

### Configuration (Project Settings > Plugins > MCP Automation Bridge)

- **Server Host**: MCP server address (default: `127.0.0.1`)
- **Server Port**: WebSocket port (default: `8091`)
- **Capability Token**: Optional security token
- **Reconnect Enabled**: Auto-reconnect on disconnect
- **Reconnect Delay**: Delay between reconnection attempts (default: 5s)
- **Heartbeat Timeout**: Connection health timeout
- **Ticker Interval**: Subsystem tick frequency (default: 0.25s)

### Environment Variables (Override Settings)

- `MCP_AUTOMATION_HOST` - Server host override
- `MCP_AUTOMATION_PORT` - Server port override
- `MCP_AUTOMATION_CAPABILITY_TOKEN` - Security token
- `MCP_IGNORE_SUBOBJECTDATA` - Disable SubobjectData detection
- `MCP_FORCE_SUBOBJECTDATA` - Force SubobjectData module linkage

## Current Version Status (v0.1.0)

### Completed Features

1. **WebSocket Transport** - Custom lightweight WebSocket client with no external dependencies
2. **Asset Operations** - Complete native asset pipeline (import, create, modify, delete)
3. **Property Marshaling** - Full `FProperty` system integration with type safety
4. **Editor Functions** - Native implementations of common editor operations
5. **Sequence/Animation** - Level Sequence Editor and animation blueprint integration
6. **Environment Tools** - Landscape and foliage manipulation
7. **Material Graph** - Material node creation and editing
8. **Source Control** - Perforce/SVN integration
9. **Telemetry** - Request tracking and performance metrics
10. **Security** - Capability token authentication
11. **Camera Control** - Native viewport camera positioning
12. **Python-Free Architecture** - 100% native C++ implementation

### In Progress

1. **Blueprint SCS Enhancements** - Improving UE 5.6+ SubobjectData subsystem compatibility
2. **Modal Dialog Interception** - Handling blocking editor dialogs

### Roadmap

#### High Priority
1. **Complete Blueprint SCS API** - Finalize SimpleConstructionScript manipulation for UE 5.6+
2. **Physics System Integration** - Native physics force application
3. **Modal Dialog Mediation** - Intercept blocking dialogs

#### Medium Priority
4. **Hot Reload Support** - Update plugin without editor restart
5. **Enhanced Telemetry** - Expanded metrics and diagnostics
6. **Batch Operations** - Multi-operation transactions

#### Low Priority
7. **Marketplace Distribution** - Packaged plugin distribution
8. **Blueprint Visual Editing** - Direct Blueprint graph manipulation

## UE 5.7 Safety Guidelines

When extending the plugin, follow these critical safety patterns:

- **NO `UPackage::SavePackage()`**: Use `McpSafeAssetSave` helper (access violations in 5.7)
- **SCS Ownership**: Component templates via `SCS->CreateNode()` + `AddNode()`
- **`ANY_PACKAGE`**: Deprecated. Use `nullptr` for path lookups
- **GetActiveWorld()**: Use helper instead of `GEditor->GetEditorWorldContext().World()`
- **TObjectIterator**: Unsafe in UE 5.7. Use `GetDerivedClasses()` helper
- **FindActorByName**: Use `FindActorByLabelOrName()` helper

## Contributions

Contributions welcome! Please open an issue or discussion before starting major work to ensure alignment with the roadmap.

### Development Guidelines

- Follow Unreal Engine C++ coding standards
- Add handler functions to appropriate `McpAutomationBridge_*Handlers.cpp` files
- Register new handlers in `InitializeHandlers()`
- Update `McpAutomationBridgeSubsystem.h` with handler declarations
- Add comprehensive error handling with structured error codes
- Test across multiple UE versions (5.0-5.7)
- Document new actions in this file
- **No Python dependencies** - All new features must be native C++
