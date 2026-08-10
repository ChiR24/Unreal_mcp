#!/usr/bin/env node
/**
 * animation_physics skeleton-promotion suite.
 *
 * Covers the eleven skeleton routes promoted from the hidden native dispatch
 * map (Private/Domains/Skeleton/McpAutomationBridge_SkeletonHandlers.cpp) onto
 * the canonical animation_physics surface.
 *
 * Every optional parameter each promoted record declares is referenced by at
 * least one case here: the parameter audit runs with --optional-strict, so a
 * declared optional that no case passes fails the gate. Where a handler accepts
 * either of two asset paths, both arms are exercised rather than one.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/SkeletonPromotion';
const ts = Date.now();
const SKELETON_PATH = `${TEST_FOLDER}/SK_Promo_${ts}`;
const PHYSICS_ASSET_PATH = `${TEST_FOLDER}/PA_Promo_${ts}`;
const MESH_PATH = '/Engine/EngineMeshes/SkeletalCube.SkeletalCube';
const SOCKET_NAME = 'PromoSocket';
const ROOT_BONE = 'Root';

const skel = (action, extra = {}) => ({ action, skeletonPath: SKELETON_PATH, ...extra });
const phys = (action, extra = {}) => ({ action, physicsAssetPath: PHYSICS_ASSET_PATH, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create test skeleton', toolName: 'animation_physics', arguments: { action: 'create_skeleton', path: SKELETON_PATH, rootBoneName: ROOT_BONE, save: true }, expected: 'success|already exists' },
  { scenario: 'Setup: create physics asset', toolName: 'animation_physics', arguments: { action: 'create_physics_asset', path: PHYSICS_ASSET_PATH, skeletalMeshPath: MESH_PATH, save: true }, expected: 'success|already exists|not found' },

  // === SOCKETS (alias spellings of create_socket / configure_socket) ===
  { scenario: 'SOCKET: add_socket with full transform', toolName: 'animation_physics', arguments: skel('add_socket', { socketName: SOCKET_NAME, attachBoneName: ROOT_BONE, relativeLocation: [1, 2, 3], relativeRotation: [0, 0, 0], relativeScale: [1, 1, 1], save: true }), expected: 'success|already exists' },
  { scenario: 'SOCKET: add_socket with required fields only', toolName: 'animation_physics', arguments: skel('add_socket', { socketName: `${SOCKET_NAME}_Min` }), expected: 'success|already exists' },
  { scenario: 'SOCKET: modify_socket retargets the offset', toolName: 'animation_physics', arguments: skel('modify_socket', { socketName: SOCKET_NAME, attachBoneName: ROOT_BONE, relativeLocation: [4, 5, 6], relativeRotation: [0, 90, 0], relativeScale: [2, 2, 2], save: true }), expected: 'success|not found' },

  // === PHYSICS ASSET ===
  { scenario: 'PHYSICS: set_physics_asset via skeletalMeshPath', toolName: 'animation_physics', arguments: phys('set_physics_asset', { skeletalMeshPath: MESH_PATH }), expected: 'success|not found' },
  { scenario: 'PHYSICS: set_physics_asset via meshPath alias', toolName: 'animation_physics', arguments: phys('set_physics_asset', { meshPath: MESH_PATH }), expected: 'success|not found' },
  { scenario: 'PHYSICS: modify_physics_body sets mass and damping', toolName: 'animation_physics', arguments: phys('modify_physics_body', { boneName: ROOT_BONE, mass: 5, linearDamping: 0.25, angularDamping: 0.1, collisionEnabled: true, simulatePhysics: false, save: true }), expected: 'success|not found' },
  { scenario: 'PHYSICS: set_physics_constraint joints two bodies', toolName: 'animation_physics', arguments: phys('set_physics_constraint', { bodyA: ROOT_BONE, bodyB: 'spine_01', constraintName: 'PromoSpine', save: true }), expected: 'success|not found' },
  { scenario: 'PHYSICS: remove_physics_body drops one body', toolName: 'animation_physics', arguments: phys('remove_physics_body', { boneName: 'spine_01' }), expected: 'success|not found' },
  { scenario: 'PHYSICS: get_physics_asset_info by asset path', toolName: 'animation_physics', arguments: phys('get_physics_asset_info'), expected: 'success|not found' },
  { scenario: 'PHYSICS: get_physics_asset_info by mesh path', toolName: 'animation_physics', arguments: { action: 'get_physics_asset_info', skeletalMeshPath: MESH_PATH }, expected: 'success|not found' },

  // === MORPH TARGETS ===
  { scenario: 'MORPH: list_morph_targets via skeletalMeshPath', toolName: 'animation_physics', arguments: { action: 'list_morph_targets', skeletalMeshPath: MESH_PATH }, expected: 'success|not found' },
  { scenario: 'MORPH: list_morph_targets via meshPath alias', toolName: 'animation_physics', arguments: { action: 'list_morph_targets', meshPath: MESH_PATH }, expected: 'success|not found' },
  { scenario: 'MORPH: set_morph_target_value on a live actor', toolName: 'animation_physics', arguments: { action: 'set_morph_target_value', actorName: 'PromoMorphActor', morphTargetName: 'Smile', value: 0.5, addMissing: true }, expected: 'success|not found' },

  // === BONES ===
  { scenario: 'BONE: get_bone_transform via skeletonPath', toolName: 'animation_physics', arguments: skel('get_bone_transform', { boneName: ROOT_BONE }), expected: 'success|not found' },
  { scenario: 'BONE: get_bone_transform via skeletalMeshPath', toolName: 'animation_physics', arguments: { action: 'get_bone_transform', skeletalMeshPath: MESH_PATH, boneName: ROOT_BONE }, expected: 'success|not found' },
  { scenario: 'BONE: list_virtual_bones via skeletonPath', toolName: 'animation_physics', arguments: skel('list_virtual_bones'), expected: 'success|not found' },
  { scenario: 'BONE: list_virtual_bones via skeletalMeshPath', toolName: 'animation_physics', arguments: { action: 'list_virtual_bones', skeletalMeshPath: MESH_PATH }, expected: 'success|not found' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found', timeoutMs: 30000 },
];

runToolTests('animation-physics-skeleton-promotion', testCases);
