# MCP Automation Bridge Plugin

The MCP Automation Bridge is a production-ready Unreal Editor plugin that enables direct communication between the MCP server and Unreal Engine, providing **100% native C++ implementations** for all automation tasks. This document describes the current implementation and future roadmap.

## Goals
- **Direct SCS Access** – expose Blueprint SimpleConstructionScript mutations through a curated C++ API that the MCP server can call.
- **Typed Property Marshaling** – relay incoming JSON payloads through Unreal's `FProperty` system so class/enum/soft object references resolve without manual string coercion.
- **Asset Lifecycle Helpers** – wrap save/move/delete flows with redirector fix-up, source control hooks, and safety prompts suppressed via automation policies.
- **Modal Dialog Mediation** – surface blocking dialogs to the MCP server with explicit continue/cancel channels instead of stalling automation.
- **Native-Only Architecture** – all operations implemented in C++ without Python dependencies for maximum reliability and performance.

## Architecture Sketch
- Editor plugin registers a `UMcpAutomationBridge` subsystem.
- Subsystem subscribes to a local WebSocket or named pipe opened by the Node MCP server when it needs elevated actions.
- Each elevated command includes a capability token so the plugin can enforce an allow-list (exposed through project settings) and fail gracefully if disabled.
- Results are serialized back to the MCP server with structured warnings so the client can still prompt the user when manual intervention is required.

## Plugin Architecture (Current: v0.6.0)

### Core Components
- **Plugin Location**: `plugins/McpAutomationBridge/` (source) and `Public/McpAutomationBridge/` (distribution)
- **Module Type**: Editor-only subsystem (`UEditorSubsystem`)
- **Main Class**: `UMcpAutomationBridgeSubsystem` - manages WebSocket connections, request routing, and automation execution
- **WebSocket Implementation**: `FMcpBridgeWebSocket` - custom lightweight WebSocket client (no external dependencies)
- **Settings**: `UMcpAutomationBridgeSettings` - configurable via **Project Settings ▸ Plugins ▸ MCP Automation Bridge**

### Connection Management
- **WebSocket Server Mode**: Plugin connects TO the MCP server's WebSocket listener (default: `ws://127.0.0.1:8091`)
- **Handshake Protocol**: `bridge_hello` → capability token validation → `bridge_ack`
- **Reconnection**: Automatic with exponential backoff (configurable delay, 5s default)
- **Heartbeat**: Optional heartbeat tracking for connection health monitoring
- **Capability Token**: Optional security layer for authentication
- **Multi-Port Support**: Can connect to multiple server ports simultaneously

### Request Processing
- **Thread-Safe Queue**: Incoming requests queued and processed sequentially on game thread
- **Telemetry**: Tracks success/failure rates, execution times, and action statistics
- **Error Handling**: Structured error responses with error codes and retry flags
- **Timeout Management**: Configurable timeouts for long-running operations

## Server Integration (0.1.0)
- `src/automation-bridge.ts` spins up a lightweight WebSocket server (default `ws://127.0.0.1:8091`) guarded by an optional capability token.
- Handshake flow: editor sends `bridge_hello` → server validates capability token → server responds with `bridge_ack` and caches the socket for future elevated commands.
- Environment flags: `MCP_AUTOMATION_HOST`, `MCP_AUTOMATION_PORT`, `MCP_AUTOMATION_CAPABILITY_TOKEN`, and `MCP_AUTOMATION_CLIENT_MODE` allow operators to relocate or disable the listener without code changes.
- Health endpoint (`ue://health`) now surfaces bridge connectivity status so MCP clients can confirm when the plugin is online.

## Implemented Actions (Current)

### 1. Asset Operations (`HandleAssetAction`)
✅ **Fully Implemented** - Native C++ using UE Asset Tools
- `import_asset_deferred` - Native `UAssetImportTask` with FBX/texture support
- `create_material` - `UMaterialFactoryNew` for material creation
- `create_material_instance` - Material instance creation with parameter overrides
- `duplicate_asset` - Asset duplication with overwrite control
- `rename_asset` - Asset renaming with conflict detection
- `move_asset` - Asset moving with optional redirector fixup
- `delete_assets` - Bulk asset deletion with dependency handling
- `list` - Asset Registry directory listing with filtering
- `create_folder` - Content folder creation
- `get_dependencies` - Asset dependency graph traversal
- `create_thumbnail` - Thumbnail generation
- `set_tags` - Asset metadata tagging
- `generate_report` - Asset reports
- `validate` - Asset existence validation
- `get_source_control_state` - Check checkout/revision status
- `analyze_graph` - Deep dependency analysis

