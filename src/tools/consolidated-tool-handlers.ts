import { cleanObject } from '../utils/safe-json.js';
import { Logger } from '../utils/logger.js';
import { ResponseFactory } from '../utils/response-factory.js';
import { ITools } from '../types/tool-interfaces.js';
import { toolRegistry } from './dynamic-handler-registry.js';
import { executeAutomationRequest, requireAction } from './handlers/common-handlers.js';
import { handleAssetTools } from './handlers/asset-handlers.js';
import { handleActorTools } from './handlers/actor-handlers.js';
import { handleEditorTools } from './handlers/editor-handlers.js';
import { handleLevelTools } from './handlers/level-handlers.js';
import { handleBlueprintTools, handleBlueprintGet } from './handlers/blueprint-handlers.js';
import { handleSequenceTools } from './handlers/sequence-handlers.js';
import { handleAnimationTools } from './handlers/animation-handlers.js';
import { handleEffectTools } from './handlers/effect-handlers.js';
import { handleEnvironmentTools } from './handlers/environment-handlers.js';
import { handleSystemTools, handleConsoleCommand } from './handlers/system-handlers.js';
import { handleInspectTools } from './handlers/inspect-handlers.js';
import { handlePipelineTools } from './handlers/pipeline-handlers.js';
import { handleGraphTools } from './handlers/graph-handlers.js';
import { handleAudioTools } from './handlers/audio-handlers.js';
import { handleLightingTools } from './handlers/lighting-handlers.js';
import { handlePerformanceTools } from './handlers/performance-handlers.js';
import { handleInputTools } from './handlers/input-handlers.js';
import { handleGeometryTools } from './handlers/geometry-handlers.js';
import { handleSkeletonTools } from './handlers/skeleton-handlers.js';
import { handleMaterialAuthoringTools } from './handlers/material-authoring-handlers.js';
import { handleTextureTools } from './handlers/texture-handlers.js';
import { handleAnimationAuthoringTools } from './handlers/animation-authoring-handlers.js';
import { handleAudioAuthoringTools } from './handlers/audio-authoring-handlers.js';
import { handleNiagaraAuthoringTools } from './handlers/niagara-authoring-handlers.js';
import { handleGASTools } from './handlers/gas-handlers.js';
import { handleCharacterTools } from './handlers/character-handlers.js';
import { handleCombatTools } from './handlers/combat-handlers.js';
import { handleAITools } from './handlers/ai-handlers.js';
import { handleInventoryTools } from './handlers/inventory-handlers.js';
import { handleInteractionTools } from './handlers/interaction-handlers.js';
import { handleWidgetAuthoringTools } from './handlers/widget-authoring-handlers.js';
import { handleNetworkingTools } from './handlers/networking-handlers.js';
import { handleGameFrameworkTools } from './handlers/game-framework-handlers.js';
import { handleSessionsTools } from './handlers/sessions-handlers.js';
import { handleLevelStructureTools } from './handlers/level-structure-handlers.js';
import { handleVolumeTools } from './handlers/volume-handlers.js';
import { handleNavigationTools } from './handlers/navigation-handlers.js';
import { handleSplineTools } from './handlers/spline-handlers.js';
import { handlePCGTools } from './handlers/pcg-handlers.js';
import { handlePostProcessTools } from './handlers/post-process-handlers.js';
import { handleSequencerTools } from './handlers/sequencer-handlers.js';
import { handleMovieRenderTools } from './handlers/movie-render-handlers.js';
import { handleMediaTools } from './handlers/media-handlers.js';
import { handleDataTools } from './handlers/data-handlers.js';
import { handleBuildTools } from './handlers/build-handlers.js';
import { handleTestingTools } from './handlers/testing-handlers.js';
import { handleEditorUtilitiesTools } from './handlers/editor-utilities-handlers.js';
import { handleGameplaySystemsTools } from './handlers/gameplay-systems-handlers.js';
import { handleCharacterAvatarTools } from './handlers/character-avatar-handlers.js';
import { handleAssetPluginsTools } from './handlers/asset-plugins-handlers.js';
import { handleAudioMiddlewareTools } from './handlers/audio-middleware-handlers.js';
import { handleLiveLinkTools } from './handlers/livelink-handlers.js';
import { handleVirtualProductionTools } from './handlers/virtual-production-handlers.js';
import { handleXRPluginsTools } from './handlers/xr-plugins-handlers.js';
import { handleAINPCTools } from './handlers/ai-npc-handlers.js';
import { handleUtilityPluginsTools } from './handlers/utility-plugins-handlers.js';
import { handlePhysicsDestructionTools } from './handlers/physics-destruction-handlers.js';
import { handleAccessibilityTools } from './handlers/accessibility-handlers.js';
import { handleModdingTools } from './handlers/modding-handlers.js';
// import { getDynamicHandlerForTool } from './dynamic-handler-registry.js';
// import { consolidatedToolDefinitions } from './consolidated-tool-definitions.js';

