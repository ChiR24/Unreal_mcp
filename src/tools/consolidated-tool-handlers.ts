import { cleanObject } from '../utils/safe-json.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('ConsolidatedToolHandler');

// Helper functions (extracted from original)
function ensureArgsPresent(args: any) {
  if (args === null || args === undefined) {
    throw new Error('Invalid arguments: null or undefined');
  }
}

function requireAction(args: any): string {
  ensureArgsPresent(args);
  const action = args.action;
  if (typeof action !== 'string' || action.trim() === '') {
    throw new Error('Missing required parameter: action');
  }
  return action;
}

function requireNonEmptyString(value: any, field: string, message?: string): string {
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error(message ?? `Invalid ${field}: must be a non-empty string`);
  }
  return value;
}

function getAutomationBridgeInfo(tools: any) {
  const automationBridge = tools?.automationBridge;
  const canSend = Boolean(
    automationBridge && typeof automationBridge.sendAutomationRequest === 'function'
  );
  let isConnected = false;
  if (canSend && typeof automationBridge.isConnected === 'function') {
    try {
      isConnected = Boolean(automationBridge.isConnected());
    } catch {
      isConnected = false;
    }
  }

  return {
    instance: automationBridge,
    canSend,
    isConnected
  };
}

// Export the main consolidated tool call handler
export async function handleConsolidatedToolCall(
  name: string,
  args: any,
  tools: any
) {
  const startTime = Date.now();
  log.debug(`Starting execution of ${name} at ${new Date().toISOString()}`);
  
  try {
    const action = requireAction(args);
    
    switch (name) {
      case 'manage_asset': {
        switch (action) {
          case 'list': {
            const res = await tools.assetResources.list(args.directory || '/Game', false);
            return cleanObject(res);
          }
          case 'create_folder': {
            if (typeof args.path !== 'string' || args.path.trim() === '') {
              throw new Error('Invalid path: must be a non-empty string');
            }
            const normalizedPath = args.path.trim();
            const res = await tools.assetTools.createFolder(normalizedPath);
            return cleanObject(res);
          }
          case 'import': {
            const sourcePath = typeof args.sourcePath === 'string' ? args.sourcePath.trim() : '';
            const destinationPath = typeof args.destinationPath === 'string' ? args.destinationPath.trim() : '';

            if (!sourcePath || !destinationPath) {
              throw new Error('Both sourcePath and destinationPath are required for import action');
            }

            const res = await tools.assetTools.importAsset({
              sourcePath,
              destinationPath,
              overwrite: args.overwrite === true,
              save: args.save !== false
            });
            return cleanObject(res);
          }
          default:
            return cleanObject({ success: false, error: 'UNKNOWN_ASSET_ACTION', message: `Asset action not implemented: ${String(args.action)}` });
        }
      }
        
      case 'control_actor': {
        const automationInfo = getAutomationBridgeInfo(tools);
        const automationBridge = automationInfo.instance;
        if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
          throw new Error('Automation bridge not connected for control_actor');
        }
        
        switch (action) {
          case 'spawn': {
            const classPath = requireNonEmptyString(args.classPath, 'classPath', 'Invalid classPath: must be a non-empty string');
            return await automationBridge.sendAutomationRequest('control_actor', {
              action: 'spawn',
              classPath,
              actorName: args.actorName,
              location: args.location,
              rotation: args.rotation
            });
          }
          case 'delete': {
            const actorName = requireNonEmptyString(args.actorName || args.name, 'actorName', 'Invalid actorName');
            return await automationBridge.sendAutomationRequest('control_actor', {
              action: 'delete',
              actorName
            });
          }
          default:
            return cleanObject({ success: false, error: 'NOT_IMPLEMENTED', message: `Actor action not implemented: ${action}` });
        }
      }
        
      case 'control_editor': {
        switch (action) {
          case 'play': {
            const res = await tools.editorTools.playInEditor();
            return cleanObject(res);
          }
          case 'stop': {
            const res = await tools.editorTools.stopPlayInEditor();
            return cleanObject(res);
          }
          case 'pause': {
            const res = await tools.editorTools.pausePlayInEditor();
            return cleanObject(res);
          }
          case 'screenshot': {
            const res = await tools.editorTools.takeScreenshot(args.filename);
            return cleanObject(res);
          }
          default:
            return cleanObject({ success: false, error: 'NOT_IMPLEMENTED', message: `Editor action not implemented: ${action}` });
        }
      }
        
      case 'manage_level': {
        switch (action) {
          case 'load':
          case 'load_level': {
            const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');
            const res = await tools.levelTools.loadLevel({ levelPath, streaming: !!args.streaming });
            return cleanObject(res);
          }
          default:
            return cleanObject({ success: false, error: 'NOT_IMPLEMENTED', message: `Level action not implemented: ${action}` });
        }
      }
        
      case 'manage_blueprint': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for blueprint operations' });
        }

        // Translate applyAndSave to compile/save flags for modify_scs action
        const processedArgs = { ...args };
        if (args.action === 'modify_scs' && args.applyAndSave === true) {
          processedArgs.compile = true;
          processedArgs.save = true;
        }

        const res = await automationInfo.instance.sendAutomationRequest('manage_blueprint', processedArgs);
        return cleanObject(res);
      }
        
      case 'blueprint_get': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for blueprint operations' });
        }
        
        const res = await automationInfo.instance.sendAutomationRequest('blueprint_get', args);
        return cleanObject(res);
      }
        
      case 'manage_sequence': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for sequence operations' });
        }
        
        const res = await automationInfo.instance.sendAutomationRequest('manage_sequence', args);
        return cleanObject(res);
      }
        
      case 'animation_physics': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for animation/physics operations' });
        }
        
        const res = await automationInfo.instance.sendAutomationRequest('animation_physics', args);
        return cleanObject(res);
      }
        
      case 'create_effect': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for effect creation operations' });
        }
        
        const res = await automationInfo.instance.sendAutomationRequest('create_effect', args);
        return cleanObject(res);
      }
        
      case 'build_environment': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for environment building operations' });
        }
        
        const res = await automationInfo.instance.sendAutomationRequest('build_environment', args);
        return cleanObject(res);
      }
        
      case 'system_control': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for system control operations' });
        }
        
        const res = await automationInfo.instance.sendAutomationRequest('system_control', args);
        return cleanObject(res);
      }
        
      case 'console_command': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for console command operations' });
        }
        
        const res = await automationInfo.instance.sendAutomationRequest('console_command', args);
        return cleanObject(res);
      }
        
      case 'inspect': {
        const automationInfo = getAutomationBridgeInfo(tools);
        if (!automationInfo.canSend) {
          return cleanObject({ success: false, error: 'NOT_AVAILABLE', message: 'Automation bridge not available for inspect operations' });
        }
        
        const res = await automationInfo.instance.sendAutomationRequest('inspect', args);
        return cleanObject(res);
      }
        
      default:
        return cleanObject({ success: false, error: 'UNKNOWN_TOOL', message: `Unknown consolidated tool: ${name}` });
    }
  } catch (err: any) {
    const duration = Date.now() - startTime;
    log.error(`[ConsolidatedToolHandler] Failed execution of ${name} after ${duration}ms: ${err?.message || String(err)}`);
    const errorMessage = err?.message || String(err);
    const isTimeout = errorMessage.includes('timeout');
    return {
      content: [
        {
          type: 'text',
          text: isTimeout
            ? `Tool ${name} timed out. Please check Unreal Engine connection.`
            : `Failed to execute ${name}: ${errorMessage}`
        }
      ],
      isError: true
    };
  }
}


