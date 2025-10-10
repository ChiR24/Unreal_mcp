import { cleanObject } from '../utils/safe-json.js';
import { Logger } from '../utils/logger.js';
import { escapePythonString } from '../utils/python.js';

const log = new Logger('ConsolidatedToolHandler');

const ACTION_REQUIRED_ERROR = 'Missing required parameter: action';
const AUTOMATION_TRANSPORT_KEYS = ['automation_bridge', 'automation', 'bridge'];

function ensureArgsPresent(args: any) {
  if (args === null || args === undefined) {
    throw new Error('Invalid arguments: null or undefined');
  }
}

function requireAction(args: any): string {
  ensureArgsPresent(args);
  const action = args.action;
  if (typeof action !== 'string' || action.trim() === '') {
    throw new Error(ACTION_REQUIRED_ERROR);
  }
  return action;
}

function requireNonEmptyString(value: any, field: string, message?: string): string {
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error(message ?? `Invalid ${field}: must be a non-empty string`);
  }
  return value;
}

function requirePositiveNumber(value: any, field: string, message?: string): number {
  if (typeof value !== 'number' || !isFinite(value) || value <= 0) {
    throw new Error(message ?? `Invalid ${field}: must be a positive number`);
  }
  return value;
}

function requireVector3Components(
  vector: any,
  message: string
): [number, number, number] {
  if (
    !vector ||
    typeof vector.x !== 'number' ||
    typeof vector.y !== 'number' ||
    typeof vector.z !== 'number'
  ) {
    throw new Error(message);
  }
  return [vector.x, vector.y, vector.z];
}

function getElicitationTimeoutMs(tools: any): number | undefined {
  if (!tools) return undefined;
  const direct = tools.elicitationTimeoutMs;
  if (typeof direct === 'number' && Number.isFinite(direct)) {
    return direct;
  }
  if (typeof tools.getElicitationTimeoutMs === 'function') {
    const value = tools.getElicitationTimeoutMs();
    if (typeof value === 'number' && Number.isFinite(value)) {
      return value;
    }
  }
  return undefined;
}

async function elicitMissingPrimitiveArgs(
  tools: any,
  args: any,
  prompt: string,
  fieldSchemas: Record<string, { type: 'string' | 'number' | 'integer' | 'boolean'; title?: string; description?: string; enum?: string[]; enumNames?: string[]; minimum?: number; maximum?: number; minLength?: number; maxLength?: number; pattern?: string; format?: string; default?: unknown }>
) {
  if (
    !tools ||
    typeof tools.supportsElicitation !== 'function' ||
    !tools.supportsElicitation() ||
    typeof tools.elicit !== 'function'
  ) {
    return;
  }

  const properties: Record<string, any> = {};
  const required: string[] = [];

  for (const [key, schema] of Object.entries(fieldSchemas)) {
    const value = args?.[key];
    const missing =
      value === undefined ||
      value === null ||
      (typeof value === 'string' && value.trim() === '');
    if (missing) {
      properties[key] = schema;
      required.push(key);
    }
  }

  if (required.length === 0) return;

  const timeoutMs = getElicitationTimeoutMs(tools);
  const options: any = {
    fallback: async () => ({ ok: false, error: 'missing-params' })
  };
  if (typeof timeoutMs === 'number') {
    options.timeoutMs = timeoutMs;
  }

  try {
    const elicited = await tools.elicit(
      prompt,
      { type: 'object', properties, required },
      options
    );

    if (elicited?.ok && elicited.value) {
      for (const key of required) {
        const value = elicited.value[key];
        if (value === undefined || value === null) continue;
        args[key] = typeof value === 'string' ? value.trim() : value;
      }
    }
  } catch (err) {
    log.debug('Special elicitation fallback skipped', {
      prompt,
      err: (err as any)?.message || String(err)
    });
  }
}

type AutomationBridgeInfo = {
  instance?: {
    sendAutomationRequest?: (action: string, payload: Record<string, unknown>, options?: { timeoutMs?: number }) => Promise<any>;
    isConnected?: () => boolean;
  };
  canSend: boolean;
  isConnected: boolean;
};

type AutomationTransportDecision = {
  useAutomation: boolean;
  allowFallback: boolean;
  explicitAutomation: boolean;
  fallbackReason?: string;
};