### 2. Editor Function Execution (`HandleExecuteEditorFunction`)
✅ **Fully Implemented** - Native subsystem operations
- `execute_console_command` - Direct `GEditor->Exec()` console command execution
- `GET_ALL_ACTORS` - `UEditorActorSubsystem::GetAllLevelActors()`
- `SPAWN_ACTOR` - `UEditorActorSubsystem::SpawnActorFromClass()`
- `DELETE_ACTOR` - `UEditorActorSubsystem::DestroyActor()`
- `ASSET_EXISTS` - `UEditorAssetLibrary::DoesAssetExist()`
- `SET_VIEWPORT_CAMERA` - `UUnrealEditorSubsystem::SetLevelViewportCameraInfo()`
- `CREATE_LEVEL` - Level creation via subsystem
- `LOAD_LEVEL` - Level loading
- `SAVE_LEVEL` - Level saving
- `BUILD_LIGHTING` - `ULevelEditorSubsystem::BuildLightMaps()`
- `LIST_ACTOR_COMPONENTS` - Enumerate components for any editor actor via `UEditorActorSubsystem`
- `RESOLVE_OBJECT` - Resolve assets or classes and surface best-effort metadata for inspection tools
- `SAVE_DIRTY_PACKAGES` / `SAVE_ALL_DIRTY_PACKAGES` - Trigger native dirty-package saves through `UEditorLoadingAndSavingUtils`
- `SAVE_ASSET` - Persist a specific asset path using throttled `UEditorAssetLibrary` helpers
- `SET_BLUEPRINT_DEFAULT` - Routes to native blueprint default editing without Python fallbacks

### 3. Property Operations (`HandleSetObjectProperty`, `HandleGetObjectProperty`)
✅ **Fully Implemented** - Typed `FProperty` marshaling
- **set_object_property** - JSON → `FProperty` conversion with type safety
- **get_object_property** - `FProperty` → JSON serialization
- **Array Operations**: append, remove, insert, get_element, set_element, clear
- **Map Operations**: set_value, get_value, remove_key, has_key, get_keys, clear
- **Set Operations**: add, remove, contains, clear
- **Supported Types**: primitives, enums, objects, soft object paths, structs (Vector/Rotator), arrays, maps, sets

### 4. Blueprint Operations (`HandleBlueprintAction`)
⚠️ **Partially Implemented** - SCS modification in progress
- Blueprint asset creation ✅
- SimpleConstructionScript (SCS) node manipulation ⚠️
- Component addition to blueprints ⚠️
- Blueprint compilation and saving ✅
- CDO (Class Default Object) property modification ✅
- Blueprint default overrides via `SET_BLUEPRINT_DEFAULT` requests ✅

### 5. Sequence/Sequencer Operations
✅ **Fully Implemented** - Level Sequence Editor integration
- **HandleSequenceAction** - Sequence creation and management
- **HandleAddSequencerKeyframe** - Keyframe operations
- **HandleManageSequencerTrack** - Track management
- **HandleAddCameraTrack** - Camera track addition
- **HandleAddAnimationTrack** - Animation track addition
- **HandleAddTransformTrack** - Transform track addition

### 6. Effect Operations (`HandleEffectAction`)
✅ **Fully Implemented** - Niagara and debug visualization
- Niagara system creation (`UNiagaraSystemFactoryNew`)
- Niagara emitter creation
- Spawn Niagara actors in level
- Modify Niagara user parameters
- Debug shape drawing (lines, boxes, spheres, arrows)
- Dynamic light spawning
- Effect cleanup operations

