/**
 * Lighting family records (15 actions).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';
import { str } from '../shared/schema-props.js';

const F = 'lighting';
const WU = ['A light actor or lighting setting must be created or configured.'];

export const LIGHTING_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_light', action: 'create_light', family: F,
    summary: 'Create a light actor in the current level.',
    whenToUse: WU, whenNotToUse: ['An existing light should be reconfigured.', 'A static light is sufficient.'],
    inputProps: { lightType: P.lightType, lightClass: P.lightClass, name: P.name, location: P.location,
      rotation: P.rotation, intensity: P.intensity, color: P.color, properties: P.properties },
    requiredOneOf: ['lightClass', 'lightType'],
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_light', lightType: 'PointLight', name: 'Light_1' },
  }),
  buildRecord({
    id: 'build_environment.create_sky_light', action: 'create_sky_light', family: F,
    summary: 'Create a sky light actor, capturing the scene or lighting from a cubemap.',
    whenToUse: WU, whenNotToUse: ['A sky light already exists.'],
    inputProps: { name: P.name, location: P.location, rotation: P.rotation, intensity: P.intensity,
      cubemapPath: { ...P.cubemapPath, description: 'Cube texture to light from (sets the specified-cubemap source); a path that does not load fails before anything spawns.' },
      recapture: P.recapture },
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_sky_light', name: 'SkyLight_1' },
  }),
  buildRecord({
    id: 'build_environment.ensure_single_sky_light', action: 'ensure_single_sky_light', family: F,
    summary: 'Ensure exactly one sky light exists in the level.',
    whenToUse: ['Duplicate sky lights must be consolidated.'],
    whenNotToUse: ['Multiple sky lights are intentionally present.'],
    inputProps: { name: { ...P.name, description: 'Label of the sky light to keep (default SkyLight); another is relabelled, or one spawned, when none has it.' }, recapture: P.recapture },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'ensure_single_sky_light' },
  }),
  buildRecord({
    id: 'build_environment.create_lightmass_volume', action: 'create_lightmass_volume', family: F,
    summary: 'Create a LightmassImportanceVolume actor.',
    whenToUse: ['A lightmass importance volume is needed for baking.'],
    whenNotToUse: ['Dynamic lighting is used exclusively.'],
    inputProps: { name: P.name, location: P.location, size: P.size },
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_lightmass_volume', name: 'LIV_1' },
  }),
  buildRecord({
    id: 'build_environment.create_lighting_enabled_level', action: 'create_lighting_enabled_level', family: F,
    summary: 'Create a level with lighting enabled.',
    whenToUse: ['A new level with lighting setup is needed.'],
    whenNotToUse: ['An existing level should be modified.'],
    inputProps: { name: P.name, path: P.path, levelName: str('Level name; appended when path is a folder.') },
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_lighting_enabled_level', name: 'LightingLevel' },
  }),
  buildRecord({
    id: 'build_environment.setup_global_illumination', action: 'setup_global_illumination', family: F,
    summary: 'Configure global illumination settings.',
    whenToUse: ['GI must be enabled or tuned.'],
    whenNotToUse: ['GI is not needed for the scene.'],
    inputProps: {
      method: {
        type: 'string',
        enum: ['LumenGI', 'ScreenSpace', 'None', 'RayTraced', 'Lightmass'],
        description: 'Global illumination method. Matches the handler-enforced value set.',
      },
      quality: {
        type: 'string',
        enum: ['Low', 'Medium', 'High', 'Epic'],
        description: 'Global illumination scalability level (sg.GlobalIlluminationQuality).',
      },
      indirectLightingIntensity: P.indirectLightingIntensity, bounces: P.bounces,
    },
    required: ['method'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'setup_global_illumination', method: 'LumenGI' },
  }),
  buildRecord({
    id: 'build_environment.configure_shadows', action: 'configure_shadows', family: F,
    summary: 'Configure shadow settings for lights.',
    whenToUse: ['Shadow quality or method must be tuned.'],
    whenNotToUse: ['Shadows are not needed.'],
    inputProps: {
      settings: { ...P.settings, description: 'The same shadow keys as the top level, nested; a value here wins.' },
      actorName: { ...P.actorName, description: 'Light whose own shadow settings (castShadows, shadowBias, shadowSlopeBias, shadowResolutionScale) change.' },
      shadowQuality: P.shadowQuality, shadowDistance: P.shadowDistance, contactShadows: P.contactShadows,
      rayTracedShadows: P.rayTracedShadows, virtualShadowMaps: P.virtualShadowMaps,
      castShadows: P.castShadows, shadowBias: P.shadowBias, shadowSlopeBias: P.shadowSlopeBias, shadowResolutionScale: P.shadowResolutionScale,
    },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_shadows', actorName: 'Light_1', shadowQuality: 'High' },
  }),
  buildRecord({
    id: 'build_environment.set_exposure', action: 'set_exposure', family: F,
    summary: 'Set exposure settings for the camera or auto-exposure.',
    whenToUse: ['Exposure must be adjusted.'],
    whenNotToUse: ['Default exposure is sufficient.'],
    inputProps: {
      actorName: P.actorName,
      method: {
        type: 'string',
        enum: ['Manual', 'AutoExposureHistogram', 'AutoExposureBasic'],
        description: 'Auto-exposure method written on the post-process volume.',
      },
      compensationValue: P.compensationValue, minBrightness: P.minBrightness, maxBrightness: P.maxBrightness,
    },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'set_exposure', compensationValue: 1.0 },
  }),
  buildRecord({
    id: 'build_environment.set_ambient_occlusion', action: 'set_ambient_occlusion', family: F,
    summary: 'Set ambient occlusion settings.',
    whenToUse: ['AO intensity or method must be tuned.'],
    whenNotToUse: ['AO is not needed.'],
    inputProps: {
      actorName: P.actorName, enabled: P.enabled,
      intensity: { type: 'number', description: 'Ambient occlusion intensity.' },
      radius: { type: 'number', description: 'Ambient occlusion radius in world units.' },
      quality: {
        type: 'string',
        enum: ['Low', 'Medium', 'High'],
        description: 'Ambient occlusion quality on the post-process volume (25, 50 or 100 of 100).',
      },
    },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'set_ambient_occlusion', intensity: 0.5 },
  }),
  buildRecord({
    id: 'build_environment.setup_volumetric_fog', action: 'setup_volumetric_fog', family: F,
    summary: 'Configure volumetric fog settings.',
    whenToUse: ['Volumetric fog must be enabled or tuned.'],
    whenNotToUse: ['Standard exponential height fog is sufficient.'],
    inputProps: { enabled: P.enabled, viewDistance: P.viewDistance },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'setup_volumetric_fog', enabled: true, viewDistance: 6000 },
  }),
  buildRecord({
    id: 'build_environment.build_lighting', action: 'build_lighting', family: F,
    topics: ['bake lighting', 'lightmass', 'build lights', 'rebuild lighting', 'bake lightmaps'],
    summary: 'Build static lighting for the current level.',
    whenToUse: ['Static lighting must be built.'],
    whenNotToUse: ['Dynamic lighting is used exclusively.'],
    inputProps: { quality: P.quality },
    effect: 'write', behavior: { longRunning: true, idempotency: 'idempotent' },
    latency: 'long-running', resources: 'high',
    exampleInput: { action: 'build_lighting', quality: 'Preview' },
  }),
  buildRecord({
    id: 'build_environment.list_light_types', action: 'list_light_types', family: F,
    topics: ['kinds available', 'light classes', 'class names'],
    summary: 'List the light classes known to the editor: DirectionalLight, PointLight, SpotLight, RectLight and any other loaded light class, with a count.',
    whenToUse: ['Available light types must be enumerated.'],
    whenNotToUse: ['A specific light type is already known.'],
    inputProps: {},
    outputProps: {
      types: { type: 'array', items: str('Light type.'), description: 'Available light types.' },
      count: { type: 'number', description: 'Number of available light types.' },
    },
    outputRequired: ['types'],
    effect: 'read', latency: 'instant', resources: 'low',
    exampleInput: { action: 'list_light_types' },
    exampleOutput: { success: true, types: ['PointLight', 'SpotLight', 'DirectionalLight'], count: 3 },
  }),
];