function getAutomationBridgeInfo(tools: any): AutomationBridgeInfo {
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

function decideAutomationTransport(
  rawTransport: unknown,
  info: AutomationBridgeInfo
): AutomationTransportDecision {
  const normalized = typeof rawTransport === 'string' ? rawTransport.trim().toLowerCase() : undefined;
  const explicitAutomation = normalized ? AUTOMATION_TRANSPORT_KEYS.includes(normalized) : false;
  const auto = !normalized || normalized === '' || normalized === 'auto' || normalized === 'default';

  if (!auto && !explicitAutomation) {
    throw new Error('Unsupported transport. Only automation_bridge is supported.');
  }

  if (!info.canSend) {
    throw new Error('Automation bridge not available');
  }

  return {
    useAutomation: true,
    allowFallback: true,
    explicitAutomation,
    fallbackReason: 'Automation bridge request failed; retrying via Python wrapper.'
  };
}

export async function handleConsolidatedToolCall(
  name: string,
  args: any,
  tools: any
) {
  const startTime = Date.now();
  // Use scoped logger (stderr) to avoid polluting stdout JSON
  log.debug(`Starting execution of ${name} at ${new Date().toISOString()}`);
  
  try {
    ensureArgsPresent(args);

    switch (name) {
      // 1. ASSET MANAGER
      case 'manage_asset':
        switch (requireAction(args)) {
          case 'list': {
            if (args.directory !== undefined && args.directory !== null && typeof args.directory !== 'string') {
              throw new Error('Invalid directory: must be a string');
            }
            const res = await tools.assetResources.list(args.directory || '/Game', false);
            return cleanObject(res);
          }
          case 'import': {
            let sourcePath = typeof args.sourcePath === 'string' ? args.sourcePath.trim() : '';
            let destinationPath = typeof args.destinationPath === 'string' ? args.destinationPath.trim() : '';

            if ((!sourcePath || !destinationPath) && typeof tools.supportsElicitation === 'function' && tools.supportsElicitation() && typeof tools.elicit === 'function') {
              const schemaProps: Record<string, any> = {};
              const required: string[] = [];

              if (!sourcePath) {
                schemaProps.sourcePath = {
                  type: 'string',
                  title: 'Source File Path',
                  description: 'Full path to the asset file on disk to import'
                };
                required.push('sourcePath');
              }

              if (!destinationPath) {
                schemaProps.destinationPath = {
                  type: 'string',
                  title: 'Destination Path',
                  description: 'Unreal content path where the asset should be imported (e.g., /Game/MCP/Assets)'
                };
                required.push('destinationPath');
              }

              if (required.length > 0) {
                const timeoutMs = getElicitationTimeoutMs(tools);
                const options: any = { fallback: async () => ({ ok: false, error: 'missing-import-params' }) };
                if (typeof timeoutMs === 'number') {
                  options.timeoutMs = timeoutMs;
                }
                const elicited = await tools.elicit(
                  'Provide the missing import parameters for manage_asset.import',
                  { type: 'object', properties: schemaProps, required },
                  options
                );

                if (elicited?.ok && elicited.value) {
                  if (typeof elicited.value.sourcePath === 'string') {
                    sourcePath = elicited.value.sourcePath.trim();
                  }
                  if (typeof elicited.value.destinationPath === 'string') {
                    destinationPath = elicited.value.destinationPath.trim();
                  }
                }
              }
            }

            const sourcePathValidated = requireNonEmptyString(sourcePath || args.sourcePath, 'sourcePath', 'Invalid sourcePath');
            const destinationPathValidated = requireNonEmptyString(destinationPath || args.destinationPath, 'destinationPath', 'Invalid destinationPath');
            
            // Use deferred asset import to avoid Task Graph recursion
            const automationInfo = getAutomationBridgeInfo(tools);
            const automationBridge = automationInfo.instance;
            if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
              throw new Error('Automation bridge not connected for asset import');
            }
            
            const response = await automationBridge.sendAutomationRequest(
              'import_asset_deferred',
              { sourcePath: sourcePathValidated, destinationPath: destinationPathValidated }
            );
            
            if (response.success === false) {
              return cleanObject({
                success: false,
                message: response.message || 'Asset import failed',
                error: response.error || response.message || 'IMPORT_FAILED'
              });
            }
            
            const res = response.result || {};
            return cleanObject({
              success: true,
              message: res.message || `Imported assets to ${destinationPathValidated}`,
              imported: res.imported || 0,
              paths: res.paths || []
            });
          }
          case 'create_material': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the material details for manage_asset.create_material',
              {
                name: {
                  type: 'string',
                  title: 'Material Name',
                  description: 'Name for the new material asset'
                },
                path: {
                  type: 'string',
                  title: 'Save Path',
                  description: 'Optional Unreal content path where the material should be saved'
                }
              }
            );
            const sanitizedName = typeof args.name === 'string' ? args.name.trim() : args.name;
            const sanitizedPath = typeof args.path === 'string' ? args.path.trim() : args.path;
            const name = requireNonEmptyString(sanitizedName, 'name', 'Invalid name: must be a non-empty string');
            const res = await tools.materialTools.createMaterial(name, sanitizedPath || '/Game/Materials');
            return cleanObject(res);
          }
          case 'duplicate': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the duplication details for manage_asset.duplicate',
              {
                sourcePath: {
                  type: 'string',
                  title: 'Source Asset Path',
                  description: 'Existing asset path to duplicate (e.g., /Game/Folder/Asset)'
                },
                destinationPath: {
                  type: 'string',
                  title: 'Destination Folder',
                  description: 'Target content folder for the duplicated asset (e.g., /Game/Folder/Duplicates)'
                }
              }
            );

            const sourcePath = requireNonEmptyString(args.sourcePath, 'sourcePath', 'Missing required parameter: sourcePath');
            const destinationPath = requireNonEmptyString(args.destinationPath, 'destinationPath', 'Missing required parameter: destinationPath');
            const newName = typeof args.newName === 'string' ? args.newName.trim() : undefined;
            const timeoutMs = typeof args.timeoutMs === 'number' && Number.isFinite(args.timeoutMs) && args.timeoutMs > 0
              ? Math.floor(args.timeoutMs)
              : undefined;

            const res = await tools.assetTools.duplicateAsset({
              sourcePath,
              destinationPath,
              newName,
              overwrite: args.overwrite === true,
              save: args.save === true,
              timeoutMs
            });
            return cleanObject(res);
          }
          case 'rename': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the rename details for manage_asset.rename',
              {
                assetPath: {
                  type: 'string',
                  title: 'Asset Path',
                  description: 'Existing asset path to rename (e.g., /Game/Folder/Asset)'
                },
                newName: {
                  type: 'string',
                  title: 'New Asset Name',
                  description: 'New asset name without folder path'
                }
              }
            );

            const assetPath = requireNonEmptyString(args.assetPath, 'assetPath', 'Missing required parameter: assetPath');
            const newName = requireNonEmptyString(args.newName, 'newName', 'Missing required parameter: newName');
            const timeoutMs = typeof args.timeoutMs === 'number' && Number.isFinite(args.timeoutMs) && args.timeoutMs > 0
              ? Math.floor(args.timeoutMs)
              : undefined;

            const res = await tools.assetTools.renameAsset({ assetPath, newName, timeoutMs });
            return cleanObject(res);
          }
          case 'move': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the move details for manage_asset.move',
              {
                assetPath: {
                  type: 'string',
                  title: 'Asset Path',
                  description: 'Existing asset path to move (e.g., /Game/Folder/Asset)'
                },
                destinationPath: {
                  type: 'string',
                  title: 'Destination Folder',
                  description: 'Target content folder (e.g., /Game/NewFolder)'
                }
              }
            );

            const assetPath = requireNonEmptyString(args.assetPath, 'assetPath', 'Missing required parameter: assetPath');
            const destinationPath = requireNonEmptyString(args.destinationPath, 'destinationPath', 'Missing required parameter: destinationPath');
            const newName = typeof args.newName === 'string' ? args.newName.trim() : undefined;
            const timeoutMs = typeof args.timeoutMs === 'number' && Number.isFinite(args.timeoutMs) && args.timeoutMs > 0
              ? Math.floor(args.timeoutMs)
              : undefined;

            const res = await tools.assetTools.moveAsset({
              assetPath,
              destinationPath,
              newName,
              fixupRedirectors: args.fixupRedirectors !== false,
              timeoutMs
            });
            return cleanObject(res);
          }
          case 'delete': {
            if (!args.assetPath && !Array.isArray(args.assetPaths)) {
              await elicitMissingPrimitiveArgs(
                tools,
                args,
                'Provide the asset path for manage_asset.delete',
                {
                  assetPath: {
                    type: 'string',
                    title: 'Asset Path',
                    description: 'Asset path to delete (e.g., /Game/Folder/Asset)'
                  }
                }
              );
            }

            let paths: string[] = [];
            if (Array.isArray(args.assetPaths)) {
              paths = args.assetPaths
                .filter((entry: unknown): entry is string => typeof entry === 'string')
                .map((entry: string) => entry.trim())
                .filter((entry: string) => entry.length > 0);
            }

            if (paths.length === 0) {
              const singlePath = requireNonEmptyString(args.assetPath, 'assetPath', 'Missing required parameter: assetPath');
              paths = [singlePath];
            }

            const timeoutMs = typeof args.timeoutMs === 'number' && Number.isFinite(args.timeoutMs) && args.timeoutMs > 0
              ? Math.floor(args.timeoutMs)
              : undefined;

            const res = await tools.assetTools.deleteAssets({
              assetPaths: paths,
              fixupRedirectors: args.fixupRedirectors !== false,
              timeoutMs
            });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown asset action: ${args.action}`);
        }

      // 2. ACTOR CONTROL
      case 'control_actor':
        switch (requireAction(args)) {
          case 'spawn': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the spawn parameters for control_actor.spawn',
              {
                classPath: {
                  type: 'string',
                  title: 'Actor Class or Asset Path',
                  description: 'Class name (e.g., StaticMeshActor) or asset path (e.g., /Engine/BasicShapes/Cube) to spawn'
                }
              }
            );
            const classPathInput = typeof args.classPath === 'string' ? args.classPath.trim() : args.classPath;
            const classPath = requireNonEmptyString(classPathInput, 'classPath', 'Invalid classPath: must be a non-empty string');
            const actorNameInput = typeof args.actorName === 'string' && args.actorName.trim() !== ''
              ? args.actorName
              : (typeof args.name === 'string' ? args.name : undefined);
            const res = await tools.actorTools.spawn({
              classPath,
              location: args.location,
              rotation: args.rotation,
              actorName: actorNameInput
            });
            return cleanObject(res);
          }
          case 'delete': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Which actor should control_actor.delete remove?',
              {
                actorName: {
                  type: 'string',
                  title: 'Actor Name',
                  description: 'Exact label of the actor to delete'
                }
              }
            );
            const actorNameArg = typeof args.actorName === 'string' && args.actorName.trim() !== ''
              ? args.actorName
              : (typeof args.name === 'string' ? args.name : undefined);
            const actorName = requireNonEmptyString(actorNameArg, 'actorName', 'Invalid actorName');
            const res = await tools.bridge.executeEditorFunction('DELETE_ACTOR', { actor_name: actorName });
            return cleanObject(res);
          }
          case 'apply_force': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the target actor for control_actor.apply_force',
              {
                actorName: {
                  type: 'string',
                  title: 'Actor Name',
                  description: 'Physics-enabled actor that should receive the force'
                }
              }
            );
            const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Invalid actorName');
            const vector = requireVector3Components(args.force, 'Invalid force: must have numeric x,y,z');
            const res = await tools.physicsTools.applyForce({
              actorName,
              forceType: 'Force',
              vector
            });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown actor action: ${args.action}`);
        }

      // 3. EDITOR CONTROL
      case 'control_editor':
        switch (requireAction(args)) {
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
          case 'set_game_speed': {
            const speed = requirePositiveNumber(args.speed, 'speed', 'Invalid speed: must be a positive number');
            // Use console command via bridge
            const res = await tools.bridge.executeConsoleCommand(`slomo ${speed}`);
            return cleanObject(res);
          }
          case 'eject': {
            const res = await tools.bridge.executeConsoleCommand('eject');
            return cleanObject(res);
          }
          case 'possess': {
            const res = await tools.bridge.executeConsoleCommand('viewself');
            return cleanObject(res);
          }
          case 'set_camera': {
            const res = await tools.editorTools.setViewportCamera(args.location, args.rotation);
            return cleanObject(res);
          }
          case 'set_view_mode': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the view mode for control_editor.set_view_mode',
              {
                viewMode: {
                  type: 'string',
                  title: 'View Mode',
                  description: 'Viewport view mode (e.g., Lit, Unlit, Wireframe)'
                }
              }
            );
            const viewMode = requireNonEmptyString(args.viewMode, 'viewMode', 'Missing required parameter: viewMode');
            const res = await tools.bridge.setSafeViewMode(viewMode);
            return cleanObject(res);
          }
          case 'set_camera_fov': {
            const fov = requirePositiveNumber(args.fov, 'fov', 'Invalid FOV: must be a positive number');
            const res = await tools.editorTools.setFOV(fov);
            return cleanObject(res);
          }
          case 'set_camera_position': {
            const res = await tools.editorTools.setViewportCamera(args.location, args.rotation);
            return cleanObject(res);
          }
          case 'screenshot': {
            const res = await tools.editorTools.takeScreenshot(args.filename);
            return cleanObject(res);
          }
          case 'set_viewport_resolution': {
            const width = requirePositiveNumber(args.width, 'width', 'Invalid width: must be a positive number');
            const height = requirePositiveNumber(args.height, 'height', 'Invalid height: must be a positive number');
            const res = await tools.editorTools.setViewportResolution(width, height);
            return cleanObject(res);
          }
          case 'console_command': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the console command to execute',
              {
                command: {
                  type: 'string',
                  title: 'Console Command',
                  description: 'The Unreal Engine console command to execute'
                }
              }
            );
            const command = requireNonEmptyString(args.command, 'command', 'Missing required parameter: command');
            const res = await tools.editorTools.executeConsoleCommand(command);
            return cleanObject(res);
          }
          case 'stop_pie': {
            const res = await tools.editorTools.stopPlayInEditor();
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown editor action: ${args.action}`);
        }

      // 4. LEVEL MANAGER
case 'manage_level':
        switch (requireAction(args)) {
          case 'load':
          case 'load_level': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Select the level to load for manage_level.load',
              {
                levelPath: {
                  type: 'string',
                  title: 'Level Path',
                  description: 'Content path of the level asset to load (e.g., /Game/Maps/MyLevel)'
                }
              }
            );
            const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');
            const res = await tools.levelTools.loadLevel({ levelPath, streaming: !!args.streaming });
            return cleanObject(res);
          }
          case 'save':
          case 'save_current_level': {
            const res = await tools.levelTools.saveLevel({ levelName: args.levelName, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'stream':
          case 'stream_level': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the streaming level path (or name) for manage_level.stream',
              {
                levelPath: {
                  type: 'string',
                  title: 'Level Path',
                  description: 'Streaming level path (e.g., "/Game/Maps/Sublevel1"). Either levelPath or levelName is required.'
                }
              }
            );
            const deriveLevelName = (value?: string | null): string | undefined => {
              if (!value) return undefined;
              const trimmed = value.trim();
              if (!trimmed) return undefined;
              const tail = trimmed.split('/').filter(Boolean).pop();
              if (!tail) return undefined;
              const namePart = tail.includes('.') ? tail.split('.')[0] : tail;
              return namePart?.trim() || undefined;
            };

            const pathInput = typeof args.levelPath === 'string' ? args.levelPath.trim() : '';
            const nameInput = typeof args.levelName === 'string' ? args.levelName.trim() : '';

            if (!pathInput && !nameInput) {
              throw new Error('Missing required parameter: levelPath');
            }

            if (typeof args.shouldBeLoaded !== 'boolean') {
              throw new Error('Missing required parameter: shouldBeLoaded');
            }

            const levelPath = pathInput || undefined;
            const levelName = nameInput || deriveLevelName(pathInput) || undefined;
            const shouldBeLoaded = Boolean(args.shouldBeLoaded);
            const shouldBeVisible = typeof args.shouldBeVisible === 'boolean' ? Boolean(args.shouldBeVisible) : undefined;

            const res = await tools.levelTools.streamLevel({
              levelPath,
              levelName,
              shouldBeLoaded,
              shouldBeVisible
            });
            return cleanObject(res);
          }
          case 'create_level': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the level details for manage_level.create_level',
              {
                levelName: {
                  type: 'string',
                  title: 'Level Name',
                  description: 'Name for the new level'
                }
              }
            );
            const levelName = requireNonEmptyString(args.levelName, 'levelName', 'Missing required parameter: levelName');
            const res = await tools.levelTools.createLevel({ 
              levelName, 
              savePath: args.savePath || args.levelPath || '/Game/Maps',
              template: args.template 
            });
            return cleanObject(res);
          }
          case 'create_light': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the light details for manage_level.create_light',
              {
                lightType: {
                  type: 'string',
                  title: 'Light Type',
                  description: 'Directional, Point, Spot, Rect, or Sky'
                },
                name: {
                  type: 'string',
                  title: 'Light Name',
                  description: 'Name for the new light actor'
                }
              }
            );
            const lightType = requireNonEmptyString(args.lightType, 'lightType', 'Missing required parameter: lightType');
            const defaultLightNames: Record<string, string> = {
              directional: 'DirectionalLight_Auto',
              point: 'PointLight_Auto',
              spot: 'SpotLight_Auto',
              rect: 'RectLight_Auto',
              sky: 'SkyLight_Auto',
              skylight: 'SkyLight_Auto'
            };
            const providedName = typeof args.name === 'string' ? args.name.trim() : '';
            const typeKey = lightType.toLowerCase();
            const name = providedName || defaultLightNames[typeKey] || 'Light_Auto';
            const toVector = (value: any, fallback: [number, number, number]): [number, number, number] => {
              if (Array.isArray(value) && value.length === 3) {
                return [Number(value[0]) || 0, Number(value[1]) || 0, Number(value[2]) || 0];
              }
              if (value && typeof value === 'object') {
                return [Number(value.x) || 0, Number(value.y) || 0, Number(value.z) || 0];
              }
              return fallback;
            };
            const toRotator = (value: any, fallback: [number, number, number]): [number, number, number] => {
              if (Array.isArray(value) && value.length === 3) {
                return [Number(value[0]) || 0, Number(value[1]) || 0, Number(value[2]) || 0];
              }
              if (value && typeof value === 'object') {
                return [Number(value.pitch) || 0, Number(value.yaw) || 0, Number(value.roll) || 0];
              }
              return fallback;
            };
            const toColor = (value: any): [number, number, number] | undefined => {
              if (Array.isArray(value) && value.length === 3) {
                return [Number(value[0]) || 0, Number(value[1]) || 0, Number(value[2]) || 0];
              }
              if (value && typeof value === 'object') {
                return [Number(value.r) || 0, Number(value.g) || 0, Number(value.b) || 0];
              }
              return undefined;
            };

            const location = toVector(args.location, [0, 0, typeKey === 'directional' ? 500 : 0]);
            const rotation = toRotator(args.rotation, [0, 0, 0]);
            const color = toColor(args.color);
            const castShadows = typeof args.castShadows === 'boolean' ? args.castShadows : undefined;

            if (typeKey === 'directional') {
              return cleanObject(await tools.lightingTools.createDirectionalLight({
                name,
                intensity: args.intensity,
                color,
                rotation,
                castShadows,
                temperature: args.temperature
              }));
            }
            if (typeKey === 'point') {
              return cleanObject(await tools.lightingTools.createPointLight({
                name,
                location,
                intensity: args.intensity,
                radius: args.radius,
                color,
                falloffExponent: args.falloffExponent,
                castShadows
              }));
            }
            if (typeKey === 'spot') {
              const innerCone = typeof args.innerCone === 'number' ? args.innerCone : undefined;
              const outerCone = typeof args.outerCone === 'number' ? args.outerCone : undefined;
              if (innerCone !== undefined && outerCone !== undefined && innerCone >= outerCone) {
                throw new Error('innerCone must be less than outerCone');
              }
              return cleanObject(await tools.lightingTools.createSpotLight({
                name,
                location,
                rotation,
                intensity: args.intensity,
                innerCone: args.innerCone,
                outerCone: args.outerCone,
                radius: args.radius,
                color,
                castShadows
              }));
            }
            if (typeKey === 'rect') {
              return cleanObject(await tools.lightingTools.createRectLight({
                name,
                location,
                rotation,
                intensity: args.intensity,
                width: args.width,
                height: args.height,
                color
              }));
            }
            if (typeKey === 'sky' || typeKey === 'skylight') {
              return cleanObject(await tools.lightingTools.createSkyLight({
                name,
                sourceType: args.sourceType,
                cubemapPath: args.cubemapPath,
                intensity: args.intensity,
                recapture: args.recapture
              }));
            }
            throw new Error(`Unknown light type: ${lightType}`);
          }
          case 'build_lighting': {
            const res = await tools.lightingTools.buildLighting({ quality: args.quality || 'High', buildReflectionCaptures: true });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown level action: ${args.action}`);
        }

      // 5. ANIMATION & PHYSICS
