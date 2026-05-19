import { cleanObject } from '../utils/safe-json.js';
import { Logger } from '../utils/logger.js';
import { ResponseFactory } from '../utils/response-factory.js';
import { ITools } from '../types/tool-interfaces.js';
import { toolRegistry } from './dynamic-handler-registry.js';
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
} from './consolidated-tool-definitions.js';
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
import { handleManageToolsTools } from './handlers/manage-tools-handlers.js';

type NormalizedToolCall = {
  name: string;
  action?: string;
  args: Record<string, unknown>;
};

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

function getToolAction(args: Record<string, unknown>): string {
  const action = args.action ?? args.subAction;
  return typeof action === 'string' ? action : requireAction(args);
}

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

const materialAuthoringActionSet = new Set<string>(MATERIAL_AUTHORING_ACTIONS);
const textureActionSet = new Set<string>(TEXTURE_ACTIONS);
const skeletonActionSet = new Set<string>(SKELETON_ACTIONS);
const lightingActionSet = new Set<string>(LIGHTING_ACTIONS);
const splineActionSet = new Set<string>(SPLINE_ACTIONS);
const performanceActionSet = new Set<string>(PERFORMANCE_ACTIONS);
const behaviorTreeActionSet = new Set<string>(BEHAVIOR_TREE_ACTIONS);
const navigationActionSet = new Set<string>(NAVIGATION_ACTIONS);
const widgetAuthoringActionSet = new Set<string>(WIDGET_AUTHORING_ACTIONS);
const sessionActionSet = new Set<string>(SESSION_ACTIONS);
const gameFrameworkActionSet = new Set<string>(GAME_FRAMEWORK_ACTIONS);
const inputActionSet = new Set<string>(INPUT_ACTIONS);
const volumeActionSet = new Set<string>(VOLUME_ACTIONS);

