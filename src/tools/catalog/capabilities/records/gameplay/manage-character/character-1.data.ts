/**
 * manage_character records — part 1 of 2 (creation, movement core). Grounded in
 * manage-character-tool.ts enum and native Character domain
 * (Plugins/.../Private/Domains/Character/). Character authoring mutates a
 * Character Blueprint asset in the editor (editorStates ['edit']).
 */
import type { CapabilityRecordSource } from '../../../model.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { CHARACTER_P as C } from './character.props.js';

const T = 'manage_character';
const F = 'character';
const W = ['A character Blueprint or its movement must be authored.'];

export const CHARACTER_1: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_character_blueprint`, action: 'create_character_blueprint', family: F,
    topics: ['character blueprint', 'player character', 'new character', 'pawn', 'third person character'],
    summary: 'Create a Character Blueprint asset.', whenToUse: W, whenNotToUse: ['A Pawn suffices.'],
    inputProps: { name: P.name, path: P.path, parentClass: { type: 'string', description: 'Character class to derive from (default Character): a native Character subclass or a Character Blueprint path.' }, skeletalMeshPath: P.skeletalMeshPath }, required: ['name'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { blueprintPath: P.blueprintPath }, outputRequired: ['blueprintPath'],
    exampleInput: { action: 'create_character_blueprint', name: 'BP_Char', parentClass: 'Character' }, exampleOutput: { success: true, message: 'Character Blueprint created', blueprintPath: '/Game/BP_Char' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_capsule_component`, action: 'configure_capsule_component', family: F,
    summary: 'Configure the capsule collision component; only the sizes sent change.', whenToUse: ['Capsule size must change.'], whenNotToUse: ['Use configure_mesh_component.'],
    inputProps: { blueprintPath: P.blueprintPath, capsuleRadius: C.capsuleRadius, capsuleHalfHeight: C.capsuleHalfHeight }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_capsule_component', blueprintPath: '/Game/BP_Char', capsuleRadius: 42, capsuleHalfHeight: 96 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_mesh_component`, action: 'configure_mesh_component', family: F,
    summary: 'Configure the skeletal mesh component.', whenToUse: ['Mesh must change.'], whenNotToUse: ['Use configure_capsule_component.'],
    inputProps: { blueprintPath: P.blueprintPath, skeletalMeshPath: P.skeletalMeshPath, animBlueprintPath: C.animBlueprintPath, meshOffset: C.meshOffset, meshRotation: C.meshRotation }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_mesh_component', blueprintPath: '/Game/BP_Char', skeletalMeshPath: '/Game/SM_Char', meshOffset: { x: 0, y: 0, z: -96 } } }),
  buildRecord({ parentTool: T, id: `${T}.configure_camera_component`, action: 'configure_camera_component', family: F,
    summary: 'Configure the camera spring arm, creating a boom and camera when missing; an existing boom changes only in the fields sent.', whenToUse: ['Camera setup needed.'], whenNotToUse: ['Use configure_mesh_component.'],
    inputProps: { blueprintPath: P.blueprintPath, springArmLength: C.springArmLength, springArmLagEnabled: C.springArmLagEnabled, springArmLagSpeed: C.springArmLagSpeed, cameraUsePawnControlRotation: C.cameraUsePawnControlRotation }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_camera_component', blueprintPath: '/Game/BP_Char', springArmLength: 300, cameraUsePawnControlRotation: true } }),
  buildRecord({ parentTool: T, id: `${T}.configure_movement_speeds`, action: 'configure_movement_speeds', family: F,
    summary: 'Configure walk/run/sprint speeds.', whenToUse: ['Speeds must change.'], whenNotToUse: ['Use set_walk_speed.'],
    inputProps: { blueprintPath: P.blueprintPath, walkSpeed: C.walkSpeed, runSpeed: C.runSpeed, crouchSpeed: C.crouchSpeed, swimSpeed: C.swimSpeed, flySpeed: C.flySpeed, acceleration: C.acceleration, deceleration: C.deceleration, groundFriction: C.groundFriction, jumpHeight: C.jumpHeight, jumpHoldTime: C.jumpHoldTime, maxJumpCount: C.maxJumpCount, airControl: C.airControl, gravityScale: C.gravityScale, fallingLateralFriction: C.fallingLateralFriction }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_movement_speeds', blueprintPath: '/Game/BP_Char', walkSpeed: 600, crouchSpeed: 300 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_jump`, action: 'configure_jump', family: F,
    summary: 'Configure jump height, hold time, multi-jump, air control and gravity.', whenToUse: ['Jump must change.'], whenNotToUse: ['Use set_jump_height.'],
    inputProps: { blueprintPath: P.blueprintPath, jumpHeight: C.jumpHeight, jumpHoldTime: C.jumpHoldTime, maxJumpCount: C.maxJumpCount, airControl: C.airControl, gravityScale: C.gravityScale, fallingLateralFriction: C.fallingLateralFriction }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_jump', blueprintPath: '/Game/BP_Char', jumpHeight: 600, maxJumpCount: 2 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_rotation`, action: 'configure_rotation', family: F,
    summary: 'Configure rotation / use-controller-rotation flags.', whenToUse: ['Rotation policy must change.'], whenNotToUse: ['Use configure_movement_speeds.'],
    inputProps: { blueprintPath: P.blueprintPath, orientToMovement: C.orientToMovement, rotationRate: C.rotationRate, useControllerRotationYaw: C.useControllerRotationYaw, useControllerRotationPitch: C.useControllerRotationPitch, useControllerRotationRoll: C.useControllerRotationRoll }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_rotation', blueprintPath: '/Game/BP_Char', orientToMovement: true, rotationRate: 540 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_nav_movement`, action: 'configure_nav_movement', family: F,
    summary: 'Configure AI-navigation movement on the character.', whenToUse: ['AI movement needed.'], whenNotToUse: ['Use configure_movement_speeds.'],
    inputProps: { blueprintPath: P.blueprintPath, navAgentRadius: C.navAgentRadius, navAgentHeight: C.navAgentHeight, avoidanceEnabled: C.avoidanceEnabled }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_nav_movement', blueprintPath: '/Game/BP_Char', navAgentRadius: 42, avoidanceEnabled: true } }),
  buildRecord({ parentTool: T, id: `${T}.setup_movement`, action: 'setup_movement', family: F,
    summary: 'Set up the base character movement component.', whenToUse: ['Movement component must initialize.'], whenNotToUse: ['Use configure_movement_speeds.'],
    inputProps: { blueprintPath: P.blueprintPath, walkSpeed: C.walkSpeed, runSpeed: C.runSpeed, acceleration: C.acceleration }, required: ['blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'setup_movement', blueprintPath: '/Game/BP_Char', walkSpeed: 600, acceleration: 2048 } }),
  buildRecord({ parentTool: T, id: `${T}.set_walk_speed`, action: 'set_walk_speed', family: F,
    summary: 'Set the character walk speed (authoring).', whenToUse: ['Walk speed must change.'], whenNotToUse: ['Use configure_movement_speeds.'],
    inputProps: { blueprintPath: P.blueprintPath, walkSpeed: C.walkSpeed }, required: ['blueprintPath', 'walkSpeed'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_walk_speed', blueprintPath: '/Game/BP_Char', walkSpeed: 300 } }),
  buildRecord({ parentTool: T, id: `${T}.set_jump_height`, action: 'set_jump_height', family: F,
    summary: 'Set the character jump height (authoring).', whenToUse: ['Jump height must change.'], whenNotToUse: ['Use configure_jump.'],
    inputProps: { blueprintPath: P.blueprintPath, jumpHeight: C.jumpHeight }, required: ['blueprintPath', 'jumpHeight'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_jump_height', blueprintPath: '/Game/BP_Char', jumpHeight: 600 } }),
  buildRecord({ parentTool: T, id: `${T}.set_gravity_scale`, action: 'set_gravity_scale', family: F,
    summary: 'Set gravity scale (authoring).', whenToUse: ['Gravity must change.'], whenNotToUse: ['Use set_walk_speed.'],
    inputProps: { blueprintPath: P.blueprintPath, gravityScale: C.gravityScale }, required: ['blueprintPath', 'gravityScale'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_gravity_scale', blueprintPath: '/Game/BP_Char', gravityScale: 1 } }),
  buildRecord({ parentTool: T, id: `${T}.set_ground_friction`, action: 'set_ground_friction', family: F,
    summary: 'Set ground friction (authoring).', whenToUse: ['Friction must change.'], whenNotToUse: ['Use set_braking_deceleration.'],
    inputProps: { blueprintPath: P.blueprintPath, groundFriction: C.groundFriction }, required: ['blueprintPath', 'groundFriction'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_ground_friction', blueprintPath: '/Game/BP_Char', groundFriction: 8 } }),
  buildRecord({ parentTool: T, id: `${T}.set_braking_deceleration`, action: 'set_braking_deceleration', family: F,
    summary: 'Set braking deceleration (authoring).', whenToUse: ['Decel must change.'], whenNotToUse: ['Use set_ground_friction.'],
    inputProps: { blueprintPath: P.blueprintPath, brakingDeceleration: C.brakingDeceleration }, required: ['blueprintPath', 'brakingDeceleration'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_braking_deceleration', blueprintPath: '/Game/BP_Char', brakingDeceleration: 2048 } }),
];
