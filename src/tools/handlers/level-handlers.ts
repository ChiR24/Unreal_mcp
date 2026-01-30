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
      const res = await tools.levelTools.createLevel({ levelName, savePath: argsTyped.savePath || (argsTyped.levelPath ? argsTyped.levelPath.split('/').slice(0, -1).join('/') : undefined) });
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
    case 'create_sublevel': {
      const sublevelName = requireNonEmptyString(argsTyped.sublevelName || argsTyped.levelName, 'sublevelName', 'Missing required parameter: sublevelName');
      const parentLevel = requireNonEmptyString(argsTyped.parentLevel || argsTyped.parentPath, 'parentLevel', 'Missing required parameter: parentLevel');
      const result = await executeAutomationRequest(
        tools,
        'manage_level',
        { ...args, subAction: 'create_sublevel', sublevelName, parentLevel }
      );
      return cleanObject(result) as HandlerResult;
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
    // Wave 2.31-2.40: Level Enhancement Actions (incl. PCG GPU)
    case 'create_pcg_hlsl_node': {
      // UE 5.7+ feature - Route to manage_pcg where C++ handler exists
      const graphPath = requireNonEmptyString(argsTyped.graphPath, 'graphPath');
      const hlslCode = requireNonEmptyString(argsTyped.hlslCode, 'hlslCode');
      const res = await executeAutomationRequest(tools, 'manage_pcg', {
        action: 'create_pcg_hlsl_node',
        subAction: 'create_pcg_hlsl_node',
        graphPath,
        hlslCode,
        nodeName: argsTyped.nodeName
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'enable_pcg_gpu_processing': {
      // UE 5.7+ feature - Route to manage_pcg where C++ handler exists
      const graphPath = requireNonEmptyString(argsTyped.graphPath, 'graphPath');
      const res = await executeAutomationRequest(tools, 'manage_pcg', {
        action: 'enable_pcg_gpu_processing',
        subAction: 'enable_pcg_gpu_processing',
        graphPath,
        enabled: argsTyped.enabled ?? true
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'configure_pcg_mode_brush': {
      // UE 5.7+ feature - Route to manage_pcg where C++ handler exists
      const res = await executeAutomationRequest(tools, 'manage_pcg', {
        action: 'configure_pcg_mode_brush',
        subAction: 'configure_pcg_mode_brush',
        brushSize: argsTyped.brushSize,
        brushStrength: argsTyped.brushStrength,
        brushFalloff: argsTyped.brushFalloff
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'export_pcg_hlsl_template': {
      // UE 5.7+ feature - Route to manage_pcg where C++ handler exists
      const outputPath = requireNonEmptyString(argsTyped.outputPath, 'outputPath');
      const res = await executeAutomationRequest(tools, 'manage_pcg', {
        action: 'export_pcg_hlsl_template',
        subAction: 'export_pcg_hlsl_template',
        outputPath,
        templateType: argsTyped.templateType ?? 'point_processor'
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'batch_execute_pcg_with_gpu': {
      // UE 5.7+ feature - Route to manage_pcg where C++ handler exists
      const graphPaths = argsTyped.graphPaths;
      if (!Array.isArray(graphPaths) || graphPaths.length === 0) {
        throw new Error('manage_pcg.batch_execute_pcg_with_gpu: graphPaths array is required');
      }
      const res = await executeAutomationRequest(tools, 'manage_pcg', {
        action: 'batch_execute_pcg_with_gpu',
        subAction: 'batch_execute_pcg_with_gpu',
        graphPaths,
        useGPU: argsTyped.useGPU ?? true
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'set_pcg_partition_grid_size': {
      const res = await executeAutomationRequest(tools, 'manage_pcg', {
        action: 'set_pcg_partition_grid_size',
        subAction: 'set_pcg_partition_grid_size',
        gridCellSize: argsTyped.gridCellSize, gridSize: argsTyped.gridCellSize
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'get_world_partition_cells': {
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'get_world_partition_cells',
        bounds: argsTyped.bounds,
        includeLoaded: argsTyped.includeLoaded ?? true,
        includeUnloaded: argsTyped.includeUnloaded ?? false
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'stream_level_async': {
      const levelPath = requireNonEmptyString(argsTyped.levelPath || argsTyped.subLevelPath, 'levelPath');
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'stream_level_async',
        levelPath,
        shouldBeLoaded: argsTyped.shouldBeLoaded ?? true,
        shouldBeVisible: argsTyped.shouldBeVisible ?? true,
        blockOnLoad: argsTyped.blockOnLoad ?? false
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'get_streaming_levels_status': {
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'get_streaming_levels_status'
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'configure_hlod_settings': {
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'configure_hlod_settings',
        hlodLayerName: argsTyped.hlodLayerName,
        cellSize: argsTyped.cellSize,
        loadingDistance: argsTyped.loadingDistance,
        spatiallyLoaded: argsTyped.spatiallyLoaded
      });
      return cleanObject(res) as HandlerResult;
    }
    case 'build_hlod_for_level': {
      const res = await executeAutomationRequest(tools, 'manage_level', {
        action: 'build_hlod_for_level',
        levelPath: argsTyped.levelPath,
        hlodLayerName: argsTyped.hlodLayerName,
        rebuildAll: argsTyped.rebuildAll ?? false
      });
      return cleanObject(res) as HandlerResult;
    }
    // PCG actions - Route to manage_pcg where C++ handlers exist
    case 'create_biome_rules':
    case 'blend_biomes':
    case 'export_pcg_to_static':
    case 'import_pcg_preset':
    case 'debug_pcg_execution': {
      const res = await executeAutomationRequest(tools, 'manage_pcg', {
        action,
        subAction: action,
        ...args
      });
      return cleanObject(res) as HandlerResult;
    }
    default:
      return await executeAutomationRequest(tools, 'manage_level', args) as HandlerResult;
  }
}
