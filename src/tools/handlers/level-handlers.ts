import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

export async function handleLevelTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'load':
    case 'load_level': {
      const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const res = await tools.levelTools.loadLevel({ levelPath, streaming: !!args.streaming });
      return cleanObject(res);
    }
    case 'save': {
      const targetPath = args.levelPath || args.savePath;
      if (targetPath) {
        const res = await tools.levelTools.saveLevelAs({ targetPath });
        return cleanObject(res);
      }
      const res = await tools.levelTools.saveLevel({ levelName: args.levelName });
      return cleanObject(res);
    }
    case 'save_as':
    case 'save_level_as': {
      // Accept savePath, destinationPath, or levelPath as the target
      const targetPath = args.savePath || args.destinationPath || args.levelPath;
      if (!targetPath) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'savePath is required for save_as action',
          action
        };
      }
      const res = await tools.levelTools.saveLevelAs({ targetPath });
      return cleanObject(res);
    }
    case 'create_level': {
      const levelName = requireNonEmptyString(args.levelName || (args.levelPath ? args.levelPath.split('/').pop() : ''), 'levelName', 'Missing required parameter: levelName');
      const res = await tools.levelTools.createLevel({ levelName, savePath: args.savePath || args.levelPath });
      return cleanObject(res);
    }
    case 'stream': {
      const levelPath = typeof args.levelPath === 'string' ? args.levelPath : undefined;
      const levelName = typeof args.levelName === 'string' ? args.levelName : undefined;
      if (!levelPath && !levelName) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: levelPath (or levelName)',
          action
        });
      }
      if (typeof args.shouldBeLoaded !== 'boolean') {
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
        shouldBeLoaded: args.shouldBeLoaded,
        shouldBeVisible: args.shouldBeVisible
      });
      return cleanObject(res);
    }
    case 'create_light': {
      // Delegate directly to the plugin's manage_level.create_light handler.
      const res = await executeAutomationRequest(tools, 'manage_level', args);
      return cleanObject(res);
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
        exportPath: args.exportPath || args.destinationPath,
        timeoutMs: typeof args.timeoutMs === 'number' ? args.timeoutMs : undefined
      });
      return cleanObject(res);
    }
    case 'import_level': {
      const res = await tools.levelTools.importLevel({
        packagePath: args.packagePath || args.sourcePath, // Allow sourcePath as fallback for backward compat
        destinationPath: args.destinationPath,
        timeoutMs: typeof args.timeoutMs === 'number' ? args.timeoutMs : undefined
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
      const levelPaths = Array.isArray(args.levelPaths) ? args.levelPaths : [args.levelPath];
      const res = await tools.levelTools.deleteLevels({ levelPaths });
      return cleanObject(res);
    }
    case 'set_metadata': {
      const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');
      const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: levelPath, metadata });
      return cleanObject(res);
    }
    case 'load_cells': {
      // Calculate origin/extent if min/max provided for C++ handler compatibility
      let origin = args.origin;
      let extent = args.extent;

      if (!origin && args.min && args.max) {
        const min = args.min;
        const max = args.max;
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
      return cleanObject(res);
    }
    case 'set_datalayer': {
      const dataLayerName = args.dataLayerName || args.dataLayerLabel;
      if (!dataLayerName || typeof dataLayerName !== 'string' || dataLayerName.trim().length === 0) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Missing required parameter: dataLayerLabel (or dataLayerName)',
          action
        });
      }
      if (!args.dataLayerState || typeof args.dataLayerState !== 'string') {
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
        actorPath: args.actorPath,
        dataLayerName, // Map label to name
        ...args
      });
      return cleanObject(res);
    }
    case 'cleanup_invalid_datalayers': {
      // Route to manage_world_partition
      const res = await executeAutomationRequest(tools, 'manage_world_partition', {
        subAction: 'cleanup_invalid_datalayers'
      }, 'World Partition support not available');
      return cleanObject(res);
    }
    case 'validate_level': {
      const levelPath = requireNonEmptyString(args.levelPath, 'levelPath', 'Missing required parameter: levelPath');

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
        const resp: any = await automationBridge.sendAutomationRequest('execute_editor_function', {
          functionName: 'ASSET_EXISTS_SIMPLE',
          path: levelPath
        });
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
      return await executeAutomationRequest(tools, 'manage_level', args);
  }
}
