/**
 * Atmosphere/sky/time family records (12 actions).
 *
 * Grounded in native EnvironmentHandlers.cpp and ENVIRONMENT_ACTIONS.
 * configure_sky_atmosphere, configure_sky_light, configure_directional_light_atmosphere,
 * configure_exponential_height_fog, configure_volumetric_cloud, configure_sun_position,
 * configure_light_color_curve, configure_sky_color_curve dispatch through
 * build_environment. create_sky_sphere, set_time_of_day, create_time_of_day_system,
 * create_fog_volume dispatch through build_environment. Native handlers call
 * MarkPackageDirty() (deferred persistence - no immediate save).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'atmosphere';
const WU = ['Atmosphere, sky, fog, or time-of-day must be configured.'];

export const ATMOSPHERE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_sky_sphere', action: 'create_sky_sphere', family: F,
    summary: 'Create a sky sphere actor with atmospheric material.',
    whenToUse: WU, whenNotToUse: ['A sky atmosphere is already present.'],
    inputProps: { action: P.action, name: P.name, path: P.path, location: P.location },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_sky_sphere', name: 'SkySphere_1' },
    exampleOutput: { success: true, message: 'Sky sphere created' },
  }),
  buildRecord({
    id: 'build_environment.set_time_of_day', action: 'set_time_of_day', family: F,
    summary: 'Set the time of day for the sun and sky system.',
    whenToUse: WU, whenNotToUse: ['A full time-of-day system should be created.'],
    inputProps: { action: P.action, time: P.time, hour: P.hour },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'set_time_of_day', time: 14.5 },
    exampleOutput: { success: true, message: 'Time of day set' },
  }),
  buildRecord({
    id: 'build_environment.create_time_of_day_system', action: 'create_time_of_day_system', family: F,
    summary: 'Create a full time-of-day system with sun and sky curves.',
    whenToUse: WU, whenNotToUse: ['A static time should be set instead.'],
    inputProps: { action: P.action, name: P.name, location: P.location },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_time_of_day_system', name: 'TOD_1' },
    exampleOutput: { success: true, message: 'Time of day system created' },
  }),
  buildRecord({
    id: 'build_environment.create_fog_volume', action: 'create_fog_volume', family: F,
    summary: 'Create a fog volume actor.',
    whenToUse: WU, whenNotToUse: ['Exponential height fog is sufficient.'],
    inputProps: { action: P.action, name: P.name, path: P.path, location: P.location, scale: P.scale, density: P.density },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_fog_volume', name: 'FogVol_1', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Fog volume created' },
  }),
  buildRecord({
    id: 'build_environment.configure_sky_atmosphere', action: 'configure_sky_atmosphere', family: F,
    summary: 'Configure sky atmosphere settings.',
    whenToUse: WU, whenNotToUse: ['Default atmosphere is sufficient.'],
    inputProps: { action: P.action, settings: P.settings, intensity: P.intensity },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_sky_atmosphere', intensity: 1.0 },
    exampleOutput: { success: true, message: 'Sky atmosphere configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_sky_light', action: 'configure_sky_light', family: F,
    summary: 'Configure sky light intensity and cubemap.',
    whenToUse: WU, whenNotToUse: ['Default sky light is sufficient.'],
    inputProps: { action: P.action, skyLightIntensity: P.skyLightIntensity, cubemapPath: P.cubemapPath, settings: P.settings },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_sky_light', skyLightIntensity: 1.0 },
    exampleOutput: { success: true, message: 'Sky light configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_directional_light_atmosphere', action: 'configure_directional_light_atmosphere', family: F,
    summary: 'Configure directional light atmosphere settings.',
    whenToUse: WU, whenNotToUse: ['Default atmosphere is sufficient.'],
    inputProps: { action: P.action, actorName: P.actorName, azimuth: P.azimuth, elevation: P.elevation, intensity: P.intensity },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_directional_light_atmosphere', actorName: 'DirectionalLight_1', azimuth: 45, elevation: 30 },
    exampleOutput: { success: true, message: 'Directional light atmosphere configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_exponential_height_fog', action: 'configure_exponential_height_fog', family: F,
    summary: 'Configure exponential height fog settings.',
    whenToUse: WU, whenNotToUse: ['Volumetric fog should be used instead.'],
    inputProps: { action: P.action, settings: P.settings, density: P.density },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_exponential_height_fog', density: 0.05 },
    exampleOutput: { success: true, message: 'Height fog configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_volumetric_cloud', action: 'configure_volumetric_cloud', family: F,
    summary: 'Configure volumetric cloud settings.',
    whenToUse: WU, whenNotToUse: ['Static cloud textures are sufficient.'],
    inputProps: { action: P.action, settings: P.settings, density: P.density },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_volumetric_cloud', density: 1.0 },
    exampleOutput: { success: true, message: 'Volumetric cloud configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_sun_position', action: 'configure_sun_position', family: F,
    summary: 'Configure sun azimuth and elevation.',
    whenToUse: WU, whenNotToUse: ['The sun should follow a time-of-day system.'],
    inputProps: { action: P.action, azimuth: P.azimuth, elevation: P.elevation },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'configure_sun_position', azimuth: 45, elevation: 60 },
    exampleOutput: { success: true, message: 'Sun position configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_light_color_curve', action: 'configure_light_color_curve', family: F,
    summary: 'Configure a light color curve asset for time-of-day.',
    whenToUse: WU, whenNotToUse: ['Static light color is sufficient.'],
    inputProps: { action: P.action, curvePath: P.curvePath, settings: P.settings },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_light_color_curve', curvePath: '/Game/Curves/LightColor' },
    exampleOutput: { success: true, message: 'Light color curve configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_sky_color_curve', action: 'configure_sky_color_curve', family: F,
    summary: 'Configure a sky color curve asset for time-of-day.',
    whenToUse: WU, whenNotToUse: ['Static sky color is sufficient.'],
    inputProps: { action: P.action, curvePath: P.curvePath, settings: P.settings },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_sky_color_curve', curvePath: '/Game/Curves/SkyColor' },
    exampleOutput: { success: true, message: 'Sky color curve configured' },
  }),
];
