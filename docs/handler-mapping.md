# Handler Mappings

This document maps the TypeScript tool definitions to their corresponding C++ handlers in the Unreal Engine plugin.

> **Note (Phase 53):** The following tools have been merged and are now deprecated:
> - `manage_blueprint_graph` → merged into `manage_blueprint`
> - `manage_audio_authoring` → merged into `manage_audio`
> - `manage_niagara_authoring` → merged into `manage_effect`
> - `manage_animation_authoring` → merged into `animation_physics`
>
> The deprecated tools still work for backward compatibility but log deprecation warnings.

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
| `get_asset` | `McpAutomationBridge_AssetWorkflowHandlers.cpp` | `HandleGetAsset` | |
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

## 17. Input Manager (`manage_input`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_input_action` | `McpAutomationBridge_InputHandlers.cpp` | `HandleInputAction` | |
| `create_input_mapping_context` | `McpAutomationBridge_InputHandlers.cpp` | `HandleInputAction` | |
| `add_mapping` | `McpAutomationBridge_InputHandlers.cpp` | `HandleInputAction` | |
| `remove_mapping` | `McpAutomationBridge_InputHandlers.cpp` | `HandleInputAction` | |
| `add_variable` | `McpAutomationBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `add_function` | `McpAutomationBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `add_event` | `McpAutomationBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | Supports custom & standard events |

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
| `export_level` | `McpAutomationBridge_LevelHandlers.cpp` | `HandleLevelAction` | |
| `import_level` | `McpAutomationBridge_LevelHandlers.cpp` | `HandleLevelAction` | |
| `load_cells` | `McpAutomationBridge_WorldPartitionHandlers.cpp` | `HandleWorldPartitionAction` | |
| `set_datalayer` | `McpAutomationBridge_WorldPartitionHandlers.cpp` | `HandleWorldPartitionAction` | |

## 6. Lighting Manager (`manage_lighting`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `spawn_light` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `spawn_sky_light` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `build_lighting` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `ensure_single_sky_light` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `create_lightmass_volume` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `setup_volumetric_fog` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `create_lighting_enabled_level` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `setup_global_illumination` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `configure_shadows` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `set_exposure` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `set_ambient_occlusion` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `list_light_types` | `McpAutomationBridge_LightingHandlers.cpp` | `HandleLightingAction` | Discovery: Returns all `ALight` subclasses |

## 7. Performance Manager (`manage_performance`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `generate_memory_report` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `configure_texture_streaming` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `merge_actors` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `start_profiling` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `stop_profiling` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `show_fps` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `show_stats` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_scalability` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_resolution_scale` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_vsync` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_frame_rate_limit` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `configure_nanite` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `configure_lod` | `McpAutomationBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |

## 8. Animation & Physics (`animation_physics`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_animation_bp` | `McpAutomationBridge_AnimationHandlers.cpp` | `HandleCreateAnimBlueprint` | |
| `play_montage` | `McpAutomationBridge_AnimationHandlers.cpp` | `HandlePlayAnimMontage` | |
| `setup_ragdoll` | `McpAutomationBridge_AnimationHandlers.cpp` | `HandleSetupRagdoll` | |
| `configure_vehicle` | `McpAutomationBridge_AnimationHandlers.cpp` | `HandleAnimationPhysicsAction` | Supports custom vehicle type passthrough |

## 9. Effects Manager (`manage_effect`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `niagara` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleEffectAction` | |
| `spawn_niagara` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleSpawnNiagaraActor` | |
| `debug_shape` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleEffectAction` | |
| `create_niagara_system` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleCreateNiagaraSystem` | |
| `create_niagara_emitter` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleCreateNiagaraEmitter` | |
| `add_niagara_module` | `McpAutomationBridge_NiagaraGraphHandlers.cpp` | `HandleNiagaraGraphAction` | |
| `list_debug_shapes` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleEffectAction` | Discovery: Returns all debug shape types |
| `clear_debug_shapes` | `McpAutomationBridge_EffectHandlers.cpp` | `HandleEffectAction` | Clears persistent debug shapes |

## 10. Environment Builder (`build_environment`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_landscape` | `McpAutomationBridge_LandscapeHandlers.cpp` | `HandleCreateLandscape` | |
| `sculpt` | `McpAutomationBridge_LandscapeHandlers.cpp` | `HandleSculptLandscape` | |
| `paint_foliage` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandlePaintFoliage` | |
| `add_foliage_instances` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandleAddFoliageInstances` | |
| `get_foliage_instances` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandleGetFoliageInstances` | |
| `remove_foliage` | `McpAutomationBridge_FoliageHandlers.cpp` | `HandleRemoveFoliage` | |
| `create_procedural_terrain` | `McpAutomationBridge_EnvironmentHandlers.cpp` | `HandleCreateProceduralTerrain` | |

## 11. System Control (`system_control`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `execute_command` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleExecuteEditorFunction` | |
| `console_command` | `McpAutomationBridge_EditorFunctionHandlers.cpp` | `HandleConsoleCommandAction` | |
| `run_ubt` | *None* | *None* | Handled in TypeScript (`child_process`) |
| `run_tests` | `McpAutomationBridge_TestHandlers.cpp` | `HandleTestAction` | |
| `subscribe` | `McpAutomationBridge_LogHandlers.cpp` | `HandleLogAction` | |
| `unsubscribe` | `McpAutomationBridge_LogHandlers.cpp` | `HandleLogAction` | |
| `spawn_category` | `McpAutomationBridge_DebugHandlers.cpp` | `HandleDebugAction` | |
| `start_session` | `McpAutomationBridge_InsightsHandlers.cpp` | `HandleInsightsAction` | |
| `lumen_update_scene` | `McpAutomationBridge_RenderHandlers.cpp` | `HandleRenderAction` | |
| `set_project_setting` | `McpAutomationBridge_EnvironmentHandlers.cpp` | `HandleSystemControlAction` | |
| `create_hud` | `McpAutomationBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |
| `set_widget_text` | `McpAutomationBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |
| `set_widget_image` | `McpAutomationBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |
| `set_widget_visibility` | `McpAutomationBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |
| `remove_widget_from_viewport` | `McpAutomationBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |

## 12. Sequencer (`manage_sequence`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `add_actor` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `play` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `add_keyframe` | `McpAutomationBridge_SequencerHandlers.cpp` | `HandleAddSequencerKeyframe` | |
| `add_camera` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleAddCameraTrack` | |
| `add_track` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | Dynamic track class resolution |
| `list_track_types` | `McpAutomationBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | Discovery: Returns all `UMovieSceneTrack` subclasses |

## 13. Introspection (`inspect`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `inspect_object` | `McpAutomationBridge_PropertyHandlers.cpp` | `HandleInspectAction` | |
| `set_property` | `McpAutomationBridge_PropertyHandlers.cpp` | `HandleSetObjectProperty` | |
| `get_property` | `McpAutomationBridge_PropertyHandlers.cpp` | `HandleGetObjectProperty` | |
| `get_components` | `McpAutomationBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `list_objects` | `McpAutomationBridge_PropertyHandlers.cpp` | `HandleInspectAction` | |

## 14. Audio Manager (`manage_audio`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_sound_cue` | `McpAutomationBridge_AudioHandlers.cpp` | `HandleAudioAction` | |
| `play_sound_at_location` | `McpAutomationBridge_AudioHandlers.cpp` | `HandleAudioAction` | |
| `create_audio_component` | `McpAutomationBridge_AudioHandlers.cpp` | `HandleAudioAction` | |

## 15. Behavior Tree Manager (`manage_behavior_tree`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `add_node` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `connect_nodes` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `remove_node` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `break_connections` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `set_node_properties` | `McpAutomationBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |

## 16. Blueprint Graph Manager (`manage_blueprint_graph`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_node` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | Dynamic node class resolution |
| `delete_node` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `connect_pins` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `break_pin_links` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `set_node_property` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `list_node_types` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | Lists all UK2Node subclasses |
| `set_pin_default_value` | `McpAutomationBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | Sets default value on input pins |

## 18. Geometry Manager (`manage_geometry`) - Phase 6

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_box` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh box primitive |
| `create_sphere` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh sphere primitive |
| `create_cylinder` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh cylinder primitive |
| `create_cone` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh cone primitive |
| `create_capsule` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh capsule primitive |
| `create_torus` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh torus primitive |
| `create_plane` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh plane primitive |
| `create_disc` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh disc primitive |
| `create_stairs` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates linear stairs with configurable steps |
| `create_spiral_stairs` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates curved/spiral stairs with inner radius |
| `create_ring` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates ring (disc with hole) using inner/outer radius |
| `boolean_union` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Boolean union of two DynamicMesh actors |
| `boolean_subtract` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Boolean subtraction of two DynamicMesh actors |
| `boolean_intersection` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Boolean intersection of two DynamicMesh actors |
| `extrude` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Linear extrude faces along direction |
| `inset` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Inset faces (shrink inward) |
| `outset` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Outset faces (expand outward) |
| `bevel` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Bevel edges/polygroups with subdivisions |
| `offset_faces` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Offset faces along normals |
| `shell` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Solidify mesh (add thickness) |
| `bend` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Bend deformer with angle and extent |
| `twist` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Twist deformer with angle and extent |
| `taper` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Taper/flare deformer with XY percentages |
| `noise_deform` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Perlin noise displacement with magnitude/frequency |
| `smooth` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Iterative smoothing with iterations and alpha |
| `simplify_mesh` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Reduces triangle count via QEM simplification |
| `subdivide` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Subdivides mesh via PN tessellation |
| `remesh_uniform` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Uniform remesh to target triangle count |
| `weld_vertices` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Weld nearby vertices within tolerance |
| `fill_holes` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Automatically fill mesh holes |
| `remove_degenerates` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Remove degenerate triangles/edges |
| `auto_uv` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Auto-generates UVs using XAtlas |
| `recalculate_normals` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Recalculates mesh normals |
| `flip_normals` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Flips mesh normals |
| `generate_collision` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Generate collision (convex, box, sphere, capsule, decomposition) |
| `convert_to_static_mesh` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Converts DynamicMesh to StaticMesh asset |
| `get_mesh_info` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Returns vertex/triangle counts, UV/normal info |
| `create_arch` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates partial torus (arch) with angle parameter |
| `create_pipe` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates hollow cylinder (boolean subtract inner) |
| `create_ramp` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates extruded right triangle polygon |
| `mirror` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Mirror mesh across axis (X/Y/Z), optionally weld seam |
| `array_linear` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Linear array with count and offset vector |
| `array_radial` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Radial array around center with count and angle |
| `triangulate` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Convert quads/n-gons to triangles |
| `poke` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Poke face centers, subdivide with offset |
| `relax` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Relaxation smoothing (Laplacian with strength) |
| `project_uv` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | UV projection (box, planar, cylindrical) |
| `recompute_tangents` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Recompute tangent space using MikkT |
| `revolve` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Revolve 2D profile around axis to create solid |
| `stretch` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Stretch mesh along axis with factor |
| `spherify` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Transform vertices toward spherical shape |
| `cylindrify` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Transform vertices toward cylindrical shape |
| `chamfer` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Chamfer edges with distance and steps |
| `merge_vertices` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Merge duplicate vertices within tolerance |
| `transform_uvs` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Transform UVs (translate, scale, rotate) |
| `boolean_trim` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Boolean trim with keepInside option |
| `self_union` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Self-union for watertight mesh |
| `extrude_along_spline` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Extrude mesh profile along spline path |
| `bridge` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Bridge gaps between edge groups |
| `loft` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Loft surface between cross-sections |
| `sweep` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Sweep profile along path with twist/scale |
| `duplicate_along_spline` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Duplicate meshes along spline path |
| `loop_cut` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Insert edge loop cuts |
| `edge_split` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Split edges based on angle threshold |
| `quadrangulate` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Convert triangles to quads |
| `remesh_voxel` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Voxel-based remesh for watertight mesh |
| `unwrap_uv` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Automatic UV unwrap with XAtlas |
| `pack_uv_islands` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Pack UV islands for optimal space usage |
| `generate_complex_collision` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Generate complex collision (mesh-based) |
| `simplify_collision` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Simplify collision with convex decomposition |
| `generate_lods` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Generate LOD levels for static mesh |
| `set_lod_settings` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Configure individual LOD settings |
| `set_lod_screen_sizes` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Set screen size thresholds for all LODs |
| `convert_to_nanite` | `McpAutomationBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Enable Nanite on static mesh (UE5+) |

## 19. Skeleton Manager (`manage_skeleton`) - Phase 7

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `get_skeleton_info` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleGetSkeletonInfo` | Returns bone count, virtual bone count, socket count |
| `list_bones` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleListBones` | Lists all bones with index, parent, reference pose |
| `list_sockets` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleListSockets` | Lists all sockets with bone, location, rotation, scale |
| `create_socket` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleCreateSocket` | Creates socket on skeleton with attachment bone |
| `configure_socket` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleConfigureSocket` | Modifies existing socket properties |
| `create_virtual_bone` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleCreateVirtualBone` | Creates virtual bone between source and target bones |
| `create_physics_asset` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleCreatePhysicsAsset` | Creates physics asset linked to skeletal mesh |
| `list_physics_bodies` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleListPhysicsBodies` | Lists physics bodies and constraints |
| `add_physics_body` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleAddPhysicsBody` | Adds capsule/sphere/box bodies to physics asset |
| `configure_physics_body` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleConfigurePhysicsBody` | Configures mass, damping, collision |
| `add_physics_constraint` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleAddPhysicsConstraint` | Creates joint between two bodies |
| `configure_constraint_limits` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleConfigureConstraintLimits` | Sets angular/linear limits |
| `rename_bone` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleRenameBone` | Renames virtual bones (regular bones require reimport) |
| `set_bone_transform` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleSetBoneTransform` | Sets reference pose transform |
| `create_morph_target` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleCreateMorphTarget` | Creates new UMorphTarget on mesh |
| `set_morph_target_deltas` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleSetMorphTargetDeltas` | Sets vertex deltas for morph target |
| `import_morph_targets` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleImportMorphTargets` | Lists morph targets (FBX import via asset pipeline) |
| `normalize_weights` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleNormalizeWeights` | Rebuilds mesh with normalized weights |
| `prune_weights` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandlePruneWeights` | Rebuilds mesh with pruned weights |
| `bind_cloth_to_skeletal_mesh` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleBindClothToSkeletalMesh` | Prepares cloth binding |
| `assign_cloth_asset_to_mesh` | `McpAutomationBridge_SkeletonHandlers.cpp` | `HandleAssignClothAssetToMesh` | Lists/assigns cloth assets |
| `create_skeleton` | `McpAutomationBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires FBX import |
| `add_bone` | `McpAutomationBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires FReferenceSkeletonModifier |
| `remove_bone` | `McpAutomationBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires FReferenceSkeletonModifier |
| `set_bone_parent` | `McpAutomationBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires hierarchy rebuild |
| `auto_skin_weights` | `McpAutomationBridge_SkeletonHandlers.cpp` | - | **Stub** - Use Skeletal Mesh Editor |
| `set_vertex_weights` | `McpAutomationBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires vertex buffer access |
| `copy_weights` | `McpAutomationBridge_SkeletonHandlers.cpp` | - | **Stub** - Use Skeletal Mesh Editor |
| `mirror_weights` | `McpAutomationBridge_SkeletonHandlers.cpp` | - | **Stub** - Use Skeletal Mesh Editor |

