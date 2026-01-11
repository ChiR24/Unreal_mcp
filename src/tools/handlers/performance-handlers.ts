import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, PerformanceArgs, HandlerResult } from '../../types/handler-types.js';

// Valid profiling types
const VALID_PROFILING_TYPES = ['cpu', 'gpu', 'memory', 'renderthread', 'all', 'fps', 'gamethread'];

// Valid scalability levels (0-4)
const MIN_SCALABILITY_LEVEL = 0;
const MAX_SCALABILITY_LEVEL = 4;

export async function handlePerformanceTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const argsTyped = args as PerformanceArgs;
  const argsRecord = args as HandlerResult;
  
  switch (action) {
    case 'start_profiling': {
      const profilingType = argsTyped.type ? String(argsTyped.type).toLowerCase() : 'all';
      if (!VALID_PROFILING_TYPES.includes(profilingType)) {
        return {
          success: false,
          error: 'INVALID_PROFILING_TYPE',
          message: `Invalid profiling type: '${argsTyped.type}'. Must be one of: ${VALID_PROFILING_TYPES.join(', ')}`,
          action: 'start_profiling'
        };
      }
      // Use normalized profilingType to ensure consistency with validation
      return cleanObject(await tools.performanceTools.startProfiling({
        type: profilingType,
        duration: argsTyped.duration
      })) as HandlerResult;
    }
    case 'stop_profiling': {
      return cleanObject(await tools.performanceTools.stopProfiling()) as HandlerResult;
    }
    case 'run_benchmark': {
      return cleanObject(await tools.performanceTools.runBenchmark({
        duration: argsTyped.duration,
        outputPath: argsTyped.outputPath
      })) as HandlerResult;
    }
    case 'show_fps': {
      return cleanObject(await tools.performanceTools.showFPS({
        enabled: argsTyped.enabled !== false,
        verbose: argsTyped.verbose
      })) as HandlerResult;
    }
    case 'show_stats': {
      return cleanObject(await tools.performanceTools.showStats({
        category: argsTyped.category || argsTyped.type || 'Unit',
        enabled: argsTyped.enabled !== false
      })) as HandlerResult;
    }
    case 'set_scalability': {
      const category = argsTyped.category || 'ViewDistance';
      let level = typeof argsTyped.level === 'number' ? argsTyped.level : 3;
      // Clamp level to valid range 0-4
      level = Math.max(MIN_SCALABILITY_LEVEL, Math.min(MAX_SCALABILITY_LEVEL, level));
      return cleanObject(await tools.performanceTools.setScalability({
        category,
        level
      })) as HandlerResult;
    }
    case 'set_resolution_scale': {
      return cleanObject(await tools.performanceTools.setResolutionScale({
        scale: argsTyped.scale
      })) as HandlerResult;
    }
    case 'set_vsync': {
      return cleanObject(await tools.performanceTools.setVSync({
        enabled: argsTyped.enabled !== false
      })) as HandlerResult;
    }
    case 'set_frame_rate_limit': {
      return cleanObject(await tools.performanceTools.setFrameRateLimit({
        maxFPS: argsTyped.maxFPS
      })) as HandlerResult;
    }
    case 'enable_gpu_timing': {
      return cleanObject(await tools.performanceTools.enableGPUTiming({
        enabled: argsTyped.enabled !== false
      })) as HandlerResult;
    }
    case 'generate_memory_report': {
      return cleanObject(await tools.performanceTools.generateMemoryReport({
        detailed: argsTyped.detailed,
        outputPath: argsTyped.outputPath
      })) as HandlerResult;
    }
    case 'configure_texture_streaming': {
      return cleanObject(await tools.performanceTools.configureTextureStreaming({
        enabled: argsTyped.enabled !== false,
        poolSize: argsRecord.poolSize as number | undefined,
        boostPlayerLocation: argsRecord.boostPlayerLocation as boolean | undefined
      })) as HandlerResult;
    }
    case 'configure_lod': {
      return cleanObject(await tools.performanceTools.configureLOD({
        forceLOD: argsRecord.forceLOD as number | undefined,
        lodBias: argsRecord.lodBias as number | undefined,
        distanceScale: argsRecord.distanceScale as number | undefined
      })) as HandlerResult;
    }
    case 'apply_baseline_settings': {
      return cleanObject(await tools.performanceTools.applyBaselinePerformanceSettings({
        distanceScale: argsRecord.distanceScale as number | undefined,
        skeletalBias: argsRecord.skeletalBias as number | undefined,
        vsync: argsRecord.vsync as boolean | undefined,
        maxFPS: argsTyped.maxFPS,
        hzb: argsRecord.hzb as boolean | undefined
      })) as HandlerResult;
    }
    case 'optimize_draw_calls':
    case 'merge_actors': {
      // If action is merge_actors, force mergeActors param to true
      const mergeParams = action === 'merge_actors' ? { ...argsRecord, mergeActors: true } : argsRecord;
      return cleanObject(await tools.performanceTools.optimizeDrawCalls({
        enableInstancing: mergeParams.enableInstancing as boolean | undefined,
        enableBatching: mergeParams.enableBatching as boolean | undefined,
        mergeActors: mergeParams.mergeActors as boolean | undefined,
        actors: mergeParams.actors as string[] | undefined
      })) as HandlerResult;
    }
    case 'configure_occlusion_culling': {
      return cleanObject(await tools.performanceTools.configureOcclusionCulling({
        enabled: argsTyped.enabled !== false,
        method: argsRecord.method as string | undefined,
        freezeRendering: argsRecord.freezeRendering as boolean | undefined
      })) as HandlerResult;
    }
    case 'optimize_shaders': {
      return cleanObject(await tools.performanceTools.optimizeShaders({
        compileOnDemand: argsRecord.compileOnDemand as boolean | undefined,
        cacheShaders: argsRecord.cacheShaders as boolean | undefined,
        reducePermutations: argsRecord.reducePermutations as boolean | undefined
      })) as HandlerResult;
    }
    case 'configure_nanite': {
      return cleanObject(await tools.performanceTools.configureNanite({
        enabled: argsTyped.enabled !== false,
        maxPixelsPerEdge: argsRecord.maxPixelsPerEdge as number | undefined,
        streamingPoolSize: argsRecord.streamingPoolSize as number | undefined
      })) as HandlerResult;
    }
    case 'configure_world_partition': {
      return cleanObject(await tools.performanceTools.configureWorldPartition({
        enabled: argsTyped.enabled !== false,
        streamingDistance: argsRecord.streamingDistance as number | undefined,
        cellSize: argsRecord.cellSize as number | undefined
      })) as HandlerResult;
    }
    default:
      throw new Error(`Unknown performance action: ${action}`);
  }
}
