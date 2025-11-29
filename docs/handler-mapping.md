# Handler Mappings

This document maps the TypeScript tool definitions to their corresponding C++ handlers in the Unreal Engine plugin.

## System Tools (`system-handlers.ts` / `EditorFunctionHandlers`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `system_control` | `run_ubt` | *None* | Handled purely in TypeScript via `child_process`. |
| `system_control` | `execute_console_command` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleExecuteEditorFunction` |
| `system_control` | `run_tests` | `McpAutomationBridge_TestHandlers.cpp` | `HandleTestAction` (if implemented) |
| `system_control` | `subscribe` (logs) | `McpAutomationBridge_LogHandlers.cpp` | `HandleLogAction` |
| `system_control` | `spawn_category` (debug) | `McpAutomationBridge_DebugHandlers.cpp` | `HandleDebugAction` |
| `system_control` | `start_session` (insights) | `McpAutomationBridge_InsightsHandlers.cpp` | `HandleInsightsAction` |
| `system_control` | `lumen_update_scene` | `McpAutomationBridge_RenderHandlers.cpp` | `HandleRenderAction` |

## Asset Tools (`assets.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `manage_asset` | `create_asset` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` |
| `manage_asset` | `delete_asset` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | |
| `manage_asset` | `duplicate_asset` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | |
| `manage_asset` | `rename_asset` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | |
| `manage_asset` | `create_render_target` | `McpAutomationBridge_RenderHandlers.cpp` | `HandleRenderAction` |
| `manage_asset` | `nanite_rebuild_mesh` | `McpAutomationBridge_RenderHandlers.cpp` | `HandleRenderAction` |
| `manage_asset` | `add_material_node` | `McpAutomationBridge_MaterialGraphHandlers.cpp` | `HandleMaterialGraphAction` |
| `manage_asset` | `connect_material_pins` | `McpAutomationBridge_MaterialGraphHandlers.cpp` | |
| `manage_asset` | `add_bt_node` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` |

## Blueprint Tools (`blueprint.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `manage_blueprint` | `create` | `McpAutomationBridge_BlueprintHandlers.cpp` | |
| `manage_blueprint` | `add_component` | `McpAutomationBridge_BlueprintHandlers.cpp` | |
| `manage_blueprint` | `modify_scs` | `McpAutomationBridge_BlueprintHandlers.cpp` | |
| `manage_blueprint` | `create_node` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` |
| `manage_blueprint` | `connect_pins` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | |

## Environment Tools (`environment.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `build_environment` | `add_foliage_instances` | `McpAutomationBridge_FoliageHandlers.cpp` | Dispatched via `HandleBuildEnvironmentAction` to `HandlePaintFoliage` |
| `build_environment` | `get_foliage_instances` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandleGetFoliageInstances` |
| `build_environment` | `remove_foliage` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandleRemoveFoliage` |
| `build_environment` | `paint_landscape` | `McpAutomationBridge_LandscapeHandlers.cpp` | `HandlePaintLandscapeLayer` |
| `build_environment` | `sculpt_landscape` | `McpAutomationBridge_LandscapeHandlers.cpp` | `HandleSculptLandscape` |
| `build_environment` | `create_procedural_terrain` | `McpAutomationBridge_EnvironmentHandlers.cpp` | `HandleCreateProceduralTerrain` |

## Effects Tools (`effect.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `manage_effect` | `niagara` | `McpAutomationBridge_EffectHandlers.cpp` | |
| `manage_effect` | `add_niagara_module` | `McpAutomationBridge_NiagaraGraphHandlers.cpp` | `HandleNiagaraGraphAction` |
| `manage_effect` | `connect_niagara_pins` | `McpAutomationBridge_NiagaraGraphHandlers.cpp` | |

## Actor Tools (`actors.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `control_actor` | `spawn_actor` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleActorAction` |
| `control_actor` | `set_location` | `McpAutomationBridge_ControlHandlers.cpp` | |
| `control_actor` | `get_components` | `McpAutomationBridge_ControlHandlers.cpp` | |

## Sequence Tools (`sequence.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `manage_sequence` | `create` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceCreate` |
| `manage_sequence` | `add_actor` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAddActor` |
| `manage_sequence` | `play` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequencePlay` |

## Editor Tools (`editor.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `control_editor` | `simulate_input` | `McpAutomationBridge_UiHandlers.cpp` | `HandleUiAutomationAction` |
| `control_editor` | `set_camera` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | |

## Level Tools (`level.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `manage_level` | `load_cells` | `McpAutomationBridge_WorldPartitionHandlers.cpp` | `HandleWorldPartitionAction` |
| `manage_level` | `set_datalayer` | `McpAutomationBridge_WorldPartitionHandlers.cpp` | |
| `manage_level` | `load` | `McpAutomationBridge_LevelHandlers.cpp` | |

## Audio Tools (`audio.ts`)

| Tool Name | Action | C++ Handler | Notes |
| :--- | :--- | :--- | :--- |
| `manage_audio` | `create_sound_cue` | `McpAutomationBridge_AudioHandlers.cpp` | `HandleAudioAction` |
| `manage_audio` | `play_sound_at_location` | `McpAutomationBridge_AudioHandlers.cpp` | |