import { cleanObject } from '../../utils/serialization/safe-json.js';
import { toolRegistry } from './dynamic-handler-registry.js';
import {
  animationAuthoringActionSet,
  audioAuthoringActionSet,
  behaviorTreeActionSet,
  blueprintGraphActionSet,
  gameFrameworkActionSet,
  getToolAction,
  inputActionSet,
  isBehaviorTreeGraphAction,
  isMaterialGraphAction,
  lightingActionSet,
  materialAuthoringActionSet,
  navigationActionSet,
  renderActionSet,
  performanceActionSet,
  resolveBehaviorTreeGraphSubAction,
  resolveMaterialGraphSubAction,
  sessionActionSet,
  skeletonActionSet,
  splineActionSet,
  textureActionSet,
  volumeActionSet,
  widgetAuthoringActionSet
} from './consolidated-routing.js';
import { executeAutomationRequest } from '../handlers/foundation/dispatch/common-handlers.js';
import { handleAITools } from '../handlers/ai/ai-handlers.js';
import { handleActorTools } from '../handlers/actor/actor-handlers.js';
import { handleAnimationAuthoringTools } from '../handlers/animation/authoring/animation-authoring-handlers.js';
import { handleAnimationTools } from '../handlers/animation/runtime/animation-handlers.js';
import { handleAssetTools } from '../handlers/asset/asset-handlers.js';
import { handleAudioAuthoringTools } from '../handlers/audio/authoring/audio-authoring-handlers.js';
import { handleAudioTools } from '../handlers/audio/runtime/audio-handlers.js';
import { handleBlueprintGet, handleBlueprintTools } from '../handlers/blueprint/blueprint-handlers.js';
import { handleCharacterTools } from '../handlers/character/character-handlers.js';
import { handleCombatTools } from '../handlers/combat/combat-handlers.js';
import { handleEditorTools } from '../handlers/editor/editor-handlers.js';
import { handleEffectTools } from '../handlers/effect/effect-handlers.js';
import { handleEnvironmentTools } from '../handlers/environment/environment-handlers.js';
import { handleGASTools } from '../handlers/gas/gas-handlers.js';
import { handleGameFrameworkTools } from '../handlers/game-framework/game-framework-handlers.js';
import { handleGeometryTools } from '../handlers/geometry/geometry-handlers.js';
import { handleGraphTools } from '../handlers/graph/graph-handlers.js';
import { handleInputTools } from '../handlers/input/input-handlers.js';
import { handleInspectTools } from '../handlers/inspect/inspect-handlers.js';
import { handleInteractionTools } from '../handlers/interaction/interaction-handlers.js';
import { handleInventoryTools } from '../handlers/inventory/inventory-handlers.js';
import { handleLevelStructureTools } from '../handlers/level/structure/level-structure-handlers.js';
import { handleLevelTools } from '../handlers/level/runtime/level-handlers.js';
import { handleLightingTools } from '../handlers/lighting/lighting-handlers.js';
import { handleManageToolsTools } from '../handlers/tools/manage-tools-handlers.js';
import { handleMaterialAuthoringTools } from '../handlers/material/material-authoring-handlers.js';
import { handleNavigationTools } from '../handlers/navigation/navigation-handlers.js';
import { handleNetworkingTools } from '../handlers/networking/networking-handlers.js';
import { handlePCGTools } from '../handlers/pcg/pcg-handlers.js';
import { handlePerformanceTools } from '../handlers/performance/performance-handlers.js';
import { handlePipelineTools } from '../handlers/pipeline/pipeline-handlers.js';
import { handleSequenceTools } from '../handlers/sequence/sequence-handlers.js';
import { handleSessionsTools } from '../handlers/sessions/sessions-handlers.js';
import { handleSkeletonTools } from '../handlers/skeleton/skeleton-handlers.js';
import { handleSplineTools } from '../handlers/spline/spline-handlers.js';
import { handleSystemTools, handleConsoleCommand } from '../handlers/system/system-handlers.js';
import { handleTextureTools } from '../handlers/texture/texture-handlers.js';
import { handleVolumeTools } from '../handlers/volume/volume-handlers.js';
import { handleWidgetAuthoringTools } from '../handlers/widget/widget-authoring-handlers.js';
import { GENERATED_PARENT_ROUTING } from './generated-routing-index.generated.js';
import type { ITools } from '../../types/tools/tool-interfaces.js';

type ParentHandler = (args: Record<string, unknown>, tools: ITools) => Promise<unknown>;

function mergeAutomationResponse(
  response: unknown,
  fields: Record<string, unknown>
): Record<string, unknown> {
  return Object.assign({}, response, fields);
}

