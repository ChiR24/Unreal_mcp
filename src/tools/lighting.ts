// Lighting tools for Unreal Engine
//
// Light creation methods now use the Automation Bridge for actor spawning.
// Python fallbacks have been removed in favor of direct plugin communication.
//
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';

export class LightingTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }


  private normalizeName(value: unknown, defaultName?: string): string {
    if (typeof value === 'string') {
      const trimmed = value.trim();
      if (trimmed.length > 0) {
        return trimmed;
      }
    }

    if (typeof defaultName === 'string') {
      const trimmedDefault = defaultName.trim();
      if (trimmedDefault.length > 0) {
        return trimmedDefault;
      }
    }

    throw new Error('Invalid name: must be a non-empty string');
  }

  /**
   * Spawn a light actor using the Automation Bridge.
   * @param lightClass The Unreal light class name (e.g. 'DirectionalLight', 'PointLight')
   * @param params Light spawn parameters
   */
  private async spawnLightViaAutomation(
    lightClass: string,
    params: {
      name: string;
      location?: [number, number, number];
      rotation?: [number, number, number];
      properties?: Record<string, any>;
    }
  ) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Cannot spawn lights without plugin support.');
    }

    try {
      const payload: Record<string, any> = {
        lightClass,
        name: params.name,
      };

      if (params.location) {
        payload.location = { x: params.location[0], y: params.location[1], z: params.location[2] };
      }

      if (params.rotation) {
        payload.rotation = { pitch: params.rotation[0], yaw: params.rotation[1], roll: params.rotation[2] };
      }

      if (params.properties) {
        payload.properties = params.properties;
      }

      const response = await this.automationBridge.sendAutomationRequest('spawn_light', payload, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        throw new Error(response.error || response.message || 'Failed to spawn light');
      }

      return response;
    } catch (error) {
      throw new Error(
        `Failed to spawn ${lightClass}: ${error instanceof Error ? error.message : String(error)}`
      );
    }
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
    const name = this.normalizeName(params.name);
    
    // Validate numeric parameters
    if (params.intensity !== undefined) {
      if (typeof params.intensity !== 'number' || !isFinite(params.intensity)) {
        throw new Error(`Invalid intensity value: ${params.intensity}`);
      }
      if (params.intensity < 0) {
        throw new Error('Invalid intensity: must be non-negative');
      }
    }
    
    if (params.temperature !== undefined) {
      if (typeof params.temperature !== 'number' || !isFinite(params.temperature)) {
        throw new Error(`Invalid temperature value: ${params.temperature}`);
      }
    }
    
    // Validate arrays
    if (params.color !== undefined) {
      if (!Array.isArray(params.color) || params.color.length !== 3) {
        throw new Error('Invalid color: must be an array [r,g,b]');
      }
      for (const c of params.color) {
        if (typeof c !== 'number' || !isFinite(c)) {
          throw new Error('Invalid color component: must be finite numbers');
        }
      }
    }
    
    if (params.rotation !== undefined) {
      if (!Array.isArray(params.rotation) || params.rotation.length !== 3) {
        throw new Error('Invalid rotation: must be an array [pitch,yaw,roll]');
      }
      for (const r of params.rotation) {
        if (typeof r !== 'number' || !isFinite(r)) {
          throw new Error('Invalid rotation component: must be finite numbers');
        }
      }
    }
    
    const rot = params.rotation || [0, 0, 0];
    
    // Build properties for the light
    const properties: Record<string, any> = {};
    if (params.intensity !== undefined) {
      properties.intensity = params.intensity;
    }
    if (params.color) {
      properties.color = { r: params.color[0], g: params.color[1], b: params.color[2], a: 1.0 };
    }
    if (params.castShadows !== undefined) {
      properties.castShadows = params.castShadows;
    }
    if (params.temperature !== undefined) {
      properties.temperature = params.temperature;
    }

    await this.spawnLightViaAutomation('DirectionalLight', {
      name,
      location: [0, 0, 500],
      rotation: rot,
      properties
    });

    return { success: true, message: `Directional light '${name}' spawned` };
  }

  // Create point light
  async createPointLight(params: {
    name: string;
    location?: [number, number, number];
    intensity?: number;
    radius?: number;
    color?: [number, number, number];
    falloffExponent?: number;
    castShadows?: boolean;
  }) {
    const name = this.normalizeName(params.name);
    
    // Validate location array
    if (params.location !== undefined) {
      if (!Array.isArray(params.location) || params.location.length !== 3) {
        throw new Error('Invalid location: must be an array [x,y,z]');
      }
      for (const l of params.location) {
        if (typeof l !== 'number' || !isFinite(l)) {
          throw new Error('Invalid location component: must be finite numbers');
        }
      }
    }
    
    // Default location if not provided
    const location = params.location || [0, 0, 0];
    
    // Validate numeric parameters
    if (params.intensity !== undefined) {
      if (typeof params.intensity !== 'number' || !isFinite(params.intensity)) {
        throw new Error(`Invalid intensity value: ${params.intensity}`);
      }
      if (params.intensity < 0) {
        throw new Error('Invalid intensity: must be non-negative');
      }
    }
    if (params.radius !== undefined) {
      if (typeof params.radius !== 'number' || !isFinite(params.radius)) {
        throw new Error(`Invalid radius value: ${params.radius}`);
      }
      if (params.radius < 0) {
        throw new Error('Invalid radius: must be non-negative');
      }
    }
    if (params.falloffExponent !== undefined) {
      if (typeof params.falloffExponent !== 'number' || !isFinite(params.falloffExponent)) {
        throw new Error(`Invalid falloffExponent value: ${params.falloffExponent}`);
      }
    }
    
    // Validate color array
    if (params.color !== undefined) {
      if (!Array.isArray(params.color) || params.color.length !== 3) {
        throw new Error('Invalid color: must be an array [r,g,b]');
      }
      for (const c of params.color) {
        if (typeof c !== 'number' || !isFinite(c)) {
          throw new Error('Invalid color component: must be finite numbers');
        }
      }
    }
    
    // Build properties for the light
    const properties: Record<string, any> = {};
    if (params.intensity !== undefined) {
      properties.intensity = params.intensity;
    }
    if (params.radius !== undefined) {
      properties.attenuationRadius = params.radius;
    }
    if (params.color) {
      properties.color = { r: params.color[0], g: params.color[1], b: params.color[2], a: 1.0 };
    }
    if (params.castShadows !== undefined) {
      properties.castShadows = params.castShadows;
    }
    if (params.falloffExponent !== undefined) {
      properties.lightFalloffExponent = params.falloffExponent;
    }

    await this.spawnLightViaAutomation('PointLight', {
      name,
      location,
      properties
    });

    return { success: true, message: `Point light '${name}' spawned at ${location.join(', ')}` };
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
    const name = this.normalizeName(params.name);
    
    // Validate required location and rotation arrays
    if (!params.location || !Array.isArray(params.location) || params.location.length !== 3) {
      throw new Error('Invalid location: must be an array [x,y,z]');
    }
    for (const l of params.location) {
      if (typeof l !== 'number' || !isFinite(l)) {
        throw new Error('Invalid location component: must be finite numbers');
      }
    }
    
    if (!params.rotation || !Array.isArray(params.rotation) || params.rotation.length !== 3) {
      throw new Error('Invalid rotation: must be an array [pitch,yaw,roll]');
    }
    for (const r of params.rotation) {
      if (typeof r !== 'number' || !isFinite(r)) {
        throw new Error('Invalid rotation component: must be finite numbers');
      }
    }
    
    // Validate optional numeric parameters
    if (params.intensity !== undefined) {
      if (typeof params.intensity !== 'number' || !isFinite(params.intensity)) {
        throw new Error(`Invalid intensity value: ${params.intensity}`);
      }
      if (params.intensity < 0) {
        throw new Error('Invalid intensity: must be non-negative');
      }
    }
    
    if (params.innerCone !== undefined) {
      if (typeof params.innerCone !== 'number' || !isFinite(params.innerCone)) {
        throw new Error(`Invalid innerCone value: ${params.innerCone}`);
      }
      if (params.innerCone < 0 || params.innerCone > 180) {
        throw new Error('Invalid innerCone: must be between 0 and 180 degrees');
      }
    }
    
    if (params.outerCone !== undefined) {
      if (typeof params.outerCone !== 'number' || !isFinite(params.outerCone)) {
        throw new Error(`Invalid outerCone value: ${params.outerCone}`);
      }
      if (params.outerCone < 0 || params.outerCone > 180) {
        throw new Error('Invalid outerCone: must be between 0 and 180 degrees');
      }
    }
    
    if (params.radius !== undefined) {
      if (typeof params.radius !== 'number' || !isFinite(params.radius)) {
        throw new Error(`Invalid radius value: ${params.radius}`);
      }
      if (params.radius < 0) {
        throw new Error('Invalid radius: must be non-negative');
      }
    }
    
    // Validate color array
    if (params.color !== undefined) {
      if (!Array.isArray(params.color) || params.color.length !== 3) {
        throw new Error('Invalid color: must be an array [r,g,b]');
      }
      for (const c of params.color) {
        if (typeof c !== 'number' || !isFinite(c)) {
          throw new Error('Invalid color component: must be finite numbers');
        }
      }
    }
    // Build properties for the light
    const properties: Record<string, any> = {};
    if (params.intensity !== undefined) {
      properties.intensity = params.intensity;
    }
    if (params.innerCone !== undefined) {
      properties.innerConeAngle = params.innerCone;
    }
    if (params.outerCone !== undefined) {
      properties.outerConeAngle = params.outerCone;
    }
    if (params.radius !== undefined) {
      properties.attenuationRadius = params.radius;
    }
    if (params.color) {
      properties.color = { r: params.color[0], g: params.color[1], b: params.color[2], a: 1.0 };
    }
    if (params.castShadows !== undefined) {
      properties.castShadows = params.castShadows;
    }

    await this.spawnLightViaAutomation('SpotLight', {
      name,
      location: params.location,
      rotation: params.rotation,
      properties
    });

    return { success: true, message: `Spot light '${name}' spawned at ${params.location.join(', ')}` };
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
    
    const name = this.normalizeName(params.name);

    // Validate required location and rotation arrays
    if (!params.location || !Array.isArray(params.location) || params.location.length !== 3) {
      throw new Error('Invalid location: must be an array [x,y,z]');
    }
    for (const l of params.location) {
      if (typeof l !== 'number' || !isFinite(l)) {
        throw new Error('Invalid location component: must be finite numbers');
      }
    }
    
    if (!params.rotation || !Array.isArray(params.rotation) || params.rotation.length !== 3) {
      throw new Error('Invalid rotation: must be an array [pitch,yaw,roll]');
    }
    for (const r of params.rotation) {
      if (typeof r !== 'number' || !isFinite(r)) {
        throw new Error('Invalid rotation component: must be finite numbers');
      }
    }
    
    // Validate optional numeric parameters
    if (params.width !== undefined) {
      if (typeof params.width !== 'number' || !isFinite(params.width)) {
        throw new Error(`Invalid width value: ${params.width}`);
      }
      if (params.width <= 0) {
        throw new Error('Invalid width: must be positive');
      }
    }
    
    if (params.height !== undefined) {
      if (typeof params.height !== 'number' || !isFinite(params.height)) {
        throw new Error(`Invalid height value: ${params.height}`);
      }
      if (params.height <= 0) {
        throw new Error('Invalid height: must be positive');
      }
    }
    
    if (params.intensity !== undefined) {
      if (typeof params.intensity !== 'number' || !isFinite(params.intensity)) {
        throw new Error(`Invalid intensity value: ${params.intensity}`);
      }
      if (params.intensity < 0) {
        throw new Error('Invalid intensity: must be non-negative');
      }
    }
    
    // Validate color array
    if (params.color !== undefined) {
      if (!Array.isArray(params.color) || params.color.length !== 3) {
        throw new Error('Invalid color: must be an array [r,g,b]');
      }
      for (const c of params.color) {
        if (typeof c !== 'number' || !isFinite(c)) {
          throw new Error('Invalid color component: must be finite numbers');
        }
      }
    }
    // Build properties for the light
    const properties: Record<string, any> = {};
    if (params.intensity !== undefined) {
      properties.intensity = params.intensity;
    }
    if (params.color) {
      properties.color = { r: params.color[0], g: params.color[1], b: params.color[2], a: 1.0 };
    }
    if (params.width !== undefined) {
      properties.sourceWidth = params.width;
    }
    if (params.height !== undefined) {
      properties.sourceHeight = params.height;
    }

    await this.spawnLightViaAutomation('RectLight', {
      name,
      location: params.location,
      rotation: params.rotation,
      properties
    });

    return { success: true, message: `Rect light '${name}' spawned at ${params.location.join(', ')}` };
  }

  /**
   * Create dynamic light (plugin-first when available)
   */
  async createDynamicLight(params: {
    name?: string;
    lightType?: 'Point' | 'Spot' | 'Directional' | 'Rect' | string;
    location?: [number, number, number] | { x: number; y: number; z: number };
    rotation?: [number, number, number];
    intensity?: number;
    color?: [number, number, number, number] | { r: number; g: number; b: number; a?: number };
    pulse?: { enabled?: boolean; frequency?: number };
  }) {
    try {
      const name = typeof params.name === 'string' && params.name.trim().length > 0 ? params.name.trim() : `DynamicLight_${Date.now() % 10000}`;
      const lightTypeRaw = typeof params.lightType === 'string' && params.lightType.trim().length > 0 ? params.lightType.trim() : 'Point';
      const location = Array.isArray(params.location) ? { x: params.location[0], y: params.location[1], z: params.location[2] } : (params.location || { x: 0, y: 0, z: 100 });

      // Try plugin-first transport
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp: any = await this.automationBridge.sendAutomationRequest('create_dynamic_light', {
            lightName: name,
            lightType: lightTypeRaw,
            location,
            rotation: params.rotation,
            intensity: params.intensity,
            color: params.color,
            pulse: params.pulse
          });
          if (resp && resp.success !== false) {
            return { success: true, message: resp.message || `Dynamic light ${name} created`, actor: resp.actor || resp.result?.actor } as any;
          }
          const errTxt = String(resp?.error ?? resp?.message ?? '');
          if (!(errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION'))) {
            return { success: false, error: resp?.error ?? resp?.message ?? 'CREATE_DYNAMIC_LIGHT_FAILED' } as any;
          }
        } catch (_e) {
          // fall back to Python/bridge implementation below
        }
      }

      // Fallback to specific light creation methods
      const typeNorm = (lightTypeRaw || 'Point').toLowerCase();
      switch (typeNorm) {
        case 'directional': case 'directionallight':
          return await this.createDirectionalLight({ name, intensity: params.intensity, color: Array.isArray(params.color) ? [params.color[0], params.color[1], params.color[2]] as any : (params.color ? [params.color.r, params.color.g, params.color.b] : undefined), rotation: params.rotation as any });
        case 'spot': case 'spotlight':
          return await this.createSpotLight({ name, location: (location as any) as [number, number, number], rotation: params.rotation as any, intensity: params.intensity, innerCone: undefined, outerCone: undefined, color: Array.isArray(params.color) ? params.color as any : (params.color ? [params.color.r, params.color.g, params.color.b] : undefined) });
        case 'rect': case 'rectlight':
          return await this.createRectLight({ name, location: (location as any) as [number, number, number], rotation: params.rotation as any, width: undefined, height: undefined, intensity: params.intensity, color: Array.isArray(params.color) ? params.color as any : (params.color ? [params.color.r, params.color.g, params.color.b] : undefined) });
        case 'point': default:
          return await this.createPointLight({ name, location: (location as any) as [number, number, number], intensity: params.intensity, radius: undefined, color: Array.isArray(params.color) ? params.color as any : (params.color ? [params.color.r, params.color.g, params.color.b] : undefined), castShadows: undefined });
      }

    } catch (err) {
      return { success: false, error: `Failed to create dynamic light: ${err}` };
    }
  }

  // Create sky light
  async createSkyLight(params: {
    name: string;
    sourceType?: 'CapturedScene' | 'SpecifiedCubemap';
    cubemapPath?: string;
    intensity?: number;
    recapture?: boolean;
  }) {
    const name = this.normalizeName(params.name);
    if (params.sourceType === 'SpecifiedCubemap' && (!params.cubemapPath || params.cubemapPath.trim().length === 0)) {
      const message = 'cubemapPath is required when sourceType is SpecifiedCubemap';
      return { success: false, error: message, message };
    }

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Sky light creation requires plugin support.');
    }

    try {
      const payload: Record<string, any> = {
        name,
        sourceType: params.sourceType || 'CapturedScene',
      };

      if (params.cubemapPath) {
        payload.cubemapPath = params.cubemapPath;
      }
      if (params.intensity !== undefined) {
        payload.intensity = params.intensity;
      }
      if (params.recapture) {
        payload.recapture = params.recapture;
      }

      const response = await this.automationBridge.sendAutomationRequest('spawn_sky_light', payload, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create sky light'
        };
      }

      return {
        success: true,
        message: response.message || 'Sky light created',
        warnings: (response.result as any)?.warnings
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to create sky light: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  // Remove duplicate SkyLights and keep only one (named target label)
  async ensureSingleSkyLight(params?: { name?: string; recapture?: boolean }) {
    const defaultName = 'MCP_Test_Sky';
    const name = this.normalizeName(params?.name, defaultName);
    const recapture = !!params?.recapture;

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Sky light management requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('ensure_single_sky_light', {
        name,
        recapture
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to ensure single sky light'
        };
      }

      const result = response.result as any;
      return {
        success: true,
        removed: result?.removed || 0,
        message: response.message || `Ensured single SkyLight (removed ${result?.removed || 0})`
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to ensure single sky light: ${error instanceof Error ? error.message : String(error)}`
      };
    }
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
      await this.bridge.executeConsoleCommand(cmd);
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
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Shadow settings configured' };
  }

  // Build lighting
  async buildLighting(params: {
    quality?: 'Preview' | 'Medium' | 'High' | 'Production';
    buildOnlySelected?: boolean;
    buildReflectionCaptures?: boolean;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Lighting build requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('build_lighting', {
        quality: params.quality || 'High',
        buildOnlySelected: params.buildOnlySelected || false,
        buildReflectionCaptures: params.buildReflectionCaptures !== false
      }, {
        timeoutMs: 300000 // 5 minutes for lighting builds
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to build lighting'
        };
      }

      return {
        success: true,
        message: response.message || 'Lighting build started'
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to build lighting: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  // Create a new level with proper lighting settings
  async createLightingEnabledLevel(params?: {
    levelName?: string;
    copyActors?: boolean;
    useTemplate?: boolean;
  }) {
    const levelName = params?.levelName || 'LightingEnabledLevel';

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Level creation requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_lighting_enabled_level', {
        levelName,
        copyActors: params?.copyActors === true,
        useTemplate: params?.useTemplate === true
      }, {
        timeoutMs: 120000 // 2 minutes for level creation
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create level'
        };
      }

      const result = response.result as any;
      return {
        success: true,
        message: response.message || `Created new level "${levelName}" with lighting enabled`,
        path: result?.path
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to create lighting-enabled level: ${error}`
      };
    }
  }

  // Create lightmass importance volume
  async createLightmassVolume(params: {
    name: string;
    location: [number, number, number];
    size: [number, number, number];
  }) {
    const name = this.normalizeName(params.name);

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Lightmass volume creation requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_lightmass_volume', {
        name,
        location: { x: params.location[0], y: params.location[1], z: params.location[2] },
        size: { x: params.size[0], y: params.size[1], z: params.size[2] }
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create lightmass volume'
        };
      }

      return {
        success: true,
        message: `LightmassImportanceVolume '${name}' created`
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to create lightmass volume: ${error}`
      };
    }
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
      await this.bridge.executeConsoleCommand(cmd);
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
      await this.bridge.executeConsoleCommand(cmd);
    }
    
    return { success: true, message: 'Ambient occlusion configured' };
  }

  // Setup volumetric fog
  async setupVolumetricFog(params: {
    enabled: boolean;
    density?: number;
    scatteringIntensity?: number;
    fogHeight?: number; // interpreted as Z location shift for ExponentialHeightFog actor
  }) {
    // Enable/disable global volumetric fog via CVar
    await this.bridge.executeConsoleCommand(`r.VolumetricFog ${params.enabled ? 1 : 0}`);

    if (!this.automationBridge) {
      return {
        success: true,
        message: 'Volumetric fog console setting applied (plugin required for fog actor adjustment)'
      };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('setup_volumetric_fog', {
        enabled: params.enabled,
        density: params.density,
        scatteringIntensity: params.scatteringIntensity,
        fogHeight: params.fogHeight
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to configure volumetric fog'
        };
      }

      return {
        success: true,
        message: 'Volumetric fog configured'
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to setup volumetric fog: ${error}`
      };
    }
  }
}
