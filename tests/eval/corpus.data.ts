// tests/eval/corpus.data.ts
// Golden discovery corpus: each intent names the capability search must answer with.
// version_negative and plugin_negative cases expect no positive match, so the
// retrieval measurement skips them.

export type CapabilityRef = { readonly tool: string; readonly action: string };

export type CorpusCase = {
  readonly id: string;
  readonly kind: string;
  readonly intent: string;
  readonly expected: CapabilityRef;
  readonly allowedAlternatives: readonly CapabilityRef[];
};

export const corpus: readonly CorpusCase[] = [
  { id: 'e.manage_tools', kind: 'exact', intent: 'list every available tool', expected: { tool: 'manage_tools', action: 'list_tools' }, allowedAlternatives: [] },
  { id: 'e.manage_asset', kind: 'exact', intent: 'import an asset from an fbx file into the project', expected: { tool: 'manage_asset', action: 'import' }, allowedAlternatives: [{ tool: 'manage_asset', action: 'duplicate_asset' }] },
  { id: 'e.manage_blueprint', kind: 'exact', intent: 'create a new blueprint class', expected: { tool: 'manage_blueprint', action: 'create' }, allowedAlternatives: [{ tool: 'manage_blueprint', action: 'create_blueprint' }] },
  { id: 'e.control_actor', kind: 'exact', intent: 'spawn an actor from a blueprint at a location', expected: { tool: 'control_actor', action: 'spawn' }, allowedAlternatives: [{ tool: 'control_actor', action: 'spawn_actor' }] },
  { id: 'e.control_editor', kind: 'exact', intent: 'start playing the game in the editor', expected: { tool: 'control_editor', action: 'play' }, allowedAlternatives: [{ tool: 'control_editor', action: 'resume' }] },
  { id: 'e.manage_level', kind: 'exact', intent: 'save the current level to disk', expected: { tool: 'manage_level', action: 'save_level' }, allowedAlternatives: [{ tool: 'manage_level', action: 'save' }] },
  { id: 'e.system_control', kind: 'exact', intent: 'run a python snippet in the editor', expected: { tool: 'system_control', action: 'execute_python' }, allowedAlternatives: [{ tool: 'system_control', action: 'execute_command' }] },
  { id: 'e.inspect', kind: 'exact', intent: 'introspect an object and read its properties', expected: { tool: 'inspect', action: 'inspect_object' }, allowedAlternatives: [{ tool: 'inspect', action: 'get_actor_details' }] },
  { id: 'e.build_environment', kind: 'exact', intent: 'create a new landscape in the world', expected: { tool: 'build_environment', action: 'create_landscape' }, allowedAlternatives: [{ tool: 'build_environment', action: 'sculpt_landscape' }] },
  { id: 'e.manage_level_structure', kind: 'exact', intent: 'create a new sublevel under the current level', expected: { tool: 'manage_level_structure', action: 'create_sublevel' }, allowedAlternatives: [{ tool: 'manage_level_structure', action: 'create_level' }] },
  { id: 'e.manage_geometry', kind: 'exact', intent: 'create a box procedural mesh', expected: { tool: 'manage_geometry', action: 'create_box' }, allowedAlternatives: [{ tool: 'manage_geometry', action: 'create_sphere' }] },
  { id: 'e.manage_pcg', kind: 'exact', intent: 'create a new pcg graph asset', expected: { tool: 'manage_pcg', action: 'create_pcg_graph' }, allowedAlternatives: [{ tool: 'manage_pcg', action: 'create_pcg_subgraph' }] },
  { id: 'e.animation_physics', kind: 'exact', intent: 'create an animation blueprint from a skeleton', expected: { tool: 'animation_physics', action: 'create_animation_blueprint' }, allowedAlternatives: [{ tool: 'animation_physics', action: 'create_anim_blueprint' }] },
  { id: 'e.manage_effect', kind: 'exact', intent: 'spawn a niagara particle system at a location', expected: { tool: 'manage_effect', action: 'spawn_niagara' }, allowedAlternatives: [{ tool: 'manage_effect', action: 'create_niagara_system' }] },
  { id: 'e.manage_gas', kind: 'exact', intent: 'create a new gameplay ability', expected: { tool: 'manage_gas', action: 'create_gameplay_ability' }, allowedAlternatives: [{ tool: 'manage_gas', action: 'create_gameplay_effect' }] },
  { id: 'e.manage_character', kind: 'exact', intent: 'create a character blueprint', expected: { tool: 'manage_character', action: 'create_character_blueprint' }, allowedAlternatives: [{ tool: 'manage_character', action: 'configure_movement_speeds' }] },
  { id: 'e.manage_combat', kind: 'exact', intent: 'create a weapon blueprint', expected: { tool: 'manage_combat', action: 'create_weapon_blueprint' }, allowedAlternatives: [{ tool: 'manage_combat', action: 'create_projectile_blueprint' }] },
  { id: 'e.manage_ai', kind: 'exact', intent: 'create an ai controller', expected: { tool: 'manage_ai', action: 'create_ai_controller' }, allowedAlternatives: [{ tool: 'manage_ai', action: 'create_behavior_tree' }] },
  { id: 'e.manage_inventory', kind: 'exact', intent: 'create an item data asset', expected: { tool: 'manage_inventory', action: 'create_item_data_asset' }, allowedAlternatives: [{ tool: 'manage_inventory', action: 'create_item_category' }] },
  { id: 'e.manage_interaction', kind: 'exact', intent: 'create an interactable interface', expected: { tool: 'manage_interaction', action: 'create_interactable_interface' }, allowedAlternatives: [{ tool: 'manage_interaction', action: 'create_door_actor' }] },
  { id: 'e.manage_audio', kind: 'exact', intent: 'create a sound cue', expected: { tool: 'manage_audio', action: 'create_sound_cue' }, allowedAlternatives: [{ tool: 'manage_audio', action: 'create_metasound' }] },
  { id: 'e.manage_sequence', kind: 'exact', intent: 'create a new sequence', expected: { tool: 'manage_sequence', action: 'create' }, allowedAlternatives: [{ tool: 'manage_sequence', action: 'open' }] },
  { id: 'e.manage_networking', kind: 'exact', intent: 'host a lan server for multiplayer', expected: { tool: 'manage_networking', action: 'host_lan_server' }, allowedAlternatives: [{ tool: 'manage_networking', action: 'join_lan_server' }] },
  { id: 'p.build_environment.hc', kind: 'high_cardinality', intent: 'create a landscape and sculpt the terrain', expected: { tool: 'build_environment', action: 'create_landscape' }, allowedAlternatives: [{ tool: 'build_environment', action: 'sculpt_landscape' }] },
  { id: 'p.build_environment.water', kind: 'collision', intent: 'create an ocean water body', expected: { tool: 'build_environment', action: 'create_water_body_ocean' }, allowedAlternatives: [] },
  { id: 'p.manage_asset.del', kind: 'destructive', intent: 'delete this imported asset permanently', expected: { tool: 'manage_asset', action: 'delete_asset' }, allowedAlternatives: [{ tool: 'manage_asset', action: 'delete' }] },
  { id: 'p.manage_asset.search', kind: 'high_cardinality', intent: 'search for all material assets', expected: { tool: 'manage_asset', action: 'search_assets' }, allowedAlternatives: [{ tool: 'manage_asset', action: 'list' }] },
  { id: 'p.manage_blueprint.widget', kind: 'ambiguous', intent: 'create a widget', expected: { tool: 'manage_blueprint', action: 'create_widget_blueprint' }, allowedAlternatives: [{ tool: 'system_control', action: 'create_widget' }] },
  { id: 'p.manage_blueprint.scs', kind: 'collision', intent: 'add an scs component to a blueprint', expected: { tool: 'manage_blueprint', action: 'add_scs_component' }, allowedAlternatives: [{ tool: 'manage_blueprint', action: 'add_component' }] },
  { id: 'p.manage_sequence.render', kind: 'collision', intent: 'render this cinematic with movie render queue', expected: { tool: 'manage_sequence', action: 'start_render' }, allowedAlternatives: [{ tool: 'manage_sequence', action: 'queue_render' }] },
  { id: 'd.control_actor', kind: 'destructive', intent: 'delete the spawned actor from the level', expected: { tool: 'control_actor', action: 'delete' }, allowedAlternatives: [{ tool: 'control_actor', action: 'destroy_actor' }] },
  { id: 'd.manage_level', kind: 'destructive', intent: 'delete the sublevel', expected: { tool: 'manage_level', action: 'delete_level' }, allowedAlternatives: [{ tool: 'manage_level', action: 'delete' }] },
  { id: 'hc.setproperty', kind: 'high_cardinality', intent: 'set the property value on the component', expected: { tool: 'control_actor', action: 'set_component_property' }, allowedAlternatives: [{ tool: 'inspect', action: 'set_component_property' }] },
  { id: 'a.control_actor.findclass', kind: 'ambiguous', intent: 'find all actors of a class', expected: { tool: 'control_actor', action: 'find_by_class' }, allowedAlternatives: [{ tool: 'inspect', action: 'find_by_class' }] },
  { id: 'v.pcg.ue51', kind: 'version_negative', intent: 'create a pcg graph in ue 5.1 which is not installed', expected: { tool: 'manage_pcg', action: 'create_pcg_graph' }, allowedAlternatives: [] },
  { id: 'pl.audio.metasound', kind: 'plugin_negative', intent: 'create a metasound but the audio authoring plugin is disabled', expected: { tool: 'manage_audio', action: 'create_metasound' }, allowedAlternatives: [] },
  { id: 'c.C7', kind: 'collision', intent: 'host a lan server for networking', expected: { tool: 'manage_networking', action: 'host_lan_server' }, allowedAlternatives: [] },
  { id: 'c.C8', kind: 'collision', intent: 'set the exposure', expected: { tool: 'build_environment', action: 'set_exposure' }, allowedAlternatives: [{ tool: 'build_environment', action: 'configure_exposure' }] },
  { id: 'c.C9', kind: 'collision', intent: 'set the volume bounds for a volume', expected: { tool: 'manage_level_structure', action: 'set_volume_extent' }, allowedAlternatives: [{ tool: 'manage_level_structure', action: 'set_volume_bounds' }] },
  { id: 'c.C10', kind: 'collision', intent: 'find assets by tag', expected: { tool: 'inspect', action: 'find_by_tag' }, allowedAlternatives: [{ tool: 'manage_asset', action: 'find_by_tag' }] },
  { id: 'c.C11', kind: 'collision', intent: 'run a console command', expected: { tool: 'system_control', action: 'console_command' }, allowedAlternatives: [{ tool: 'control_editor', action: 'console_command' }] },
  { id: 'c.C13', kind: 'collision', intent: 'set up a ragdoll on the physics asset', expected: { tool: 'animation_physics', action: 'setup_ragdoll' }, allowedAlternatives: [{ tool: 'animation_physics', action: 'activate_ragdoll' }] },
  { id: 'c.C17', kind: 'collision', intent: 'analyze the material graph', expected: { tool: 'manage_asset', action: 'analyze_graph' }, allowedAlternatives: [] },
  { id: 'c.C18', kind: 'collision', intent: 'spawn a niagara system', expected: { tool: 'manage_effect', action: 'spawn_niagara' }, allowedAlternatives: [{ tool: 'manage_effect', action: 'activate_effect' }] },
  { id: 'c.C19', kind: 'collision', intent: 'set the gameplay ability activation policy', expected: { tool: 'manage_gas', action: 'set_activation_policy' }, allowedAlternatives: [] },
  { id: 'c.C20', kind: 'collision', intent: 'apply a style to a widget', expected: { tool: 'manage_blueprint', action: 'set_style' }, allowedAlternatives: [] },
  { id: 'c.C22', kind: 'collision', intent: 'preview the physics of a skeleton', expected: { tool: 'animation_physics', action: 'configure_physics_body' }, allowedAlternatives: [] },
  { id: 'c.C25', kind: 'collision', intent: 'subtract one mesh from another', expected: { tool: 'manage_geometry', action: 'boolean_subtract' }, allowedAlternatives: [{ tool: 'manage_geometry', action: 'boolean_union' }] },
  { id: 'c.C26', kind: 'collision', intent: 'loft a profile into a mesh', expected: { tool: 'manage_geometry', action: 'loft' }, allowedAlternatives: [{ tool: 'manage_geometry', action: 'bridge' }] },
  { id: 'c.C27', kind: 'collision', intent: 'execute the pcg graph', expected: { tool: 'manage_pcg', action: 'execute_pcg_graph' }, allowedAlternatives: [] },
  { id: 'c.C30', kind: 'collision', intent: 'create a nav modifier component', expected: { tool: 'manage_ai', action: 'create_nav_modifier_component' }, allowedAlternatives: [{ tool: 'manage_ai', action: 'create_nav_link_proxy' }] },
  { id: 'c.C32', kind: 'collision', intent: 'show the current tool status', expected: { tool: 'manage_tools', action: 'get_status' }, allowedAlternatives: [] },
  { id: 'c.C33', kind: 'collision', intent: 'find all objects of a class', expected: { tool: 'inspect', action: 'find_by_class' }, allowedAlternatives: [{ tool: 'control_actor', action: 'find_by_class' }] },
  { id: 'c.C34', kind: 'collision', intent: 'spawn an actor and reuse it for inspect and animation', expected: { tool: 'control_actor', action: 'spawn' }, allowedAlternatives: [] },
  { id: 'c.C40', kind: 'collision', intent: 'build lighting for the level', expected: { tool: 'build_environment', action: 'build_lighting' }, allowedAlternatives: [{ tool: 'build_environment', action: 'bake_lightmap' }] },
  { id: 'c.C41', kind: 'collision', intent: 'take a screenshot of the viewport', expected: { tool: 'control_editor', action: 'screenshot' }, allowedAlternatives: [{ tool: 'control_editor', action: 'take_screenshot' }] },
  { id: 'plain.mesh_size', kind: 'exact', intent: 'how big is a mesh', expected: { tool: 'inspect', action: 'inspect_object' }, allowedAlternatives: [{ tool: 'inspect', action: 'get_mesh_details' }] },
  { id: 'plain.scatter_grass', kind: 'exact', intent: 'scatter grass along the ground', expected: { tool: 'build_environment', action: 'add_foliage' }, allowedAlternatives: [{ tool: 'build_environment', action: 'paint_foliage' }, { tool: 'build_environment', action: 'configure_spline_meshes' }] },
  { id: 'plain.click_game_button', kind: 'exact', intent: 'click a button in the running game', expected: { tool: 'control_editor', action: 'simulate_input' }, allowedAlternatives: [] },
  { id: 'plain.actor_positions', kind: 'exact', intent: 'actor positions in the level', expected: { tool: 'control_actor', action: 'list' }, allowedAlternatives: [{ tool: 'control_actor', action: 'get_transform' }] },
  { id: 'plain.fab_download', kind: 'exact', intent: 'download free asset from fab', expected: { tool: 'manage_asset', action: 'import_marketplace_asset' }, allowedAlternatives: [{ tool: 'manage_asset', action: 'query_marketplace' }] },
  { id: 'plain.metasound_wav', kind: 'exact', intent: 'play a wav file in a metasound', expected: { tool: 'manage_audio', action: 'edit_metasound' }, allowedAlternatives: [] },
  // Button edits: add_content_widget's member names (add_button, add_text_block) took every one.
  { id: 'plain.button_text', kind: 'collision', intent: 'change button text', expected: { tool: 'manage_blueprint', action: 'set_style' }, allowedAlternatives: [] },
  { id: 'plain.copy_button', kind: 'collision', intent: 'copy button', expected: { tool: 'manage_blueprint', action: 'duplicate_widget' }, allowedAlternatives: [] },
  { id: 'plain.delete_button', kind: 'exact', intent: 'delete the quit button', expected: { tool: 'manage_blueprint', action: 'remove_widget' }, allowedAlternatives: [] },
  { id: 'plain.move_button', kind: 'exact', intent: 'move the button', expected: { tool: 'manage_blueprint', action: 'set_position' }, allowedAlternatives: [] },
  // Verification a small model asks for in its own words: a long call's result, feet that slide, test results, validators.
  { id: 'plain.task_result', kind: 'exact', intent: 'read the result of a call that is still running', expected: { tool: 'manage_tools', action: 'get_task_result' }, allowedAlternatives: [] },
  { id: 'plain.feet_slide', kind: 'exact', intent: 'do the feet slide', expected: { tool: 'control_actor', action: 'sample_motion' }, allowedAlternatives: [] },
  { id: 'plain.bone_over_time', kind: 'exact', intent: 'track a bone position over time', expected: { tool: 'control_actor', action: 'sample_motion' }, allowedAlternatives: [] },
  { id: 'plain.test_results', kind: 'exact', intent: 'run the automation tests and read the results', expected: { tool: 'system_control', action: 'run_tests' }, allowedAlternatives: [] },
  { id: 'plain.data_validation', kind: 'exact', intent: 'check that assets pass data validation', expected: { tool: 'system_control', action: 'validate_assets' }, allowedAlternatives: [] },
  { id: 'plain.mesh_watertight', kind: 'exact', intent: 'is the mesh watertight', expected: { tool: 'inspect', action: 'check_mesh' }, allowedAlternatives: [] },
  { id: 'plain.camera_sees', kind: 'exact', intent: 'what can the camera see', expected: { tool: 'inspect', action: 'describe_view' }, allowedAlternatives: [] },
  { id: 'plain.export_gltf', kind: 'exact', intent: 'export the mesh to gltf', expected: { tool: 'manage_asset', action: 'export_mesh' }, allowedAlternatives: [] },
  { id: 'plain.depth_pass', kind: 'exact', intent: 'write a depth map of the view', expected: { tool: 'inspect', action: 'capture_passes' }, allowedAlternatives: [] },
  { id: 'plain.control_rig_node', kind: 'exact', intent: 'add a node to the control rig', expected: { tool: 'animation_physics', action: 'edit_control_rig' }, allowedAlternatives: [] },
  { id: 'plain.anim_ik_node', kind: 'exact', intent: 'add a two bone ik node to the animation blueprint', expected: { tool: 'manage_blueprint', action: 'create_node' }, allowedAlternatives: [{ tool: 'manage_blueprint', action: 'add_node' }] },
  { id: 'plain.foot_contacts', kind: 'exact', intent: 'when do the feet touch the ground in this run animation', expected: { tool: 'animation_physics', action: 'analyze_animation' }, allowedAlternatives: [] },
  { id: 'n.near_tie_del', kind: 'near_tie_destructive', intent: 'delete the selected object', expected: { tool: 'control_actor', action: 'delete' }, allowedAlternatives: [{ tool: 'manage_level', action: 'delete_level' }, { tool: 'manage_asset', action: 'delete_asset' }] },
];