case 'animation_physics':
        switch (requireAction(args)) {
          case 'create_animation_bp': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for animation_physics.create_animation_bp',
              {
                name: {
                  type: 'string',
                  title: 'Blueprint Name',
                  description: 'Name of the Animation Blueprint to create'
                },
                skeletonPath: {
                  type: 'string',
                  title: 'Skeleton Path',
                  description: 'Content path of the skeleton asset to bind'
                }
              }
            );
            const name = requireNonEmptyString(args.name, 'name', 'Invalid name');
            const skeletonPath = requireNonEmptyString(args.skeletonPath, 'skeletonPath', 'Invalid skeletonPath');
            const res = await tools.animationTools.createAnimationBlueprint({ name, skeletonPath, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'play_montage': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide playback details for animation_physics.play_montage',
              {
                actorName: {
                  type: 'string',
                  title: 'Actor Name',
                  description: 'Actor that should play the montage'
                },
                montagePath: {
                  type: 'string',
                  title: 'Montage Path',
                  description: 'Montage or animation asset path to play'
                }
              }
            );
            const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Invalid actorName');
            const montagePath = args.montagePath || args.animationPath;
            const validatedMontage = requireNonEmptyString(montagePath, 'montagePath', 'Invalid montagePath');
            const res = await tools.animationTools.playAnimation({ actorName, animationType: 'Montage', animationPath: validatedMontage, playRate: args.playRate });
            return cleanObject(res);
          }
          case 'setup_ragdoll': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide setup details for animation_physics.setup_ragdoll',
              {
                skeletonPath: {
                  type: 'string',
                  title: 'Skeleton Path',
                  description: 'Content path for the skeleton asset'
                },
                physicsAssetName: {
                  type: 'string',
                  title: 'Physics Asset Name',
                  description: 'Name of the physics asset to apply'
                }
              }
            );
            const skeletonPath = requireNonEmptyString(args.skeletonPath, 'skeletonPath', 'Invalid skeletonPath');
            const physicsAssetName = requireNonEmptyString(args.physicsAssetName, 'physicsAssetName', 'Invalid physicsAssetName');
            const res = await tools.physicsTools.setupRagdoll({ skeletonPath, physicsAssetName, blendWeight: args.blendWeight, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'configure_vehicle': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide vehicle details for animation_physics.configure_vehicle',
              {
                vehicleName: {
                  type: 'string',
                  title: 'Vehicle Name',
                  description: 'Identifier of the vehicle actor or Blueprint to configure'
                },
                vehicleType: {
                  type: 'string',
                  title: 'Vehicle Type',
                  description: 'Vehicle archetype (Car, Bike, Tank, Aircraft)'
                }
              }
            );

            const vehicleNameInput = typeof args.vehicleName === 'string' && args.vehicleName.trim() !== ''
              ? args.vehicleName
              : (typeof args.name === 'string' ? args.name : undefined);
            const vehicleName = requireNonEmptyString(vehicleNameInput, 'vehicleName', 'Missing required parameter: vehicleName');

            const vehicleTypeRaw = requireNonEmptyString(args.vehicleType, 'vehicleType', 'Missing required parameter: vehicleType');
            const normalizedType = vehicleTypeRaw.trim().toLowerCase();
            const typeMap: Record<string, 'Car' | 'Bike' | 'Tank' | 'Aircraft'> = {
              car: 'Car',
              bike: 'Bike',
              motorcycle: 'Bike',
              motorbike: 'Bike',
              tank: 'Tank',
              aircraft: 'Aircraft',
              plane: 'Aircraft'
            };
            const vehicleType = typeMap[normalizedType];
            if (!vehicleType) {
              throw new Error('Invalid vehicleType: expected Car, Bike, Tank, or Aircraft');
            }

            const sanitizeNumber = (value: any, field: string) => {
              if (typeof value !== 'number' || !Number.isFinite(value)) {
                throw new Error(`Invalid ${field}: must be a finite number`);
              }
              return value;
            };

            let wheels: Array<{ name: string; radius: number; width: number; mass: number; isSteering: boolean; isDriving: boolean }> | undefined;
            if (Array.isArray(args.wheels)) {
              const wheelEntries: any[] = args.wheels as any[];
              wheels = wheelEntries.map((wheel: any, index: number) => {
                if (!wheel || typeof wheel !== 'object') {
                  throw new Error(`Invalid wheel entry at index ${index}`);
                }
                const wheelName = requireNonEmptyString(wheel.name, `wheels[${index}].name`, `Missing wheel name at index ${index}`);
                const radius = sanitizeNumber(wheel.radius, `wheels[${index}].radius`);
                const width = sanitizeNumber(wheel.width, `wheels[${index}].width`);
                const mass = sanitizeNumber(wheel.mass, `wheels[${index}].mass`);
                const isSteering = Boolean(wheel.isSteering);
                const isDriving = Boolean(wheel.isDriving);
                return { name: wheelName, radius, width, mass, isSteering, isDriving };
              });
            }

            let engine: { maxRPM: number; torqueCurve: Array<[number, number]> } | undefined;
            if (args.engine && typeof args.engine === 'object') {
              const maxRPM = sanitizeNumber((args.engine as any).maxRPM, 'engine.maxRPM');
              const torqueCurveInput = (args.engine as any).torqueCurve;
              if (!Array.isArray(torqueCurveInput) || torqueCurveInput.length === 0) {
                throw new Error('engine.torqueCurve must be a non-empty array of [RPM, Torque] pairs');
              }
              const torqueCurve = torqueCurveInput.map((point: any, index: number) => {
                if (!Array.isArray(point) || point.length < 2) {
                  throw new Error(`Invalid torque curve entry at index ${index}: expected [RPM, Torque]`);
                }
                const rpm = sanitizeNumber(point[0], `engine.torqueCurve[${index}][0]`);
                const torque = sanitizeNumber(point[1], `engine.torqueCurve[${index}][1]`);
                return [rpm, torque] as [number, number];
              });
              engine = { maxRPM, torqueCurve };
            }

            let transmission: { gears: number[]; finalDriveRatio: number } | undefined;
            if (args.transmission && typeof args.transmission === 'object') {
              const gearsInput = (args.transmission as any).gears;
              if (!Array.isArray(gearsInput) || gearsInput.length === 0) {
                throw new Error('transmission.gears must be a non-empty array of numbers');
              }
              const gears = gearsInput.map((value: any, index: number) => sanitizeNumber(value, `transmission.gears[${index}]`));
              const finalDriveRatio = sanitizeNumber((args.transmission as any).finalDriveRatio, 'transmission.finalDriveRatio');
              transmission = { gears, finalDriveRatio };
            }

            const pluginDependencies = Array.isArray(args.pluginDependencies)
              ? args.pluginDependencies
                  .map((dep: any) => (typeof dep === 'string' ? dep.trim() : ''))
                  .filter((dep: string) => dep.length > 0)
              : undefined;

            const res = await tools.physicsTools.configureVehicle({
              vehicleName,
              vehicleType,
              wheels,
              engine,
              transmission,
              pluginDependencies
            });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown animation/physics action: ${args.action}`);
        }

// 6. EFFECTS SYSTEM
      case 'create_effect':
        switch (requireAction(args)) {
          case 'particle': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the particle effect details for create_effect.particle',
              {
                effectType: {
                  type: 'string',
                  title: 'Effect Type',
                  description: 'Preset effect type to spawn (e.g., Fire, Smoke)'
                }
              }
            );
            const res = await tools.niagaraTools.createEffect({ effectType: args.effectType, name: args.name, location: args.location, scale: args.scale, customParameters: args.customParameters });
            return cleanObject(res);
          }
          case 'niagara': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the Niagara system path for create_effect.niagara',
              {
                systemPath: {
                  type: 'string',
                  title: 'Niagara System Path',
                  description: 'Asset path of the Niagara system to spawn'
                }
              }
            );
            const systemPath = requireNonEmptyString(args.systemPath, 'systemPath', 'Invalid systemPath');
            const verifyResult = await tools.bridge.executePythonWithResult(`
import unreal, json
path = r"${systemPath}"
exists = unreal.EditorAssetLibrary.does_asset_exist(path)
print('RESULT:' + json.dumps({'success': exists, 'exists': exists, 'path': path}))
`.trim());
            if (!verifyResult?.exists) {
              return cleanObject({ success: false, error: `Niagara system not found at ${systemPath}` });
            }
            const loc = Array.isArray(args.location)
              ? { x: args.location[0], y: args.location[1], z: args.location[2] }
              : args.location || { x: 0, y: 0, z: 0 };
            const res = await tools.niagaraTools.spawnEffect({
              systemPath,
              location: [loc.x ?? 0, loc.y ?? 0, loc.z ?? 0],
              rotation: Array.isArray(args.rotation) ? args.rotation : undefined,
              scale: args.scale
            });
            return cleanObject(res);
          }
          case 'debug_shape': {
            const shapeInput = args.shape ?? 'Sphere';
            const shape = String(shapeInput).trim().toLowerCase();
            const originalShapeLabel = String(shapeInput).trim() || 'shape';
            const loc = args.location || { x: 0, y: 0, z: 0 };
            const size = args.size || 100;
            const color = args.color || [255, 0, 0, 255];
            const duration = args.duration || 5;
            if (shape === 'line') {
              const end = args.end || { x: loc.x + size, y: loc.y, z: loc.z };
              return cleanObject(await tools.debugTools.drawDebugLine({ start: [loc.x, loc.y, loc.z], end: [end.x, end.y, end.z], color, duration }));
            } else if (shape === 'box') {
              const extent = [size, size, size];
              return cleanObject(await tools.debugTools.drawDebugBox({ center: [loc.x, loc.y, loc.z], extent, color, duration }));
            } else if (shape === 'sphere') {
              return cleanObject(await tools.debugTools.drawDebugSphere({ center: [loc.x, loc.y, loc.z], radius: size, color, duration }));
            } else if (shape === 'capsule') {
              return cleanObject(await tools.debugTools.drawDebugCapsule({ center: [loc.x, loc.y, loc.z], halfHeight: size, radius: Math.max(10, size/3), color, duration }));
            } else if (shape === 'cone') {
              return cleanObject(await tools.debugTools.drawDebugCone({ origin: [loc.x, loc.y, loc.z], direction: [0,0,1], length: size, angleWidth: 0.5, angleHeight: 0.5, color, duration }));
            } else if (shape === 'arrow') {
              const end = args.end || { x: loc.x + size, y: loc.y, z: loc.z };
              return cleanObject(await tools.debugTools.drawDebugArrow({ start: [loc.x, loc.y, loc.z], end: [end.x, end.y, end.z], color, duration }));
            } else if (shape === 'point') {
              return cleanObject(await tools.debugTools.drawDebugPoint({ location: [loc.x, loc.y, loc.z], size, color, duration }));
            } else if (shape === 'text' || shape === 'string') {
              const text = args.text || 'Debug';
              return cleanObject(await tools.debugTools.drawDebugString({ location: [loc.x, loc.y, loc.z], text, color, duration }));
            }
            // Default fallback
            return cleanObject({ success: false, error: `Unsupported debug shape: ${originalShapeLabel}` });
          }
          case 'spawn_niagara': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the Niagara system path for create_effect.spawn_niagara',
              {
                systemPath: {
                  type: 'string',
                  title: 'Niagara System Path',
                  description: 'Asset path of the Niagara system to spawn'
                }
              }
            );
            const systemPath = requireNonEmptyString(args.systemPath, 'systemPath', 'Invalid systemPath');
            const loc = Array.isArray(args.location)
              ? { x: args.location[0], y: args.location[1], z: args.location[2] }
              : args.location || { x: 0, y: 0, z: 0 };
            const res = await tools.niagaraTools.spawnEffect({
              systemPath,
              location: [loc.x ?? 0, loc.y ?? 0, loc.z ?? 0],
              rotation: Array.isArray(args.rotation) ? args.rotation : undefined,
              scale: args.scale,
              autoDestroy: args.autoDestroy,
              attachToActor: args.attachToActor
            });
            return cleanObject(res);
          }
          case 'set_niagara_parameter': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the parameter details for create_effect.set_niagara_parameter',
              {
                systemName: {
                  type: 'string',
                  title: 'System Name',
                  description: 'Name of the Niagara system'
                },
                parameterName: {
                  type: 'string',
                  title: 'Parameter Name',
                  description: 'Name of the parameter to set'
                },
                parameterType: {
                  type: 'string',
                  title: 'Parameter Type',
                  description: 'Type of parameter (Float, Vector, Color, Bool, Int)'
                }
              }
            );
            const systemName = requireNonEmptyString(args.systemName, 'systemName', 'Invalid systemName');
            const parameterName = requireNonEmptyString(args.parameterName, 'parameterName', 'Invalid parameterName');
            const parameterType = requireNonEmptyString(args.parameterType, 'parameterType', 'Invalid parameterType');
            const res = await tools.niagaraTools.setParameter({
              systemName,
              parameterName,
              parameterType: parameterType as 'Float' | 'Vector' | 'Color' | 'Bool' | 'Int',
              value: args.value,
              isUserParameter: args.isUserParameter
            });
            return cleanObject(res);
          }
          case 'clear_debug_shapes': {
            const res = await tools.debugTools.clearDebugShapes();
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown effect action: ${args.action}`);
        }

