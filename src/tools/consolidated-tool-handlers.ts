import { cleanObject } from '../utils/safe-json.js';
import { Logger } from '../utils/logger.js';
import { ITools } from '../types/tool-interfaces.js';


const log = new Logger('ConsolidatedToolHandler');

// Helper functions
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

async function executeAutomationRequest(
  tools: ITools,
  toolName: string,
  args: any,
  errorMessage: string = 'Automation bridge not available'
) {
  const automationBridge = tools.automationBridge;
  if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
    throw new Error(errorMessage);
  }
  return await automationBridge.sendAutomationRequest(toolName, args);
}

// Export the main consolidated tool call handler
export async function handleConsolidatedToolCall(
  name: string,
  args: any,
  tools: ITools
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
          case 'duplicate': {
            const res = await tools.assetTools.duplicateAsset({
              sourcePath: args.sourcePath,
              destinationPath: args.destinationPath
            });
            return cleanObject(res);
          }
          case 'rename': {
            const res = await tools.assetTools.renameAsset({
              sourcePath: args.sourcePath,
              destinationPath: args.destinationPath
            });
            return cleanObject(res);
          }
          case 'move': {
            const res = await tools.assetTools.moveAsset({
              sourcePath: args.sourcePath,
              destinationPath: args.destinationPath
            });
            return cleanObject(res);
          }
          case 'delete': {
            const res = await tools.assetTools.deleteAssets({
              paths: args.paths
            });
            return cleanObject(res);
          }
          default:
            // Fallback to direct bridge call for other asset actions if needed, or error
            return await executeAutomationRequest(tools, 'manage_asset', args);
        }
      }

      case 'control_actor': {
        switch (action) {
          case 'spawn': {
            const classPath = requireNonEmptyString(args.classPath, 'classPath', 'Invalid classPath: must be a non-empty string');
            const res = await tools.actorTools.spawn({
              classPath,
              actorName: args.actorName,
              location: args.location,
              rotation: args.rotation
            });
            return cleanObject(res);
          }
          case 'delete': {
            const actorName = requireNonEmptyString(args.actorName || args.name, 'actorName', 'Invalid actorName');
            const res = await tools.actorTools.delete({ actorName });
            return cleanObject(res);
          }
          case 'apply_force': {
            const actorName = requireNonEmptyString(args.actorName, 'actorName');
            const res = await tools.actorTools.applyForce({
              actorName,
              force: args.force
            });
            return cleanObject(res);
          }
          case 'set_transform': {
            const actorName = requireNonEmptyString(args.actorName, 'actorName');
            const res = await tools.actorTools.setTransform({
              actorName,
              location: args.location,
              rotation: args.rotation,
              scale: args.scale
            });
            return cleanObject(res);
          }
          case 'get_transform': {
            const actorName = requireNonEmptyString(args.actorName, 'actorName');
            const res = await tools.actorTools.getTransform(actorName);
            return cleanObject(res);
          }
          default:
            // Fallback to direct bridge call or error
            return await executeAutomationRequest(tools, 'control_actor', args);
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
            return await executeAutomationRequest(tools, 'control_editor', args);
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
            return await executeAutomationRequest(tools, 'manage_level', args);
        }
      }

      case 'manage_blueprint': {
        switch (action) {
          case 'create': {
            const res = await tools.blueprintTools.createBlueprint({
              name: args.name,
              blueprintType: args.blueprintType,
              savePath: args.savePath,
              parentClass: args.parentClass,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'ensure_exists': {
            const res = await tools.blueprintTools.waitForBlueprint(args.name, args.timeoutMs);
            return cleanObject(res);
          }
          case 'add_variable': {
            const res = await tools.blueprintTools.addVariable({
              blueprintName: args.name,
              variableName: args.variableName,
              variableType: args.variableType,
              defaultValue: args.defaultValue,
              category: args.category,
              isReplicated: args.isReplicated,
              isPublic: args.isPublic,
              variablePinType: args.variablePinType,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'set_variable_metadata': {
            const res = await tools.blueprintTools.setVariableMetadata({
              blueprintName: args.name,
              variableName: args.variableName,
              metadata: args.metadata,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'add_event': {
            const res = await tools.blueprintTools.addEvent({
              blueprintName: args.name,
              eventType: args.eventType,
              customEventName: args.customEventName,
              parameters: args.parameters,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'remove_event': {
            const res = await tools.blueprintTools.removeEvent({
              blueprintName: args.name,
              eventName: args.eventName,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'add_function': {
            const res = await tools.blueprintTools.addFunction({
              blueprintName: args.name,
              functionName: args.functionName,
              inputs: args.inputs,
              outputs: args.outputs,
              isPublic: args.isPublic,
              category: args.category,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'add_component': {
            const res = await tools.blueprintTools.addComponent({
              blueprintName: args.name,
              componentType: args.componentType,
              componentName: args.componentName,
              attachTo: args.attachTo,
              transform: args.transform,
              properties: args.properties,
              compile: args.applyAndSave,
              save: args.applyAndSave,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'modify_scs': {
            const res = await tools.blueprintTools.modifyConstructionScript({
              blueprintPath: args.name,
              operations: args.operations,
              compile: args.applyAndSave,
              save: args.applyAndSave,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'set_scs_transform': {
            const res = await tools.blueprintTools.setSCSComponentTransform({
              blueprintPath: args.name,
              componentName: args.componentName,
              location: args.location,
              rotation: args.rotation,
              scale: args.scale,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'add_construction_script': {
            const res = await tools.blueprintTools.addConstructionScript({
              blueprintName: args.name,
              scriptName: args.scriptName,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'add_node': {
            const res = await tools.blueprintTools.addNode({
              blueprintName: args.name,
              nodeType: args.nodeType,
              graphName: args.graphName,
              functionName: args.functionName,
              variableName: args.variableName,
              nodeName: args.nodeName,
              posX: args.posX,
              posY: args.posY,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'add_scs_component': {
            const res = await tools.blueprintTools.addSCSComponent({
              blueprintPath: args.name,
              componentClass: args.componentType,
              componentName: args.componentName,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'reparent_scs_component': {
            const res = await tools.blueprintTools.reparentSCSComponent({
              blueprintPath: args.name,
              componentName: args.componentName,
              newParent: args.newParent,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'set_scs_property': {
            const res = await tools.blueprintTools.setSCSComponentProperty({
              blueprintPath: args.name,
              componentName: args.componentName,
              propertyName: args.propertyName,
              propertyValue: args.propertyValue,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'remove_scs_component': {
            const res = await tools.blueprintTools.removeSCSComponent({
              blueprintPath: args.name,
              componentName: args.componentName,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'get_scs': {
            const res = await tools.blueprintTools.getBlueprintSCS({
              blueprintPath: args.name,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'set_default': {
            const res = await tools.blueprintTools.setBlueprintDefault({
              blueprintName: args.name,
              propertyName: args.propertyName,
              value: args.value
            });
            return cleanObject(res);
          }
          case 'compile': {
            const res = await tools.blueprintTools.compileBlueprint({
              blueprintName: args.name,
              saveAfterCompile: args.saveAfterCompile
            });
            return cleanObject(res);
          }
          case 'probe_handle': {
            const res = await tools.blueprintTools.probeSubobjectDataHandle({
              componentClass: args.componentClass
            });
            return cleanObject(res);
          }
          case 'get': {
            const res = await tools.blueprintTools.getBlueprintInfo({
              blueprintPath: args.name,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          default: {
            // Translate applyAndSave to compile/save flags for modify_scs action
            const processedArgs = { ...args };
            if (args.action === 'modify_scs' && args.applyAndSave === true) {
              processedArgs.compile = true;
              processedArgs.save = true;
            }
            const res = await executeAutomationRequest(tools, 'manage_blueprint', processedArgs, 'Automation bridge not available for blueprint operations');
            return cleanObject(res);
          }
        }
      }

      case 'blueprint_get': {
        const res = await executeAutomationRequest(tools, 'blueprint_get', args, 'Automation bridge not available for blueprint operations');
        return cleanObject(res);
      }

      case 'manage_sequence': {
        const seqAction = String(action || '').trim();
        switch (seqAction) {
          case 'create': {
            const name = requireNonEmptyString(args.name, 'name', 'Missing required parameter: name');
            const res = await tools.sequenceTools.create({ name, path: args.path });
            return cleanObject(res);
          }
          case 'open': {
            const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
            const res = await tools.sequenceTools.open({ path });
            return cleanObject(res);
          }
          case 'add_camera': {
            const res = await tools.sequenceTools.addCamera({ spawnable: args.spawnable });
            return cleanObject(res);
          }
          case 'add_actor': {
            const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
            const res = await tools.sequenceTools.addActor({ actorName, createBinding: args.createBinding });
            return cleanObject(res);
          }
          case 'add_actors': {
            const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames : [];
            const res = await tools.sequenceTools.addActors({ actorNames });
            return cleanObject(res);
          }
          case 'remove_actors': {
            const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames : [];
            const res = await tools.sequenceTools.removeActors({ actorNames });
            return cleanObject(res);
          }
          case 'get_bindings': {
            const res = await tools.sequenceTools.getBindings({ path: args.path });
            return cleanObject(res);
          }
          case 'add_spawnable_from_class': {
            const className = requireNonEmptyString(args.className, 'className', 'Missing required parameter: className');
            const res = await tools.sequenceTools.addSpawnableFromClass({ className, path: args.path });
            return cleanObject(res);
          }
          case 'play': {
            const res = await tools.sequenceTools.play({ startTime: args.startTime, loopMode: args.loopMode });
            return cleanObject(res);
          }
          case 'pause': {
            const res = await tools.sequenceTools.pause();
            return cleanObject(res);
          }
          case 'stop': {
            const res = await tools.sequenceTools.stop();
            return cleanObject(res);
          }
          case 'set_properties': {
            const res = await tools.sequenceTools.setSequenceProperties({
              path: args.path,
              frameRate: args.frameRate,
              lengthInFrames: args.lengthInFrames,
              playbackStart: args.playbackStart,
              playbackEnd: args.playbackEnd
            });
            return cleanObject(res);
          }
          case 'get_properties': {
            const res = await tools.sequenceTools.getSequenceProperties({ path: args.path });
            return cleanObject(res);
          }
          case 'set_playback_speed': {
            const speed = Number(args.speed);
            if (!Number.isFinite(speed) || speed <= 0) {
              throw new Error('Invalid speed: must be a positive number');
            }
            const res = await tools.sequenceTools.setPlaybackSpeed({ speed });
            return cleanObject(res);
          }
          case 'list': {
            const res = await tools.sequenceTools.list({ path: args.path });
            return cleanObject(res);
          }
          case 'duplicate': {
            const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
            const destDir = requireNonEmptyString(args.destinationPath, 'destinationPath', 'Missing required parameter: destinationPath');
            const newName = requireNonEmptyString(args.newName || path.split('/').pop(), 'newName', 'Missing required parameter: newName');
            const baseDir = destDir.replace(/\/$/, '');
            const destPath = `${baseDir}/${newName}`;
            const res = await tools.sequenceTools.duplicate({ path, destinationPath: destPath });
            return cleanObject(res);
          }
          case 'rename': {
            const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
            const newName = requireNonEmptyString(args.newName, 'newName', 'Missing required parameter: newName');
            const res = await tools.sequenceTools.rename({ path, newName });
            return cleanObject(res);
          }
          case 'delete': {
            const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
            const res = await tools.sequenceTools.deleteSequence({ path });
            return cleanObject(res);
          }
          case 'get_metadata': {
            const res = await tools.sequenceTools.getMetadata({ path: args.path });
            return cleanObject(res);
          }
          default:
            return await executeAutomationRequest(tools, 'manage_sequence', args);
        }
      }

      case 'animation_physics': {
        const res = await executeAutomationRequest(tools, 'animation_physics', args, 'Automation bridge not available for animation/physics operations');
        return cleanObject(res);
      }

      case 'create_effect': {
        const res = await executeAutomationRequest(tools, 'create_effect', args, 'Automation bridge not available for effect creation operations');
        return cleanObject(res);
      }

      case 'build_environment': {
        const res = await executeAutomationRequest(tools, 'build_environment', args, 'Automation bridge not available for environment building operations');
        return cleanObject(res);
      }

      case 'system_control': {
        const res = await executeAutomationRequest(tools, 'system_control', args, 'Automation bridge not available for system control operations');
        return cleanObject(res);
      }

      case 'console_command': {
        const res = await executeAutomationRequest(tools, 'console_command', args, 'Automation bridge not available for console command operations');
        return cleanObject(res);
      }

      case 'inspect': {
        const res = await executeAutomationRequest(tools, 'inspect', args, 'Automation bridge not available for inspect operations');
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