type NormalizedToolCall = {
  name: string;
  action: string;
  args: Record<string, unknown>;
};

// Interface for deprecation warning flags (stored in globalThis for once-per-session warnings)
interface DeprecationFlags {
  __blueprintGraphDeprecationLogged?: boolean;
  __audioAuthoringDeprecationLogged?: boolean;
  __niagaraAuthoringDeprecationLogged?: boolean;
  __animationAuthoringDeprecationLogged?: boolean;
  __sequencerDeprecationLogged?: boolean;
  __movieRenderDeprecationLogged?: boolean;
  __levelStructureDeprecationLogged?: boolean;
  __waterDeprecationLogged?: boolean;
  __weatherDeprecationLogged?: boolean;
}

const MATERIAL_GRAPH_ACTION_MAP: Record<string, string> = {
  add_material_node: 'add_node',
  connect_material_pins: 'connect_pins',
  remove_material_node: 'remove_node',
  break_material_connections: 'break_connections',
  get_material_node_details: 'get_node_details',
};

const BEHAVIOR_TREE_ACTION_MAP: Record<string, string> = {
  add_bt_node: 'add_node',
  connect_bt_nodes: 'connect_nodes',
  remove_bt_node: 'remove_node',
  break_bt_connections: 'break_connections',
  set_bt_node_properties: 'set_node_properties'
};

const NIAGARA_GRAPH_ACTION_MAP: Record<string, string> = {
  add_niagara_module: 'add_module',
  connect_niagara_pins: 'connect_pins',
  remove_niagara_node: 'remove_node',
  set_niagara_parameter: 'set_parameter'
};

function isMaterialGraphAction(action: string): boolean {
  return (
    Object.prototype.hasOwnProperty.call(MATERIAL_GRAPH_ACTION_MAP, action) ||
    action.includes('material_node') ||
    action.includes('material_pins') ||
    action.includes('material_connections')
  );
}

function isBehaviorTreeGraphAction(action: string): boolean {
  return (
    Object.prototype.hasOwnProperty.call(BEHAVIOR_TREE_ACTION_MAP, action) ||
    action.includes('_bt_') ||
    action.includes('behavior_tree')
  );
}

function isNiagaraGraphAction(action: string): boolean {
  return (
    Object.prototype.hasOwnProperty.call(NIAGARA_GRAPH_ACTION_MAP, action) ||
    action.includes('niagara_module') ||
    action.includes('niagara_pins') ||
    action.includes('niagara_node') ||
    action.includes('niagara_parameter')
  );
}

function normalizeToolCall(
  name: string,
  args: Record<string, unknown>
): NormalizedToolCall {
  let normalizedName = name;
  let action: string;

  if (args && typeof args.action === 'string') {
    action = args.action;
  } else if (normalizedName === 'console_command') {
    normalizedName = 'system_control';
    action = 'console_command';
  } else {
    action = requireAction(args);
  }

  if (normalizedName === 'create_effect') normalizedName = 'manage_effect';
  if (normalizedName === 'console_command') {
    normalizedName = 'system_control';
    action = 'console_command';
  }
  // manage_pipeline is a standalone tool - do NOT reroute to system_control
  // It has its own actions: list_categories, set_categories, get_status
  if (normalizedName === 'manage_tests') {
    normalizedName = 'system_control';
    action = 'run_tests';
  }

  return {
    name: normalizedName,
    action,
    args
  };
}