### 7. Animation & Physics (`HandleAnimationPhysicsAction`)
✅ **Fully Implemented** - Animation blueprint and physics setup
- **HandleCreateAnimBlueprint** - Animation blueprint creation
- **HandlePlayAnimMontage** - Montage playback control
- **HandleSetupRagdoll** - Ragdoll physics configuration
- Physics asset manipulation
- Skeletal mesh animation control

### 8. Environment Building (`HandleBuildEnvironmentAction`)
✅ **Fully Implemented** - Landscape and foliage tools
- **HandleCreateLandscape** - Landscape actor creation
- **HandleEditLandscape** - Landscape editing dispatcher
- **HandleModifyHeightmap** - Heightmap sculpting
- **HandlePaintLandscapeLayer** - Layer painting
- **HandlePaintFoliage** - Foliage painting with density control
- **HandleRemoveFoliage** - Foliage removal
- **HandleGetFoliageInstances** - Foliage instance queries

### 9. Material Graph Operations (`HandleCreateMaterialNodes`)
✅ **Fully Implemented** - Material editor graph manipulation
- **HandleAddMaterialTextureSample** - Add texture sample nodes
- **HandleAddMaterialExpression** - Add material expression nodes
- Material graph node connections
- Material parameter setup

### 10. Asset Workflow (`HandleSourceControl*`, `HandleFixupRedirectors`, etc.)
✅ **Fully Implemented** - Production asset pipeline support
- **HandleSourceControlCheckout** - Perforce/SVN checkout
- **HandleSourceControlSubmit** - Perforce/SVN submit
- **HandleFixupRedirectors** - Redirector cleanup after moves
- **HandleBulkRenameAssets** - Batch asset renaming
- **HandleBulkDeleteAssets** - Batch asset deletion
- **HandleGenerateThumbnail** - Thumbnail generation

### 11. Actor & Editor Control
✅ **Fully Implemented** - Viewport and actor manipulation
- PIE (Play In Editor) control - `ULevelEditorSubsystem::EditorPlaySimulate()`
- Camera positioning and rotation - `UUnrealEditorSubsystem::SetLevelViewportCameraInfo()`
- View mode changes - Native `GEditor->Exec()` with safety validation
- Actor spawning with class resolution
- Actor deletion with label matching
- Actor property modification

### 12. Python Execution (`HandleExecuteEditorPython`)
❌ **REMOVED** - Python execution is no longer supported
- All automation is now implemented natively in C++
- `execute_editor_python` action returns error: "Python execution is no longer supported"
- Improves security, reliability, and performance
- **Migration**: Use native automation actions instead

## Tool Coverage Matrix (Updated)

