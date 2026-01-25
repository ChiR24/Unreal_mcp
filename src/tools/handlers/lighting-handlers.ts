import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { LightingArgs } from '../../types/handler-types.js';
import { normalizeLocation, executeAutomationRequest } from './common-handlers.js';

// Valid light types supported by UE
const VALID_LIGHT_TYPES = ['point', 'directional', 'spot', 'rect', 'sky'];

// Valid GI methods
const VALID_GI_METHODS = ['lumen', 'screenspace', 'none', 'raytraced', 'ssgi'];

// Helper to coerce unknown to number | undefined
const toNumber = (val: unknown): number | undefined => {
  if (val === undefined || val === null) return undefined;
  const n = Number(val);
  return isFinite(n) ? n : undefined;
};

// Helper to coerce unknown to boolean | undefined
const toBoolean = (val: unknown): boolean | undefined => {
  if (val === undefined || val === null) return undefined;
  return Boolean(val);
};

// Helper to coerce unknown to string | undefined
const toString = (val: unknown): string | undefined => {
  if (val === undefined || val === null) return undefined;
  return String(val);
};

// Helper to coerce unknown to [number, number, number] | undefined
const toColor3 = (val: unknown): [number, number, number] | undefined => {
  if (!Array.isArray(val) || val.length < 3) return undefined;
  return [Number(val[0]) || 0, Number(val[1]) || 0, Number(val[2]) || 0];
};

