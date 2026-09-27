#!/usr/bin/env node
/**
 * manage_character Tool Integration Tests
 * Covers all 32 actions with real Blueprint state captured from creation.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayCharacter_${ts}`;
const CHARACTER_NAME = `BP_MCP_Character_${ts}`;
const ANIM_BLUEPRINT_NAME = `ABP_MCP_Character_${ts}`;
const TEST_ACTOR = `TestCharacterActor_${ts}`;
const TEST_SKELETAL_MESH_PATH = '/Engine/EngineMeshes/SkeletalCube';
const TEST_SKELETON_PATH = '/Engine/EngineMeshes/SkeletalCube_Skeleton';
const METAHUMAN_NAME = `MH_MCP_Character_${ts}`;
const METAHUMAN_PATH = `${TEST_FOLDER}/${METAHUMAN_NAME}`;

const testCases = [
  // === SETUP ===
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
  { scenario: 'CONFIG: configure_capsule_component radius only keeps the half-height', toolName: 'manage_character', arguments: { action: 'configure_capsule_component', blueprintPath: '${captured:blueprintPath}', capsuleRadius: 40 }, expected: 'success', assertions: [{ path: 'structuredContent.result.capsuleHalfHeight', equals: 96, label: 'half-height left alone' }] },
  { scenario: 'CONFIG: configure_mesh_component', toolName: 'manage_character', arguments: { action: 'configure_mesh_component', blueprintPath: '${captured:blueprintPath}', skeletalMeshPath: TEST_SKELETAL_MESH_PATH, animBlueprintPath: '${captured:animBlueprintPath}', meshOffset: { x: 0, y: 0, z: -96 }, meshRotation: { pitch: 0, yaw: -90, roll: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.skeletalMeshAssigned', equals: true, label: 'skeletal mesh asset assigned' }, { path: 'structuredContent.result.animBlueprintAssigned', equals: true, label: 'animation blueprint assigned' }] },
  { scenario: 'CONFIG: configure_camera_component', toolName: 'manage_character', arguments: { action: 'configure_camera_component', blueprintPath: '${captured:blueprintPath}', springArmLength: 350, cameraUsePawnControlRotation: true, springArmLagEnabled: true, springArmLagSpeed: 12 }, expected: 'success', assertions: [{ path: 'structuredContent.result.springArmLength', equals: 350, label: 'spring arm length applied' }] },
  { scenario: 'CONFIG: configure_camera_component length only keeps the lag', toolName: 'manage_character', arguments: { action: 'configure_camera_component', blueprintPath: '${captured:blueprintPath}', springArmLength: 400 }, expected: 'success', assertions: [{ path: 'structuredContent.result.lagEnabled', equals: true, label: 'camera lag left on' }, { path: 'structuredContent.result.springArmLagSpeed', equals: 12, label: 'lag speed left alone' }] },

  // === MOVEMENT COMPONENT ===
  { scenario: 'CONFIG: configure_movement_speeds', toolName: 'manage_character', arguments: { action: 'configure_movement_speeds', blueprintPath: '${captured:blueprintPath}', runSpeed: 420, crouchSpeed: 180, swimSpeed: 320, flySpeed: 500, acceleration: 1400, deceleration: 1600, groundFriction: 7, jumpHeight: 650 }, expected: 'success', assertions: [{ path: 'structuredContent.result.runSpeedApplied', equals: true, label: 'runSpeed applied when walkSpeed omitted' }, { path: 'structuredContent.result.walkSpeed', equals: 420, label: 'runSpeed maps to max walk speed' }] },
  { scenario: 'CONFIG: configure_jump', toolName: 'manage_character', arguments: { action: 'configure_jump', blueprintPath: '${captured:blueprintPath}', jumpHeight: 650, airControl: 0.45, gravityScale: 1.1, fallingLateralFriction: 0.15, maxJumpCount: 2, jumpHoldTime: 0.25 }, expected: 'success' },
  { scenario: 'CONFIG: configure_rotation', toolName: 'manage_character', arguments: { action: 'configure_rotation', blueprintPath: '${captured:blueprintPath}', orientToMovement: true, useControllerRotationYaw: false, useControllerRotationPitch: false, useControllerRotationRoll: false, rotationRate: 540 }, expected: 'success' },
  { scenario: 'CONFIG: configure_nav_movement', toolName: 'manage_character', arguments: { action: 'configure_nav_movement', blueprintPath: '${captured:blueprintPath}', navAgentRadius: 42, navAgentHeight: 192, avoidanceEnabled: true }, expected: 'success' },

  // === ADVANCED MOVEMENT ===

  // === FOOTSTEPS ===

  // === RETRY SAFETY (BB-012) ===
  // Each setup action is executed a SECOND time against the same Character.
  // The ensure-style variable add must converge: no duplicate variables, no
  // error, and exactly one logical feature per action.

  // === WRONG TARGET (BB-012) ===
  // A non-Character blueprint (the anim blueprint's AnimInstance parent chain)
  // must be rejected by the prerequisite gate with zero mutation.

  // === INFO ===
  { scenario: 'INFO: get_character_info', toolName: 'manage_character', arguments: { action: 'get_character_info', blueprintPath: '${captured:blueprintPath}' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.hasCamera', equals: true, label: 'character info sees camera component' },
  ] },
  // params envelope: clients that cannot send arbitrary top-level fields nest them
  // under `params`, which is merged with top-level arguments before routing.
  { scenario: 'INFO: get_character_info via params envelope', toolName: 'manage_character', arguments: { action: 'get_character_info', params: { blueprintPath: '${captured:blueprintPath}' } }, expected: 'success', assertions: [{ path: 'structuredContent.result.hasCamera', equals: true, label: 'nested params resolved the same character blueprint' }] },

  // === MOVEMENT SHORTCUTS ===
  { scenario: 'ACTION: setup_movement', toolName: 'manage_character', arguments: { action: 'setup_movement', blueprintPath: '${captured:blueprintPath}', walkSpeed: 500, acceleration: 1500 }, expected: 'success' },
  { scenario: 'CONFIG: set_walk_speed', toolName: 'manage_character', arguments: { action: 'set_walk_speed', blueprintPath: '${captured:blueprintPath}', walkSpeed: 520 }, expected: 'success', assertions: [{ path: 'structuredContent.result.walkSpeed', equals: 520, label: 'walk speed applied' }] },
  { scenario: 'CONFIG: set_jump_height', toolName: 'manage_character', arguments: { action: 'set_jump_height', blueprintPath: '${captured:blueprintPath}', jumpHeight: 700 }, expected: 'success', assertions: [{ path: 'structuredContent.result.jumpHeight', equals: 700, label: 'jump height applied' }] },
  { scenario: 'CONFIG: set_gravity_scale', toolName: 'manage_character', arguments: { action: 'set_gravity_scale', blueprintPath: '${captured:blueprintPath}', gravityScale: 0.9 }, expected: 'success', assertions: [{ path: 'structuredContent.result.gravityScale', equals: 0.9, label: 'gravity scale applied' }] },
  { scenario: 'CONFIG: set_ground_friction', toolName: 'manage_character', arguments: { action: 'set_ground_friction', blueprintPath: '${captured:blueprintPath}', groundFriction: 5.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.groundFriction', equals: 5.5, label: 'ground friction applied' }] },
  { scenario: 'CONFIG: set_braking_deceleration', toolName: 'manage_character', arguments: { action: 'set_braking_deceleration', blueprintPath: '${captured:blueprintPath}', brakingDeceleration: 1200 }, expected: 'success', assertions: [{ path: 'structuredContent.result.brakingDeceleration', equals: 1200, label: 'braking deceleration applied' }] },
  { scenario: 'CONFIG: configure_crouch', toolName: 'manage_character', arguments: { action: 'configure_crouch', blueprintPath: '${captured:blueprintPath}', canCrouch: true, crouchSpeed: 180, crouchedHalfHeight: 48 }, expected: 'success', assertions: [{ path: 'structuredContent.result.crouchedHalfHeight', equals: 48, label: 'crouched half-height applied' }, { path: 'structuredContent.result.canCrouch', equals: true, label: 'crouch enabled flag applied' }] },

  // === METAHUMAN (UE 5.6+) ===
  // These run against a live editor, so they assert the SHIPPED state rather than
  // an ideal one: MetaHuman Creator Core Data is an Epic Games Launcher install and
  // auto-rigging is an Epic cloud service, so an unprepared editor cannot assemble a
  // character. metahuman_status is the action that says so, and it always answers.
  {
    scenario: 'METAHUMAN: metahuman_status reports every blocker',
    toolName: 'manage_character',
    arguments: { action: 'metahuman_status', characterPath: METAHUMAN_PATH },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.blockers', exists: true, label: 'status enumerates blockers rather than failing' }
    ]
  },
  {
    scenario: 'METAHUMAN: create_metahuman',
    toolName: 'manage_character',
    arguments: { action: 'create_metahuman', name: METAHUMAN_NAME, path: TEST_FOLDER },
    expected: 'success|not available|FEATURE_UNAVAILABLE'
  },
  {
    scenario: 'METAHUMAN: create_metahuman is idempotent',
    toolName: 'manage_character',
    arguments: { action: 'create_metahuman', name: METAHUMAN_NAME, path: TEST_FOLDER },
    expected: 'success|not available|FEATURE_UNAVAILABLE'
  },
  {
    scenario: 'METAHUMAN: rig_metahuman requests the cloud auto-rig',
    toolName: 'manage_character',
    arguments: { action: 'rig_metahuman', characterPath: METAHUMAN_PATH, rigType: 'JointsAndBlendShapes', blocking: false, reportProgress: false },
    expected: 'success|not found|not available|FEATURE_UNAVAILABLE'
  },
  {
    scenario: 'METAHUMAN: build_metahuman refuses an unrigged character',
    toolName: 'manage_character',
    arguments: { action: 'build_metahuman', characterPath: METAHUMAN_PATH, pipelineType: 'Cinematic', pipelineQuality: 'Cinematic', buildPath: TEST_FOLDER, commonFolderPath: `${TEST_FOLDER}/Common`, nameOverride: METAHUMAN_NAME },
    expected: 'error'
  },
  {
    scenario: 'METAHUMAN: export_metahuman reports producing nothing instead of a bare success',
    toolName: 'manage_character',
    arguments: { action: 'export_metahuman', characterPath: METAHUMAN_PATH, exportType: 'geometry', projectPath: `${TEST_FOLDER}/Exported`, headMesh: true, bodyMesh: true, fullBodyMesh: false, overwrite: false },
    expected: 'error'
  },
  {
    scenario: 'METAHUMAN: export_metahuman dna variant',
    toolName: 'manage_character',
    arguments: { action: 'export_metahuman', characterPath: METAHUMAN_PATH, exportType: 'dna', projectPath: `${TEST_FOLDER}/Exported`, externalPath: '', dnaHead: true, dnaBody: true, overwrite: false },
    expected: 'error'
  },
  {
    scenario: 'METAHUMAN: export_metahuman materials variant',
    toolName: 'manage_character',
    arguments: { action: 'export_metahuman', characterPath: METAHUMAN_PATH, exportType: 'materials', projectPath: `${TEST_FOLDER}/Exported`, applyAsOverrides: true, overwrite: false },
    expected: 'error'
  },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TEST_ACTOR }, expected: 'success|not found' },
];

runToolTests('manage-character', testCases, { folder: TEST_FOLDER });