| Consolidated Tool | Actions | Bridge Status | Notes |
|-------------------|---------|---------------|-------|
| **manage_asset** | `list` | ✅ Native | Asset Registry native listing |
| | `import` | ✅ Native | `UAssetImportTask` FBX/texture import |
| | `create_material` | ✅ Native | `UMaterialFactoryNew` native creation |
| | `create_material_instance` | ✅ Native | Native material instance factory |
| | `duplicate` / `rename` / `move` | ✅ Native | `UEditorAssetLibrary` + `AssetTools` |
| | `delete` | ✅ Native | Bulk deletion with dependency handling |
| **control_actor** | `spawn` | ✅ Native | `UEditorActorSubsystem::SpawnActorFromClass()` |
| | `delete` | ✅ Native | `UEditorActorSubsystem::DestroyActor()` |
| | `apply_force` | 🔧 Planned | Physics forces native implementation pending |
| **control_editor** | `play` / `stop` / `pause` | ✅ Native | `ULevelEditorSubsystem` PIE control |
| | `set_camera` | ✅ Native | `UUnrealEditorSubsystem::SetLevelViewportCameraInfo()` |
| | `set_view_mode` | ✅ Native | Safe viewmode validation + `GEditor->Exec()` |
| | `console_command` | ✅ Native | `GEditor->Exec()` direct execution |
| **manage_level** | `load` / `save` / `stream` | ✅ Native | Editor function handlers for level ops |
| | `create_level` | ✅ Native | Native `CREATE_LEVEL` function |
| | `create_light` | ✅ Native | `SPAWN_ACTOR` with light class |
| | `build_lighting` | ✅ Native | `ULevelEditorSubsystem::BuildLightMaps()` |
| **animation_physics** | `create_animation_bp` | ✅ Native | Animation blueprint factory |
| | `play_montage` | ✅ Native | Native montage playback control |
| | `setup_ragdoll` | ✅ Native | Ragdoll physics configuration |
| | `configure_vehicle` | ⚠️ Partial | Complex vehicle setup in progress |
| **create_effect** | `niagara` | ✅ Native | `UNiagaraSystemFactoryNew` native creation |
| | `spawn_niagara` | ✅ Native | Native Niagara actor spawning |
| | `debug_shape` | ✅ Native | Debug line/box/sphere drawing |
| | `dynamic_light` | ✅ Native | Dynamic light spawning |
| **manage_blueprint** | `create` | ✅ Native | Blueprint asset creation |
| | `add_component` | ✅ Native | SCS manipulation via native APIs; UE 5.7+ SubobjectDataInterface support added |
| | `edit_defaults` | ✅ Native | CDO property modification |
| **build_environment** | `create_landscape` | ✅ Native | Landscape actor creation |
| | `sculpt` / `paint` | ✅ Native | Heightmap/layer editing |
| | `add_foliage` | ✅ Native | Foliage painting with density control |
| **system_control** | `profile` / `show_fps` | ✅ Native | Console command execution |
| | `set_quality` | ✅ Native | Quality settings via console |
| | `screenshot` | ✅ Native | Screenshot capture commands |
| **console_command** | (all) | ✅ Native | Direct `GEditor->Exec()` with safety filtering |
| **execute_python** | (all) | ❌ Removed | Python execution removed; use native actions |
| **manage_sequence** | `create` / `add_track` | ✅ Native | Level Sequence Editor native operations |
| | `keyframe` | ✅ Native | Native keyframe manipulation |
| **inspect** | `get_property` | ✅ Native | `FProperty` → JSON serialization |
| | `set_property` | ✅ Native | JSON → `FProperty` typed marshaling |
| | `inspect_cdo` | ✅ Native | Inspect any Blueprint CDO without spawning an actor. CDO properties via reflection; for Actor BPs enumerates CDO components with effective overrides. Supports detailed, componentName, propertyNames filters. |
| | `list` | ✅ Native | Actor/asset listing via subsystems |
| **manage_audio** | `create_sound_cue` | ✅ Native | Sound Cue asset creation |
| | `play_sound_at_location` | ✅ Native | 3D spatial sound playback |
| | `create_audio_component` | ✅ Native | Audio component creation |
| **manage_behavior_tree** | `add_node` | ✅ Native | Behavior Tree node creation |
| | `connect_nodes` | ✅ Native | Node connection management |
| | `set_node_properties` | ✅ Native | Node property editing |

**Legend:**
- ✅ **Native** = Fully implemented in C++ plugin
- ⚠️ **Partial** = Some operations implemented; work in progress
- 🔧 **Planned** = Designed but not yet implemented
- ❌ **Removed** = Feature removed (Python execution)

## Current Version Status (v0.6.0)

### ✅ Completed Features
1. ✔️ **WebSocket Transport** - Custom lightweight WebSocket client with no external dependencies
2. ✔️ **Asset Operations** - Complete native asset pipeline (import, create, modify, delete)
3. ✔️ **Property Marshaling** - Full `FProperty` system integration with type safety
4. ✔️ **Editor Functions** - Native implementations of common editor operations
5. ✔️ **Sequence/Animation** - Level Sequence Editor and animation blueprint integration
6. ✔️ **Environment Tools** - Landscape and foliage manipulation
7. ✔️ **Material Graph** - Material node creation and editing
8. ✔️ **Source Control** - Perforce/SVN integration
9. ✔️ **Telemetry** - Request tracking and performance metrics
10. ✔️ **Security** - Capability token authentication
11. ✔️ **Camera Control** - Native viewport camera positioning via `SetLevelViewportCameraInfo()`
12. ✔️ **Python-Free Architecture** - 100% native C++ implementation