// 7. BLUEPRINT MANAGER
      case 'manage_blueprint':
        switch (requireAction(args)) {
          case 'create': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for manage_blueprint.create',
              {
                name: {
                  type: 'string',
                  title: 'Blueprint Name',
                  description: 'Name for the new Blueprint asset'
                },
                blueprintType: {
                  type: 'string',
                  title: 'Blueprint Type',
                  description: 'Base type such as Actor, Pawn, Character, etc.'
                }
              }
            );
            const res = await tools.blueprintTools.createBlueprint({
              name: args.name,
              blueprintType: args.blueprintType || 'Actor',
              savePath: args.savePath,
              parentClass: args.parentClass
            });
            return cleanObject(res);
          }
          case 'add_component': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for manage_blueprint.add_component',
              {
                name: {
                  type: 'string',
                  title: 'Blueprint Name',
                  description: 'Blueprint asset to modify'
                },
                componentType: {
                  type: 'string',
                  title: 'Component Type',
                  description: 'Component class to add (e.g., StaticMeshComponent)'
                },
                componentName: {
                  type: 'string',
                  title: 'Component Name',
                  description: 'Name for the new component'
                }
              }
            );
            const res = await tools.blueprintTools.addComponent({
              blueprintName: args.name,
              componentType: args.componentType,
              componentName: args.componentName,
              attachTo: args.attachTo,
              transform: args.transform,
              properties: args.properties,
              compile: typeof args.compile === 'boolean' ? args.compile : undefined,
              save: typeof args.save === 'boolean' ? args.save : undefined,
              timeoutMs: typeof args.timeoutMs === 'number' ? args.timeoutMs : undefined
            });
            return cleanObject(res);
          }
          case 'modify_scs': {
            const blueprintCandidate = typeof args.blueprintPath === 'string' ? args.blueprintPath : args.name;
            const blueprintPath = typeof blueprintCandidate === 'string' && blueprintCandidate.trim().length > 0
              ? blueprintCandidate.trim()
              : undefined;
            if (!blueprintPath) {
              throw new Error('blueprintPath (or name) is required for modify_scs');
            }

            if (!Array.isArray(args.operations) || args.operations.length === 0) {
              throw new Error('operations array is required for modify_scs');
            }

            const res = await tools.blueprintTools.modifyConstructionScript({
              blueprintPath,
              operations: args.operations,
              compile: typeof args.compile === 'boolean' ? args.compile : undefined,
              save: typeof args.save === 'boolean' ? args.save : undefined,
              timeoutMs: typeof args.timeoutMs === 'number' ? args.timeoutMs : undefined
            });

            return cleanObject(res);
          }
          case 'set_default': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for manage_blueprint.set_default',
              {
                propertyName: {
                  type: 'string',
                  title: 'Property Name',
                  description: 'Blueprint default property to set'
                }
              }
            );
            const propertyName = requireNonEmptyString(args.propertyName, 'propertyName', 'Missing required parameter: propertyName');
            const res = await tools.blueprintTools.setBlueprintDefault({
              blueprintName: args.name,
              propertyName,
              value: args.value
            });
            return cleanObject(res);
          }
          case 'add_variable': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for manage_blueprint.add_variable',
              {
                name: { type: 'string', title: 'Blueprint Name' },
                variableName: { type: 'string', title: 'Variable Name' },
                variableType: { type: 'string', title: 'Variable Type' }
              }
            );
            const res = await tools.blueprintTools.addVariable({
              blueprintName: args.name,
              variableName: args.variableName,
              variableType: args.variableType || 'Float',
              defaultValue: args.defaultValue,
              category: args.category,
              isReplicated: args.isReplicated,
              isPublic: args.isPublic,
              variablePinType: args.variablePinType
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
              category: args.category
            });
            return cleanObject(res);
          }
          case 'add_event': {
            const res = await tools.blueprintTools.addEvent({
              blueprintName: args.name,
              eventType: args.eventType || 'Custom',
              customEventName: args.customEventName,
              parameters: args.parameters
            });
            return cleanObject(res);
          }
            case 'set_variable_metadata': {
              const res = await tools.blueprintTools.setVariableMetadata({
                blueprintName: args.name,
                variableName: args.variableName,
                metadata: args.metadata
              });
              return cleanObject(res);
            }
            case 'add_construction_script': {
              const res = await tools.blueprintTools.addConstructionScript({
                blueprintName: args.name,
                scriptName: args.scriptName
              });
              return cleanObject(res);
            }
          case 'compile': {
            const res = await tools.blueprintTools.compileBlueprint({ blueprintName: args.name, saveAfterCompile: args.saveAfterCompile });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown blueprint action: ${args.action}`);
        }

// 8. ENVIRONMENT BUILDER
      case 'build_environment':
        switch (requireAction(args)) {
          case 'create_landscape': {
            const res = await tools.landscapeTools.createLandscape({ name: args.name, sizeX: args.sizeX, sizeY: args.sizeY, materialPath: args.materialPath });
            return cleanObject(res);
          }
          case 'sculpt': {
            const res = await tools.landscapeTools.sculptLandscape({ landscapeName: args.name, tool: args.tool, brushSize: args.brushSize, strength: args.strength });
            return cleanObject(res);
          }
          case 'add_foliage': {
            const res = await tools.foliageTools.addFoliageType({ name: args.name, meshPath: args.meshPath, density: args.density });
            return cleanObject(res);
          }
          case 'paint_foliage': {
            const pos = args.position ? [args.position.x || 0, args.position.y || 0, args.position.z || 0] : [0,0,0];
            const res = await tools.foliageTools.paintFoliage({ foliageType: args.foliageType, position: pos, brushSize: args.brushSize, paintDensity: args.paintDensity, eraseMode: args.eraseMode });
            return cleanObject(res);
          }
          case 'create_procedural_terrain': {
            const loc = args.location ? [args.location.x||0, args.location.y||0, args.location.z||0] : [0,0,0];
            const res = await tools.buildEnvAdvanced.createProceduralTerrain({
              name: args.name || 'ProceduralTerrain',
              location: loc as [number,number,number],
              sizeX: args.sizeX,
              sizeY: args.sizeY,
              subdivisions: args.subdivisions,
              heightFunction: args.heightFunction,
              material: args.materialPath
            });
            return cleanObject(res);
          }
          case 'create_procedural_foliage': {
            if (!args.bounds || !args.bounds.location || !args.bounds.size) throw new Error('bounds.location and bounds.size are required');
            const bounds = {
              location: [args.bounds.location.x||0, args.bounds.location.y||0, args.bounds.location.z||0] as [number,number,number],
              size: [args.bounds.size.x||1000, args.bounds.size.y||1000, args.bounds.size.z||100] as [number,number,number]
            };
            const res = await tools.buildEnvAdvanced.createProceduralFoliage({
              name: args.name || 'ProceduralFoliage',
              bounds,
              foliageTypes: args.foliageTypes || [],
              seed: args.seed
            });
            return cleanObject(res);
          }
          case 'add_foliage_instances': {
            if (!args.foliageType) throw new Error('foliageType is required');
            if (!Array.isArray(args.transforms)) throw new Error('transforms array is required');
            const transforms = (args.transforms as any[]).map(t => ({
              location: [t.location?.x||0, t.location?.y||0, t.location?.z||0] as [number,number,number],
              rotation: t.rotation ? [t.rotation.pitch||0, t.rotation.yaw||0, t.rotation.roll||0] as [number,number,number] : undefined,
              scale: t.scale ? [t.scale.x||1, t.scale.y||1, t.scale.z||1] as [number,number,number] : undefined
            }));
            const res = await tools.buildEnvAdvanced.addFoliageInstances({ foliageType: args.foliageType, transforms });
            return cleanObject(res);
          }
          case 'create_landscape_grass_type': {
            const res = await tools.buildEnvAdvanced.createLandscapeGrassType({
              name: args.name || 'GrassType',
              meshPath: args.meshPath,
              density: args.density,
              minScale: args.minScale,
              maxScale: args.maxScale
            });
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown environment action: ${args.action}`);
        }

      // 9. SYSTEM CONTROL
