import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, LevelArgs } from '../../types/handler-types.js';
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

export async function handleLevelTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const argsTyped = args as LevelArgs;
  
  switch (action) {
    case 'load':
    case 'load_level': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const res = await tools.levelTools.loadLevel({ levelPath, streaming: !!argsTyped.streaming });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'save': {
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
      const res = await tools.levelTools.createLevel({ levelName, savePath: argsTyped.savePath || argsTyped.levelPath });
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
    case 'create_light': {
      // Delegate directly to the plugin's manage_level.create_light handler.
      const res = await executeAutomationRequest(tools, 'manage_level', args);
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
    case 'delete': {
      const levelPaths = Array.isArray(argsTyped.levelPaths) 
        ? argsTyped.levelPaths.filter((p): p is string => typeof p === 'string') 
        : (argsTyped.levelPath ? [argsTyped.levelPath] : []);
      const res = await tools.levelTools.deleteLevels({ levelPaths });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'set_metadata': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const metadata = (argsTyped.metadata && typeof argsTyped.metadata === 'object') ? argsTyped.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: levelPath, metadata });
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'load_cells': {
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