### ⚠️ In Progress
1. **Blueprint SCS Enhancements** - Improving UE 5.6+ SubobjectData subsystem compatibility
   - Native component addition and modification (UE 5.7+ supported)
   - Full SCS tree manipulation
2. **Modal Dialog Interception** - Handling blocking editor dialogs
   - Asset save prompts
   - Package checkout dialogs

### 📅 Roadmap (v0.7.0+)

#### High Priority
1. **Complete Blueprint SCS API** - Finalize SimpleConstructionScript manipulation for UE 5.6+
   - Leverage `SubobjectData` subsystem when available
   - Fallback to legacy reflection-based approach for older engines
   - Native component property editing

2. **Physics System Integration** - Native physics force application
   - `UPrimitiveComponent::AddForce()` wrappers
   - Physics constraint manipulation
   - Impulse and torque application
   - Vehicle physics configuration

3. **Modal Dialog Mediation** - Intercept blocking dialogs
   - Asset save prompts
   - Package checkout dialogs
   - Build lighting confirmation
   - Timeout-based fallback responses

#### Medium Priority
4. **Hot Reload Support** - Update plugin without editor restart
   - Dynamic handler registration
   - State preservation across reloads

5. **Enhanced Telemetry** - Expanded metrics and diagnostics
   - Per-action success/failure rates
   - Performance profiling data
   - Connection health history

6. **Batch Operations** - Multi-operation transactions
   - Transactional asset workflows
   - Undo/redo support
   - Rollback on failure

7. **Advanced Security** - Enhanced authentication
   - Session token support
   - Per-action capability tokens
   - Client certificate validation

#### Low Priority
8. **Marketplace Distribution** - Packaged plugin distribution
   - Pre-compiled binaries for multiple UE versions
   - Simplified installation process
   - Documentation and samples

9. **Blueprint Visual Editing** - Direct Blueprint graph manipulation
   - Node creation and connection
   - Function/macro creation
   - Event graph editing

## Dependencies

### Required Unreal Engine Modules
- **Core** - Base engine functionality
- **CoreUObject** - UObject system
- **Engine** - Runtime engine
- **UnrealEd** - Editor subsystems
- **EditorScriptingUtilities** - Editor scripting APIs
- **AssetTools** - Asset manipulation
- **AssetRegistry** - Asset database
- **LevelEditor** - Level editing subsystems
- **BlueprintGraph** - Blueprint editing
- **LevelSequence** / **LevelSequenceEditor** - Sequencer
- **Landscape** / **LandscapeEditor** - Terrain tools
- **Niagara** / **NiagaraEditor** - VFX system
- **MaterialEditor** - Material editing

### Optional Modules (Auto-Detected)
- **SubobjectDataInterface** - UE 5.7+ Blueprint SCS subsystem
- **ControlRig** - Animation and physics tools
- **SourceControl** - Version control integration

## Installation & Configuration

### Plugin Installation
1. Copy `Public/McpAutomationBridge/` to your project's `Plugins/` directory
2. Regenerate project files
3. Enable plugin via **Edit ▸ Plugins ▸ MCP Automation Bridge**
4. Restart editor

### Configuration (Project Settings ▸ Plugins ▸ MCP Automation Bridge)
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

### Deprecated Settings (Removed)
- ~~`MCP_ALLOW_PYTHON_FALLBACKS`~~ - Python execution has been removed
- ~~`Allow Python Fallbacks`~~ - Setting removed from project settings
- ~~`bAllowPythonFallbacks`~~ - Configuration property removed
- ~~`bAllowAllPythonFallbacks`~~ - Configuration property removed

## Contributions

Contributions welcome! Please open an issue or discussion before starting major work to ensure alignment with the roadmap.

### Development Guidelines
- Follow Unreal Engine C++ coding standards
- Add handler functions to appropriate `McpAutomationBridge_*Handlers.cpp` files
- Register new handlers in `ProcessAutomationRequest()`
- Update `McpAutomationBridgeSubsystem.h` with handler declarations
- Add comprehensive error handling with structured error codes
- Test across multiple UE versions (5.0-5.7)
- Document new actions in this file
- **No Python dependencies** - All new features must be native C++