const insightsActionSet = new Set<string>([
  'start_session',
  'start_unreal_insights',
  'capture_insights_trace',
  'get_trace_status',
  'pause_session',
  'resume_session',
  'stop_session',
  'write_snapshot',
  'send_snapshot',
  'analyze_trace'
]);

// Per-parent dispatch closures (private routing logic, preserved as source).
// The parent name -> handlerKey mapping is generated; this map turns each
// handlerKey into the actual dispatch closure.
const PARENT_DISPATCH: Record<string, ParentHandler> = {
  asset: async (args, tools) => {
    const action = getToolAction(args);
    if (materialAuthoringActionSet.has(action)) return handleMaterialAuthoringTools(action, args, tools) as Promise<unknown>;
    if (textureActionSet.has(action)) return handleTextureTools(action, args, tools) as Promise<unknown>;
    if (action === 'nanite_rebuild_mesh') {
      const payload = { ...args, subAction: action };
      return cleanObject(await executeAutomationRequest(tools, 'manage_render', payload, `Automation bridge not available for ${action}`)) as Promise<unknown>;
    }
    if (isMaterialGraphAction(action)) {
      const subAction = resolveMaterialGraphSubAction(action);
      return handleGraphTools('manage_material_graph', subAction, args, tools) as Promise<unknown>;
    }
    if (isBehaviorTreeGraphAction(action)) {
      const subAction = resolveBehaviorTreeGraphSubAction(action);
      return handleGraphTools('manage_behavior_tree', subAction, args, tools) as Promise<unknown>;
    }
    return handleAssetTools(action, args, tools) as Promise<unknown>;
  },
  blueprint: async (args, tools) => {
    const action = getToolAction(args);
    if (action === 'create_blueprint') return handleBlueprintTools('create', args, tools) as Promise<unknown>;
    if (action === 'get_blueprint') return handleBlueprintGet(args, tools) as Promise<unknown>;
    if (widgetAuthoringActionSet.has(action)) return handleWidgetAuthoringTools(action, args, tools) as Promise<unknown>;
    if (blueprintGraphActionSet.has(action)) return handleGraphTools('manage_blueprint', action, args, tools) as Promise<unknown>;
    return handleBlueprintTools(action, args, tools) as Promise<unknown>;
  },
  actor: (args, tools) => handleActorTools(getToolAction(args), args, tools) as Promise<unknown>,
  editor: (args, tools) => handleEditorTools(getToolAction(args), args, tools) as Promise<unknown>,
  level: (args, tools) => handleLevelTools(getToolAction(args), args, tools) as Promise<unknown>,
  animation: async (args, tools) => {
    const action = getToolAction(args);
    if (skeletonActionSet.has(action)) return handleSkeletonTools(action, args, tools) as Promise<unknown>;
    if (animationAuthoringActionSet.has(action)) return handleAnimationAuthoringTools(action, args, tools) as Promise<unknown>;
    if (action === 'add_notify' && (args.frame !== undefined || args.assetPath !== undefined)) {
      return handleAnimationAuthoringTools(action, args, tools) as Promise<unknown>;
    }
    return handleAnimationTools(action, args, tools) as Promise<unknown>;
  },
  effect: (args, tools) => handleEffectTools(getToolAction(args), args, tools) as Promise<unknown>,
  environment: async (args, tools) => {
    const action = getToolAction(args);
    if (lightingActionSet.has(action)) return handleLightingTools(action, args, tools) as Promise<unknown>;
    if (renderActionSet.has(action)) {
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_render',
        { ...args, subAction: action },
        `Automation bridge not available for ${action}`
      )) as Promise<unknown>;
    }
    if (splineActionSet.has(action)) return handleSplineTools(action, args, tools) as Promise<unknown>;
    return handleEnvironmentTools(action, args, tools) as Promise<unknown>;
  },
  system: async (args, tools) => {
    const action = getToolAction(args);
    if (action === 'console_command') return handleConsoleCommand(args, tools) as Promise<unknown>;
    if (action === 'run_ubt') return handlePipelineTools(action, args, tools) as Promise<unknown>;
    if (performanceActionSet.has(action)) return handlePerformanceTools(action, args, tools) as Promise<unknown>;
    if (action === 'run_tests') return cleanObject(await executeAutomationRequest(tools, 'manage_tests', { ...args, subAction: action }, 'Bridge unavailable')) as Promise<unknown>;
    if (action === 'subscribe' || action === 'unsubscribe') {
      return cleanObject(await executeAutomationRequest(tools, 'manage_logs', { ...args, subAction: action }, 'Bridge unavailable')) as Promise<unknown>;
    }
    if (action === 'spawn_category') {
      const categoryName = typeof args.categoryName === 'string'
        ? args.categoryName.trim()
        : (typeof args.category === 'string' ? args.category.trim() : 'AI');
      if (!/^[A-Za-z0-9_-]+$/.test(categoryName)) {
        return { success: false, error: 'INVALID_CATEGORY_NAME', message: 'Category names may only contain letters, numbers, underscores, and hyphens.' };
      }
      const response = await executeAutomationRequest(tools, 'manage_debug', { ...args, subAction: action, categoryName }, 'Bridge unavailable');
      return cleanObject(mergeAutomationResponse(response, { action, categoryName }));
    }
    if (insightsActionSet.has(action)) {
      const channels = typeof args.channels === 'string' ? args.channels.trim() : '';
      if (channels && !/^[A-Za-z0-9_, -]+$/.test(channels)) {
        return { success: false, error: 'INVALID_CHANNELS', message: 'Trace channels contain unsupported characters.' };
      }
      const payload = channels ? {
        ...args,
        action,
        subAction: action,
        channels
      } : {
        ...args,
        action,
        subAction: action
      };
      const response = await executeAutomationRequest(tools, 'manage_insights', payload, 'Bridge unavailable');
      const metadata = channels ? { action, channels, sessionType: 'trace' } : { action, sessionType: 'trace' };
      return cleanObject(mergeAutomationResponse(response, metadata));
    }
    if (action === 'lumen_update_scene') return cleanObject(await executeAutomationRequest(tools, 'manage_render', { ...args, subAction: action }, 'Bridge unavailable')) as Promise<unknown>;
    return handleSystemTools(action, args, tools) as Promise<unknown>;
  },
  sequence: (args, tools) => handleSequenceTools(getToolAction(args), args, tools) as Promise<unknown>,
  inspect: (args, tools) => handleInspectTools(getToolAction(args), args, tools) as Promise<unknown>,
  tools: (args, tools) => handleManageToolsTools(getToolAction(args), args, tools) as Promise<unknown>,
  audio: async (args, tools) => {
    const action = getToolAction(args);
    if (audioAuthoringActionSet.has(action)) return handleAudioAuthoringTools(action, args, tools) as Promise<unknown>;
    return handleAudioTools(action, args, tools) as Promise<unknown>;
  },
  geometry: (args, tools) => handleGeometryTools(getToolAction(args), args, tools) as Promise<unknown>,
  pcg: (args, tools) => handlePCGTools(getToolAction(args), args, tools) as Promise<unknown>,
  gas: (args, tools) => handleGASTools(getToolAction(args), args, tools) as Promise<unknown>,
  character: (args, tools) => handleCharacterTools(getToolAction(args), args, tools) as Promise<unknown>,
  combat: (args, tools) => handleCombatTools(getToolAction(args), args, tools) as Promise<unknown>,
  ai: async (args, tools) => {
    const action = getToolAction(args);
    if (behaviorTreeActionSet.has(action)) return handleGraphTools('manage_behavior_tree', action, args, tools) as Promise<unknown>;
    if (navigationActionSet.has(action)) return handleNavigationTools(action, args, tools) as Promise<unknown>;
    return handleAITools(action, args, tools) as Promise<unknown>;
  },
  inventory: (args, tools) => handleInventoryTools(getToolAction(args), args, tools) as Promise<unknown>,
  interaction: (args, tools) => handleInteractionTools(getToolAction(args), args, tools) as Promise<unknown>,
  networking: async (args, tools) => {
    const action = getToolAction(args);
    if (sessionActionSet.has(action)) return handleSessionsTools(action, args, tools) as Promise<unknown>;
    if (gameFrameworkActionSet.has(action)) return handleGameFrameworkTools(action, args, tools) as Promise<unknown>;
    if (inputActionSet.has(action)) return handleInputTools(action, args, tools) as Promise<unknown>;
    return handleNetworkingTools(action, args, tools) as Promise<unknown>;
  },
  levelStructure: async (args, tools) => {
    const action = getToolAction(args);
    if (volumeActionSet.has(action)) return handleVolumeTools(action, args, tools) as Promise<unknown>;
    return handleLevelStructureTools(action, args, tools) as Promise<unknown>;
  },
};

export function registerDefaultHandlers(): void {
  // Registration is driven by the generated routing index (Task 23): the
  // parent name -> handlerKey map is generated; the dispatch closures above
  // are private routing logic. No hand-written schema/action table remains.
  for (const entry of GENERATED_PARENT_ROUTING) {
    const dispatch = PARENT_DISPATCH[entry.handlerKey];
    if (!dispatch) {
      throw new Error(`No parent dispatch registered for handlerKey '${entry.handlerKey}' (tool '${entry.name}')`);
    }
    toolRegistry.register(entry.name, (args, tools) => dispatch(args, tools));
  }
}
