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
// import { getDynamicHandlerForTool } from './dynamic-handler-registry.js';
// import { consolidatedToolDefinitions } from './consolidated-tool-definitions.js';

type NormalizedToolCall = {
  name: string;
  action: string;
  args: Record<string, unknown>;
};

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
  if (normalizedName === 'manage_pipeline') {
    normalizedName = 'system_control';
    action = 'run_ubt';
  }
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

  // 2. BLUEPRINT MANAGER
  toolRegistry.register('manage_blueprint', async (args, tools) => {
    const action = getAction(args);
    if (action === 'get_blueprint') {
      return await handleBlueprintGet(args, tools);
    }
    const graphActions = ['create_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details'];
    if (graphActions.includes(action)) {
      return await handleGraphTools('manage_blueprint_graph', action, args, tools);
    }
    return await handleBlueprintTools(action, args, tools);
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

  // 5. LEVEL MANAGER
  toolRegistry.register('manage_level', async (args, tools) => {
    const action = getAction(args);
    if (['load_cells', 'set_datalayer'].includes(action)) {
      const payload = { ...args, subAction: action };
      return cleanObject(await executeAutomationRequest(tools, 'manage_world_partition', payload, 'Automation bridge not available'));
    }
    return await handleLevelTools(action, args, tools);
  });

  // 6. ANIMATION & PHYSICS
  toolRegistry.register('animation_physics', async (args, tools) => await handleAnimationTools(getAction(args), args, tools));

  // 7. EFFECTS MANAGER
  toolRegistry.register('manage_effect', async (args, tools) => {
    const action = getAction(args);
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

  // 8. ENVIRONMENT BUILDER
  toolRegistry.register('build_environment', async (args, tools) => await handleEnvironmentTools(getAction(args), args, tools));

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

  // 10. SEQUENCER
  toolRegistry.register('manage_sequence', async (args, tools) => await handleSequenceTools(getAction(args), args, tools));

  // 11. INTROSPECTION
  toolRegistry.register('inspect', async (args, tools) => await handleInspectTools(getAction(args), args, tools));

  // 12. AUDIO
  toolRegistry.register('manage_audio', async (args, tools) => await handleAudioTools(getAction(args), args, tools));

  // 13. BEHAVIOR TREE
  toolRegistry.register('manage_behavior_tree', async (args, tools) => await handleGraphTools('manage_behavior_tree', getAction(args), args, tools));

  // 14. BLUEPRINT GRAPH DIRECT
  toolRegistry.register('manage_blueprint_graph', async (args, tools) => await handleGraphTools('manage_blueprint_graph', getAction(args), args, tools));

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

  // 24. ANIMATION AUTHORING (Phase 10)
  toolRegistry.register('manage_animation_authoring', async (args, tools) => await handleAnimationAuthoringTools(getAction(args), args, tools));

  // 25. AUDIO AUTHORING (Phase 11)
  toolRegistry.register('manage_audio_authoring', async (args, tools) => await handleAudioAuthoringTools(getAction(args), args, tools));

  // 26. NIAGARA AUTHORING (Phase 12)
  toolRegistry.register('manage_niagara_authoring', async (args, tools) => await handleNiagaraAuthoringTools(getAction(args), args, tools));

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

  // 37. LEVEL STRUCTURE (Phase 23)
  toolRegistry.register('manage_level_structure', async (args, tools) => await handleLevelStructureTools(getAction(args), args, tools));

  // 38. VOLUMES & ZONES (Phase 24)
  toolRegistry.register('manage_volumes', async (args, tools) => await handleVolumeTools(getAction(args), args, tools));

  // 39. NAVIGATION SYSTEM (Phase 25)
  toolRegistry.register('manage_navigation', async (args, tools) => await handleNavigationTools(getAction(args), args, tools));

  // 40. SPLINE SYSTEM (Phase 26)
  toolRegistry.register('manage_splines', async (args, tools) => await handleSplineTools(getAction(args), args, tools));
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