export async function handleLightingTools(action: string, args: LightingArgs, tools: ITools) {
  // Normalize location parameter to accept both {x,y,z} and [x,y,z] formats
  const normalizedLocation = normalizeLocation(args.location);

  switch (action) {
    case 'spawn_light':
    case 'create_light': {
      // Map generic create_light to specific types if provided
      const lightType = args.lightType ? String(args.lightType).toLowerCase() : 'point';
      
      // Validate light type
      if (!VALID_LIGHT_TYPES.includes(lightType)) {
        return {
          success: false,
          error: 'INVALID_LIGHT_TYPE',
          message: `Invalid lightType: '${args.lightType}'. Must be one of: ${VALID_LIGHT_TYPES.join(', ')}`,
          action: action
        };
      }
      const commonParams = {
        name: toString(args.name),
        location: normalizedLocation || args.location,
        rotation: args.rotation,
        intensity: toNumber(args.intensity),
        color: toColor3(args.color),
        castShadows: toBoolean(args.castShadows)
      };

      if (lightType === 'directional') {
        return cleanObject(await tools.lightingTools.createDirectionalLight({
          ...commonParams,
          temperature: args.temperature,
          useAsAtmosphereSunLight: args.useAsAtmosphereSunLight
        }));
      } else if (lightType === 'spot') {
        return cleanObject(await tools.lightingTools.createSpotLight({
          ...commonParams,
          location: normalizedLocation || [0, 0, 0],
          rotation: args.rotation || [0, 0, 0],
          innerCone: toNumber(args.innerCone),
          outerCone: toNumber(args.outerCone),
          radius: toNumber(args.radius)
        }));
      } else if (lightType === 'rect') {
        return cleanObject(await tools.lightingTools.createRectLight({
          ...commonParams,
          location: normalizedLocation || [0, 0, 0],
          rotation: args.rotation || [0, 0, 0],
          width: toNumber(args.width),
          height: toNumber(args.height)
        }));
      } else {
        // Default to Point
        return cleanObject(await tools.lightingTools.createPointLight({
          ...commonParams,
          radius: toNumber(args.radius),
          falloffExponent: toNumber(args.falloffExponent)
        }));
      }
    }
    case 'create_dynamic_light': {
      return cleanObject(await tools.lightingTools.createDynamicLight({
        name: toString(args.name),
        lightType: toString(args.lightType),
        location: args.location,
        rotation: args.rotation,
        intensity: toNumber(args.intensity),
        color: args.color,
        pulse: args.pulse
      }));
    }
    case 'spawn_sky_light':
    case 'create_sky_light': {
      return cleanObject(await tools.lightingTools.createSkyLight({
        name: toString(args.name),
        sourceType: toString(args.sourceType),
        cubemapPath: args.cubemapPath,
        intensity: toNumber(args.intensity),
        recapture: toBoolean(args.recapture)
      }));
    }
    case 'ensure_single_sky_light': {
      return cleanObject(await tools.lightingTools.ensureSingleSkyLight({
        name: toString(args.name),
        recapture: toBoolean(args.recapture)
      }));
    }
    case 'create_lightmass_volume': {
      return cleanObject(await tools.lightingTools.createLightmassVolume({
        name: toString(args.name),
        location: args.location,
        size: args.size
      }));
    }
    case 'setup_volumetric_fog': {
      return cleanObject(await tools.lightingTools.setupVolumetricFog({
        enabled: args.enabled !== false,
        density: toNumber(args.density),
        scatteringIntensity: toNumber(args.scatteringIntensity),
        fogHeight: toNumber(args.fogHeight)
      }));
    }
    case 'setup_global_illumination': {
      // Validate GI method if provided
      let methodLower: string | undefined;
      if (args.method) {
        methodLower = String(args.method).toLowerCase();
        // Map common variations to standard UE internal strings
        if (methodLower === 'lumengi') methodLower = 'lumen';
        if (methodLower === 'screen_space') methodLower = 'screenspace';
        
        if (!VALID_GI_METHODS.includes(methodLower)) {
          return {
            success: false,
            error: 'INVALID_GI_METHOD',
            message: `Invalid GI method: '${args.method}'. Must be one of: ${VALID_GI_METHODS.join(', ')}`,
            action: 'setup_global_illumination'
          };
        }
      }
      return cleanObject(await tools.lightingTools.setupGlobalIllumination({
        method: methodLower || toString(args.method),
        quality: toString(args.quality),
        indirectLightingIntensity: toNumber(args.indirectLightingIntensity),
        bounces: toNumber(args.bounces)
      }));
    }
    case 'configure_shadows': {
      return cleanObject(await tools.lightingTools.configureShadows({
        shadowQuality: toString(args.shadowQuality),
        cascadedShadows: toBoolean(args.cascadedShadows),
        shadowDistance: toNumber(args.shadowDistance),
        contactShadows: toBoolean(args.contactShadows),
        rayTracedShadows: toBoolean(args.rayTracedShadows)
      }));
    }
    case 'set_exposure': {
      return cleanObject(await tools.lightingTools.setExposure({
        method: toString(args.method),
        compensationValue: toNumber(args.compensationValue),
        minBrightness: toNumber(args.minBrightness),
        maxBrightness: toNumber(args.maxBrightness)
      }));
    }
    case 'set_ambient_occlusion': {
      return cleanObject(await tools.lightingTools.setAmbientOcclusion({
        enabled: args.enabled !== false,
        intensity: toNumber(args.intensity),
        radius: toNumber(args.radius),
        quality: toString(args.quality)
      }));
    }
    case 'build_lighting': {
      const quality = toString(args.quality) || 'Preview';
      return cleanObject(await tools.lightingTools.buildLighting({
        quality: quality,
        buildOnlySelected: toBoolean(args.buildOnlySelected),
        buildReflectionCaptures: toBoolean(args.buildReflectionCaptures)
      }));
    }
    case 'create_lighting_enabled_level': {
      return cleanObject(await tools.lightingTools.createLightingEnabledLevel({
        levelName: toString(args.levelName),
        copyActors: toBoolean(args.copyActors),
        useTemplate: toBoolean(args.useTemplate)
      }));
    }
    case 'list_light_types': {
      return cleanObject(await tools.lightingTools.listLightTypes());
    }
    case 'configure_lumen_gi': {
      return cleanObject(await tools.lightingTools.configureLumenGI({
        quality: toNumber(args.lumenQuality),
        detailTrace: toBoolean(args.lumenDetailTrace),
        updateSpeed: toNumber(args.lumenUpdateSpeed),
        finalGatherQuality: toNumber(args.lumenFinalGatherQuality)
      }));
    }
    case 'set_lumen_reflections': {
      return cleanObject(await tools.lightingTools.setLumenReflections({
        quality: toNumber(args.lumenReflectionQuality),
        detailTrace: toBoolean(args.lumenDetailTrace)
      }));
    }
    case 'tune_lumen_performance': {
      return cleanObject(await tools.lightingTools.tuneLumenPerformance({
        quality: toNumber(args.lumenQuality),
        updateSpeed: toNumber(args.lumenUpdateSpeed)
      }));
    }
    case 'create_lumen_volume': {
      return cleanObject(await tools.lightingTools.createLumenVolume({
        name: toString(args.name),
        location: normalizedLocation,
        size: args.size
      }));
    }
    case 'set_virtual_shadow_maps': {
      return cleanObject(await tools.lightingTools.setVirtualShadowMaps({
        enabled: args.enabled !== false,
        resolution: toNumber(args.virtualShadowMapResolution),
        quality: toNumber(args.virtualShadowMapQuality)
      }));
    }
    // Wave 5.11-5.20: MegaLights & Advanced Lighting Actions
    case 'configure_megalights_scene': {
      // 5.7+ feature: Enable MegaLights for scene
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'configure_megalights_scene',
        enabled: args.megalightsEnabled !== false,
        budget: toNumber(args.megalightsBudget),
        quality: toString(args.megalightsQuality)
      }));
    }
    case 'get_megalights_budget': {
      // 5.7+ feature: Get light budget stats
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'get_megalights_budget'
      }));
    }
    case 'optimize_lights_for_megalights': {
      // 5.7+ feature: Optimize lights for MegaLights system
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'optimize_lights_for_megalights',
        budget: toNumber(args.megalightsBudget)
      }));
    }
    case 'configure_gi_settings': {
      // Configure global illumination settings
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'configure_gi_settings',
        method: toString(args.giMethod),
        quality: toString(args.quality),
        bounces: toNumber(args.bounces),
        indirectLightingIntensity: toNumber(args.indirectLightingIntensity)
      }));
    }
    case 'bake_lighting_preview': {
      // Quick lighting preview bake
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'bake_lighting_preview',
        quality: toString(args.lightQuality),
        preview: args.previewBake !== false
      }));
    }
    case 'get_light_complexity': {
      // Get light complexity analysis
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'get_light_complexity'
      }));
    }
    case 'configure_volumetric_fog': {
      // Configure volumetric fog (more advanced than setup_volumetric_fog)
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'configure_volumetric_fog',
        enabled: args.enabled !== false,
        density: toNumber(args.density),
        scatteringIntensity: toNumber(args.scatteringIntensity),
        fogHeight: toNumber(args.fogHeight),
        inscatteringColor: toColor3(args.fogInscatteringColor),
        extinctionScale: toNumber(args.fogExtinctionScale),
        viewDistance: toNumber(args.fogViewDistance),
        startDistance: toNumber(args.fogStartDistance)
      }));
    }
    case 'create_light_batch': {
      // Create multiple lights at once
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'create_light_batch',
        lights: args.lights
      }));
    }
    case 'configure_shadow_settings': {
      // Configure shadow settings
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'configure_shadow_settings',
        shadowQuality: toString(args.shadowQuality),
        shadowBias: toNumber(args.shadowBias),
        shadowSlopeBias: toNumber(args.shadowSlopeBias),
        shadowResolution: toNumber(args.shadowResolution),
        cascadedShadows: toBoolean(args.cascadedShadows),
        dynamicShadowCascades: toNumber(args.dynamicShadowCascades),
        contactShadows: toBoolean(args.contactShadows),
        insetShadows: toBoolean(args.insetShadows),
        rayTracedShadows: toBoolean(args.rayTracedShadows)
      }));
    }
    case 'validate_lighting_setup': {
      // Validate lighting for issues
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: 'validate_lighting_setup',
        validatePerformance: toBoolean(args.validatePerformance),
        validateOverlap: toBoolean(args.validateOverlap),
        validateShadows: toBoolean(args.validateShadows)
      }));
    }

    // Forward Post-Process / Ray Tracing / Scene Capture actions to C++
    case 'configure_ray_traced_shadows':
    case 'configure_ray_traced_gi':
    case 'configure_ray_traced_reflections':
    case 'configure_ray_traced_ao':
    case 'configure_path_tracing':
    case 'create_scene_capture_2d':
    case 'create_scene_capture_cube':
    case 'capture_scene':
    case 'set_light_channel':
    case 'set_actor_light_channel':
    case 'configure_lightmass_settings':
    case 'build_lighting_quality':
    case 'configure_indirect_lighting_cache':
    case 'configure_volumetric_lightmap': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_lighting', {
        action: action,
        ...args
      }));
    }

    default:
      throw new Error(`Unknown lighting action: ${action}`);
  }
}

