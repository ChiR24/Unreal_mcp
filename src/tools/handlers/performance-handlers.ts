import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, PerformanceArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { TOOL_ACTIONS } from '../../utils/action-constants.js';


// Valid profiling types
const VALID_PROFILING_TYPES = ['cpu', 'gpu', 'memory', 'renderthread', 'all', 'fps', 'gamethread'];

// Valid scalability levels (0-4)
const MIN_SCALABILITY_LEVEL = 0;
const MAX_SCALABILITY_LEVEL = 4;

export async function handlePerformanceTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const argsTyped = args as PerformanceArgs;
  const argsRecord = args as Record<string, unknown>;

  switch (action) {
    case 'start_profiling': {
      const profilingType = argsTyped.type ? String(argsTyped.type).toLowerCase() : 'all';
      if (!VALID_PROFILING_TYPES.includes(profilingType)) {
      return {
        success: false,
        isError: true,
        error: 'INVALID_PROFILING_TYPE',
        message: `Invalid profiling type: '${argsTyped.type}'. Must be one of: ${VALID_PROFILING_TYPES.join(', ')}`,
        action: 'start_profiling'
      };
      }
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.START_PROFILING, {
        type: profilingType,
        duration: argsTyped.duration
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'stop_profiling': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.STOP_PROFILING, {}) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'run_benchmark': {
      const duration = typeof argsTyped.duration === 'number' ? argsTyped.duration : 60;
      const res = await executeAutomationRequest(tools, 'run_benchmark', {
        duration,
        type: typeof argsRecord.type === 'string' ? argsRecord.type : undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'show_fps': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.SHOW_FPS, {
        enabled: argsTyped.enabled !== false,
        verbose: argsTyped.verbose
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'show_stats': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.SHOW_STATS, {
        category: argsTyped.category || argsTyped.type || 'Unit',
        enabled: argsTyped.enabled !== false
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_scalability': {
      const category = argsTyped.category || 'ViewDistance';
      let level = typeof argsTyped.level === 'number' ? argsTyped.level : 3;
      // Clamp level to valid range 0-4
      level = Math.max(MIN_SCALABILITY_LEVEL, Math.min(MAX_SCALABILITY_LEVEL, level));
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_SCALABILITY, {
        category,
        level
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_resolution_scale': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_RESOLUTION_SCALE, {
        scale: argsTyped.scale
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_vsync': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_VSYNC, {
        enabled: argsTyped.enabled !== false
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'set_frame_rate_limit': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_FRAME_RATE_LIMIT, {
        maxFPS: argsTyped.maxFPS
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'enable_gpu_timing': {
      const enabled = argsTyped.enabled !== false;
      const res = await executeAutomationRequest(tools, 'manage_performance', {
        subAction: 'enable_gpu_timing',
        enabled
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'generate_memory_report': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.GENERATE_MEMORY_REPORT, {
        detailed: argsTyped.detailed,
        outputPath: argsTyped.outputPath
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'configure_texture_streaming': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.CONFIGURE_TEXTURE_STREAMING, {
        enabled: argsTyped.enabled !== false,
        poolSize: argsRecord.poolSize as number | undefined,
        boostPlayerLocation: argsRecord.boostPlayerLocation as boolean | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'configure_lod': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.CONFIGURE_LOD, {
        forceLOD: argsRecord.forceLOD as number | undefined,
        lodBias: argsRecord.lodBias as number | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'apply_baseline_settings': {
      // Delegate to C++ handler which uses IConsoleManager for reliable CVar setting
      const profile = typeof argsRecord.profile === 'string' ? argsRecord.profile : 'balanced';

      const res = await executeAutomationRequest(tools, 'apply_baseline_settings', {
        profile
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'optimize_draw_calls':
    case 'merge_actors': {
      // If action is merge_actors, force mergeActors param to true
      const mergeParams = action === 'merge_actors' ? { ...argsRecord, mergeActors: true } : argsRecord;

      if (mergeParams.mergeActors) {
        // Use automation bridge for actor merging
        const actors = Array.isArray(mergeParams.actors)
          ? mergeParams.actors.filter((name): name is string => typeof name === 'string' && name.length > 0)
          : undefined;

        if (!actors || actors.length < 2) {
        return {
          success: false,
          isError: true,
          error: 'Merge actors requires an "actors" array with at least 2 valid actor names.'
        };
        }

        const res = await executeAutomationRequest(tools, TOOL_ACTIONS.MERGE_ACTORS, {
          enableInstancing: mergeParams.enableInstancing as boolean | undefined,
          mergeActors: true,
          actors,
          replaceSourceActors: mergeParams.replaceSourceActors as boolean | undefined,
          packageName: typeof mergeParams.packageName === 'string' ? mergeParams.packageName : undefined,
          outputPath: typeof mergeParams.outputPath === 'string' ? mergeParams.outputPath : undefined
        }) as Record<string, unknown>;
        return cleanObject(res);
      }

      const res = await executeAutomationRequest(tools, 'optimize_draw_calls', {
        enabled: typeof mergeParams.enabled === 'boolean'
          ? mergeParams.enabled
          : mergeParams.enableBatching as boolean | undefined,
        instancing: mergeParams.enableInstancing as boolean | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'configure_occlusion_culling': {
      const res = await executeAutomationRequest(tools, 'configure_occlusion_culling', {
        enabled: argsTyped.enabled !== false,
        slop: argsRecord.slop as number | undefined,
        minScreenRadius: argsRecord.minScreenRadius as number | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'optimize_shaders': {
      const res = await executeAutomationRequest(tools, 'optimize_shaders', {
        mode: typeof argsRecord.mode === 'string' ? argsRecord.mode : undefined,
        forceRecompile: argsRecord.forceRecompile as boolean | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'configure_nanite': {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.CONFIGURE_NANITE, {
        enabled: argsTyped.enabled !== false,
        maxPixelsPerEdge: argsRecord.maxPixelsPerEdge as number | undefined,
        streamingPoolSize: argsRecord.streamingPoolSize as number | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'configure_world_partition': {
      // Delegate to C++ handler which uses IConsoleManager for reliable CVar setting
      const res = await executeAutomationRequest(tools, 'configure_world_partition', {
        enabled: argsTyped.enabled !== false,
        cellSize: argsRecord.cellSize as number | undefined,
        loadingRange: typeof argsRecord.streamingDistance === 'number'
          ? argsRecord.streamingDistance
          : argsRecord.loadingRange as number | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    default:
      return ResponseFactory.error(`Unknown performance action: ${action}`);
  }
}
