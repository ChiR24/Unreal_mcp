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
// import { getDynamicHandlerForTool } from './dynamic-handler-registry.js';
// import { consolidatedToolDefinitions } from './consolidated-tool-definitions.js';

type NormalizedToolCall = {
  name: string;
  action: string;
  args: any;
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
  args: any
): NormalizedToolCall {
  let normalizedName = name;
  let action: string;

  if (args && typeof (args as any).action === 'string') {
    action = (args as any).action;
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
  // 1. ASSET MANAGER
  toolRegistry.register('manage_asset', async (args, tools) => {
    const action = args.subAction || args.action || requireAction(args); // Fallback assumption
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
    const action = args.action || requireAction(args);
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
    return await handleActorTools(args.action || requireAction(args), args, tools);
  });

  // 4. EDITOR CONTROL
  toolRegistry.register('control_editor', async (args, tools) => {
    const action = args.action || requireAction(args);
    if (action === 'simulate_input') {
      const payload = { ...args, subAction: action };
      return cleanObject(await executeAutomationRequest(tools, 'manage_ui', payload, 'Automation bridge not available'));
    }
    return await handleEditorTools(action, args, tools);
  });

  // 5. LEVEL MANAGER
  toolRegistry.register('manage_level', async (args, tools) => {
    const action = args.action || requireAction(args);
    if (['load_cells', 'set_datalayer'].includes(action)) {
      const payload = { ...args, subAction: action };
      return cleanObject(await executeAutomationRequest(tools, 'manage_world_partition', payload, 'Automation bridge not available'));
    }
    return await handleLevelTools(action, args, tools);
  });

  // 6. ANIMATION & PHYSICS
  toolRegistry.register('animation_physics', async (args, tools) => await handleAnimationTools(args.action || requireAction(args), args, tools));

  // 7. EFFECTS MANAGER
  toolRegistry.register('manage_effect', async (args, tools) => {
    const action = args.action || requireAction(args);
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
  toolRegistry.register('build_environment', async (args, tools) => await handleEnvironmentTools(args.action || requireAction(args), args, tools));

  // 9. SYSTEM CONTROL
  toolRegistry.register('system_control', async (args, tools) => {
    const action = args.action || requireAction(args);
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
  toolRegistry.register('manage_sequence', async (args, tools) => await handleSequenceTools(args.action || requireAction(args), args, tools));

  // 11. INTROSPECTION
  toolRegistry.register('inspect', async (args, tools) => await handleInspectTools(args.action || requireAction(args), args, tools));

  // 12. AUDIO
  toolRegistry.register('manage_audio', async (args, tools) => await handleAudioTools(args.action || requireAction(args), args, tools));

  // 13. BEHAVIOR TREE
  toolRegistry.register('manage_behavior_tree', async (args, tools) => await handleGraphTools('manage_behavior_tree', args.action || requireAction(args), args, tools));

  // 14. BLUEPRINT GRAPH DIRECT
  toolRegistry.register('manage_blueprint_graph', async (args, tools) => await handleGraphTools('manage_blueprint_graph', args.action || requireAction(args), args, tools));

  // 15. RENDER TOOLS
  toolRegistry.register('manage_render', async (args, tools) => {
    const action = args.action || requireAction(args);
    return cleanObject(await executeAutomationRequest(tools, 'manage_render', { ...args, subAction: action }, 'Bridge unavailable'));
  });

  // 16. WORLD PARTITION
  toolRegistry.register('manage_world_partition', async (args, tools) => {
    const action = args.action || requireAction(args);
    return cleanObject(await executeAutomationRequest(tools, 'manage_world_partition', { ...args, subAction: action }, 'Bridge unavailable'));
  });

  // 17. LIGHTING
  toolRegistry.register('manage_lighting', async (args, tools) => await handleLightingTools(args.action || requireAction(args), args, tools));

  // 18. PERFORMANCE
  toolRegistry.register('manage_performance', async (args, tools) => await handlePerformanceTools(args.action || requireAction(args), args, tools));

  // 19. INPUT
  toolRegistry.register('manage_input', async (args, tools) => await handleInputTools(args.action || requireAction(args), args, tools));
}

// Initialize default handlers immediately
registerDefaultHandlers();

// Export the main consolidated tool call handler
export async function handleConsolidatedToolCall(
  name: string,
  args: any,
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

  } catch (err: any) {
    const duration = Date.now() - startTime;
    logger.error(`Failed execution of ${name} after ${duration}ms: ${err?.message || String(err)}`);
    const errorMessage = err?.message || String(err);
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