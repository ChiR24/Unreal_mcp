/**
 * Level-structure volume records, part B (15 of 28 volume actions): reverb/cull/
 * precomputed-visibility/lightmass/nav/camera-blocking/post-process volumes plus
 * volume property management and listing. This shard is one of two that together
 * own all 28 volume records (no stale monolith). Grounded in the native
 * LevelStructure volume dispatch and manage-level-structure-tool.ts
 * (VOLUME_ACTIONS). All are editor-state 'edit'.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'volume';
const NR = 'Distinct manage_level_structure volume verb and target; no cross-tool duplicate.';

export const LEVEL_VOLUME_B_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_reverb_volume', dispatchAction: 'create_reverb_volume',
    family: F, summary: 'Create a reverb volume applying a reverb effect in a region.',
    whenToUse: ['A reverb effect must be scoped to a region.'], whenNotToUse: ['A generic audio volume is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, reverbVolume: P.reverbVolume, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_reverb_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Reverb volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_cull_distance_volume', dispatchAction: 'create_cull_distance_volume',
    family: F, summary: 'Create a cull distance volume defining size/distance cull pairs.',
    whenToUse: ['Per-size cull distances must be defined for a region.'], whenNotToUse: ['A generic volume is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, cullDistances: P.cullDistances, save: P.save },
    required: ['location', 'cullDistances'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_cull_distance_volume', location: { x: 0, y: 0, z: 0 }, cullDistances: [{ size: 100, cullDistance: 5000 }] },
    exampleOutput: { success: true, message: 'Cull distance volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'add_cull_distance_volume', dispatchAction: 'add_cull_distance_volume',
    family: F, summary: 'Add (alias of create) a cull distance volume.',
    whenToUse: ['A cull distance volume must be added using the add verb.'], whenNotToUse: ['Prefer create_cull_distance_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, cullDistances: P.cullDistances, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_cull_distance_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Cull distance volume added' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_precomputed_visibility_volume', dispatchAction: 'create_precomputed_visibility_volume',
    family: F, summary: 'Create a precomputed visibility volume for occlusion culling.',
    whenToUse: ['Visibility should be precomputed within a region.'], whenNotToUse: ['A cull distance volume is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_precomputed_visibility_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Precomputed visibility volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_lightmass_importance_volume', dispatchAction: 'create_lightmass_importance_volume',
    family: F, summary: 'Create a Lightmass importance volume focusing bake quality.',
    whenToUse: ['Lightmap bake quality must be focused on a region.'], whenNotToUse: ['A generic volume is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_lightmass_importance_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Lightmass importance volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_nav_mesh_bounds_volume', dispatchAction: 'create_nav_mesh_bounds_volume',
    family: F, summary: 'Create a nav mesh bounds volume delimiting navmesh generation.',
    whenToUse: ['Navmesh generation must be bounded to a region.'], whenNotToUse: ['A nav modifier volume is needed; use create_nav_modifier_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_nav_mesh_bounds_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Nav mesh bounds volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_nav_modifier_volume', dispatchAction: 'create_nav_modifier_volume',
    family: F, summary: 'Create a nav modifier volume overriding area flags within a region.',
    whenToUse: ['Nav area flags must be overridden in a region.'], whenNotToUse: ['A nav bounds volume is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_nav_modifier_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Nav modifier volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_camera_blocking_volume', dispatchAction: 'create_camera_blocking_volume',
    family: F, summary: 'Create a camera blocking volume hiding geometry from cameras.',
    whenToUse: ['Cameras must be blocked from seeing behind a region.'], whenNotToUse: ['A generic blocking volume is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_camera_blocking_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Camera blocking volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'create_post_process_volume', dispatchAction: 'create_post_process_volume',
    family: F, summary: 'Create a post-process volume (optionally unbound) for grading.',
    whenToUse: ['Post-process grading must be scoped or applied globally.'], whenNotToUse: ['A generic volume is needed.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, bUnbound: P.bUnbound, blendRadius: P.blendRadius, blendWeight: P.blendWeight, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_post_process_volume', location: { x: 0, y: 0, z: 0 }, bUnbound: false },
    exampleOutput: { success: true, message: 'Post process volume created' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'add_post_process_volume', dispatchAction: 'add_post_process_volume',
    family: F, summary: 'Add (alias of create) a post-process volume.',
    whenToUse: ['A post-process volume must be added using the add verb.'], whenNotToUse: ['Prefer create_post_process_volume.'],
    inputProps: { volumeName: P.volumeName, location: P.location, extent: P.extent, save: P.save },
    required: ['location'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_post_process_volume', location: { x: 0, y: 0, z: 0 } },
    exampleOutput: { success: true, message: 'Post process volume added' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_extent', dispatchAction: 'set_volume_extent',
    family: F, summary: 'Set the extent (half-size) of an existing volume.',
    whenToUse: ['A volume must be resized.'], whenNotToUse: ['A new volume must be created; use create_*_volume.'],
    inputProps: { volumeName: P.volumeName, extent: P.extent, save: P.save },
    required: ['volumeName', 'extent'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_volume_extent', volumeName: 'PP_01', extent: { x: 1000, y: 1000, z: 500 } },
    exampleOutput: { success: true, message: 'Volume extent set' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_bounds', dispatchAction: 'set_volume_bounds',
    family: F, summary: 'Set the bounds (origin/extent) of an existing volume.',
    whenToUse: ['A volume must be repositioned/resized via bounds.'], whenNotToUse: ['Only the extent must change; use set_volume_extent.'],
    inputProps: { volumeName: P.volumeName, bounds: P.bounds, save: P.save },
    required: ['volumeName', 'bounds'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_volume_bounds', volumeName: 'PP_01', bounds: { origin: { x: 0, y: 0, z: 0 }, extent: { x: 1000, y: 1000, z: 500 } } },
    exampleOutput: { success: true, message: 'Volume bounds set' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_properties', dispatchAction: 'set_volume_properties',
    family: F, summary: 'Set generic properties (enabled, blend, priority) on an existing volume.',
    whenToUse: ['Volume properties must be updated without recreating it.'], whenNotToUse: ['The volume extent must change; use set_volume_extent.'],
    inputProps: { volumeName: P.volumeName, bEnabled: P.bEnabled, priority: P.priority, blendWeight: P.blendWeight, save: P.save },
    required: ['volumeName'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_volume_properties', volumeName: 'PP_01', bEnabled: true },
    exampleOutput: { success: true, message: 'Volume properties set' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'remove_volume', dispatchAction: 'remove_volume',
    family: F, summary: 'Remove a volume actor from the level.',
    whenToUse: ['A volume actor must be permanently removed.'], whenNotToUse: ['The volume should be disabled; use set_volume_properties.'],
    inputProps: { volumeName: P.volumeName, save: P.save },
    required: ['volumeName'], effect: 'destructive', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'remove_volume', volumeName: 'PP_01' },
    exampleOutput: { success: true, message: 'Volume removed' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'get_volumes_info', dispatchAction: 'get_volumes_info',
    family: F, summary: 'List volumes in the level, optionally filtered by type.',
    whenToUse: ['The set of volumes must be enumerated or inspected.'], whenNotToUse: ['A single volume must be resized; use set_volume_extent.'],
    inputProps: { filter: P.filter, volumeType: P.volumeType },
    required: [], effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_volumes_info', filter: 'Trigger' },
    exampleOutput: { success: true, message: 'Volumes listed', volumes: ['/Game/Maps/Demo.Trigger_01'] },
    outputProps: { volumes: { type: 'array', items: { type: 'string' }, description: 'Volume actor paths.' } },
    normalizationRationale: NR,
  }),
];
