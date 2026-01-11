import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, LevelArgs, HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

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

export async function handleLevelTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const argsTyped = args as LevelArgs;
  
  switch (action) {
    case 'load':
    case 'load_level': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const res = await tools.levelTools.loadLevel({ levelPath, streaming: !!argsTyped.streaming });
      return cleanObject(res) as HandlerResult;
    }
    case 'save': {
      const targetPath = argsTyped.levelPath || argsTyped.savePath;
      if (targetPath) {
        const res = await tools.levelTools.saveLevelAs({ targetPath });
        return cleanObject(res) as HandlerResult;
      }
      const res = await tools.levelTools.saveLevel({ levelName: argsTyped.levelName });
      return cleanObject(res) as HandlerResult;
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
      return cleanObject(res) as HandlerResult;
    }
    case 'create_level': {
      const levelPathStr = typeof argsTyped.levelPath === 'string' ? argsTyped.levelPath : '';
      const levelName = requireNonEmptyString(argsTyped.levelName || levelPathStr.split('/').pop() || '', 'levelName', 'Missing required parameter: levelName');
      const res = await tools.levelTools.createLevel({ levelName, savePath: argsTyped.savePath || argsTyped.levelPath });
      return cleanObject(res) as HandlerResult;
    }
    case 'add_sublevel': {
      const subLevelPath = requireNonEmptyString(argsTyped.subLevelPath || argsTyped.levelPath, 'subLevelPath', 'Missing required parameter: subLevelPath');
      const res = await tools.levelTools.addSubLevel({
        subLevelPath,
        parentLevel: argsTyped.parentLevel || argsTyped.parentPath,
        streamingMethod: argsTyped.streamingMethod
      });
      return cleanObject(res) as HandlerResult;
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
      return cleanObject(res) as HandlerResult;
    }
    case 'create_light': {
      // Delegate directly to the plugin's manage_level.create_light handler.
      const res = await executeAutomationRequest(tools, 'manage_level', args);
      return cleanObject(res) as HandlerResult;
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
        return { ...cleanObject(res) as HandlerResult, action: 'spawn_light' };
      } catch (_e) {
        return await executeAutomationRequest(tools, 'manage_level', args) as HandlerResult;
      }
    }
    case 'build_lighting': {
      return cleanObject(await tools.lightingTools.buildLighting({
        quality: (argsTyped.quality as string) || 'Preview',
        buildOnlySelected: false,
        buildReflectionCaptures: false
      })) as HandlerResult;
    }
    case 'export_level': {
      const res = await tools.levelTools.exportLevel({
        levelPath: argsTyped.levelPath,
        exportPath: argsTyped.exportPath ?? argsTyped.destinationPath ?? '',
        timeoutMs: typeof argsTyped.timeoutMs === 'number' ? argsTyped.timeoutMs : undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'import_level': {
      const res = await tools.levelTools.importLevel({
        packagePath: argsTyped.packagePath ?? argsTyped.sourcePath ?? '',
        destinationPath: argsTyped.destinationPath,
        timeoutMs: typeof argsTyped.timeoutMs === 'number' ? argsTyped.timeoutMs : undefined
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'list_levels': {
      const res = await tools.levelTools.listLevels();
      return cleanObject(res) as HandlerResult;
    }
    case 'get_summary': {
      const res = await tools.levelTools.getLevelSummary(argsTyped.levelPath);
      return cleanObject(res) as HandlerResult;
    }
    case 'delete': {
      const levelPaths = Array.isArray(argsTyped.levelPaths) 
        ? argsTyped.levelPaths.filter((p): p is string => typeof p === 'string') 
        : (argsTyped.levelPath ? [argsTyped.levelPath] : []);
      const res = await tools.levelTools.deleteLevels({ levelPaths });
      return cleanObject(res) as HandlerResult;
    }
    case 'set_metadata': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const metadata = (argsTyped.metadata && typeof argsTyped.metadata === 'object') ? argsTyped.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: levelPath, metadata });
      return cleanObject(res) as HandlerResult;
    }
    case 'load_cells': {
      // Calculate origin/extent if min/max provided for C++ handler compatibility
      let origin = argsTyped.origin;
      let extent = argsTyped.extent;

      if (!origin && argsTyped.min && argsTyped.max) {
        const min = argsTyped.min;
        const max = argsTyped.max;
        const minX = min[0] ?? 0, minY = min[1] ?? 0, minZ = min[2] ?? 0;
        const maxX = max[0] ?? 0, maxY = max[1] ?? 0, maxZ = max[2] ?? 0;
        origin = [(minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2];
        extent = [(maxX - minX) / 2, (maxY - minY) / 2, (maxZ - minZ) / 2];
      }

      const payload = {
        subAction: 'load_cells',
        origin: origin,
        extent: extent,
        ...args // Allow other args to override if explicit
      };

      const res = await executeAutomationRequest(tools, 'manage_world_partition', payload);
      return cleanObject(res) as HandlerResult;
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
        ...args
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'cleanup_invalid_datalayers': {
      // Route to manage_world_partition
      const res = await executeAutomationRequest(tools, 'manage_world_partition', {
        subAction: 'cleanup_invalid_datalayers'
      }, 'World Partition support not available');
      return cleanObject(res) as HandlerResult;
    }
    case 'validate_level': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');

      // Sentinel prefix for reliable bridge error detection
      const BRIDGE_ERROR_SENTINEL = '[BRIDGE_UNAVAILABLE]';
      const bridgeErrorMessage = `${BRIDGE_ERROR_SENTINEL} Automation bridge not available; cannot validate level asset`;

      try {
        // executeAutomationRequest throws if bridge unavailable (common-handlers.ts:70,74)
        const resp = await executeAutomationRequest(
          tools,
          'execute_editor_function',
          { functionName: 'ASSET_EXISTS_SIMPLE', path: levelPath },
          bridgeErrorMessage
        ) as AutomationResponse;
        
        const result = resp?.result ?? resp ?? {};
        const exists = Boolean(result.exists);

        return cleanObject({
          success: true,
          exists,
          levelPath: result.path ?? levelPath,
          classPath: result.class,
          error: exists ? undefined : 'NOT_FOUND',
          message: exists ? 'Level asset exists' : 'Level asset not found'
        }) as HandlerResult;
      } catch (err: unknown) {
        const errorMessage = err instanceof Error ? err.message : String(err);
        
        // Detect bridge errors via sentinel OR "not connected" substring (from executeAutomationRequest:74)
        const isBridgeError = errorMessage.startsWith(BRIDGE_ERROR_SENTINEL) || 
                              errorMessage.includes('not connected to Unreal Engine');
        
        return cleanObject({
          success: false,
          error: isBridgeError ? 'BRIDGE_UNAVAILABLE' : 'VALIDATION_FAILED',
          message: isBridgeError 
            ? 'Automation bridge not available; cannot validate level asset'  // Preserve original message
            : `Level validation failed: ${errorMessage}`,
          levelPath
        }) as HandlerResult;
      }
    }
    default:
      return await executeAutomationRequest(tools, 'manage_level', args) as HandlerResult;
  }
}