function normalizeToolCall(
  name: string,
  args: Record<string, unknown>
): NormalizedToolCall {
  const actionValue = args.action ?? args.subAction;
  const action = typeof actionValue === 'string' ? actionValue : undefined;

  return {
    name,
    action,
    args
  };
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function mergeActionParams(args: Record<string, unknown>): Record<string, unknown> {
  if (!isRecord(args.params)) return args;

  const merged = { ...args.params, ...args };
  delete merged.params;
  return merged;
}

function registerDefaultHandlers() {
  toolRegistry.register('manage_asset', async (args, tools) => {
    const action = getToolAction(args);
    if (materialAuthoringActionSet.has(action)) return await handleMaterialAuthoringTools(action, args, tools);
    if (textureActionSet.has(action)) return await handleTextureTools(action, args, tools);
    if (action === 'nanite_rebuild_mesh') {
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

  toolRegistry.register('manage_blueprint', async (args, tools) => {
    const action = getToolAction(args);
    if (action === 'create_blueprint') return await handleBlueprintTools('create', args, tools);
    if (action === 'get_blueprint') return await handleBlueprintGet(args, tools);
    if (widgetAuthoringActionSet.has(action)) return await handleWidgetAuthoringTools(action, args, tools);
    const graphActions = ['create_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details', 'list_node_types', 'set_pin_default_value'];
    if (graphActions.includes(action)) return await handleGraphTools('manage_blueprint', action, args, tools);
    return await handleBlueprintTools(action, args, tools);
  });

  toolRegistry.register('control_actor', async (args, tools) => await handleActorTools(getToolAction(args), args, tools));
  toolRegistry.register('control_editor', async (args, tools) => await handleEditorTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_level', async (args, tools) => await handleLevelTools(getToolAction(args), args, tools));

  const animationAuthoringActions = new Set([
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
  toolRegistry.register('animation_physics', async (args, tools) => {
    const action = getToolAction(args);
    if (skeletonActionSet.has(action)) return await handleSkeletonTools(action, args, tools);
    if (animationAuthoringActions.has(action)) return await handleAnimationAuthoringTools(action, args, tools);
    if (action === 'add_notify' && (args.frame !== undefined || args.assetPath !== undefined)) {
      return await handleAnimationAuthoringTools(action, args, tools);
    }
    return await handleAnimationTools(action, args, tools);
  });

  toolRegistry.register('manage_effect', async (args, tools) => await handleEffectTools(getToolAction(args), args, tools));

  toolRegistry.register('build_environment', async (args, tools) => {
    const action = getToolAction(args);
    if (lightingActionSet.has(action)) return await handleLightingTools(action, args, tools);
    if (splineActionSet.has(action)) return await handleSplineTools(action, args, tools);
    return await handleEnvironmentTools(action, args, tools);
  });

  toolRegistry.register('system_control', async (args, tools) => {
    const action = getToolAction(args);
    if (action === 'console_command') return await handleConsoleCommand(args, tools);
    if (action === 'run_ubt') return await handlePipelineTools(action, args, tools);
    if (performanceActionSet.has(action)) return await handlePerformanceTools(action, args, tools);
    if (action === 'run_tests') return cleanObject(await executeAutomationRequest(tools, 'manage_tests', { ...args, subAction: action }, 'Bridge unavailable'));
    if (action === 'subscribe' || action === 'unsubscribe') {
      return cleanObject(await executeAutomationRequest(tools, 'manage_logs', { ...args, subAction: action }, 'Bridge unavailable'));
    }
    if (action === 'spawn_category') {
      const categoryName = typeof args.categoryName === 'string'
        ? args.categoryName.trim()
        : (typeof args.category === 'string' ? args.category.trim() : 'AI');
      if (!/^[A-Za-z0-9_-]+$/.test(categoryName)) {
        return { success: false, error: 'INVALID_CATEGORY_NAME', message: 'Category names may only contain letters, numbers, underscores, and hyphens.' };
      }
      const payload = { ...(args as Record<string, unknown>), subAction: action, categoryName };
      const res = await executeAutomationRequest(tools, 'manage_debug', payload, 'Bridge unavailable') as Record<string, unknown>;
      return cleanObject(Object.assign({}, res, { action, categoryName }));
    }
    if (action === 'start_session') {
      const channels = typeof args.channels === 'string' ? args.channels.trim() : '';
      if (channels && !/^[A-Za-z0-9_, -]+$/.test(channels)) {
        return { success: false, error: 'INVALID_CHANNELS', message: 'Trace channels contain unsupported characters.' };
      }
      const res = await executeAutomationRequest(tools, 'manage_insights', {
        ...args,
        action,
        subAction: action,
        channels
      }, 'Bridge unavailable') as Record<string, unknown>;
      return cleanObject({ ...res, action, channels, sessionType: 'trace' });
    }
    if (action === 'lumen_update_scene') return cleanObject(await executeAutomationRequest(tools, 'manage_render', { ...args, subAction: action }, 'Bridge unavailable'));
    return await handleSystemTools(action, args, tools);
  });

  toolRegistry.register('manage_sequence', async (args, tools) => await handleSequenceTools(getToolAction(args), args, tools));
  toolRegistry.register('inspect', async (args, tools) => await handleInspectTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_tools', async (args, tools) => await handleManageToolsTools(getToolAction(args), args, tools));
  const audioAuthoringActions = new Set([
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
    const action = getToolAction(args);
    if (audioAuthoringActions.has(action)) return await handleAudioAuthoringTools(action, args, tools);
    return await handleAudioTools(action, args, tools);
  });

  toolRegistry.register('manage_geometry', async (args, tools) => await handleGeometryTools(getToolAction(args), args, tools));

  toolRegistry.register('manage_gas', async (args, tools) => await handleGASTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_character', async (args, tools) => await handleCharacterTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_combat', async (args, tools) => await handleCombatTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_ai', async (args, tools) => {
    const action = getToolAction(args);
    if (behaviorTreeActionSet.has(action)) return await handleGraphTools('manage_behavior_tree', action, args, tools);
    if (navigationActionSet.has(action)) return await handleNavigationTools(action, args, tools);
    return await handleAITools(action, args, tools);
  });
  toolRegistry.register('manage_inventory', async (args, tools) => await handleInventoryTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_interaction', async (args, tools) => await handleInteractionTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_networking', async (args, tools) => {
    const action = getToolAction(args);
    if (sessionActionSet.has(action)) return await handleSessionsTools(action, args, tools);
    if (gameFrameworkActionSet.has(action)) return await handleGameFrameworkTools(action, args, tools);
    if (inputActionSet.has(action)) return await handleInputTools(action, args, tools);
    return await handleNetworkingTools(action, args, tools);
  });
  toolRegistry.register('manage_level_structure', async (args, tools) => {
    const action = getToolAction(args);
    if (volumeActionSet.has(action)) return await handleVolumeTools(action, args, tools);
    return await handleLevelStructureTools(action, args, tools);
  });
}

registerDefaultHandlers();

export async function handleConsolidatedToolCall(
  name: string,
  args: Record<string, unknown>,
  tools: ITools
) {
  const logger = new Logger('ConsolidatedToolHandler');
  const startTime = Date.now();

  try {
    const expandedArgs = mergeActionParams(args);
    const normalized = normalizeToolCall(name, expandedArgs);
    const normalizedArgs = normalized.args;

    if (normalized.action && !normalizedArgs.action) {
      normalizedArgs.action = normalized.action;
    }

    const handler = toolRegistry.getHandler(normalized.name);
    if (handler) {
      return await handler(normalizedArgs, tools);
    }

    return cleanObject({ success: false, error: 'UNKNOWN_TOOL', message: `Unknown consolidated tool: ${name}`, data: null });
  } catch (err: unknown) {
    const duration = Date.now() - startTime;
    const errObj = err as Record<string, unknown> | null;
    const errorMessage = typeof errObj?.message === 'string' ? errObj.message : String(err);
    logger.error(`Failed execution of ${name} after ${duration}ms: ${errorMessage}`);

    const isTimeout = errorMessage.toLowerCase().includes('timeout');
    const text = isTimeout
      ? `Tool ${name} timed out. Please check Unreal Engine connection.`
      : `Failed to execute ${name}: ${errorMessage}`;

    return ResponseFactory.error(text);
  }
}