case 'system_control':
        switch (requireAction(args)) {
          case 'profile': {
            const res = await tools.performanceTools.startProfiling({ type: args.profileType, duration: args.duration });
            return cleanObject(res);
          }
          case 'show_fps': {
            const res = await tools.performanceTools.showFPS({ enabled: !!args.enabled, verbose: !!args.verbose });
            return cleanObject(res);
          }
          case 'set_quality': {
            const res = await tools.performanceTools.setScalability({ category: args.category, level: args.level });
            return cleanObject(res);
          }
          case 'play_sound': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide the audio asset for system_control.play_sound',
              {
                soundPath: {
                  type: 'string',
                  title: 'Sound Asset Path',
                  description: 'Asset path of the sound to play'
                }
              }
            );
            const soundPath = requireNonEmptyString(args.soundPath, 'soundPath', 'Missing required parameter: soundPath');
            if (args.location && typeof args.location === 'object') {
              const loc = [args.location.x || 0, args.location.y || 0, args.location.z || 0];
              const res = await tools.audioTools.playSoundAtLocation({ soundPath, location: loc as [number, number, number], volume: args.volume, pitch: args.pitch, startTime: args.startTime });
              return cleanObject(res);
            }
            const res = await tools.audioTools.playSound2D({ soundPath, volume: args.volume, pitch: args.pitch, startTime: args.startTime });
            return cleanObject(res);
          }
          case 'create_widget': {
            await elicitMissingPrimitiveArgs(
              tools,
              args,
              'Provide details for system_control.create_widget',
              {
                widgetName: {
                  type: 'string',
                  title: 'Widget Name',
                  description: 'Name for the new UI widget asset'
                },
                widgetType: {
                  type: 'string',
                  title: 'Widget Type',
                  description: 'Widget type such as HUD, Menu, Overlay, etc.'
                }
              }
            );
            const widgetName = requireNonEmptyString(args.widgetName ?? args.name, 'widgetName', 'Missing required parameter: widgetName');
            const widgetType = requireNonEmptyString(args.widgetType, 'widgetType', 'Missing required parameter: widgetType');
            const res = await tools.uiTools.createWidget({ name: widgetName, type: widgetType as any, savePath: args.savePath });
            return cleanObject(res);
          }
          case 'show_widget': {
            const res = await tools.uiTools.setWidgetVisibility({ widgetName: args.widgetName, visible: args.visible !== false });
            return cleanObject(res);
          }
          case 'screenshot': {
            const res = await tools.visualTools.takeScreenshot({ resolution: args.resolution });
            return cleanObject(res);
          }
          case 'engine_start': {
            const res = await tools.engineTools.launchEditor({ editorExe: args.editorExe, projectPath: args.projectPath });
            return cleanObject(res);
          }
          case 'engine_quit': {
            const res = await tools.engineTools.quitEditor();
            return cleanObject(res);
          }
          default:
            throw new Error(`Unknown system action: ${args.action}`);
        }

      // 10. CONSOLE COMMAND - handle validation here
      case 'console_command':
        if (!args.command || typeof args.command !== 'string' || args.command.trim() === '') {
          return { success: true, message: 'Empty command' } as any;
        }
        // Basic safety filter
        const cmd = String(args.command).trim();
        const blocked = [/\bquit\b/i, /\bexit\b/i, /debugcrash/i];
        if (blocked.some(r => r.test(cmd))) {
          return { success: false, error: 'Command blocked for safety' } as any;
        }
        try {
          const raw = await tools.bridge.executeConsoleCommand(cmd);
          const summary = tools.bridge.summarizeConsoleCommand(cmd, raw);
          const output = summary.output || '';
          const looksInvalid = /unknown|invalid/i.test(output);
          return cleanObject({
            success: summary.returnValue !== false && !looksInvalid,
            command: summary.command,
            output: output || undefined,
            logLines: summary.logLines?.length ? summary.logLines : undefined,
            returnValue: summary.returnValue,
            message: !looksInvalid
              ? (output || 'Command executed')
              : undefined,
            error: looksInvalid ? output : undefined,
            raw: summary.raw
          });
        } catch (e: any) {
          return cleanObject({ success: false, command: cmd, error: e?.message || String(e) });
        }

      // 11. PYTHON EXECUTION
      case 'execute_python': {
        const automationInfo = getAutomationBridgeInfo(tools);
        const decision = decideAutomationTransport(args.transport, automationInfo);
        const automationBridge = automationInfo.instance;
        const warnings: string[] = [];

        const addWarning = (value?: string) => {
          if (!value) return;
          if (!warnings.includes(value)) {
            warnings.push(value);
          }
        };

        const hasTemplate = typeof args.template === 'string' && args.template.trim().length > 0;
        if (hasTemplate) {
          const templateName = args.template.trim();
          try {
            const templateResult = await tools.bridge.executeEditorFunction(templateName, args.templateParams);
            const success = templateResult?.success !== false;
            const message = typeof templateResult?.message === 'string'
              ? templateResult.message
              : `Template ${templateName} executed${success ? '' : ' with errors'}`;
            const error = success
              ? undefined
              : (typeof templateResult?.error === 'string' ? templateResult.error : 'Template reported failure');
            const templateWarnings = Array.isArray(templateResult?.warnings) ? templateResult.warnings : undefined;

            const payload: Record<string, unknown> = {
              success,
              result: templateResult,
              message,
              error,
              transport: 'automation_bridge'
            };

            const combinedWarnings = [...(templateWarnings ?? []), ...warnings].filter(Boolean);
            if (combinedWarnings.length > 0) {
              payload.warnings = combinedWarnings;
            }

            return cleanObject(payload);
          } catch (err: any) {
            const payload: Record<string, unknown> = {
              success: false,
              error: err?.message || String(err),
              message: `Failed to execute template ${templateName}`,
              transport: 'automation_bridge'
            };
            if (warnings.length > 0) {
              payload.warnings = warnings;
            }
            return cleanObject(payload);
          }
        }

        const scriptArg = requireNonEmptyString(args.script, 'script', 'Missing required parameter: script');
        let script = scriptArg;

        if (args.context && typeof args.context === 'object') {
          try {
            const contextJson = JSON.stringify(args.context);
            const escapedJson = escapePythonString(contextJson);
            const prelude = `import json\nMCP_INPUT = json.loads("${escapedJson}")`;
            script = `${prelude}\n${script}`;
          } catch (err) {
            throw new Error(`Failed to serialise context: ${(err as Error)?.message || err}`);
          }
        }

        if (!automationBridge?.sendAutomationRequest) {
          throw new Error('Automation bridge not connected');
        }

        const timeoutMs =
          typeof args.timeoutMs === 'number' && Number.isFinite(args.timeoutMs) && args.timeoutMs > 0
            ? Math.floor(args.timeoutMs)
            : undefined;

        try {
          const response = await automationBridge.sendAutomationRequest(
            'execute_editor_python',
            { script },
            { timeoutMs }
          );
          const success = response.success !== false;
          const message = typeof response.message === 'string'
            ? response.message
            : success
              ? 'Python script executed via automation bridge.'
              : 'Automation bridge reported failure.';
          const error = success
            ? undefined
            : (typeof response.error === 'string' ? response.error : 'AUTOMATION_BRIDGE_FAILURE');

          const payload: Record<string, unknown> = {
            success,
            message,
            error,
            transport: 'automation_bridge',
            result: response.result,
            bridge: {
              requestId: response.requestId,
              success: response.success !== false,
              error: response.error
            }
          };

          if (warnings.length > 0) {
            payload.warnings = warnings;
          }

          return cleanObject(payload);
        } catch (err: any) {
          if (!decision.allowFallback) {
            return cleanObject({
              success: false,
              error: err?.message || String(err),
              message: 'Automation bridge execution failed',
              transport: 'automation_bridge'
            });
          }
          addWarning(decision.fallbackReason);
          addWarning(`Automation bridge error: ${err?.message || String(err)}.`);
        }

        const captureResult = args.captureResult !== false;

        try {
          const result = captureResult
            ? await tools.bridge.executePythonWithResult(script)
            : await tools.bridge.executePython(script);

          const success = typeof result === 'object' && result !== null && typeof (result as any).success === 'boolean'
            ? Boolean((result as any).success)
            : true;
          const existingWarnings = Array.isArray((result as any)?.warnings)
            ? (result as any).warnings
            : undefined;
          const message = typeof (result as any)?.message === 'string'
            ? (result as any).message
            : (typeof result === 'string' ? result : captureResult ? 'Python script executed' : 'Python command sent');
          const error = success ? undefined : (typeof (result as any)?.error === 'string' ? (result as any).error : 'Python execution reported failure');

          const payload: Record<string, unknown> = {
            success,
            result,
            message,
            error,
            transport: 'automation_bridge'
          };

          const combinedWarnings = [...(existingWarnings ?? []), ...warnings].filter(Boolean);
          if (combinedWarnings.length > 0) {
            payload.warnings = combinedWarnings;
          }

          return cleanObject(payload);
        } catch (err: any) {
          const payload: Record<string, unknown> = {
            success: false,
            error: err?.message || String(err),
            message: 'Python execution failed',
            transport: 'automation_bridge'
          };
          if (warnings.length > 0) {
            payload.warnings = warnings;
          }
          return cleanObject(payload);
        }
      }


      // 12. REMOTE CONTROL PRESETS - Direct implementation
      case 'manage_rc':
        // Handle RC operations directly through RcTools
        let rcResult: any;
        const rcAction = requireAction(args);
        switch (rcAction) {
          // Support both 'create_preset' and 'create' for compatibility
          case 'create_preset':
          case 'create':
            // Support both 'name' and 'presetName' parameter names
            const presetName = args.name || args.presetName;
            if (!presetName) throw new Error('Missing required parameter: name or presetName');
            rcResult = await tools.rcTools.createPreset({ 
              name: presetName, 
              path: args.path 
            });
            // Return consistent output with presetId for tests
            if (rcResult.success) {
              rcResult.message = `Remote Control preset created: ${presetName}`;
              // Ensure presetId is set (for test compatibility)
              if (rcResult.presetPath && !rcResult.presetId) {
                rcResult.presetId = rcResult.presetPath;
              }
            }
            break;
            
          case 'list':
            // List all presets - implement via RcTools
            rcResult = await tools.rcTools.listPresets();
            break;
            
          case 'delete':
          case 'delete_preset':
            const presetIdentifier = args.presetId || args.presetPath;
            if (!presetIdentifier) throw new Error('Missing required parameter: presetId');
            rcResult = await tools.rcTools.deletePreset(presetIdentifier);
            if (rcResult.success) {
              rcResult.message = 'Preset deleted successfully';
            }
            break;
            
          case 'expose_actor':
            if (!args.presetPath) throw new Error('Missing required parameter: presetPath');
            if (!args.actorName) throw new Error('Missing required parameter: actorName');
            
            rcResult = await tools.rcTools.exposeActor({ 
              presetPath: args.presetPath,
              actorName: args.actorName
            });
            if (rcResult.success) {
              rcResult.message = `Actor '${args.actorName}' exposed to preset`;
            }
            break;
            
          case 'expose_property':
          case 'expose':  // Support simplified name from tests
            // Support both presetPath and presetId
            const presetPathExp = args.presetPath || args.presetId;
            if (!presetPathExp) throw new Error('Missing required parameter: presetPath or presetId');
            if (!args.objectPath) throw new Error('Missing required parameter: objectPath');
            if (!args.propertyName) throw new Error('Missing required parameter: propertyName');
            
            rcResult = await tools.rcTools.exposeProperty({ 
              presetPath: presetPathExp,
              objectPath: args.objectPath, 
              propertyName: args.propertyName 
            });
            if (rcResult.success) {
              rcResult.message = `Property '${args.propertyName}' exposed to preset`;
            }
            break;
            
          case 'list_fields':
          case 'get_exposed':  // Support test naming
            const presetPathList = args.presetPath || args.presetId;
            if (!presetPathList) throw new Error('Missing required parameter: presetPath or presetId');
            
            rcResult = await tools.rcTools.listFields({ 
              presetPath: presetPathList 
            });
            // Map 'fields' to 'exposedProperties' for test compatibility
            if (rcResult.success && rcResult.fields) {
              rcResult.exposedProperties = rcResult.fields;
            }
            break;
            
          case 'set_property':
          case 'set_value':  // Support test naming
            // Support both patterns
            const objPathSet = args.objectPath || args.presetId;
            const propNameSet = args.propertyName || args.propertyLabel;
            
            if (!objPathSet) throw new Error('Missing required parameter: objectPath or presetId');
            if (!propNameSet) throw new Error('Missing required parameter: propertyName or propertyLabel');
            if (args.value === undefined) throw new Error('Missing required parameter: value');
            
            rcResult = await tools.rcTools.setProperty({ 
              objectPath: objPathSet,
              propertyName: propNameSet,
              value: args.value 
            });
            if (rcResult.success) {
              rcResult.message = `Property '${propNameSet}' value updated`;
            }
            break;
            
          case 'get_property':
          case 'get_value':  // Support test naming
            const objPathGet = args.objectPath || args.presetId;
            const propNameGet = args.propertyName || args.propertyLabel;
            
            if (!objPathGet) throw new Error('Missing required parameter: objectPath or presetId');
            if (!propNameGet) throw new Error('Missing required parameter: propertyName or propertyLabel');
            
            rcResult = await tools.rcTools.getProperty({ 
              objectPath: objPathGet,
              propertyName: propNameGet 
            });
            break;
            
          case 'call_function':
            if (!args.presetId) throw new Error('Missing required parameter: presetId');
            if (!args.functionLabel) throw new Error('Missing required parameter: functionLabel');
            
            // For now, return not implemented
            rcResult = { 
              success: false, 
              error: 'Function calls not yet implemented' 
            };
            break;
            
          default:
            throw new Error(`Unknown RC action: ${rcAction}. Valid actions are: create_preset, expose_actor, expose_property, list_fields, set_property, get_property, or their simplified versions: create, list, delete, expose, get_exposed, set_value, get_value, call_function`);
        }
        
        // Return result directly - MCP formatting will be handled by response validator
        // Clean to prevent circular references
        return cleanObject(rcResult);

  // 13. SEQUENCER / CINEMATICS
      case 'manage_sequence':
        // Direct handling for sequence operations
        const seqResult = await (async () => {
          const sequenceTools = tools.sequenceTools;
          if (!sequenceTools) throw new Error('Sequence tools not available');
          const action = requireAction(args);
          
          switch (action) {
            case 'create':
              return await sequenceTools.create({ name: args.name, path: args.path });
            case 'open':
              return await sequenceTools.open({ path: args.path });
            case 'add_camera':
              return await sequenceTools.addCamera({ spawnable: args.spawnable !== false });
            case 'add_actor':
              return await sequenceTools.addActor({ actorName: args.actorName });
            case 'add_actors':
              if (!args.actorNames) throw new Error('Missing required parameter: actorNames');
              return await sequenceTools.addActors({ actorNames: args.actorNames });
            case 'remove_actors':
              if (!args.actorNames) throw new Error('Missing required parameter: actorNames');
              return await sequenceTools.removeActors({ actorNames: args.actorNames });
            case 'get_bindings':
              return await sequenceTools.getBindings({ path: args.path });
            case 'add_spawnable_from_class':
              if (!args.className) throw new Error('Missing required parameter: className');
              return await sequenceTools.addSpawnableFromClass({ className: args.className, path: args.path });
            case 'play':
              return await sequenceTools.play({ loopMode: args.loopMode });
            case 'pause':
              return await sequenceTools.pause();
            case 'stop':
              return await sequenceTools.stop();
            case 'set_properties':
              return await sequenceTools.setSequenceProperties({
                path: args.path,
                frameRate: args.frameRate,
                lengthInFrames: args.lengthInFrames,
                playbackStart: args.playbackStart,
                playbackEnd: args.playbackEnd
              });
            case 'get_properties':
              return await sequenceTools.getSequenceProperties({ path: args.path });
            case 'set_playback_speed':
              if (args.speed === undefined) throw new Error('Missing required parameter: speed');
              return await sequenceTools.setPlaybackSpeed({ speed: args.speed });
            default:
              throw new Error(`Unknown sequence action: ${action}`);
          }
        })();
        
        // Return result directly - MCP formatting will be handled by response validator
        // Clean to prevent circular references
        return cleanObject(seqResult);
  // 14. INTROSPECTION
