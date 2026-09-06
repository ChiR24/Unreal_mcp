/**
 * Centralized action name constants for executeAutomationRequest calls.
 *
 * This file eliminates string literal duplication across handlers,
 * making refactoring safer and providing single source of truth.
 */

// ============================================================================
// PRIMARY TOOL ACTIONS (2nd parameter to executeAutomationRequest)
// ============================================================================

/** Primary tool/domain action names */
export const TOOL_ACTIONS = {
  // ==================== CORE TOOLS ====================
  CONTROL_ACTOR: 'control_actor',

  // ==================== AUTHORING TOOLS ====================
  MANAGE_BLUEPRINT: 'manage_blueprint',
  MANAGE_MATERIAL_AUTHORING: 'manage_material_authoring',
  MANAGE_TEXTURE: 'manage_texture',

  // ==================== GAMEPLAY TOOLS ====================
  ANIMATION_PHYSICS: 'animation_physics',

  // ==================== CONSOLE/SYSTEM ====================
  CONSOLE_COMMAND: 'console_command',

  // ==================== AUDIO ACTIONS ====================
  CREATE_SOUND_CUE: 'create_sound_cue',
  PLAY_SOUND_AT_LOCATION: 'play_sound_at_location',
  PLAY_SOUND_2D: 'play_sound_2d',
  CREATE_AUDIO_COMPONENT: 'create_audio_component',
  SET_SOUND_ATTENUATION: 'set_sound_attenuation',
  CREATE_SOUND_CLASS: 'create_sound_class',
  CREATE_SOUND_MIX: 'create_sound_mix',
  PUSH_SOUND_MIX: 'push_sound_mix',
  POP_SOUND_MIX: 'pop_sound_mix',
  CREATE_AMBIENT_SOUND: 'create_ambient_sound',
  CREATE_REVERB_ZONE: 'create_reverb_zone',
  ENABLE_AUDIO_ANALYSIS: 'enable_audio_analysis',
  FADE_SOUND: 'fade_sound',
  SET_DOPPLER_EFFECT: 'set_doppler_effect',
  SET_AUDIO_OCCLUSION: 'set_audio_occlusion',
  SPAWN_SOUND_AT_LOCATION: 'spawn_sound_at_location',
  PLAY_SOUND_ATTACHED: 'play_sound_attached',
  SET_SOUND_MIX_CLASS_OVERRIDE: 'set_sound_mix_class_override',
  CLEAR_SOUND_MIX_CLASS_OVERRIDE: 'clear_sound_mix_class_override',
  SET_BASE_SOUND_MIX: 'set_base_sound_mix',
  PRIME_SOUND: 'prime_sound',

  // ==================== LIGHTING ACTIONS ====================
  SPAWN_LIGHT: 'spawn_light',
  SPAWN_SKY_LIGHT: 'spawn_sky_light',
  ENSURE_SINGLE_SKY_LIGHT: 'ensure_single_sky_light',
  SETUP_GLOBAL_ILLUMINATION: 'setup_global_illumination',
  CONFIGURE_SHADOWS: 'configure_shadows',
  BAKE_LIGHTMAP: 'bake_lightmap',
  CREATE_LIGHTING_ENABLED_LEVEL: 'create_lighting_enabled_level',
  CREATE_LIGHTMASS_VOLUME: 'create_lightmass_volume',
  SET_EXPOSURE: 'set_exposure',
  SET_AMBIENT_OCCLUSION: 'set_ambient_occlusion',
  SETUP_VOLUMETRIC_FOG: 'setup_volumetric_fog',
  LIST_LIGHT_TYPES: 'list_light_types',

  // ==================== PERFORMANCE ACTIONS ====================
  START_PROFILING: 'start_profiling',
  STOP_PROFILING: 'stop_profiling',
  SHOW_FPS: 'show_fps',
  SHOW_STATS: 'show_stats',
  SET_SCALABILITY: 'set_scalability',
  SET_RESOLUTION_SCALE: 'set_resolution_scale',
  SET_VSYNC: 'set_vsync',
  SET_FRAME_RATE_LIMIT: 'set_frame_rate_limit',
  GENERATE_MEMORY_REPORT: 'generate_memory_report',
  CONFIGURE_TEXTURE_STREAMING: 'configure_texture_streaming',
  CONFIGURE_LOD: 'configure_lod',
  MERGE_ACTORS: 'merge_actors',
  CONFIGURE_NANITE: 'configure_nanite',
} as const;
