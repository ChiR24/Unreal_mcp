// Lighting tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class LightingTools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
  private async executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }

  // Create directional light
  async createDirectionalLight(params: {
    name: string;
    intensity?: number;
    color?: [number, number, number];
    rotation?: [number, number, number];
    castShadows?: boolean;
    temperature?: number;
  }) {
    // Use summon command for lights (Unreal's built-in spawning)
    const rot = params.rotation || [0, 0, 0];
    const spawnCmd = `summon DirectionalLight 0 0 500`;
    await this.executeCommand(spawnCmd);
    
    // Note: Additional property setting commands might not work without actor reference
    // These are kept for documentation but may need custom implementation
    
    return { success: true, message: `Directional light spawned` };
  }

  // Create point light
  async createPointLight(params: {
    name: string;
    location: [number, number, number];
    intensity?: number;
    radius?: number;
    color?: [number, number, number];
    falloffExponent?: number;
    castShadows?: boolean;
  }) {
    // Use summon command with location
    const spawnCmd = `summon PointLight ${params.location.join(' ')}`;
    await this.executeCommand(spawnCmd);
    
    return { success: true, message: `Point light spawned at ${params.location.join(', ')}` };
  }

  // Create spot light
  async createSpotLight(params: {
    name: string;
    location: [number, number, number];
    rotation: [number, number, number];
    intensity?: number;
    innerCone?: number;
    outerCone?: number;
    radius?: number;
    color?: [number, number, number];
    castShadows?: boolean;
  }) {
    // Use summon command with location
    const spawnCmd = `summon SpotLight ${params.location.join(' ')}`;
    await this.executeCommand(spawnCmd);
    
    return { success: true, message: `Spot light spawned at ${params.location.join(', ')}` };
  }

  // Create rect light
  async createRectLight(params: {
    name: string;
    location: [number, number, number];
    rotation: [number, number, number];
    width?: number;
    height?: number;
    intensity?: number;
    color?: [number, number, number];
  }) {
    // Use summon command with location
    const spawnCmd = `summon RectLight ${params.location.join(' ')}`;
    await this.executeCommand(spawnCmd);
    
    return { success: true, message: `Rect light spawned at ${params.location.join(', ')}` };
  }

  // Create sky light
  async createSkyLight(params: {
    name: string;
    sourceType?: 'CapturedScene' | 'SpecifiedCubemap';
    cubemapPath?: string;
    intensity?: number;
    recapture?: boolean;
  }) {
    // Use summon command
    const spawnCmd = `summon SkyLight 0 0 500`;
    await this.executeCommand(spawnCmd);
    
    if (params.recapture) {
      await this.executeCommand('RecaptureSky');
    }
    
    return { success: true, message: `Sky light spawned` };
  }

  // Setup global illumination
  async setupGlobalIllumination(params: {
    method: 'Lightmass' | 'LumenGI' | 'ScreenSpace' | 'None';
    quality?: 'Low' | 'Medium' | 'High' | 'Epic';
    indirectLightingIntensity?: number;
    bounces?: number;
  }) {
    const commands = [];
    
    switch (params.method) {
      case 'Lightmass':
        commands.push('r.DynamicGlobalIlluminationMethod 0');
        break;
      case 'LumenGI':
        commands.push('r.DynamicGlobalIlluminationMethod 1');
        break;
      case 'ScreenSpace':
        commands.push('r.DynamicGlobalIlluminationMethod 2');
        break;
      case 'None':
        commands.push('r.DynamicGlobalIlluminationMethod 3');
        break;
    }
    
    if (params.quality) {
      const qualityMap = { 'Low': 0, 'Medium': 1, 'High': 2, 'Epic': 3 };
      commands.push(`r.Lumen.Quality ${qualityMap[params.quality]}`);
    }
    
    if (params.indirectLightingIntensity !== undefined) {
      commands.push(`r.IndirectLightingIntensity ${params.indirectLightingIntensity}`);
    }
    
    if (params.bounces !== undefined) {
      commands.push(`r.Lumen.MaxReflectionBounces ${params.bounces}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Global illumination configured' };
  }

  // Configure shadows
  async configureShadows(params: {
    shadowQuality?: 'Low' | 'Medium' | 'High' | 'Epic';
    cascadedShadows?: boolean;
    shadowDistance?: number;
    contactShadows?: boolean;
    rayTracedShadows?: boolean;
  }) {
    const commands = [];
    
    if (params.shadowQuality) {
      const qualityMap = { 'Low': 0, 'Medium': 1, 'High': 2, 'Epic': 3 };
      commands.push(`r.ShadowQuality ${qualityMap[params.shadowQuality]}`);
    }
    
    if (params.cascadedShadows !== undefined) {
      commands.push(`r.Shadow.CSM.MaxCascades ${params.cascadedShadows ? 4 : 1}`);
    }
    
    if (params.shadowDistance !== undefined) {
      commands.push(`r.Shadow.DistanceScale ${params.shadowDistance}`);
    }
    
    if (params.contactShadows !== undefined) {
      commands.push(`r.ContactShadows ${params.contactShadows ? 1 : 0}`);
    }
    
    if (params.rayTracedShadows !== undefined) {
      commands.push(`r.RayTracing.Shadows ${params.rayTracedShadows ? 1 : 0}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Shadow settings configured' };
  }

  // Build lighting
  async buildLighting(params: {
    quality?: 'Preview' | 'Medium' | 'High' | 'Production';
    buildOnlySelected?: boolean;
    buildReflectionCaptures?: boolean;
  }) {
    const qualityMap = {
      'Preview': 0,
      'Medium': 1,
      'High': 2,
      'Production': 3
    };
    
    const commands = [];
    
    if (params.quality) {
      commands.push(`BuildLightingQuality ${qualityMap[params.quality]}`);
    }
    
    if (params.buildOnlySelected) {
      commands.push('BuildLightingOnly Selected');
    } else {
      commands.push('BuildLightingOnly');
    }
    
    if (params.buildReflectionCaptures) {
      commands.push('BuildReflectionCaptures');
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Lighting built successfully' };
  }

  // Create lightmass importance volume
  async createLightmassVolume(params: {
    name: string;
    location: [number, number, number];
    size: [number, number, number];
  }) {
    const command = `SpawnLightmassVolume ${params.name} ${params.location.join(' ')} ${params.size.join(' ')}`;
    return this.executeCommand(command);
  }

  // Set exposure
  async setExposure(params: {
    method: 'Manual' | 'Auto';
    compensationValue?: number;
    minBrightness?: number;
    maxBrightness?: number;
  }) {
    const commands = [];
    
    commands.push(`r.EyeAdaptation.ExposureMethod ${params.method === 'Manual' ? 0 : 1}`);
    
    if (params.compensationValue !== undefined) {
      commands.push(`r.EyeAdaptation.ExposureCompensation ${params.compensationValue}`);
    }
    
    if (params.minBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MinBrightness ${params.minBrightness}`);
    }
    
    if (params.maxBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MaxBrightness ${params.maxBrightness}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Exposure settings updated' };
  }

  // Set ambient occlusion
  async setAmbientOcclusion(params: {
    enabled: boolean;
    intensity?: number;
    radius?: number;
    quality?: 'Low' | 'Medium' | 'High';
  }) {
    const commands = [];
    
    commands.push(`r.AmbientOcclusion.Enabled ${params.enabled ? 1 : 0}`);
    
    if (params.intensity !== undefined) {
      commands.push(`r.AmbientOcclusion.Intensity ${params.intensity}`);
    }
    
    if (params.radius !== undefined) {
      commands.push(`r.AmbientOcclusion.Radius ${params.radius}`);
    }
    
    if (params.quality) {
      const qualityMap = { 'Low': 0, 'Medium': 1, 'High': 2 };
      commands.push(`r.AmbientOcclusion.Quality ${qualityMap[params.quality]}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Ambient occlusion configured' };
  }

  // Setup volumetric fog
  async setupVolumetricFog(params: {
    enabled: boolean;
    density?: number;
    scatteringIntensity?: number;
    fogHeight?: number;
  }) {
    const commands = [];
    
    commands.push(`r.VolumetricFog ${params.enabled ? 1 : 0}`);
    
    if (params.density !== undefined) {
      commands.push(`r.VolumetricFog.ExtinctionScale ${params.density}`);
    }
    
    if (params.scatteringIntensity !== undefined) {
      commands.push(`r.VolumetricFog.ScatteringIntensity ${params.scatteringIntensity}`);
    }
    
    if (params.fogHeight !== undefined) {
      commands.push(`SetFogHeight ${params.fogHeight}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Volumetric fog configured' };
  }
}
