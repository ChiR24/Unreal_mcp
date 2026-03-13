/**
 * Lighting tool handlers - migrated from class-based LightingTools
 * All operations route through executeAutomationRequest to the C++ bridge
 */
import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { LightingArgs } from '../../types/handler-types.js';
import { executeAutomationRequest, normalizeLocation } from './common-handlers.js';
import { toNumber, toBoolean, toString, toColor3, toLocationObj, toRotationObj, normalizeName } from '../../utils/type-coercion.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { TOOL_ACTIONS } from '../../utils/action-constants.js';


// Valid light types supported by UE - accepts multiple formats
const VALID_LIGHT_TYPES = [
  'point', 'directional', 'spot', 'rect', 'sky',           // lowercase short names
  'pointlight', 'directionallight', 'spotlight', 'rectlight', 'skylight'  // lowercase class names
];

// Alias for lighting-specific name normalization
const normalizeLightName = (value: unknown, defaultName?: string): string => normalizeName(value, defaultName, 'Light');

/**
 * Spawn a light via the automation bridge
 */
async function spawnLight(
  tools: ITools,
  lightClass: string,
  params: {
    name: string;
    location?: unknown;
    rotation?: unknown;
    properties?: Record<string, unknown>;
  }
): Promise<Record<string, unknown>> {
  const payload: Record<string, unknown> = {
    lightClass,
    name: params.name,
  };

  if (params.location) {
    const locObj = toLocationObj(params.location);
    if (locObj) payload.location = locObj;
  }

  if (params.rotation) {
    const rotObj = toRotationObj(params.rotation);
    if (rotObj) payload.rotation = rotObj;
  }

  if (params.properties) {
    payload.properties = params.properties;
  }

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SPAWN_LIGHT, payload, 'Automation bridge not available for light spawning')) as Record<string, unknown>;
}

/**
 * Create directional light
 */
async function createDirectionalLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const intensity = toNumber(args.intensity);
  const color = toColor3(args.color);
  const castShadows = toBoolean(args.castShadows);
  const temperature = toNumber(args.temperature);
  const useAsAtmosphereSunLight = toBoolean(args.useAsAtmosphereSunLight);

  // Validate numeric parameters
  if (intensity !== undefined && intensity < 0) {
    return { success: false, isError: true, error: 'Invalid intensity: must be non-negative' };
  }

  // Build properties for the light
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.intensity = intensity;
  if (color) properties.color = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  if (castShadows !== undefined) properties.castShadows = castShadows;
  if (temperature !== undefined) properties.temperature = temperature;
  if (useAsAtmosphereSunLight !== undefined) properties.useAsAtmosphereSunLight = useAsAtmosphereSunLight;

  const result = await spawnLight(tools, 'DirectionalLight', {
    name,
    location: [0, 0, 500],
    rotation: args.rotation || [0, 0, 0],
    properties
  });

  return cleanObject(result);
}

/**
 * Create point light
 */
async function createPointLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = normalizeLocation(args.location) || [0, 0, 0];
  const intensity = toNumber(args.intensity);
  const radius = toNumber(args.radius);
  const color = toColor3(args.color);
  const castShadows = toBoolean(args.castShadows);
  const falloffExponent = toNumber(args.falloffExponent);

  // Validate numeric parameters
  if (intensity !== undefined && intensity < 0) {
    return { success: false, isError: true, error: 'Invalid intensity: must be non-negative' };
  }
  if (radius !== undefined && radius < 0) {
    return { success: false, isError: true, error: 'Invalid radius: must be non-negative' };
  }

  // Build properties for the light
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.intensity = intensity;
  if (radius !== undefined) properties.attenuationRadius = radius;
  if (color) properties.color = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  if (castShadows !== undefined) properties.castShadows = castShadows;
  if (falloffExponent !== undefined) properties.lightFalloffExponent = falloffExponent;

  const result = await spawnLight(tools, 'PointLight', {
    name,
    location,
    rotation: args.rotation,
    properties
  });

  return cleanObject(result);
}

/**
 * Create spot light
 */
