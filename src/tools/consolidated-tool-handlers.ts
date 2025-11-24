import { cleanObject } from '../utils/safe-json.js';
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

// Export the main consolidated tool call handler
export async function handleConsolidatedToolCall(
  name: string,
  args: any,
  tools: ITools
) {
  const startTime = Date.now();
  console.log(`Starting execution of ${name} at ${new Date().toISOString()}`);

  try {
    // Special case for console_command which uses 'command' instead of 'action'
    let action = '';
    if (name !== 'console_command') {
      action = requireAction(args);
    }

    switch (name) {
      case 'manage_asset':
        return await handleAssetTools(action, args, tools);

      case 'control_actor':
        return await handleActorTools(action, args, tools);

      case 'control_editor':
        return await handleEditorTools(action, args, tools);

      case 'manage_level':
        return await handleLevelTools(action, args, tools);

      case 'manage_blueprint':
        return await handleBlueprintTools(action, args, tools);

      case 'blueprint_get':
        return await handleBlueprintGet(args, tools);

      case 'manage_sequence':
        return await handleSequenceTools(action, args, tools);

      case 'animation_physics':
        return await handleAnimationTools(action, args, tools);

      case 'create_effect':
        return await handleEffectTools(action, args, tools);

      case 'build_environment':
        return await handleEnvironmentTools(action, args, tools);

      case 'system_control':
        return await handleSystemTools(action, args, tools);

      case 'console_command':
        return await handleConsoleCommand(args, tools);

      case 'inspect':
        return await handleInspectTools(action, args, tools);

      case 'manage_world_partition':
      case 'manage_render':
      case 'manage_pipeline':
      case 'manage_tests':
      case 'manage_logs':
      case 'manage_debug':
      case 'manage_insights':
      case 'manage_ui':
      case 'manage_blueprint_graph':
      case 'manage_niagara_graph':
      case 'manage_material_graph':
      case 'manage_behavior_tree': {
        // Forward directly to automation bridge, mapping 'action' to 'subAction'
        const payload = { ...args, subAction: action };
        const res = await executeAutomationRequest(tools, name, payload, `Automation bridge not available for ${name}`);
        return cleanObject(res);
      }

      default:
        return cleanObject({ success: false, error: 'UNKNOWN_TOOL', message: `Unknown consolidated tool: ${name}` });
    }
  } catch (err: any) {
    const duration = Date.now() - startTime;
    console.error(`[ConsolidatedToolHandler] Failed execution of ${name} after ${duration}ms: ${err?.message || String(err)}`);
    const errorMessage = err?.message || String(err);
    const lowerError = errorMessage.toString().toLowerCase();
    const isTimeout = lowerError.includes('timeout');

    let text: string;
    if (isTimeout) {
      text = `Tool ${name} timed out. Please check Unreal Engine connection.`;
    } else {
      if (name === 'manage_asset') {
        const actionRaw = (args && typeof (args as any).action === 'string') ? (args as any).action : '';
        const actionLower = actionRaw.toLowerCase();
        if (actionLower === 'import' && (lowerError === 'import_failed' || lowerError.includes('import failed'))) {
          text = 'Import error: unsupported_format (IMPORT_FAILED)';
        } else {
          text = `Failed to execute ${name}: ${errorMessage}`;
        }
      } else {
        text = `Failed to execute ${name}: ${errorMessage}`;
      }
    }

    return {
      content: [
        {
          type: 'text',
          text
        }
      ],
      isError: true
    };
  }
}