// Registration of default handlers
function registerDefaultHandlers() {
  // Helper to extract action string from args
  const getAction = (args: Record<string, unknown>): string => {
    const action = args.action ?? args.subAction;
    return typeof action === 'string' ? action : requireAction(args);
  };

  // 1. ASSET MANAGER
  toolRegistry.register('manage_asset', async (args, tools) => {
    const action = getAction(args);
    // Reroute merged functionality
    if (['create_render_target', 'nanite_rebuild_mesh'].includes(action)) {
      const payload = { ...args, subAction: action };
      return cleanObject(await executeAutomationRequest(tools, 'manage_render', payload, `Automation bridge not available for ${action}`));
    }
    if (isMaterialGraphAction(action)) {
      const subAction = MATERIAL_GRAPH_ACTION_MAP[action] || action;
      return await handleGraphTools('manage_material_graph', subAction, args, tools);
    }
    if (isBehaviorTreeGraphAction(action)) {
      const subAction = BEHAVIOR_TREE_ACTION_MAP[action] || action;
      return await handleGraphTools('manage_behavior_tree', subAction, args, tools);
    }
    return await handleAssetTools(action, args, tools);
  });

  // 2. BLUEPRINT MANAGER (includes merged manage_blueprint_graph - Phase 53)
  toolRegistry.register('manage_blueprint', async (args, tools) => {
    const action = getAction(args);
    if (action === 'get_blueprint') {
      return await handleBlueprintGet(args, tools);
    }
    // Graph actions (merged from manage_blueprint_graph)
    const graphActions = ['create_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details', 'list_node_types', 'set_pin_default_value'];
    if (graphActions.includes(action)) {
      return await handleGraphTools('manage_blueprint_graph', action, args, tools);
    }
    return await handleBlueprintTools(action, args, tools);
  });

  // DEPRECATED: manage_blueprint_graph now merged into manage_blueprint (Phase 53)
  // Kept for backward compatibility - routes to manage_blueprint
  toolRegistry.register('manage_blueprint_graph', async (args, tools) => {
    // Deprecation warning logged once per session
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__blueprintGraphDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_blueprint_graph is deprecated and merged into manage_blueprint. Use manage_blueprint instead.');
      globalObj.__blueprintGraphDeprecationLogged = true;
    }
    return await handleGraphTools('manage_blueprint_graph', getAction(args), args, tools);
  });

  // 3. ACTOR CONTROL
  toolRegistry.register('control_actor', async (args, tools) => {
    return await handleActorTools(getAction(args), args, tools);
  });

  // 4. EDITOR CONTROL
  toolRegistry.register('control_editor', async (args, tools) => {
    const action = getAction(args);
    if (action === 'simulate_input') {
      const payload = { ...args, subAction: action };
      return cleanObject(await executeAutomationRequest(tools, 'manage_ui', payload, 'Automation bridge not available'));
    }
    return await handleEditorTools(action, args, tools);
  });

  // 5. LEVEL MANAGER (consolidated: includes manage_level_structure - Phase 54)
  const LEVEL_STRUCTURE_ACTIONS = new Set([
    'create_sublevel', 'configure_level_streaming', 'set_streaming_distance', 'configure_level_bounds',
    'enable_world_partition', 'configure_grid_size', 'create_data_layer', 'assign_actor_to_data_layer',
    'configure_hlod_layer', 'create_minimap_volume', 'open_level_blueprint', 'add_level_blueprint_node',
    'connect_level_blueprint_nodes', 'create_level_instance', 'create_packed_level_actor', 'get_level_structure_info'
  ]);
  toolRegistry.register('manage_level', async (args, tools) => {
    const action = getAction(args);
    // Route World Partition actions
    if (['load_cells', 'set_datalayer'].includes(action)) {
      const payload = { ...args, subAction: action };
      return cleanObject(await executeAutomationRequest(tools, 'manage_world_partition', payload, 'Automation bridge not available'));
    }
    // Route level structure actions (merged from manage_level_structure - Phase 54)
    if (LEVEL_STRUCTURE_ACTIONS.has(action)) {
      return await handleLevelStructureTools(action, args, tools);
    }
    return await handleLevelTools(action, args, tools);
  });

  // 6. ANIMATION & PHYSICS (merged with manage_animation_authoring - Phase 53)
  const ANIMATION_AUTHORING_ACTIONS = new Set([
    'create_animation_sequence', 'set_sequence_length', 'add_bone_track', 'set_bone_key', 'set_curve_key',
    'add_notify_state', 'add_sync_marker', 'set_root_motion_settings', 'set_additive_settings',
    'create_montage', 'add_montage_section', 'add_montage_slot', 'set_section_timing',
    'add_montage_notify', 'set_blend_in', 'set_blend_out', 'link_sections',
    'create_blend_space_1d', 'create_blend_space_2d', 'add_blend_sample', 'set_axis_settings', 'set_interpolation_settings',
    'create_aim_offset', 'add_aim_offset_sample',
    'create_anim_blueprint', 'add_state_machine', 'add_state', 'add_transition', 'set_transition_rules',
    'add_blend_node', 'add_cached_pose', 'add_slot_node', 'add_layered_blend_per_bone', 'set_anim_graph_node_value',
    'create_control_rig', 'add_control', 'add_rig_unit', 'connect_rig_elements', 'create_pose_library',
    'create_ik_rig', 'add_ik_chain', 'create_ik_retargeter', 'set_retarget_chain_mapping',
    'get_animation_info'
  ]);
  toolRegistry.register('animation_physics', async (args, tools) => {
    const action = getAction(args);
    // Route authoring-specific actions to the authoring handler
    if (ANIMATION_AUTHORING_ACTIONS.has(action)) {
      return await handleAnimationAuthoringTools(action, args, tools);
    }
    // Handle add_notify conflict resolution:
    // - frame-based (args.frame) or asset-based (args.assetPath) = authoring handler (animation sequence notifies)
    // - time-based (args.time) or montage-based (args.montagePath) = runtime handler (montage notifies)
    // - If neither, default to runtime handler which will return appropriate error if invalid
    if (action === 'add_notify' && (args.frame !== undefined || args.assetPath !== undefined)) {
      return await handleAnimationAuthoringTools(action, args, tools);
    }
    return await handleAnimationTools(action, args, tools);
  });

  // 7. EFFECTS MANAGER (merged with manage_niagara_authoring - Phase 53)
  const NIAGARA_AUTHORING_ACTIONS = new Set([
    'add_emitter_to_system', 'set_emitter_properties',
    'add_spawn_rate_module', 'add_spawn_burst_module', 'add_spawn_per_unit_module',
    'add_initialize_particle_module', 'add_particle_state_module',
    'add_force_module', 'add_velocity_module', 'add_acceleration_module',
    'add_size_module', 'add_color_module',
    'add_sprite_renderer_module', 'add_mesh_renderer_module', 'add_ribbon_renderer_module', 'add_light_renderer_module',
    'add_collision_module', 'add_kill_particles_module', 'add_camera_offset_module',
    'add_user_parameter', 'set_parameter_value', 'bind_parameter_to_source',
    'add_skeletal_mesh_data_interface', 'add_static_mesh_data_interface', 'add_spline_data_interface',
    'add_audio_spectrum_data_interface', 'add_collision_query_data_interface',
    'add_event_generator', 'add_event_receiver', 'configure_event_payload',
    'enable_gpu_simulation', 'add_simulation_stage',
    'get_niagara_info', 'validate_niagara_system'
  ]);
  toolRegistry.register('manage_effect', async (args, tools) => {
    const action = getAction(args);
    // Route authoring-specific actions to the authoring handler
    if (NIAGARA_AUTHORING_ACTIONS.has(action)) {
      return await handleNiagaraAuthoringTools(action, args, tools);
    }
    if (isNiagaraGraphAction(action)) {
      // Instance check
      const isInstanceOp = action === 'set_niagara_parameter' && (args.actorName || (args.systemName && !args.assetPath && !args.systemPath));
      if (isInstanceOp) {
        return await handleEffectTools(action, args, tools);
      }
      const subAction = NIAGARA_GRAPH_ACTION_MAP[action] || action;
      return await handleGraphTools('manage_niagara_graph', subAction, args, tools);
    }
    return await handleEffectTools(action, args, tools);
  });

  // 8. ENVIRONMENT BUILDER (consolidated: includes manage_water + manage_weather - Phase 54)
  // Water actions merged from manage_water (Phase 54) - kept for documentation
  const _WATER_ACTIONS = new Set([
    'create_water_body_ocean', 'create_water_body_lake', 'create_water_body_river',
    'configure_water_body', 'configure_water_waves', 'get_water_body_info', 'list_water_bodies',
    'set_river_depth', 'set_ocean_extent', 'set_water_static_mesh', 'set_river_transitions',
    'set_water_zone', 'get_water_surface_info', 'get_wave_info'
  ]);
  // Weather actions merged from manage_weather (Phase 54) - kept for documentation
  const _WEATHER_ACTIONS = new Set([
    'configure_wind', 'create_weather_system',
    'configure_rain_particles', 'configure_snow_particles', 'configure_lightning'
  ]);
  // Suppress unused variable warnings (these are documentation-only)
  void _WATER_ACTIONS; void _WEATHER_ACTIONS;
  // All water/weather/environment actions route to the same handler.
  // Action sets (WATER_ACTIONS, WEATHER_ACTIONS) defined above for documentation purposes.
  toolRegistry.register('build_environment', async (args, tools) => {
    return await handleEnvironmentTools(getAction(args), args, tools);
  });

  // 9. SYSTEM CONTROL
  toolRegistry.register('system_control', async (args, tools) => {
    const action = getAction(args);
    if (action === 'console_command') return await handleConsoleCommand(args, tools);
    if (action === 'run_ubt') return await handlePipelineTools(action, args, tools);

    if (action === 'run_tests') return cleanObject(await executeAutomationRequest(tools, 'manage_tests', { ...args, subAction: action }, 'Bridge unavailable'));
    if (action === 'subscribe' || action === 'unsubscribe') return cleanObject(await executeAutomationRequest(tools, 'manage_logs', { ...args, subAction: action }, 'Bridge unavailable'));
    if (action === 'spawn_category') return cleanObject(await executeAutomationRequest(tools, 'manage_debug', { ...args, subAction: action }, 'Bridge unavailable'));
    if (action === 'start_session') return cleanObject(await executeAutomationRequest(tools, 'manage_insights', { ...args, subAction: action }, 'Bridge unavailable'));
    if (action === 'lumen_update_scene') return cleanObject(await executeAutomationRequest(tools, 'manage_render', { ...args, subAction: action }, 'Bridge unavailable'));

    return await handleSystemTools(action, args, tools);
  });

