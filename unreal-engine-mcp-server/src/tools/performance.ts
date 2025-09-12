// Performance tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class PerformanceTools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
  private async executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        WorldContextObject: null,
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }

  // Start profiling
  async startProfiling(params: {
    type: 'CPU' | 'GPU' | 'Memory' | 'RenderThread' | 'GameThread' | 'All';
    duration?: number;
  }) {
    const commands = [];
    
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
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `${params.type} profiling started` };
  }

  // Stop profiling
  async stopProfiling() {
    const commands = [
      'stat stopfile',
      'stat none'
    ];
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Profiling stopped' };
  }

  // Show FPS
  async showFPS(params: {
    enabled: boolean;
    verbose?: boolean;
  }) {
    // Use stat unit instead of stat fps to avoid console object lookup warnings
    // stat unit shows Frame, Game, Draw, and GPU times which is more comprehensive
    const command = params.enabled 
      ? (params.verbose ? 'stat unit' : 'stat unit')
      : 'stat none';
    
    await this.bridge.executeConsoleCommand(command);
    return { success: true, message: params.enabled ? 'FPS display enabled' : 'FPS display disabled' };
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

    const base = categoryBaseMap[params.category] || params.category;
    const command = `sg.${base}Quality ${params.level}`;

    // Apply the scalability setting
    await this.bridge.executeConsoleCommand(command);

    // Simple verification using only available GameUserSettings methods
    // Only the core quality settings have Python API support
    const coreSettings = ['ViewDistance', 'AntiAliasing', 'PostProcess', 'Shadow', 'Texture'];
    
    if (coreSettings.includes(base)) {
      // Try to verify using Python for core settings only
      const py = `
import unreal, json
result = {'success': True, 'category': '${base}', 'requested': ${params.level}, 'actual': -1, 'method': 'GameUserSettings'}
try:
    gus = unreal.GameUserSettings.get_game_user_settings()
    if gus:
        # Only use methods that actually exist in the Python API
        get_map = {
            'ViewDistance': 'get_view_distance_quality',
            'AntiAliasing': 'get_anti_aliasing_quality',
            'PostProcess': 'get_post_processing_quality',  # Note: different name
            'Shadow': 'get_shadow_quality',
            'Texture': 'get_texture_quality',
        }
        gfn = get_map.get('${base}')
        if gfn and hasattr(gus, gfn):
            result['actual'] = int(getattr(gus, gfn)())
        else:
            result['method'] = 'CVarOnly'
    else:
        result['method'] = 'NoGameUserSettings'
except Exception as e:
    # Silently handle errors - don't propagate them
    result['method'] = 'CVarOnly'
print('RESULT:' + json.dumps(result))
`.trim();

      try {
        const pyResp = await this.bridge.executePython(py);
        let out = '';
        if (pyResp?.LogOutput && Array.isArray(pyResp.LogOutput)) {
          out = pyResp.LogOutput.map((l: any) => l.Output || '').join('');
        } else if (typeof pyResp === 'string') {
          out = pyResp;
        } else {
          out = JSON.stringify(pyResp);
        }
        
        const m = out.match(/RESULT:({.*})/);
        if (m) {
          try {
            const parsed = JSON.parse(m[1]);
            const verified = parsed.success && (parsed.actual === params.level);
            return {
              success: true,
              message: `${params.category} quality set to level ${params.level}`,
              verified,
              readback: parsed.actual,
              method: parsed.method || 'Unknown'
            };
          } catch {
            // Fall through to simple success
          }
        }
      } catch {
        // Ignore Python errors and fall through
      }
    }

    // For non-core settings or if Python verification fails, return simple success
    return { 
      success: true, 
      message: `${params.category} quality set to level ${params.level}`,
      method: 'CVarOnly'
    };
  }

  // Set resolution scale
  async setResolutionScale(params: {
    scale: number; // 0.5 to 2.0
  }) {
    const clampedScale = Math.max(0.5, Math.min(2.0, params.scale));
    const command = `r.ScreenPercentage ${clampedScale * 100}`;
    return this.bridge.executeConsoleCommand(command);
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
    const commands = [];
    
    if (params.detailed) {
      commands.push('memreport -full');
    } else {
      commands.push('memreport');
    }
    
    if (params.outputPath) {
      commands.push(`obj savepackage ${params.outputPath}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Memory report generated' };
  }

  // Texture streaming
  async configureTextureStreaming(params: {
    enabled: boolean;
    poolSize?: number; // MB
    boostPlayerLocation?: boolean;
  }) {
    const commands = [];
    
    commands.push(`r.TextureStreaming ${params.enabled ? 1 : 0}`);
    
    if (params.poolSize !== undefined) {
      commands.push(`r.Streaming.PoolSize ${params.poolSize}`);
    }
    
    if (params.boostPlayerLocation !== undefined) {
      commands.push(`r.Streaming.UseFixedPoolSize ${params.boostPlayerLocation ? 1 : 0}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Texture streaming configured' };
  }

  // LOD settings
  async configureLOD(params: {
    forceLOD?: number;
    lodBias?: number; // skeletal LOD bias (int)
    distanceScale?: number; // distance scale (float) applied to both static and skeletal
  }) {
    const commands = [];
    
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
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
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

    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }

    return { success: true, message: 'Baseline performance settings applied', params: p };
  }

  // Draw call optimization
  async optimizeDrawCalls(params: {
    enableInstancing?: boolean;
    enableBatching?: boolean; // no-op (deprecated internal toggle)
    mergeActors?: boolean;
  }) {
    const commands = [];
    
    if (params.enableInstancing !== undefined) {
      commands.push(`r.MeshDrawCommands.DynamicInstancing ${params.enableInstancing ? 1 : 0}`);
    }
    
    // Avoid using r.RHICmdBypass; it's a low-level debug toggle and not suitable for general batching control
    
    if (params.mergeActors) {
      commands.push('MergeActors');
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Draw call optimization configured' };
  }

  // Occlusion culling
  async configureOcclusionCulling(params: {
    enabled: boolean;
    method?: 'Hardware' | 'Software' | 'Hierarchical';
    freezeRendering?: boolean;
  }) {
    const commands = [];
    
    // Enable/disable HZB occlusion (boolean)
    commands.push(`r.HZBOcclusion ${params.enabled ? 1 : 0}`);
    
    // Optional freeze rendering toggle
    if (params.freezeRendering !== undefined) {
      commands.push(`FreezeRendering ${params.freezeRendering ? 1 : 0}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Occlusion culling configured' };
  }

  // Shader compilation
  async optimizeShaders(params: {
    compileOnDemand?: boolean;
    cacheShaders?: boolean;
    reducePermutations?: boolean;
  }) {
    const commands = [];
    
    if (params.compileOnDemand !== undefined) {
      commands.push(`r.ShaderDevelopmentMode ${params.compileOnDemand ? 1 : 0}`);
    }
    
    if (params.cacheShaders !== undefined) {
      commands.push(`r.ShaderPipelineCache.Enabled ${params.cacheShaders ? 1 : 0}`);
    }
    
    if (params.reducePermutations) {
      commands.push('RecompileShaders changed');
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Shader optimization configured' };
  }

  // Nanite settings
  async configureNanite(params: {
    enabled: boolean;
    maxPixelsPerEdge?: number;
    streamingPoolSize?: number;
  }) {
    const commands = [];
    
    commands.push(`r.Nanite ${params.enabled ? 1 : 0}`);
    
    if (params.maxPixelsPerEdge !== undefined) {
      commands.push(`r.Nanite.MaxPixelsPerEdge ${params.maxPixelsPerEdge}`);
    }
    
    if (params.streamingPoolSize !== undefined) {
      commands.push(`r.Nanite.StreamingPoolSize ${params.streamingPoolSize}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Nanite configured' };
  }

  // World Partition streaming
  async configureWorldPartition(params: {
    enabled: boolean;
    streamingDistance?: number;
    cellSize?: number;
  }) {
    const commands = [];
    
    commands.push(`wp.Runtime.EnableStreaming ${params.enabled ? 1 : 0}`);
    
    if (params.streamingDistance !== undefined) {
      commands.push(`wp.Runtime.StreamingDistance ${params.streamingDistance}`);
    }
    
    if (params.cellSize !== undefined) {
      commands.push(`wp.Runtime.CellSize ${params.cellSize}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'World Partition configured' };
  }

  // Benchmark
  async runBenchmark(params: {
    duration?: number;
    outputPath?: string;
  }) {
    const duration = params.duration || 60;
    
    // Start recording and GPU profiling
    await this.bridge.executeConsoleCommand('stat startfile');
    await this.bridge.executeConsoleCommand('profilegpu');
    
    // Wait for the requested duration
    await new Promise(resolve => setTimeout(resolve, duration * 1000));
    
    // Stop recording and clear stats
    await this.bridge.executeConsoleCommand('stat stopfile');
    await this.bridge.executeConsoleCommand('stat none');
    
    return { success: true, message: `Benchmark completed for ${duration} seconds` };
  }
}
