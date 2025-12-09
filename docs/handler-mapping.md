# Handler Mappings

This document maps the TypeScript tool definitions to their corresponding C++ handlers in the Unreal Engine plugin.

## 1. Asset Manager (`manage_asset`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `list` | `McpAutomationBridge_AssetQueryHandlers.cpp` | `HandleAssetQueryAction` | |
| `search_assets` | `McpAutomationBridge_AssetQueryHandlers.cpp` | `HandleAssetQueryAction` | |
| `import` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `duplicate` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `rename` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `move` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `delete` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `delete_assets` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleBulkDeleteAssets` | |
| `create_folder` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `get_dependencies` | `McpAutomationBridge_AssetQueryHandlers.cpp` | `HandleGetAssetDependencies` | |
| `get_source_control_state` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleSourceControlCheckout` | |
| `analyze_graph` | `McpAutomationBridge_AssetQueryHandlers.cpp` | `HandleGetAssetReferences` | |
| `create_thumbnail` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleGenerateThumbnail` | |
| `set_tags` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `validate` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `fixup_redirectors` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleFixupRedirectors` | |
| `generate_report` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `create_material` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `create_material_instance` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `create_render_target` | `McpAutomationBridge_RenderHandlers.cpp` | `HandleRenderAction` | |
| `nanite_rebuild_mesh` | `McpAutomationBridge_RenderHandlers.cpp` | `HandleRenderAction` | |
| `add_material_node` | `McpAutomationBridge_MaterialGraphHandlers.cpp` | `HandleAddMaterialExpression` | |
| `connect_material_pins` | `McpAutomationBridge_MaterialGraphHandlers.cpp` | `HandleCreateMaterialNodes` | |
| `remove_material_node` | `McpAutomationBridge_MaterialGraphHandlers.cpp` | `HandleCreateMaterialNodes` | |
| `add_bt_node` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `connect_bt_nodes` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |

## 2. Blueprint Manager (`manage_blueprint`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create` | `McpAutomationBridge_BlueprintCreationHandlers.cpp` | `HandleBlueprintAction` | |
| `get_blueprint` | `McpAutomationBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `compile` | `McpAutomationBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `add_component` | `McpAutomationBridge_SCSHandlers.cpp` | `HandleBlueprintAction` | Uses `SubobjectData` in UE 5.7+ |
| `set_default` | `McpAutomationBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `modify_scs` | `McpAutomationBridge_SCSHandlers.cpp` | `HandleBlueprintAction` | |
| `get_scs` | `McpAutomationBridge_SCSHandlers.cpp` | `HandleBlueprintAction` | |
| `create_node` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `delete_node` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `connect_pins` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `break_pin_links` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `set_node_property` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `add_variable` | `McpAutomationBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `add_function` | `McpAutomationBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |

## 3. Actor Control (`control_actor`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `spawn` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `spawn_blueprint` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `delete` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `delete_by_tag` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `duplicate` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `apply_force` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `set_transform` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `get_transform` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `set_visibility` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `add_component` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | Runtime component addition |
| `add_tag` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `find_by_tag` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `list` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `attach` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `detach` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |

## 4. Editor Control (`control_editor`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `play` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `stop` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `set_camera` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `console_command` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleConsoleCommandAction` | |
| `screenshot` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `simulate_input` | `McpAutomationBridge_UiHandlers.cpp` | `HandleUiAction` | |
| `create_bookmark` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |

## 5. Level Manager (`manage_level`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `load` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleLevelAction` | |
| `save` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleLevelAction` | |
| `create_level` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleLevelAction` | |
| `create_light` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `build_lighting` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `load_cells` | `McpAutomationBridge_WorldPartitionHandlers.cpp` | `HandleWorldPartitionAction` | |
| `set_datalayer` | `McpAutomationBridge_WorldPartitionHandlers.cpp` | `HandleWorldPartitionAction` | |

