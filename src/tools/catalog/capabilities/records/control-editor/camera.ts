/**
 * Camera and view-target records: set_view_target, set_game_view_target,
 * set_camera, set_camera_position, set_viewport_camera, set_camera_fov.
 *
 * Grounded in src/tools/handlers/editor/editor-viewport-actions.ts.
 * set_game_view_target aliases to set_view_target; set_camera_position and
 * set_viewport_camera alias to set_camera. All are read-effect viewport
 * navigation operations (idempotent, safe to retry, no undo).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const F = 'camera';
const D = 'editor';
const NR = 'Distinct control_editor viewport camera operation with unique view semantics.';

export const CAMERA_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_view_target', domain: D, family: F,
    summary: 'Set the PIE view target to a specific actor by name.',
    whenToUse: ['The camera must follow or focus on a specific actor in PIE.'],
    whenNotToUse: ['A fixed camera position is preferred (use set_camera).'],
    inputProps: { actorName: P.actorName, name: P.name, objectPath: P.objectPath, location: P.location, rotation: P.rotation, blendTime: P.blendTime },
    required: ['actorName'],
    effect: 'read',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_view_target', actorName: 'BP_CameraTarget' },
    exampleOutput: { success: true, message: 'View target set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
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
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_game_view_target', actorName: 'BP_CameraTarget' },
    exampleOutput: { success: true, message: 'View target set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes set_game_view_target to set_view_target for handler routing; bridge dispatches set_view_target.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_camera', domain: D, family: F,
    summary: 'Set the editor viewport camera position and rotation.',
    whenToUse: ['The viewport camera must be moved to a specific position and orientation.'],
    whenNotToUse: ['The camera should follow an actor (use set_view_target).'],
    inputProps: { location: P.location, rotation: P.rotation, actorName: P.actorName },
    required: ['location', 'rotation'],
    effect: 'read',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_camera', location: { x: 0, y: 0, z: 500 }, rotation: { pitch: -45, yaw: 0, roll: 0 } },
    exampleOutput: { success: true, message: 'Camera set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_camera_position', dispatchAction: 'set_camera',
    domain: D, family: F,
    summary: 'Set the editor viewport camera (alias for set_camera).',
    whenToUse: ['The viewport camera must be moved using the set_camera_position alias.'],
    whenNotToUse: ['The camera should follow an actor (use set_view_target).'],
    inputProps: { location: P.location, rotation: P.rotation, actorName: P.actorName },
    required: ['location', 'rotation'],
    effect: 'read',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_camera_position', location: { x: 100, y: 200, z: 300 }, rotation: { pitch: 0, yaw: 90, roll: 0 } },
    exampleOutput: { success: true, message: 'Camera set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes set_camera_position to set_camera for handler routing; bridge dispatches set_camera.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_viewport_camera', dispatchAction: 'set_camera',
    domain: D, family: F,
    summary: 'Set the editor viewport camera (alias for set_camera).',
    whenToUse: ['The viewport camera must be moved using the set_viewport_camera alias.'],
    whenNotToUse: ['The camera should follow an actor (use set_view_target).'],
    inputProps: { location: P.location, rotation: P.rotation, actorName: P.actorName },
    required: ['location', 'rotation'],
    effect: 'read',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_viewport_camera', location: { x: 0, y: 0, z: 100 }, rotation: { pitch: -90, yaw: 0, roll: 0 } },
    exampleOutput: { success: true, message: 'Camera set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes set_viewport_camera to set_camera for handler routing; bridge dispatches set_camera.',
  }),
  buildCoreRecord({
    parentTool: 'control_editor', action: 'set_camera_fov', domain: D, family: F,
    summary: 'Set the editor viewport camera field of view.',
    whenToUse: ['The camera FOV must be adjusted for the viewport.'],
    whenNotToUse: ['The default FOV is acceptable.'],
    inputProps: { fov: P.fov },
    required: ['fov'],
    effect: 'read',
    costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_camera_fov', fov: 90 },
    exampleOutput: { success: true, message: 'FOV set to 90' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