## 20. Material Authoring Manager (`manage_material_authoring`) - Phase 8

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_material` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates new UMaterial asset |
| `set_blend_mode` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets blend mode (opaque, masked, translucent, etc.) |
| `set_shading_model` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets shading model (default_lit, unlit, subsurface, etc.) |
| `set_material_domain` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets domain (surface, deferred_decal, light_function, etc.) |
| `add_texture_sample` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionTextureSample node |
| `add_texture_coordinate` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionTextureCoordinate node |
| `add_scalar_parameter` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionScalarParameter node |
| `add_vector_parameter` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionVectorParameter node |
| `add_static_switch_parameter` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionStaticSwitchParameter node |
| `add_math_node` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds math nodes (add, multiply, divide, power, lerp, etc.) |
| `add_world_position` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionWorldPosition node |
| `add_vertex_normal` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionVertexNormalWS node |
| `add_pixel_depth` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionPixelDepth node |
| `add_fresnel` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionFresnel node |
| `add_reflection_vector` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionReflectionVectorWS node |
| `add_panner` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionPanner node |
| `add_rotator` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionRotator node |
| `add_noise` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionNoise node |
| `add_voronoi` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionVoronoiNoise node |
| `add_if` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionIf node |
| `add_switch` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionSwitch node |
| `add_custom_expression` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionCustom node (HLSL code) |
| `connect_nodes` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Connects material expression pins |
| `disconnect_nodes` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Disconnects material expression pins |
| `create_material_function` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates UMaterialFunction asset |
| `add_function_input` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionFunctionInput node |
| `add_function_output` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionFunctionOutput node |
| `use_material_function` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionMaterialFunctionCall node |
| `create_material_instance` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates UMaterialInstanceConstant asset |
| `set_scalar_parameter_value` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets scalar parameter on material instance |
| `set_vector_parameter_value` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets vector parameter on material instance |
| `set_texture_parameter_value` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets texture parameter on material instance |
| `create_landscape_material` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates landscape-ready material |
| `create_decal_material` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates deferred decal material |
| `create_post_process_material` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates post process material |
| `add_landscape_layer` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionLandscapeLayerBlend node |
| `configure_layer_blend` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Configures layer blend settings |
| `compile_material` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Compiles material and reports errors |
| `get_material_info` | `McpAutomationBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Returns material properties and node info |

## 21. Texture Manager (`manage_texture`) - Phase 9

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_noise_texture` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Creates procedural noise texture (Perlin, Simplex, Worley, Voronoi) |
| `create_gradient_texture` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Creates gradient texture (Linear, Radial, Angular) |
| `create_pattern_texture` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Creates pattern texture (Checker, Grid, Brick, Dots, Stripes) |
| `create_normal_from_height` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Converts height map to normal map using Sobel/Prewitt |
| `create_ao_from_mesh` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Bakes AO from mesh (placeholder - requires GPU) |
| `resize_texture` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `adjust_levels` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `adjust_curves` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `blur` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `sharpen` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `invert` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `desaturate` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `channel_pack` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `channel_extract` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `combine_textures` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `set_compression_settings` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Sets texture compression (TC_Default, TC_Normalmap, etc.) |
| `set_texture_group` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Sets LOD group (TEXTUREGROUP_World, etc.) |
| `set_lod_bias` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Sets LOD bias value |
| `configure_virtual_texture` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Enables/disables virtual texture streaming |
| `set_streaming_priority` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Sets streaming priority and NeverStream flag |
| `get_texture_info` | `McpAutomationBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Returns texture dimensions, format, compression, mip count |

## 22. Animation Authoring Manager (`manage_animation_authoring`) - Phase 10

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Animation Sequences** | | | |
| `create_animation_sequence` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UAnimSequence with specified skeleton, frames, framerate |
| `set_sequence_length` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets sequence duration via IAnimationDataController |
| `add_bone_track` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds bone curve to sequence |
| `set_bone_key` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets transform keyframe at frame |
| `set_curve_key` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets curve value keyframe |
| `add_notify` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds UAnimNotify at time/frame |
| `add_notify_state` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds UAnimNotifyState with duration |
| `add_sync_marker` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds FAnimSyncMarker |
| `set_root_motion_settings` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures root motion, root lock |
| `set_additive_settings` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures additive type, base pose |
| **Animation Montages** | | | |
| `create_montage` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UAnimMontage with skeleton |
| `add_montage_section` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds FCompositeSection at time |
| `add_montage_slot` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds animation to slot track |
| `set_section_timing` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Updates section start time |
| `add_montage_notify` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds notify to montage |
| `set_blend_in` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures blend in time/curve |
| `set_blend_out` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures blend out time/curve |
| `link_sections` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Links montage sections |
| **Blend Spaces** | | | |
| `create_blend_space_1d` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UBlendSpace1D |
| `create_blend_space_2d` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UBlendSpace |
| `add_blend_sample` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds animation sample at position |
| `set_axis_settings` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures axis name, min, max, grid |
| `set_interpolation_settings` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets target weight interpolation speed |
| `create_aim_offset` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UAimOffsetBlendSpace |
| `add_aim_offset_sample` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds sample at yaw/pitch |
| **Animation Blueprints** | | | |
| `create_anim_blueprint` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UAnimBlueprint |
| `add_state_machine` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds state machine to anim graph |
| `add_state` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds state to state machine |
| `add_transition` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds transition between states |
| `set_transition_rules` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures blend time, logic type |
| `add_blend_node` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds blend node to anim graph |
| `add_cached_pose` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds cached pose node |
| `add_slot_node` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds slot node for montages |
| `add_layered_blend_per_bone` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds layered blend per bone |
| `set_anim_graph_node_value` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets node property value |
| **Control Rig** | | | |
| `create_control_rig` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UControlRigBlueprint (if available) |
| `add_control` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds control to rig |
| `add_rig_unit` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds IK/FK solver unit |
| `connect_rig_elements` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Connects rig elements |
| `create_pose_library` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UPoseAsset |
| **Retargeting** | | | |
| `create_ik_rig` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UIKRigDefinition |
| `add_ik_chain` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds IK chain to rig |
| `create_ik_retargeter` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UIKRetargeter |
| `set_retarget_chain_mapping` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Maps source to target chain |
| **Utility** | | | |
| `get_animation_info` | `McpAutomationBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Returns animation asset properties |

## 23. Audio Authoring Manager (`manage_audio_authoring`) - Phase 11

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Sound Cues** | | | |
| `create_sound_cue` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates USoundCue with optional wave player |
| `add_cue_node` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds wave_player, mixer, random, modulator, looping, etc. |
| `connect_cue_nodes` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Connects sound cue nodes as parent-child |
| `set_cue_attenuation` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets attenuation settings on sound cue |
| `set_cue_concurrency` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets concurrency settings on sound cue |
| **MetaSounds** | | | |
| `create_metasound` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates MetaSound asset (UE 5.0+) |
| `add_metasound_node` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds node to MetaSound graph |
| `connect_metasound_nodes` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Connects MetaSound nodes |
| `add_metasound_input` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds input parameter to MetaSound |
| `add_metasound_output` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds output to MetaSound |
| `set_metasound_default` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets default value for MetaSound input |
| **Sound Classes & Mixes** | | | |
| `create_sound_class` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates USoundClass asset |
| `set_class_properties` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets volume, pitch, LPF, stereo bleed, etc. |
| `set_class_parent` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets parent sound class for hierarchy |
| `create_sound_mix` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates USoundMix asset |
| `add_mix_modifier` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds FSoundClassAdjuster to sound mix |
| `configure_mix_eq` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Configures EQ settings on sound mix |
| **Attenuation & Spatialization** | | | |
| `create_attenuation_settings` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates USoundAttenuation asset |
| `configure_distance_attenuation` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets inner radius, falloff, algorithm |
| `configure_spatialization` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets spatialization algorithm (Panner/HRTF) |
| `configure_occlusion` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets occlusion LPF, volume, interpolation |
| `configure_reverb_send` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets reverb wet levels and distances |
| **Dialogue System** | | | |
| `create_dialogue_voice` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates UDialogueVoice with gender/plurality |
| `create_dialogue_wave` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates UDialogueWave with spoken text |
| `set_dialogue_context` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets dialogue context mappings |
| **Effects** | | | |
| `create_reverb_effect` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates UReverbEffect with density, decay, etc. |
| `create_source_effect_chain` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates source effect chain (AudioMixer) |
| `add_source_effect` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds effect to source effect chain |
| `create_submix_effect` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates submix effect preset |
| **Utility** | | | |
| `get_audio_info` | `McpAutomationBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Returns audio asset properties (type-specific) |

## 24. Niagara Authoring Manager (`manage_niagara_authoring`) - Phase 12

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Systems & Emitters** | | | |
| `create_niagara_system` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Creates UNiagaraSystem asset |
| `create_niagara_emitter` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Creates UNiagaraEmitter asset |
| `add_emitter_to_system` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds emitter to system |
| `set_emitter_properties` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets enabled, local space, sim target |
| **Spawn Modules** | | | |
| `add_spawn_rate_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures spawn rate (particles/sec) |
| `add_spawn_burst_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures burst spawn (count, time) |
| `add_spawn_per_unit_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures spawn per unit distance |
| **Particle Modules** | | | |
| `add_initialize_particle_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets lifetime, mass, initial size |
| `add_particle_state_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures particle state behavior |
| `add_force_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds gravity, drag, vortex, curl noise, etc. |
| `add_velocity_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets initial velocity (linear, cone, point) |
| `add_acceleration_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets particle acceleration vector |
| **Appearance Modules** | | | |
| `add_size_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures uniform/non-uniform size |
| `add_color_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets color, color mode, color curves |
| **Renderer Modules** | | | |
| `add_sprite_renderer_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds UNiagaraSpriteRendererProperties |
| `add_mesh_renderer_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds UNiagaraMeshRendererProperties |
| `add_ribbon_renderer_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds UNiagaraRibbonRendererProperties |
| `add_light_renderer_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds UNiagaraLightRendererProperties |
| **Behavior Modules** | | | |
| `add_collision_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures collision mode, restitution, friction |
| `add_kill_particles_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures kill conditions (age, box, sphere) |
| `add_camera_offset_module` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets camera offset for particles |
| **Parameters** | | | |
| `add_user_parameter` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds exposed user parameter |
| `set_parameter_value` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets parameter value |
| `bind_parameter_to_source` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Binds parameter to source |
| **Data Interfaces** | | | |
| `add_skeletal_mesh_data_interface` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds skeletal mesh sampling DI |
| `add_static_mesh_data_interface` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds static mesh sampling DI |
| `add_spline_data_interface` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds spline data interface |
| `add_audio_spectrum_data_interface` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds audio spectrum reactive DI |
| `add_collision_query_data_interface` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds collision query DI |
| **Events** | | | |
| `add_event_generator` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds event generator to emitter |
| `add_event_receiver` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds event receiver with optional spawn |
| `configure_event_payload` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures event payload attributes |
| **GPU Simulation** | | | |
| `enable_gpu_simulation` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Enables GPU compute simulation |
| `add_simulation_stage` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds custom simulation stage |
| **Utility** | | | |
| `get_niagara_info` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Returns system/emitter info, parameters, renderers |
| `validate_niagara_system` | `McpAutomationBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Validates system and returns errors/warnings |

## 25. GAS Manager (`manage_gas`) - Phase 13

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Components & Attributes** | | | |
| `add_ability_system_component` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds ASC via SCS to blueprint |
| `configure_asc` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets replication mode on ASC |
| `create_attribute_set` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Creates UAttributeSet blueprint |
| `add_attribute` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds FGameplayAttributeData member |
| `set_attribute_base_value` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures attribute base value |
| `set_attribute_clamping` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures min/max clamping |
| **Gameplay Abilities** | | | |
| `create_gameplay_ability` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Creates UGameplayAbility blueprint |
| `set_ability_tags` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets ability/cancel/block tags |
| `set_ability_costs` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets cost GameplayEffect class |
| `set_ability_cooldown` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets cooldown GameplayEffect class |
| `set_ability_targeting` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures targeting type |
| `add_ability_task` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Reference for AbilityTask usage |
| `set_activation_policy` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets net execution policy |
| `set_instancing_policy` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets instancing policy |
| **Gameplay Effects** | | | |
| `create_gameplay_effect` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Creates UGameplayEffect blueprint |
| `set_effect_duration` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets duration policy and time |
| `add_effect_modifier` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds modifier with operation/magnitude |
| `set_modifier_magnitude` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets magnitude on existing modifier |
| `add_effect_execution_calculation` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds execution calculation reference |
| `add_effect_cue` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds gameplay cue tag to effect |
| `set_effect_stacking` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets stacking type and limit |
| `set_effect_tags` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets granted/application/removal tags |
| **Gameplay Cues** | | | |
| `create_gameplay_cue_notify` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Creates static or actor cue notify |
| `configure_cue_trigger` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures trigger type |
| `set_cue_effects` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures particle/sound/shake refs |
| `add_tag_to_asset` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds gameplay tag to asset |
| **Utility** | | | |
| `get_gas_info` | `McpAutomationBridge_GASHandlers.cpp` | `HandleManageGASAction` | Returns GAS asset info and properties |

## 26. Character Manager (`manage_character`) - Phase 14

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Character Creation** | | | |
| `create_character_blueprint` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Creates ACharacter blueprint with capsule, mesh, movement |
| `configure_capsule_component` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Sets radius, half-height, collision |
| `configure_mesh_component` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures skeletal mesh, animation BP |
| `configure_camera_component` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Sets up camera boom and camera component |
| **Movement Component** | | | |
| `configure_movement_speeds` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Walk, run, sprint, crouch, swim, fly speeds |
| `configure_jump` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Jump height, air control, double jump |
| `configure_rotation` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Orient to movement, use controller rotation |
| `add_custom_movement_mode` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Adds custom movement mode enum |
| `configure_nav_movement` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | NavMesh agent settings |
| **Advanced Movement** | | | |
| `setup_mantling` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures mantling system (trace, animation) |
| `setup_vaulting` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures vaulting system |
| `setup_climbing` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures climbing system |
| `setup_sliding` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures sliding system |
| `setup_wall_running` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures wall running system |
| `setup_grappling` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures grappling hook system |
| **Footsteps System** | | | |
| `setup_footstep_system` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Creates footstep audio system |
| `map_surface_to_sound` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Maps physical surface to sound |
| `configure_footstep_fx` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures footstep VFX (dust, splashes) |
| **Utility** | | | |
| `get_character_info` | `McpAutomationBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Returns character blueprint info |

