import {
  BEHAVIOR_TREE_ACTIONS,
  GAME_FRAMEWORK_ACTIONS,
  INPUT_ACTIONS,
  LIGHTING_ACTIONS,
  MATERIAL_AUTHORING_ACTIONS,
  NAVIGATION_ACTIONS,
  PERFORMANCE_ACTIONS,
  SESSION_ACTIONS,
  SKELETON_ACTIONS,
  SPLINE_ACTIONS,
  TEXTURE_ACTIONS,
  VOLUME_ACTIONS,
  WIDGET_AUTHORING_ACTIONS
} from '../catalog/consolidated-tool-definitions.js';
import { requireAction } from '../handlers/foundation/dispatch/common-handlers.js';

const MATERIAL_GRAPH_ACTION_MAP: Record<string, string> = {
  add_material_node: 'add_node',
  connect_material_pins: 'connect_pins',
  remove_material_node: 'remove_node',
  break_material_connections: 'break_connections',
  get_material_node_details: 'get_node_details'
};

const BEHAVIOR_TREE_ACTION_MAP: Record<string, string> = {
  add_bt_node: 'add_node',
  connect_bt_nodes: 'connect_nodes',
  remove_bt_node: 'remove_node',
  break_bt_connections: 'break_connections',
  set_bt_node_properties: 'set_node_properties'
};

export const materialAuthoringActionSet = new Set<string>(MATERIAL_AUTHORING_ACTIONS);
export const textureActionSet = new Set<string>(TEXTURE_ACTIONS);
export const skeletonActionSet = new Set<string>(SKELETON_ACTIONS);
export const lightingActionSet = new Set<string>(LIGHTING_ACTIONS);
export const splineActionSet = new Set<string>(SPLINE_ACTIONS);
export const performanceActionSet = new Set<string>(PERFORMANCE_ACTIONS);
export const behaviorTreeActionSet = new Set<string>(BEHAVIOR_TREE_ACTIONS);
export const navigationActionSet = new Set<string>(NAVIGATION_ACTIONS);
export const widgetAuthoringActionSet = new Set<string>(WIDGET_AUTHORING_ACTIONS);
export const sessionActionSet = new Set<string>(SESSION_ACTIONS);
export const gameFrameworkActionSet = new Set<string>(GAME_FRAMEWORK_ACTIONS);
export const inputActionSet = new Set<string>(INPUT_ACTIONS);
export const volumeActionSet = new Set<string>(VOLUME_ACTIONS);
export const blueprintGraphActionSet = new Set<string>([
  'create_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property',
  'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details',
  'list_node_types', 'set_pin_default_value'
]);
export const animationAuthoringActionSet = new Set<string>([
  'create_animation_sequence', 'set_sequence_length', 'add_bone_track', 'set_bone_key', 'set_curve_key',
  'add_notify_state', 'add_sync_marker', 'set_root_motion_settings', 'set_additive_settings',
  'create_montage', 'add_montage_section', 'add_montage_slot', 'set_section_timing',
  'add_montage_notify', 'set_blend_in', 'set_blend_out', 'link_sections',
  'create_blend_space_1d', 'create_blend_space_2d', 'add_blend_sample', 'force_rebuild_blend_space', 'set_axis_settings', 'set_interpolation_settings',
  'create_aim_offset', 'add_aim_offset_sample',
  'create_anim_blueprint', 'create_animation_bp', 'create_animation_blueprint', 'add_state_machine', 'add_state', 'add_transition', 'set_transition_rules',
  'add_blend_node', 'add_cached_pose', 'add_slot_node', 'add_layered_blend_per_bone', 'set_anim_graph_node_value',
  'create_control_rig', 'create_ik_rig', 'create_ik_retargeter', 'set_retarget_chain_mapping', 'get_animation_info'
]);
export const audioAuthoringActionSet = new Set<string>([
  'create_sound_cue', 'create_sound_class', 'create_sound_mix',
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

export const dataActionSet = new Set<string>([
  'create_data_asset', 'create_primary_data_asset',
  'create_data_table', 'add_data_table_row', 'modify_data_table_row', 'delete_data_table_row',
  'import_data_table_csv', 'export_data_table_csv',
  'create_curve_table', 'create_curve_float', 'create_curve_linear_color',
  'create_save_game_class', 'add_save_variable', 'save_game_to_slot', 'load_game_from_slot',
  'delete_save_slot', 'check_save_slot_exists', 'get_save_slot_names', 'configure_async_save_load',
  'create_gameplay_tag', 'create_tag_container', 'add_tag_to_container', 'remove_tag_from_container',
  'check_tag_match', 'register_native_tag', 'create_tag_table',
  'read_config_value', 'write_config_value', 'get_section', 'create_config_section',
  'flush_config', 'reload_config', 'get_config_hierarchy'
]);

export function getToolAction(args: Record<string, unknown>): string {
  const action = args.action ?? args.subAction;
  return typeof action === 'string' ? action : requireAction(args);
}

export function isMaterialGraphAction(action: string): boolean {
  return (
    Object.prototype.hasOwnProperty.call(MATERIAL_GRAPH_ACTION_MAP, action) ||
    action.includes('material_node') ||
    action.includes('material_pins') ||
    action.includes('material_connections')
  );
}

export function isBehaviorTreeGraphAction(action: string): boolean {
  return (
    Object.prototype.hasOwnProperty.call(BEHAVIOR_TREE_ACTION_MAP, action) ||
    action.includes('_bt_') ||
    action.includes('behavior_tree')
  );
}

export function resolveMaterialGraphSubAction(action: string): string {
  return MATERIAL_GRAPH_ACTION_MAP[action] ?? action;
}

export function resolveBehaviorTreeGraphSubAction(action: string): string {
  return BEHAVIOR_TREE_ACTION_MAP[action] ?? action;
}
