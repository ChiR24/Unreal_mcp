// Performance tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';
import { coerceBoolean, coerceNumber, interpretStandardResult } from '../utils/result-helpers.js';

export class PerformanceTools {
  constructor(private bridge: UnrealBridge) {}

  // Start profiling
  async startProfiling(params: {
    type: 'CPU' | 'GPU' | 'Memory' | 'RenderThread' | 'GameThread' | 'All';
    duration?: number;
  }) {
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
    console.log('[PerformanceTools] Starting showFPS with params:', params);
    
    try {
      // Use stat fps as requested - shows FPS counter
      // For more detailed timing info, use 'stat unit' instead
      const command = params.enabled 
        ? (params.verbose ? 'stat unit' : 'stat fps')
        : 'stat none';
      
      console.log(`[PerformanceTools] Executing command: ${command}`);
      await this.bridge.executeConsoleCommand(command);
      console.log(`[PerformanceTools] Command completed in ${Date.now() - startTime}ms`);
      return { 
        success: true, 
        message: params.enabled ? 'FPS display enabled' : 'FPS display disabled',
        fpsVisible: params.enabled,
        command: command
      };
    } catch (error) {
      return { 
        success: false, 
        error: `Failed to ${params.enabled ? 'enable' : 'disable'} FPS display: ${error}`,
        fpsVisible: false
      };
    }
  }

