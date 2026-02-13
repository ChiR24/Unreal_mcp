import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { LightingArgs } from '../../types/handler-types.js';
import { normalizeLocation } from './common-handlers.js';

// Valid light types supported by UE - accepts multiple formats
const VALID_LIGHT_TYPES = [
  'point', 'directional', 'spot', 'rect', 'sky',           // lowercase short names
  'pointlight', 'directionallight', 'spotlight', 'rectlight', 'skylight'  // lowercase class names
];

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
      let lightType = args.lightType ? String(args.lightType).toLowerCase() : 'point';
      
      // Normalize class names to short names (pointlight -> point, directionallight -> directional, etc.)
      if (lightType.endsWith('light') && lightType !== 'light') {
        lightType = lightType.replace(/light$/, '');
      }
      
      // Validate light type
      if (!VALID_LIGHT_TYPES.includes(lightType) && !VALID_LIGHT_TYPES.includes(lightType + 'light')) {
        return {
          success: false,
          error: 'INVALID_LIGHT_TYPE',
          message: `Invalid lightType: '${args.lightType}'. Must be one of: point, directional, spot, rect, sky (or class names: PointLight, DirectionalLight, etc.)`,
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
      // Normalize and validate GI method
      let normalizedMethod: string | undefined;
      if (args.method) {
        const methodLower = String(args.method).toLowerCase();
        // Map to C++ expected values
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
            error: 'INVALID_GI_METHOD',
            message: `Invalid GI method: '${args.method}'. Must be one of: LumenGI, ScreenSpace, None, RayTraced, Lightmass`,
            action: 'setup_global_illumination'
          };
        }
      }
      return cleanObject(await tools.lightingTools.setupGlobalIllumination({
        method: normalizedMethod || toString(args.method),
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
      return cleanObject(await tools.lightingTools.buildLighting({
        quality: toString(args.quality),
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
    default:
      throw new Error(`Unknown lighting action: ${action}`);
  }
}
