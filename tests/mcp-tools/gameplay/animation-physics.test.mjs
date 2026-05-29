#!/usr/bin/env node
/**
 * animation_physics Tool Integration Tests
 * Covers all 55 actions with proper setup/teardown sequencing.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
const ts = Date.now();

// The skeleton created in setup - used by all CREATE actions that need a skeleton
const TEST_SKELETON_PATH = `${TEST_FOLDER}/SK_AnimPhys_${ts}`;
const TEST_IK_RIG_PATH = `${TEST_FOLDER}/Testik_rig`;
const TEST_IK_RETARGETER_PATH = `${TEST_FOLDER}/Testik_retargeter`;

const testCases = [
// === SETUP ===
{ scenario: 'Setup: delete stale test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'error|ASSET_NOT_FOUND|success|not found' },
{ scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
{ scenario: 'Setup: create test skeleton', toolName: 'animation_physics', arguments: { action: 'create_skeleton', path: TEST_SKELETON_PATH, rootBoneName: 'Root', save: true }, expected: 'success|already exists' },

// === CREATE (Animation Blueprints - need skeletonPath) ===
{ scenario: 'CREATE: create_animation_blueprint', toolName: 'animation_physics', arguments: {"action": "create_animation_blueprint", "name": "Testanimation_blueprint", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH, "parentClass": "Actor"}, expected: 'success|already exists' },
{ scenario: 'CREATE: create_animation_bp', toolName: 'animation_physics', arguments: {"action": "create_animation_bp", "name": "Testanimation_bp", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH, "parentClass": "Actor"}, expected: 'success|already exists' },
{ scenario: 'CREATE: create_anim_blueprint', toolName: 'animation_physics', arguments: {"action": "create_anim_blueprint", "name": "Testanim_blueprint", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH, "parentClass": "Actor"}, expected: 'success|already exists' },

// === CREATE (Blend Spaces - need skeletonPath) ===
{ scenario: 'CREATE: create_blend_space', toolName: 'animation_physics', arguments: {"action": "create_blend_space", "name": "Testblend_space", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },
{ scenario: 'CREATE: create_blend_space_1d', toolName: 'animation_physics', arguments: {"action": "create_blend_space_1d", "name": "Testblend_space_1d", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },
{ scenario: 'CREATE: create_blend_space_2d', toolName: 'animation_physics', arguments: {"action": "create_blend_space_2d", "name": "Testblend_space_2d", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },

  // === CREATE (Blend Tree - needs blueprintPath to existing AnimBP) ===
  // ANIMGRAPH_MODULE_UNAVAILABLE: primary intent since AnimGraph BlendTree headers may not be compiled in
  { scenario: 'CREATE: create_blend_tree', toolName: 'animation_physics', arguments: {"action": "create_blend_tree", "name": "Testblend_tree", "path": TEST_FOLDER, "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`}, expected: 'error|ANIMGRAPH_MODULE_UNAVAILABLE|success|already exists' },

// === CREATE (Procedural Anim - needs skeletonPath) ===
{ scenario: 'CREATE: create_procedural_anim', toolName: 'animation_physics', arguments: {"action": "create_procedural_anim", "name": "Testprocedural_anim", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH, "boneTracks": [{"boneName": "Root", "frames": [{"frame": 0}]}], "frameRate": 30}, expected: 'success|already exists' },

// === CREATE (Aim Offset - needs skeletonPath) ===
{ scenario: 'CREATE: create_aim_offset', toolName: 'animation_physics', arguments: {"action": "create_aim_offset", "name": "Testaim_offset", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },

// === ADD (Aim Offset Sample - needs assetPath to the aim offset) ===
{ scenario: 'ADD: add_aim_offset_sample', toolName: 'animation_physics', arguments: {"action": "add_aim_offset_sample", "assetPath": `${TEST_FOLDER}/Testaim_offset`, "animationPath": `${TEST_FOLDER}/Testprocedural_anim`, "yaw": 0, "pitch": 0}, expected: 'success|already exists' },

// === CREATE (State Machine - needs blueprintPath) ===
{ scenario: 'CREATE: create_state_machine', toolName: 'animation_physics', arguments: {"action": "create_state_machine", "machineName": "Teststate_machine", "path": TEST_FOLDER, "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`}, expected: 'success|already exists' },

// === ADD (State Machine - needs blueprintPath) ===
{ scenario: 'ADD: add_state_machine', toolName: 'animation_physics', arguments: {"action": "add_state_machine", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "stateMachineName": "Teststate_machine"}, expected: 'success|already exists' },

// === ADD (State - needs blueprintPath and stateMachineName) ===
{ scenario: 'ADD: add_state', toolName: 'animation_physics', arguments: {"action": "add_state", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "stateMachineName": "Teststate_machine", "stateName": "Teststate"}, expected: 'success|already exists' },

// === ADD (Transition - needs blueprintPath, stateMachineName, fromState, toState) ===
{ scenario: 'ADD: add_transition', toolName: 'animation_physics', arguments: {"action": "add_transition", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "stateMachineName": "Teststate_machine", "fromState": "Teststate", "toState": "Teststate"}, expected: 'success|already exists' },

// === CONFIG (Transition Rules - needs blueprintPath) ===
{ scenario: 'CONFIG: set_transition_rules', toolName: 'animation_physics', arguments: {"action": "set_transition_rules", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "stateMachineName": "Teststate_machine", "fromState": "Teststate", "toState": "Teststate", "blendTime": 0.2}, expected: 'success' },

// === ADD (Blend Node - needs blueprintPath) ===
{ scenario: 'ADD: add_blend_node', toolName: 'animation_physics', arguments: {"action": "add_blend_node", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "blendType": "TwoWayBlend", "nodeName": "Testblend_node"}, expected: 'success|already exists' },

// === ADD (Cached Pose - needs blueprintPath) ===
{ scenario: 'ADD: add_cached_pose', toolName: 'animation_physics', arguments: {"action": "add_cached_pose", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "cacheName": "Testcached_pose"}, expected: 'success|already exists' },

// === ADD (Slot Node - needs blueprintPath) ===
{ scenario: 'ADD: add_slot_node', toolName: 'animation_physics', arguments: {"action": "add_slot_node", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "slotName": "Testslot_node"}, expected: 'success|already exists' },
{ scenario: 'ADD: add_layered_blend_per_bone', toolName: 'animation_physics', arguments: {"action": "add_layered_blend_per_bone", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "layerSetup": [{"branchFilters": [{"boneName": "Root", "blendDepth": 1}]}]}, expected: 'error|ANIMGRAPH_MODULE_UNAVAILABLE|success' },
{ scenario: 'CONFIG: set_anim_graph_node_value', toolName: 'animation_physics', arguments: {"action": "set_anim_graph_node_value", "blueprintPath": `${TEST_FOLDER}/Testanimation_blueprint`, "nodeName": "Testblend_node", "propertyName": "NodeComment", "value": "MCPUpdatedBlendNode"}, expected: 'error|ANIMGRAPH_MODULE_UNAVAILABLE|NODE_NOT_FOUND|PROPERTY_NOT_FOUND|success' },

// === CREATE (Control Rig - needs skeletonPath) ===
{ scenario: 'CREATE: create_control_rig', toolName: 'animation_physics', arguments: {"action": "create_control_rig", "name": "Testcontrol_rig", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists|NOT_AVAILABLE' },

// === CREATE (IK Rig - explicit retargeter dependency; unavailable on some engine builds) ===
{ scenario: 'SETUP: create IK Rig for retargeter', toolName: 'animation_physics', arguments: {"action": "create_ik_rig", "name": "Testik_rig", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'error|IKRIG_FACTORY_UNAVAILABLE|NOT_SUPPORTED|success|already exists' },
{ scenario: 'CREATE: create_ik_retargeter', toolName: 'animation_physics', arguments: {"action": "create_ik_retargeter", "name": "Testik_retargeter", "path": TEST_FOLDER, "sourceIKRigPath": TEST_IK_RIG_PATH, "targetIKRigPath": TEST_IK_RIG_PATH, "save": true}, expected: 'error|IKRETARGET_FACTORY_UNAVAILABLE|NOT_SUPPORTED|success|already exists' },
{ scenario: 'CONFIG: set_retarget_chain_mapping', toolName: 'animation_physics', arguments: {"action": "set_retarget_chain_mapping", "assetPath": TEST_IK_RETARGETER_PATH, "sourceChain": "Root", "targetChain": "Root"}, expected: 'error|NOT_SUPPORTED|success' },

// === ACTION (Setup IK - needs name and skeletonPath) ===
{ scenario: 'ACTION: setup_ik', toolName: 'animation_physics', arguments: {"action": "setup_ik", "name": "TestIK", "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },

// === CREATE (Pose Library - needs skeletonPath) ===
{ scenario: 'CREATE: create_pose_library', toolName: 'animation_physics', arguments: {"action": "create_pose_library", "name": "Testpose_library", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },

// === CREATE (Animation Asset - needs skeletonPath) ===
{ scenario: 'CREATE: create_animation_asset', toolName: 'animation_physics', arguments: {"action": "create_animation_asset", "name": "Testanimation_asset", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },

// === CREATE (Animation Sequence - needs skeletonPath) ===
{ scenario: 'CREATE: create_animation_sequence', toolName: 'animation_physics', arguments: {"action": "create_animation_sequence", "name": "Testanimation_sequence", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },

// === CONFIG (Sequence Length - needs assetPath) ===
{ scenario: 'CONFIG: set_sequence_length', toolName: 'animation_physics', arguments: {"action": "set_sequence_length", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "numFrames": 60}, expected: 'success' },

// === ADD (Bone Track - needs assetPath and boneName) ===
{ scenario: 'ADD: add_bone_track', toolName: 'animation_physics', arguments: {"action": "add_bone_track", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "boneName": "Root"}, expected: 'success|already exists' },

// === CONFIG (Bone Key - needs assetPath) ===
{ scenario: 'CONFIG: set_bone_key', toolName: 'animation_physics', arguments: {"action": "set_bone_key", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "boneName": "Root", "frame": 0, "location": {"x": 0, "y": 0, "z": 0}}, expected: 'success' },

// === CONFIG (Curve Key - needs assetPath) ===
{ scenario: 'CONFIG: set_curve_key', toolName: 'animation_physics', arguments: {"action": "set_curve_key", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "curveName": "TestCurve", "frame": 0, "value": 1.0}, expected: 'success' },

// === CREATE (Montage - needs skeletonPath) ===
{ scenario: 'CREATE: create_montage', toolName: 'animation_physics', arguments: {"action": "create_montage", "name": "Testmontage", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH}, expected: 'success|already exists' },

// === ADD (Montage Section - needs assetPath) ===
{ scenario: 'ADD: add_montage_section', toolName: 'animation_physics', arguments: {"action": "add_montage_section", "assetPath": `${TEST_FOLDER}/Testmontage`, "sectionName": "Testmontage_section", "startTime": 0}, expected: 'success|already exists' },

// === ADD (Montage Slot - needs assetPath and animationPath) ===
{ scenario: 'ADD: add_montage_slot', toolName: 'animation_physics', arguments: {"action": "add_montage_slot", "assetPath": `${TEST_FOLDER}/Testmontage`, "animationPath": `${TEST_FOLDER}/Testanimation_sequence`, "slotName": "DefaultSlot", "startTime": 0}, expected: 'success|already exists' },

// === CONFIG (Section Timing - needs assetPath) ===
{ scenario: 'CONFIG: set_section_timing', toolName: 'animation_physics', arguments: {"action": "set_section_timing", "assetPath": `${TEST_FOLDER}/Testmontage`, "sectionName": "Testmontage_section", "startTime": 0, "length": 1.0}, expected: 'success' },

// === ADD (Montage Notify - needs assetPath; no notifyClass = simple notify event) ===
{ scenario: 'ADD: add_montage_notify', toolName: 'animation_physics', arguments: {"action": "add_montage_notify", "assetPath": `${TEST_FOLDER}/Testmontage`, "time": 0.5, "notifyName": "Testmontage_notify"}, expected: 'success|already exists' },

// === CONFIG (Blend In - needs assetPath) ===
{ scenario: 'CONFIG: set_blend_in', toolName: 'animation_physics', arguments: {"action": "set_blend_in", "assetPath": `${TEST_FOLDER}/Testmontage`, "blendTime": 0.25}, expected: 'success' },

// === CONFIG (Blend Out - needs assetPath) ===
{ scenario: 'CONFIG: set_blend_out', toolName: 'animation_physics', arguments: {"action": "set_blend_out", "assetPath": `${TEST_FOLDER}/Testmontage`, "blendTime": 0.25}, expected: 'success' },

// === CONNECT (Link Sections - needs assetPath) ===
{ scenario: 'CONNECT: link_sections', toolName: 'animation_physics', arguments: {"action": "link_sections", "assetPath": `${TEST_FOLDER}/Testmontage`, "fromSection": "Testmontage_section", "toSection": "Testmontage_section"}, expected: 'success' },

// === ADD (Notify - needs assetPath and notifyName; no notifyClass = simple notify) ===
{ scenario: 'ADD: add_notify', toolName: 'animation_physics', arguments: {"action": "add_notify", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "notifyName": "Testnotify", "frame": 0}, expected: 'success|already exists' },
{ scenario: 'ADD: add_notify_state', toolName: 'animation_physics', arguments: {"action": "add_notify_state", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "notifyName": "Testnotify_state", "startFrame": 1, "endFrame": 10, "trackIndex": 0, "save": true}, expected: 'success' },
{ scenario: 'ADD: add_sync_marker', toolName: 'animation_physics', arguments: {"action": "add_sync_marker", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "markerName": "Testsync_marker", "frame": 5, "save": true}, expected: 'success' },
{ scenario: 'CONFIG: set_root_motion_settings', toolName: 'animation_physics', arguments: {"action": "set_root_motion_settings", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "enableRootMotion": true, "rootMotionRootLock": "RefPose", "forceRootLock": false, "save": true}, expected: 'success' },
{ scenario: 'CONFIG: set_additive_settings', toolName: 'animation_physics', arguments: {"action": "set_additive_settings", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`, "additiveAnimType": "NoAdditive", "basePoseType": "RefPose", "basePoseFrame": 0, "save": true}, expected: 'success' },
{ scenario: 'INFO: get_animation_info', toolName: 'animation_physics', arguments: {"action": "get_animation_info", "assetPath": `${TEST_FOLDER}/Testanimation_sequence`}, expected: 'success' },

// === SETUP: Spawn a SkeletalMeshActor for playback/ragdoll tests ===
{ scenario: 'Setup: spawn SkeletalMeshActor', toolName: 'control_actor', arguments: {"action": "spawn", "actorName": "SkelTestActor", "actorClass": "SkeletalMeshActor"}, expected: 'success|already exists' },

// === PLAYBACK (Montage - needs actorName with SkeletalMeshComponent) ===
{ scenario: 'PLAYBACK: play_montage', toolName: 'animation_physics', arguments: {"action": "play_montage", "actorName": "SkelTestActor", "montagePath": `${TEST_FOLDER}/Testmontage`, "playRate": 1.25}, expected: 'success|COMPONENT_NOT_FOUND' },
{ scenario: 'PLAYBACK: play_anim_montage', toolName: 'animation_physics', arguments: {"action": "play_anim_montage", "actorName": "SkelTestActor", "montagePath": `${TEST_FOLDER}/Testmontage`}, expected: 'success|COMPONENT_NOT_FOUND' },

// === ACTION (Ragdoll - needs actorName with SkeletalMeshComponent) ===
{ scenario: 'ACTION: setup_ragdoll', toolName: 'animation_physics', arguments: {"action": "setup_ragdoll", "actorName": "SkelTestActor"}, expected: 'success|COMPONENT_NOT_FOUND' },
{ scenario: 'ACTION: activate_ragdoll', toolName: 'animation_physics', arguments: {"action": "activate_ragdoll", "actorName": "SkelTestActor"}, expected: 'success|COMPONENT_NOT_FOUND' },

// === CONFIG (Vehicle - needs actorName) ===
{ scenario: 'CONFIG: configure_vehicle', toolName: 'animation_physics', arguments: {"action": "configure_vehicle", "actorName": "SkelTestActor", "vehicleType": "WheeledVehicle4W", "dragCoefficient": 0.32}, expected: 'success|NOT_AVAILABLE|COMPONENT_CREATION_FAILED|ACTOR_NOT_FOUND' },

  // === ACTION (Physics Simulation - needs skeletonPath; may fail if no preview mesh) ===
  { scenario: 'ACTION: setup_physics_simulation', toolName: 'animation_physics', arguments: {"action": "setup_physics_simulation", "skeletonPath": TEST_SKELETON_PATH}, expected: 'error|ASSET_NOT_FOUND|SKELETON_MISSING_PREVIEW' },

// === CREATE (Anim Blueprint repeat - needs skeletonPath) ===
{ scenario: 'CREATE: create_anim_blueprint', toolName: 'animation_physics', arguments: {"action": "create_anim_blueprint", "name": "Testanim_blueprint", "path": TEST_FOLDER, "skeletonPath": TEST_SKELETON_PATH, "parentClass": "Actor"}, expected: 'success|already exists' },

// === ADD (Blend Sample - needs assetPath and animationPath) ===
{ scenario: 'ADD: add_blend_sample', toolName: 'animation_physics', arguments: {"action": "add_blend_sample", "assetPath": `${TEST_FOLDER}/Testblend_space_1d`, "animationPath": `${TEST_FOLDER}/Testanimation_sequence`, "sampleValue": 100}, expected: 'success|already exists' },

// === CONFIG (Axis Settings - needs assetPath) ===
{ scenario: 'CONFIG: set_axis_settings', toolName: 'animation_physics', arguments: {"action": "set_axis_settings", "assetPath": `${TEST_FOLDER}/Testblend_space_1d`, "axis": "X", "axisName": "Speed", "minValue": 0, "maxValue": 600}, expected: 'success' },

{ scenario: 'ACTION: force_rebuild_blend_space', toolName: 'animation_physics', arguments: {"action": "force_rebuild_blend_space", "assetPath": `${TEST_FOLDER}/Testblend_space_1d`, "rebuildBlendParameters": true, "compileReferencers": false, "save": false}, expected: 'success' },

// === CONFIG (Interpolation Settings - needs assetPath) ===
{ scenario: 'CONFIG: set_interpolation_settings', toolName: 'animation_physics', arguments: {"action": "set_interpolation_settings", "assetPath": `${TEST_FOLDER}/Testblend_space_1d`, "interpolationType": "Lerp"}, expected: 'success' },

// === ACTION (Retargeting - needs sourceSkeleton and targetSkeleton) ===
{ scenario: 'ACTION: setup_retargeting', toolName: 'animation_physics', arguments: {"action": "setup_retargeting", "sourceSkeleton": TEST_SKELETON_PATH, "targetSkeleton": TEST_SKELETON_PATH, "assets": [`${TEST_FOLDER}/Testanimation_sequence`], "savePath": TEST_FOLDER, "suffix": "_Retargeted", "overwrite": true}, expected: 'success' },

// === ACTION (Cleanup - needs artifacts array) ===
{ scenario: 'ACTION: cleanup', toolName: 'animation_physics', arguments: {"action": "cleanup", "artifacts": [
  `${TEST_FOLDER}/Testanimation_blueprint`,
  `${TEST_FOLDER}/Testanimation_bp`,
  `${TEST_FOLDER}/Testanim_blueprint`,
  `${TEST_FOLDER}/Testblend_space`,
  `${TEST_FOLDER}/Testblend_space_1d`,
  `${TEST_FOLDER}/Testblend_space_2d`,
  `${TEST_FOLDER}/Testprocedural_anim`,
  `${TEST_FOLDER}/Testaim_offset`,
  `${TEST_FOLDER}/Testcontrol_rig`,
  TEST_IK_RIG_PATH,
  TEST_IK_RETARGETER_PATH,
  `${TEST_FOLDER}/Testpose_library`,
  `${TEST_FOLDER}/Testanimation_asset`,
  `${TEST_FOLDER}/Testanimation_sequence`,
  `${TEST_FOLDER}/Testmontage`,
  TEST_SKELETON_PATH,
  '/Game/MCPTest/AuthoringAssets_CtrlRig',
  '/Game/Animations/TestIK',
  '/Game/Animations/PoseLibraries/Testpose_library'
]}, expected: 'success|not found' },

// === CLEANUP ===
{ scenario: 'Cleanup: delete SkelTestActor', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'SkelTestActor' }, expected: 'success|not found' },
{ scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

// === SKELETON ACTIONS ===
{
  /**
   * animation_physics skeleton action integration tests
   * Exercises real skeleton, socket, skin-weight, physics-asset, cloth, morph, and info actions.
   */

  const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
  const SKELETAL_MESH_PATH = '/Engine/EngineMeshes/SkeletalCube';
  const ts = Date.now();

  const SKELETON_PATH = `${TEST_FOLDER}/SK_Test_${ts}`;
  const PHYSICS_ASSET_PATH = `${TEST_FOLDER}/PHYS_Test_${ts}`;
  const PHYSICS_SIM_NAME = `PHYS_Sim_${ts}`;
  const ROOT_BONE = 'Root';
  const MESH_BONE_A = 'Bone01';
  const MESH_BONE_B = 'Bone02';
  const CHILD_BONE = `McpChild_${ts}`;
  const TEMP_BONE = `McpTemp_${ts}`;
  const VIRTUAL_BONE = `VB_Mcp_${ts}`;
  const RENAMED_VIRTUAL_BONE = `VB_McpRenamed_${ts}`;
  const SOCKET_NAME = `McpSocket_${ts}`;
  const MORPH_TARGET_NAME = `McpMorph_${ts}`;
  const PROFILE_NAME = `McpWeights_${ts}`;

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: create test skeleton', toolName: 'animation_physics', arguments: { action: 'create_skeleton', path: SKELETON_PATH, rootBoneName: ROOT_BONE, save: true }, expected: 'success|already exists' },

    // === SKELETON STRUCTURE ===
    { scenario: 'ADD: add child bone', toolName: 'animation_physics', arguments: { action: 'add_bone', skeletonPath: SKELETON_PATH, boneName: CHILD_BONE, parentBoneName: ROOT_BONE, location: [10, 0, 0], rotation: [0, 0, 0], scale: 1, save: true }, expected: 'success|already exists' },
    { scenario: 'CREATE: create virtual bone', toolName: 'animation_physics', arguments: { action: 'create_virtual_bone', skeletonPath: SKELETON_PATH, sourceBoneName: ROOT_BONE, targetBoneName: CHILD_BONE, boneName: VIRTUAL_BONE, save: true }, expected: 'success|already exists' },
    { scenario: 'ACTION: rename virtual bone', toolName: 'animation_physics', arguments: { action: 'rename_bone', skeletonPath: SKELETON_PATH, boneName: VIRTUAL_BONE, newBoneName: RENAMED_VIRTUAL_BONE, save: true }, expected: 'success' },
    { scenario: 'CONFIG: set mesh bone transform', toolName: 'animation_physics', arguments: { action: 'set_bone_transform', skeletalMeshPath: SKELETAL_MESH_PATH, boneName: MESH_BONE_A, location: [0, 0, 0], rotation: [0, 0, 0], scale: [1, 1, 1], save: false }, expected: 'success' },
    { scenario: 'ADD: add temp bone', toolName: 'animation_physics', arguments: { action: 'add_bone', skeletonPath: SKELETON_PATH, boneName: TEMP_BONE, parentBoneName: ROOT_BONE, location: [0, 10, 0], save: true }, expected: 'success|already exists' },
    { scenario: 'CONFIG: set temp bone parent', toolName: 'animation_physics', arguments: { action: 'set_bone_parent', skeletonPath: SKELETON_PATH, boneName: TEMP_BONE, parentBoneName: CHILD_BONE, save: true }, expected: 'success' },
    { scenario: 'DELETE: remove temp bone', toolName: 'animation_physics', arguments: { action: 'remove_bone', skeletonPath: SKELETON_PATH, boneName: TEMP_BONE, removeChildren: true, save: true }, expected: 'success|not found' },

    // === SOCKETS ===
    { scenario: 'CREATE: create socket', toolName: 'animation_physics', arguments: { action: 'create_socket', skeletonPath: SKELETON_PATH, socketName: SOCKET_NAME, attachBoneName: CHILD_BONE, relativeLocation: [1, 2, 3], relativeRotation: [0, 45, 0], relativeScale: [1, 1, 1], save: true }, expected: 'success|already exists' },
    { scenario: 'CONFIG: configure socket', toolName: 'animation_physics', arguments: { action: 'configure_socket', skeletonPath: SKELETON_PATH, socketName: SOCKET_NAME, attachBoneName: CHILD_BONE, relativeLocation: [4, 5, 6], relativeRotation: [0, 90, 0], relativeScale: [1.25, 1.25, 1.25], save: true }, expected: 'success' },
    { scenario: 'INFO: list sockets', toolName: 'animation_physics', arguments: { action: 'list_sockets', skeletonPath: SKELETON_PATH }, expected: 'success' },

    // === SKELETON INFO ===
    { scenario: 'INFO: get skeleton info', toolName: 'animation_physics', arguments: { action: 'get_skeleton_info', skeletonPath: SKELETON_PATH }, expected: 'success' },
    { scenario: 'INFO: list bones', toolName: 'animation_physics', arguments: { action: 'list_bones', skeletonPath: SKELETON_PATH }, expected: 'success' },

    // === PHYSICS ASSET ===
    { scenario: 'CREATE: create physics asset', toolName: 'animation_physics', arguments: { action: 'create_physics_asset', skeletalMeshPath: SKELETAL_MESH_PATH, outputPath: PHYSICS_ASSET_PATH, save: true }, expected: 'success|already exists' },
    { scenario: 'ACTION: setup physics simulation from skeletal mesh', toolName: 'animation_physics', arguments: { action: 'setup_physics_simulation', skeletalMeshPath: SKELETAL_MESH_PATH, savePath: TEST_FOLDER, physicsAssetName: PHYSICS_SIM_NAME, assignToMesh: false }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.existingAsset', equals: false, label: 'new physics simulation asset created from skeletalMeshPath' }] },
    { scenario: 'INFO: list physics bodies', toolName: 'animation_physics', arguments: { action: 'list_physics_bodies', physicsAssetPath: PHYSICS_ASSET_PATH }, expected: 'success' },
    { scenario: 'ADD: add physics body A', toolName: 'animation_physics', arguments: { action: 'add_physics_body', physicsAssetPath: PHYSICS_ASSET_PATH, boneName: MESH_BONE_A, bodyType: 'Sphere', radius: 12, center: [0, 0, 0], save: true }, expected: 'success|already exists' },
    { scenario: 'ADD: add physics body B', toolName: 'animation_physics', arguments: { action: 'add_physics_body', physicsAssetPath: PHYSICS_ASSET_PATH, boneName: MESH_BONE_B, bodyType: 'Sphere', radius: 10, center: [0, 0, 0], save: true }, expected: 'success|already exists' },
    { scenario: 'CONFIG: configure physics body', toolName: 'animation_physics', arguments: { action: 'configure_physics_body', physicsAssetPath: PHYSICS_ASSET_PATH, boneName: MESH_BONE_A, mass: 5, linearDamping: 0.25, angularDamping: 0.5, collisionEnabled: true, simulatePhysics: false, save: true }, expected: 'success' },
    { scenario: 'ADD: add physics constraint', toolName: 'animation_physics', arguments: { action: 'add_physics_constraint', physicsAssetPath: PHYSICS_ASSET_PATH, bodyA: MESH_BONE_A, bodyB: MESH_BONE_B, constraintName: `McpConstraint_${ts}`, save: true }, expected: 'success|already exists' },
    { scenario: 'CONFIG: configure constraint limits', toolName: 'animation_physics', arguments: { action: 'configure_constraint_limits', physicsAssetPath: PHYSICS_ASSET_PATH, bodyA: MESH_BONE_A, bodyB: MESH_BONE_B, limits: { swing1LimitAngle: 30, swing2LimitAngle: 35, twistLimitAngle: 20, swing1Motion: 'Limited', swing2Motion: 'Limited', twistMotion: 'Limited' }, save: true }, expected: 'success' },

    // === SKIN WEIGHTS ===
    { scenario: 'ACTION: auto skin weights', toolName: 'animation_physics', arguments: { action: 'auto_skin_weights', skeletalMeshPath: SKELETAL_MESH_PATH, save: false }, expected: 'success' },
    { scenario: 'CONFIG: set vertex weights', toolName: 'animation_physics', arguments: { action: 'set_vertex_weights', skeletalMeshPath: SKELETAL_MESH_PATH, profileName: PROFILE_NAME, lodIndex: 0, weights: [{ vertexIndex: 0, influences: [{ boneIndex: 0, weight: 1 }] }], save: false }, expected: 'success' },
    { scenario: 'ACTION: normalize weights', toolName: 'animation_physics', arguments: { action: 'normalize_weights', skeletalMeshPath: SKELETAL_MESH_PATH, save: false }, expected: 'success' },
    { scenario: 'ACTION: prune weights', toolName: 'animation_physics', arguments: { action: 'prune_weights', skeletalMeshPath: SKELETAL_MESH_PATH, threshold: 0.01, save: false }, expected: 'success' },
    { scenario: 'ACTION: copy weights', toolName: 'animation_physics', arguments: { action: 'copy_weights', sourceMeshPath: SKELETAL_MESH_PATH, targetMeshPath: SKELETAL_MESH_PATH, profileName: `${PROFILE_NAME}_Copy`, lodIndex: 0 }, expected: 'success' },
    { scenario: 'ACTION: mirror weights', toolName: 'animation_physics', arguments: { action: 'mirror_weights', skeletalMeshPath: SKELETAL_MESH_PATH, axis: 'X', profileName: `${PROFILE_NAME}_Mirror`, lodIndex: 0, save: false }, expected: 'success' },

    // === CLOTH AND MORPHS ===
    { scenario: 'CONNECT: bind cloth to skeletal mesh', toolName: 'animation_physics', arguments: { action: 'bind_cloth_to_skeletal_mesh', skeletalMeshPath: SKELETAL_MESH_PATH, save: false }, expected: 'success' },
    { scenario: 'CONNECT: assign cloth asset to mesh', toolName: 'animation_physics', arguments: { action: 'assign_cloth_asset_to_mesh', skeletalMeshPath: SKELETAL_MESH_PATH, save: false }, expected: 'error|manual intervention|required' },
    { scenario: 'CREATE: create morph target', toolName: 'animation_physics', arguments: { action: 'create_morph_target', skeletalMeshPath: SKELETAL_MESH_PATH, morphTargetName: MORPH_TARGET_NAME, deltas: [{ vertexIndex: 0, positionDelta: { x: 0, y: 0, z: 1 } }], save: false }, expected: 'success|already exists' },
    { scenario: 'CONFIG: set morph target deltas', toolName: 'animation_physics', arguments: { action: 'set_morph_target_deltas', skeletalMeshPath: SKELETAL_MESH_PATH, morphTargetName: MORPH_TARGET_NAME, deltas: [{ vertexIndex: 0, positionDelta: { x: 0, y: 0, z: 2 } }], save: false }, expected: 'success' },
    { scenario: 'ACTION: import morph targets', toolName: 'animation_physics', arguments: { action: 'import_morph_targets', skeletalMeshPath: SKELETAL_MESH_PATH, morphTargetPath: SKELETAL_MESH_PATH, save: false }, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('animation-physics', testCases);
