import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, LevelArgs } from '../../types/handler-types.js';
import { executeAutomationRequest, requireNonEmptyString, validateSecurityPatterns } from './common-handlers.js';

/** Response from automation request */
interface AutomationResponse {
  success?: boolean;
  result?: {
    exists?: boolean;
    path?: string;
    class?: string;
    [key: string]: unknown;
  };
  [key: string]: unknown;
}

/**
 * Normalize level arguments to support both camelCase and snake_case
 */
function normalizeLevelArgs(args: LevelArgs): LevelArgs {
  const raw = args as Record<string, unknown>;
  return {
    ...args,
    // Map snake_case to camelCase
    levelPath: (raw.level_path as string | undefined) ?? args.levelPath,
    levelName: (raw.level_name as string | undefined) ?? args.levelName,
    savePath: (raw.save_path as string | undefined) ?? args.savePath,
    destinationPath: (raw.destination_path as string | undefined) ?? args.destinationPath,
    subLevelPath: (raw.sub_level_path as string | undefined) ?? args.subLevelPath,
    parentLevel: (raw.parent_level as string | undefined) ?? args.parentLevel,
    parentPath: (raw.parent_path as string | undefined) ?? args.parentPath,
    streamingMethod: (raw.streaming_method as 'Blueprint' | 'AlwaysLoaded' | undefined) ?? args.streamingMethod,
    sourcePath: (raw.source_path as string | undefined) ?? args.sourcePath,
    newName: (raw.new_name as string | undefined) ?? (args as Record<string, unknown>).newName as string | undefined,
    levelPaths: (raw.level_paths as string[] | undefined) ?? args.levelPaths,
    packagePath: (raw.package_path as string | undefined) ?? args.packagePath,
    exportPath: (raw.export_path as string | undefined) ?? args.exportPath,
  };
}