## 27. Combat Manager (`manage_combat`) - Phase 15

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Weapon Base** | | | |
| `create_weapon_blueprint` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates AActor weapon blueprint with components |
| `configure_weapon_mesh` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets skeletal/static mesh on weapon |
| `configure_weapon_sockets` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Configures muzzle, grip, attachment sockets |
| `set_weapon_stats` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets damage, fire rate, range, spread |
| **Firing Modes** | | | |
| `configure_hitscan` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Configures hitscan trace settings |
| `configure_projectile` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets projectile class, spawn settings |
| `configure_spread_pattern` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Configures pellet spread, pattern type |
| `configure_recoil_pattern` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets recoil curve, recovery settings |
| `configure_aim_down_sights` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | ADS zoom, FOV, camera offset |
| **Projectiles** | | | |
| `create_projectile_blueprint` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates AActor projectile blueprint |
| `configure_projectile_movement` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets speed, gravity, rotation following |
| `configure_projectile_collision` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Collision channels, ignore actors |
| `configure_projectile_homing` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Homing acceleration, target type |
| **Damage System** | | | |
| `create_damage_type` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates UDamageType blueprint |
| `configure_damage_execution` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets damage multipliers, falloff |
| `setup_hitbox_component` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Adds hitbox with damage multiplier |
| **Weapon Features** | | | |
| `setup_reload_system` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates reload variables, montage slot |
| `setup_ammo_system` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Magazine size, reserve ammo, ammo types |
| `setup_attachment_system` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Attachment slots, stat modifiers |
| `setup_weapon_switching` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Weapon slots, switch timing |
| **Effects** | | | |
| `configure_muzzle_flash` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Particle system, light flash settings |
| `configure_tracer` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Tracer particle, frequency settings |
| `configure_impact_effects` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Surface-based impact particles, decals |
| `configure_shell_ejection` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Shell mesh, ejection socket, physics |
| **Melee Combat** | | | |
| `create_melee_trace` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates trace function, socket setup |
| `configure_combo_system` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Combo chain, timing windows |
| `create_hit_pause` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Hitstop duration, time dilation |
| `configure_hit_reaction` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Hit reaction montages, stagger |
| `setup_parry_block_system` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Parry window, block angle, stamina |
| `configure_weapon_trails` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Trail particle, socket binding |
| **Utility** | | | |
| `get_combat_info` | `McpAutomationBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Returns weapon/combat component info |

## 28. AI Manager (`manage_ai`) - Phase 16

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **AI Controller** | | | |
| `create_ai_controller` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates AAIController blueprint |
| `assign_behavior_tree` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets behavior tree on controller |
| `assign_blackboard` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets blackboard asset on controller |
| **Blackboard** | | | |
| `create_blackboard_asset` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates UBlackboardData asset |
| `add_blackboard_key` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds key (bool, int, float, vector, rotator, object, class, enum, name, string) |
| `set_key_instance_synced` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets instance sync flag on key |
| **Behavior Tree** | | | |
| `create_behavior_tree` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates UBehaviorTree asset |
| `add_composite_node` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds selector, sequence, parallel, simple_parallel |
| `add_task_node` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds move_to, rotate_to_face, wait, play_animation, play_sound, run_eqs_query, etc. |
| `add_decorator` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds blackboard, cooldown, cone_check, loop, time_limit, force_success, etc. |
| `add_service` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds default_focus, run_eqs services |
| `configure_bt_node` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets node properties |
| **EQS** | | | |
| `create_eqs_query` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates UEnvQuery asset |
| `add_eqs_generator` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds actors_of_class, current_location, donut, grid, on_circle, pathing_grid, points |
| `add_eqs_context` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds querier, item, target contexts |
| `add_eqs_test` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds distance, dot, overlap, pathfinding, project, random, trace, gameplay_tags tests |
| `configure_test_scoring` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures test scoring settings |
| **Perception** | | | |
| `add_ai_perception_component` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds UAIPerceptionComponent to blueprint |
| `configure_sight_config` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets sight radius, angle, age, detection by affiliation |
| `configure_hearing_config` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets hearing radius |
| `configure_damage_sense_config` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures damage sensing |
| `set_perception_team` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets AI perception team ID |
| **State Trees (UE5.3+)** | | | |
| `create_state_tree` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates UStateTree asset |
| `add_state_tree_state` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds state to state tree |
| `add_state_tree_transition` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds transition between states |
| `configure_state_tree_task` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures state task and conditions |
| **Smart Objects** | | | |
| `create_smart_object_definition` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates USmartObjectDefinition asset |
| `add_smart_object_slot` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds slot to smart object definition |
| `configure_slot_behavior` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures slot behavior settings |
| `add_smart_object_component` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds USmartObjectComponent to blueprint |
| **Mass AI (Crowds)** | | | |
| `create_mass_entity_config` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates mass entity config data asset |
| `configure_mass_entity` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures mass entity traits and fragments |
| `add_mass_spawner` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds AMassSpawner to level |
| **Utility** | | | |
| `get_ai_info` | `McpAutomationBridge_AIHandlers.cpp` | `HandleManageAIAction` | Returns AI asset info and configuration |

## 29. Inventory Manager (`manage_inventory`) - Phase 17

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Data Assets** | | | |
| `create_item_data_asset` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates UPrimaryDataAsset for item data |
| `set_item_properties` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets name, description, icon, mesh, weight, rarity |
| `create_item_category` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates category data asset |
| `assign_item_category` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Assigns item to category |
| **Inventory Component** | | | |
| `create_inventory_component` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds UActorComponent for inventory |
| `configure_inventory_slots` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets slot count, stack sizes |
| `add_inventory_functions` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds Add/Remove/Has item functions |
| `configure_inventory_events` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures OnItemAdded/Removed events |
| `set_inventory_replication` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets replication mode for multiplayer |
| **Pickups** | | | |
| `create_pickup_actor` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates pickup actor blueprint |
| `configure_pickup_interaction` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets interaction radius, prompt |
| `configure_pickup_respawn` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures respawn timing |
| `configure_pickup_effects` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets pickup VFX, SFX |
| **Equipment** | | | |
| `create_equipment_component` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds equipment management component |
| `define_equipment_slots` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Defines slot types (head, body, weapon, etc.) |
| `configure_equipment_effects` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets stat modifiers on equip |
| `add_equipment_functions` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds Equip/Unequip functions |
| `configure_equipment_visuals` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets mesh attachment on equip |
| **Loot System** | | | |
| `create_loot_table` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates loot table data asset |
| `add_loot_entry` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds item to loot table with weight |
| `configure_loot_drop` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets drop chance, quantity range |
| `set_loot_quality_tiers` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures rarity tiers and colors |
| **Crafting** | | | |
| `create_crafting_recipe` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates recipe data asset |
| `configure_recipe_requirements` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets input items and quantities |
| `create_crafting_station` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates crafting station actor |
| `add_crafting_component` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds crafting functionality component |
| **Utility** | | | |
| `get_inventory_info` | `McpAutomationBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Returns inventory/equipment info |

## 30. Interaction Manager (`manage_interaction`) - Phase 18

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Interaction Component** | | | |
| `create_interaction_component` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Adds interaction component to blueprint with trace settings |
| `configure_interaction_trace` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Configures trace type, channel, distance, frequency |
| `configure_interaction_widget` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets widget class, offset, prompt text format |
| `add_interaction_events` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates OnInteractionStart/End/Found/Lost event dispatchers |
| **Interactables** | | | |
| `create_interactable_interface` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates IInteractable UInterface with Interact/CanInteract functions |
| `create_door_actor` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates door blueprint with pivot, rotation animation, sounds |
| `configure_door_properties` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets open angle, time, direction, locked state, key item |
| `create_switch_actor` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates switch/button/lever with on/off states |
| `configure_switch_properties` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets switch type, toggleable, one-shot, target actors |
| `create_chest_actor` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates chest/container with lid animation, loot integration |
| `configure_chest_properties` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets loot table, locked state, respawn settings |
| `create_lever_actor` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates lever with rotation/translation animation |
| **Destructibles** | | | |
| `setup_destructible_mesh` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets up GeometryCollection with fracture mode, piece count |
| `configure_destruction_levels` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Configures damage thresholds, mesh indices, physics |
| `configure_destruction_effects` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets destroy sound, particle, debris settings |
| `configure_destruction_damage` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets max health, damage thresholds, multipliers |
| `add_destruction_component` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Adds destruction management component |
| **Trigger System** | | | |
| `create_trigger_actor` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates trigger volume actor (box, sphere, capsule) |
| `configure_trigger_events` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets onEnter, onExit, onStay events |
| `configure_trigger_filter` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets class, tag, interface filters |
| `configure_trigger_response` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets response type, cooldown, max activations |
| **Utility** | | | |
| `get_interaction_info` | `McpAutomationBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Returns interaction component/actor properties |

## 31. Widget Authoring Manager (`manage_widget_authoring`) - Phase 19

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Widget Creation** | | | |
| `create_widget_blueprint` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates UUserWidget blueprint asset |
| `set_widget_parent_class` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets parent class for widget |
| **Layout Panels** | | | |
| `add_canvas_panel` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UCanvasPanel container |
| `add_horizontal_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UHorizontalBox layout |
| `add_vertical_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UVerticalBox layout |
| `add_overlay` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UOverlay container |
| `add_grid_panel` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UGridPanel container |
| `add_uniform_grid` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UUniformGridPanel |
| `add_wrap_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UWrapBox container |
| `add_scroll_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UScrollBox container |
| `add_size_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds USizeBox constraint |
| `add_scale_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UScaleBox container |
| `add_border` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UBorder container |
| **Common Widgets** | | | |
| `add_text_block` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UTextBlock widget |
| `add_rich_text_block` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds URichTextBlock widget |
| `add_image` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UImage widget |
| `add_button` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UButton widget |
| `add_check_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UCheckBox widget |
| `add_slider` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds USlider widget |
| `add_progress_bar` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UProgressBar widget |
| `add_text_input` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds editable text widget |
| `add_combo_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UComboBoxString widget |
| `add_spin_box` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds USpinBox widget |
| `add_list_view` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UListView widget |
| `add_tree_view` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UTreeView widget |
| **Layout & Styling** | | | |
| `set_anchor` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget anchors |
| `set_alignment` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget alignment |
| `set_position` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget position |
| `set_size` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget size |
| `set_padding` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget padding |
| `set_z_order` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget z-order |
| `set_render_transform` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets render transform |
| `set_visibility` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget visibility |
| `set_style` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget styling |
| `set_clipping` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget clipping |
| **Bindings & Events** | | | |
| `create_property_binding` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates property binding |
| `bind_text` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds text property |
| `bind_visibility` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds visibility |
| `bind_color` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds color/opacity |
| `bind_enabled` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds enabled state |
| `bind_on_clicked` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds click event |
| `bind_on_hovered` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds hover events |
| `bind_on_value_changed` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds value change event |
| **Widget Animations** | | | |
| `create_widget_animation` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates UWidgetAnimation |
| `add_animation_track` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds animation track |
| `add_animation_keyframe` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds keyframe to track |
| `set_animation_loop` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Configures animation loop |
| **UI Templates** | | | |
| `create_main_menu` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates main menu template |
| `create_pause_menu` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates pause menu template |
| `create_settings_menu` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates settings menu |
| `create_loading_screen` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates loading screen |
| `create_hud_widget` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates HUD template |
| `add_health_bar` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds health bar element |
| `add_ammo_counter` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds ammo counter |
| `add_minimap` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds minimap element |
| `add_crosshair` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds crosshair element |
| `add_compass` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds compass element |
| `add_interaction_prompt` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds interaction prompt |
| `add_objective_tracker` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds objective tracker |
| `add_damage_indicator` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds damage indicator |
| `create_inventory_ui` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates inventory UI |
| `create_dialog_widget` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates dialog widget |
| `create_radial_menu` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates radial menu |
| **Utility** | | | |
| `get_widget_info` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Returns widget blueprint info |
| `preview_widget` | `McpAutomationBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Opens widget in preview |

