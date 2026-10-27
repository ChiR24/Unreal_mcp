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
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';
import { num } from '../shared/schema-props.js';

const F = 'atmosphere';
const WU = ['Atmosphere, sky, fog, or time-of-day must be configured.'];
// The fog records read density as the height fog's FogDensity; the shared property said "Foliage density."
const FOG_DENSITY = { ...P.density, description: 'Fog density, the height fog\'s FogDensity (0.02 a light haze, 0.05 and up a thick fog).' };
// The configure variants find their actor by actorName, else the level's first
// actor of that kind, and spawn one only when the level has none.
const TARGET = {
  actorName: { ...P.actorName, description: 'Label of the actor to configure; the first one of its kind in the level when omitted. One is spawned with this label when none exists.' },
  location: { ...P.location, description: 'Where the actor is spawned when the level has none; an existing actor is not moved.' },
};
// The sun and directional light are aimed by azimuth and elevation (or the hour), which a
// rotation would silently override, so only the other variants take one.
const ROTATED = { ...TARGET, rotation: { ...P.rotation, description: 'Actor rotation, applied on every call.' } };
// The color curves are edited in place: curvePath names the asset itself.
const CURVE_PATH = { ...P.curvePath, description: 'The curve asset to edit, created there when missing (default /Game/Environment/Curves/MCP_SkyColorCurve or MCP_LightColorCurve).' };
const CURVE_KEYS = {
  type: 'array', minItems: 1,
  description: 'Color keys that replace the curve\'s keys; required to change an existing curve. A new curve without keys is flat white.',
  items: {
    type: 'object', description: 'One key.',
    properties: {
      time: num('Key time, e.g. the hour of day.'),
      color: { type: 'object', description: 'Color {r, g, b, a}; a defaults to 1.', properties: { r: num('Red.'), g: num('Green.'), b: num('Blue.'), a: num('Alpha.') }, additionalProperties: false },
    },
    required: ['time', 'color'], additionalProperties: false,
  },
};

export const ATMOSPHERE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_sky_sphere', action: 'create_sky_sphere', family: F,
    summary: 'Create the sky lighting rig in the level being edited: a Sky Atmosphere, a sun (Directional Light) and a Sky Light, labelled <name>_Atmosphere, <name>_Sun and <name>_SkyLight (SkyAtmosphere, DirectionalLight and SkyLight without a name). An actor already carrying one of those labels is reused and set up rather than doubled, but a sun or atmosphere labelled otherwise is not found, so the level gets a second one. No sky sphere mesh, clouds or fog: add those with configure_volumetric_cloud and create_fog_volume. Replies createdActorCount (0 when all three existed) and the three actor names.',
    whenToUse: WU, whenNotToUse: ['A sky atmosphere or sun is already present under another label: configure it (configure_sky_atmosphere, configure_directional_light_atmosphere) instead of adding a second.'],
    inputProps: { name: P.name, location: P.location },
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_sky_sphere', name: 'SkySphere_1' },
  }),
  buildRecord({
    id: 'build_environment.set_time_of_day', action: 'set_time_of_day', family: F,
    summary: 'Set the time of day: the hour sets the sun\'s elevation (azimuth and elevation may be given directly).',
    whenToUse: WU, whenNotToUse: ['A full time-of-day system should be created.'],
    inputProps: { time: P.time, hour: P.hour, azimuth: P.azimuth, elevation: P.elevation, settings: P.actorSettings, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'set_time_of_day', time: 14.5 },
  }),
  buildRecord({
    id: 'build_environment.create_time_of_day_system', action: 'create_time_of_day_system', family: F,
    summary: 'Create one actor, TimeOfDaySystem (or name), carrying its own sun (a directional light at intensity 10, the atmosphere sun), sky light (intensity 1) and sky atmosphere, all movable, with the sun at noon; an actor already labelled so gets the missing parts. Nothing animates it, no curves and no day cycle: move the sun with set_time_of_day. A level that already has a sun or sky atmosphere (create_sky_sphere, the default level template) ends up with two of each, so use one rig or the other.',
    whenToUse: WU, whenNotToUse: ['The level already has a sun and sky atmosphere: set_time_of_day moves its sun.'],
    inputProps: { name: P.name, location: P.location },
    required: [], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_time_of_day_system', name: 'TOD_1' },
  }),
  buildRecord({
    id: 'build_environment.create_fog_volume', action: 'create_fog_volume', family: F,
    summary: 'Create an Exponential Height Fog actor in the level being edited, or set up the one already labelled name (default ExponentialHeightFog), with density and settings (properties of its fog component). It is height fog over the whole level, not a local volume, and a fog labelled otherwise is not found, so the level gets a second one.',
    whenToUse: WU, whenNotToUse: ['The level has a height fog under another label: configure_exponential_height_fog finds the first one.'],
    inputProps: { name: P.name, location: P.location, rotation: P.rotation, density: FOG_DENSITY, settings: P.actorSettings },
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_fog_volume', name: 'FogVol_1', location: { x: 0, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.configure_sky_atmosphere', action: 'configure_sky_atmosphere', family: F,
    summary: 'Configure sky atmosphere settings.',
    whenToUse: WU, whenNotToUse: ['Default atmosphere is sufficient.'],
    inputProps: { settings: P.actorSettings, ...ROTATED },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_sky_atmosphere', settings: { MieScatteringScale: 0.1 } },
  }),
  buildRecord({
    id: 'build_environment.configure_sky_light', action: 'configure_sky_light', family: F,
    summary: 'Configure sky light intensity and cubemap.',
    whenToUse: WU, whenNotToUse: ['Default sky light is sufficient.'],
    inputProps: { skyLightIntensity: P.skyLightIntensity,
      cubemapPath: { ...P.cubemapPath, description: 'Cube texture the sky light uses (switches its source to a specified cubemap); a path that does not load fails the call.' },
      settings: P.actorSettings, ...ROTATED },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_sky_light', skyLightIntensity: 1.0 },
  }),
  buildRecord({
    id: 'build_environment.configure_directional_light_atmosphere', action: 'configure_directional_light_atmosphere', family: F,
    summary: 'Configure a directional light: its angle (azimuth, elevation above the horizon), intensity, and any light property such as LightColor through settings.',
    whenToUse: WU, whenNotToUse: ['Default atmosphere is sufficient.'],
    inputProps: { azimuth: P.azimuth, elevation: P.elevation, intensity: P.intensity, settings: P.actorSettings, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_directional_light_atmosphere', actorName: 'DirectionalLight_1', azimuth: 45, elevation: 30 },
  }),
  buildRecord({
    id: 'build_environment.configure_exponential_height_fog', action: 'configure_exponential_height_fog', family: F,
    summary: 'Configure exponential height fog settings.',
    whenToUse: WU, whenNotToUse: ['Volumetric fog should be used instead.'],
    inputProps: { settings: P.actorSettings, density: FOG_DENSITY, ...ROTATED },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_exponential_height_fog', density: 0.05 },
  }),
  buildRecord({
    id: 'build_environment.configure_volumetric_cloud', action: 'configure_volumetric_cloud', family: F,
    summary: 'Configure volumetric cloud settings.',
    whenToUse: WU, whenNotToUse: ['Static cloud textures are sufficient.'],
    inputProps: { settings: P.actorSettings, ...ROTATED },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_volumetric_cloud', settings: { LayerBottomAltitude: 2 } },
  }),
  buildRecord({
    id: 'build_environment.configure_sun_position', action: 'configure_sun_position', family: F,
    summary: 'Configure sun azimuth and elevation (or an hour of day) on the level\'s sun.',
    whenToUse: WU, whenNotToUse: ['The sun should follow a time-of-day system.'],
    inputProps: { azimuth: P.azimuth, elevation: P.elevation, time: P.time, hour: P.hour, settings: P.actorSettings, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'configure_sun_position', azimuth: 45, elevation: 60 },
  }),
  buildRecord({
    id: 'build_environment.configure_light_color_curve', action: 'configure_light_color_curve', family: F,
    summary: 'Set the color keys of a light color curve asset (made at curvePath when missing) for time-of-day.',
    whenToUse: WU, whenNotToUse: ['Static light color is sufficient.'],
    inputProps: { curvePath: CURVE_PATH, keys: CURVE_KEYS },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_light_color_curve', curvePath: '/Game/Curves/LightColor', keys: [{ time: 6, color: { r: 1, g: 0.6, b: 0.3 } }, { time: 12, color: { r: 1, g: 1, b: 0.95 } }] },
  }),
  buildRecord({
    id: 'build_environment.configure_sky_color_curve', action: 'configure_sky_color_curve', family: F,
    summary: 'Set the color keys of a sky color curve asset (made at curvePath when missing) for time-of-day.',
    whenToUse: WU, whenNotToUse: ['Static sky color is sufficient.'],
    inputProps: { curvePath: CURVE_PATH, keys: CURVE_KEYS },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_sky_color_curve', curvePath: '/Game/Curves/SkyColor', keys: [{ time: 0, color: { r: 0.05, g: 0.05, b: 0.2 } }, { time: 12, color: { r: 0.4, g: 0.6, b: 1 } }] },
  }),
];