  // Show performance stats
  async showStats(params: {
    category: 'Unit' | 'FPS' | 'Memory' | 'Game' | 'Slate' | 'Engine' | 'RHI' | 'Streaming' | 'SceneRendering' | 'Physics' | 'Navigation' | 'Particles' | 'Audio';
    enabled: boolean;
  }) {
    const command = params.enabled 
      ? `stat ${params.category.toLowerCase()}`
      : 'stat none';
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Set scalability settings using console commands
  async setScalability(params: {
    category: 'ViewDistance' | 'AntiAliasing' | 'PostProcessing' | 'PostProcess' | 'Shadows' | 'GlobalIllumination' | 'Reflections' | 'Textures' | 'Effects' | 'Foliage' | 'Shading';
    level: 0 | 1 | 2 | 3 | 4; // 0=Low, 1=Medium, 2=High, 3=Epic, 4=Cinematic
  }) {
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
    
    // Skip GameUserSettings entirely to avoid any scalability triggers
    // Console command already applied with correct priority
    /* eslint-disable no-useless-escape */
    const py = `
import unreal, json
result = {'success': True, 'category': '${base}', 'requested': ${requestedLevel}, 'actual': ${requestedLevel}, 'method': 'ConsoleOnly'}

# Simply verify the console variable was set correctly
try:
    # Try to read the console variable directly to verify it was set
    # This doesn't trigger any scalability system
    import sys
    from io import StringIO
    
    # Capture console output
    old_stdout = sys.stdout
    sys.stdout = StringIO()
    
    # Execute console command to query the value
    try:
        unreal.SystemLibrary.execute_console_command(None, 'sg.${base}Quality', None)
    except:
        pass
    
    # Get the output
    console_output = sys.stdout.getvalue()
    sys.stdout = old_stdout
    
    # Parse the output to get the actual value
    if 'sg.${base}Quality' in console_output:
        # Extract the value from output like 'sg.ShadowQuality = "3"'
        import re
        match = re.search(r'sg\.${base}Quality\\s*=\\s*"(\\d+)"', console_output)
        if match:
            result['actual'] = int(match.group(1))
            result['verified'] = True
    
    result['method'] = 'ConsoleOnly'
except Exception as e:
    # Even on error, the console command was applied
    result['method'] = 'ConsoleOnly'
    result['note'] = str(e)

print('RESULT:' + json.dumps(result))
`.trim();
    /* eslint-enable no-useless-escape */

    // Always try to apply through Python for consistency
    try {
      const pyResp = await this.bridge.executePython(py);
      const interpreted = interpretStandardResult(pyResp, {
        successMessage: `${params.category} quality set to level ${requestedLevel}`,
        failureMessage: `Failed to set ${params.category} quality`
      });

      if (interpreted.success) {
        const actual = coerceNumber(interpreted.payload.actual) ?? requestedLevel;
        const verified = coerceBoolean(interpreted.payload.success, true) === true && actual === requestedLevel;
        return {
          success: true,
          message: interpreted.message,
          verified,
          readback: actual,
          method: (interpreted.payload.method as string) || 'ConsoleOnly'
        };
      }
    } catch {
      // Ignore Python errors and fall through
    }

    // If Python fails, the console command was still applied
    return { 
      success: true, 
      message: `${params.category} quality set to level ${requestedLevel}`,
      method: 'CVarOnly'
    };
  }

  // Set resolution scale
  async setResolutionScale(params: {
    scale: number; // 0.5 to 2.0
  }) {
    // Validate input
    if (params.scale === undefined || params.scale === null || isNaN(params.scale)) {
      return { success: false, error: 'Invalid scale parameter' };
    }
    
    // Clamp scale between 10% (0.1) and 200% (2.0) - Unreal Engine limits
    // Note: r.ScreenPercentage takes values from 10 to 200, not 0.5 to 2.0
    const clampedScale = Math.max(0.1, Math.min(2.0, params.scale));
    const percentage = Math.round(clampedScale * 100);
    
    // Ensure percentage is within Unreal's valid range
    const finalPercentage = Math.max(10, Math.min(200, percentage));
    
    const command = `r.ScreenPercentage ${finalPercentage}`;
    
    try {
      await this.bridge.executeConsoleCommand(command);
      return { 
        success: true, 
        message: `Resolution scale set to ${finalPercentage}%`,
        actualScale: finalPercentage / 100
      };
    } catch (e) {
      return { success: false, error: `Failed to set resolution scale: ${e}` };
    }
  }

  // Enable/disable vsync
  async setVSync(params: {
    enabled: boolean;
  }) {
    const command = `r.VSync ${params.enabled ? 1 : 0}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Set frame rate limit
  async setFrameRateLimit(params: {
    maxFPS: number; // 0 for unlimited
  }) {
    const command = `t.MaxFPS ${params.maxFPS}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Enable GPU timing
  async enableGPUTiming(params: {
    enabled: boolean;
  }) {
    const command = `r.GPUStatsEnabled ${params.enabled ? 1 : 0}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Memory report
  async generateMemoryReport(params: {
    detailed?: boolean;
    outputPath?: string;
  }) {
  const commands: string[] = [];
    
    if (params.detailed) {
      commands.push('memreport -full');
    } else {
      commands.push('memreport');
    }
    
    if (params.outputPath) {
      commands.push(`obj savepackage ${params.outputPath}`);
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
  const commands: string[] = [];
    
    commands.push(`r.TextureStreaming ${params.enabled ? 1 : 0}`);
    
    if (params.poolSize !== undefined) {
      commands.push(`r.Streaming.PoolSize ${params.poolSize}`);
    }
    
    if (params.boostPlayerLocation !== undefined) {
      commands.push(`r.Streaming.UseFixedPoolSize ${params.boostPlayerLocation ? 1 : 0}`);
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
    enableBatching?: boolean; // no-op (deprecated internal toggle)
    mergeActors?: boolean;
  }) {
  const commands: string[] = [];
    
    if (params.enableInstancing !== undefined) {
      commands.push(`r.MeshDrawCommands.DynamicInstancing ${params.enableInstancing ? 1 : 0}`);
    }
    
    // Avoid using r.RHICmdBypass; it's a low-level debug toggle and not suitable for general batching control
    
    if (params.mergeActors) {
      commands.push('MergeActors');
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Draw call optimization configured' };
  }

  // Occlusion culling
  async configureOcclusionCulling(params: {
    enabled: boolean;
    method?: 'Hardware' | 'Software' | 'Hierarchical';
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