## 32. Networking Manager (`manage_networking`) - Phase 20

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Replication** | | | |
| `set_property_replicated` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets property replication flag |
| `set_replication_condition` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets COND_* replication condition |
| `configure_net_update_frequency` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures update frequency (Hz) |
| `configure_net_priority` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets network bandwidth priority |
| `set_net_dormancy` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets DORM_* dormancy mode |
| `configure_replication_graph` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures replication graph settings |
| **RPCs** | | | |
| `create_rpc_function` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Creates Server/Client/NetMulticast RPC |
| `configure_rpc_validation` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Enables RPC validation |
| `set_rpc_reliability` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets reliable/unreliable RPC |
| **Authority & Ownership** | | | |
| `set_owner` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets actor owner at runtime |
| `set_autonomous_proxy` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures autonomous proxy role |
| `check_has_authority` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Checks if local has authority |
| `check_is_locally_controlled` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Checks local control for Pawns |
| **Network Relevancy** | | | |
| `configure_net_cull_distance` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets network culling distance |
| `set_always_relevant` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets bAlwaysRelevant flag |
| `set_only_relevant_to_owner` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets bOnlyRelevantToOwner flag |
| **Net Serialization** | | | |
| `configure_net_serialization` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures custom NetSerialize |
| `set_replicated_using` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets ReplicatedUsing RepNotify |
| `configure_push_model` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Enables push-model replication |
| **Network Prediction** | | | |
| `configure_client_prediction` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Enables client-side prediction |
| `configure_server_correction` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures server reconciliation |
| `add_network_prediction_data` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Adds prediction data structure |
| `configure_movement_prediction` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures CMC network smoothing |
| **Connection & Session** | | | |
| `configure_net_driver` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures net driver settings |
| `set_net_role` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets initial net role |
| `configure_replicated_movement` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures movement replication |
| **Utility** | | | |
| `get_networking_info` | `McpAutomationBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Returns networking configuration |

## 33. Game Framework Manager (`manage_game_framework`) - Phase 21

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Core Class Creation** | | | |
| `create_game_mode` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates GameMode blueprint (AGameModeBase or AGameMode) |
| `create_game_state` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates GameState blueprint |
| `create_player_controller` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates PlayerController blueprint |
| `create_player_state` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates PlayerState blueprint |
| `create_game_instance` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates GameInstance blueprint |
| `create_hud_class` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates HUD blueprint |
| **Game Mode Configuration** | | | |
| `set_default_pawn_class` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets DefaultPawnClass on GameMode |
| `set_player_controller_class` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets PlayerControllerClass on GameMode |
| `set_game_state_class` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets GameStateClass on GameMode |
| `set_player_state_class` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets PlayerStateClass on GameMode |
| `configure_game_rules` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures game rules (min players, ready up, time limits) |
| **Match Flow** | | | |
| `setup_match_states` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Defines match state machine (waiting, warmup, in_progress, etc.) |
| `configure_round_system` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures round-based gameplay |
| `configure_team_system` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets up teams with colors and friendly fire |
| `configure_scoring_system` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Defines scoring rules and limits |
| `configure_spawn_system` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures spawn selection strategy |
| **Player Management** | | | |
| `configure_player_start` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures PlayerStart actor properties |
| `set_respawn_rules` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets respawn delay and location rules |
| `configure_spectating` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures spectator mode options |
| **Utility** | | | |
| `get_game_framework_info` | `McpAutomationBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Queries GameMode class configuration |

## 34. Sessions Manager (`manage_sessions`) - Phase 22

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Session Management** | | | |
| `configure_local_session_settings` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures max players, session name, private/public |
| `configure_session_interface` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures Online Subsystem session interface |
| **Local Multiplayer** | | | |
| `configure_split_screen` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Enables/disables split-screen mode |
| `set_split_screen_type` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Sets split type (horizontal, vertical, quadrant) |
| `add_local_player` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Adds local player to session |
| `remove_local_player` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Removes local player from session |
| **LAN** | | | |
| `configure_lan_play` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures LAN broadcast/discovery settings |
| `host_lan_server` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Hosts LAN server on specified port |
| `join_lan_server` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Joins LAN server by IP/port |
| **Voice Chat** | | | |
| `enable_voice_chat` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Enables/disables voice chat |
| `configure_voice_settings` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures voice input/output settings |
| `set_voice_channel` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Sets player voice channel |
| `mute_player` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Mutes/unmutes specific player |
| `set_voice_attenuation` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures 3D voice attenuation |
| `configure_push_to_talk` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures push-to-talk settings |
| **Utility** | | | |
| `get_sessions_info` | `McpAutomationBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Returns current session configuration info |

## 35. Level Structure Manager (`manage_level_structure`) - Phase 23

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Levels** | | | |
| `create_level` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates new level asset |
| `create_sublevel` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates sublevel in current world |
| `configure_level_streaming` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Configures streaming method (Blueprint, AlwaysLoaded, etc.) |
| `set_streaming_distance` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Sets streaming distance thresholds |
| `configure_level_bounds` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Sets level bounds for streaming/culling |
| **World Partition** | | | |
| `enable_world_partition` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Enables World Partition on level |
| `configure_grid_size` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Sets World Partition grid cell size |
| `create_data_layer` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates UDataLayerAsset |
| `assign_actor_to_data_layer` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Assigns actor to data layer |
| `configure_hlod_layer` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Configures HLOD layer settings |
| `create_minimap_volume` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates minimap bounds volume |
| **Level Blueprint** | | | |
| `open_level_blueprint` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Opens Level Blueprint in editor |
| `add_level_blueprint_node` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Adds node to Level Blueprint |
| `connect_level_blueprint_nodes` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Connects pins between nodes |
| **Level Instances** | | | |
| `create_level_instance` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates ALevelInstance actor |
| `create_packed_level_actor` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates APackedLevelActor |
| **Utility** | | | |
| `get_level_structure_info` | `McpAutomationBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Returns level structure information |

## 36. Volumes Manager (`manage_volumes`) - Phase 24

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Trigger Volumes** | | | |
| `create_trigger_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ATriggerVolume |
| `create_trigger_box` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ATriggerBox |
| `create_trigger_sphere` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ATriggerSphere |
| `create_trigger_capsule` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ATriggerCapsule |
| **Gameplay Volumes** | | | |
| `create_blocking_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ABlockingVolume |
| `create_kill_z_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates AKillZVolume |
| `create_pain_causing_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates APainCausingVolume with damage settings |
| `create_physics_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates APhysicsVolume with gravity/friction |
| **Audio Volumes** | | | |
| `create_audio_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates AAudioVolume |
| `create_reverb_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates AAudioVolume with reverb settings |
| **Rendering Volumes** | | | |
| `create_cull_distance_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ACullDistanceVolume |
| `create_precomputed_visibility_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates APrecomputedVisibilityVolume |
| `create_lightmass_importance_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ALightmassImportanceVolume |
| **Navigation Volumes** | | | |
| `create_nav_mesh_bounds_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ANavMeshBoundsVolume |
| `create_nav_modifier_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ANavModifierVolume |
| `create_camera_blocking_volume` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ACameraBlockingVolume |
| **Configuration** | | | |
| `set_volume_extent` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Sets volume brush extent (X, Y, Z) |
| `set_volume_properties` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Sets volume-specific properties |
| **Utility** | | | |
| `get_volumes_info` | `McpAutomationBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Returns volume information |

## 37. Navigation Manager (`manage_navigation`) - Phase 25

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **NavMesh Configuration** | | | |
| `configure_nav_mesh_settings` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Sets TileSizeUU, MinRegionArea, NavMeshResolutionParams (UE 5.7+) |
| `set_nav_agent_properties` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Sets AgentRadius, AgentHeight, AgentMaxSlope |
| `rebuild_navigation` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Triggers NavSys->Build() |
| **Nav Modifiers** | | | |
| `create_nav_modifier_component` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Creates UNavModifierComponent via SCS |
| `set_nav_area_class` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Sets area class on modifier component |
| `configure_nav_area_cost` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Configures DefaultCost on area CDO |
| **Nav Links** | | | |
| `create_nav_link_proxy` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Spawns ANavLinkProxy with FNavigationLink |
| `configure_nav_link` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Updates link start/end, direction, snap radius |
| `set_nav_link_type` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Toggles bSmartLinkIsRelevant (simple/smart) |
| `create_smart_link` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Spawns NavLinkProxy with smart link enabled |
| `configure_smart_link_behavior` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Configures UNavLinkCustomComponent settings |
| **Utility** | | | |
| `get_navigation_info` | `McpAutomationBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Returns NavMesh stats, agent properties, link counts |