// 10. SEQUENCER (consolidated: includes manage_sequencer + manage_movie_render - Phase 54)
  const MOVIE_RENDER_ACTIONS = new Set([
    'create_queue', 'add_job', 'remove_job', 'clear_queue', 'get_queue', 'configure_job', 'set_sequence', 'set_map',
    'configure_output', 'set_resolution', 'set_frame_rate', 'set_output_directory', 'set_file_name_format',
    'add_render_pass', 'remove_render_pass', 'get_render_passes', 'configure_render_pass',
    'configure_anti_aliasing', 'set_spatial_sample_count', 'set_temporal_sample_count',
    'add_burn_in', 'remove_burn_in', 'configure_burn_in',
    'start_render', 'stop_render', 'get_render_status', 'get_render_progress',
    'add_console_variable', 'remove_console_variable', 'configure_high_res_settings', 'set_tile_count'
  ]);
  const SEQUENCER_ACTIONS = new Set([
    'create_master_sequence', 'add_subsequence', 'remove_subsequence', 'get_subsequences',
    'add_shot_track', 'add_shot', 'remove_shot', 'get_shots',
    'create_cine_camera_actor', 'configure_camera_settings', 'add_camera_cut_track', 'add_camera_cut',
    'bind_actor', 'unbind_actor', 'remove_section', 'get_tracks',
    'remove_keyframe', 'get_keyframes', 'set_playback_range', 'get_playback_range', 'get_sequence_info',
    'play_sequence', 'pause_sequence', 'stop_sequence', 'scrub_to_time',
    'list_sequences', 'duplicate_sequence', 'delete_sequence', 'export_sequence'
  ]);
  toolRegistry.register('manage_sequence', async (args, tools) => {
    const action = getAction(args);
    // Route MRQ actions to movie render handler
    if (MOVIE_RENDER_ACTIONS.has(action)) {
      return await handleMovieRenderTools(action, args, tools);
    }
    // Route advanced sequencer actions to sequencer handler
    if (SEQUENCER_ACTIONS.has(action)) {
      return await handleSequencerTools(action, args, tools);
    }
    // Default: core sequence operations
    return await handleSequenceTools(action, args, tools);
  });

  // 11. INTROSPECTION
  toolRegistry.register('inspect', async (args, tools) => await handleInspectTools(getAction(args), args, tools));

  // 12. AUDIO (merged with manage_audio_authoring - Phase 53)
  const AUDIO_AUTHORING_ACTIONS = new Set([
    'add_cue_node', 'connect_cue_nodes', 'set_cue_attenuation', 'set_cue_concurrency',
    'create_metasound', 'add_metasound_node', 'connect_metasound_nodes',
    'add_metasound_input', 'add_metasound_output', 'set_metasound_default',
    'set_class_properties', 'set_class_parent', 'add_mix_modifier', 'configure_mix_eq',
    'create_attenuation_settings', 'configure_distance_attenuation',
    'configure_spatialization', 'configure_occlusion', 'configure_reverb_send',
    'create_dialogue_voice', 'create_dialogue_wave', 'set_dialogue_context',
    'create_reverb_effect', 'create_source_effect_chain', 'add_source_effect', 'create_submix_effect',
    'get_audio_info'
  ]);
  toolRegistry.register('manage_audio', async (args, tools) => {
    const action = getAction(args);
    // Route authoring-specific actions to the authoring handler
    if (AUDIO_AUTHORING_ACTIONS.has(action)) {
      return await handleAudioAuthoringTools(action, args, tools);
    }
    return await handleAudioTools(action, args, tools);
  });

  // 13. BEHAVIOR TREE
  toolRegistry.register('manage_behavior_tree', async (args, tools) => await handleGraphTools('manage_behavior_tree', getAction(args), args, tools));

  // 14. [REMOVED] manage_blueprint_graph - now merged into manage_blueprint (Phase 53)
  // See registration after manage_blueprint for backward compatibility alias

  // 15. RENDER TOOLS
  toolRegistry.register('manage_render', async (args, tools) => {
    const action = getAction(args);
    return cleanObject(await executeAutomationRequest(tools, 'manage_render', { ...args, subAction: action }, 'Bridge unavailable'));
  });

  // 16. WORLD PARTITION
  toolRegistry.register('manage_world_partition', async (args, tools) => {
    const action = getAction(args);
    return cleanObject(await executeAutomationRequest(tools, 'manage_world_partition', { ...args, subAction: action }, 'Bridge unavailable'));
  });

  // 17. LIGHTING
  toolRegistry.register('manage_lighting', async (args, tools) => await handleLightingTools(getAction(args), args, tools));

  // 18. PERFORMANCE
  toolRegistry.register('manage_performance', async (args, tools) => await handlePerformanceTools(getAction(args), args, tools));

  // 19. INPUT
  toolRegistry.register('manage_input', async (args, tools) => await handleInputTools(getAction(args), args, tools));

  // 20. GEOMETRY SCRIPT (Phase 6)
  toolRegistry.register('manage_geometry', async (args, tools) => await handleGeometryTools(getAction(args), args, tools));

  // 21. SKELETON MANAGER (Phase 7)
  toolRegistry.register('manage_skeleton', async (args, tools) => await handleSkeletonTools(getAction(args), args, tools));

  // 22. MATERIAL AUTHORING (Phase 8)
  toolRegistry.register('manage_material_authoring', async (args, tools) => await handleMaterialAuthoringTools(getAction(args), args, tools));

  // 23. TEXTURE MANAGEMENT (Phase 9)
  toolRegistry.register('manage_texture', async (args, tools) => await handleTextureTools(getAction(args), args, tools));

  // 24. [DEPRECATED] ANIMATION AUTHORING - now merged into animation_physics (Phase 53)
  // Backward compatibility alias - logs deprecation warning once per session
  toolRegistry.register('manage_animation_authoring', async (args, tools) => {
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__animationAuthoringDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_animation_authoring is deprecated and merged into animation_physics. Use animation_physics instead.');
      globalObj.__animationAuthoringDeprecationLogged = true;
    }
    return await handleAnimationAuthoringTools(getAction(args), args, tools);
  });

  // 25. [DEPRECATED] AUDIO AUTHORING - now merged into manage_audio (Phase 53)
  // Backward compatibility alias - logs deprecation warning once per session
  toolRegistry.register('manage_audio_authoring', async (args, tools) => {
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__audioAuthoringDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_audio_authoring is deprecated and merged into manage_audio. Use manage_audio instead.');
      globalObj.__audioAuthoringDeprecationLogged = true;
    }
    return await handleAudioAuthoringTools(getAction(args), args, tools);
  });

  // 26. [DEPRECATED] NIAGARA AUTHORING - now merged into manage_effect (Phase 53)
  // Backward compatibility alias - logs deprecation warning once per session
  toolRegistry.register('manage_niagara_authoring', async (args, tools) => {
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__niagaraAuthoringDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_niagara_authoring is deprecated and merged into manage_effect. Use manage_effect instead.');
      globalObj.__niagaraAuthoringDeprecationLogged = true;
    }
    return await handleNiagaraAuthoringTools(getAction(args), args, tools);
  });

  // 27. GAS - GAMEPLAY ABILITY SYSTEM (Phase 13)
  toolRegistry.register('manage_gas', async (args, tools) => await handleGASTools(getAction(args), args, tools));

  // 28. CHARACTER & MOVEMENT SYSTEM (Phase 14)
  toolRegistry.register('manage_character', async (args, tools) => await handleCharacterTools(getAction(args), args, tools));

  // 29. COMBAT & WEAPONS SYSTEM (Phase 15)
  toolRegistry.register('manage_combat', async (args, tools) => await handleCombatTools(getAction(args), args, tools));

  // 30. AI SYSTEM (Phase 16)
  toolRegistry.register('manage_ai', async (args, tools) => await handleAITools(getAction(args), args, tools));

  // 31. INVENTORY & ITEMS SYSTEM (Phase 17)
  toolRegistry.register('manage_inventory', async (args, tools) => await handleInventoryTools(getAction(args), args, tools));

  // 32. INTERACTION SYSTEM (Phase 18)
  toolRegistry.register('manage_interaction', async (args, tools) => await handleInteractionTools(getAction(args), args, tools));

  // 33. WIDGET AUTHORING SYSTEM (Phase 19)
  toolRegistry.register('manage_widget_authoring', async (args, tools) => await handleWidgetAuthoringTools(getAction(args), args, tools));

  // 34. NETWORKING & MULTIPLAYER (Phase 20)
  toolRegistry.register('manage_networking', async (args, tools) => await handleNetworkingTools(getAction(args), args, tools));

  // 35. GAME FRAMEWORK (Phase 21)
  toolRegistry.register('manage_game_framework', async (args, tools) => await handleGameFrameworkTools(getAction(args), args, tools));

  // 36. SESSIONS & LOCAL MULTIPLAYER (Phase 22)
  toolRegistry.register('manage_sessions', async (args, tools) => await handleSessionsTools(getAction(args), args, tools));

  // 37. [DEPRECATED] LEVEL STRUCTURE - now merged into manage_level (Phase 54)
  // Backward compatibility alias - logs deprecation warning once per session
  toolRegistry.register('manage_level_structure', async (args, tools) => {
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__levelStructureDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_level_structure is deprecated and merged into manage_level. Use manage_level instead.');
      globalObj.__levelStructureDeprecationLogged = true;
    }
    return await handleLevelStructureTools(getAction(args), args, tools);
  });

  // 38. VOLUMES & ZONES (Phase 24)
  toolRegistry.register('manage_volumes', async (args, tools) => await handleVolumeTools(getAction(args), args, tools));

  // 39. NAVIGATION SYSTEM (Phase 25)
  toolRegistry.register('manage_navigation', async (args, tools) => await handleNavigationTools(getAction(args), args, tools));

  // 40. SPLINE SYSTEM (Phase 26)
  toolRegistry.register('manage_splines', async (args, tools) => await handleSplineTools(getAction(args), args, tools));

  // 41. PCG FRAMEWORK (Phase 27)
  toolRegistry.register('manage_pcg', async (args, tools) => await handlePCGTools(getAction(args), args, tools));

  // 42. [DEPRECATED] WATER SYSTEM - now merged into build_environment (Phase 54)
  // Backward compatibility alias - logs deprecation warning once per session
  toolRegistry.register('manage_water', async (args, tools) => {
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__waterDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_water is deprecated and merged into build_environment. Use build_environment instead.');
      globalObj.__waterDeprecationLogged = true;
    }
    return await handleEnvironmentTools(getAction(args), args, tools);
  });

  // 43. [DEPRECATED] WEATHER SYSTEM - now merged into build_environment (Phase 54)
  // Backward compatibility alias - logs deprecation warning once per session
  toolRegistry.register('manage_weather', async (args, tools) => {
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__weatherDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_weather is deprecated and merged into build_environment. Use build_environment instead.');
      globalObj.__weatherDeprecationLogged = true;
    }
    return await handleEnvironmentTools(getAction(args), args, tools);
  });

  // 44. POST-PROCESS & RENDERING SYSTEM (Phase 29)
  toolRegistry.register('manage_post_process', async (args, tools) => await handlePostProcessTools(getAction(args), args, tools));

  // 45. [DEPRECATED] SEQUENCER - now merged into manage_sequence (Phase 54)
  // Backward compatibility alias - logs deprecation warning once per session
  toolRegistry.register('manage_sequencer', async (args, tools) => {
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__sequencerDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_sequencer is deprecated and merged into manage_sequence. Use manage_sequence instead.');
      globalObj.__sequencerDeprecationLogged = true;
    }
    return await handleSequencerTools(getAction(args), args, tools);
  });

  // 46. [DEPRECATED] MOVIE RENDER QUEUE - now merged into manage_sequence (Phase 54)
  // Backward compatibility alias - logs deprecation warning once per session
  toolRegistry.register('manage_movie_render', async (args, tools) => {
    const globalObj = globalThis as unknown as DeprecationFlags;
    if (!globalObj.__movieRenderDeprecationLogged) {
      const deprecationLogger = new Logger('DeprecationWarning');
      deprecationLogger.warn('manage_movie_render is deprecated and merged into manage_sequence. Use manage_sequence instead.');
      globalObj.__movieRenderDeprecationLogged = true;
    }
    return await handleMovieRenderTools(getAction(args), args, tools);
  });

  // 47. MEDIA FRAMEWORK (Phase 30 - Cinematics & Media)
  toolRegistry.register('manage_media', async (args, tools) => await handleMediaTools(getAction(args), args, tools));

  // 48. DATA & PERSISTENCE (Phase 31)
  toolRegistry.register('manage_data', async (args, tools) => await handleDataTools(getAction(args), args, tools));

  // 49. BUILD & DEPLOYMENT (Phase 32)
  toolRegistry.register('manage_build', async (args, tools) => await handleBuildTools(getAction(args), args, tools));

  // 50. TESTING & QUALITY (Phase 33)
  toolRegistry.register('manage_testing', async (args, tools) => await handleTestingTools(getAction(args), args, tools));

  // 51. EDITOR UTILITIES (Phase 34)
  toolRegistry.register('manage_editor_utilities', async (args, tools) => await handleEditorUtilitiesTools(getAction(args), args, tools));

  // 52. GAMEPLAY SYSTEMS (Phase 35)
  toolRegistry.register('manage_gameplay_systems', async (args, tools) => await handleGameplaySystemsTools(getAction(args), args, tools));

  // 53. CHARACTER & AVATAR (Phase 36)
  toolRegistry.register('manage_character_avatar', async (args, tools) => await handleCharacterAvatarTools(getAction(args), args, tools));

  // 54. ASSET & CONTENT PLUGINS (Phase 37)
  toolRegistry.register('manage_asset_plugins', async (args, tools) => await handleAssetPluginsTools(getAction(args), args, tools));

  // 55. AUDIO MIDDLEWARE PLUGINS (Phase 38)
  toolRegistry.register('manage_audio_middleware', async (args, tools) => await handleAudioMiddlewareTools(getAction(args), args, tools));

  // 56. LIVE LINK & MOTION CAPTURE (Phase 39)
  toolRegistry.register('manage_livelink', async (args, tools) => await handleLiveLinkTools(getAction(args), args, tools));

  // 57. VIRTUAL PRODUCTION PLUGINS (Phase 40)
  toolRegistry.register('manage_virtual_production', async (args, tools) => await handleVirtualProductionTools(getAction(args), args, tools));

  // 58. XR PLUGINS (Phase 41)
  toolRegistry.register('manage_xr', async (args, tools) => await handleXRPluginsTools(getAction(args), args, tools));

  // 59. AI & NPC PLUGINS (Phase 42)
  toolRegistry.register('manage_ai_npc', async (args, tools) => await handleAINPCTools(getAction(args), args, tools));

  // 60. UTILITY PLUGINS (Phase 43)
  toolRegistry.register('manage_utility_plugins', async (args, tools) => await handleUtilityPluginsTools(getAction(args), args, tools));

  // 61. PHYSICS & DESTRUCTION (Phase 44)
  toolRegistry.register('manage_physics_destruction', async (args, tools) => await handlePhysicsDestructionTools(getAction(args), args, tools));

  // 62. ACCESSIBILITY SYSTEM (Phase 45)
  toolRegistry.register('manage_accessibility', async (args, tools) => await handleAccessibilityTools(getAction(args), args, tools));

  // 63. MODDING & UGC SYSTEM (Phase 46)
  toolRegistry.register('manage_modding', async (args, tools) => await handleModdingTools(getAction(args), args, tools));

  // 64. PIPELINE MANAGEMENT
  toolRegistry.register('manage_pipeline', async (args, tools) => await handlePipelineTools(getAction(args), args, tools));
}

