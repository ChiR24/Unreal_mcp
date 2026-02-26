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
      // Run benchmark using console commands with timing
      const duration = typeof argsTyped.duration === 'number' ? argsTyped.duration : 60;
      
      // Start recording
      await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: 'stat startfile' });
      await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: 'profilegpu' });
      
      // Wait for duration
      await new Promise(resolve => setTimeout(resolve, duration * 1000));
      
      // Stop recording
      await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: 'stat stopfile' });
      await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: 'stat none' });
      
      return { success: true, message: `Benchmark completed for ${duration} seconds` };
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
      // Console command only - no automation bridge action
      const enabled = argsTyped.enabled !== false;
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, {
        command: `r.GPUStatsEnabled ${enabled ? 1 : 0}`
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
        lodBias: argsRecord.lodBias as number | undefined,
        distanceScale: argsRecord.distanceScale as number | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    case 'apply_baseline_settings': {
      // Console commands only - composite action
      const distanceScale = typeof argsRecord.distanceScale === 'number' ? argsRecord.distanceScale : 1.0;
      const skeletalBias = typeof argsRecord.skeletalBias === 'number' ? argsRecord.skeletalBias : 0;
      const vsync = typeof argsRecord.vsync === 'boolean' ? argsRecord.vsync : false;
      const maxFPS = typeof argsTyped.maxFPS === 'number' ? argsTyped.maxFPS : 60;
      const hzb = typeof argsRecord.hzb === 'boolean' ? argsRecord.hzb : true;
      
      const commands = [
        `r.StaticMeshLODDistanceScale ${distanceScale}`,
        `r.SkeletalMeshLODDistanceScale ${distanceScale}`,
        `r.SkeletalMeshLODBias ${skeletalBias}`,
        `r.HZBOcclusion ${hzb ? 1 : 0}`,
        `r.VSync ${vsync ? 1 : 0}`,
        `t.MaxFPS ${maxFPS}`,
      ];
      
      for (const cmd of commands) {
        await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
      }
      
      return { 
        success: true, 
        message: 'Baseline performance settings applied',
        params: { distanceScale, skeletalBias, vsync, maxFPS, hzb }
      };
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
          actors: actors
        }) as Record<string, unknown>;
        return cleanObject(res);
      }
      
      // Non-merge draw call optimization - console commands only
      const commands: string[] = [];
      if (typeof mergeParams.enableInstancing === 'boolean') {
        commands.push(`r.MeshDrawCommands.DynamicInstancing ${mergeParams.enableInstancing ? 1 : 0}`);
      }
      
      for (const cmd of commands) {
        await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
      }
      
      return { success: true, message: 'Draw call optimization configured' };
    }
    case 'configure_occlusion_culling': {
      // Console commands only
      const enabled = argsTyped.enabled !== false;
      const commands: string[] = [`r.HZBOcclusion ${enabled ? 1 : 0}`];
      
      if (typeof argsRecord.freezeRendering === 'boolean') {
        commands.push(`FreezeRendering ${argsRecord.freezeRendering ? 1 : 0}`);
      }
      
      for (const cmd of commands) {
        await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
      }
      
      return { success: true, message: 'Occlusion culling configured' };
    }
    case 'optimize_shaders': {
      // Console commands only
      const commands: string[] = [];
      
      if (typeof argsRecord.compileOnDemand === 'boolean') {
        commands.push(`r.ShaderDevelopmentMode ${argsRecord.compileOnDemand ? 1 : 0}`);
      }
      if (typeof argsRecord.cacheShaders === 'boolean') {
        commands.push(`r.ShaderPipelineCache.Enabled ${argsRecord.cacheShaders ? 1 : 0}`);
      }
      if (argsRecord.reducePermutations === true) {
        commands.push('RecompileShaders changed');
      }
      
      for (const cmd of commands) {
        await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
      }
      
      return { success: true, message: 'Shader optimization configured' };
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
      // Console commands only
      const enabled = argsTyped.enabled !== false;
      const commands: string[] = [`wp.Runtime.EnableStreaming ${enabled ? 1 : 0}`];
      
      if (typeof argsRecord.streamingDistance === 'number') {
        commands.push(`wp.Runtime.StreamingDistance ${argsRecord.streamingDistance}`);
      }
      if (typeof argsRecord.cellSize === 'number') {
        commands.push(`wp.Runtime.CellSize ${argsRecord.cellSize}`);
      }
      
      for (const cmd of commands) {
        await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
      }
      
      return { success: true, message: 'World Partition configured' };
    }
    default:
      return ResponseFactory.error(`Unknown performance action: ${action}`);
  }
}