## 38. Splines Manager (`manage_splines`) - Phase 26

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Spline Creation** | | | |
| `create_spline_actor` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates ASplineActor with USplineComponent |
| `add_spline_point` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Adds point at index with position/tangent |
| `remove_spline_point` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Removes point at specified index |
| `set_spline_point_position` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets point location in world/local space |
| `set_spline_point_tangents` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets arrive/leave tangents |
| `set_spline_point_rotation` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets point rotation |
| `set_spline_point_scale` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets point scale |
| `set_spline_type` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets type (linear, curve, constant, clamped_curve) |
| **Spline Mesh** | | | |
| `create_spline_mesh_component` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates USplineMeshComponent on actor |
| `set_spline_mesh_asset` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets static mesh asset on spline mesh |
| `configure_spline_mesh_axis` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets forward axis (X, Y, Z) |
| `set_spline_mesh_material` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets material on spline mesh |
| **Mesh Scattering** | | | |
| `scatter_meshes_along_spline` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Spawns mesh instances along spline |
| `configure_mesh_spacing` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets spacing mode (distance, count) |
| `configure_mesh_randomization` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets random offset, rotation, scale |
| **Quick Templates** | | | |
| `create_road_spline` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates road with configurable width, lanes |
| `create_river_spline` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates river with water material |
| `create_fence_spline` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates fence with posts and rails |
| `create_wall_spline` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates wall with height and thickness |
| `create_cable_spline` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates hanging cable with sag |
| `create_pipe_spline` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates pipe with radius and segments |
| **Utility** | | | |
| `get_splines_info` | `McpAutomationBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Returns spline info (points, length, closed) |

## 39. PCG Manager (`manage_pcg`) - Phase 27

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Graph Management** | | | |
| `create_pcg_graph` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleCreatePCGGraph` | Creates UPCGGraph asset with package |
| `create_pcg_subgraph` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleCreatePCGSubgraph` | Creates embedded subgraph in parent |
| `add_pcg_node` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddPCGNode` | Adds node by settings class name |
| `connect_pcg_pins` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleConnectPCGPins` | Connects pins via Graph->AddEdge |
| `set_pcg_node_settings` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleSetPCGNodeSettings` | Sets properties via UE reflection |
| **Input Nodes** | | | |
| `add_landscape_data_node` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddLandscapeDataNode` | UPCGDataFromActorSettings |
| `add_spline_data_node` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddSplineDataNode` | UPCGDataFromActorSettings |
| `add_volume_data_node` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddVolumeDataNode` | UPCGDataFromActorSettings |
| `add_actor_data_node` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddActorDataNode` | Mode: GetSinglePoint/GetActorReference/ParseActorComponents |
| `add_texture_data_node` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddTextureDataNode` | UPCGPointFromMeshSettings |
| **Samplers** | | | |
| `add_surface_sampler` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddSurfaceSampler` | PointsPerSquaredMeter, PointExtents, Looseness |
| `add_mesh_sampler` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddMeshSampler` | UPCGPointFromMeshSettings with StaticMesh |
| `add_spline_sampler` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddSplineSampler` | Dimension, Mode, SubdivisionsPerSegment |
| `add_volume_sampler` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddVolumeSampler` | VoxelSize, bUnbounded |
| **Filters & Modifiers** | | | |
| `add_bounds_modifier` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddBoundsModifier` | UPCGPointExtentsModifierSettings |
| `add_density_filter` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddDensityFilter` | LowerBound, UpperBound, bInvertFilter |
| `add_height_filter` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddHeightFilter` | UPCGFilterByAttributeSettings (Position.Z) |
| `add_slope_filter` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddSlopeFilter` | UPCGFilterByAttributeSettings (Normal.Z) |
| `add_distance_filter` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddDistanceFilter` | UPCGFilterByIndexSettings |
| `add_bounds_filter` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddBoundsFilter` | UPCGFilterByAttributeSettings |
| `add_self_pruning` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddSelfPruning` | PruningType, RadiusSimilarityFactor |
| **Transform Operations** | | | |
| `add_transform_points` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddTransformPoints` | Offset/Rotation/Scale Min/Max |
| `add_project_to_surface` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddProjectToSurface` | UPCGProjectionSettings |
| `add_copy_points` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddCopyPoints` | UPCGDuplicatePointSettings |
| `add_merge_points` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddMergePoints` | bMergeMetadata |
| **Spawners** | | | |
| `add_static_mesh_spawner` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddStaticMeshSpawner` | bApplyMeshBoundsToPoints, bSynchronousLoad |
| `add_actor_spawner` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddActorSpawner` | EPCGSpawnActorOption options |
| `add_spline_spawner` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleAddSplineSpawner` | UPCGSpawnActorSettings |
| **Execution** | | | |
| `execute_pcg_graph` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleExecutePCGGraph` | Finds actor, calls PCGComponent->Generate() |
| `set_pcg_partition_grid_size` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleSetPCGPartitionGridSize` | HiGen partition configuration |
| **Utility** | | | |
| `get_pcg_info` | `McpAutomationBridge_PCGHandlers.cpp` | `HandleGetPCGInfo` | Lists graphs or returns node/connection info |

## 40. Water Manager (`manage_water`) - Phase 28

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Water Body Creation** | | | Requires Water plugin (Experimental) |
| `create_water_body_ocean` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Creates AWaterBodyOcean with configurable height offset |
| `create_water_body_lake` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Creates AWaterBodyLake with spline-defined shape |
| `create_water_body_river` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Creates AWaterBodyRiver with flow direction |
| **Water Configuration** | | | |
| `configure_water_body` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Set water, underwater, water info, and static mesh materials |
| `configure_water_waves` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Query Gerstner wave settings (configure via asset editor) |
| **Water Info** | | | |
| `get_water_body_info` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Returns type, wave support, physical material, channel depth |
| `list_water_bodies` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Lists all water bodies with type and location |
| **River Configuration** | | | |
| `set_river_depth` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Sets river depth/width at spline key via SetRiverDepthAtSplineInputKey |
| **Ocean Configuration** | | | |
| `set_ocean_extent` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Sets ocean extent, collision bounds, and height offset |
| **Water Mesh** | | | |
| `set_water_static_mesh` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Enable static mesh mode and set mesh override |
| **Transitions** | | | |
| `set_river_transitions` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Set lake/ocean transition materials for river |
| **Water Zone** | | | |
| `set_water_zone` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Override water zone for water body |
| **Surface Queries** | | | |
| `get_water_surface_info` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Query surface location, normal, velocity at point |
| `get_wave_info` | `McpAutomationBridge_WaterHandlers.cpp` | `HandleWaterAction` | Query wave height, max height, attenuation, normal at point |

## 41. Weather Manager (`manage_weather`) - Phase 28

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Wind** | | | |
| `configure_wind` | `McpAutomationBridge_WeatherHandlers.cpp` | `HandleWeatherAction` | Spawns/configures WindDirectionalSource with strength, speed, gusts |
| **Weather System** | | | |
| `create_weather_system` | `McpAutomationBridge_WeatherHandlers.cpp` | `HandleWeatherAction` | Spawns master weather controller actor |
| **Precipitation** | | | |
| `configure_rain_particles` | `McpAutomationBridge_WeatherHandlers.cpp` | `HandleWeatherAction` | Spawns rain Niagara system with intensity/coverage |
| `configure_snow_particles` | `McpAutomationBridge_WeatherHandlers.cpp` | `HandleWeatherAction` | Spawns snow Niagara system with intensity/coverage |
| **Lightning** | | | |
| `configure_lightning` | `McpAutomationBridge_WeatherHandlers.cpp` | `HandleWeatherAction` | Spawns lightning actor with flash intensity/duration |

## 42. Post-Process Manager (`manage_post_process`) - Phase 29

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Post-Process Volume Core** | | | |
| `create_post_process_volume` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Creates APostProcessVolume with bounds/unbound option |
| `configure_pp_blend` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Sets blend weight and radius |
| `configure_pp_priority` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Sets PPV priority |
| `get_post_process_settings` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Returns all PPV settings as JSON |
| **Visual Effects** | | | |
| `configure_bloom` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Intensity, threshold, size scale, convolution settings |
| `configure_dof` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Focal distance, fstop, blade count, bokeh |
| `configure_motion_blur` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Amount, max velocity, per-object size |
| **Color & Lens Effects** | | | |
| `configure_color_grading` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Saturation, contrast, gamma, gain, offset per zone |
| `configure_white_balance` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Temperature, tint |
| `configure_vignette` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Intensity |
| `configure_chromatic_aberration` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Intensity, start offset |
| `configure_film_grain` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Intensity, response, texture, jitter |
| `configure_lens_flares` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Intensity, tint, bokeh size, threshold, texture |
| **Reflection Captures** | | | |
| `create_sphere_reflection_capture` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Spawns sphere reflection capture with influence radius |
| `create_box_reflection_capture` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Spawns box reflection capture with transition distance |
| `create_planar_reflection` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Spawns planar reflection with screen percentage |
| `recapture_scene` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Recaptures all reflection captures in scene |
| **Ray Tracing** | | | |
| `configure_ray_traced_shadows` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Enable via console variable |
| `configure_ray_traced_gi` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Enable via console variable |
| `configure_ray_traced_reflections` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Enable via console variable |
| `configure_ray_traced_ao` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Enable via console variable |
| `configure_path_tracing` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Enable via console variable |
| **Scene Captures** | | | |
| `create_scene_capture_2d` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Spawns ASceneCapture2D with render target |
| `create_scene_capture_cube` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Spawns ASceneCaptureCube with cube render target |
| `capture_scene` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Triggers capture on scene capture actor |
| **Light Channels** | | | |
| `set_light_channel` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Sets light channels on light component |
| `set_actor_light_channel` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Sets light channels on actor's primitive components |
| **Lightmass** | | | |
| `configure_lightmass_settings` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Indirect lighting quality, bounces |
| `build_lighting_quality` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Triggers lighting build with quality preset |
| `configure_indirect_lighting_cache` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | ILC quality settings |
| `configure_volumetric_lightmap` | `McpAutomationBridge_PostProcessHandlers.cpp` | `HandlePostProcessAction` | Volumetric lightmap detail cell size |

## 43. Sequencer Manager (`manage_sequencer`) - Phase 30

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Sequence Creation & Management** | | | |
| `create_master_sequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Creates ULevelSequence with display rate |
| `add_subsequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Adds UMovieSceneSubSection to shot track |
| `remove_subsequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Removes subsequence section |
| `get_subsequences` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Lists all subsequences |
| `list_sequences` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Lists all LevelSequence assets |
| `duplicate_sequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Duplicates sequence asset |
| `delete_sequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Deletes sequence asset |
| **Shot Tracks** | | | |
| `add_shot_track` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Adds UMovieSceneSubTrack |
| `add_shot` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Adds shot section with sequence |
| `remove_shot` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Removes shot section |
| `get_shots` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Lists all shots |
| **Camera** | | | |
| `create_cine_camera_actor` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Spawns ACineCameraActor with settings |
| `configure_camera_settings` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Sets focal length, aperture, sensor size |
| `add_camera_cut_track` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Adds UMovieSceneCameraCutTrack |
| `add_camera_cut` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Adds camera cut section with binding |
| **Actor Binding** | | | |
| `bind_actor` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Binds as possessable or spawnable |
| `unbind_actor` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Removes binding |
| `get_bindings` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Lists all bindings |
| **Tracks & Sections** | | | |
| `add_track` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Adds Transform, Animation, Audio, Event, Fade tracks |
| `remove_track` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Removes track |
| `get_tracks` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Lists tracks for binding or master |
| `add_section` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Adds section to track |
| `remove_section` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Removes section |
| **Keyframes** | | | |
| `add_keyframe` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Adds keyframe to float channel |
| `remove_keyframe` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Removes keyframe at frame |
| `get_keyframes` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Lists all keyframes with channel info |
| **Playback** | | | |
| `set_playback_range` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Sets playback start/end time |
| `get_playback_range` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Returns playback range |
| `set_display_rate` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Sets FPS display rate |
| `get_sequence_info` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Returns full sequence metadata |
| `play_sequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Plays via ALevelSequenceActor |
| `pause_sequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Pauses playback |
| `stop_sequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Stops playback |
| `scrub_to_time` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | Seeks to time |
| **Export** | | | |
| `export_sequence` | `McpAutomationBridge_SequencerConsolidatedHandlers.cpp` | `HandleSequencerAction` | FBX/USD export support |

## 44. Movie Render Manager (`manage_movie_render`) - Phase 30

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Queue Management** | | | Requires MovieRenderPipeline plugin |
| `create_queue` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Gets/creates UMoviePipelineQueue |
| `add_job` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Adds render job with sequence/map |
| `remove_job` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Removes job by index |
| `clear_queue` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Clears all jobs |
| `get_queue` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Lists all jobs |
| **Job Configuration** | | | |
| `configure_job` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Updates job settings |
| `configure_output` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Sets output directory, resolution, format |
| **Render Settings** | | | |
| `add_render_pass` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Adds deferred render passes |
| `configure_anti_aliasing` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Spatial/temporal sample counts |
| `configure_high_res_settings` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Tile count, overlap ratio |
| `add_console_variable` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Adds CVars to job |
| **Render Execution** | | | |
| `start_render` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Starts PIE executor |
| `stop_render` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Cancels active pipeline |
| `get_render_status` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Returns render state |
| `get_render_progress` | `McpAutomationBridge_MovieRenderHandlers.cpp` | `HandleMovieRenderAction` | Returns progress info |

## 45. Media Manager (`manage_media`) - Phase 30

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Asset Creation** | | | Requires MediaAssets module |
| `create_media_player` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Creates UMediaPlayer asset |
| `create_file_media_source` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Creates UFileMediaSource |
| `create_stream_media_source` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Creates UStreamMediaSource |
| `create_media_texture` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Creates UMediaTexture with player binding |
| `create_media_playlist` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Creates UMediaPlaylist |
| **Media Info** | | | |
| `get_media_info` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Returns duration, tracks, state |
| **Playback Control** | | | |
| `open_source` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Opens media source in player |
| `open_url` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Opens URL in player |
| `play` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Starts playback |
| `pause` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Pauses playback |
| `stop` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Stops playback |
| `close` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Closes media |
| `seek` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Seeks to time |
| `set_rate` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Sets playback rate |
| `set_looping` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Enables/disables looping |
| **Playback Query** | | | |
| `get_duration` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Returns total duration |
| `get_time` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Returns current time |
| `get_state` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Returns full playback state |
| **Playlist Management** | | | |
| `add_to_playlist` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Adds source to playlist |
| `get_playlist` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Lists playlist contents |
| **Texture Binding** | | | |
| `bind_to_texture` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Binds player to texture |
| `unbind_from_texture` | `McpAutomationBridge_MediaHandlers.cpp` | `HandleMediaAction` | Unbinds player from texture |

## 46. Data Manager (`manage_data`) - Phase 31

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Data Assets** | | | |
| `create_data_asset` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Creates UDataAsset with class name |
| `create_primary_data_asset` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Creates UPrimaryDataAsset |
| `get_data_asset_info` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Returns data asset properties |
| `set_data_asset_property` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Sets property via reflection |
| **Data Tables** | | | |
| `create_data_table` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Creates UDataTable with row struct |
| `add_data_table_row` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Adds row with JSON data |
| `remove_data_table_row` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Removes row by name |
| `get_data_table_row` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Returns single row as JSON |
| `get_data_table_rows` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Returns all rows as JSON array |
| `import_data_table_csv` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Imports from CSV file |
| `export_data_table_csv` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Exports to CSV file |
| `empty_data_table` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Removes all rows |
| **Curve Tables** | | | |
| `create_curve_table` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Creates UCurveTable with interpolation |
| `add_curve_row` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Adds curve row with keys |
| `get_curve_value` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Evaluates curve at X value |
| `import_curve_table_csv` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Imports from CSV file |
| `export_curve_table_csv` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Exports to CSV file |
| **Save Game** | | | |
| `create_save_game_blueprint` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Creates USaveGame blueprint |
| `save_game_to_slot` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Saves game to named slot |
| `load_game_from_slot` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Loads game from slot |
| `delete_save_slot` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Deletes save slot |
| `does_save_exist` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Checks if slot exists |
| `get_save_slot_names` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Lists all save slots |
| **Gameplay Tags** | | | |
| `create_gameplay_tag` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Creates tag in config |
| `add_native_gameplay_tag` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Adds native gameplay tag |
| `request_gameplay_tag` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Requests existing tag |
| `check_tag_match` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Checks tag hierarchy match |
| `create_tag_container` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Creates tag container |
| `add_tag_to_container` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Adds tag to container |
| `remove_tag_from_container` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Removes tag from container |
| `has_tag` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Checks container for tag |
| `get_all_gameplay_tags` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Lists all registered tags |
| **Config** | | | |
| `read_config_value` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Reads from config section |
| `write_config_value` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Writes to config section |
| `get_config_section` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Returns entire section |
| `flush_config` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Writes config to disk |
| `reload_config` | `McpAutomationBridge_DataHandlers.cpp` | `HandleManageDataAction` | Reloads config from disk |

## 47. Build & Deployment Manager (`manage_build`) - Phase 32

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Build Pipeline** | | | |
| `run_ubt` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Runs UnrealBuildTool with args |
| `generate_project_files` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Generates project files |
| `compile_shaders` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Reports shader compilation status |
| `cook_content` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Runs content cooking for platform |
| `package_project` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Packages project for distribution |
| `configure_build_settings` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Configures build optimization settings |
| `get_build_info` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Returns engine version, build config |
| **Platform Configuration** | | | |
| `configure_platform` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Configures platform-specific settings |
| `get_platform_settings` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Returns platform settings |
| `get_target_platforms` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Lists available target platforms |
| **Asset Validation** | | | |
| `validate_assets` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Runs asset validation commandlet |
| `audit_assets` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Audits assets for issues |
| `get_asset_size_info` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Returns asset size breakdown |
| `get_asset_references` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Returns asset reference graph |
| **PAK & Chunking** | | | |
| `configure_chunking` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Configures asset chunking settings |
| `create_pak_file` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Creates PAK file (via UAT) |
| `configure_encryption` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Configures PAK encryption |
| **Plugin Management** | | | |
| `list_plugins` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Lists all plugins with status |
| `enable_plugin` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Enables plugin in .uproject |
| `disable_plugin` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Disables plugin in .uproject |
| `get_plugin_info` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Returns plugin descriptor info |
| **DDC Management** | | | |
| `clear_ddc` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Clears Derived Data Cache |
| `get_ddc_stats` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Returns DDC statistics |
| `configure_ddc` | `McpAutomationBridge_BuildHandlers.cpp` | `HandleManageBuildAction` | Reports DDC configuration |

## 48. Testing & Quality Manager (`manage_testing`) - Phase 33

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Automation Tests** | | | |
| `list_tests` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Lists available automation tests |
| `run_tests` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Runs automation tests with filter |
| `run_test` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Runs single automation test by name |
| `get_test_results` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Gets automation test results |
| `get_test_info` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Gets test system info |
| **Functional Tests** | | | |
| `list_functional_tests` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Lists functional tests in level |
| `run_functional_test` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Runs specific functional test |
| `get_functional_test_results` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Gets functional test results |
| **Profiling - Trace** | | | |
| `start_trace` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Starts Unreal Insights trace |
| `stop_trace` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Stops Unreal Insights trace |
| `get_trace_status` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Gets trace recording status |
| **Profiling - Visual Logger** | | | |
| `enable_visual_logger` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Enables visual logger |
| `disable_visual_logger` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Disables visual logger |
| `get_visual_logger_status` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Gets visual logger status |
| **Profiling - Stats** | | | |
| `start_stats_capture` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Starts stats capture |
| `stop_stats_capture` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Stops stats capture |
| `get_memory_report` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Gets memory usage report |
| `get_performance_stats` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Gets performance statistics |
| **Validation** | | | |
| `validate_asset` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Validates single asset |
| `validate_assets_in_path` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Validates all assets in directory |
| `validate_blueprint` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Validates blueprint compilation |
| `check_map_errors` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Checks current map for errors |
| `fix_redirectors` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Fixes asset redirectors |
| `get_redirectors` | `McpAutomationBridge_TestingHandlers.cpp` | `HandleManageTestingAction` | Lists asset redirectors |

## 49. Editor Utilities Manager (`manage_editor_utilities`) - Phase 34

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Editor Modes** | | | |
| `set_editor_mode` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Sets editor mode (Default, Landscape, Foliage, MeshPaint, Geometry) |
| `configure_editor_preferences` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Configures editor preference category |
| `set_grid_settings` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Sets grid size, rotation snap, scale snap |
| `set_snap_settings` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Sets snap settings |
| **Content Browser** | | | |
| `navigate_to_path` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Navigates content browser to path |
| `sync_to_asset` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Syncs content browser to specific asset |
| `create_collection` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Creates asset collection (Local/Private/Shared) |
| `add_to_collection` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Adds assets to collection |
| `show_in_explorer` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Opens file explorer at asset path |
| **Selection** | | | |
| `select_actor` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Selects actor by name |
| `select_actors_by_class` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Selects all actors of class |
| `select_actors_by_tag` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Selects all actors with tag |
| `deselect_all` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Deselects all actors |
| `group_actors` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Groups selected actors |
| `ungroup_actors` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Ungroups selected actors |
| `get_selected_actors` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Gets list of selected actors |
| **Collision** | | | |
| `create_collision_channel` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Creates custom collision channel (requires DefaultEngine.ini) |
| `create_collision_profile` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Creates collision profile |
| `configure_channel_responses` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Configures channel response mappings |
| `get_collision_info` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Gets available channels and profiles |
| **Physical Materials** | | | |
| `create_physical_material` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Creates UPhysicalMaterial asset |
| `set_friction` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Sets friction on physical material |
| `set_restitution` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Sets restitution/bounciness |
| `configure_surface_type` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Configures surface type |
| `get_physical_material_info` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Gets physical material properties |
| **Subsystems** | | | |
| `create_game_instance_subsystem` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Guidance for creating game instance subsystem |
| `create_world_subsystem` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Guidance for creating world subsystem |
| `create_local_player_subsystem` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Guidance for creating local player subsystem |
| `get_subsystem_info` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Gets available subsystem types |
| **Timers** | | | |
| `set_timer` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Sets up timer for function call |
| `clear_timer` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Clears timer by handle |
| `clear_all_timers` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Clears all timers for actor |
| `get_active_timers` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Gets active timers |
| **Delegates** | | | |
| `create_event_dispatcher` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Creates event dispatcher in blueprint |
| `bind_to_event` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Guidance for event binding |
| `unbind_from_event` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Guidance for event unbinding |
| `broadcast_event` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Guidance for event broadcasting |
| `create_blueprint_interface` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Creates UBlueprint interface asset |
| **Transactions** | | | |
| `begin_transaction` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Begins undo transaction |
| `end_transaction` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Ends undo transaction |
| `cancel_transaction` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Cancels current transaction |
| `undo` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Performs undo operation |
| `redo` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Performs redo operation |
| `get_transaction_history` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Gets transaction history and undo/redo state |
| **Utility** | | | |
| `get_editor_utilities_info` | `McpAutomationBridge_EditorUtilitiesHandlers.cpp` | `HandleManageEditorUtilitiesAction` | Gets current editor state info |

## 50. Gameplay Systems Manager (`manage_gameplay_systems`) - Phase 35

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Targeting** | | | |
| `create_targeting_component` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates targeting component with settings |
| `configure_lock_on_target` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Configures lock-on target behavior |
| `configure_aim_assist` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Configures aim assist settings |
| **Checkpoints** | | | |
| `create_checkpoint_actor` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates checkpoint actor with trigger |
| `save_checkpoint` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Saves game state to checkpoint slot |
| `load_checkpoint` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Loads game state from checkpoint slot |
| **Objectives** | | | |
| `create_objective` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates objective with type and state |
| `set_objective_state` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Sets objective state and progress |
| `configure_objective_markers` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Configures objective marker visibility |
| **World Markers** | | | |
| `create_world_marker` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates world marker at location |
| `create_ping_system` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates multiplayer ping system |
| `configure_marker_widget` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Configures marker widget settings |
| **Photo Mode** | | | |
| `enable_photo_mode` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Enables/disables photo mode |
| `configure_photo_mode_camera` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Configures photo mode camera settings |
| `take_photo_mode_screenshot` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Takes screenshot in photo mode |
| **Quest/Dialogue** | | | |
| `create_quest_data_asset` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates quest data asset |
| `create_dialogue_tree` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates dialogue tree data asset |
| `add_dialogue_node` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Adds node to dialogue tree |
| `play_dialogue` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Starts dialogue playback |
| **Instancing** | | | |
| `create_instanced_static_mesh_component` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates ISM component on actor |
| `create_hierarchical_instanced_static_mesh` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates HISM component on actor |
| `add_instance` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Adds instance(s) to ISM/HISM component |
| `remove_instance` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Removes instance(s) from ISM/HISM component |
| `get_instance_count` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Gets instance count from ISM/HISM component |
| **HLOD** | | | |
| `create_hlod_layer` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates HLOD layer asset |
| `configure_hlod_settings` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Configures HLOD layer settings |
| `build_hlod` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Initiates HLOD build |
| `assign_actor_to_hlod` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Assigns actor to HLOD layer |
| **Localization** | | | |
| `create_string_table` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates UStringTable asset |
| `add_string_entry` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Adds entry to string table |
| `get_string_entry` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Gets entry from string table |
| `import_localization` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Imports localization from CSV/PO/JSON |
| `export_localization` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Exports localization to CSV/PO/JSON |
| `set_culture` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Sets current culture/language |
| `get_available_cultures` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Gets list of available cultures |
| **Scalability** | | | |
| `create_device_profile` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Creates device profile |
| `configure_scalability_group` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Configures scalability group quality |
| `set_quality_level` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Sets overall quality level (0-4) |
| `get_scalability_settings` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Gets current scalability settings |
| `set_resolution_scale` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Sets resolution scale percentage |
| **Utility** | | | |
| `get_gameplay_systems_info` | `McpAutomationBridge_GameplaySystemsHandlers.cpp` | `HandleManageGameplaySystemsAction` | Gets gameplay systems status info |

## 51. Character & Avatar Manager (`manage_character_avatar`) - Phase 36

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **MetaHuman** | | | |
| `import_metahuman` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Guidance for MetaHuman SDK import |
| `spawn_metahuman_actor` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Spawns MetaHuman actor in level |
| `get_metahuman_component` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Gets MetaHuman component from actor |
| `set_body_type` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets MetaHuman body type |
| `set_face_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets MetaHuman face rig parameter |
| `set_skin_tone` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets MetaHuman skin tone |
| `set_hair_style` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets MetaHuman hair style |
| `set_eye_color` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets MetaHuman eye color |
| `configure_metahuman_lod` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Configures MetaHuman LOD settings |
| `enable_body_correctives` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Enables body corrective shapes |
| `enable_neck_correctives` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Enables neck corrective shapes |
| `set_quality_level` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets MetaHuman quality level |
| `configure_face_rig` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Configures face rig settings |
| `set_body_part` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets MetaHuman body part |
| `get_metahuman_info` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Gets MetaHuman component info |
| `list_available_presets` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Lists available MetaHuman presets |
| `apply_preset` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Applies MetaHuman preset |
| `export_metahuman_settings` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Exports MetaHuman settings to JSON |
| **Groom/Hair** | | | |
| `create_groom_asset` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Creates UGroomAsset (requires HairStrands) |
| `import_groom` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Imports groom from Alembic file |
| `create_groom_binding` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Creates UGroomBindingAsset |
| `spawn_groom_actor` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Spawns AGroomActor in level |
| `attach_groom_to_skeletal_mesh` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Attaches groom to skeletal mesh |
| `configure_hair_simulation` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Configures hair physics simulation |
| `set_hair_width` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets groom hair width |
| `set_hair_root_scale` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets hair root width scale |
| `set_hair_tip_scale` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets hair tip width scale |
| `set_hair_color` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets groom hair color |
| `configure_hair_physics` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Configures hair physics parameters |
| `configure_hair_rendering` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Configures hair rendering settings |
| `enable_hair_simulation` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Enables/disables hair simulation |
| `get_groom_info` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Gets groom asset information |
| **Mutable/Customizable** | | | |
| `create_customizable_object` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Creates UCustomizableObject asset |
| `compile_customizable_object` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Compiles customizable object |
| `create_customizable_instance` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Creates UCustomizableObjectInstance |
| `set_bool_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets bool parameter on instance |
| `set_int_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets int parameter on instance |
| `set_float_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets float parameter on instance |
| `set_color_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets color parameter on instance |
| `set_vector_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets vector parameter on instance |
| `set_texture_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets texture parameter on instance |
| `set_transform_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets transform parameter on instance |
| `set_projector_parameter` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets projector parameter on instance |
| `update_skeletal_mesh` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Updates skeletal mesh from instance |
| `bake_customizable_instance` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Bakes instance to static mesh |
| `get_parameter_info` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Gets customizable object parameter info |
| `get_instance_info` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Gets customizable instance info |
| `spawn_customizable_actor` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Spawns actor with customizable component |
| **Ready Player Me** | | | |
| `load_avatar_from_url` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Loads RPM avatar from URL |
| `load_avatar_from_glb` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Loads RPM avatar from GLB file |
| `create_rpm_actor` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Creates Ready Player Me actor |
| `apply_avatar_to_character` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Applies RPM avatar to character |
| `configure_rpm_materials` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Configures RPM material settings |
| `set_rpm_outfit` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Sets RPM outfit configuration |
| `get_avatar_metadata` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Gets RPM avatar metadata |
| `cache_avatar` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Caches RPM avatar locally |
| `clear_avatar_cache` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Clears RPM avatar cache |
| `create_rpm_animation_blueprint` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Creates animation BP for RPM |
| `retarget_rpm_animation` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Retargets animations to RPM skeleton |
| `get_rpm_info` | `McpAutomationBridge_CharacterAvatarHandlers.cpp` | `HandleManageCharacterAvatarAction` | Gets Ready Player Me integration info |

## 52. Asset & Content Plugins Manager (`manage_asset_plugins`) - Phase 37

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Interchange (18 actions)** | | | |
| `import_with_interchange` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports asset using Interchange pipeline |
| `export_with_interchange` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Exports asset using Interchange pipeline |
| `create_interchange_pipeline` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates custom Interchange pipeline |
| `configure_static_mesh_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures static mesh import settings |
| `configure_skeletal_mesh_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures skeletal mesh import settings |
| `configure_texture_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures texture import settings |
| `configure_material_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures material import settings |
| `configure_animation_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures animation import settings |
| `set_pipeline_source` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets pipeline source file |
| `get_available_translators` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Lists available Interchange translators |
| `register_factory_node` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Registers custom factory node |
| `configure_scene_import` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures scene import settings |
| `configure_common_pipelines_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures common pipeline settings |
| `get_interchange_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets Interchange plugin status |
| **USD (24 actions)** | | | |
| `create_usd_stage` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates new USD stage |
| `open_usd_stage` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Opens existing USD stage |
| `close_usd_stage` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Closes USD stage |
| `save_usd_stage` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Saves USD stage to file |
| `create_usd_prim` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates prim in USD stage |
| `set_usd_prim_attribute` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets prim attribute value |
| `get_usd_prim_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets prim information |
| `add_reference` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Adds reference to USD prim |
| `export_actor_to_usd` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Exports actor to USD |
| `export_level_to_usd` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Exports level to USD |
| `import_usd_layer` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports USD layer |
| `create_usd_layer` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates new USD layer |
| `mute_usd_layer` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Mutes/unmutes USD layer |
| `set_edit_target` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets USD edit target layer |
| `enable_usd_live_edit` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Enables live USD editing |
| `create_usd_assets_from_prims` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates UE assets from USD prims |
| `configure_usd_stage_options` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures USD stage options |
| `get_usd_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets USD plugin status |
| **Alembic (15 actions)** | | | |
| `import_alembic_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports Alembic file |
| `set_alembic_import_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures Alembic import settings |
| `create_geometry_cache_track` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates geometry cache track |
| `import_alembic_groom` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports Alembic groom data |
| `configure_geometry_cache_playback` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures geometry cache playback |
| `create_geometry_cache_actor` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates geometry cache actor |
| `sample_geometry_cache_transform` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Samples geometry cache transform |
| `configure_alembic_compression` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures Alembic compression |
| `export_alembic` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Exports to Alembic format |
| `get_alembic_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets Alembic plugin status |
| **glTF (16 actions)** | | | |
| `import_gltf_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports glTF file |
| `import_glb_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports GLB file |
| `export_to_gltf` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Exports to glTF format |
| `export_to_glb` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Exports to GLB format |
| `configure_gltf_import_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures glTF import settings |
| `configure_gltf_export_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures glTF export settings |
| `import_gltf_static_mesh` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports glTF as static mesh |
| `import_gltf_skeletal_mesh` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports glTF as skeletal mesh |
| `import_gltf_scene` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports glTF scene hierarchy |
| `set_draco_compression` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Enables Draco compression |
| `configure_material_import` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures material import |
| `get_gltf_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets glTF plugin status |
| **Datasmith (18 actions)** | | | |
| `import_datasmith_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports Datasmith file |
| `configure_datasmith_import` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures Datasmith import |
| `import_cad_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports CAD file |
| `import_cad_assembly` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports CAD assembly |
| `configure_cad_import_options` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures CAD import options |
| `import_revit_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports Revit file |
| `import_sketchup_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports SketchUp file |
| `import_3ds_max_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports 3ds Max file |
| `import_rhino_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports Rhino file |
| `import_archicad_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports ArchiCAD file |
| `configure_tessellation` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures tessellation settings |
| `configure_lightmap_settings` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures lightmap settings |
| `reimport_datasmith` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Reimports Datasmith scene |
| `get_datasmith_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets Datasmith plugin status |
| **SpeedTree (13 actions)** | | | |
| `import_speedtree_model` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports SpeedTree model |
| `import_speedtree_9` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports SpeedTree 9 format |
| `import_speedtree_atlas` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports SpeedTree atlas |
| `configure_speedtree_wind` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures SpeedTree wind |
| `set_speedtree_wind_type` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets SpeedTree wind type |
| `set_speedtree_wind_speed` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets SpeedTree wind speed |
| `configure_speedtree_lod` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures SpeedTree LOD |
| `set_speedtree_lod_distances` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets SpeedTree LOD distances |
| `set_speedtree_lod_transition` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets SpeedTree LOD transition |
| `create_speedtree_material` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates SpeedTree material |
| `configure_speedtree_collision` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures SpeedTree collision |
| `apply_speedtree_wind_to_material` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Applies wind to material |
| `get_speedtree_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets SpeedTree plugin status |
| **Quixel/Fab (12 actions)** | | | |
| `connect_to_bridge` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Connects to Quixel Bridge |
| `disconnect_from_bridge` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Disconnects from Bridge |
| `import_megascan_surface` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports Megascan surface |
| `import_megascan_3d_asset` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports Megascan 3D asset |
| `import_megascan_3d_plant` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports Megascan 3D plant |
| `configure_megascan_lods` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures Megascan LODs |
| `apply_megascan_material` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Applies Megascan material |
| `search_quixel_library` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Searches Quixel library |
| `download_fab_asset` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Downloads Fab asset |
| `get_quixel_bridge_status` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets Quixel Bridge status |
| `get_quixel_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets Quixel plugin status |
| **Houdini Engine (22 actions)** | | | |
| `import_hda` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports HDA file |
| `instantiate_hda` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Instantiates HDA in level |
| `rebake_hda` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Rebakes HDA output |
| `set_hda_float_parameter` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets HDA float parameter |
| `set_hda_int_parameter` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets HDA int parameter |
| `set_hda_string_parameter` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets HDA string parameter |
| `set_hda_toggle_parameter` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets HDA toggle parameter |
| `get_hda_parameter_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets HDA parameter info |
| `cook_hda` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Cooks HDA |
| `recook_hda` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Recooks HDA |
| `bake_hda_to_actors` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Bakes HDA to actors |
| `bake_hda_to_blueprint` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Bakes HDA to Blueprint |
| `bake_hda_to_foliage` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Bakes HDA to foliage |
| `bake_hda_to_landscape` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Bakes HDA to landscape |
| `set_hda_input_object` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets HDA input object |
| `set_hda_input_landscape` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets HDA input landscape |
| `set_hda_input_curve` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets HDA input curve |
| `clear_hda_input` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Clears HDA input |
| `get_hda_output_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets HDA output info |
| `rebuild_hda` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Rebuilds HDA instance |
| `delete_hda_instance` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Deletes HDA instance |
| `get_houdini_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets Houdini Engine status |
| **Substance (20 actions)** | | | |
| `import_sbsar_file` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Imports SBSAR file |
| `create_substance_graph_instance` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates Substance graph instance |
| `set_substance_input_float` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets Substance float input |
| `set_substance_input_int` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets Substance int input |
| `set_substance_input_color` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets Substance color input |
| `set_substance_input_image` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Sets Substance image input |
| `get_substance_input_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets Substance input info |
| `render_substance_textures` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Renders Substance textures |
| `configure_substance_output_size` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures output size |
| `randomize_substance_seed` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Randomizes Substance seed |
| `create_material_from_substance` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Creates material from Substance |
| `configure_substance_output` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Configures Substance output |
| `reimport_substance` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Reimports Substance asset |
| `get_substance_info` | `McpAutomationBridge_AssetPluginsHandlers.cpp` | `HandleManageAssetPluginsAction` | Gets Substance plugin status |