async function createSpotLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = normalizeLocation(args.location);
  const intensity = toNumber(args.intensity);
  const innerCone = toNumber(args.innerCone);
  const outerCone = toNumber(args.outerCone);
  const radius = toNumber(args.radius);
  const color = toColor3(args.color);
  const castShadows = toBoolean(args.castShadows);

  // Validate required location
  if (!location) {
    return { success: false, isError: true, error: 'Location is required for spot light' };
  }

  // Validate numeric parameters
  if (intensity !== undefined && intensity < 0) {
    return { success: false, isError: true, error: 'Invalid intensity: must be non-negative' };
  }
  if (innerCone !== undefined && (innerCone < 0 || innerCone > 180)) {
    return { success: false, isError: true, error: 'Invalid innerCone: must be between 0 and 180 degrees' };
  }
  if (outerCone !== undefined && (outerCone < 0 || outerCone > 180)) {
    return { success: false, isError: true, error: 'Invalid outerCone: must be between 0 and 180 degrees' };
  }
  if (radius !== undefined && radius < 0) {
    return { success: false, isError: true, error: 'Invalid radius: must be non-negative' };
  }

  // Build properties for the light
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.intensity = intensity;
  if (innerCone !== undefined) properties.innerConeAngle = innerCone;
  if (outerCone !== undefined) properties.outerConeAngle = outerCone;
  if (radius !== undefined) properties.attenuationRadius = radius;
  if (color) properties.color = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  if (castShadows !== undefined) properties.castShadows = castShadows;

  const result = await spawnLight(tools, 'SpotLight', {
    name,
    location,
    rotation: args.rotation || [0, 0, 0],
    properties
  });

  return cleanObject(result);
}

/**
 * Create rect light
 */
async function createRectLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = normalizeLocation(args.location);
  const intensity = toNumber(args.intensity);
  const width = toNumber(args.width);
  const height = toNumber(args.height);
  const color = toColor3(args.color);
  // Used in properties below

  // Validate required location
  if (!location) {
    return { success: false, isError: true, error: 'Location is required for rect light' };
  }

  // Validate numeric parameters
  if (intensity !== undefined && intensity < 0) {
    return { success: false, isError: true, error: 'Invalid intensity: must be non-negative' };
  }
  if (width !== undefined && width <= 0) {
    return { success: false, isError: true, error: 'Invalid width: must be positive' };
  }
  if (height !== undefined && height <= 0) {
    return { success: false, isError: true, error: 'Invalid height: must be positive' };
  }

  // Build properties for the light
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.intensity = intensity;
  if (color) properties.color = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  if (width !== undefined) properties.sourceWidth = width;
  if (height !== undefined) properties.sourceHeight = height;

  const result = await spawnLight(tools, 'RectLight', {
    name,
    location,
    rotation: args.rotation || [0, 0, 0],
    properties
  });

  return cleanObject(result);
}

/**
 * Create dynamic light - routes to specific light type handlers
 */
async function createDynamicLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const lightTypeRaw = toString(args.lightType) || 'Point';
  const intensity = toNumber(args.intensity);
  const color = toColor3(args.color);
  const typeNorm = lightTypeRaw.toLowerCase();

  switch (typeNorm) {
    case 'directional':
    case 'directionallight':
      return createDirectionalLight(tools, { ...args, name, intensity, color });
    case 'spot':
    case 'spotlight':
      return createSpotLight(tools, { ...args, name, intensity, color });
    case 'rect':
    case 'rectlight':
      return createRectLight(tools, { ...args, name, intensity, color });
    case 'point':
    case 'pointlight':
    default:
      return createPointLight(tools, { ...args, name, intensity, color });
  }
}

/**
 * Create sky light
 */
async function createSkyLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const sourceType = toString(args.sourceType) || 'CapturedScene';
  const cubemapPath = toString(args.cubemapPath);
  const intensity = toNumber(args.intensity);
  const recapture = toBoolean(args.recapture);
  const realTimeCapture = toBoolean(args.realTimeCapture);
  const castShadows = toBoolean(args.castShadows);
  const color = toColor3(args.color);

  // Validate cubemap requirement
  if (sourceType === 'SpecifiedCubemap' && !cubemapPath) {
    return { success: false, isError: true, error: 'cubemapPath is required when sourceType is SpecifiedCubemap' };
  }

  const payload: Record<string, unknown> = {
    name,
    sourceType,
    location: args.location,
    rotation: args.rotation
  };

  if (cubemapPath) payload.cubemapPath = cubemapPath;
  if (intensity !== undefined) payload.intensity = intensity;
  if (recapture !== undefined) payload.recapture = recapture;

  // Build properties
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.Intensity = intensity;
  if (castShadows !== undefined) properties.CastShadows = castShadows;
  if (realTimeCapture !== undefined) properties.RealTimeCapture = realTimeCapture;
  if (color) properties.LightColor = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  
  if (Object.keys(properties).length > 0) payload.properties = properties;

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SPAWN_SKY_LIGHT, payload, 'Automation bridge not available for sky light creation')) as Record<string, unknown>;
}

