/**
 * Water family records (8 actions).
 *
 * Grounded in native EnvironmentHandlersTimeWater.cpp. create_water_body_ocean,
 * create_water_body_lake, create_water_body_river, create_water_body_custom,
 * configure_water_waves, configure_water_material, configure_water_collision,
 * create_buoyancy_component dispatch through build_environment. Native handlers
 * call MarkPackageDirty() (deferred persistence - no immediate save).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'water';
const WU = ['A water body or water simulation must be created or configured.'];

export const WATER_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_water_body_ocean', action: 'create_water_body_ocean', family: F,
    summary: 'Create an ocean water body actor.',
    whenToUse: WU, whenNotToUse: ['A lake or river water body is needed.'],
    inputProps: { action: P.action, name: P.name, waterBodyName: P.waterBodyName, location: P.location, materialPath: P.materialPath },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_water_body_ocean', name: 'Ocean_1', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Ocean water body created' },
  }),
  buildRecord({
    id: 'build_environment.create_water_body_lake', action: 'create_water_body_lake', family: F,
    summary: 'Create a lake water body actor.',
    whenToUse: WU, whenNotToUse: ['An ocean or river water body is needed.'],
    inputProps: { action: P.action, name: P.name, waterBodyName: P.waterBodyName, location: P.location, materialPath: P.materialPath },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_water_body_lake', name: 'Lake_1', location: { x: 0, y: 0, z: 100 } },
    exampleOutput: { success: true, message: 'Lake water body created' },
  }),
  buildRecord({
    id: 'build_environment.create_water_body_river', action: 'create_water_body_river', family: F,
    summary: 'Create a river water body actor.',
    whenToUse: WU, whenNotToUse: ['An ocean or lake water body is needed.'],
    inputProps: { action: P.action, name: P.name, waterBodyName: P.waterBodyName, location: P.location, materialPath: P.materialPath },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_water_body_river', name: 'River_1', location: { x: 0, y: 0, z: 50 } },
    exampleOutput: { success: true, message: 'River water body created' },
  }),
  buildRecord({
    id: 'build_environment.create_water_body_custom', action: 'create_water_body_custom', family: F,
    summary: 'Create a custom water body actor.',
    whenToUse: WU, whenNotToUse: ['A standard ocean/lake/river body is sufficient.'],
    inputProps: { action: P.action, name: P.name, waterBodyName: P.waterBodyName, location: P.location, materialPath: P.materialPath },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_water_body_custom', name: 'CustomWater_1' },
    exampleOutput: { success: true, message: 'Custom water body created' },
  }),
  buildRecord({
    id: 'build_environment.configure_water_waves', action: 'configure_water_waves', family: F,
    summary: 'Configure water wave settings (height, length, amplitude).',
    whenToUse: WU, whenNotToUse: ['Default waves are sufficient.'],
    inputProps: { action: P.action, waveHeight: P.waveHeight, waveLength: P.waveLength,
      amplitude: P.amplitude, steepness: P.steepness, speed: P.speed, direction: P.direction },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_water_waves', waveHeight: 1.0, waveLength: 10, speed: 1.0 },
    exampleOutput: { success: true, message: 'Water waves configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_water_material', action: 'configure_water_material', family: F,
    summary: 'Configure the material applied to a water body.',
    whenToUse: WU, whenNotToUse: ['Default water material is sufficient.'],
    inputProps: { action: P.action, materialPath: P.materialPath, settings: P.settings },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_water_material', materialPath: '/Game/Materials/M_Water' },
    exampleOutput: { success: true, message: 'Water material configured' },
  }),
  buildRecord({
    id: 'build_environment.configure_water_collision', action: 'configure_water_collision', family: F,
    summary: 'Configure water collision settings.',
    whenToUse: WU, whenNotToUse: ['Default collision is sufficient.'],
    inputProps: { action: P.action, collisionEnabled: P.collisionEnabled, settings: P.settings },
    required: ['action'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_water_collision', collisionEnabled: true },
    exampleOutput: { success: true, message: 'Water collision configured' },
  }),
  buildRecord({
    id: 'build_environment.create_buoyancy_component', action: 'create_buoyancy_component', family: F,
    summary: 'Create a buoyancy component on an actor for water interaction.',
    whenToUse: WU, whenNotToUse: ['Buoyancy is not needed.'],
    inputProps: { action: P.action, actorPath: P.actorPath, actorName: P.actorName, targetActor: P.targetActor, settings: P.settings },
    required: ['action'], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_buoyancy_component', actorName: 'Boat_1' },
    exampleOutput: { success: true, message: 'Buoyancy component created' },
  }),
];