## 53. Audio Middleware Manager (`manage_audio_middleware`) - Phase 38

> **Note:** Bink Video actions are fully implemented. Wwise and FMOD require the respective external plugins (Audiokinetic Wwise, FMOD Studio) to be installed. Core actions for Wwise/FMOD (event posting, RTPC/parameter control, bank loading, switches, states) are fully implemented. Advanced actions (spatial audio configuration, room/portal setup, occlusion, aux sends) return acknowledgment status when the plugin is detected but require project-specific configuration.

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Wwise (30 actions)** | | | |
| `connect_wwise_project` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Notes Wwise project path |
| `post_wwise_event` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Posts Wwise event on actor |
| `post_wwise_event_at_location` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Posts Wwise event at location |
| `stop_wwise_event` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Stops Wwise event by ID |
| `set_rtpc_value` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets global RTPC value |
| `set_rtpc_value_on_actor` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets RTPC on actor |
| `get_rtpc_value` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets RTPC value |
| `set_wwise_switch` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets global switch |
| `set_wwise_switch_on_actor` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets switch on actor |
| `set_wwise_state` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets global state |
| `load_wwise_bank` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Loads sound bank |
| `unload_wwise_bank` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Unloads sound bank |
| `get_loaded_banks` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets loaded banks info |
| `create_wwise_component` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Creates AkComponent on actor |
| `configure_wwise_component` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures AkComponent |
| `configure_spatial_audio` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures spatial audio |
| `configure_room` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures Wwise room |
| `configure_portal` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures Wwise portal |
| `set_listener_position` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets listener position |
| `get_wwise_event_duration` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets event duration |
| `create_wwise_trigger` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Creates Wwise trigger |
| `set_wwise_game_object` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Registers game object |
| `unset_wwise_game_object` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Unregisters game object |
| `post_wwise_trigger` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Posts Wwise trigger |
| `set_aux_send` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets aux send |
| `configure_occlusion` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures occlusion |
| `set_wwise_project_path` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets project path |
| `get_wwise_status` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets Wwise status |
| `configure_wwise_init` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures initialization |
| `restart_wwise_engine` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Restarts Wwise engine |
| **FMOD (30 actions)** | | | |
| `connect_fmod_project` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Notes FMOD project path |
| `play_fmod_event` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Plays FMOD event 2D |
| `play_fmod_event_at_location` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Plays FMOD event at location |
| `stop_fmod_event` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Stops FMOD events on actor |
| `set_fmod_parameter` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets parameter on component |
| `set_fmod_global_parameter` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets global parameter |
| `get_fmod_parameter` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets parameter value |
| `load_fmod_bank` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Loads FMOD bank |
| `unload_fmod_bank` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Unloads FMOD bank |
| `get_fmod_loaded_banks` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets loaded banks |
| `create_fmod_component` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Creates FMOD component |
| `configure_fmod_component` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures FMOD component |
| `set_fmod_bus_volume` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets bus volume |
| `set_fmod_bus_paused` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Pauses bus |
| `set_fmod_bus_mute` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Mutes bus |
| `set_fmod_vca_volume` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets VCA volume |
| `apply_fmod_snapshot` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Applies FMOD snapshot |
| `release_fmod_snapshot` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Releases FMOD snapshot |
| `set_fmod_listener_attributes` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets listener attributes |
| `get_fmod_event_info` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets event info |
| `configure_fmod_occlusion` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures occlusion |
| `configure_fmod_attenuation` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures attenuation |
| `set_fmod_studio_path` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets studio path |
| `get_fmod_status` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets FMOD status |
| `configure_fmod_init` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures initialization |
| `restart_fmod_engine` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Restarts FMOD engine |
| `set_fmod_3d_attributes` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets 3D attributes |
| `get_fmod_memory_usage` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets memory usage |
| `pause_all_fmod_events` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Pauses all events |
| `resume_all_fmod_events` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Resumes all events |
| **Bink Video (20 actions)** | | | |
| `create_bink_media_player` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Creates UBinkMediaPlayer |
| `open_bink_video` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Opens video URL |
| `play_bink` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Plays video |
| `pause_bink` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Pauses video |
| `stop_bink` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Stops video |
| `seek_bink` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Seeks to time |
| `set_bink_looping` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets looping |
| `set_bink_rate` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets playback rate |
| `set_bink_volume` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets volume |
| `get_bink_duration` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets duration |
| `get_bink_time` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets current time |
| `get_bink_status` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets playback status |
| `create_bink_texture` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Creates UBinkMediaTexture |
| `configure_bink_texture` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures texture settings |
| `set_bink_texture_player` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Assigns player to texture |
| `draw_bink_to_texture` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Triggers draw |
| `configure_bink_buffer_mode` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets buffer mode |
| `configure_bink_sound_track` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Configures sound track |
| `configure_bink_draw_style` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Sets draw style |
| `get_bink_dimensions` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets video dimensions |
| **Utility** | | | |
| `get_audio_middleware_info` | `McpAutomationBridge_AudioMiddlewareHandlers.cpp` | `HandleManageAudioMiddlewareAction` | Gets middleware availability |

