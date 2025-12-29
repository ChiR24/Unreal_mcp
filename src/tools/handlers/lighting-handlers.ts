import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { LightingArgs } from '../../types/handler-types.js';
import { normalizeLocation } from './common-handlers.js';

// Valid light types supported by UE
const VALID_LIGHT_TYPES = ['point', 'directional', 'spot', 'rect', 'sky'];

// Valid GI methods
const VALID_GI_METHODS = ['lumen', 'screenspace', 'none', 'raytraced', 'ssgi'];

export async function handleLightingTools(action: string, args: LightingArgs, tools: ITools) {
  // Normalize location parameter to accept both {x,y,z} and [x,y,z] formats
  const normalizedLocation = normalizeLocation(args.location);

  switch (action) {
    case 'spawn_light':
    case 'create_light': {
      // Map generic create_light to specific types if provided
      const lightType = args.lightType ? args.lightType.toLowerCase() : 'point';
      
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
        name: args.name,
        location: normalizedLocation || args.location,
        rotation: args.rotation,
        intensity: args.intensity,
        color: args.color,
        castShadows: args.castShadows
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
          innerCone: args.innerCone,
          outerCone: args.outerCone,
          radius: args.radius
        }));
      } else if (lightType === 'rect') {
        return cleanObject(await tools.lightingTools.createRectLight({
          ...commonParams,
          location: normalizedLocation || [0, 0, 0],
          rotation: args.rotation || [0, 0, 0],
          width: args.width,
          height: args.height
        }));
      } else {
        // Default to Point
        return cleanObject(await tools.lightingTools.createPointLight({
          ...commonParams,
          radius: args.radius,
          falloffExponent: args.falloffExponent
        }));
      }
    }
    case 'create_dynamic_light': {
      return cleanObject(await tools.lightingTools.createDynamicLight({
        name: args.name,
        lightType: args.lightType,
        location: args.location,
        rotation: args.rotation,
        intensity: args.intensity,
        color: args.color,
        pulse: args.pulse
      }));
    }
    case 'spawn_sky_light':
    case 'create_sky_light': {
      return cleanObject(await tools.lightingTools.createSkyLight({
        name: args.name,
        sourceType: args.sourceType,
        cubemapPath: args.cubemapPath,
        intensity: args.intensity,
        recapture: args.recapture
      }));
    }
    case 'ensure_single_sky_light': {
      return cleanObject(await tools.lightingTools.ensureSingleSkyLight({
        name: args.name,
        recapture: args.recapture
      }));
    }
    case 'create_lightmass_volume': {
      return cleanObject(await tools.lightingTools.createLightmassVolume({
        name: args.name,
        location: args.location,
        size: args.size
      }));
    }
    case 'setup_volumetric_fog': {
      return cleanObject(await tools.lightingTools.setupVolumetricFog({
        enabled: args.enabled !== false,
        density: args.density,
        scatteringIntensity: args.scatteringIntensity,
        fogHeight: args.fogHeight
      }));
    }
    case 'setup_global_illumination': {
      // Validate GI method if provided
      let methodLower: string | undefined;
      if (args.method) {
        methodLower = String(args.method).toLowerCase();
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
        method: methodLower || args.method,
        quality: args.quality,
        indirectLightingIntensity: args.indirectLightingIntensity,
        bounces: args.bounces
      }));
    }
    case 'configure_shadows': {
      return cleanObject(await tools.lightingTools.configureShadows({
        shadowQuality: args.shadowQuality,
        cascadedShadows: args.cascadedShadows,
        shadowDistance: args.shadowDistance,
        contactShadows: args.contactShadows,
        rayTracedShadows: args.rayTracedShadows
      }));
    }
    case 'set_exposure': {
      return cleanObject(await tools.lightingTools.setExposure({
        method: args.method,
        compensationValue: args.compensationValue,
        minBrightness: args.minBrightness,
        maxBrightness: args.maxBrightness
      }));
    }
    case 'set_ambient_occlusion': {
      return cleanObject(await tools.lightingTools.setAmbientOcclusion({
        enabled: args.enabled !== false,
        intensity: args.intensity,
        radius: args.radius,
        quality: args.quality
      }));
    }
    case 'build_lighting': {
      return cleanObject(await tools.lightingTools.buildLighting({
        quality: args.quality,
        buildOnlySelected: args.buildOnlySelected,
        buildReflectionCaptures: args.buildReflectionCaptures
      }));
    }
    case 'create_lighting_enabled_level': {
      return cleanObject(await tools.lightingTools.createLightingEnabledLevel({
        levelName: args.levelName,
        copyActors: args.copyActors,
        useTemplate: args.useTemplate
      }));
    }
    case 'list_light_types': {
      return cleanObject(await tools.lightingTools.listLightTypes());
    }
    default:
      throw new Error(`Unknown lighting action: ${action}`);
  }
}