export async function handleLevelTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  // Normalize args to support both camelCase and snake_case
  const argsTyped = normalizeLevelArgs(args as LevelArgs);
  
  // Security validation: check for path traversal attempts
  const securityError = validateSecurityPatterns(args as Record<string, unknown>);
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
      const res = await tools.levelTools.loadLevel({ levelPath, streaming: !!argsTyped.streaming });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'save':
    case 'save_level': {
      const targetPath = argsTyped.levelPath || argsTyped.savePath;
      if (targetPath) {
        const res = await tools.levelTools.saveLevelAs({ targetPath });
        return cleanObject(res) as Record<string, unknown>;
      }
      const res = await tools.levelTools.saveLevel({ levelName: argsTyped.levelName });
      return cleanObject(res) as Record<string, unknown>;
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
      const res = await tools.levelTools.saveLevelAs({ targetPath });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'create_level': {
      const levelPathStr = typeof argsTyped.levelPath === 'string' ? argsTyped.levelPath : '';
      const levelName = requireNonEmptyString(argsTyped.levelName || levelPathStr.split('/').pop() || '', 'levelName', 'Missing required parameter: levelName');
      // CRITICAL: Pass useWorldPartition to C++ - default to false to avoid 20+ second freeze during World Partition uninitialize
      // World Partition levels cause permanent hang when unloaded via GEditor->NewMap()
      const res = await tools.levelTools.createLevel({
        levelName,
        savePath: argsTyped.savePath || argsTyped.levelPath,
        useWorldPartition: argsTyped.useWorldPartition ?? false
      });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'add_sublevel': {
      const subLevelPath = requireNonEmptyString(argsTyped.subLevelPath || argsTyped.levelPath, 'subLevelPath', 'Missing required parameter: subLevelPath');
      const res = await tools.levelTools.addSubLevel({
        subLevelPath,
        parentLevel: argsTyped.parentLevel || argsTyped.parentPath,
        streamingMethod: argsTyped.streamingMethod
      });
      return cleanObject(res) as Record<string, unknown>;
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

      const res = await tools.levelTools.streamLevel({
        levelPath,
        levelName,
        shouldBeLoaded: argsTyped.shouldBeLoaded,
        shouldBeVisible: argsTyped.shouldBeVisible
      });
      return cleanObject(res) as Record<string, unknown>;
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
      const res = await tools.levelTools.streamLevel({
        levelPath,
        levelName,
        shouldBeLoaded: false,
        shouldBeVisible: false
      });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'create_light': {
      // Validate required parameters for create_light
      const lightType = argsTyped.type || argsTyped.lightType;
      if (!lightType || typeof lightType !== 'string' || lightType.trim() === '') {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: type (or lightType). Valid types: Point, Directional, Spot, Sky, Rect',
          action
        });
      }
      // Validate light type is one of the supported types
      const validTypes = ['Point', 'Directional', 'Spot', 'Sky', 'Rect', 'PointLight', 'DirectionalLight', 'SpotLight', 'SkyLight', 'RectLight'];
      if (!validTypes.some(t => t.toLowerCase() === lightType.toLowerCase())) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: `Invalid light type: ${lightType}. Valid types: Point, Directional, Spot, Sky, Rect`,
          action
        });
      }
      // CRITICAL FIX: Normalize 'type' to 'lightType' for C++ handler compatibility
      // The C++ handler checks for 'lightType' field, not 'type'
      const normalizedArgs = {
        ...args,
        lightType: lightType,
        // Remove 'type' to avoid confusion
        type: undefined
      };
      // Remove undefined fields
      const cleanNormalizedArgs = Object.fromEntries(
        Object.entries(normalizedArgs).filter(([, v]) => v !== undefined)
      );
      const res = await executeAutomationRequest(tools, 'manage_level', cleanNormalizedArgs);
      return cleanObject(res) as Record<string, unknown>;
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

      const lightType = argsTyped.lightType || 'Point';
      const classPath = lightClassMap[lightType] || '/Script/Engine.PointLight';

      try {
        const res = await tools.actorTools.spawn({
          classPath,
          actorName: argsTyped.name,
          location: argsTyped.location,
          rotation: argsTyped.rotation
        });
        return { ...cleanObject(res) as Record<string, unknown>, action: 'spawn_light' };
      } catch (_e) {
        return await executeAutomationRequest(tools, 'manage_level', args) as Record<string, unknown>;
      }
    }
    case 'build_lighting': {
      // Pass user-provided values, default to false if not specified
      const argsRecord = args as Record<string, unknown>;
      return cleanObject(await tools.lightingTools.buildLighting({
        quality: (argsTyped.quality as string) || 'Preview',
        buildOnlySelected: typeof argsRecord.buildOnlySelected === 'boolean' ? argsRecord.buildOnlySelected : false,
        buildReflectionCaptures: typeof argsRecord.buildReflectionCaptures === 'boolean' ? argsRecord.buildReflectionCaptures : false
      })) as Record<string, unknown>;
    }
    case 'export_level': {
      const res = await tools.levelTools.exportLevel({
        levelPath: argsTyped.levelPath,
        exportPath: argsTyped.exportPath ?? argsTyped.destinationPath ?? '',
        timeoutMs: typeof argsTyped.timeoutMs === 'number' ? argsTyped.timeoutMs : undefined
      });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'import_level': {
      const res = await tools.levelTools.importLevel({
        packagePath: argsTyped.packagePath ?? argsTyped.sourcePath ?? '',
        destinationPath: argsTyped.destinationPath,
        timeoutMs: typeof argsTyped.timeoutMs === 'number' ? argsTyped.timeoutMs : undefined
      });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'list_levels': {
      const res = await tools.levelTools.listLevels();
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'get_summary': {
      const res = await tools.levelTools.getLevelSummary(argsTyped.levelPath);
      return cleanObject(res) as Record<string, unknown>;
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
      const res = await tools.levelTools.deleteLevels({ levelPaths });
      return cleanObject(res) as Record<string, unknown>;
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
        destinationPath
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
        destinationPath: argsTyped.destinationPath
      });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'get_current_level': {
      const res = await tools.levelTools.listLevels();
      const resTyped = res as { currentMap?: string; currentMapPath?: string };
      return cleanObject({
        success: true,
        levelName: resTyped.currentMap,
        levelPath: resTyped.currentMapPath,
        ...(res as Record<string, unknown>)
      }) as Record<string, unknown>;
    }
    case 'set_metadata': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const metadata = (argsTyped.metadata && typeof argsTyped.metadata === 'object') ? argsTyped.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: levelPath, metadata });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'load_cells': {
      // Validate required parameters for load_cells
      const hasCells = Array.isArray(argsTyped.cells) && argsTyped.cells.length > 0;
      const hasOrigin = Array.isArray(argsTyped.origin) && argsTyped.origin.length >= 2;
      const hasExtent = Array.isArray(argsTyped.extent) && argsTyped.extent.length >= 2;
      const hasMin = Array.isArray(argsTyped.min) && argsTyped.min.length >= 2;
      const hasMax = Array.isArray(argsTyped.max) && argsTyped.max.length >= 2;
      
      // Must have either cells, or origin+extent, or min+max
      if (!hasCells && !(hasOrigin && hasExtent) && !(hasMin && hasMax)) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameters: must provide either cells array, or origin+extent, or min+max',
          action
        });
      }

      // Calculate origin/extent if min/max provided for C++ handler compatibility
      let origin = argsTyped.origin;
      let extent = argsTyped.extent;

      if (!origin && argsTyped.min && argsTyped.max) {
        const min = argsTyped.min;
        const max = argsTyped.max;
        origin = [(min[0] + max[0]) / 2, (min[1] + max[1]) / 2, (min[2] + max[2]) / 2];
        extent = [(max[0] - min[0]) / 2, (max[1] - min[1]) / 2, (max[2] - min[2]) / 2];
      }

      const payload = {
        subAction: 'load_cells',
        origin: origin,
        extent: extent,
        ...args // Allow other args to override if explicit
      };

      const res = await executeAutomationRequest(tools, 'manage_world_partition', payload);
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'set_datalayer': {
      const dataLayerName = argsTyped.dataLayerName || argsTyped.dataLayerLabel;
      if (!dataLayerName || typeof dataLayerName !== 'string' || dataLayerName.trim().length === 0) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: dataLayerLabel (or dataLayerName)',
          action
        });
      }
      if (!argsTyped.dataLayerState || typeof argsTyped.dataLayerState !== 'string') {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: dataLayerState',
          action,
          dataLayerName
        });
      }

      const res = await executeAutomationRequest(tools, 'manage_world_partition', {
        subAction: 'set_datalayer',
        actorPath: argsTyped.actorPath,
        dataLayerName, // Map label to name
        dataLayerState: argsTyped.dataLayerState, // Pass validated dataLayerState to C++
        ...args
      });
      return cleanObject(res) as Record<string, unknown>;
    }
case 'cleanup_invalid_datalayers': {
      // Route to manage_world_partition
      const res = await executeAutomationRequest(tools, 'manage_world_partition', {
        subAction: 'cleanup_invalid_datalayers'
      }, 'World Partition support not available');
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'create_datalayer': {
      // Route to manage_world_partition
      const dataLayerName = argsTyped.dataLayerName || argsTyped.dataLayerLabel;
      if (!dataLayerName || typeof dataLayerName !== 'string' || dataLayerName.trim().length === 0) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: dataLayerName',
          action
        });
      }
      const res = await executeAutomationRequest(tools, 'manage_world_partition', {
        subAction: 'create_datalayer',
        dataLayerName
      }, 'World Partition support not available');
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
        const result = resp?.result ?? resp ?? {};
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
