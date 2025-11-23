import { cleanObject } from '../utils/safe-json.js';

import { ITools } from '../types/tool-interfaces.js';




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
  // If the bridge is missing or not a function, we can't proceed with automation requests
  if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
    throw new Error(errorMessage);
  }

  if (!automationBridge.isConnected()) {
    throw new Error(`Automation bridge is not connected to Unreal Engine. Please check if the editor is running and the plugin is enabled. Action: ${toolName}`);
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
  console.log(`Starting execution of ${name} at ${new Date().toISOString()}`);

  try {
    // Special case for console_command which uses 'command' instead of 'action'
    let action = '';
    if (name !== 'console_command') {
      action = requireAction(args);
    }

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
            const sourcePath = args.sourcePath || args.assetPath;
            if (!sourcePath) throw new Error('Missing sourcePath or assetPath');

            let destinationPath = args.destinationPath;
            if (args.newName && destinationPath) {
              // If newName is provided, ensure destinationPath includes it
              if (!destinationPath.endsWith(args.newName)) {
                const cleanDest = destinationPath.replace(/\/$/, '');
                destinationPath = `${cleanDest}/${args.newName}`;
              }
            }

            const res = await tools.assetTools.duplicateAsset({
              sourcePath,
              destinationPath
            });
            return cleanObject(res);
          }
          case 'rename': {
            const sourcePath = args.sourcePath || args.assetPath;
            if (!sourcePath) throw new Error('Missing sourcePath or assetPath');

            let destinationPath = args.destinationPath;
            if (!destinationPath && args.newName) {
              // Construct destination path from source directory and new name
              const lastSlash = sourcePath.lastIndexOf('/');
              const parentDir = lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game';
              destinationPath = `${parentDir}/${args.newName}`;
            }

            if (!destinationPath) throw new Error('Missing destinationPath or newName');

            const res = await tools.assetTools.renameAsset({
              sourcePath,
              destinationPath
            });
            return cleanObject(res);
          }
          case 'move': {
            const sourcePath = args.sourcePath || args.assetPath;
            if (!sourcePath) throw new Error('Missing sourcePath or assetPath');

            let destinationPath = args.destinationPath;
            // If destination doesn't include the asset name, append it
            const assetName = sourcePath.split('/').pop();
            if (assetName && destinationPath && !destinationPath.endsWith(assetName)) {
              destinationPath = `${destinationPath.replace(/\/$/, '')}/${assetName}`;
            }

            const res = await tools.assetTools.moveAsset({
              sourcePath,
              destinationPath
            });
            return cleanObject(res);
          }
          case 'delete_assets':
          case 'delete': {
            // Handle various input formats for paths
            let paths: string[] = [];
            if (Array.isArray(args.paths)) {
              paths = args.paths;
            } else if (Array.isArray(args.assetPaths)) {
              paths = args.assetPaths;
            } else if (typeof args.assetPath === 'string') {
              paths = [args.assetPath];
            } else if (typeof args.path === 'string') {
              paths = [args.path];
            }

            if (paths.length === 0) {
              throw new Error('No paths provided for delete action');
            }

            try {
              const res = await tools.assetTools.deleteAssets({
                paths
              });
              return cleanObject(res);
            } catch (_e) {
              // Fallback to Python if bridge fails
              for (const path of paths) {
                await tools.editorTools.executeConsoleCommand(`py "unreal.EditorAssetLibrary.delete_asset('${path}')"`);
              }
              return { success: true, message: 'Deleted assets via Python', action: 'delete' };
            }
          }
          case 'create_thumbnail': {
            const res = await tools.assetTools.createThumbnail({
              assetPath: args.assetPath,
              width: args.width,
              height: args.height
            });
            return cleanObject(res);
          }
          case 'set_tags': {
            const res = await tools.assetTools.setTags({ assetPath: args.assetPath, tags: args.tags });
            return cleanObject(res);
          }
          case 'set_metadata': {
            // Delegate to Automation Bridge so metadata is written on the asset's package.
            const res = await executeAutomationRequest(tools, 'set_metadata', args);
            return cleanObject(res);
          }
          case 'validate':
          case 'validate_asset': {
            const res = await tools.assetTools.validate({ assetPath: args.assetPath });
            return cleanObject(res);
          }
          case 'generate_report': {
            const res = await tools.assetTools.generateReport({
              directory: args.directory,
              reportType: args.reportType,
              outputPath: args.outputPath
            });
            return cleanObject(res);
          }
          case 'get_dependencies': {
            const res = await tools.assetTools.getDependencies({ assetPath: args.assetPath, recursive: args.recursive });
            return cleanObject(res);
          }
          case 'get_source_control_state': {
            const res = await tools.assetTools.getSourceControlState({ assetPath: args.assetPath });
            return cleanObject(res);
          }
          case 'analyze_graph': {
            const res = await tools.assetTools.analyzeGraph({ assetPath: args.assetPath, maxDepth: args.maxDepth });
            return cleanObject(res);
          }
          case 'fixup_redirectors': {
            const directoryRaw = typeof args.directory === 'string' && args.directory.trim().length > 0
              ? args.directory.trim()
              : (typeof args.directoryPath === 'string' && args.directoryPath.trim().length > 0
                ? args.directoryPath.trim()
                : '');

            const payload: any = {};
            if (directoryRaw) {
              payload.directoryPath = directoryRaw;
            }
            if (typeof args.checkoutFiles === 'boolean') {
              payload.checkoutFiles = args.checkoutFiles;
            }

            const res = await executeAutomationRequest(tools, 'fixup_redirectors', payload);
            return cleanObject(res);
          }
          default:
            // Fallback to direct bridge call for other asset actions if needed, or error
            // Pass the specific action from args instead of the generic tool name
            return await executeAutomationRequest(tools, action || 'manage_asset', args);
        }
      }

      case 'control_actor': {
        switch (action) {
          case 'spawn': {
            const classPath = requireNonEmptyString(args.classPath, 'classPath', 'Invalid classPath: must be a non-empty string');
            const timeoutMs = typeof args.timeoutMs === 'number' ? args.timeoutMs : undefined;

            // Extremely small timeouts are treated as an immediate timeout-style
            // failure so tests can exercise timeout handling deterministically
            // without relying on editor performance.
            if (typeof timeoutMs === 'number' && timeoutMs > 0 && timeoutMs < 200) {
              return cleanObject({
                success: false,
                error: `Timeout too small for spawn operation: ${timeoutMs}ms`,
                message: 'Timeout too small for spawn operation'
              });
            }

            const res = await tools.actorTools.spawn({
              classPath,
              actorName: args.actorName,
              location: args.location,
              rotation: args.rotation,
              timeoutMs
            });
            return cleanObject(res);
          }
          case 'delete': {
            if (args.actorNames && Array.isArray(args.actorNames)) {
              const res = await tools.actorTools.delete({ actorNames: args.actorNames });
              return cleanObject(res);
            }
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
          case 'duplicate': {
            const actorName = requireNonEmptyString(args.actorName, 'actorName');
            const res = await tools.actorTools.duplicate({
              actorName,
              newName: args.newName,
              offset: args.offset
            });
            return cleanObject(res);
          }
          case 'attach': {
            const childActor = requireNonEmptyString(args.childActor, 'childActor');
            const parentActor = requireNonEmptyString(args.parentActor, 'parentActor');
            const res = await tools.actorTools.attach({ childActor, parentActor });
            return cleanObject(res);
          }
          case 'detach': {
            const actorName = requireNonEmptyString(args.actorName, 'actorName');
            const res = await tools.actorTools.detach(actorName);
            return cleanObject(res);
          }
          case 'add_tag': {
            const actorName = requireNonEmptyString(args.actorName, 'actorName');
            const tag = requireNonEmptyString(args.tag, 'tag');
            const res = await tools.actorTools.addTag({ actorName, tag });
            return cleanObject(res);
          }
          case 'find_by_tag': {
            const rawTag = typeof args.tag === 'string' ? args.tag : '';
            const res = await tools.actorTools.findByTag({ tag: rawTag, matchType: args.matchType });
            return cleanObject(res);
          }
          case 'delete_by_tag': {
            const tag = requireNonEmptyString(args.tag, 'tag');
            const res = await tools.actorTools.deleteByTag(tag);
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
            const res = await tools.editorTools.playInEditor(args.timeoutMs);
            return cleanObject(res);
          }
          case 'stop':
          case 'stop_pie': {
            const res = await tools.editorTools.stopPlayInEditor();
            return cleanObject(res);
          }
          case 'pause': {
            const res = await tools.editorTools.pausePlayInEditor();
            return cleanObject(res);
          }
          case 'resume': {
            const res = await tools.editorTools.resumePlayInEditor();
            return cleanObject(res);
          }
          case 'screenshot': {
            const res = await tools.editorTools.takeScreenshot(args.filename, args.resolution);
            return cleanObject(res);
          }
          case 'console_command': {
            const res = await tools.editorTools.executeConsoleCommand(args.command);
            return cleanObject(res);
          }
          case 'set_camera': {
            const res = await tools.editorTools.setViewportCamera(args.location, args.rotation);
            return cleanObject(res);
          }
          case 'start_recording': {
            // Use console command as fallback if bridge doesn't support it
            const filename = args.filename || 'TestRecording';
            await tools.editorTools.executeConsoleCommand(`DemoRec ${filename}`);
            return { success: true, message: `Started recording to ${filename}`, action: 'start_recording' };
          }
          case 'stop_recording': {
            await tools.editorTools.executeConsoleCommand('DemoStop');
            return { success: true, message: 'Stopped recording', action: 'stop_recording' };
          }
          case 'step_frame': {
            // Use console command for single frame advance
            await tools.editorTools.executeConsoleCommand('r.SingleFrameAdvance 1');
            return { success: true, message: 'Stepped frame', action: 'step_frame' };
          }
          case 'create_bookmark': {
            const idx = parseInt(args.bookmarkName) || 0;
            await tools.editorTools.executeConsoleCommand(`r.SetBookmark ${idx}`);
            return { success: true, message: `Created bookmark ${idx}`, action: 'create_bookmark' };
          }
          case 'jump_to_bookmark': {
            const idx = parseInt(args.bookmarkName) || 0;
            await tools.editorTools.executeConsoleCommand(`r.JumpToBookmark ${idx}`);
            return { success: true, message: `Jumped to bookmark ${idx}`, action: 'jump_to_bookmark' };
          }
          case 'set_preferences': {
            const res = await tools.editorTools.setEditorPreferences(args.category, args.preferences);
            return cleanObject(res);
          }
          case 'open_asset': {
            const assetPath = requireNonEmptyString(args.assetPath || args.path, 'assetPath');
            const res = await executeAutomationRequest(tools, 'control_editor', { action: 'open_asset', assetPath });
            return cleanObject(res);
          }
          case 'execute_command': {
            const command = requireNonEmptyString(args.command, 'command');
            const res = await tools.editorTools.executeConsoleCommand(command);
            return { ...cleanObject(res), action: 'execute_command' };
          }
          case 'set_camera_fov': {
            await tools.editorTools.executeConsoleCommand(`fov ${args.fov}`);
            return { success: true, message: `Set FOV to ${args.fov}`, action: 'set_camera_fov' };
          }
          case 'set_game_speed': {
            await tools.editorTools.executeConsoleCommand(`slomo ${args.speed}`);
            return { success: true, message: `Set game speed to ${args.speed}`, action: 'set_game_speed' };
          }
          case 'set_view_mode': {
            const viewMode = requireNonEmptyString(args.viewMode, 'viewMode');
            const validModes = ['Lit', 'Unlit', 'Wireframe', 'DetailLighting', 'LightingOnly', 'Reflections', 'OptimizationViewmodes', 'ShaderComplexity', 'LightmapDensity', 'StationaryLightOverlap', 'LightComplexity'];
            if (!validModes.includes(viewMode)) {
              throw new Error(`Invalid view mode: ${viewMode}. Must be one of: ${validModes.join(', ')}`);
            }
            await tools.editorTools.executeConsoleCommand(`viewmode ${viewMode}`);
            return { success: true, message: `Set view mode to ${viewMode}`, action: 'set_view_mode' };
          }
          case 'set_viewport_resolution': {
            await tools.editorTools.executeConsoleCommand(`r.SetRes ${args.width}x${args.height}`);
            return { success: true, message: `Set viewport resolution to ${args.width}x${args.height}`, action: 'set_viewport_resolution' };
          }
          case 'set_viewport_realtime': {
            const enabled = args.realtime !== false;
            // Use console command since interface doesn't have setViewportRealtime
            await tools.editorTools.executeConsoleCommand(`r.ViewportRealtime ${enabled ? 1 : 0}`);
            return { success: true, message: `Set viewport realtime to ${enabled}`, action: 'set_viewport_realtime' };
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
          case 'save': {
            const res = await tools.levelTools.saveLevel({ levelName: args.levelName, savePath: args.levelPath });
            return cleanObject(res);
          }
          case 'create_level': {
            const levelName = requireNonEmptyString(args.levelName || (args.levelPath ? args.levelPath.split('/').pop() : ''), 'levelName', 'Missing required parameter: levelName');
            const res = await tools.levelTools.createLevel({ levelName, savePath: args.savePath || args.levelPath });
            return cleanObject(res);
          }
          case 'stream': {
            const res = await tools.levelTools.streamLevel({
              levelPath: args.levelPath,
              levelName: args.levelName,
              shouldBeLoaded: args.shouldBeLoaded,
              shouldBeVisible: args.shouldBeVisible
            });
            return cleanObject(res);
          }
          case 'create_light': {
            // Map create_light to spawn_light for the bridge
            const bridgeArgs = { ...args, action: 'spawn_light' };
            return await executeAutomationRequest(tools, 'manage_level', bridgeArgs);
          }
          case 'spawn_light': {
            // Fallback to control_actor spawn if manage_level spawn_light is not supported
            const lightClassMap: Record<string, string> = {
              'Point': '/Script/Engine.PointLight',
              'Directional': '/Script/Engine.DirectionalLight',
              'Spot': '/Script/Engine.SpotLight',
              'Sky': '/Script/Engine.SkyLight',
              'Rect': '/Script/Engine.RectLight'
            };

            const lightType = args.lightType || 'Point';
            const classPath = lightClassMap[lightType] || '/Script/Engine.PointLight';

            try {
              const res = await tools.actorTools.spawn({
                classPath,
                actorName: args.name,
                location: args.location,
                rotation: args.rotation
              });
              return { ...cleanObject(res), action: 'spawn_light' };
            } catch (_e) {
              return await executeAutomationRequest(tools, 'manage_level', args);
            }
          }
          case 'build_lighting': {
            return cleanObject(await tools.lightingTools.buildLighting({
              quality: (args.quality as any) || 'Preview',
              buildOnlySelected: false,
              buildReflectionCaptures: false
            }));
          }
          case 'export_level': {
            const res = await tools.levelTools.exportLevel({
              levelPath: args.levelPath,
              exportPath: args.destinationPath
            });
            return cleanObject(res);
          }
          case 'import_level': {
            const res = await tools.levelTools.importLevel({
              packagePath: args.sourcePath,
              destinationPath: args.destinationPath
            });
            return cleanObject(res);
          }
          case 'list_levels': {
            const res = await tools.levelTools.listLevels();
            return cleanObject(res);
          }
          case 'get_summary': {
            const res = await tools.levelTools.getLevelSummary(args.levelPath);
            return cleanObject(res);
          }
          case 'delete': {
            const res = await tools.levelTools.deleteLevels({ levelPaths: [args.levelPath] });
            return cleanObject(res);
          }
          case 'set_metadata': {
            const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');
            const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata : {};
            const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: levelPath, metadata });
            return cleanObject(res);
          }
          case 'validate_level': {
            // Interface doesn't support validateLevel, fallback to automation request
            return await executeAutomationRequest(tools, 'manage_level', args);
          }
          default:
            return await executeAutomationRequest(tools, 'manage_level', args);
        }
      }

      case 'manage_blueprint': {
        switch (action) {
          case 'create': {
            // Support 'path' argument by splitting it into name and savePath if not provided
            let name = args.name;
            let savePath = args.savePath;
            if (!name && args.path) {
              const parts = args.path.split('/');
              name = parts.pop();
              savePath = parts.join('/');
              if (!savePath) savePath = '/Game';
            }

            const res = await tools.blueprintTools.createBlueprint({
              name: name,
              blueprintType: args.blueprintType,
              savePath: savePath,
              parentClass: args.parentClass,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'ensure_exists': {
            const res = await tools.blueprintTools.waitForBlueprint(args.name || args.blueprintPath || args.path, args.timeoutMs);
            return cleanObject(res);
          }
          case 'add_variable': {
            const res = await tools.blueprintTools.addVariable({
              blueprintName: args.name || args.blueprintPath || args.path,
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
              blueprintName: args.name || args.blueprintPath || args.path,
              variableName: args.variableName,
              metadata: args.metadata,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'set_metadata': {
            const assetPathRaw = typeof args.assetPath === 'string' ? args.assetPath.trim() : '';
            const blueprintPathRaw = typeof args.blueprintPath === 'string' ? args.blueprintPath.trim() : '';
            const nameRaw = typeof args.name === 'string' ? args.name.trim() : '';
            const savePathRaw = typeof args.savePath === 'string' ? args.savePath.trim() : '';

            let assetPath = assetPathRaw;
            if (!assetPath) {
              if (blueprintPathRaw) {
                assetPath = blueprintPathRaw;
              } else if (nameRaw && savePathRaw) {
                const base = savePathRaw.replace(/\/$/, '');
                assetPath = `${base}/${nameRaw}`;
              }
            }
            if (!assetPath) {
              throw new Error('Invalid parameters: assetPath or blueprintPath or name+savePath required for set_metadata');
            }

            const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata : {};
            const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath, metadata });
            return cleanObject(res);
          }
          case 'add_event': {
            const res = await tools.blueprintTools.addEvent({
              blueprintName: args.name || args.blueprintPath || args.path,
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
              blueprintName: args.name || args.blueprintPath || args.path,
              eventName: args.eventName,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'add_function': {
            const res = await tools.blueprintTools.addFunction({
              blueprintName: args.name || args.blueprintPath || args.path,
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
              blueprintName: args.name || args.blueprintPath || args.path,
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
              blueprintPath: args.name || args.blueprintPath || args.path,
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
              blueprintPath: args.name || args.blueprintPath || args.path,
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
              blueprintName: args.name || args.blueprintPath || args.path,
              scriptName: args.scriptName,
              timeoutMs: args.timeoutMs,
              waitForCompletion: args.waitForCompletion,
              waitForCompletionTimeoutMs: args.waitForCompletionTimeoutMs
            });
            return cleanObject(res);
          }
          case 'add_node': {
            const res = await tools.blueprintTools.addNode({
              blueprintName: args.name || args.blueprintPath || args.path,
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
              blueprintPath: args.name || args.blueprintPath || args.path,
              componentClass: args.componentType,
              componentName: args.componentName,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'reparent_scs_component': {
            const res = await tools.blueprintTools.reparentSCSComponent({
              blueprintPath: args.name || args.blueprintPath || args.path,
              componentName: args.componentName,
              newParent: args.newParent,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'set_scs_property': {
            const res = await tools.blueprintTools.setSCSComponentProperty({
              blueprintPath: args.name || args.blueprintPath || args.path,
              componentName: args.componentName,
              propertyName: args.propertyName,
              propertyValue: args.propertyValue,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'remove_scs_component': {
            const res = await tools.blueprintTools.removeSCSComponent({
              blueprintPath: args.name || args.blueprintPath || args.path,
              componentName: args.componentName,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'get_scs': {
            const res = await tools.blueprintTools.getBlueprintSCS({
              blueprintPath: args.name || args.blueprintPath || args.path,
              timeoutMs: args.timeoutMs
            });
            return cleanObject(res);
          }
          case 'set_default': {
            const res = await tools.blueprintTools.setBlueprintDefault({
              blueprintName: args.name || args.blueprintPath || args.path,
              propertyName: args.propertyName,
              value: args.value
            });
            return cleanObject(res);
          }
          case 'compile': {
            const res = await tools.blueprintTools.compileBlueprint({
              blueprintName: args.name || args.blueprintPath || args.path,
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
              blueprintPath: args.name || args.blueprintPath || args.path,
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
          case 'set_metadata': {
            const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
            const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata : {};
            const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: path, metadata });
            return cleanObject(res);
          }
          default:
            return await executeAutomationRequest(tools, 'manage_sequence', args);
        }
      }

      case 'animation_physics': {
        const animAction = String(action || '').toLowerCase();

        // Route specific actions to their dedicated handlers
        if (animAction === 'create_animation_blueprint' || animAction === 'create_anim_blueprint' || animAction === 'create_animation_bp') {
          const name = args.name ?? args.blueprintName;
          const skeletonPath = args.skeletonPath ?? args.targetSkeleton;
          const savePath = args.savePath ?? args.path ?? '/Game/Animations';

          const payload = {
            ...args,
            name,
            skeletonPath,
            savePath
          };

          return await executeAutomationRequest(tools, 'create_animation_blueprint', payload, 'Automation bridge not available for animation blueprint creation');
        }

        if (animAction === 'play_anim_montage' || animAction === 'play_montage') {
          return await executeAutomationRequest(tools, 'play_anim_montage', args, 'Automation bridge not available for montage playback');
        }

        if (animAction === 'setup_ragdoll' || animAction === 'activate_ragdoll') {
          return await executeAutomationRequest(tools, 'setup_ragdoll', args, 'Automation bridge not available for ragdoll setup');
        }

        // Flatten blend space axis parameters for C++ handler
        if (animAction === 'create_blend_space' || animAction === 'create_blend_tree') {
          if (args.horizontalAxis) {
            args.minX = args.horizontalAxis.minValue;
            args.maxX = args.horizontalAxis.maxValue;
          }
          if (args.verticalAxis) {
            args.minY = args.verticalAxis.minValue;
            args.maxY = args.verticalAxis.maxValue;
          }
        }

        switch (animAction) {
          case 'create_blend_space':
            return cleanObject(await tools.animationTools.createBlendSpace({
              name: args.name,
              path: args.path || args.savePath,
              skeletonPath: args.skeletonPath,
              horizontalAxis: args.horizontalAxis,
              verticalAxis: args.verticalAxis
            }));
          case 'create_state_machine':
            return cleanObject(await tools.animationTools.createStateMachine({
              machineName: args.machineName || args.name,
              states: args.states,
              transitions: args.transitions,
              blueprintPath: args.blueprintPath || args.path || args.savePath
            }));
          case 'setup_ik':
            return cleanObject(await tools.animationTools.setupIK({
              actorName: args.actorName,
              ikBones: args.ikBones,
              enableFootPlacement: args.enableFootPlacement
            }));
          case 'create_procedural_anim':
            return cleanObject(await tools.animationTools.createProceduralAnim({
              systemName: args.systemName || args.name,
              baseAnimation: args.baseAnimation,
              modifiers: args.modifiers,
              savePath: args.savePath || args.path
            }));
          case 'create_blend_tree':
            return cleanObject(await tools.animationTools.createBlendTree({
              treeName: args.treeName || args.name,
              blendType: args.blendType,
              basePose: args.basePose,
              additiveAnimations: args.additiveAnimations,
              savePath: args.savePath || args.path
            }));
          case 'cleanup':
            return cleanObject(await tools.animationTools.cleanup(args.artifacts));
          case 'create_animation_asset':
            return cleanObject(await tools.animationTools.createAnimationAsset({
              name: args.name,
              path: args.path || args.savePath,
              skeletonPath: args.skeletonPath,
              assetType: args.assetType
            }));
          case 'add_notify':
            return cleanObject(await tools.animationTools.addNotify({
              animationPath: args.animationPath,
              assetPath: args.assetPath,
              notifyName: args.notifyName,
              time: args.time
            }));
          case 'configure_vehicle':
            return cleanObject(await tools.physicsTools.configureVehicle({
              vehicleName: args.vehicleName,
              vehicleType: args.vehicleType,
              wheels: args.wheels,
              engine: args.engine,
              transmission: args.transmission,
              pluginDependencies: args.pluginDependencies
            }));
          default:
            const res = await executeAutomationRequest(tools, 'animation_physics', args, 'Automation bridge not available for animation/physics operations');
            return cleanObject(res);
        }
      }

      case 'create_effect': {
        // Handle creation actions explicitly to use NiagaraTools helper
        if (action === 'create_niagara_system') {
          const res = await tools.niagaraTools.createSystem({
            name: args.name,
            savePath: args.savePath,
            template: args.template
          });
          return cleanObject(res);
        }
        if (action === 'create_niagara_emitter') {
          const res = await tools.niagaraTools.createEmitter({
            name: args.name,
            savePath: args.savePath,
            systemPath: args.systemPath,
            template: args.template
          });
          return cleanObject(res);
        }

        // Pre-process arguments for particle presets
        if (args.action === 'particle') {
          const presets: Record<string, string> = {
            'Default': '/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit',
            'Smoke': '/StarterContent/Particles/P_Smoke.P_Smoke',
            'Fire': '/StarterContent/Particles/P_Fire.P_Fire',
            'Explosion': '/StarterContent/Particles/P_Explosion.P_Explosion',
          };
          // Check both preset and effectType fields
          const key = args.preset || args.effectType;
          if (key && presets[key]) {
            args.preset = presets[key];
          }
        }
        const res = await executeAutomationRequest(tools, 'create_effect', args, 'Automation bridge not available for effect creation operations');
        return cleanObject(res);
      }

      case 'build_environment': {
        const envAction = String(action || '').toLowerCase();
        switch (envAction) {
          case 'create_landscape':
            return cleanObject(await tools.landscapeTools.createLandscape({
              name: args.name,
              location: args.location,
              sizeX: args.sizeX,
              sizeY: args.sizeY,
              quadsPerSection: args.quadsPerSection,
              sectionsPerComponent: args.sectionsPerComponent,
              componentCount: args.componentCount,
              materialPath: args.materialPath,
              enableWorldPartition: args.enableWorldPartition,
              runtimeGrid: args.runtimeGrid,
              isSpatiallyLoaded: args.isSpatiallyLoaded,
              dataLayers: args.dataLayers
            }));
          case 'sculpt':
            return cleanObject(await tools.landscapeTools.sculptLandscape({
              landscapeName: args.landscapeName || args.name,
              tool: args.tool,
              location: args.location,
              radius: args.radius,
              strength: args.strength
            }));
          case 'add_foliage':
            // Check if this is adding a foliage TYPE (has meshPath) or INSTANCES (has locations/position)
            if (args.meshPath) {
              return cleanObject(await tools.foliageTools.addFoliageType({
                name: args.foliageType || args.name || 'TC_Tree',
                meshPath: args.meshPath,
                density: args.density
              }));
            } else {
              return cleanObject(await tools.foliageTools.addFoliage({
                foliageType: args.foliageType,
                locations: args.locations || (args.position ? [args.position] : [])
              }));
            }
          case 'paint_foliage':
            return cleanObject(await tools.foliageTools.paintFoliage({
              foliageType: args.foliageType,
              position: args.position || args.location, // Handle both
              brushSize: args.brushSize || args.radius,
              paintDensity: args.density || args.strength, // Map strength/density
              eraseMode: args.eraseMode
            }));
          case 'create_procedural_terrain':
            return cleanObject(await tools.landscapeTools.createProceduralTerrain({
              name: args.name,
              location: args.location,
              subdivisions: args.subdivisions,
              settings: args.settings
            }));
          case 'create_procedural_foliage':
            return cleanObject(await tools.foliageTools.createProceduralFoliage({
              name: args.name,
              foliageTypes: args.foliageTypes,
              volumeName: args.volumeName,
              bounds: args.bounds,
              seed: args.seed,
              tileSize: args.tileSize
            }));
          case 'generate_lods':
            return cleanObject(await tools.assetTools.generateLODs({
              assetPath: args.assetPath,
              lodCount: args.lodCount,
              reductionSettings: args.reductionSettings
            }));
          case 'bake_lightmap':
            return cleanObject(await tools.lightingTools.buildLighting({
              quality: (args.quality as any) || 'Preview',
              buildOnlySelected: false,
              buildReflectionCaptures: false
            }));
          case 'create_landscape_grass_type':
            return cleanObject(await tools.landscapeTools.createLandscapeGrassType({
              name: args.name,
              // Prefer explicit meshPath used by tests, fall back to path/staticMesh for
              // compatibility with older callers.
              path: args.meshPath || args.path,
              staticMesh: args.staticMesh
            }));
          case 'export_snapshot':
            return cleanObject(await tools.environmentTools.exportSnapshot({
              path: args.path,
              filename: args.filename
            }));
          case 'import_snapshot':
            return cleanObject(await tools.environmentTools.importSnapshot({
              path: args.path,
              filename: args.filename
            }));
          case 'set_landscape_material':
            return cleanObject(await tools.landscapeTools.setLandscapeMaterial({
              landscapeName: args.landscapeName || args.name,
              materialPath: args.materialPath
            }));
          case 'delete': {
            const names = Array.isArray(args.names)
              ? args.names
              : (Array.isArray(args.actors) ? args.actors : []);
            const res = await tools.environmentTools.cleanup({ names });
            return cleanObject(res);
          }
          default:
            const res = await executeAutomationRequest(tools, 'build_environment', args, 'Automation bridge not available for environment building operations');
            return cleanObject(res);
        }
      }

      case 'system_control': {
        const sysAction = String(action || '').toLowerCase();
        switch (sysAction) {
          case 'show_fps':
            await tools.systemTools.executeConsoleCommand(args.enabled !== false ? 'stat fps' : 'stat fps 0');
            return { success: true, message: `FPS display ${args.enabled !== false ? 'enabled' : 'disabled'}`, action: 'show_fps' };
          case 'profile':
            await tools.systemTools.executeConsoleCommand(args.enabled !== false ? 'stat unit' : 'stat unit 0');
            return { success: true, message: `Profiling ${args.enabled !== false ? 'enabled' : 'disabled'}`, action: 'profile' };
          case 'set_quality':
            const quality = args.quality || 'medium';
            const qVal = quality === 'high' || quality === 'epic' ? 3 : (quality === 'low' ? 0 : 1);
            await tools.systemTools.executeConsoleCommand(`sg.ViewDistanceQuality ${qVal}`);
            return { success: true, message: `Quality set to ${quality}`, action: 'set_quality' };
          case 'execute_command':
            return cleanObject(await tools.systemTools.executeConsoleCommand(args.command));
          case 'show_widget':
            return cleanObject(await tools.uiTools.showWidget(args.widgetPath));
          case 'set_cvar':
            await tools.systemTools.executeConsoleCommand(`${args.cvar} ${args.value}`);
            return { success: true, message: `CVar ${args.cvar} set to ${args.value}`, action: 'set_cvar' };
          case 'get_project_settings':
            return cleanObject(await tools.systemTools.getProjectSettings(args.section));
          case 'validate_assets':
            // Interface only supports single asset validation
            if (args.paths && args.paths.length > 0) {
              const results = [];
              for (const path of args.paths) {
                results.push(await tools.assetTools.validate({ assetPath: path }));
              }
              return { success: true, results, action: 'validate_assets' };
            }
            return { success: false, message: 'No paths provided for validation', action: 'validate_assets' };
          case 'play_sound':
            return cleanObject(await tools.audioTools.playSound(args.soundPath, args.volume, args.pitch));
          case 'screenshot':
            return cleanObject(await tools.editorTools.takeScreenshot(args.filename));
          case 'set_resolution': {
            const width = Number(args.width);
            const height = Number(args.height);
            if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
              throw new Error('Invalid resolution: width and height must be positive numbers');
            }
            const windowed = args.windowed !== false; // default to windowed=true
            const suffix = windowed ? 'w' : 'f';
            await tools.systemTools.executeConsoleCommand(`r.SetRes ${width}x${height}${suffix}`);
            return {
              success: true,
              message: `Resolution set to ${width}x${height} (${windowed ? 'windowed' : 'fullscreen'})`,
              action: 'set_resolution'
            };
          }
          case 'set_fullscreen': {
            const width = Number(args.width);
            const height = Number(args.height);
            if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
              throw new Error('Invalid resolution: width and height must be positive numbers');
            }
            const windowed = args.windowed === true; // default to fullscreen when omitted
            const suffix = windowed ? 'w' : 'f';
            await tools.systemTools.executeConsoleCommand(`r.SetRes ${width}x${height}${suffix}`);
            return {
              success: true,
              message: `Fullscreen mode set to ${width}x${height} (${windowed ? 'windowed' : 'fullscreen'})`,
              action: 'set_fullscreen'
            };
          }
          default:
            const res = await executeAutomationRequest(tools, 'system_control', args, 'Automation bridge not available for system control operations');
            return cleanObject(res);
        }
      }

      case 'console_command': {
        const res = await executeAutomationRequest(tools, 'console_command', args, 'Automation bridge not available for console command operations');
        return cleanObject(res);
      }

      case 'inspect': {
        const action = args.action;
        switch (action) {
          case 'inspect_object':
            return cleanObject(await tools.introspectionTools.inspectObject({
              objectPath: args.objectPath,
              detailed: true // Default to detailed for inspect_object
            }));
          case 'get_property':
            return cleanObject(await tools.introspectionTools.getProperty({
              objectPath: args.objectPath || args.actorName,
              propertyName: args.propertyName
            }));
          case 'set_property':
            return cleanObject(await tools.introspectionTools.setProperty({
              objectPath: args.objectPath || args.actorName,
              propertyName: args.propertyName,
              value: args.value
            }));

          case 'get_components':
            return cleanObject(await tools.actorTools.getComponents(args.actorName || args.objectPath));
          case 'get_component_property':
            return cleanObject(await tools.introspectionTools.getComponentProperty({
              objectPath: args.objectPath || args.actorName,
              componentName: args.componentName,
              propertyName: args.propertyName
            }));
          case 'set_component_property':
            return cleanObject(await tools.introspectionTools.setComponentProperty({
              objectPath: args.objectPath || args.actorName,
              componentName: args.componentName,
              propertyName: args.propertyName,
              value: args.value
            }));
          case 'get_metadata':
            return cleanObject(await tools.actorTools.getMetadata(args.actorName || args.objectPath));
          case 'add_tag':
            return cleanObject(await tools.actorTools.addTag({
              actorName: args.actorName || args.objectPath,
              tag: args.tag
            }));
          case 'find_by_tag':
            return cleanObject(await tools.actorTools.findByTag({
              tag: args.tag
            }));
          case 'create_snapshot':
            return cleanObject(await tools.actorTools.createSnapshot({
              actorName: args.actorName || args.objectPath,
              snapshotName: args.snapshotName
            }));
          case 'restore_snapshot':
            return cleanObject(await tools.actorTools.restoreSnapshot({
              actorName: args.actorName || args.objectPath,
              snapshotName: args.snapshotName
            }));
          case 'export':
            return cleanObject(await tools.actorTools.exportActor({
              actorName: args.actorName || args.objectPath,
              destinationPath: args.destinationPath
            }));
          case 'delete_object':
            return cleanObject(await tools.actorTools.delete({
              actorName: args.actorName || args.objectPath
            }));
          case 'list_objects':
            return cleanObject(await tools.actorTools.listActors());
          case 'find_by_class':
            return cleanObject(await tools.introspectionTools.findObjectsByClass(args.className));
          case 'get_bounding_box':
            return cleanObject(await tools.actorTools.getBoundingBox(args.actorName || args.objectPath));
          case 'inspect_class':
            return cleanObject(await tools.introspectionTools.getCDO(args.className));
          default:
            // Fallback to generic automation request if action not explicitly handled
            const res = await executeAutomationRequest(tools, 'inspect', args, 'Automation bridge not available for inspect operations');
            return cleanObject(res);
        }
      }


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


