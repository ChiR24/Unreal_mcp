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