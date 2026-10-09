/**
 * Camera and view-target records: set_view_target, set_game_view_target,
 * set_camera, set_camera_position, set_viewport_camera, set_camera_fov.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'camera';
const D = 'editor';

export const CAMERA_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_view_target', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
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
    editorStates: ANY_EDITOR_STATE,
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
    editorStates: ANY_EDITOR_STATE,
    summary: 'Set the viewport camera position and rotation: the level viewport, or, while Play In Editor runs with the player ejected (control_editor play control=eject), the free camera the game view is drawn from.',
    whenToUse: [
      'The viewport camera must be moved to a specific position and orientation.',
      'The view of a running game must be placed, for a screenshot of it: eject the player first, then set the camera.',
    ],
    whenNotToUse: [
      'The camera should follow an actor (use set_view_target).',
      'Play In Editor runs and the player is not ejected: the game draws its view target\'s camera (the pawn, or an actor set with view_target), which has no free camera to move, so the call is refused with PIE_VIEW_NOT_EJECTED (eject first, or use cameraOp view_target).',
    ],
    inputProps: { location: P.location, rotation: P.rotation },
    required: ['location', 'rotation'],
    effect: 'read',
    outputProps: {
      view: { type: 'string', description: 'Which view moved: editor_viewport (the level viewport), or pie_ejected (the free camera of an ejected player while Play In Editor runs).' },
      cameraLocation: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Where the camera is after the call, {x, y, z}.' },
      cameraRotation: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'How the camera is turned after the call, {pitch, yaw, roll}.' },
      locationApplied: { type: 'boolean', description: 'Whether the viewport took the requested location; false (with CAMERA_NOT_APPLIED) when it is locked to an actor, piloting or orthographic.' },
      rotationApplied: { type: 'boolean', description: 'Whether the viewport took the requested rotation.' },
    },
    exampleInput: { action: 'set_camera', location: { x: 0, y: 0, z: 500 }, rotation: { pitch: -45, yaw: 0, roll: 0 } },
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_camera_fov', domain: D, family: F,
    editorStates: ANY_EDITOR_STATE,
    summary: 'Set the camera field of view of the view on screen: the level viewport, the free view of an ejected player, or while the player plays the running game\'s camera (method player_camera_manager; locked to the value until play stops).',
    whenToUse: ['The camera FOV must be adjusted for the viewport.'],
    whenNotToUse: ['The default FOV is acceptable.'],
    inputProps: { fov: P.fov },
    required: ['fov'],
    effect: 'read',
   
    exampleInput: { action: 'set_camera_fov', fov: 90 },
  }),
];
