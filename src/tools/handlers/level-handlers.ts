import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, LevelArgs, AutomationResponse } from '../../types/handler-types.js';
import { executeAutomationRequest, normalizePathFields, requireNonEmptyString, validateSecurityPatterns } from './common-handlers.js';

// AutomationResponse now imported from types/handler-types.js

const LEVEL_STRUCTURE_ONLY_ACTIONS = new Set([
  'load_cells',
  'set_datalayer',
  'cleanup_invalid_datalayers',
  'create_datalayer'
]);
const LIGHT_TYPE_CLASS_PATHS = new Map<string, string>([
  ['point', '/Script/Engine.PointLight'],
  ['pointlight', '/Script/Engine.PointLight'],
  ['directional', '/Script/Engine.DirectionalLight'],
  ['directionallight', '/Script/Engine.DirectionalLight'],
  ['spot', '/Script/Engine.SpotLight'],
  ['spotlight', '/Script/Engine.SpotLight'],
  ['sky', '/Script/Engine.SkyLight'],
  ['skylight', '/Script/Engine.SkyLight'],
  ['rect', '/Script/Engine.RectLight'],
  ['rectlight', '/Script/Engine.RectLight']
]);
const CREATE_LIGHT_TYPES = new Set<string>([
  'point',
  'directional',
  'spot',
  'sky',
  'rect',
  'pointlight',
  'directionallight',
  'spotlight',
  'skylight',
  'rectlight'
]);

/** Result payload structure for level responses */
interface ResultPayload {
  exists?: boolean;
  path?: string;
  class?: string;
  error?: string;
  message?: string;
  [key: string]: unknown;
}

function normalizeLightType(lightType: unknown): string | undefined {
  return typeof lightType === 'string' ? lightType.trim().toLowerCase() : undefined;
}

function normalizeLevelPathValue(path: string): string {
  const normalized = normalizePathFields({ path }, ['path']);
  return normalized.path as string;
}

function normalizeLevelPathArray(paths: unknown): string[] | undefined {
  if (!Array.isArray(paths)) return undefined;
  return paths
    .filter((path): path is string => typeof path === 'string')
    .map(path => normalizeLevelPathValue(path));
}

/**
 * Normalize level arguments to support both camelCase and snake_case
 */
function normalizeLevelArgs(args: LevelArgs): LevelArgs {
  const raw = args as Record<string, unknown>;
  const assetPath = typeof raw.assetPath === 'string' ? raw.assetPath : undefined;
  const targetPath = typeof raw.targetPath === 'string' ? raw.targetPath : undefined;
  const pathAlias = typeof raw.path === 'string' ? raw.path : undefined;
  const mapped = {
    ...args,
    // Map snake_case to camelCase
    levelPath: (raw.level_path as string | undefined) ?? args.levelPath ?? assetPath ?? pathAlias,
    levelName: (raw.level_name as string | undefined) ?? args.levelName,
    savePath: (raw.save_path as string | undefined) ?? args.savePath,
    destinationPath: (raw.destination_path as string | undefined) ?? args.destinationPath ?? targetPath,
    subLevelPath: (raw.sublevelPath as string | undefined) ?? (raw.sub_level_path as string | undefined) ?? args.subLevelPath,
    parentLevel: (raw.parent_level as string | undefined) ?? args.parentLevel,
    parentPath: (raw.parent_path as string | undefined) ?? args.parentPath,
    streamingMethod: (raw.streaming_method as 'Blueprint' | 'AlwaysLoaded' | undefined) ?? args.streamingMethod,
    sourcePath: (raw.source_path as string | undefined) ?? args.sourcePath,
    newName: (raw.new_name as string | undefined) ?? (args as Record<string, unknown>).newName as string | undefined,
    levelPaths: normalizeLevelPathArray(raw.level_paths ?? args.levelPaths),
    packagePath: (raw.package_path as string | undefined) ?? args.packagePath,
    exportPath: (raw.export_path as string | undefined) ?? args.exportPath,
  };
  return normalizePathFields(mapped as Record<string, unknown>, [
    'levelPath',
    'savePath',
    'destinationPath',
    'subLevelPath',
    'parentLevel',
    'parentPath',
    'sourcePath',
    'packagePath',
    'exportPath',
    'path'
  ]) as LevelArgs;
}