case 'inspect':
  const inspectAction = requireAction(args);
  switch (inspectAction) {
          case 'inspect_object': {
            const res = await tools.introspectionTools.inspectObject({ objectPath: args.objectPath, detailed: args.detailed });
            return cleanObject(res);
          }
          case 'get_property': {
            const objectPath = requireNonEmptyString(args.objectPath, 'objectPath', 'Missing required parameter: objectPath');
            const propertyName = requireNonEmptyString(args.propertyName, 'propertyName', 'Missing required parameter: propertyName');
            const automationInfo = getAutomationBridgeInfo(tools);
            decideAutomationTransport(args.transport, automationInfo);
            const automationBridge = automationInfo.instance;

            if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
              throw new Error('Automation bridge not connected');
            }

            const timeoutMs =
              typeof args.timeoutMs === 'number' && Number.isFinite(args.timeoutMs) && args.timeoutMs > 0
                ? Math.floor(args.timeoutMs)
                : undefined;

            try {
              const response = await automationBridge.sendAutomationRequest(
                'get_object_property',
                { objectPath, propertyName },
                { timeoutMs }
              );

              const success = response.success !== false;
              const rawResult =
                response.result && typeof response.result === 'object'
                  ? { ...(response.result as Record<string, unknown>) }
                  : response.result;
              const value =
                (rawResult as any)?.value ??
                (rawResult as any)?.propertyValue ??
                (success ? rawResult : undefined);

              const message = typeof response.message === 'string'
                ? response.message
                : success
                  ? 'Property retrieved via automation bridge.'
                  : 'Automation bridge reported failure.';

              const payload: Record<string, unknown> = {
                success,
                message,
                error: success ? undefined : response.error ?? 'AUTOMATION_BRIDGE_FAILURE',
                value,
                propertyValue: value,
                transport: 'automation_bridge',
                bridge: {
                  requestId: response.requestId,
                  success: response.success !== false,
                  error: response.error
                },
                raw: rawResult
              };

              return cleanObject(payload);
            } catch (err: any) {
              return cleanObject({
                success: false,
                error: err?.message || String(err),
                message: 'Automation bridge property lookup failed',
                transport: 'automation_bridge'
              });
            }
          }
          case 'set_property': {
            const objectPath = requireNonEmptyString(args.objectPath, 'objectPath', 'Missing required parameter: objectPath');
            const propertyName = requireNonEmptyString(args.propertyName, 'propertyName', 'Missing required parameter: propertyName');
            const automationInfo = getAutomationBridgeInfo(tools);
            decideAutomationTransport(args.transport, automationInfo);
            const automationBridge = automationInfo.instance;

            if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
              throw new Error('Automation bridge not connected');
            }

            const payload: Record<string, unknown> = {
              objectPath,
              propertyName,
              value: args.value
            };

            if (args.markDirty !== undefined) {
              payload.markDirty = Boolean(args.markDirty);
            }

            const timeoutMs =
              typeof args.timeoutMs === 'number' && Number.isFinite(args.timeoutMs) && args.timeoutMs > 0
                ? Math.floor(args.timeoutMs)
                : undefined;

            try {
              const response = await automationBridge.sendAutomationRequest(
                'set_object_property',
                payload,
                { timeoutMs }
              );

              const success = response.success !== false;
              const rawResult =
                response.result && typeof response.result === 'object'
                  ? { ...(response.result as Record<string, unknown>) }
                  : response.result;
              const message = typeof response.message === 'string'
                ? response.message
                : success
                  ? 'Property updated via automation bridge.'
                  : 'Automation bridge reported failure.';

              const resultPayload: Record<string, unknown> = {
                success,
                message,
                error: success ? undefined : response.error ?? 'AUTOMATION_BRIDGE_FAILURE',
                transport: 'automation_bridge',
                bridge: {
                  requestId: response.requestId,
                  success: response.success !== false,
                  error: response.error
                },
                result: response.result,
                raw: rawResult
              };

              return cleanObject(resultPayload);
            } catch (err: any) {
              return cleanObject({
                success: false,
                error: err?.message || String(err),
                message: 'Automation bridge property update failed',
                transport: 'automation_bridge'
              });
            }
          }
          default:
            throw new Error(`Unknown inspect action: ${inspectAction}`);
        }
        

      default:
        throw new Error(`Unknown consolidated tool: ${name}`);
    }

// All cases return (or throw) above; this is a type guard for exhaustiveness.

  } catch (err: any) {
    const duration = Date.now() - startTime;
    console.log(`[ConsolidatedToolHandler] Failed execution of ${name} after ${duration}ms: ${err?.message || String(err)}`);
    
    // Return consistent error structure matching regular tool handlers
    const errorMessage = err?.message || String(err);
    const isTimeout = errorMessage.includes('timeout');
    
    return {
      content: [{
        type: 'text',
        text: isTimeout 
          ? `Tool ${name} timed out. Please check Unreal Engine connection.`
          : `Failed to execute ${name}: ${errorMessage}`
      }],
      isError: true
    };
  }
}
