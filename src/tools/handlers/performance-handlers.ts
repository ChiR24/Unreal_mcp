import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';

// Valid profiling types
const VALID_PROFILING_TYPES = ['cpu', 'gpu', 'memory', 'renderthread', 'all', 'fps', 'gamethread'];

// Valid scalability levels (0-4)
const MIN_SCALABILITY_LEVEL = 0;
const MAX_SCALABILITY_LEVEL = 4;

export async function handlePerformanceTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'start_profiling': {
      const profilingType = args.type ? String(args.type).toLowerCase() : 'all';
      if (!VALID_PROFILING_TYPES.includes(profilingType)) {
        return {
          success: false,
          error: 'INVALID_PROFILING_TYPE',
          message: `Invalid profiling type: '${args.type}'. Must be one of: ${VALID_PROFILING_TYPES.join(', ')}`,
          action: 'start_profiling'
        };
      }
      // Use normalized profilingType to ensure consistency with validation
      return cleanObject(await tools.performanceTools.startProfiling({
        type: profilingType,
        duration: args.duration
      }));
    }
    case 'stop_profiling': {
      return cleanObject(await tools.performanceTools.stopProfiling());
    }
    case 'run_benchmark': {
      return cleanObject(await tools.performanceTools.runBenchmark({
        duration: args.duration,
        outputPath: args.outputPath
      }));
    }
    case 'show_fps': {
      return cleanObject(await tools.performanceTools.showFPS({
        enabled: args.enabled !== false,
        verbose: args.verbose
      }));
    }
    case 'show_stats': {
      return cleanObject(await tools.performanceTools.showStats({
        category: args.category || args.type || 'Unit',
        enabled: args.enabled !== false
      }));
    }
    case 'set_scalability': {
      const category = args.category || 'ViewDistance';
      let level = typeof args.level === 'number' ? args.level : 3;
      // Clamp level to valid range 0-4
      level = Math.max(MIN_SCALABILITY_LEVEL, Math.min(MAX_SCALABILITY_LEVEL, level));
      return cleanObject(await tools.performanceTools.setScalability({
        category,
        level
      }));
    }
    case 'set_resolution_scale': {
      return cleanObject(await tools.performanceTools.setResolutionScale({
        scale: args.scale
      }));
    }
    case 'set_vsync': {
      return cleanObject(await tools.performanceTools.setVSync({
        enabled: args.enabled !== false
      }));
    }
    case 'set_frame_rate_limit': {
      return cleanObject(await tools.performanceTools.setFrameRateLimit({
        maxFPS: args.maxFPS
      }));
    }
    case 'enable_gpu_timing': {
      return cleanObject(await tools.performanceTools.enableGPUTiming({
        enabled: args.enabled !== false
      }));
    }
    case 'generate_memory_report': {
      return cleanObject(await tools.performanceTools.generateMemoryReport({
        detailed: args.detailed,
        outputPath: args.outputPath
      }));
    }
    case 'configure_texture_streaming': {
      return cleanObject(await tools.performanceTools.configureTextureStreaming({
        enabled: args.enabled !== false,
        poolSize: args.poolSize,
        boostPlayerLocation: args.boostPlayerLocation
      }));
    }
    case 'configure_lod': {
      return cleanObject(await tools.performanceTools.configureLOD({
        forceLOD: args.forceLOD,
        lodBias: args.lodBias,
        distanceScale: args.distanceScale
      }));
    }
    case 'apply_baseline_settings': {
      return cleanObject(await tools.performanceTools.applyBaselinePerformanceSettings({
        distanceScale: args.distanceScale,
        skeletalBias: args.skeletalBias,
        vsync: args.vsync,
        maxFPS: args.maxFPS,
        hzb: args.hzb
      }));
    }
    case 'optimize_draw_calls':
    case 'merge_actors': {
      // If action is merge_actors, force mergeActors param to true
      const mergeParams = action === 'merge_actors' ? { ...args, mergeActors: true } : args;
      return cleanObject(await tools.performanceTools.optimizeDrawCalls({
        enableInstancing: mergeParams.enableInstancing,
        enableBatching: mergeParams.enableBatching,
        mergeActors: mergeParams.mergeActors,
        actors: mergeParams.actors
      }));
    }
    case 'configure_occlusion_culling': {
      return cleanObject(await tools.performanceTools.configureOcclusionCulling({
        enabled: args.enabled !== false,
        method: args.method,
        freezeRendering: args.freezeRendering
      }));
    }
    case 'optimize_shaders': {
      return cleanObject(await tools.performanceTools.optimizeShaders({
        compileOnDemand: args.compileOnDemand,
        cacheShaders: args.cacheShaders,
        reducePermutations: args.reducePermutations
      }));
    }
    case 'configure_nanite': {
      return cleanObject(await tools.performanceTools.configureNanite({
        enabled: args.enabled !== false,
        maxPixelsPerEdge: args.maxPixelsPerEdge,
        streamingPoolSize: args.streamingPoolSize
      }));
    }
    case 'configure_world_partition': {
      return cleanObject(await tools.performanceTools.configureWorldPartition({
        enabled: args.enabled !== false,
        streamingDistance: args.streamingDistance,
        cellSize: args.cellSize
      }));
    }
    default:
      throw new Error(`Unknown performance action: ${action}`);
  }
}