// Initialize default handlers immediately
registerDefaultHandlers();

// Export the main consolidated tool call handler
export async function handleConsolidatedToolCall(
  name: string,
  args: Record<string, unknown>,
  tools: ITools
) {
  const logger = new Logger('ConsolidatedToolHandler');
  const startTime = Date.now();
  logger.info(`Starting execution of ${name} at ${new Date().toISOString()}`);

  try {
    const normalized = normalizeToolCall(name, args);
    const normalizedName = normalized.name;
    const normalizedArgs = normalized.args;
    // Note: action extracted inside handler usually, but here we might pass it if needed.
    // The handlers above re-extract or use normalizedArgs.action. 
    // `normalizeToolCall` puts `action` into `normalized.action` but does NOT necessarily put it into `normalized.args.action`.
    // Let's ensure args has action if we relied on it above.
    if (normalized.action && !normalizedArgs.action) {
      normalizedArgs.action = normalized.action;
    }

    const handler = toolRegistry.getHandler(normalizedName);

    if (handler) {
      return await handler(normalizedArgs, tools);
    }

    // Fallback or Unknown
    return cleanObject({ success: false, error: 'UNKNOWN_TOOL', message: `Unknown consolidated tool: ${name}` });

  } catch (err: unknown) {
    const duration = Date.now() - startTime;
    const errObj = err as Record<string, unknown> | null;
    const errorMessage = typeof errObj?.message === 'string' ? errObj.message : String(err);
    logger.error(`Failed execution of ${name} after ${duration}ms: ${errorMessage}`);
    const lowerError = errorMessage.toString().toLowerCase();
    const isTimeout = lowerError.includes('timeout');

    let text: string;
    if (isTimeout) {
      text = `Tool ${name} timed out. Please check Unreal Engine connection.`;
    } else {
      text = `Failed to execute ${name}: ${errorMessage}`;
    }

    return ResponseFactory.error(text);
  }
}