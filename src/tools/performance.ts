// Performance tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';

export class PerformanceTools {
  private log = new Logger('PerformanceTools');
  private automationBridge?: AutomationBridge;

  constructor(private bridge: UnrealBridge, automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  // Start profiling
  async startProfiling(params: {
    type: string;
    duration?: number;
  }) {
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('start_profiling', {
          type: params.type,
          duration: params.duration
        });
        if (response.success) return { success: true, message: `${params.type} profiling started (bridge)` };
      } catch (_e) {
        // Fallback
      }
    }

    const commands: string[] = [];

    switch (params.type) {
      case 'CPU':
        commands.push('stat startfile');
        break;
      case 'GPU':
        commands.push('profilegpu');
        break;
      case 'Memory':
        commands.push('stat memory');
        break;
      case 'RenderThread':
        commands.push('stat renderthread');
        break;
      case 'GameThread':
        commands.push('stat game');
        break;
      case 'All':
        commands.push('stat startfile');
        commands.push('profilegpu');
        commands.push('stat memory');
        break;
    }

    if (params.duration) {
      commands.push(`stat stopfile ${params.duration}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: `${params.type} profiling started` };
  }

  // Stop profiling
  async stopProfiling() {
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('stop_profiling', {});
        if (response.success) return { success: true, message: 'Profiling stopped (bridge)' };
      } catch (_e) {
        // Fallback
      }
    }

    const commands = [
      'stat stopfile',
      'stat none'
    ];

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Profiling stopped' };
  }

  // Show FPS
  async showFPS(params: {
    enabled: boolean;
    verbose?: boolean;
  }) {
    const startTime = Date.now();
    this.log.debug('Starting showFPS with params:', params);

    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('show_fps', {
          enabled: params.enabled,
          verbose: params.verbose
        });
        if (response.success) return {
          success: true,
          message: params.enabled ? 'FPS display enabled (bridge)' : 'FPS display disabled (bridge)',
          fpsVisible: params.enabled
        };
      } catch (_e) {
        // Fallback
      }
    }

    try {
      // Use stat fps as requested - shows FPS counter
      // For more detailed timing info, use 'stat unit' instead
      const command = params.enabled
        ? (params.verbose ? 'stat unit' : 'stat fps')
        : 'stat none';

      this.log.debug(`Executing command: ${command}`);
      await this.bridge.executeConsoleCommand(command);
      this.log.debug(`Command completed in ${Date.now() - startTime}ms`);
      return {
        success: true,
        message: params.enabled ? 'FPS display enabled' : 'FPS display disabled',
        fpsVisible: params.enabled,
        command: command
      };
    } catch (error: unknown) {
      return {
        success: false,
        error: `Failed to ${params.enabled ? 'enable' : 'disable'} FPS display: ${error}`,
        fpsVisible: false
      };
    }
  }

  // Show performance stats
  async showStats(params: {
    category: string;
    enabled: boolean;
  }) {
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('show_stats', {
          category: params.category,
          enabled: params.enabled
        });
        if (response.success) return { success: true, message: `Stat '${params.category}' configured (bridge)` };
      } catch (_e) {
        // Fallback
      }
    }

    const command = params.enabled
      ? `stat ${params.category.toLowerCase()}`
      : 'stat none';

    return this.bridge.executeConsoleCommand(command);
  }

  // Set scalability settings using console commands
  async setScalability(params: {
    category: string;
    level: number; // 0=Low, 1=Medium, 2=High, 3=Epic, 4=Cinematic
  }) {
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('set_scalability', {
          category: params.category,
          level: params.level
        });
        if (response.success) return { success: true, message: `${params.category} quality set to level ${params.level} (bridge)` };
      } catch (_e) {
        // Fallback
      }
    }

    // Map incoming category to the base name expected by "sg.<Base>Quality"
    // Note: Several CVars use singular form (Shadow/Texture/Reflection)
    const categoryBaseMap: Record<string, string> = {
      ViewDistance: 'ViewDistance',
      AntiAliasing: 'AntiAliasing',
      PostProcessing: 'PostProcess',
      PostProcess: 'PostProcess',
      Shadows: 'Shadow',
      GlobalIllumination: 'GlobalIllumination',
      Reflections: 'Reflection',
      Textures: 'Texture',
      Effects: 'Effects',
      Foliage: 'Foliage',
      Shading: 'Shading',
    };
    const requestedLevel = Number(params.level);
    if (!Number.isInteger(requestedLevel) || requestedLevel < 0 || requestedLevel > 4) {
      return {
        success: false,
        error: 'Invalid scalability level. Expected integer between 0 and 4.'
      };
    }

    const base = categoryBaseMap[params.category] || params.category;

    // Use direct console command to set with highest priority (SetByConsole)
    // This avoids conflicts with the scalability system
    const setCommand = `sg.${base}Quality ${requestedLevel}`;

    // Apply the console command directly
    await this.bridge.executeConsoleCommand(setCommand);

    return {
      success: true,
      message: `${params.category} quality set to level ${requestedLevel}`,
      method: 'Console'
    };
  }

  // Set resolution scale
  async setResolutionScale(params: {
    scale?: number; // Accepts both percentage (10-200) and multiplier (0.1-2.0)
  }) {
    // Validate input
    if (params.scale === undefined || params.scale === null || isNaN(params.scale)) {
      return { success: false, error: 'Invalid scale parameter' };
    }

    // Intelligently detect if scale is a percentage (10-200) or multiplier (0.1-2.0)
    // If scale >= 10, assume it's already a percentage value
    let percentage: number;
    if (params.scale >= 10) {
      // User passed percentage directly (e.g., 100 for 100%)
      percentage = Math.round(params.scale);
    } else {
      // User passed multiplier (e.g., 1.0 for 100%)
      percentage = Math.round(params.scale * 100);
    }

    // Clamp to Unreal's valid range (10-200%)
    const finalPercentage = Math.max(10, Math.min(200, percentage));

    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('set_resolution_scale', { scale: finalPercentage });

        if (response.success) return { success: true, message: `Resolution scale set to ${finalPercentage}% (bridge)`, actualScale: finalPercentage / 100 };
      } catch (_e) {
        // Fallback
      }
    }

    const command = `r.ScreenPercentage ${finalPercentage}`;

    try {
      await this.bridge.executeConsoleCommand(command);
      return {
        success: true,
        message: `Resolution scale set to ${finalPercentage}%`,
        actualScale: finalPercentage / 100
      };
    } catch (e: unknown) {
      return { success: false, error: `Failed to set resolution scale: ${e}` };
    }
  }

  // Enable/disable vsync
  async setVSync(params: {
    enabled: boolean;
  }) {
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('set_vsync', { enabled: params.enabled });
        if (response.success) return { success: true, message: 'VSync configured (bridge)' };
      } catch (_e) {
        // Fallback
      }
    }
    const command = `r.VSync ${params.enabled ? 1 : 0}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Set frame rate limit
  async setFrameRateLimit(params: {
    maxFPS?: number; // 0 for unlimited
  }) {
    const maxFPS = params.maxFPS ?? 0; // Default to unlimited
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('set_frame_rate_limit', { maxFPS });
        if (response.success) return { success: true, message: 'Max FPS set (bridge)' };
      } catch (_e) {
        // Fallback
      }
    }
    const command = `t.MaxFPS ${maxFPS}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Enable GPU timing
  async enableGPUTiming(params: {
    enabled: boolean;
  }) {
    // Note: C++ handler doesn't seem to have explicit 'enable_gpu_timing' in the list I saw earlier? 
    // Checking McpAutomationBridge_PerformanceHandlers.cpp content provided:
    // It has: generate_memory_report, start/stop_profiling, show_fps, show_stats, set_scalability, set_resolution_scale, set_vsync, set_frame_rate_limit, configure_nanite, configure_lod.
    // IT DOES NOT HAVE enable_gpu_timing.
    // So we stick to console command for this one, or add it to C++ later. 
    // I will NOT add bridge call here to avoid failure since I know it's missing.

    const command = `r.GPUStatsEnabled ${params.enabled ? 1 : 0}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Memory report
  async generateMemoryReport(params: {
    detailed?: boolean;
    outputPath?: string;
  }) {
    // If output path is specified, use Automation Bridge for file writing
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('generate_memory_report', {
          detailed: params.detailed ?? false,
          outputPath: params.outputPath
        });

        // Even if no output path, bridge can run the report
        if (response.success) {
          return { success: true, message: response.message || 'Memory report generated' };
        }
      } catch (error: unknown) {
        // Fallback only if no output path (since console can't save to file reliably)
        if (params.outputPath) {
          return { success: false, error: `Failed to generate memory report: ${error instanceof Error ? error.message : String(error)}` };
        }
      }
    }