## 54. Live Link & Motion Capture Manager (`manage_livelink`) - Phase 39

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Sources (9 actions)** | | | |
| `add_livelink_source` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Adds Live Link source by factory |
| `remove_livelink_source` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Removes source by GUID |
| `list_livelink_sources` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Lists all active sources |
| `get_source_status` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets source connection status |
| `get_source_type` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets source type name |
| `configure_source_settings` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures source settings |
| `add_messagebus_source` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Adds Message Bus source |
| `discover_messagebus_sources` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Discovers network sources |
| `remove_all_sources` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Removes all sources |
| **Subjects (15 actions)** | | | |
| `list_livelink_subjects` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Lists all subjects |
| `get_subject_role` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets subject role type |
| `get_subject_state` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets subject enabled state |
| `enable_subject` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Enables subject |
| `disable_subject` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Disables subject |
| `pause_subject` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Pauses subject updates |
| `unpause_subject` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Resumes subject updates |
| `clear_subject_frames` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Clears buffered frames |
| `get_subject_static_data` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets static data (bones, etc.) |
| `get_subject_frame_data` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets latest frame data |
| `add_virtual_subject` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Creates virtual subject |
| `remove_virtual_subject` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Removes virtual subject |
| `configure_subject_settings` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures subject settings |
| `get_subject_frame_times` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets frame timing info |
| `get_subjects_by_role` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Filters subjects by role |
| **Presets (8 actions)** | | | |
| `create_livelink_preset` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Creates preset asset |
| `load_livelink_preset` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Loads preset from path |
| `apply_livelink_preset` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Applies preset to client |
| `add_preset_to_client` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Adds preset sources to client |
| `build_preset_from_client` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Builds preset from current config |
| `save_livelink_preset` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Saves preset to disk |
| `get_preset_sources` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets sources in preset |
| `get_preset_subjects` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets subjects in preset |
| **Components (8 actions)** | | | |
| `add_livelink_controller` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Adds controller to actor |
| `configure_livelink_controller` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures controller |
| `set_controller_subject` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Sets controller subject |
| `set_controller_role` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Sets controller role |
| `enable_controller_evaluation` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Enables evaluation |
| `disable_controller_evaluation` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Disables evaluation |
| `set_controlled_component` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Sets component to control |
| `get_controller_info` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets controller info |
| **Timecode (6 actions)** | | | |
| `configure_livelink_timecode` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures timecode |
| `set_timecode_provider` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Sets timecode provider |
| `get_livelink_timecode` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets current timecode |
| `configure_time_sync` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures time sync |
| `set_buffer_settings` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Sets buffer settings |
| `configure_frame_interpolation` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures interpolation |
| **Face Tracking (8 actions)** | | | |
| `configure_face_source` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures face source |
| `configure_arkit_mapping` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Maps ARKit blendshapes |
| `set_face_neutral_pose` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Sets neutral pose |
| `get_face_blendshapes` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets blendshape values |
| `configure_blendshape_remap` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Remaps blendshapes |
| `apply_face_to_skeletal_mesh` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Applies face data to mesh |
| `configure_face_retargeting` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures retargeting |
| `get_face_tracking_status` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets face tracking status |
| **Skeleton Mapping (6 actions)** | | | |
| `configure_skeleton_mapping` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Configures skeleton mapping |
| `create_retarget_asset` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Creates retarget asset |
| `configure_bone_mapping` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Maps bones |
| `configure_curve_mapping` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Maps curves |
| `apply_mocap_to_character` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Applies mocap to character |
| `get_skeleton_mapping_info` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets mapping info |
| **Utility (4 actions)** | | | |
| `get_livelink_info` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Gets Live Link availability |
| `list_available_roles` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Lists available roles |
| `list_source_factories` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Lists source factories |
| `force_livelink_tick` | `McpAutomationBridge_LiveLinkHandlers.cpp` | `HandleManageLiveLinkAction` | Forces tick update |

---

