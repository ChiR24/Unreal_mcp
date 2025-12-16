import { cleanObject } from '../utils/safe-json.js';
import { Logger } from '../utils/logger.js';
import { ResponseFactory } from '../utils/response-factory.js';
import { ITools } from '../types/tool-interfaces.js';
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
import { getDynamicHandlerForTool } from './dynamic-handler-registry.js';
import { consolidatedToolDefinitions } from './consolidated-tool-definitions.js';

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

async function invokeNamedTool(
  name: string,
  action: string,
  args: any,
  tools: ITools
) {
  switch (name) {
    // 1. ASSET MANAGER
    case 'manage_asset': {
      // Reroute merged functionality
      if (['create_render_target', 'nanite_rebuild_mesh'].includes(action)) {
        const payload = { ...args, subAction: action };
        return cleanObject(await executeAutomationRequest(tools, 'manage_render', payload, `Automation bridge not available for ${action}`));
      }
      if (isMaterialGraphAction(action)) {
        // Map new action names to legacy subActions if needed, or just pass through if plugin supports them.
        // The plugin expects 'manage_material_graph' for these.
        // We need to map 'add_material_node' -> 'add_node' etc if the plugin is strict, 
        // or we assume we updated the plugin. 
        // For now, let's assume we map them to the old tool 'manage_material_graph' with the old action names.
        const subAction = MATERIAL_GRAPH_ACTION_MAP[action] || action;

        return await handleGraphTools('manage_material_graph', subAction, args, tools);
      }
      if (isBehaviorTreeGraphAction(action)) {
        const subAction = BEHAVIOR_TREE_ACTION_MAP[action] || action;
        return await handleGraphTools('manage_behavior_tree', subAction, args, tools);
      }
      return await handleAssetTools(action, args, tools);
    }

    // 2. BLUEPRINT MANAGER

    case 'manage_blueprint': {
      if (action === 'get_blueprint') {
        return await handleBlueprintGet(args, tools);
      }
      // Graph actions
      const graphActions = ['create_node', 'delete_node', 'connect_pins', 'break_pin_links', 'set_node_property', 'create_reroute_node', 'get_node_details', 'get_graph_details', 'get_pin_details'];
      if (graphActions.includes(action)) {
        return await handleGraphTools('manage_blueprint_graph', action, args, tools);
      }
      return await handleBlueprintTools(action, args, tools);
    }

    // 3. ACTOR CONTROL
    case 'control_actor':
      return await handleActorTools(action, args, tools);

    // 4. EDITOR CONTROL
    case 'control_editor': {
      if (action === 'simulate_input') {
        const payload = { ...args, subAction: action };
        return cleanObject(await executeAutomationRequest(tools, 'manage_ui', payload, 'Automation bridge not available'));
      }
      return await handleEditorTools(action, args, tools);
    }

    // 5. LEVEL MANAGER
    case 'manage_level': {
      if (['load_cells', 'set_datalayer'].includes(action)) {
        const payload = { ...args, subAction: action };
        return cleanObject(await executeAutomationRequest(tools, 'manage_world_partition', payload, 'Automation bridge not available'));
      }
      return await handleLevelTools(action, args, tools);
    }

    // 6. ANIMATION & PHYSICS
    case 'animation_physics':
      return await handleAnimationTools(action, args, tools);

    // 7. EFFECTS MANAGER (Renamed from create_effect)
    case 'manage_effect': {
      if (isNiagaraGraphAction(action)) {
        // Special case: set_niagara_parameter can be for actor (Effect Tool) or asset (Graph Tool)
        // If actorName is present, or systemName is present but no path, it's an instance operation -> handleEffectTools
        const isInstanceOp = action === 'set_niagara_parameter' && (args.actorName || (args.systemName && !args.assetPath && !args.systemPath));
        if (isInstanceOp) {
          return await handleEffectTools(action, args, tools);
        }

        const subAction = NIAGARA_GRAPH_ACTION_MAP[action] || action;
        return await handleGraphTools('manage_niagara_graph', subAction, args, tools);
      }
      return await handleEffectTools(action, args, tools);
    }

    // 8. ENVIRONMENT BUILDER
    case 'build_environment':
      return await handleEnvironmentTools(action, args, tools);

    // 9. SYSTEM CONTROL
    case 'system_control': {
      if (action === 'console_command') return await handleConsoleCommand(args, tools);
      if (action === 'run_ubt') return await handlePipelineTools(action, args, tools);

      // Bridge forwards
      if (action === 'run_tests') return cleanObject(await executeAutomationRequest(tools, 'manage_tests', { ...args, subAction: action }, 'Bridge unavailable'));
      if (action === 'subscribe' || action === 'unsubscribe') return cleanObject(await executeAutomationRequest(tools, 'manage_logs', { ...args, subAction: action }, 'Bridge unavailable'));
      if (action === 'spawn_category') return cleanObject(await executeAutomationRequest(tools, 'manage_debug', { ...args, subAction: action }, 'Bridge unavailable'));
      if (action === 'start_session') return cleanObject(await executeAutomationRequest(tools, 'manage_insights', { ...args, subAction: action }, 'Bridge unavailable'));
      if (action === 'lumen_update_scene') return cleanObject(await executeAutomationRequest(tools, 'manage_render', { ...args, subAction: action }, 'Bridge unavailable'));

      return await handleSystemTools(action, args, tools);
    }

    // 10. SEQUENCER
    case 'manage_sequence':
      return await handleSequenceTools(action, args, tools);

    // 11. INTROSPECTION
    case 'inspect':
      return await handleInspectTools(action, args, tools);

    // 12. AUDIO
    case 'manage_audio':
      return await handleAudioTools(action, args, tools);

    // 13. BEHAVIOR TREE
    case 'manage_behavior_tree':
      return await handleGraphTools('manage_behavior_tree', action, args, tools);

    // 14. BLUEPRINT GRAPH DIRECT
    case 'manage_blueprint_graph':
      return await handleGraphTools('manage_blueprint_graph', action, args, tools);

    // 15. RENDER TOOLS
    case 'manage_render':
      return cleanObject(await executeAutomationRequest(tools, 'manage_render', { ...args, subAction: action }, 'Bridge unavailable'));

    // 16. WORLD PARTITION
    case 'manage_world_partition':
      return cleanObject(await executeAutomationRequest(tools, 'manage_world_partition', { ...args, subAction: action }, 'Bridge unavailable'));

    // 17. LIGHTING
    case 'manage_lighting':
      return await handleLightingTools(action, args, tools);

    // 18. PERFORMANCE
    case 'manage_performance':
      return await handlePerformanceTools(action, args, tools);

    // 19. INPUT
    case 'manage_input':
      return await handleInputTools(action, args, tools);

    default:
      return cleanObject({ success: false, error: 'UNKNOWN_TOOL', message: `Unknown consolidated tool: ${name}` });
  }
}

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
    const action = normalized.action;
    const normalizedArgs = normalized.args;

    const toolDef: any = (consolidatedToolDefinitions as any[]).find(t => t.name === normalizedName);
    const dynamicHandler = await getDynamicHandlerForTool(normalizedName, action);

    if (!dynamicHandler && toolDef && toolDef.inputSchema && toolDef.inputSchema.properties && toolDef.inputSchema.properties.action) {
      const actionProp: any = toolDef.inputSchema.properties.action;
      const allowed: string[] | undefined = Array.isArray(actionProp.enum) ? actionProp.enum as string[] : undefined;
      if (allowed && !allowed.includes(action)) {
        return cleanObject({
          success: false,
          error: 'UNKNOWN_ACTION',
          message: `Unknown action '${action}' for consolidated tool '${normalizedName}'.`
        });
      }
    }
    const defaultHandler = async () => invokeNamedTool(normalizedName, action, normalizedArgs, tools);

    if (dynamicHandler) {
      return await dynamicHandler({ name: normalizedName, action, args: normalizedArgs, tools, defaultHandler });
    }

    return await defaultHandler();
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