    const commands: string[] = [];

    if (params.detailed) {
      commands.push('memreport -full');
    } else {
      commands.push('memreport');
    }

    // Writing reports to disk via console is not supported
    if (params.outputPath) {
      throw new Error('Saving memreport to a file requires Automation Bridge support');
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Memory report generated' };
  }

  // Texture streaming
  async configureTextureStreaming(params: {
    enabled: boolean;
    poolSize?: number; // MB
    boostPlayerLocation?: boolean;
  }) {
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('configure_texture_streaming', {
          enabled: params.enabled,
          poolSize: params.poolSize,
          boostPlayerLocation: params.boostPlayerLocation
        });

        if (response.success) return { success: true, message: response.message || 'Texture streaming configured (bridge)' };
      } catch (_error) {
        // Fallback
      }
    }

    if (params.boostPlayerLocation && !this.automationBridge) {
      throw new Error('Boosting player location for streaming requires Automation Bridge support');
    }

    const commands: string[] = [];

    commands.push(`r.TextureStreaming ${params.enabled ? 1 : 0}`);

    if (params.poolSize !== undefined) {
      commands.push(`r.Streaming.PoolSize ${params.poolSize}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Texture streaming configured' };
  }

  // LOD settings
  async configureLOD(params: {
    forceLOD?: number;
    lodBias?: number; // skeletal LOD bias (int)
    distanceScale?: number; // distance scale (float) applied to both static and skeletal
  }) {
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('configure_lod', {
          forceLOD: params.forceLOD,
          lodBias: params.lodBias
          // Note: C++ handler doesn't seem to have explicit 'distanceScale'.
          // We will stick to console for proper implementation of distanceScale
        });

        // If we have distanceScale, we still need to apply it via console as C++ seems to miss it
        if (params.distanceScale !== undefined) {
          await this.bridge.executeConsoleCommand(`r.StaticMeshLODDistanceScale ${params.distanceScale}`);
          await this.bridge.executeConsoleCommand(`r.SkeletalMeshLODDistanceScale ${params.distanceScale}`);
        }

        if (response.success) return { success: true, message: 'LOD settings configured' };
      } catch (_e) {
        // Fallback
      }
    }

    const commands: string[] = [];

    if (params.forceLOD !== undefined) {
      commands.push(`r.ForceLOD ${params.forceLOD}`);
    }

    if (params.lodBias !== undefined) {
      // Skeletal mesh LOD bias is an integer bias value
      commands.push(`r.SkeletalMeshLODBias ${params.lodBias}`);
    }

    if (params.distanceScale !== undefined) {
      // Apply distance scale to both static and skeletal meshes
      commands.push(`r.StaticMeshLODDistanceScale ${params.distanceScale}`);
      commands.push(`r.SkeletalMeshLODDistanceScale ${params.distanceScale}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'LOD settings configured' };
  }

  // Apply a baseline performance profile (explicit CVar enforcement)
  async applyBaselinePerformanceSettings(params?: {
    distanceScale?: number; // default 1.0
    skeletalBias?: number;  // default 0
    vsync?: boolean;        // default false
    maxFPS?: number;        // default 60
    hzb?: boolean;          // default true
  }) {
    // This is a composite helper, stick to console or individual bridge calls.
    // Console is efficient enough for batch cvar setting.
    const p = {
      distanceScale: params?.distanceScale ?? 1.0,
      skeletalBias: params?.skeletalBias ?? 0,
      vsync: params?.vsync ?? false,
      maxFPS: params?.maxFPS ?? 60,
      hzb: params?.hzb ?? true,
    };

    const commands = [
      `r.StaticMeshLODDistanceScale ${p.distanceScale}`,
      `r.SkeletalMeshLODDistanceScale ${p.distanceScale}`,
      `r.SkeletalMeshLODBias ${p.skeletalBias}`,
      `r.HZBOcclusion ${p.hzb ? 1 : 0}`,
      `r.VSync ${p.vsync ? 1 : 0}`,
      `t.MaxFPS ${p.maxFPS}`,
    ];

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Baseline performance settings applied', params: p };
  }

  // Draw call optimization
  async optimizeDrawCalls(params: {
    enableInstancing?: boolean;
    enableBatching?: boolean;
    mergeActors?: boolean;
    actors?: string[];
  }) {
    // If merging actors, bridge is required and actors must be provided
    if (params.mergeActors) {
      if (this.automationBridge) {
        try {
          const actors = Array.isArray(params.actors)
            ? params.actors.filter((name): name is string => typeof name === 'string' && name.length > 0)
            : undefined;

          if (!actors || actors.length < 2) {
            return {
              success: false,
              error: 'Merge actors requires an "actors" array with at least 2 valid actor names.'
            };
          }

          const payload: Record<string, unknown> = {
            enableInstancing: params.enableInstancing,
            mergeActors: params.mergeActors,
            actors: actors
          };

          const response = await this.automationBridge.sendAutomationRequest('merge_actors', payload);

          return response.success
            ? { success: true, message: response.message || 'Actors merged for optimization' }
            : { success: false, error: response.message || response.error || 'Failed to merge actors' };
        } catch (error: unknown) {
          return { success: false, error: `Failed to merge actors: ${error instanceof Error ? error.message : String(error)}` };
        }
      }
      throw new Error('Actor merging requires Automation Bridge support');
    }

    const commands: string[] = [];

    if (params.enableInstancing !== undefined) {
      commands.push(`r.MeshDrawCommands.DynamicInstancing ${params.enableInstancing ? 1 : 0}`);
    }

    // Avoid using r.RHICmdBypass; it's a low-level debug toggle and not suitable for general batching control

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Draw call optimization configured' };
  }

  // Occlusion culling
  async configureOcclusionCulling(params: {
    enabled: boolean;
    method?: string;
    freezeRendering?: boolean;
  }) {
    const commands: string[] = [];

    // Enable/disable HZB occlusion (boolean)
    commands.push(`r.HZBOcclusion ${params.enabled ? 1 : 0}`);

    // Optional freeze rendering toggle
    if (params.freezeRendering !== undefined) {
      commands.push(`FreezeRendering ${params.freezeRendering ? 1 : 0}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Occlusion culling configured' };
  }

  // Shader compilation
  async optimizeShaders(params: {
    compileOnDemand?: boolean;
    cacheShaders?: boolean;
    reducePermutations?: boolean;
  }) {
    const commands: string[] = [];

    if (params.compileOnDemand !== undefined) {
      commands.push(`r.ShaderDevelopmentMode ${params.compileOnDemand ? 1 : 0}`);
    }

    if (params.cacheShaders !== undefined) {
      commands.push(`r.ShaderPipelineCache.Enabled ${params.cacheShaders ? 1 : 0}`);
    }

    if (params.reducePermutations) {
      commands.push('RecompileShaders changed');
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Shader optimization configured' };
  }

  // Nanite settings
  async configureNanite(params: {
    enabled: boolean;
    maxPixelsPerEdge?: number;
    streamingPoolSize?: number;
  }) {
    if (this.automationBridge) {
      try {
        const response = await this.automationBridge.sendAutomationRequest('configure_nanite', {
          enabled: params.enabled,
          maxPixelsPerEdge: params.maxPixelsPerEdge,
          streamingPoolSize: params.streamingPoolSize
        });
        // C++ handler snippet only showed `r.Nanite`. 
        // Checking snippet: `if (CVar) CVar->Set(bEnabled ? 1 : 0);`
        // It missed maxPixelsPerEdge and streamingPoolSize in the snippet I read.
        // Let's rely on fallback or partial console commands for the extras.
        if (params.maxPixelsPerEdge !== undefined) {
          await this.bridge.executeConsoleCommand(`r.Nanite.MaxPixelsPerEdge ${params.maxPixelsPerEdge}`);
        }
        if (params.streamingPoolSize !== undefined) {
          await this.bridge.executeConsoleCommand(`r.Nanite.StreamingPoolSize ${params.streamingPoolSize}`);
        }

        if (response.success) return { success: true, message: 'Nanite configured' };
      } catch (_e) {
        // Fallback
      }
    }

    const commands: string[] = [];

    commands.push(`r.Nanite ${params.enabled ? 1 : 0}`);

    if (params.maxPixelsPerEdge !== undefined) {
      commands.push(`r.Nanite.MaxPixelsPerEdge ${params.maxPixelsPerEdge}`);
    }

    if (params.streamingPoolSize !== undefined) {
      commands.push(`r.Nanite.StreamingPoolSize ${params.streamingPoolSize}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Nanite configured' };
  }

  // World Partition streaming
  async configureWorldPartition(params: {
    enabled: boolean;
    streamingDistance?: number;
    cellSize?: number;
  }) {
    const commands: string[] = [];

    commands.push(`wp.Runtime.EnableStreaming ${params.enabled ? 1 : 0}`);

    if (params.streamingDistance !== undefined) {
      commands.push(`wp.Runtime.StreamingDistance ${params.streamingDistance}`);
    }

    if (params.cellSize !== undefined) {
      commands.push(`wp.Runtime.CellSize ${params.cellSize}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'World Partition configured' };
  }

  // Benchmark
  async runBenchmark(params: {
    duration?: number;
    outputPath?: string;
  }) {
    const duration = params.duration || 60;

    // Start recording and GPU profiling
    await this.bridge.executeConsoleCommands(['stat startfile', 'profilegpu']);

    // Wait for the requested duration
    await new Promise(resolve => setTimeout(resolve, duration * 1000));

    // Stop recording and clear stats
    await this.bridge.executeConsoleCommands(['stat stopfile', 'stat none']);

    return { success: true, message: `Benchmark completed for ${duration} seconds` };
  }
}