## 6. Animation & Physics (`animation_physics`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_animation_bp` | `McpAutomationBridge_AnimationHandlers.cpp` | `HandleCreateAnimBlueprint` | |
| `play_montage` | `McpAutomationBridge_AnimationHandlers.cpp` | `HandlePlayAnimMontage` | |
| `setup_ragdoll` | `McpAutomationBridge_AnimationHandlers.cpp` | `HandleSetupRagdoll` | |
| `configure_vehicle` | `McpAutomationBridge_AnimationHandlers.cpp` | `HandleAnimationPhysicsAction` | |

## 7. Effects Manager (`manage_effect`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `niagara` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleEffectAction` | |
| `spawn_niagara` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleSpawnNiagaraActor` | |
| `debug_shape` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleEffectAction` | |
| `create_niagara_system` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleCreateNiagaraSystem` | |
| `create_niagara_emitter` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleCreateNiagaraEmitter` | |
| `add_niagara_module` | `McpAutomationBridge_NiagaraGraphHandlers.cpp` | `HandleNiagaraGraphAction` | |

## 8. Environment Builder (`build_environment`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_landscape` | `McpAutomationBridge_LandscapeHandlers.cpp` | `HandleCreateLandscape` | |
| `sculpt` | `McpAutomationBridge_LandscapeHandlers.cpp` | `HandleSculptLandscape` | |
| `paint_foliage` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandlePaintFoliage` | |
| `add_foliage_instances` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandleAddFoliageInstances` | |
| `get_foliage_instances` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandleGetFoliageInstances` | |
| `remove_foliage` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandleRemoveFoliage` | |
| `create_procedural_terrain` | `McpAutomationBridge_EnvironmentHandlers.cpp` | `HandleCreateProceduralTerrain` | |

## 9. System Control (`system_control`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `profile` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `show_fps` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_quality` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `execute_command` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleExecuteEditorFunction` | |
| `console_command` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleConsoleCommandAction` | |
| `run_ubt` | *None* | *None* | Handled in TypeScript (`child_process`) |
| `run_tests` | `McpAutomationBridge_TestHandlers.cpp` | `HandleTestAction` | |
| `subscribe` | `McpAutomationBridge_LogHandlers.cpp` | `HandleLogAction` | |
| `spawn_category` | `McpAutomationBridge_DebugHandlers.cpp` | `HandleDebugAction` | |
| `start_session` | `McpAutomationBridge_InsightsHandlers.cpp` | `HandleInsightsAction` | |
| `lumen_update_scene` | `McpAutomationBridge_RenderHandlers.cpp` | `HandleRenderAction` | |

## 10. Sequencer (`manage_sequence`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `add_actor` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `play` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `add_keyframe` | `McpAutomationBridge_SequencerHandlers.cpp` | `HandleAddSequencerKeyframe` | |
| `add_camera` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleAddCameraTrack` | |

## 11. Introspection (`inspect`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `inspect_object` | `McpAutomationBridge_PropertyHandlers.cpp` | `HandleInspectAction` | |
| `set_property` | `McpAutomationBridge_PropertyHandlers.cpp` | `HandleSetObjectProperty` | |
| `get_property` | `McpAutomationBridge_PropertyHandlers.cpp` | `HandleGetObjectProperty` | |
| `get_components` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `list_objects` | `McpAutomationBridge_PropertyHandlers.cpp` | `HandleInspectAction` | |

## 12. Audio Manager (`manage_audio`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_sound_cue` | `McpAutomationBridge_AudioHandlers.cpp` | `HandleAudioAction` | |
| `play_sound_at_location` | `McpAutomationBridge_AudioHandlers.cpp` | `HandleAudioAction` | |
| `create_audio_component` | `McpAutomationBridge_AudioHandlers.cpp` | `HandleAudioAction` | |

## 13. Behavior Tree Manager (`manage_behavior_tree`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `add_node` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `connect_nodes` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `remove_node` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `break_connections` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `set_node_properties` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |