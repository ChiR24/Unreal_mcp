/**
 * Water family records (8 actions).
 *
 * Grounded in native EnvironmentHandlersTimeWater.cpp. create_water_body_ocean,
 * create_water_body_lake, create_water_body_river, create_water_body_custom,
 * configure_water_waves, configure_water_material, configure_water_collision,
 * create_buoyancy_component dispatch through build_environment. Native handlers
 * call MarkPackageDirty() (deferred persistence - no immediate save).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'water';
const WU = ['A water body or water simulation must be created or configured.'];
// A create finds the body by waterBodyName (or name) and spawns it when missing; material
// and collision go on that body.
const BODY = {
  name: P.name, waterBodyName: P.waterBodyName, location: P.location,
  rotation: { ...P.rotation, description: 'Water body rotation, applied on every call.' },
  materialPath: P.materialPath, materialIndex: P.materialIndex,
  collisionEnabled: { ...P.collisionEnabled, description: 'Collision on the body\'s primitive components: true = query and physics, false = none.' },
  settings: P.actorSettings,
};
// The configure variants target the body named by waterBodyName, else the first ocean,
// lake, river or custom body in the level.
const TARGET = { waterBodyName: { ...P.waterBodyName, description: 'Water body to configure; the first ocean, lake, river or custom body when omitted.' } };

export const WATER_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_water_body_ocean', action: 'create_water_body_ocean', family: F,
    summary: 'Create an ocean water body actor with the editor\'s default ocean material and waves.',
    whenToUse: WU, whenNotToUse: ['A lake or river water body is needed.'],
    inputProps: BODY,
    required: [], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_water_body_ocean', name: 'Ocean_1', location: { x: 0, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.create_water_body_lake', action: 'create_water_body_lake', family: F,
    summary: 'Create a lake water body actor.',
    whenToUse: WU, whenNotToUse: ['An ocean or river water body is needed.'],
    inputProps: BODY,
    required: [], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_water_body_lake', name: 'Lake_1', location: { x: 0, y: 0, z: 100 } },
  }),
  buildRecord({
    id: 'build_environment.create_water_body_river', action: 'create_water_body_river', family: F,
    summary: 'Create a river water body actor.',
    whenToUse: WU, whenNotToUse: ['An ocean or lake water body is needed.'],
    inputProps: BODY,
    required: [], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_water_body_river', name: 'River_1', location: { x: 0, y: 0, z: 50 } },
  }),
  buildRecord({
    id: 'build_environment.create_water_body_custom', action: 'create_water_body_custom', family: F,
    summary: 'Create a custom water body actor.',
    whenToUse: WU, whenNotToUse: ['A standard ocean/lake/river body is sufficient.'],
    inputProps: BODY,
    required: [], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_water_body_custom', name: 'CustomWater_1' },
  }),
  buildRecord({
    id: 'build_environment.configure_water_waves', action: 'configure_water_waves', family: F,
    summary: 'Give a water body Gerstner waves, the swell its surface and anything floating on it follow: height, wavelength, steepness and wind direction. The call fails when the body does not end up holding the waves.',
    whenToUse: WU, whenNotToUse: ['Default waves are sufficient.'],
    inputProps: { waveHeight: { ...P.waveHeight, description: 'Height (amplitude) of the largest wave in cm; the body\'s other waves shrink to 15% of it, as a real sea\'s do. A calm harbour is about 5-10, open sea 50 or more.' },
      waveLength: { ...P.waveLength, description: 'Wavelength of the longest wave in cm, crest to crest; the shortest waves are 13% of it (a harbour is about 1500-2500).' },
      amplitude: { ...P.amplitude, description: 'Same as waveHeight; waveHeight wins when both are given.' },
      steepness: { ...P.steepness, description: 'Crest sharpness of the short waves, 0 (smooth swell) to 1 (peaked); the long waves take about half. Keep it under about 0.3: steep crests from many waves fold the surface over itself.' },
      direction: { ...P.direction, description: 'Wind direction: its yaw sets where the waves travel.' }, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_water_waves', waveHeight: 1.0, waveLength: 10 },
  }),
  buildRecord({
    id: 'build_environment.configure_water_material', action: 'configure_water_material', family: F,
    summary: 'Configure the material applied to a water body.',
    whenToUse: WU, whenNotToUse: ['Default water material is sufficient.'],
    inputProps: { materialPath: P.materialPath, materialIndex: P.materialIndex, settings: P.settings, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_water_material', materialPath: '/Game/Materials/M_Water' },
  }),
  buildRecord({
    id: 'build_environment.configure_water_collision', action: 'configure_water_collision', family: F,
    summary: 'Configure water collision settings.',
    whenToUse: WU, whenNotToUse: ['Default collision is sufficient.'],
    inputProps: { collisionEnabled: P.collisionEnabled, settings: P.settings, ...TARGET },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_water_collision', collisionEnabled: true },
  }),
  buildRecord({
    id: 'build_environment.create_buoyancy_component', action: 'create_buoyancy_component', topics: ['make a boat float'], family: F,
    summary: 'Make an actor float: add a buoyancy component (pontoons in settings.BuoyancyData) to an actor whose root is a physics-simulating mesh, and let it take the water body\'s overlap at level load, so one placed already in the water floats instead of sinking. In a Blueprint, add /Script/Water.BuoyancyComponent with edit_scs and set the class default bGenerateOverlapEventsDuringLevelStreaming to true.',
    whenToUse: WU, whenNotToUse: ['Buoyancy is not needed.'],
    inputProps: { actorPath: P.actorPath, actorName: P.actorName, targetActor: P.targetActor, settings: P.settings },
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_buoyancy_component', actorName: 'Boat_1' },
  }),
];
