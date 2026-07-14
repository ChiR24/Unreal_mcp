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
  widgetAuthoringActionSet,
  dataActionSet
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
import { handleDataTools } from '../handlers/data/data-handlers.js';
import { handleProjectSettingsTools } from '../handlers/core/project-settings-handlers.js';

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

export function registerDefaultHandlers() {
  toolRegistry.register('manage_asset', async (args, tools) => {
    const action = getToolAction(args);
    if (materialAuthoringActionSet.has(action)) return await handleMaterialAuthoringTools(action, args, tools);
    if (textureActionSet.has(action)) return await handleTextureTools(action, args, tools);
    if (action === 'nanite_rebuild_mesh') {
      const payload = { ...args, subAction: action };
      return cleanObject(await executeAutomationRequest(tools, 'manage_render', payload, `Automation bridge not available for ${action}`));
    }
    if (isMaterialGraphAction(action)) {
      const subAction = resolveMaterialGraphSubAction(action);
      return await handleGraphTools('manage_material_graph', subAction, args, tools);
    }
    if (isBehaviorTreeGraphAction(action)) {
      const subAction = resolveBehaviorTreeGraphSubAction(action);
      return await handleGraphTools('manage_behavior_tree', subAction, args, tools);
    }
    return await handleAssetTools(action, args, tools);
  });

  toolRegistry.register('manage_blueprint', async (args, tools) => {
    const action = getToolAction(args);
    if (action === 'create_blueprint') return await handleBlueprintTools('create', args, tools);
    if (action === 'get_blueprint') return await handleBlueprintGet(args, tools);
    if (widgetAuthoringActionSet.has(action)) return await handleWidgetAuthoringTools(action, args, tools);
    if (blueprintGraphActionSet.has(action)) return await handleGraphTools('manage_blueprint', action, args, tools);
    return await handleBlueprintTools(action, args, tools);
  });

  toolRegistry.register('control_actor', async (args, tools) => await handleActorTools(getToolAction(args), args, tools));
  toolRegistry.register('control_editor', async (args, tools) => await handleEditorTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_project_settings', async (args, tools) => await handleProjectSettingsTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_level', async (args, tools) => await handleLevelTools(getToolAction(args), args, tools));

  toolRegistry.register('animation_physics', async (args, tools) => {
    const action = getToolAction(args);
    if (skeletonActionSet.has(action)) return await handleSkeletonTools(action, args, tools);
    if (animationAuthoringActionSet.has(action)) return await handleAnimationAuthoringTools(action, args, tools);
    if (action === 'add_notify' && (args.frame !== undefined || args.assetPath !== undefined)) {
      return await handleAnimationAuthoringTools(action, args, tools);
    }
    return await handleAnimationTools(action, args, tools);
  });

  toolRegistry.register('manage_effect', async (args, tools) => await handleEffectTools(getToolAction(args), args, tools));

  toolRegistry.register('build_environment', async (args, tools) => {
    const action = getToolAction(args);
    if (lightingActionSet.has(action)) return await handleLightingTools(action, args, tools);
    if (renderActionSet.has(action)) {
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_render',
        { ...args, subAction: action },
        `Automation bridge not available for ${action}`
      ));
    }
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
    if (action === 'lumen_update_scene') return cleanObject(await executeAutomationRequest(tools, 'manage_render', { ...args, subAction: action }, 'Bridge unavailable'));
    return await handleSystemTools(action, args, tools);
  });

  toolRegistry.register('manage_sequence', async (args, tools) => await handleSequenceTools(getToolAction(args), args, tools));
  toolRegistry.register('inspect', async (args, tools) => await handleInspectTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_tools', async (args, tools) => await handleManageToolsTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_audio', async (args, tools) => {
    const action = getToolAction(args);
    if (audioAuthoringActionSet.has(action)) return await handleAudioAuthoringTools(action, args, tools);
    return await handleAudioTools(action, args, tools);
  });

  toolRegistry.register('manage_geometry', async (args, tools) => await handleGeometryTools(getToolAction(args), args, tools));
  toolRegistry.register('manage_pcg', async (args, tools) => await handlePCGTools(getToolAction(args), args, tools));

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
  toolRegistry.register('manage_data', async (args, tools) => {
    const action = getToolAction(args);
    if (dataActionSet.has(action)) return await handleDataTools(action, args, tools);
    return await handleDataTools(action, args, tools);
  });
}
