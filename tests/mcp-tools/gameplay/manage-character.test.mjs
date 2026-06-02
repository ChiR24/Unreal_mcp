#!/usr/bin/env node
/**
 * manage_character Tool Integration Tests
 * Covers all 27 actions with real Blueprint state captured from creation.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayCharacter_${ts}`;
const CHARACTER_NAME = `BP_MCP_Character_${ts}`;
const ANIM_BLUEPRINT_NAME = `ABP_MCP_Character_${ts}`;
const TEST_ACTOR = `TestCharacterActor_${ts}`;
const TEST_SKELETAL_MESH_PATH = '/Engine/EngineMeshes/SkeletalCube';
const TEST_SKELETON_PATH = '/Engine/EngineMeshes/SkeletalCube_Skeleton';

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  {
    scenario: 'Setup: create animation blueprint for mesh assignment',
    toolName: 'animation_physics',
    arguments: { action: 'create_anim_blueprint', name: ANIM_BLUEPRINT_NAME, path: TEST_FOLDER, skeletonPath: TEST_SKELETON_PATH, parentClass: 'AnimInstance' },
    expected: 'success|already exists',
    captureResult: { key: 'animBlueprintPath', fromField: 'data.result.assetPath' }
  },
  { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },

  // === CREATE ===
  {
    scenario: 'CREATE: create_character_blueprint',
    toolName: 'manage_character',
    arguments: { action: 'create_character_blueprint', name: CHARACTER_NAME, path: TEST_FOLDER, parentClass: 'Character' },
    expected: 'success',
    captureResult: { key: 'blueprintPath', fromField: 'result.assetPath' },
    assertions: [
      { path: 'structuredContent.result.parentClass', equals: 'Character', label: 'created blueprint uses Character parent' },
      { path: 'structuredContent.result.existsAfter', equals: true, label: 'character blueprint asset exists after creation' }
    ]
  },

  // === COMPONENT CONFIG ===
  { scenario: 'CONFIG: configure_capsule_component', toolName: 'manage_character', arguments: { action: 'configure_capsule_component', blueprintPath: '${captured:blueprintPath}', capsuleRadius: 44, capsuleHalfHeight: 96 }, expected: 'success', assertions: [{ path: 'structuredContent.result.capsuleRadius', equals: 44, label: 'capsule radius applied' }] },
  { scenario: 'CONFIG: configure_mesh_component', toolName: 'manage_character', arguments: { action: 'configure_mesh_component', blueprintPath: '${captured:blueprintPath}', skeletalMeshPath: TEST_SKELETAL_MESH_PATH, animBlueprintPath: '${captured:animBlueprintPath}', meshOffset: { x: 0, y: 0, z: -96 }, meshRotation: { pitch: 0, yaw: -90, roll: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.skeletalMeshAssigned', equals: true, label: 'skeletal mesh asset assigned' }, { path: 'structuredContent.result.animBlueprintAssigned', equals: true, label: 'animation blueprint assigned' }] },
  { scenario: 'CONFIG: configure_camera_component', toolName: 'manage_character', arguments: { action: 'configure_camera_component', blueprintPath: '${captured:blueprintPath}', springArmLength: 350, cameraUsePawnControlRotation: true, springArmLagEnabled: true, springArmLagSpeed: 12 }, expected: 'success', assertions: [{ path: 'structuredContent.result.springArmLength', equals: 350, label: 'spring arm length applied' }] },

  // === MOVEMENT COMPONENT ===
  { scenario: 'CONFIG: configure_movement_speeds', toolName: 'manage_character', arguments: { action: 'configure_movement_speeds', blueprintPath: '${captured:blueprintPath}', runSpeed: 420, crouchSpeed: 180, swimSpeed: 320, flySpeed: 500, acceleration: 1400, deceleration: 1600, groundFriction: 7 }, expected: 'success', assertions: [{ path: 'structuredContent.result.runSpeedApplied', equals: true, label: 'runSpeed applied when walkSpeed omitted' }, { path: 'structuredContent.result.walkSpeed', equals: 420, label: 'runSpeed maps to max walk speed' }] },
  { scenario: 'CONFIG: configure_jump', toolName: 'manage_character', arguments: { action: 'configure_jump', blueprintPath: '${captured:blueprintPath}', jumpHeight: 650, airControl: 0.45, gravityScale: 1.1, fallingLateralFriction: 0.15, maxJumpCount: 2, jumpHoldTime: 0.25 }, expected: 'success' },
  { scenario: 'CONFIG: configure_rotation', toolName: 'manage_character', arguments: { action: 'configure_rotation', blueprintPath: '${captured:blueprintPath}', orientToMovement: true, useControllerRotationYaw: false, useControllerRotationPitch: false, useControllerRotationRoll: false, rotationRate: 540 }, expected: 'success' },
  { scenario: 'ADD: add_custom_movement_mode', toolName: 'manage_character', arguments: { action: 'add_custom_movement_mode', blueprintPath: '${captured:blueprintPath}', modeName: `Dash_${ts}`, modeId: 3, customSpeed: 900 }, expected: 'success', assertions: [{ path: 'structuredContent.result.modeId', equals: 3, label: 'custom movement mode id applied' }] },
  { scenario: 'CONFIG: configure_nav_movement', toolName: 'manage_character', arguments: { action: 'configure_nav_movement', blueprintPath: '${captured:blueprintPath}', navAgentRadius: 42, navAgentHeight: 192, avoidanceEnabled: true }, expected: 'success' },

  // === ADVANCED MOVEMENT ===
  { scenario: 'ACTION: setup_mantling', toolName: 'manage_character', arguments: { action: 'setup_mantling', blueprintPath: '${captured:blueprintPath}', mantleHeight: 180, mantleReachDistance: 120 }, expected: 'success', assertions: [{ path: 'structuredContent.result.mantleHeight', equals: 180, label: 'mantle height applied' }] },
  { scenario: 'ACTION: setup_vaulting', toolName: 'manage_character', arguments: { action: 'setup_vaulting', blueprintPath: '${captured:blueprintPath}', vaultHeight: 120, vaultDepth: 200 }, expected: 'success', assertions: [{ path: 'structuredContent.result.vaultDepth', equals: 200, label: 'vault depth applied' }] },
  { scenario: 'ACTION: setup_climbing', toolName: 'manage_character', arguments: { action: 'setup_climbing', blueprintPath: '${captured:blueprintPath}', climbSpeed: 240, climbableTag: 'Climbable' }, expected: 'success', assertions: [{ path: 'structuredContent.result.climbSpeed', equals: 240, label: 'climb speed applied' }] },
  { scenario: 'ACTION: setup_sliding', toolName: 'manage_character', arguments: { action: 'setup_sliding', blueprintPath: '${captured:blueprintPath}', slideSpeed: 850, slideDuration: 1.1, slideCooldown: 0.6 }, expected: 'success', assertions: [{ path: 'structuredContent.result.slideSpeed', equals: 850, label: 'slide speed applied' }] },
  { scenario: 'ACTION: setup_wall_running', toolName: 'manage_character', arguments: { action: 'setup_wall_running', blueprintPath: '${captured:blueprintPath}', wallRunSpeed: 720, wallRunDuration: 1.8, wallRunGravityScale: 0.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.wallRunGravityScale', equals: 0.5, label: 'wall run gravity scale applied' }] },
  { scenario: 'ACTION: setup_grappling', toolName: 'manage_character', arguments: { action: 'setup_grappling', blueprintPath: '${captured:blueprintPath}', grappleRange: 1500, grappleSpeed: 1800, grappleTargetTag: 'GrappleTarget' }, expected: 'success', assertions: [{ path: 'structuredContent.result.grappleRange', equals: 1500, label: 'grapple range applied' }] },

  // === FOOTSTEPS ===
  { scenario: 'ACTION: setup_footstep_system', toolName: 'manage_character', arguments: { action: 'setup_footstep_system', blueprintPath: '${captured:blueprintPath}', footstepEnabled: true, footstepSocketLeft: 'foot_l', footstepSocketRight: 'foot_r', footstepTraceDistance: 75 }, expected: 'success', assertions: [{ path: 'structuredContent.result.traceDistance', equals: 75, label: 'footstep trace distance applied' }] },
  { scenario: 'ACTION: map_surface_to_sound', toolName: 'manage_character', arguments: { action: 'map_surface_to_sound', blueprintPath: '${captured:blueprintPath}', surfaceType: 'Default' }, expected: 'success', assertions: [{ path: 'structuredContent.result.surfaceType', equals: 'Default', label: 'surface type mapped' }] },
  { scenario: 'CONFIG: configure_footstep_fx', toolName: 'manage_character', arguments: { action: 'configure_footstep_fx', blueprintPath: '${captured:blueprintPath}', volumeMultiplier: 0.8, particleScale: 1.25 }, expected: 'success', assertions: [{ path: 'structuredContent.result.particleScale', equals: 1.25, label: 'footstep particle scale applied' }] },

  // === INFO ===
  { scenario: 'INFO: get_character_info', toolName: 'manage_character', arguments: { action: 'get_character_info', blueprintPath: '${captured:blueprintPath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.hasCamera', equals: true, label: 'character info sees camera component' }] },

  // === MOVEMENT SHORTCUTS ===
  { scenario: 'ACTION: setup_movement', toolName: 'manage_character', arguments: { action: 'setup_movement', blueprintPath: '${captured:blueprintPath}', walkSpeed: 500, acceleration: 1500 }, expected: 'success' },
  { scenario: 'CONFIG: set_walk_speed', toolName: 'manage_character', arguments: { action: 'set_walk_speed', blueprintPath: '${captured:blueprintPath}', walkSpeed: 520 }, expected: 'success', assertions: [{ path: 'structuredContent.result.walkSpeed', equals: 520, label: 'walk speed applied' }] },
  { scenario: 'CONFIG: set_jump_height', toolName: 'manage_character', arguments: { action: 'set_jump_height', blueprintPath: '${captured:blueprintPath}', jumpHeight: 700 }, expected: 'success', assertions: [{ path: 'structuredContent.result.jumpHeight', equals: 700, label: 'jump height applied' }] },
  { scenario: 'CONFIG: set_gravity_scale', toolName: 'manage_character', arguments: { action: 'set_gravity_scale', blueprintPath: '${captured:blueprintPath}', gravityScale: 0.9 }, expected: 'success', assertions: [{ path: 'structuredContent.result.gravityScale', equals: 0.9, label: 'gravity scale applied' }] },
  { scenario: 'CONFIG: set_ground_friction', toolName: 'manage_character', arguments: { action: 'set_ground_friction', blueprintPath: '${captured:blueprintPath}', groundFriction: 5.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.groundFriction', equals: 5.5, label: 'ground friction applied' }] },
  { scenario: 'CONFIG: set_braking_deceleration', toolName: 'manage_character', arguments: { action: 'set_braking_deceleration', blueprintPath: '${captured:blueprintPath}', brakingDeceleration: 1200 }, expected: 'success', assertions: [{ path: 'structuredContent.result.brakingDeceleration', equals: 1200, label: 'braking deceleration applied' }] },
  { scenario: 'CONFIG: configure_crouch', toolName: 'manage_character', arguments: { action: 'configure_crouch', blueprintPath: '${captured:blueprintPath}', canCrouch: true, crouchSpeed: 180, crouchedHalfHeight: 48 }, expected: 'success', assertions: [{ path: 'structuredContent.result.crouchedHalfHeight', equals: 48, label: 'crouched half-height applied' }, { path: 'structuredContent.result.canCrouch', equals: true, label: 'crouch enabled flag applied' }] },
  { scenario: 'CONFIG: configure_sprint', toolName: 'manage_character', arguments: { action: 'configure_sprint', blueprintPath: '${captured:blueprintPath}', sprintSpeed: 850 }, expected: 'success', assertions: [{ path: 'structuredContent.result.sprintSpeed', equals: 850, label: 'sprint speed applied' }, { path: 'structuredContent.result.stateVariable', equals: 'bIsSprinting', label: 'sprint state variable created' }] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TEST_ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-character', testCases);
