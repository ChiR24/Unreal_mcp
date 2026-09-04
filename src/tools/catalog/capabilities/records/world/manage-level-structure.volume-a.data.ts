/**
 * Level-structure volume records, part A (13 of 28 volume actions): trigger/
 * blocking/kill-Z/pain/physics/audio/reverb volumes. This shard is one of two
 * that together own all 28 volume records (no stale monolith). Grounded in the
 * native LevelStructure volume dispatch and manage-level-structure-tool.ts
 * (VOLUME_ACTIONS). Water runtime detection applies to bWaterVolume physics
 * volumes. All are editor-state 'edit'.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'volume';
const NR = 'Distinct manage_level_structure volume verb and target; no cross-tool duplicate.';

export const LEVEL_VOLUME_A_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_trigger_volume', dispatchAction: 'create_trigger_volume',
    family: F, summary: 'Create a trigger volume (box) in the level.',
    whenToUse: ['An area-triggered gameplay volume is needed.'], whenNotToUse: ['A sphere/capsule trigger is needed; use create_trigger_*.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location', 'extent'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_trigger_volume', location: { x: 0, y: 0, z: 0 }, extent: { x: 500, y: 500, z: 200 } },
    exampleOutput: { success: true, message: 'Trigger volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'add_trigger_volume', dispatchAction: 'add_trigger_volume',
    family: F, summary: 'Add (alias of create) a trigger volume to an actor.',
    whenToUse: ['A trigger volume must be added using the add verb.'], whenNotToUse: ['Prefer create_trigger_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, actorPath: P.actorPath, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_trigger_volume', location: { x: 0, y: 0, z: 0 }, actorPath: '/Game/Maps/Demo.Demo:PersistentLevel.SM_House' },
    exampleOutput: { success: true, message: 'Trigger volume added' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_trigger_box', dispatchAction: 'create_trigger_box',
    family: F, summary: 'Create a box-shaped trigger volume.',
    whenToUse: ['A box trigger volume must be created.'], whenNotToUse: ['A sphere/capsule trigger is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, boxExtent: P.boxExtent, save: P.save },
    required: ['location', 'boxExtent'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_trigger_box', location: { x: 0, y: 0, z: 0 }, boxExtent: { x: 200, y: 200, z: 100 } },
    exampleOutput: { success: true, message: 'Box trigger volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_trigger_sphere', dispatchAction: 'create_trigger_sphere',
    family: F, summary: 'Create a sphere-shaped trigger volume.',
    whenToUse: ['A sphere trigger volume must be created.'], whenNotToUse: ['A box trigger is needed.'],
    inputProps: { rotation: P.rotation, volumeName: P.volumeName, location: P.location, sphereRadius: P.sphereRadius, save: P.save },
    required: ['location', 'sphereRadius'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_trigger_sphere', location: { x: 0, y: 0, z: 0 }, sphereRadius: 300 },
    exampleOutput: { success: true, message: 'Sphere trigger volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_trigger_capsule', dispatchAction: 'create_trigger_capsule',
    family: F, summary: 'Create a capsule-shaped trigger volume.',
    whenToUse: ['A capsule trigger volume must be created.'], whenNotToUse: ['A box trigger is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, capsuleRadius: P.capsuleRadius, capsuleHalfHeight: P.capsuleHalfHeight, save: P.save },
    required: ['location', 'capsuleRadius'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_trigger_capsule', location: { x: 0, y: 0, z: 0 }, capsuleRadius: 150 },
    exampleOutput: { success: true, message: 'Capsule trigger volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_blocking_volume', dispatchAction: 'create_blocking_volume',
    family: F, summary: 'Create a blocking volume that prevents actor traversal.',
    whenToUse: ['An area must block movement.'], whenNotToUse: ['A trigger is needed; use create_trigger_*.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_blocking_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Blocking volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'add_blocking_volume', dispatchAction: 'add_blocking_volume',
    family: F, summary: 'Add (alias of create) a blocking volume.',
    whenToUse: ['A blocking volume must be added using the add verb.'], whenNotToUse: ['Prefer create_blocking_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, actorPath: P.actorPath, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_blocking_volume', location: { x: 0, y: 0, z: 0 }, actorPath: '/Game/Maps/Demo.Demo:PersistentLevel.SM_House' },
    exampleOutput: { success: true, message: 'Blocking volume added' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_kill_z_volume', dispatchAction: 'create_kill_z_volume',
    family: F, summary: 'Create a kill-Z volume that resets actors falling below it.',
    whenToUse: ['Actors falling below a plane must be killed/reset.'], whenNotToUse: ['A generic blocking volume is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_kill_z_volume', location: { x: 0, y: 0, z: -1000 } },
    exampleOutput: { success: true, message: 'Kill-Z volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'add_kill_z_volume', dispatchAction: 'add_kill_z_volume',
    family: F, summary: 'Add (alias of create) a kill-Z volume.',
    whenToUse: ['A kill-Z volume must be added using the add verb.'], whenNotToUse: ['Prefer create_kill_z_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, actorPath: P.actorPath, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_kill_z_volume', location: { x: 0, y: 0, z: -1000 }, actorPath: '/Game/Maps/Demo.Demo:PersistentLevel.SM_House' },
    exampleOutput: { success: true, message: 'Kill-Z volume added' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_pain_causing_volume', dispatchAction: 'create_pain_causing_volume',
    family: F, summary: 'Create a pain-causing volume that damages actors inside it.',
    whenToUse: ['Actors inside a region must take damage per second.'], whenNotToUse: ['A physics volume is needed; use create_physics_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, bPainCausing: P.bPainCausing, damagePerSec: P.damagePerSec, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_pain_causing_volume', location: { x: 0, y: 0, z: 0 }, damagePerSec: 10 },
    exampleOutput: { success: true, message: 'Pain-causing volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_physics_volume', dispatchAction: 'create_physics_volume',
    family: F, summary: 'Create a physics volume overriding fluid friction and terminal velocity.',
    whenToUse: ['Fluid friction/terminal velocity must be tuned in a region.'], whenNotToUse: ['Damage is needed; use create_pain_causing_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, fluidFriction: P.fluidFriction, terminalVelocity: P.terminalVelocity, bWaterVolume: P.bWaterVolume, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_physics_volume', location: { x: 0, y: 0, z: 0 }, bWaterVolume: true },
    exampleOutput: { success: true, message: 'Physics volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'add_physics_volume', dispatchAction: 'add_physics_volume',
    family: F, summary: 'Add (alias of create) a physics volume.',
    whenToUse: ['A physics volume must be added using the add verb.'], whenNotToUse: ['Prefer create_physics_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, actorPath: P.actorPath, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_physics_volume', location: { x: 0, y: 0, z: 0 }, actorPath: '/Game/Maps/Demo.Demo:PersistentLevel.SM_House' },
    exampleOutput: { success: true, message: 'Physics volume added' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_audio_volume', dispatchAction: 'create_audio_volume',
    family: F, summary: 'Create an audio volume controlling reverb/fade in a region.',
    whenToUse: ['Audio reverb/fade behavior must be scoped to a region.'], whenNotToUse: ['A reverb volume specifically is needed; use create_reverb_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, bEnabled: P.bEnabled, reverbVolume: P.reverbVolume, fadeTime: P.fadeTime, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_audio_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Audio volume created' }, normalizationRationale: NR,
  }),
];