/**
 * Ensure single sky light
 */
async function ensureSingleSkyLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const defaultName = 'MCP_Test_Sky';
  const name = normalizeLightName(args.name, defaultName);
  const recapture = args.recapture !== false;

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.ENSURE_SINGLE_SKY_LIGHT, { name, recapture }, 'Automation bridge not available for sky light management')) as Record<string, unknown>;
}

/**
 * Setup global illumination
 */
async function setupGlobalIllumination(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  if (!args.method) {
    return {
      success: false,
      isError: true,
      error: 'MISSING_REQUIRED_PARAM',
      message: "'method' parameter is required for setup_global_illumination. Must be one of: LumenGI, ScreenSpace, None, RayTraced, Lightmass"
    };
  }

  // Normalize method
  let normalizedMethod: string | undefined;
  const methodLower = String(args.method).toLowerCase();
  
  if (methodLower === 'lumen' || methodLower === 'lumengi') {
    normalizedMethod = 'LumenGI';
  } else if (methodLower === 'screenspace' || methodLower === 'ssgi') {
    normalizedMethod = 'ScreenSpace';
  } else if (methodLower === 'none') {
    normalizedMethod = 'None';
  } else if (methodLower === 'raytraced') {
    normalizedMethod = 'RayTraced';
  } else if (methodLower === 'lightmass') {
    normalizedMethod = 'Lightmass';
  } else {
    return {
      success: false,
      isError: true,
      error: 'INVALID_GI_METHOD',
      message: `Invalid GI method: '${args.method}'. Must be one of: LumenGI, ScreenSpace, None, RayTraced, Lightmass`
    };
  }

  const payload = {
    method: normalizedMethod,
    quality: toString(args.quality),
    indirectLightingIntensity: toNumber(args.indirectLightingIntensity),
    bounces: toNumber(args.bounces)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SETUP_GLOBAL_ILLUMINATION, payload);

  // If bridge fails with connection error, fall back to console commands
  const resultObj = result as Record<string, unknown>;
  if (resultObj.success === false && typeof resultObj.error === 'string' && 
      (resultObj.error.includes('not available') || resultObj.error.includes('Connection'))) {
    // Console command fallback
    const commands: string[] = [];
    
    switch (normalizedMethod) {
      case 'Lightmass': commands.push('r.DynamicGlobalIlluminationMethod 0'); break;
      case 'LumenGI': commands.push('r.DynamicGlobalIlluminationMethod 1'); break;
      case 'ScreenSpace': commands.push('r.DynamicGlobalIlluminationMethod 2'); break;
      case 'None': commands.push('r.DynamicGlobalIlluminationMethod 3'); break;
    }

    if (args.quality) {
      const qualityMap: Record<string, number> = { 'Low': 0, 'Medium': 1, 'High': 2, 'Epic': 3 };
      commands.push(`r.Lumen.Quality ${qualityMap[args.quality] ?? 1}`);
    }

    if (args.indirectLightingIntensity !== undefined) {
      commands.push(`r.IndirectLightingIntensity ${args.indirectLightingIntensity}`);
    }

    if (args.bounces !== undefined) {
      commands.push(`r.Lumen.MaxReflectionBounces ${args.bounces}`);
    }

    for (const cmd of commands) {
      await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
    }

    return { success: true, message: 'Global illumination configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

/**
 * Configure shadows
 */
async function configureShadows(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const payload = {
    shadowQuality: toString(args.shadowQuality),
    cascadedShadows: toBoolean(args.cascadedShadows),
    shadowDistance: toNumber(args.shadowDistance),
    contactShadows: toBoolean(args.contactShadows),
    rayTracedShadows: toBoolean(args.rayTracedShadows),
    virtualShadowMaps: toBoolean(args.rayTracedShadows)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.CONFIGURE_SHADOWS, payload);

  // Fallback to console commands if bridge fails
  const resultObj = result as Record<string, unknown>;
  if (resultObj.success === false && typeof resultObj.error === 'string' &&
      (resultObj.error.includes('not available') || resultObj.error.includes('Connection'))) {
    const commands: string[] = [];

    if (args.shadowQuality) {
      const qualityMap: Record<string, number> = { 'Low': 0, 'Medium': 1, 'High': 2, 'Epic': 3 };
      commands.push(`r.ShadowQuality ${qualityMap[args.shadowQuality] ?? 1}`);
    }

    if (args.cascadedShadows !== undefined) {
      commands.push(`r.Shadow.CSM.MaxCascades ${args.cascadedShadows ? 4 : 1}`);
    }

    if (args.shadowDistance !== undefined) {
      commands.push(`r.Shadow.DistanceScale ${args.shadowDistance}`);
    }

    if (args.contactShadows !== undefined) {
      commands.push(`r.ContactShadows ${args.contactShadows ? 1 : 0}`);
    }

    if (args.rayTracedShadows !== undefined) {
      commands.push(`r.RayTracing.Shadows ${args.rayTracedShadows ? 1 : 0}`);
    }

    for (const cmd of commands) {
      await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
    }

    return { success: true, message: 'Shadow settings configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

/**
 * Build lighting
 */
async function buildLighting(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const payload = {
    quality: toString(args.quality) || 'High',
    buildOnlySelected: toBoolean(args.buildOnlySelected) || false,
    buildReflectionCaptures: toBoolean(args.buildReflectionCaptures) !== false,
    levelPath: toString(args.levelPath)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.BAKE_LIGHTMAP, payload, 'Automation bridge not available for lighting build')) as Record<string, unknown>;
}

/**
 * Create lighting enabled level
 */
async function createLightingEnabledLevel(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const levelName = toString(args.levelName) || 'LightingEnabledLevel';
  let path = toString(args.path);
  if (!path) path = `/Game/Maps/${levelName}`;

  const payload = {
    path,
    levelName,
    copyActors: toBoolean(args.copyActors) === true,
    useTemplate: toBoolean(args.useTemplate) === true
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_LIGHTING_ENABLED_LEVEL, payload, 'Automation bridge not available for level creation')) as Record<string, unknown>;
}

/**
 * Create lightmass volume
 */
async function createLightmassVolume(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = toLocationObj(args.location) || { x: 0, y: 0, z: 0 };
  const size = toLocationObj(args.size) || { x: 1000, y: 1000, z: 1000 };

  const payload = { name, location, size };
  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_LIGHTMASS_VOLUME, payload, 'Automation bridge not available for lightmass volume creation')) as Record<string, unknown>;
}

/**
 * Set exposure
 */
async function setExposure(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const payload = {
    method: toString(args.method),
    compensationValue: toNumber(args.compensationValue),
    minBrightness: toNumber(args.minBrightness),
    maxBrightness: toNumber(args.maxBrightness)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_EXPOSURE, payload);

  // Fallback to console commands
  const resultObj = result as Record<string, unknown>;
  if (resultObj.success === false && typeof resultObj.error === 'string' &&
      (resultObj.error.includes('not available') || resultObj.error.includes('Connection'))) {
    const commands: string[] = [];

    commands.push(`r.EyeAdaptation.ExposureMethod ${args.method === 'Manual' ? 0 : 1}`);

    if (args.compensationValue !== undefined) {
      commands.push(`r.EyeAdaptation.ExposureCompensation ${args.compensationValue}`);
    }
    if (args.minBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MinBrightness ${args.minBrightness}`);
    }
    if (args.maxBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MaxBrightness ${args.maxBrightness}`);
    }

    for (const cmd of commands) {
      await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
    }

    return { success: true, message: 'Exposure settings updated (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

/**
 * Set ambient occlusion
 */
async function setAmbientOcclusion(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const enabled = args.enabled !== false;
  const payload = {
    enabled,
    intensity: toNumber(args.intensity),
    radius: toNumber(args.radius),
    quality: toString(args.quality)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_AMBIENT_OCCLUSION, payload);

  // Fallback to console commands
  const resultObj = result as Record<string, unknown>;
  if (resultObj.success === false && typeof resultObj.error === 'string' &&
      (resultObj.error.includes('not available') || resultObj.error.includes('Connection'))) {
    const commands: string[] = [];

    commands.push(`r.AmbientOcclusion.Enabled ${enabled ? 1 : 0}`);

    if (args.intensity !== undefined) {
      commands.push(`r.AmbientOcclusion.Intensity ${args.intensity}`);
    }
    if (args.radius !== undefined) {
      commands.push(`r.AmbientOcclusion.Radius ${args.radius}`);
    }
    if (args.quality) {
      const qualityMap: Record<string, number> = { 'Low': 0, 'Medium': 1, 'High': 2 };
      commands.push(`r.AmbientOcclusion.Quality ${qualityMap[args.quality] ?? 1}`);
    }

    for (const cmd of commands) {
      await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: cmd });
    }

    return { success: true, message: 'Ambient occlusion configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

/**
 * Setup volumetric fog
 */
async function setupVolumetricFog(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const enabled = args.enabled !== false;

  // Enable/disable global volumetric fog via CVar
  await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: `r.VolumetricFog ${enabled ? 1 : 0}` });

  const payload = {
    enabled,
    density: toNumber(args.density),
    scatteringIntensity: toNumber(args.scatteringIntensity),
    fogHeight: toNumber(args.fogHeight)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SETUP_VOLUMETRIC_FOG, payload, 'Volumetric fog console setting applied (plugin required for fog actor adjustment)')) as Record<string, unknown>;
}

/**
 * List light types
 */
async function listLightTypes(tools: ITools): Promise<Record<string, unknown>> {
  return (await executeAutomationRequest(tools, TOOL_ACTIONS.LIST_LIGHT_TYPES, {}, 'Automation bridge not available for listing light types')) as Record<string, unknown>;
}

/**
 * Main handler for lighting tools
 */
export async function handleLightingTools(action: string, args: LightingArgs, tools: ITools): Promise<Record<string, unknown>> {
  switch (action) {
    case 'spawn_light':
    case 'create_light': {
      // Map generic create_light to specific types
      let lightType = args.lightType ? String(args.lightType).toLowerCase() : 'point';

      // Normalize class names to short names
      if (lightType.endsWith('light') && lightType !== 'light') {
        lightType = lightType.replace(/light$/, '');
      }

      // Validate light type
      if (!VALID_LIGHT_TYPES.includes(lightType) && !VALID_LIGHT_TYPES.includes(lightType + 'light')) {
        return {
          success: false,
          isError: true,
          error: 'INVALID_LIGHT_TYPE',
          message: `Invalid lightType: '${args.lightType}'. Must be one of: point, directional, spot, rect, sky`
        };
      }

      // Route to specific handler
      if (lightType === 'directional') {
        return createDirectionalLight(tools, args);
      } else if (lightType === 'spot') {
        return createSpotLight(tools, args);
      } else if (lightType === 'rect') {
        return createRectLight(tools, args);
      } else if (lightType === 'sky') {
        return createSkyLight(tools, args);
      } else {
        // Default to Point
        return createPointLight(tools, args);
      }
    }

    case 'create_dynamic_light':
      return createDynamicLight(tools, args);

    case 'spawn_sky_light':
    case 'create_sky_light':
      return cleanObject(await createSkyLight(tools, args));

    case 'ensure_single_sky_light':
      return cleanObject(await ensureSingleSkyLight(tools, args));

    case 'create_lightmass_volume':
      return cleanObject(await createLightmassVolume(tools, args));

    case 'setup_volumetric_fog':
      return cleanObject(await setupVolumetricFog(tools, args));

    case 'setup_global_illumination':
      return cleanObject(await setupGlobalIllumination(tools, args));

    case 'configure_shadows':
      return cleanObject(await configureShadows(tools, args));

    case 'set_exposure':
      return cleanObject(await setExposure(tools, args));

    case 'set_ambient_occlusion':
      return cleanObject(await setAmbientOcclusion(tools, args));

    case 'build_lighting':
      return cleanObject(await buildLighting(tools, args));

    case 'create_lighting_enabled_level':
      return cleanObject(await createLightingEnabledLevel(tools, args));

    case 'list_light_types':
      return cleanObject(await listLightTypes(tools));

    default:
      return ResponseFactory.error(`Unknown lighting action: ${action}`);
  }
}
