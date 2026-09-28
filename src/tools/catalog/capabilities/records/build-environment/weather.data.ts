/**
 * Weather family records (5 actions).
 *
 * Grounded in native EnvironmentHandlersWeatherActors.cpp. create_weather_system,
 * configure_rain_particles, configure_snow_particles, configure_wind,
 * configure_lightning dispatch through build_environment. Native handlers call
 * MarkPackageDirty() (deferred persistence).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'weather';
const WU = ['A weather system or weather particle effect must be configured.'];
// Each variant finds its emitter or wind actor by actorName (rain, snow and
// lightning default to WeatherEmitter, SnowEmitter, LightningEmitter), spawning it when missing.
const TARGET = {
  actorName: { ...P.actorName, description: 'Label of the weather actor to configure; spawned with this label when the level has none.' },
  location: { ...P.location, description: 'Where the actor is spawned when it does not exist yet; an existing actor is not moved.' },
};
const ROTATION = { ...P.rotation, description: 'Actor rotation, applied on every call.' };

export const WEATHER_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_weather_system', action: 'create_weather_system', family: F,
    summary: 'Create a weather system actor with particle effects.',
    whenToUse: WU, whenNotToUse: ['Individual particle systems should be used.'],
    inputProps: { name: P.name, location: P.location, rotation: ROTATION, particleSystemPath: P.particleSystemPath },
    required: [], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_weather_system', name: 'Weather_1' },
  }),
  buildRecord({
    id: 'build_environment.configure_rain_particles', action: 'configure_rain_particles', family: F,
    summary: 'Configure rain particle settings.',
    whenToUse: WU, whenNotToUse: ['Rain is not needed.'],
    inputProps: { particleSystemPath: P.particleSystemPath, settings: P.settings, rotation: ROTATION, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_rain_particles', particleSystemPath: '/Game/Weather/P_Rain' },
  }),
  buildRecord({
    id: 'build_environment.configure_snow_particles', action: 'configure_snow_particles', family: F,
    summary: 'Configure snow particle settings.',
    whenToUse: WU, whenNotToUse: ['Snow is not needed.'],
    inputProps: { particleSystemPath: P.particleSystemPath, settings: P.settings, rotation: ROTATION, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_snow_particles', particleSystemPath: '/Game/Weather/P_Snow' },
  }),
  buildRecord({
    id: 'build_environment.configure_wind', action: 'configure_wind', family: F,
    summary: 'Configure wind settings for foliage and particles.',
    whenToUse: WU, whenNotToUse: ['Wind is not needed.'],
    inputProps: { settings: P.settings, speed: P.speed, direction: P.direction, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_wind', speed: 10, direction: { pitch: 0, yaw: 90, roll: 0 } },
  }),
  buildRecord({
    id: 'build_environment.configure_lightning', action: 'configure_lightning', family: F,
    summary: 'Configure lightning effect settings.',
    whenToUse: WU, whenNotToUse: ['Lightning is not needed.'],
    inputProps: { particleSystemPath: P.particleSystemPath, settings: P.settings, rotation: ROTATION, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_lightning' },
  }),
];
