/**
 * Level-structure volume management (5 of the 6 volume records): extent,
 * bounds, properties, removal and listing of existing volumes. Creation of
 * every volume class is the single `create_volume` record in volume-a.
 * Grounded in HandleManageVolumesAction
 * (Domains/Volume/McpAutomationBridge_VolumeHandlers.cpp). All are
 * editor-state 'edit'.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'volume';
const NR = 'Distinct manage_level_structure volume verb and target; no cross-tool duplicate.';

export const LEVEL_VOLUME_B_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_extent', dispatchAction: 'set_volume_extent',
    family: F, summary: 'Set the extent (half-size) of an existing volume.',
    whenToUse: ['A volume must be resized.'], whenNotToUse: ['A new volume must be created; use create_volume.'],
    inputProps: { volumeName: P.volumeName, extent: P.extent, save: P.save },
    required: ['volumeName', 'extent'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_volume_extent', volumeName: 'PP_01', extent: { x: 1000, y: 1000, z: 500 } },
    exampleOutput: { success: true, message: 'Volume extent set' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_bounds', dispatchAction: 'set_volume_bounds',
    family: F, summary: 'Set the bounds (min/max) of an existing volume.',
    whenToUse: ['A volume must be repositioned/resized via min/max bounds.'], whenNotToUse: ['Only the extent must change; use set_volume_extent.'],
    inputProps: { volumeName: P.volumeName, bounds: P.boundsArray, save: P.save },
    required: ['volumeName', 'bounds'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_volume_bounds', volumeName: 'PP_01', bounds: [-1000, -1000, -500, 1000, 1000, 500] },
    exampleOutput: { success: true, message: 'Volume bounds set' }, normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_level_structure', action: 'set_volume_properties', dispatchAction: 'set_volume_properties',
    family: F, summary: 'Set generic properties (enabled, blend, priority) on an existing volume.',
    whenToUse: ['Volume properties must be updated without recreating it.'], whenNotToUse: ['The volume extent must change; use set_volume_extent.'],
    inputProps: { volumeName: P.volumeName, bEnabled: P.bEnabled, priority: P.priority, blendWeight: P.blendWeight, bWaterVolume: P.bWaterVolume, fluidFriction: P.fluidFriction, terminalVelocity: P.terminalVelocity, save: P.save },
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