## 55. Virtual Production Manager (`manage_virtual_production`) - Phase 40

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **nDisplay Cluster (10 actions)** | | | |
| `create_ndisplay_config` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates nDisplay config asset |
| `add_cluster_node` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds cluster node to config |
| `remove_cluster_node` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Removes cluster node |
| `add_viewport` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds viewport to node |
| `remove_viewport` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Removes viewport |
| `set_viewport_camera` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets viewport camera |
| `configure_viewport_region` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures viewport region |
| `set_projection_policy` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets projection policy |
| `configure_warp_blend` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures warp/blend |
| `list_cluster_nodes` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Lists all cluster nodes |
| **nDisplay LED/ICVFX (10 actions)** | | | |
| `create_led_wall` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates LED wall |
| `configure_led_wall_size` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures LED wall size |
| `configure_icvfx_camera` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures ICVFX camera |
| `add_icvfx_camera` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds ICVFX camera |
| `remove_icvfx_camera` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Removes ICVFX camera |
| `configure_inner_frustum` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures inner frustum |
| `configure_outer_viewport` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures outer viewport |
| `set_chromakey_settings` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets chromakey settings |
| `configure_light_cards` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures light cards |
| `set_stage_settings` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets stage settings |
| **nDisplay Sync (5 actions)** | | | |
| `set_sync_policy` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets sync policy |
| `configure_genlock` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures genlock |
| `set_primary_node` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets primary node |
| `configure_network_settings` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures network settings |
| `get_ndisplay_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets nDisplay info |
| **Composure (12 actions)** | | | |
| `create_composure_element` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates composure element |
| `delete_composure_element` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Deletes composure element |
| `add_composure_layer` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds composure layer |
| `remove_composure_layer` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Removes composure layer |
| `attach_child_layer` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Attaches child layer |
| `detach_child_layer` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Detaches child layer |
| `add_input_pass` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds input pass |
| `add_transform_pass` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds transform pass |
| `add_output_pass` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds output pass |
| `configure_chroma_keyer` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures chroma keyer |
| `bind_render_target` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Binds render target |
| `get_composure_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets Composure info |
| **OpenColorIO (10 actions)** | | | |
| `create_ocio_config` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates OCIO config asset |
| `load_ocio_config` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Loads OCIO config file |
| `get_ocio_colorspaces` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets available colorspaces |
| `get_ocio_displays` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets display transforms |
| `set_display_view` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets display/view |
| `add_colorspace_transform` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds colorspace transform |
| `apply_ocio_look` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Applies OCIO look |
| `configure_viewport_ocio` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures viewport OCIO |
| `set_ocio_working_colorspace` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets working colorspace |
| `get_ocio_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets OCIO info |
| **Remote Control (15 actions)** | | | |
| `create_remote_control_preset` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates RC preset |
| `load_remote_control_preset` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Loads RC preset |
| `expose_property` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Exposes property |
| `unexpose_property` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Unexposes property |
| `expose_function` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Exposes function |
| `create_controller` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates controller |
| `bind_controller` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Binds controller |
| `get_exposed_properties` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets exposed properties |
| `set_exposed_property_value` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets exposed value |
| `get_exposed_property_value` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets exposed value |
| `start_web_server` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Starts web server |
| `stop_web_server` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Stops web server |
| `get_web_server_status` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets web server status |
| `create_layout_group` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates layout group |
| `get_remote_control_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets RC info |
| **DMX (20 actions)** | | | |
| `create_dmx_library` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates DMX library |
| `import_gdtf` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Imports GDTF file |
| `create_fixture_type` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates fixture type |
| `add_fixture_mode` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds fixture mode |
| `add_fixture_function` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds fixture function |
| `create_fixture_patch` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates fixture patch |
| `assign_fixture_to_universe` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Assigns to universe |
| `configure_dmx_port` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures DMX port |
| `create_artnet_port` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates Art-Net port |
| `create_sacn_port` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates sACN port |
| `send_dmx` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sends DMX data |
| `receive_dmx` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Receives DMX data |
| `set_fixture_channel_value` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets channel value |
| `get_fixture_channel_value` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets channel value |
| `add_dmx_component` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds DMX component |
| `configure_dmx_component` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures DMX component |
| `list_dmx_universes` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Lists universes |
| `list_dmx_fixtures` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Lists fixtures |
| `create_dmx_sequencer_track` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates sequencer track |
| `get_dmx_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets DMX info |
| **OSC (12 actions)** | | | |
| `create_osc_server` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates OSC server |
| `stop_osc_server` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Stops OSC server |
| `create_osc_client` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates OSC client |
| `send_osc_message` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sends OSC message |
| `send_osc_bundle` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sends OSC bundle |
| `bind_osc_address` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Binds OSC address |
| `unbind_osc_address` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Unbinds OSC address |
| `bind_osc_to_property` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Binds OSC to property |
| `list_osc_servers` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Lists OSC servers |
| `list_osc_clients` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Lists OSC clients |
| `configure_osc_dispatcher` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures dispatcher |
| `get_osc_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets OSC info |
| **MIDI (15 actions)** | | | |
| `list_midi_devices` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Lists MIDI devices |
| `open_midi_input` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Opens MIDI input |
| `close_midi_input` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Closes MIDI input |
| `open_midi_output` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Opens MIDI output |
| `close_midi_output` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Closes MIDI output |
| `send_midi_note_on` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sends note on |
| `send_midi_note_off` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sends note off |
| `send_midi_cc` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sends CC message |
| `send_midi_pitch_bend` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sends pitch bend |
| `send_midi_program_change` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sends program change |
| `bind_midi_to_property` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Binds MIDI to property |
| `unbind_midi` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Unbinds MIDI |
| `configure_midi_learn` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures MIDI learn |
| `add_midi_device_component` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds MIDI component |
| `get_midi_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets MIDI info |
| **Timecode (18 actions)** | | | |
| `create_timecode_provider` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates timecode provider |
| `set_timecode_provider` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets timecode provider |
| `get_current_timecode` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets current timecode |
| `set_frame_rate` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets frame rate |
| `configure_ltc_timecode` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures LTC |
| `configure_aja_timecode` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures AJA |
| `configure_blackmagic_timecode` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures Blackmagic |
| `configure_system_time_timecode` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures system time |
| `enable_timecode_genlock` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Enables genlock |
| `disable_timecode_genlock` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Disables genlock |
| `set_custom_timestep` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Sets custom timestep |
| `configure_genlock_source` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Configures genlock source |
| `get_timecode_provider_status` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets provider status |
| `synchronize_timecode` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Synchronizes timecode |
| `create_timecode_synchronizer` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Creates synchronizer |
| `add_timecode_source` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Adds timecode source |
| `list_timecode_providers` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Lists providers |
| `get_timecode_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets timecode info |
| **Utility (3 actions)** | | | |
| `get_virtual_production_info` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Gets VP availability |
| `list_active_vp_sessions` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Lists active sessions |
| `reset_vp_state` | `McpAutomationBridge_VirtualProductionHandlers.cpp` | `HandleManageVirtualProductionAction` | Resets VP state |

### Phase 40 Action Summary

| Subsystem | Actions |
|-----------|---------|
| nDisplay Cluster | 10 |
| nDisplay LED/ICVFX | 10 |
| nDisplay Sync | 5 |
| Composure | 12 |
| OpenColorIO | 10 |
| Remote Control | 15 |
| DMX | 20 |
| OSC | 12 |
| MIDI | 15 |
| Timecode | 18 |
| Utility | 3 |
| **Total** | **130** |

### Implementation Notes
- All 130 actions fully implemented in both TypeScript and C++ handlers
- Virtual Production conditional compilation via `__has_include()` checks:
  - `MCP_HAS_NDISPLAY` - nDisplay cluster and ICVFX
  - `MCP_HAS_COMPOSURE` - Composure compositing
  - `MCP_HAS_OCIO` - OpenColorIO color management
  - `MCP_HAS_REMOTE_CONTROL` - Remote Control API
  - `MCP_HAS_DMX` - DMX lighting protocol
  - `MCP_HAS_OSC` - OSC network protocol
  - `MCP_HAS_MIDI` - MIDI device control
  - `MCP_HAS_TIMECODE` - Timecode synchronization
- Graceful fallback messages when plugins not enabled
- Hardware requirements: LED walls, DMX fixtures, timecode generators, MIDI controllers

---

## 56. XR Manager (`manage_xr`) - Phase 41

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **OpenXR Core (20 actions)** | | | |
| `get_openxr_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets OpenXR runtime info |
| `configure_openxr_settings` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures OpenXR settings |
| `set_tracking_origin` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Sets tracking origin |
| `get_tracking_origin` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets tracking origin |
| `create_xr_action_set` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Creates action set |
| `add_xr_action` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Adds XR action |
| `bind_xr_action` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Binds XR action |
| `get_xr_action_state` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets action state |
| `trigger_haptic_feedback` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Triggers haptics |
| `stop_haptic_feedback` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Stops haptics |
| `get_hmd_pose` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets HMD pose |
| `get_controller_pose` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets controller pose |
| `get_hand_tracking_data` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets hand tracking |
| `enable_hand_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables hand tracking |
| `disable_hand_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Disables hand tracking |
| `get_eye_tracking_data` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets eye tracking |
| `enable_eye_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables eye tracking |
| `get_view_configuration` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets view config |
| `set_render_scale` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Sets render scale |
| `get_supported_extensions` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets extensions |
| **Meta Quest (22 actions)** | | | |
| `get_quest_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Quest info |
| `configure_quest_settings` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures Quest |
| `enable_passthrough` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables passthrough |
| `disable_passthrough` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Disables passthrough |
| `configure_passthrough_style` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures passthrough style |
| `enable_scene_capture` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables scene capture |
| `get_scene_anchors` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets scene anchors |
| `get_room_layout` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets room layout |
| `enable_quest_hand_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables Quest hands |
| `get_quest_hand_pose` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Quest hand pose |
| `enable_quest_face_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables Quest face |
| `get_quest_face_state` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Quest face state |
| `enable_quest_eye_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables Quest eyes |
| `get_quest_eye_gaze` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Quest eye gaze |
| `enable_quest_body_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables Quest body |
| `get_quest_body_state` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Quest body state |
| `create_spatial_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Creates spatial anchor |
| `save_spatial_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Saves spatial anchor |
| `load_spatial_anchors` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Loads spatial anchors |
| `delete_spatial_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Deletes spatial anchor |
| `configure_guardian_bounds` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures guardian |
| `get_guardian_geometry` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets guardian geometry |
| **SteamVR (18 actions)** | | | |
| `get_steamvr_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets SteamVR info |
| `configure_steamvr_settings` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures SteamVR |
| `configure_chaperone_bounds` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures chaperone |
| `get_chaperone_geometry` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets chaperone geometry |
| `create_steamvr_overlay` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Creates overlay |
| `set_overlay_texture` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Sets overlay texture |
| `show_overlay` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Shows overlay |
| `hide_overlay` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Hides overlay |
| `destroy_overlay` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Destroys overlay |
| `get_tracked_device_count` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets device count |
| `get_tracked_device_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets device info |
| `get_lighthouse_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets lighthouse info |
| `trigger_steamvr_haptic` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Triggers SteamVR haptic |
| `get_steamvr_action_manifest` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets action manifest |
| `set_steamvr_action_manifest` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Sets action manifest |
| `enable_steamvr_skeletal_input` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables skeletal input |
| `get_skeletal_bone_data` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets skeletal bones |
| `configure_steamvr_render` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures SteamVR render |
| **Apple ARKit (22 actions)** | | | |
| `get_arkit_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets ARKit info |
| `configure_arkit_session` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures ARKit session |
| `start_arkit_session` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Starts ARKit session |
| `pause_arkit_session` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Pauses ARKit session |
| `configure_world_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures world tracking |
| `get_tracked_planes` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets tracked planes |
| `get_tracked_images` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets tracked images |
| `add_reference_image` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Adds reference image |
| `enable_people_occlusion` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables people occlusion |
| `disable_people_occlusion` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Disables people occlusion |
| `enable_arkit_face_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables ARKit face |
| `get_arkit_face_blendshapes` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets face blendshapes |
| `get_arkit_face_geometry` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets face geometry |
| `enable_body_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables body tracking |
| `get_body_skeleton` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets body skeleton |
| `create_arkit_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Creates ARKit anchor |
| `remove_arkit_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Removes ARKit anchor |
| `get_light_estimation` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets light estimation |
| `enable_scene_reconstruction` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables scene reconstruction |
| `get_scene_mesh` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets scene mesh |
| `perform_raycast` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Performs raycast |
| `get_camera_intrinsics` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets camera intrinsics |
| **Google ARCore (18 actions)** | | | |
| `get_arcore_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets ARCore info |
| `configure_arcore_session` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures ARCore session |
| `start_arcore_session` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Starts ARCore session |
| `pause_arcore_session` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Pauses ARCore session |
| `get_arcore_planes` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets ARCore planes |
| `get_arcore_points` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets ARCore points |
| `create_arcore_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Creates ARCore anchor |
| `remove_arcore_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Removes ARCore anchor |
| `enable_depth_api` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables depth API |
| `get_depth_image` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets depth image |
| `enable_geospatial` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables geospatial |
| `get_geospatial_pose` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets geospatial pose |
| `create_geospatial_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Creates geospatial anchor |
| `resolve_cloud_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Resolves cloud anchor |
| `host_cloud_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Hosts cloud anchor |
| `enable_arcore_augmented_images` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables augmented images |
| `get_arcore_light_estimate` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets ARCore light estimate |
| `perform_arcore_raycast` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Performs ARCore raycast |
| **Varjo (16 actions)** | | | |
| `get_varjo_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Varjo info |
| `configure_varjo_settings` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures Varjo |
| `enable_varjo_passthrough` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables Varjo passthrough |
| `disable_varjo_passthrough` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Disables Varjo passthrough |
| `configure_varjo_depth_test` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures Varjo depth test |
| `enable_varjo_eye_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables Varjo eye tracking |
| `get_varjo_gaze_data` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Varjo gaze data |
| `calibrate_varjo_eye_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Calibrates Varjo eye tracking |
| `enable_foveated_rendering` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables foveated rendering |
| `configure_foveated_rendering` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures foveated rendering |
| `enable_varjo_mixed_reality` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables Varjo mixed reality |
| `configure_varjo_chroma_key` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures Varjo chroma key |
| `get_varjo_camera_intrinsics` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Varjo camera intrinsics |
| `enable_varjo_depth_estimation` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables Varjo depth estimation |
| `get_varjo_environment_cubemap` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets Varjo environment cubemap |
| `configure_varjo_markers` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures Varjo markers |
| **HoloLens (20 actions)** | | | |
| `get_hololens_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets HoloLens info |
| `configure_hololens_settings` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures HoloLens |
| `enable_spatial_mapping` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables spatial mapping |
| `disable_spatial_mapping` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Disables spatial mapping |
| `get_spatial_mesh` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets spatial mesh |
| `configure_spatial_mapping_quality` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures spatial mapping quality |
| `enable_scene_understanding` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables scene understanding |
| `get_scene_objects` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets scene objects |
| `enable_qr_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables QR tracking |
| `get_tracked_qr_codes` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets tracked QR codes |
| `create_world_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Creates world anchor |
| `save_world_anchor` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Saves world anchor |
| `load_world_anchors` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Loads world anchors |
| `enable_hololens_hand_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables HoloLens hand tracking |
| `get_hololens_hand_mesh` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets HoloLens hand mesh |
| `enable_hololens_eye_tracking` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Enables HoloLens eye tracking |
| `get_hololens_gaze_ray` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets HoloLens gaze ray |
| `register_voice_command` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Registers voice command |
| `unregister_voice_command` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Unregisters voice command |
| `get_registered_voice_commands` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets registered voice commands |
| **Utilities (6 actions)** | | | |
| `get_xr_system_info` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets XR system info |
| `list_xr_devices` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Lists XR devices |
| `set_xr_device_priority` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Sets XR device priority |
| `reset_xr_orientation` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Resets XR orientation |
| `configure_xr_spectator` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Configures XR spectator |
| `get_xr_runtime_name` | `McpAutomationBridge_XRPluginsHandlers.cpp` | `HandleManageXRAction` | Gets XR runtime name |

### Phase 41 Action Summary

| Platform | Actions |
|----------|---------|
| OpenXR Core | 20 |
| Meta Quest | 22 |
| SteamVR | 18 |
| Apple ARKit | 22 |
| Google ARCore | 18 |
| Varjo | 16 |
| HoloLens | 20 |
| Utilities | 6 |
| **Total** | **142** |

### Implementation Notes
- All 142 actions fully implemented in both TypeScript and C++ handlers
- XR conditional compilation via `__has_include()` checks:
  - `MCP_HAS_HMD` - HeadMountedDisplay core
  - `MCP_HAS_XR_TRACKING` - IXRTrackingSystem
  - `MCP_HAS_OPENXR` - OpenXR runtime
  - `MCP_HAS_OCULUSXR` - Meta Quest platform
  - `MCP_HAS_STEAMVR` - SteamVR platform
  - `MCP_HAS_ARKIT` - Apple ARKit
  - `MCP_HAS_ARCORE` - Google ARCore
  - `MCP_HAS_VARJO` - Varjo headsets
  - `MCP_HAS_HOLOLENS` - Microsoft HoloLens
- Graceful fallback messages when XR plugins not enabled
- Hardware requirements: XR headsets, controllers, AR-capable devices
