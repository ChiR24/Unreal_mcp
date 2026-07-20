/**
 * manage_character records — part 1 of 2 (creation, movement core). Grounded in
 * manage-character-tool.ts enum and native Character domain
 * (Plugins/.../Private/Domains/Character/). Character authoring mutates a
 * Character Blueprint asset in the editor (editorStates ['edit']).
 *
 * The TS handler validates only blueprintPath (plus name / modeName /
 * surfaceType where noted) and forwards the rest verbatim, so each record's
 * optional set is exactly the fields the matching native handler reads.
 * Action-specific parameters live in ./character.props.ts.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { CHARACTER_P as C } from './character.props.js';

const T = 'manage_character';
const F = 'character';
const W = ['A character Blueprint or its movement must be authored.'];

export const CHARACTER_1: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_character_blueprint`, action: 'create_character_blueprint', family: F,
    summary: 'Create a Character Blueprint asset.', whenToUse: W, whenNotToUse: ['A Pawn suffices.'],
    inputProps: { action: P.action, name: P.name, path: P.path, parentClass: P.string_, skeletalMeshPath: P.skeletalMeshPath }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { blueprintPath: P.blueprintPath }, outputRequired: ['blueprintPath'],
    exampleInput: { action: 'create_character_blueprint', name: 'BP_Char', parentClass: 'Character' }, exampleOutput: { success: true, message: 'Character Blueprint created' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_capsule_component`, action: 'configure_capsule_component', family: F,
    summary: 'Configure the capsule collision component.', whenToUse: ['Capsule size must change.'], whenNotToUse: ['Use configure_mesh_component.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, capsuleRadius: C.capsuleRadius, capsuleHalfHeight: C.capsuleHalfHeight }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_capsule_component', blueprintPath: '/Game/BP_Char', capsuleRadius: 42, capsuleHalfHeight: 96 }, exampleOutput: { success: true, message: 'Capsule configured' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_mesh_component`, action: 'configure_mesh_component', family: F,
    summary: 'Configure the skeletal mesh component.', whenToUse: ['Mesh must change.'], whenNotToUse: ['Use configure_capsule_component.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, skeletalMeshPath: P.skeletalMeshPath, animBlueprintPath: C.animBlueprintPath, meshOffset: C.meshOffset, meshRotation: C.meshRotation }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_mesh_component', blueprintPath: '/Game/BP_Char', skeletalMeshPath: '/Game/SM_Char', meshOffset: { x: 0, y: 0, z: -96 } }, exampleOutput: { success: true, message: 'Mesh component configured' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_camera_component`, action: 'configure_camera_component', family: F,
    summary: 'Configure the camera/spring-arm component.', whenToUse: ['Camera setup needed.'], whenNotToUse: ['Use configure_mesh_component.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, springArmLength: C.springArmLength, springArmLagEnabled: C.springArmLagEnabled, springArmLagSpeed: C.springArmLagSpeed, cameraUsePawnControlRotation: C.cameraUsePawnControlRotation }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_camera_component', blueprintPath: '/Game/BP_Char', springArmLength: 300, cameraUsePawnControlRotation: true }, exampleOutput: { success: true, message: 'Camera component configured' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_movement_speeds`, action: 'configure_movement_speeds', family: F,
    summary: 'Configure walk/run/sprint speeds.', whenToUse: ['Speeds must change.'], whenNotToUse: ['Use set_walk_speed.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, walkSpeed: C.walkSpeed, runSpeed: C.runSpeed, crouchSpeed: C.crouchSpeed, swimSpeed: C.swimSpeed, flySpeed: C.flySpeed, acceleration: C.acceleration, deceleration: C.deceleration, groundFriction: C.groundFriction }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_movement_speeds', blueprintPath: '/Game/BP_Char', walkSpeed: 600, crouchSpeed: 300 }, exampleOutput: { success: true, message: 'Movement speeds configured' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_jump`, action: 'configure_jump', family: F,
    summary: 'Configure jump velocity/z.,', whenToUse: ['Jump must change.'], whenNotToUse: ['Use set_jump_height.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, jumpHeight: C.jumpHeight, jumpHoldTime: C.jumpHoldTime, maxJumpCount: C.maxJumpCount, airControl: C.airControl, gravityScale: C.gravityScale, fallingLateralFriction: C.fallingLateralFriction }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_jump', blueprintPath: '/Game/BP_Char', jumpHeight: 600, maxJumpCount: 2 }, exampleOutput: { success: true, message: 'Jump configured' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_rotation`, action: 'configure_rotation', family: F,
    summary: 'Configure rotation / use-controller-rotation flags.', whenToUse: ['Rotation policy must change.'], whenNotToUse: ['Use configure_movement_speeds.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, orientToMovement: C.orientToMovement, rotationRate: C.rotationRate, useControllerRotationYaw: C.useControllerRotationYaw, useControllerRotationPitch: C.useControllerRotationPitch, useControllerRotationRoll: C.useControllerRotationRoll }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_rotation', blueprintPath: '/Game/BP_Char', orientToMovement: true, rotationRate: 540 }, exampleOutput: { success: true, message: 'Rotation configured' } }),
  buildRecord({ parentTool: T, id: `${T}.add_custom_movement_mode`, action: 'add_custom_movement_mode', family: F,
    summary: 'Add a custom movement mode to the character.', whenToUse: ['Custom locomotion state needed.'], whenNotToUse: ['Use setup_sliding.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, modeName: C.modeName, modeId: C.modeId, customSpeed: C.customSpeed }, required: ['action', 'blueprintPath', 'modeName'],
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'add_custom_movement_mode', blueprintPath: '/Game/BP_Char', modeName: 'Hover', modeId: 1, customSpeed: 600 }, exampleOutput: { success: true, message: 'Custom movement mode added' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_nav_movement`, action: 'configure_nav_movement', family: F,
    summary: 'Configure AI-navigation movement on the character.', whenToUse: ['AI movement needed.'], whenNotToUse: ['Use configure_movement_speeds.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, navAgentRadius: C.navAgentRadius, navAgentHeight: C.navAgentHeight, avoidanceEnabled: C.avoidanceEnabled }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_nav_movement', blueprintPath: '/Game/BP_Char', navAgentRadius: 42, avoidanceEnabled: true }, exampleOutput: { success: true, message: 'Nav movement configured' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_movement`, action: 'setup_movement', family: F,
    summary: 'Set up the base character movement component.', whenToUse: ['Movement component must initialize.'], whenNotToUse: ['Use configure_movement_speeds.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, walkSpeed: C.walkSpeed, runSpeed: C.runSpeed, acceleration: C.acceleration }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_movement', blueprintPath: '/Game/BP_Char', walkSpeed: 600, acceleration: 2048 }, exampleOutput: { success: true, message: 'Movement set up' } }),
  buildRecord({ parentTool: T, id: `${T}.set_walk_speed`, action: 'set_walk_speed', family: F,
    summary: 'Set the character walk speed (authoring).', whenToUse: ['Walk speed must change.'], whenNotToUse: ['Use configure_movement_speeds.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, walkSpeed: C.walkSpeed }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_walk_speed', blueprintPath: '/Game/BP_Char', walkSpeed: 300 }, exampleOutput: { success: true, message: 'Walk speed set' } }),
  buildRecord({ parentTool: T, id: `${T}.set_jump_height`, action: 'set_jump_height', family: F,
    summary: 'Set the character jump height (authoring).', whenToUse: ['Jump height must change.'], whenNotToUse: ['Use configure_jump.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, jumpHeight: C.jumpHeight }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_jump_height', blueprintPath: '/Game/BP_Char', jumpHeight: 600 }, exampleOutput: { success: true, message: 'Jump height set' } }),
  buildRecord({ parentTool: T, id: `${T}.set_gravity_scale`, action: 'set_gravity_scale', family: F,
    summary: 'Set gravity scale (authoring).', whenToUse: ['Gravity must change.'], whenNotToUse: ['Use set_walk_speed.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, gravityScale: C.gravityScale }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_gravity_scale', blueprintPath: '/Game/BP_Char', gravityScale: 1 }, exampleOutput: { success: true, message: 'Gravity scale set' } }),
  buildRecord({ parentTool: T, id: `${T}.set_ground_friction`, action: 'set_ground_friction', family: F,
    summary: 'Set ground friction (authoring).', whenToUse: ['Friction must change.'], whenNotToUse: ['Use set_braking_deceleration.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, groundFriction: C.groundFriction }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_ground_friction', blueprintPath: '/Game/BP_Char', groundFriction: 8 }, exampleOutput: { success: true, message: 'Ground friction set' } }),
  buildRecord({ parentTool: T, id: `${T}.set_braking_deceleration`, action: 'set_braking_deceleration', family: F,
    summary: 'Set braking deceleration (authoring).', whenToUse: ['Decel must change.'], whenNotToUse: ['Use set_ground_friction.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, brakingDeceleration: C.brakingDeceleration }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_braking_deceleration', blueprintPath: '/Game/BP_Char', brakingDeceleration: 2048 }, exampleOutput: { success: true, message: 'Braking deceleration set' } }),
];