export async function handleLevelTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  // Normalize args to support both camelCase and snake_case
  const argsTyped = normalizeLevelArgs(args as LevelArgs);

  if (LEVEL_STRUCTURE_ONLY_ACTIONS.has(action)) {
    return cleanObject({
      success: false,
      error: 'INVALID_ACTION',
      message: `${action} belongs to manage_level_structure/world partition handlers, not manage_level`,
      action,
      tool: 'manage_level_structure'
    });
  }

  // Security validation: check for path traversal attempts
  const securityError = validateSecurityPatterns(argsTyped as Record<string, unknown>);
  if (securityError) {
    return cleanObject({
      success: false,
      error: 'SECURITY_VIOLATION',
      message: securityError,
      action
    });
  }

  switch (action) {
    case 'load':
    case 'load_level': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'load',
        levelPath,
        streaming: !!argsTyped.streaming
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'save':
    case 'save_level': {
      const targetPath = argsTyped.levelPath || argsTyped.savePath;
      if (targetPath) {
        const res = await executeAutomationRequest(tools, 'manage_level', {
          action: 'save_level_as',
          savePath: targetPath
        }) as Record<string, unknown>;
        return cleanObject(res);
      }
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'save',
        levelName: argsTyped.levelName
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'save_as':
    case 'save_level_as': {
      // Accept savePath, destinationPath, or levelPath as the target
      const targetPath = argsTyped.savePath || argsTyped.destinationPath || argsTyped.levelPath;
      if (!targetPath) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'savePath is required for save_as action',
          action
        };
      }
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'save_level_as',
        savePath: targetPath
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'create_level': {
      const levelPathStr = typeof argsTyped.levelPath === 'string' ? argsTyped.levelPath : '';
      const levelName = requireNonEmptyString(argsTyped.levelName || levelPathStr.split('/').pop() || '', 'levelName', 'Missing required parameter: levelName');
      // Use the inactive-world creation path, then load the saved level so
      // manage_level create_level preserves its historical "create and open"
      // behavior without calling the UE 5.7-crashy GEditor->NewMap path.
      const levelParentPath = argsTyped.savePath || argsTyped.levelPath || '/Game/Maps';
      const createRes = await executeAutomationRequest(tools, 'manage_level_structure', {
        subAction: 'create_level',
        levelPath: levelParentPath,
        levelName,
        bCreateWorldPartition: argsTyped.useWorldPartition ?? false,
        save: true
      }) as Record<string, unknown>;
      if (createRes.success !== true) {
        return cleanObject(createRes);
      }

      const result = createRes.result && typeof createRes.result === 'object' && !Array.isArray(createRes.result)
        ? createRes.result as Record<string, unknown>
        : {};
      const createdLevelPath = typeof result.levelPath === 'string'
        ? result.levelPath
        : `${String(levelParentPath).replace(/\/+$/, '')}/${levelName}`;

      const loadRes = await executeAutomationRequest(tools, 'manage_level', {
        action: 'load',
        levelPath: createdLevelPath,
        saveDirtyPackages: argsTyped.saveDirtyPackages === true
      }) as Record<string, unknown>;
      if (loadRes.success !== true) {
        return cleanObject(loadRes);
      }

      return cleanObject({
        ...createRes,
        result: {
          ...result,
          loaded: true
        }
      });
    }
    case 'add_sublevel': {
      const subLevelPath = requireNonEmptyString(argsTyped.subLevelPath || argsTyped.levelPath, 'subLevelPath', 'Missing required parameter: subLevelPath');
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'add_sublevel',
        subLevelPath,
        parentLevel: argsTyped.parentLevel || argsTyped.parentPath,
        streamingMethod: argsTyped.streamingMethod
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'stream': {
      const levelPath = typeof argsTyped.levelPath === 'string' ? argsTyped.levelPath : undefined;
      const levelName = typeof argsTyped.levelName === 'string' ? argsTyped.levelName : undefined;
      if (!levelPath && !levelName) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: levelPath (or levelName)',
          action
        });
      }
      if (typeof argsTyped.shouldBeLoaded !== 'boolean') {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: shouldBeLoaded (boolean)',
          action,
          levelPath,
          levelName
        });
      }

      const res = await executeAutomationRequest(tools, 'stream_level', {
        levelPath,
        levelName,
        shouldBeLoaded: argsTyped.shouldBeLoaded,
        shouldBeVisible: argsTyped.shouldBeVisible
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'unload': {
      // unload is a convenience alias for stream with shouldBeLoaded=false
      const levelPath = typeof argsTyped.levelPath === 'string' ? argsTyped.levelPath : undefined;
      const levelName = typeof argsTyped.levelName === 'string' ? argsTyped.levelName : undefined;
      if (!levelPath && !levelName) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: levelPath (or levelName)',
          action
        });
      }
      const res = await executeAutomationRequest(tools, 'stream_level', {
        levelPath,
        levelName,
        shouldBeLoaded: false,
        shouldBeVisible: false
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'create_light': {
      // Validate required parameters for create_light
      const lightType = argsTyped.type || argsTyped.lightType;
      if (!lightType || typeof lightType !== 'string' || lightType.trim() === '') {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: type (or lightType). Valid types: Point, Directional, Spot, Sky, Rect, PointLight, DirectionalLight, SpotLight, SkyLight, RectLight',
          action
        });
      }
      // Validate light type is one of the supported types
      const normalizedLightType = normalizeLightType(lightType);
      if (!normalizedLightType || !CREATE_LIGHT_TYPES.has(normalizedLightType)) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: `Invalid light type: ${lightType}. Valid types: Point, Directional, Spot, Sky, Rect, PointLight, DirectionalLight, SpotLight, SkyLight, RectLight`,
          action
        });
      }
      const properties: Record<string, unknown> = {};
      if (typeof argsTyped.intensity === 'number') {
        properties.intensity = argsTyped.intensity;
      }
      const color = (argsTyped as Record<string, unknown>).color;
      if (Array.isArray(color) && color.length >= 3) {
        properties.color = {
          r: Number(color[0]),
          g: Number(color[1]),
          b: Number(color[2]),
          a: color.length > 3 ? Number(color[3]) : 1
        };
      } else if (color && typeof color === 'object') {
        properties.color = color;
      }

      const res = await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'create_light',
        lightType,
        name: argsTyped.name,
        location: argsTyped.location,
        rotation: argsTyped.rotation,
        properties: Object.keys(properties).length > 0 ? properties : undefined
      });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'spawn_light': {
      // Fallback to control_actor spawn if manage_level spawn_light is not supported
      const lightType = normalizeLightType(argsTyped.lightType) ?? normalizeLightType((argsTyped as Record<string, unknown>).type) ?? 'point';
      const classPath = LIGHT_TYPE_CLASS_PATHS.get(lightType) || '/Script/Engine.PointLight';

      try {
        const res = await executeAutomationRequest(tools, 'control_actor', {
          action: 'spawn',
          classPath,
          actorName: argsTyped.name,
          location: argsTyped.location,
          rotation: argsTyped.rotation,
          scale: argsTyped.scale
        }) as Record<string, unknown>;
        return { ...cleanObject(res), action: 'spawn_light' };
      } catch (_e) {
        return await executeAutomationRequest(tools, 'manage_level', args) as Record<string, unknown>;
      }
    }
    case 'build_lighting': {
      // Pass user-provided values, default to false if not specified
      const argsRecord = args as Record<string, unknown>;
      const res = await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'build_lighting',
        quality: (argsTyped.quality as string) || 'Preview',
        buildOnlySelected: typeof argsRecord.buildOnlySelected === 'boolean' ? argsRecord.buildOnlySelected : false,
        buildReflectionCaptures: typeof argsRecord.buildReflectionCaptures === 'boolean' ? argsRecord.buildReflectionCaptures : false
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'export_level': {
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'export_level',
        levelPath: argsTyped.levelPath,
        exportPath: argsTyped.exportPath ?? argsTyped.destinationPath ?? '',
        timeoutMs: typeof argsTyped.timeoutMs === 'number' ? argsTyped.timeoutMs : undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'import_level': {
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'import_level',
        packagePath: argsTyped.packagePath ?? argsTyped.sourcePath ?? '',
        destinationPath: argsTyped.destinationPath,
        overwrite: argsTyped.overwrite === true,
        timeoutMs: typeof argsTyped.timeoutMs === 'number' ? argsTyped.timeoutMs : undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'list_levels': {
      const res = await executeAutomationRequest(tools, 'list_levels', {}) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'get_summary': {
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'get_summary',
        levelPath: argsTyped.levelPath
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'delete':
    case 'delete_level': {
      const levelPaths = Array.isArray(argsTyped.levelPaths)
        ? argsTyped.levelPaths.filter((p): p is string => typeof p === 'string')
        : (argsTyped.levelPath ? [argsTyped.levelPath] : []);
      // Validate at least one path is provided for delete
      if (levelPaths.length === 0) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'levelPath or levelPaths is required for delete',
          action
        });
      }
      if (levelPaths.length === 1) {
        const res = await executeAutomationRequest(tools, 'manage_level', {
          action: 'delete_level',
          levelPath: levelPaths[0]
        }) as Record<string, unknown>;
        return cleanObject(res);
      }

      const results: Record<string, unknown>[] = [];
      for (const levelPath of levelPaths) {
        const result = await executeAutomationRequest(tools, 'manage_level', {
          action: 'delete_level',
          levelPath
        }) as Record<string, unknown>;
        results.push(cleanObject(result));
      }

      const failed = results.find(result => result.success === false || result.isError === true);
      return cleanObject({
        success: !failed,
        deletedCount: results.filter(result => result.success !== false && result.isError !== true).length,
        results,
        error: failed ? 'DELETE_FAILED' : undefined,
        message: failed ? 'One or more level deletes failed' : 'Levels deleted'
      });
    }
    case 'rename_level': {
      const sourcePath = argsTyped.levelPath || argsTyped.sourcePath;
      if (!sourcePath) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'levelPath or sourcePath is required for rename_level',
          action
        });
      }
      if (!argsTyped.newName) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'newName is required for rename_level',
          action
        });
      }
      // Calculate destination path from source path and new name
      const lastSlash = sourcePath.lastIndexOf('/');
      const parentDir = lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game';
      const destinationPath = `${parentDir}/${argsTyped.newName}`;
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'rename',
        levelPath: sourcePath,
        destinationPath,
        overwrite: argsTyped.overwrite === true
      });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'duplicate_level': {
      const sourcePath = argsTyped.sourcePath || argsTyped.levelPath;
      if (!sourcePath) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'sourcePath or levelPath is required for duplicate_level',
          action
        });
      }
      if (!argsTyped.destinationPath) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'destinationPath is required for duplicate_level',
          action
        });
      }
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'duplicate',
        sourcePath,
        destinationPath: argsTyped.destinationPath,
        overwrite: argsTyped.overwrite === true
      });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'get_current_level': {
      const res = await executeAutomationRequest(tools, 'manage_level', { action }) as Record<string, unknown>;
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'set_metadata': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const metadata = (argsTyped.metadata && typeof argsTyped.metadata === 'object') ? argsTyped.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: levelPath, metadata });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'validate_level': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');

      // Prefer an editor-side existence check when the automation bridge is available.
      const automationBridge = tools.automationBridge;
      if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function' || !automationBridge.isConnected()) {
        return cleanObject({
          success: false,
          error: 'BRIDGE_UNAVAILABLE',
          message: 'Automation bridge not available; cannot validate level asset',
          levelPath
        });
      }

      try {
        const resp = await automationBridge.sendAutomationRequest('execute_editor_function', {
          functionName: 'ASSET_EXISTS_SIMPLE',
          path: levelPath
        }) as AutomationResponse;

        // Check for error envelope first
        const respState = resp as { success?: boolean; isError?: boolean };
        if (respState.success === false || respState.isError === true) {
          return cleanObject(resp as Record<string, unknown>);
        }

        const result = (resp?.result ?? {}) as ResultPayload;
        const exists = Boolean(result.exists);

        return cleanObject({
          success: true,
          exists,
          levelPath: result.path ?? levelPath,
          classPath: result.class,
          error: exists ? undefined : 'NOT_FOUND',
          message: exists ? 'Level asset exists' : 'Level asset not found'
        });
      } catch (err) {
        return cleanObject({
          success: false,
          error: 'VALIDATION_FAILED',
          message: `Level validation failed: ${err instanceof Error ? err.message : String(err)}`,
          levelPath
        });
      }
    }
    default:
      return await executeAutomationRequest(tools, 'manage_level', args) as Record<string, unknown>;
  }
}
