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

  // Set scalability settings with correct CVar names and verify via GameUserSettings
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

    // Apply the scalability setting first
    await this.bridge.executeConsoleCommand(command);

    // Best-effort verification via GameUserSettings getters (editor-safe)
    const py = `
import unreal, json
result = {'success': True, 'category': '${base}', 'requested': ${params.level}, 'actual': -1, 'method': 'GameUserSettings'}
try:
    gus = unreal.GameUserSettings.get_game_user_settings()
    if gus:
        # Keep GameUserSettings in sync so readback matches
        set_map = {
            'ViewDistance': 'set_view_distance_quality',
            'AntiAliasing': 'set_anti_aliasing_quality',
            'PostProcess': 'set_post_process_quality',
            'Shadow': 'set_shadow_quality',
            'GlobalIllumination': 'set_global_illumination_quality',
            'Reflection': 'set_reflection_quality',
            'Texture': 'set_texture_quality',
            'Effects': 'set_effects_quality',
            'Foliage': 'set_foliage_quality',
            'Shading': 'set_shading_quality',
        }
        get_map = {
            'ViewDistance': 'get_view_distance_quality',
            'AntiAliasing': 'get_anti_aliasing_quality',
            'PostProcess': 'get_post_process_quality',
            'Shadow': 'get_shadow_quality',
            'GlobalIllumination': 'get_global_illumination_quality',
            'Reflection': 'get_reflection_quality',
            'Texture': 'get_texture_quality',
            'Effects': 'get_effects_quality',
            'Foliage': 'get_foliage_quality',
            'Shading': 'get_shading_quality',
        }
        sfn = set_map.get('${base}')
        if sfn and hasattr(gus, sfn):
            getattr(gus, sfn)(${params.level})
            try:
                gus.apply_settings(False)
            except Exception:
                pass
        gfn = get_map.get('${base}')
        if gfn and hasattr(gus, gfn):
            result['actual'] = int(getattr(gus, gfn)())
        else:
            result['method'] = 'CVarOnly'
    else:
        result['method'] = 'NoGameUserSettings'
except Exception as e:
    result['success'] = False
    result['error'] = str(e)
print('RESULT:' + json.dumps(result))
`.trim();

    try {
      const pyResp = await this.bridge.executePython(py);
      let out = '';
      if (pyResp?.LogOutput && Array.isArray(pyResp.LogOutput)) out = pyResp.LogOutput.map((l: any) => l.Output || '').join('');
      else if (typeof pyResp === 'string') out = pyResp; else out = JSON.stringify(pyResp);
      const m = out.match(/RESULT:({.*})/);
      if (m) {
        try {
          const parsed = JSON.parse(m[1]);
          const verified = parsed.success && (parsed.actual === undefined || parsed.actual === null ? true : parsed.actual === params.level);
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
    } catch {}

    // Fallback: return simple success without verification
    return { success: true, message: `${params.category} quality set to level ${params.level}` };
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
    lodBias?: number;
    distanceScale?: number;
  }) {
    const commands = [];
    
    if (params.forceLOD !== undefined) {
      commands.push(`r.ForceLOD ${params.forceLOD}`);
    }
    
    if (params.lodBias !== undefined) {
      commands.push(`r.StaticMeshLODDistanceScale ${params.lodBias}`);
    }
    
    if (params.distanceScale !== undefined) {
      commands.push(`r.SkeletalMeshLODBias ${params.distanceScale}`);
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'LOD settings configured' };
  }

  // Draw call optimization
  async optimizeDrawCalls(params: {
    enableInstancing?: boolean;
    enableBatching?: boolean;
    mergeActors?: boolean;
  }) {
    const commands = [];
    
    if (params.enableInstancing !== undefined) {
      commands.push(`r.MeshDrawCommands.DynamicInstancing ${params.enableInstancing ? 1 : 0}`);
    }
    
    if (params.enableBatching !== undefined) {
      commands.push(`r.RHICmdBypass ${params.enableBatching ? 1 : 0}`);
    }
    
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
    
    commands.push(`r.AllowOcclusionQueries ${params.enabled ? 1 : 0}`);
    
    if (params.method) {
      switch (params.method) {
        case 'Hardware':
          commands.push('r.HZBOcclusion 0');
          break;
        case 'Software':
          commands.push('r.HZBOcclusion 1');
          break;
        case 'Hierarchical':
          commands.push('r.HZBOcclusion 2');
          break;
      }
    }
    
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
    const commands = [];
    
    commands.push('stat startfile');
    commands.push('profilegpu');
    commands.push(`exec Benchmark ${duration}`);
    
    if (params.outputPath) {
      commands.push(`stat stopfile ${params.outputPath}`);
    } else {
      commands.push('stat stopfile');
    }
    
    for (const cmd of commands) {
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: `Benchmark running for ${duration} seconds` };
  }
}
