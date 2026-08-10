/**
 * Weather family records (5 actions).
 *
 * Grounded in native EnvironmentHandlersWeatherActors.cpp. create_weather_system,
 * configure_rain_particles, configure_snow_particles, configure_wind,
 * configure_lightning dispatch through build_environment. Native handlers call
 * MarkPackageDirty() (deferred persistence).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'weather';
const WU = ['A weather system or weather particle effect must be configured.'];

export const WEATHER_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_weather_system', action: 'create_weather_system', family: F,
    summary: 'Create a weather system actor with particle effects.',
    whenToUse: WU, whenNotToUse: ['Individual particle systems should be used.'],
    inputProps: { action: P.action, name: P.name, location: P.location, particleSystemPath: P.particleSystemPath },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_weather_system', name: 'Weather_1' },
    exampleOutput: { success: true, message: 'Weather system created' },
  }),
  buildRecord({
    id: 'build_environment.configure_rain_particles', action: 'configure_rain_particles', family: F,
    summary: 'Configure rain particle settings.',
    whenToUse: WU, whenNotToUse: ['Rain is not needed.'],
    inputProps: { action: P.action, particleSystemPath: P.particleSystemPath, density: P.density, settings: P.settings },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_rain_particles', density: 0.8 },
    exampleOutput: { success: true, message: 'Rain particles configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_snow_particles', action: 'configure_snow_particles', family: F,
    summary: 'Configure snow particle settings.',
    whenToUse: WU, whenNotToUse: ['Snow is not needed.'],
    inputProps: { action: P.action, particleSystemPath: P.particleSystemPath, density: P.density, settings: P.settings },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_snow_particles', density: 0.5 },
    exampleOutput: { success: true, message: 'Snow particles configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_wind', action: 'configure_wind', family: F,
    summary: 'Configure wind settings for foliage and particles.',
    whenToUse: WU, whenNotToUse: ['Wind is not needed.'],
    inputProps: { action: P.action, settings: P.settings, speed: P.speed, direction: P.direction },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_wind', speed: 10, direction: { pitch: 0, yaw: 90, roll: 0 } },
    exampleOutput: { success: true, message: 'Wind configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_lightning', action: 'configure_lightning', family: F,
    summary: 'Configure lightning effect settings.',
    whenToUse: WU, whenNotToUse: ['Lightning is not needed.'],
    inputProps: { action: P.action, particleSystemPath: P.particleSystemPath, settings: P.settings },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_lightning' },
    exampleOutput: { success: true, message: 'Lightning configured' },
  }),
];
