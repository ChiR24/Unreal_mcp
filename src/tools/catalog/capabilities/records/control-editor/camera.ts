/**
 * Camera and view-target records: set_view_target, set_game_view_target,
 * set_camera, set_camera_position, set_viewport_camera, set_camera_fov.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'camera';
const D = 'editor';

export const CAMERA_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_view_target', domain: D, family: F,
    summary: 'Set the PIE view target to a specific actor by name.',
    whenToUse: ['The camera must follow or focus on a specific actor in PIE.'],
    whenNotToUse: ['A fixed camera position is preferred (use set_camera).'],
    inputProps: { actorName: P.actorName, name: P.name, objectPath: P.objectPath, location: P.location, rotation: P.rotation, blendTime: P.blendTime },
    required: ['actorName'],
    effect: 'read',
   
    exampleInput: { action: 'set_view_target', actorName: 'BP_CameraTarget' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_game_view_target', dispatchAction: 'set_view_target',
    domain: D, family: F,
    summary: 'Set the PIE view target (alias for set_view_target).',
    whenToUse: ['The camera view target must be set using the set_game_view_target alias.'],
    whenNotToUse: ['A fixed camera position is preferred (use set_camera).'],
    inputProps: { actorName: P.actorName, name: P.name, objectPath: P.objectPath, location: P.location, rotation: P.rotation, blendTime: P.blendTime },
    required: ['actorName'],
    effect: 'read',
   
    exampleInput: { action: 'set_game_view_target', actorName: 'BP_CameraTarget' },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_camera', domain: D, family: F,
    summary: 'Set the editor viewport camera position and rotation.',
    whenToUse: ['The viewport camera must be moved to a specific position and orientation.'],
    whenNotToUse: ['The camera should follow an actor (use set_view_target).'],
    inputProps: { location: P.location, rotation: P.rotation },
    required: ['location', 'rotation'],
    effect: 'read',
   
    exampleInput: { action: 'set_camera', location: { x: 0, y: 0, z: 500 }, rotation: { pitch: -45, yaw: 0, roll: 0 } },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_camera_fov', domain: D, family: F,
    summary: 'Set the editor viewport camera field of view.',
    whenToUse: ['The camera FOV must be adjusted for the viewport.'],
    whenNotToUse: ['The default FOV is acceptable.'],
    inputProps: { fov: P.fov },
    required: ['fov'],
    effect: 'read',
   
    exampleInput: { action: 'set_camera_fov', fov: 90 },
  }),
];
