#!/usr/bin/env node
import path from 'node:path';
import { pathToFileURL } from 'node:url';
/**
 * Gameplay Tools Integration Tests - EXPANDED
 * 
 * Tools: animation_physics (162), manage_effect (74), manage_character (78),
 *        manage_combat (67), manage_ai (103), manage_networking (73),
 *        manage_gameplay_systems (50), manage_gameplay_primitives (62)
 * Total Actions: 669
 * Test Cases: ~1,340 (2x coverage: success + edge cases)
 * 
 * Usage:
 *   node tests/category-tests/gameplay-tools.test.mjs
 */

import { runToolTests } from '../test-runner.mjs';

const TEST_FOLDER = '/Game/GameplayToolsTest';

// ============================================================================
// ANIMATION_PHYSICS (162 actions) - FULL COVERAGE
// ============================================================================
const animationPhysicsTests = [
  // === Animation Blueprint Creation & Management ===
  { scenario: 'AnimPhys: create_animation_bp', toolName: 'animation_physics', arguments: { action: 'create_animation_bp', name: 'ABP_Test', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_anim_blueprint', toolName: 'animation_physics', arguments: { action: 'create_anim_blueprint', name: 'ABP_Test2', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: play_montage', toolName: 'animation_physics', arguments: { action: 'play_montage', actorName: 'SkeletalActor', montagePath: `${TEST_FOLDER}/AM_Attack`, playRate: 1.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: get_animation_info', toolName: 'animation_physics', arguments: { action: 'get_animation_info', animationPath: `${TEST_FOLDER}/ABP_Test` }, expected: 'success|not found' },
  
  // === State Machines ===
  { scenario: 'AnimPhys: create_state_machine', toolName: 'animation_physics', arguments: { action: 'create_state_machine', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Locomotion' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_machine', toolName: 'animation_physics', arguments: { action: 'add_state_machine', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Combat' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_idle', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Locomotion', stateName: 'Idle', isEntryState: true }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_walk', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Locomotion', stateName: 'Walk' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_run', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Locomotion', stateName: 'Run' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_jump', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Locomotion', stateName: 'Jump' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_transition_idle_walk', toolName: 'animation_physics', arguments: { action: 'add_transition', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Locomotion', fromState: 'Idle', toState: 'Walk' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_transition_walk_run', toolName: 'animation_physics', arguments: { action: 'add_transition', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Locomotion', fromState: 'Walk', toState: 'Run' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_transition_rules', toolName: 'animation_physics', arguments: { action: 'set_transition_rules', animBpPath: `${TEST_FOLDER}/ABP_Test`, stateMachineName: 'Locomotion', fromState: 'Idle', toState: 'Walk', automaticTriggerRule: 'SpeedThreshold' }, expected: 'success|not found' },
  
  // === Animation Sequences ===
  { scenario: 'AnimPhys: create_animation_sequence', toolName: 'animation_physics', arguments: { action: 'create_animation_sequence', name: 'AS_Run', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', numFrames: 30, frameRate: 30 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_animation_asset', toolName: 'animation_physics', arguments: { action: 'create_animation_asset', name: 'AS_Walk', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_sequence_length', toolName: 'animation_physics', arguments: { action: 'set_sequence_length', assetPath: `${TEST_FOLDER}/AS_Run`, numFrames: 60 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_bone_track', toolName: 'animation_physics', arguments: { action: 'add_bone_track', assetPath: `${TEST_FOLDER}/AS_Run`, boneName: 'root' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_bone_key', toolName: 'animation_physics', arguments: { action: 'set_bone_key', assetPath: `${TEST_FOLDER}/AS_Run`, boneName: 'root', frame: 0, location: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_curve_key', toolName: 'animation_physics', arguments: { action: 'set_curve_key', assetPath: `${TEST_FOLDER}/AS_Run`, curveName: 'Speed', frame: 0, value: 0 }, expected: 'success|not found' },
  
  // === Animation Montages ===
  { scenario: 'AnimPhys: create_montage', toolName: 'animation_physics', arguments: { action: 'create_montage', name: 'AM_Attack', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_section', toolName: 'animation_physics', arguments: { action: 'add_montage_section', assetPath: `${TEST_FOLDER}/AM_Attack`, sectionName: 'WindUp', startTime: 0, length: 0.5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_slot', toolName: 'animation_physics', arguments: { action: 'add_montage_slot', assetPath: `${TEST_FOLDER}/AM_Attack`, slotName: 'UpperBody' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_section_timing', toolName: 'animation_physics', arguments: { action: 'set_section_timing', assetPath: `${TEST_FOLDER}/AM_Attack`, sectionName: 'WindUp', startTime: 0, length: 0.3 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_notify', toolName: 'animation_physics', arguments: { action: 'add_montage_notify', assetPath: `${TEST_FOLDER}/AM_Attack`, notifyName: 'DealDamage', time: 0.3 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_blend_in', toolName: 'animation_physics', arguments: { action: 'set_blend_in', assetPath: `${TEST_FOLDER}/AM_Attack`, blendTime: 0.2 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_blend_out', toolName: 'animation_physics', arguments: { action: 'set_blend_out', assetPath: `${TEST_FOLDER}/AM_Attack`, blendTime: 0.2 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: link_sections', toolName: 'animation_physics', arguments: { action: 'link_sections', assetPath: `${TEST_FOLDER}/AM_Attack`, fromSection: 'WindUp', toSection: 'Strike' }, expected: 'success|not found' },
  
  // === Blend Spaces ===
  { scenario: 'AnimPhys: create_blend_space', toolName: 'animation_physics', arguments: { action: 'create_blend_space', name: 'BS_Locomotion', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_blend_space_1d', toolName: 'animation_physics', arguments: { action: 'create_blend_space_1d', name: 'BS_Walk1D', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', axisName: 'Speed', axisMin: 0, axisMax: 600 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_blend_space_2d', toolName: 'animation_physics', arguments: { action: 'create_blend_space_2d', name: 'BS_Loco2D', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', horizontalAxisName: 'Speed', verticalAxisName: 'Direction' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_blend_sample', toolName: 'animation_physics', arguments: { action: 'add_blend_sample', assetPath: `${TEST_FOLDER}/BS_Locomotion`, animationPath: '/Engine/Animation/Idle', sampleValue: 0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_axis_settings', toolName: 'animation_physics', arguments: { action: 'set_axis_settings', assetPath: `${TEST_FOLDER}/BS_Locomotion`, axis: 'Horizontal', axisName: 'Speed', minValue: 0, maxValue: 600 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_interpolation_settings', toolName: 'animation_physics', arguments: { action: 'set_interpolation_settings', assetPath: `${TEST_FOLDER}/BS_Locomotion`, interpolationType: 'Linear', targetWeightInterpolationSpeed: 5.0 }, expected: 'success|not found' },
  
  // === Aim Offset ===
  { scenario: 'AnimPhys: create_aim_offset', toolName: 'animation_physics', arguments: { action: 'create_aim_offset', name: 'AO_Rifle', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_aim_offset_sample', toolName: 'animation_physics', arguments: { action: 'add_aim_offset_sample', assetPath: `${TEST_FOLDER}/AO_Rifle`, animationPath: '/Engine/Animation/Idle', yaw: 0, pitch: 0 }, expected: 'success|not found' },
  
  // === Blend Nodes ===
  { scenario: 'AnimPhys: create_blend_tree', toolName: 'animation_physics', arguments: { action: 'create_blend_tree', animBpPath: `${TEST_FOLDER}/ABP_Test`, nodeName: 'BlendTree_Combat' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_blend_node', toolName: 'animation_physics', arguments: { action: 'add_blend_node', animBpPath: `${TEST_FOLDER}/ABP_Test`, blendType: 'BlendByBool', nodeName: 'IsCrouching', x: 200, y: 100 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_cached_pose', toolName: 'animation_physics', arguments: { action: 'add_cached_pose', animBpPath: `${TEST_FOLDER}/ABP_Test`, cacheName: 'BasePose' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_slot_node', toolName: 'animation_physics', arguments: { action: 'add_slot_node', animBpPath: `${TEST_FOLDER}/ABP_Test`, slotName: 'DefaultSlot' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_layered_blend_per_bone', toolName: 'animation_physics', arguments: { action: 'add_layered_blend_per_bone', animBpPath: `${TEST_FOLDER}/ABP_Test`, layerSetup: [{ boneName: 'spine_03', depth: 0 }] }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_anim_graph_node_value', toolName: 'animation_physics', arguments: { action: 'set_anim_graph_node_value', animBpPath: `${TEST_FOLDER}/ABP_Test`, nodeName: 'BlendTree_Combat', propertyName: 'Alpha', value: 0.5 }, expected: 'success|not found' },
  
  // === Notifies ===
  { scenario: 'AnimPhys: add_notify', toolName: 'animation_physics', arguments: { action: 'add_notify', animationPath: `${TEST_FOLDER}/AM_Attack`, notifyClass: 'AnimNotify_PlaySound', time: 0.5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_notify_state', toolName: 'animation_physics', arguments: { action: 'add_notify_state', animationPath: `${TEST_FOLDER}/AM_Attack`, notifyClass: 'AnimNotifyState_Trail', startFrame: 10, endFrame: 30 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_sync_marker', toolName: 'animation_physics', arguments: { action: 'add_sync_marker', assetPath: `${TEST_FOLDER}/AS_Run`, markerName: 'FootDown', frame: 15 }, expected: 'success|not found' },
  
  // === Root Motion ===
  { scenario: 'AnimPhys: set_root_motion_settings', toolName: 'animation_physics', arguments: { action: 'set_root_motion_settings', assetPath: `${TEST_FOLDER}/AS_Run`, enableRootMotion: true, forceRootLock: false }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_additive_settings', toolName: 'animation_physics', arguments: { action: 'set_additive_settings', assetPath: `${TEST_FOLDER}/AS_Run`, additiveAnimType: 'LocalSpace', basePoseType: 'FirstFrame' }, expected: 'success|not found' },
  
  // === Control Rig ===
  { scenario: 'AnimPhys: create_control_rig', toolName: 'animation_physics', arguments: { action: 'create_control_rig', name: 'CR_Test', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_Test`, controlName: 'Root_Control', controlType: 'Transform', parentBone: 'root' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_rig_unit', toolName: 'animation_physics', arguments: { action: 'add_rig_unit', assetPath: `${TEST_FOLDER}/CR_Test`, unitType: 'FABRIK', unitName: 'Arm_IK' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: connect_rig_elements', toolName: 'animation_physics', arguments: { action: 'connect_rig_elements', assetPath: `${TEST_FOLDER}/CR_Test`, sourceElement: 'Root_Control', sourcePin: 'Output', targetElement: 'Arm_IK', targetPin: 'Input' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: get_control_rig_controls', toolName: 'animation_physics', arguments: { action: 'get_control_rig_controls', controlRigPath: `${TEST_FOLDER}/CR_Test` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_control_value', toolName: 'animation_physics', arguments: { action: 'set_control_value', actorName: 'SkeletalActor', controlName: 'Root_Control', value: { x: 0, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: reset_control_rig', toolName: 'animation_physics', arguments: { action: 'reset_control_rig', actorName: 'SkeletalActor' }, expected: 'success|not found' },
  
  // === IK Rig ===
  { scenario: 'AnimPhys: create_ik_rig', toolName: 'animation_physics', arguments: { action: 'create_ik_rig', name: 'IKRig_Test', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_chain', toolName: 'animation_physics', arguments: { action: 'add_ik_chain', assetPath: `${TEST_FOLDER}/IKRig_Test`, chainName: 'LeftArm', startBone: 'clavicle_l', endBone: 'hand_l' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_goal', toolName: 'animation_physics', arguments: { action: 'add_ik_goal', assetPath: `${TEST_FOLDER}/IKRig_Test`, goal: 'LeftHand_Goal', chainName: 'LeftArm' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: setup_ik', toolName: 'animation_physics', arguments: { action: 'setup_ik', animBpPath: `${TEST_FOLDER}/ABP_Test`, boneName: 'hand_r' }, expected: 'success|not found' },
  
  // === IK Retargeter ===
  { scenario: 'AnimPhys: create_ik_retargeter', toolName: 'animation_physics', arguments: { action: 'create_ik_retargeter', name: 'RTG_Test', path: TEST_FOLDER, sourceIKRigPath: `${TEST_FOLDER}/IKRig_Source`, targetIKRigPath: `${TEST_FOLDER}/IKRig_Target` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_retarget_chain_mapping', toolName: 'animation_physics', arguments: { action: 'set_retarget_chain_mapping', assetPath: `${TEST_FOLDER}/RTG_Test`, sourceChain: 'LeftArm', targetChain: 'LeftArm' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: setup_retargeting', toolName: 'animation_physics', arguments: { action: 'setup_retargeting', sourceSkeleton: '/Engine/EngineMeshes/Skeleton1', targetSkeleton: '/Engine/EngineMeshes/Skeleton2' }, expected: 'success|not found' },
  
  // === Pose Library ===
  { scenario: 'AnimPhys: create_pose_library', toolName: 'animation_physics', arguments: { action: 'create_pose_library', name: 'PL_Faces', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: apply_pose_asset', toolName: 'animation_physics', arguments: { action: 'apply_pose_asset', actorName: 'SkeletalActor', assetPath: `${TEST_FOLDER}/PL_Faces` }, expected: 'success|not found' },
  
  // === Procedural Animation ===
  { scenario: 'AnimPhys: create_procedural_anim', toolName: 'animation_physics', arguments: { action: 'create_procedural_anim', name: 'PA_Breathe', path: TEST_FOLDER }, expected: 'success|not found' },
  
  // === Ragdoll & Physics ===
  { scenario: 'AnimPhys: setup_ragdoll', toolName: 'animation_physics', arguments: { action: 'setup_ragdoll', skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: activate_ragdoll', toolName: 'animation_physics', arguments: { action: 'activate_ragdoll', actorName: 'SkeletalActor' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: setup_physics_simulation', toolName: 'animation_physics', arguments: { action: 'setup_physics_simulation', actorName: 'SkeletalActor', boneName: 'pelvis' }, expected: 'success|not found' },
  
  // === Vehicle ===
  { scenario: 'AnimPhys: configure_vehicle', toolName: 'animation_physics', arguments: { action: 'configure_vehicle', vehicleName: 'BP_Car', vehicleType: 'Wheeled' }, expected: 'success|not found' },
  
  // === Motion Matching ===
  { scenario: 'AnimPhys: create_pose_search_database', toolName: 'animation_physics', arguments: { action: 'create_pose_search_database', name: 'PSD_Locomotion', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: configure_motion_matching', toolName: 'animation_physics', arguments: { action: 'configure_motion_matching', animBpPath: `${TEST_FOLDER}/ABP_Test`, databasePath: `${TEST_FOLDER}/PSD_Locomotion` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_trajectory_prediction', toolName: 'animation_physics', arguments: { action: 'add_trajectory_prediction', animBpPath: `${TEST_FOLDER}/ABP_Test` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: get_motion_matching_state', toolName: 'animation_physics', arguments: { action: 'get_motion_matching_state', actorName: 'SkeletalActor' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_motion_matching_goal', toolName: 'animation_physics', arguments: { action: 'set_motion_matching_goal', actorName: 'SkeletalActor', goal: { velocity: { x: 300, y: 0, z: 0 } } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: list_pose_search_databases', toolName: 'animation_physics', arguments: { action: 'list_pose_search_databases', path: TEST_FOLDER }, expected: 'success' },
  
  // === Animation Modifier & ML ===
  { scenario: 'AnimPhys: create_animation_modifier', toolName: 'animation_physics', arguments: { action: 'create_animation_modifier', name: 'AM_FootIK', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: setup_ml_deformer', toolName: 'animation_physics', arguments: { action: 'setup_ml_deformer', name: 'MLD_Test', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: configure_ml_deformer_training', toolName: 'animation_physics', arguments: { action: 'configure_ml_deformer_training', assetPath: `${TEST_FOLDER}/MLD_Test`, iterations: 1000 }, expected: 'success|not found' },
  
  // === CHAOS DESTRUCTION (29 actions) ===
  { scenario: 'AnimPhys: chaos_create_geometry_collection', toolName: 'animation_physics', arguments: { action: 'chaos_create_geometry_collection', name: 'GC_Wall', path: TEST_FOLDER, meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_fracture_uniform', toolName: 'animation_physics', arguments: { action: 'chaos_fracture_uniform', assetPath: `${TEST_FOLDER}/GC_Wall`, siteCount: 50 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_fracture_clustered', toolName: 'animation_physics', arguments: { action: 'chaos_fracture_clustered', assetPath: `${TEST_FOLDER}/GC_Wall`, clusterCount: 5, siteCount: 30 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_fracture_radial', toolName: 'animation_physics', arguments: { action: 'chaos_fracture_radial', assetPath: `${TEST_FOLDER}/GC_Wall`, center: { x: 0, y: 0, z: 50 }, angularSteps: 8, radialSteps: 3 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_fracture_slice', toolName: 'animation_physics', arguments: { action: 'chaos_fracture_slice', assetPath: `${TEST_FOLDER}/GC_Wall`, sliceCount: 5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_fracture_brick', toolName: 'animation_physics', arguments: { action: 'chaos_fracture_brick', assetPath: `${TEST_FOLDER}/GC_Wall`, brickLength: 50, brickHeight: 25, brickDepth: 20 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_flatten_fracture', toolName: 'animation_physics', arguments: { action: 'chaos_flatten_fracture', assetPath: `${TEST_FOLDER}/GC_Wall` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_geometry_collection_materials', toolName: 'animation_physics', arguments: { action: 'chaos_set_geometry_collection_materials', assetPath: `${TEST_FOLDER}/GC_Wall`, materialPath: '/Engine/BasicMaterials/M_Basic' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_damage_thresholds', toolName: 'animation_physics', arguments: { action: 'chaos_set_damage_thresholds', assetPath: `${TEST_FOLDER}/GC_Wall`, damageThreshold: 100 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cluster_connection_type', toolName: 'animation_physics', arguments: { action: 'chaos_set_cluster_connection_type', assetPath: `${TEST_FOLDER}/GC_Wall`, connectionType: 'PointImplicit' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_collision_particles_fraction', toolName: 'animation_physics', arguments: { action: 'chaos_set_collision_particles_fraction', assetPath: `${TEST_FOLDER}/GC_Wall`, fraction: 0.25 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_remove_on_break', toolName: 'animation_physics', arguments: { action: 'chaos_set_remove_on_break', assetPath: `${TEST_FOLDER}/GC_Wall`, removeOnBreak: true, removeDelay: 5.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_field_system_actor', toolName: 'animation_physics', arguments: { action: 'chaos_create_field_system_actor', actorName: 'FS_Explosion', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'AnimPhys: chaos_add_transient_field', toolName: 'animation_physics', arguments: { action: 'chaos_add_transient_field', actorName: 'FS_Explosion', fieldType: 'RadialVector', magnitude: 1000 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_persistent_field', toolName: 'animation_physics', arguments: { action: 'chaos_add_persistent_field', actorName: 'FS_Explosion', fieldType: 'UniformVector', direction: { x: 0, y: 0, z: -1 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_construction_field', toolName: 'animation_physics', arguments: { action: 'chaos_add_construction_field', assetPath: `${TEST_FOLDER}/GC_Wall`, fieldType: 'Anchor' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_field_radial_falloff', toolName: 'animation_physics', arguments: { action: 'chaos_add_field_radial_falloff', actorName: 'FS_Explosion', radius: 500, magnitude: 1.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_field_radial_vector', toolName: 'animation_physics', arguments: { action: 'chaos_add_field_radial_vector', actorName: 'FS_Explosion', magnitude: 2000 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_field_uniform_vector', toolName: 'animation_physics', arguments: { action: 'chaos_add_field_uniform_vector', actorName: 'FS_Explosion', direction: { x: 0, y: 0, z: 1 }, magnitude: 500 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_field_noise', toolName: 'animation_physics', arguments: { action: 'chaos_add_field_noise', actorName: 'FS_Explosion', minRange: 0, maxRange: 100 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_field_strain', toolName: 'animation_physics', arguments: { action: 'chaos_add_field_strain', actorName: 'FS_Explosion', magnitude: 1000 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_anchor_field', toolName: 'animation_physics', arguments: { action: 'chaos_create_anchor_field', assetPath: `${TEST_FOLDER}/GC_Wall`, anchorBone: 'root' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_dynamic_state', toolName: 'animation_physics', arguments: { action: 'chaos_set_dynamic_state', assetPath: `${TEST_FOLDER}/GC_Wall`, dynamicState: 'Sleeping' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_enable_clustering', toolName: 'animation_physics', arguments: { action: 'chaos_enable_clustering', assetPath: `${TEST_FOLDER}/GC_Wall`, enabled: true, clusterGroupIndex: 0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_get_geometry_collection_stats', toolName: 'animation_physics', arguments: { action: 'chaos_get_geometry_collection_stats', assetPath: `${TEST_FOLDER}/GC_Wall` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_geometry_collection_cache', toolName: 'animation_physics', arguments: { action: 'chaos_create_geometry_collection_cache', name: 'GCC_Wall', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_record_geometry_collection_cache', toolName: 'animation_physics', arguments: { action: 'chaos_record_geometry_collection_cache', actorName: 'GC_Wall_Actor', cachePath: `${TEST_FOLDER}/GCC_Wall` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_apply_cache_to_collection', toolName: 'animation_physics', arguments: { action: 'chaos_apply_cache_to_collection', actorName: 'GC_Wall_Actor', cachePath: `${TEST_FOLDER}/GCC_Wall` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_remove_geometry_collection_cache', toolName: 'animation_physics', arguments: { action: 'chaos_remove_geometry_collection_cache', cachePath: `${TEST_FOLDER}/GCC_Wall` }, expected: 'success|not found' },
  
  // === CHAOS VEHICLES (19 actions) ===
  { scenario: 'AnimPhys: chaos_create_wheeled_vehicle_bp', toolName: 'animation_physics', arguments: { action: 'chaos_create_wheeled_vehicle_bp', name: 'BP_Car', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_vehicle_wheel', toolName: 'animation_physics', arguments: { action: 'chaos_add_vehicle_wheel', blueprintPath: `${TEST_FOLDER}/BP_Car`, wheelName: 'FrontLeft', boneName: 'Wheel_FL' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_remove_wheel_from_vehicle', toolName: 'animation_physics', arguments: { action: 'chaos_remove_wheel_from_vehicle', blueprintPath: `${TEST_FOLDER}/BP_Car`, wheelName: 'FrontLeft' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_engine_setup', toolName: 'animation_physics', arguments: { action: 'chaos_configure_engine_setup', blueprintPath: `${TEST_FOLDER}/BP_Car`, maxRPM: 6000, maxTorque: 400 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_transmission_setup', toolName: 'animation_physics', arguments: { action: 'chaos_configure_transmission_setup', blueprintPath: `${TEST_FOLDER}/BP_Car`, gearCount: 5, finalDriveRatio: 3.5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_steering_setup', toolName: 'animation_physics', arguments: { action: 'chaos_configure_steering_setup', blueprintPath: `${TEST_FOLDER}/BP_Car`, steeringType: 'Ackermann', maxSteeringAngle: 45 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_differential_setup', toolName: 'animation_physics', arguments: { action: 'chaos_configure_differential_setup', blueprintPath: `${TEST_FOLDER}/BP_Car`, differentialType: 'AllWheelDrive', frontRearSplit: 0.5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_suspension_setup', toolName: 'animation_physics', arguments: { action: 'chaos_configure_suspension_setup', blueprintPath: `${TEST_FOLDER}/BP_Car`, springRate: 250, dampingRatio: 0.5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_brake_setup', toolName: 'animation_physics', arguments: { action: 'chaos_configure_brake_setup', blueprintPath: `${TEST_FOLDER}/BP_Car`, maxBrakeTorque: 1000, brakeBias: 0.6 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_vehicle_mesh', toolName: 'animation_physics', arguments: { action: 'chaos_set_vehicle_mesh', blueprintPath: `${TEST_FOLDER}/BP_Car`, meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_wheel_class', toolName: 'animation_physics', arguments: { action: 'chaos_set_wheel_class', blueprintPath: `${TEST_FOLDER}/BP_Car`, wheelName: 'FrontLeft', wheelClass: 'ChaosVehicleWheel' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_wheel_offset', toolName: 'animation_physics', arguments: { action: 'chaos_set_wheel_offset', blueprintPath: `${TEST_FOLDER}/BP_Car`, wheelName: 'FrontLeft', offset: { x: 0, y: 80, z: -30 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_wheel_radius', toolName: 'animation_physics', arguments: { action: 'chaos_set_wheel_radius', blueprintPath: `${TEST_FOLDER}/BP_Car`, wheelName: 'FrontLeft', radius: 35 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_vehicle_mass', toolName: 'animation_physics', arguments: { action: 'chaos_set_vehicle_mass', blueprintPath: `${TEST_FOLDER}/BP_Car`, mass: 1500 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_drag_coefficient', toolName: 'animation_physics', arguments: { action: 'chaos_set_drag_coefficient', blueprintPath: `${TEST_FOLDER}/BP_Car`, dragCoefficient: 0.35 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_center_of_mass', toolName: 'animation_physics', arguments: { action: 'chaos_set_center_of_mass', blueprintPath: `${TEST_FOLDER}/BP_Car`, offset: { x: 0, y: 0, z: -25 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_vehicle_animation_instance', toolName: 'animation_physics', arguments: { action: 'chaos_create_vehicle_animation_instance', name: 'VAI_Car', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_vehicle_animation_bp', toolName: 'animation_physics', arguments: { action: 'chaos_set_vehicle_animation_bp', blueprintPath: `${TEST_FOLDER}/BP_Car`, animBpPath: `${TEST_FOLDER}/VAI_Car` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_get_vehicle_config', toolName: 'animation_physics', arguments: { action: 'chaos_get_vehicle_config', blueprintPath: `${TEST_FOLDER}/BP_Car` }, expected: 'success|not found' },
  
  // === CHAOS CLOTH (15 actions) ===
  { scenario: 'AnimPhys: chaos_create_cloth_config', toolName: 'animation_physics', arguments: { action: 'chaos_create_cloth_config', name: 'CC_Cape', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_cloth_shared_sim_config', toolName: 'animation_physics', arguments: { action: 'chaos_create_cloth_shared_sim_config', name: 'CCSS_Shared', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_apply_cloth_to_skeletal_mesh', toolName: 'animation_physics', arguments: { action: 'chaos_apply_cloth_to_skeletal_mesh', skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube', clothConfigPath: `${TEST_FOLDER}/CC_Cape` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_remove_cloth_from_skeletal_mesh', toolName: 'animation_physics', arguments: { action: 'chaos_remove_cloth_from_skeletal_mesh', skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_mass_properties', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_mass_properties', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, massMode: 'UniformMass', mass: 1.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_gravity', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_gravity', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, gravityScale: 1.0, useGravityOverride: false }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_damping', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_damping', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, damping: 0.01 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_collision_properties', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_collision_properties', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, collisionThickness: 1.0, frictionCoefficient: 0.8 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_stiffness', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_stiffness', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, stretchStiffness: 1.0, bendStiffness: 1.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_tether_stiffness', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_tether_stiffness', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, tetherStiffness: 1.0, tetherScale: 1.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_aerodynamics', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_aerodynamics', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, dragCoefficient: 0.5, liftCoefficient: 0.5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_anim_drive', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_anim_drive', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, animDriveStiffness: 0.0, animDriveDamping: 0.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_long_range_attachment', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_long_range_attachment', clothConfigPath: `${TEST_FOLDER}/CC_Cape`, useLongRangeAttachments: true, tetherMaxDistance: 100 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_get_cloth_config', toolName: 'animation_physics', arguments: { action: 'chaos_get_cloth_config', clothConfigPath: `${TEST_FOLDER}/CC_Cape` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_get_cloth_stats', toolName: 'animation_physics', arguments: { action: 'chaos_get_cloth_stats', actorName: 'SkeletalActor' }, expected: 'success|not found' },
  
  // === CHAOS FLESH (13 actions) ===
  { scenario: 'AnimPhys: chaos_create_flesh_asset', toolName: 'animation_physics', arguments: { action: 'chaos_create_flesh_asset', name: 'FA_Muscle', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_flesh_component', toolName: 'animation_physics', arguments: { action: 'chaos_create_flesh_component', blueprintPath: `${TEST_FOLDER}/BP_Character`, componentName: 'FleshComponent' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_flesh_simulation_properties', toolName: 'animation_physics', arguments: { action: 'chaos_set_flesh_simulation_properties', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle`, substeps: 4 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_flesh_stiffness', toolName: 'animation_physics', arguments: { action: 'chaos_set_flesh_stiffness', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle`, stiffness: 100 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_flesh_damping', toolName: 'animation_physics', arguments: { action: 'chaos_set_flesh_damping', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle`, damping: 0.5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_flesh_incompressibility', toolName: 'animation_physics', arguments: { action: 'chaos_set_flesh_incompressibility', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle`, incompressibility: 100 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_flesh_inflation', toolName: 'animation_physics', arguments: { action: 'chaos_set_flesh_inflation', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle`, inflation: 0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_flesh_solver_iterations', toolName: 'animation_physics', arguments: { action: 'chaos_set_flesh_solver_iterations', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle`, iterations: 10 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_bind_flesh_to_skeleton', toolName: 'animation_physics', arguments: { action: 'chaos_bind_flesh_to_skeleton', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle`, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_flesh_rest_state', toolName: 'animation_physics', arguments: { action: 'chaos_set_flesh_rest_state', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_flesh_cache', toolName: 'animation_physics', arguments: { action: 'chaos_create_flesh_cache', name: 'FC_Muscle', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_record_flesh_simulation', toolName: 'animation_physics', arguments: { action: 'chaos_record_flesh_simulation', actorName: 'FleshActor', cachePath: `${TEST_FOLDER}/FC_Muscle` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_get_flesh_asset_info', toolName: 'animation_physics', arguments: { action: 'chaos_get_flesh_asset_info', fleshAssetPath: `${TEST_FOLDER}/FA_Muscle` }, expected: 'success|not found' },
  
  // === Utility & Wave 2.41-2.50 ===
  { scenario: 'AnimPhys: chaos_get_physics_destruction_info', toolName: 'animation_physics', arguments: { action: 'chaos_get_physics_destruction_info' }, expected: 'success' },
  { scenario: 'AnimPhys: chaos_list_geometry_collections', toolName: 'animation_physics', arguments: { action: 'chaos_list_geometry_collections', path: TEST_FOLDER }, expected: 'success' },
  { scenario: 'AnimPhys: chaos_list_chaos_vehicles', toolName: 'animation_physics', arguments: { action: 'chaos_list_chaos_vehicles', path: TEST_FOLDER }, expected: 'success' },
  { scenario: 'AnimPhys: chaos_get_plugin_status', toolName: 'animation_physics', arguments: { action: 'chaos_get_plugin_status' }, expected: 'success' },
  { scenario: 'AnimPhys: cleanup', toolName: 'animation_physics', arguments: { action: 'cleanup' }, expected: 'success' },
  { scenario: 'AnimPhys: create_anim_layer', toolName: 'animation_physics', arguments: { action: 'create_anim_layer', animBpPath: `${TEST_FOLDER}/ABP_Test`, layerName: 'UpperBody' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: stack_anim_layers', toolName: 'animation_physics', arguments: { action: 'stack_anim_layers', animBpPath: `${TEST_FOLDER}/ABP_Test`, layers: ['Base', 'UpperBody', 'Face'] }, expected: 'success|not found' },
  { scenario: 'AnimPhys: configure_squash_stretch', toolName: 'animation_physics', arguments: { action: 'configure_squash_stretch', animBpPath: `${TEST_FOLDER}/ABP_Test`, enabled: true, squashAmount: 0.1, stretchAmount: 0.1 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_rigging_layer', toolName: 'animation_physics', arguments: { action: 'create_rigging_layer', controlRigPath: `${TEST_FOLDER}/CR_Test`, layerName: 'FK_Layer' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: configure_layer_blend_mode', toolName: 'animation_physics', arguments: { action: 'configure_layer_blend_mode', controlRigPath: `${TEST_FOLDER}/CR_Test`, layerName: 'FK_Layer', blendMode: 'Additive' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_control_rig_physics', toolName: 'animation_physics', arguments: { action: 'create_control_rig_physics', controlRigPath: `${TEST_FOLDER}/CR_Test`, physicsProfile: 'Default' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: configure_ragdoll_profile', toolName: 'animation_physics', arguments: { action: 'configure_ragdoll_profile', blueprintPath: `${TEST_FOLDER}/BP_Character`, profileName: 'Combat' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: blend_ragdoll_to_animation', toolName: 'animation_physics', arguments: { action: 'blend_ragdoll_to_animation', actorName: 'SkeletalActor', blendTime: 0.5 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: get_bone_transforms', toolName: 'animation_physics', arguments: { action: 'get_bone_transforms', actorName: 'SkeletalActor', boneNames: ['root', 'pelvis', 'spine_01'] }, expected: 'success|not found' },
  
  // === EXTENDED ANIMATION PHYSICS (Additional ~150 tests for comprehensive coverage) ===
  // More State Machine Variations
  { scenario: 'AnimPhys: add_state_machine_combat', toolName: 'animation_physics', arguments: { action: 'add_state_machine', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_attack1', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', stateName: 'Attack1' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_attack2', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', stateName: 'Attack2' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_attack3', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', stateName: 'Attack3' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_block', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', stateName: 'Block' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_dodge', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', stateName: 'Dodge' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_state_stunned', toolName: 'animation_physics', arguments: { action: 'add_state', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', stateName: 'Stunned' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_transition_attack_combo', toolName: 'animation_physics', arguments: { action: 'add_transition', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', fromState: 'Attack1', toState: 'Attack2' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_transition_attack2_attack3', toolName: 'animation_physics', arguments: { action: 'add_transition', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', fromState: 'Attack2', toState: 'Attack3' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_transition_rules_combo', toolName: 'animation_physics', arguments: { action: 'set_transition_rules', animBpPath: `${TEST_FOLDER}/ABP_Combat`, stateMachineName: 'CombatStates', fromState: 'Attack1', toState: 'Attack2', automaticTriggerRule: 'TimeRemaining', automaticTriggerTime: 0.3 }, expected: 'success|not found' },

  // More Animation Sequence Operations
  { scenario: 'AnimPhys: create_animation_sequence_attack', toolName: 'animation_physics', arguments: { action: 'create_animation_sequence', name: 'AS_Attack', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', numFrames: 45, frameRate: 30 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_animation_sequence_crouch', toolName: 'animation_physics', arguments: { action: 'create_animation_sequence', name: 'AS_Crouch', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', numFrames: 20, frameRate: 30 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_animation_sequence_jump_start', toolName: 'animation_physics', arguments: { action: 'create_animation_sequence', name: 'AS_JumpStart', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', numFrames: 15, frameRate: 30 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_animation_sequence_jump_loop', toolName: 'animation_physics', arguments: { action: 'create_animation_sequence', name: 'AS_JumpLoop', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', numFrames: 30, frameRate: 30 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_animation_sequence_land', toolName: 'animation_physics', arguments: { action: 'create_animation_sequence', name: 'AS_Land', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', numFrames: 25, frameRate: 30 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_bone_track_spine', toolName: 'animation_physics', arguments: { action: 'add_bone_track', assetPath: `${TEST_FOLDER}/AS_Attack`, boneName: 'spine_01' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_bone_track_arm', toolName: 'animation_physics', arguments: { action: 'add_bone_track', assetPath: `${TEST_FOLDER}/AS_Attack`, boneName: 'upperarm_r' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_bone_key_midframe', toolName: 'animation_physics', arguments: { action: 'set_bone_key', assetPath: `${TEST_FOLDER}/AS_Attack`, boneName: 'upperarm_r', frame: 22, rotation: { x: 45, y: 0, z: 30 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_bone_key_endframe', toolName: 'animation_physics', arguments: { action: 'set_bone_key', assetPath: `${TEST_FOLDER}/AS_Attack`, boneName: 'upperarm_r', frame: 44, rotation: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_curve_key_blend', toolName: 'animation_physics', arguments: { action: 'set_curve_key', assetPath: `${TEST_FOLDER}/AS_Attack`, curveName: 'BlendWeight', frame: 0, value: 0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_curve_key_blend_peak', toolName: 'animation_physics', arguments: { action: 'set_curve_key', assetPath: `${TEST_FOLDER}/AS_Attack`, curveName: 'BlendWeight', frame: 22, value: 1 }, expected: 'success|not found' },

  // More Montage Operations
  { scenario: 'AnimPhys: create_montage_reload', toolName: 'animation_physics', arguments: { action: 'create_montage', name: 'AM_Reload', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_montage_hit_react', toolName: 'animation_physics', arguments: { action: 'create_montage', name: 'AM_HitReact', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_montage_death', toolName: 'animation_physics', arguments: { action: 'create_montage', name: 'AM_Death', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_section_eject', toolName: 'animation_physics', arguments: { action: 'add_montage_section', assetPath: `${TEST_FOLDER}/AM_Reload`, sectionName: 'EjectMag', startTime: 0, length: 0.3 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_section_insert', toolName: 'animation_physics', arguments: { action: 'add_montage_section', assetPath: `${TEST_FOLDER}/AM_Reload`, sectionName: 'InsertMag', startTime: 0.3, length: 0.4 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_section_chamber', toolName: 'animation_physics', arguments: { action: 'add_montage_section', assetPath: `${TEST_FOLDER}/AM_Reload`, sectionName: 'ChamberRound', startTime: 0.7, length: 0.3 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_slot_fullbody', toolName: 'animation_physics', arguments: { action: 'add_montage_slot', assetPath: `${TEST_FOLDER}/AM_Death`, slotName: 'FullBody' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_notify_sound', toolName: 'animation_physics', arguments: { action: 'add_montage_notify', assetPath: `${TEST_FOLDER}/AM_Reload`, notifyName: 'PlayMagEjectSound', time: 0.15 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_montage_notify_vfx', toolName: 'animation_physics', arguments: { action: 'add_montage_notify', assetPath: `${TEST_FOLDER}/AM_Reload`, notifyName: 'SpawnBrassParticle', time: 0.18 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: link_sections_reload', toolName: 'animation_physics', arguments: { action: 'link_sections', assetPath: `${TEST_FOLDER}/AM_Reload`, fromSection: 'EjectMag', toSection: 'InsertMag' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: link_sections_reload2', toolName: 'animation_physics', arguments: { action: 'link_sections', assetPath: `${TEST_FOLDER}/AM_Reload`, fromSection: 'InsertMag', toSection: 'ChamberRound' }, expected: 'success|not found' },

  // More Blend Space Operations
  { scenario: 'AnimPhys: create_blend_space_strafe', toolName: 'animation_physics', arguments: { action: 'create_blend_space_2d', name: 'BS_Strafe', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', horizontalAxisName: 'Direction', verticalAxisName: 'Speed' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_blend_space_aim', toolName: 'animation_physics', arguments: { action: 'create_blend_space_2d', name: 'BS_AimOffset', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton', horizontalAxisName: 'Yaw', verticalAxisName: 'Pitch' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_axis_settings_direction', toolName: 'animation_physics', arguments: { action: 'set_axis_settings', assetPath: `${TEST_FOLDER}/BS_Strafe`, axis: 'Horizontal', axisName: 'Direction', minValue: -180, maxValue: 180 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_axis_settings_speed', toolName: 'animation_physics', arguments: { action: 'set_axis_settings', assetPath: `${TEST_FOLDER}/BS_Strafe`, axis: 'Vertical', axisName: 'Speed', minValue: 0, maxValue: 600 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_blend_sample_fwd_walk', toolName: 'animation_physics', arguments: { action: 'add_blend_sample', assetPath: `${TEST_FOLDER}/BS_Strafe`, animationPath: `${TEST_FOLDER}/AS_WalkFwd`, sampleValue: { x: 0, y: 200 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_blend_sample_fwd_run', toolName: 'animation_physics', arguments: { action: 'add_blend_sample', assetPath: `${TEST_FOLDER}/BS_Strafe`, animationPath: `${TEST_FOLDER}/AS_RunFwd`, sampleValue: { x: 0, y: 600 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_blend_sample_left', toolName: 'animation_physics', arguments: { action: 'add_blend_sample', assetPath: `${TEST_FOLDER}/BS_Strafe`, animationPath: `${TEST_FOLDER}/AS_StrafeLeft`, sampleValue: { x: -90, y: 400 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_blend_sample_right', toolName: 'animation_physics', arguments: { action: 'add_blend_sample', assetPath: `${TEST_FOLDER}/BS_Strafe`, animationPath: `${TEST_FOLDER}/AS_StrafeRight`, sampleValue: { x: 90, y: 400 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_interpolation_settings_strafe', toolName: 'animation_physics', arguments: { action: 'set_interpolation_settings', assetPath: `${TEST_FOLDER}/BS_Strafe`, interpolationType: 'Smoothed', targetWeightInterpolationSpeed: 8.0 }, expected: 'success|not found' },

  // More Control Rig Operations
  { scenario: 'AnimPhys: create_control_rig_fullbody', toolName: 'animation_physics', arguments: { action: 'create_control_rig', name: 'CR_FullBody', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control_pelvis', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_FullBody`, controlName: 'Pelvis_Control', controlType: 'Transform', parentBone: 'pelvis' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control_spine', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_FullBody`, controlName: 'Spine_Control', controlType: 'Transform', parentBone: 'spine_01' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control_chest', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_FullBody`, controlName: 'Chest_Control', controlType: 'Transform', parentBone: 'spine_03' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control_head', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_FullBody`, controlName: 'Head_Control', controlType: 'Transform', parentBone: 'head' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control_hand_l', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_FullBody`, controlName: 'Hand_L_Control', controlType: 'Transform', parentBone: 'hand_l' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control_hand_r', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_FullBody`, controlName: 'Hand_R_Control', controlType: 'Transform', parentBone: 'hand_r' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control_foot_l', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_FullBody`, controlName: 'Foot_L_Control', controlType: 'Transform', parentBone: 'foot_l' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_control_foot_r', toolName: 'animation_physics', arguments: { action: 'add_control', assetPath: `${TEST_FOLDER}/CR_FullBody`, controlName: 'Foot_R_Control', controlType: 'Transform', parentBone: 'foot_r' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_rig_unit_spine_ik', toolName: 'animation_physics', arguments: { action: 'add_rig_unit', assetPath: `${TEST_FOLDER}/CR_FullBody`, unitType: 'SplineIK', unitName: 'Spine_IK' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_rig_unit_twistbone', toolName: 'animation_physics', arguments: { action: 'add_rig_unit', assetPath: `${TEST_FOLDER}/CR_FullBody`, unitType: 'TwistBones', unitName: 'ForearmTwist' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_rig_unit_aim', toolName: 'animation_physics', arguments: { action: 'add_rig_unit', assetPath: `${TEST_FOLDER}/CR_FullBody`, unitType: 'Aim', unitName: 'Head_Aim' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: connect_rig_elements_spine', toolName: 'animation_physics', arguments: { action: 'connect_rig_elements', assetPath: `${TEST_FOLDER}/CR_FullBody`, sourceElement: 'Pelvis_Control', sourcePin: 'Transform', targetElement: 'Spine_IK', targetPin: 'RootTransform' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_control_value_pelvis', toolName: 'animation_physics', arguments: { action: 'set_control_value', actorName: 'SkeletalActor', controlName: 'Pelvis_Control', value: { x: 0, y: 0, z: 90 } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_control_value_head', toolName: 'animation_physics', arguments: { action: 'set_control_value', actorName: 'SkeletalActor', controlName: 'Head_Control', value: { x: 15, y: 30, z: 0 } }, expected: 'success|not found' },

  // More IK Rig Operations
  { scenario: 'AnimPhys: create_ik_rig_biped', toolName: 'animation_physics', arguments: { action: 'create_ik_rig', name: 'IKRig_Biped', path: TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_chain_left_leg', toolName: 'animation_physics', arguments: { action: 'add_ik_chain', assetPath: `${TEST_FOLDER}/IKRig_Biped`, chainName: 'LeftLeg', startBone: 'thigh_l', endBone: 'foot_l' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_chain_right_leg', toolName: 'animation_physics', arguments: { action: 'add_ik_chain', assetPath: `${TEST_FOLDER}/IKRig_Biped`, chainName: 'RightLeg', startBone: 'thigh_r', endBone: 'foot_r' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_chain_right_arm', toolName: 'animation_physics', arguments: { action: 'add_ik_chain', assetPath: `${TEST_FOLDER}/IKRig_Biped`, chainName: 'RightArm', startBone: 'clavicle_r', endBone: 'hand_r' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_chain_spine', toolName: 'animation_physics', arguments: { action: 'add_ik_chain', assetPath: `${TEST_FOLDER}/IKRig_Biped`, chainName: 'Spine', startBone: 'pelvis', endBone: 'head' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_goal_left_foot', toolName: 'animation_physics', arguments: { action: 'add_ik_goal', assetPath: `${TEST_FOLDER}/IKRig_Biped`, goal: 'LeftFoot_Goal', chainName: 'LeftLeg' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_goal_right_foot', toolName: 'animation_physics', arguments: { action: 'add_ik_goal', assetPath: `${TEST_FOLDER}/IKRig_Biped`, goal: 'RightFoot_Goal', chainName: 'RightLeg' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_ik_goal_right_hand', toolName: 'animation_physics', arguments: { action: 'add_ik_goal', assetPath: `${TEST_FOLDER}/IKRig_Biped`, goal: 'RightHand_Goal', chainName: 'RightArm' }, expected: 'success|not found' },

  // More IK Retargeter Operations
  { scenario: 'AnimPhys: create_ik_retargeter_mannequin', toolName: 'animation_physics', arguments: { action: 'create_ik_retargeter', name: 'RTG_Mannequin', path: TEST_FOLDER, sourceIKRigPath: `${TEST_FOLDER}/IKRig_Source`, targetIKRigPath: `${TEST_FOLDER}/IKRig_Mannequin` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_retarget_chain_mapping_spine', toolName: 'animation_physics', arguments: { action: 'set_retarget_chain_mapping', assetPath: `${TEST_FOLDER}/RTG_Mannequin`, sourceChain: 'Spine', targetChain: 'Spine' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_retarget_chain_mapping_left_leg', toolName: 'animation_physics', arguments: { action: 'set_retarget_chain_mapping', assetPath: `${TEST_FOLDER}/RTG_Mannequin`, sourceChain: 'LeftLeg', targetChain: 'LeftLeg' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_retarget_chain_mapping_right_leg', toolName: 'animation_physics', arguments: { action: 'set_retarget_chain_mapping', assetPath: `${TEST_FOLDER}/RTG_Mannequin`, sourceChain: 'RightLeg', targetChain: 'RightLeg' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_retarget_chain_mapping_right_arm', toolName: 'animation_physics', arguments: { action: 'set_retarget_chain_mapping', assetPath: `${TEST_FOLDER}/RTG_Mannequin`, sourceChain: 'RightArm', targetChain: 'RightArm' }, expected: 'success|not found' },

  // More Motion Matching Operations
  { scenario: 'AnimPhys: create_pose_search_database_combat', toolName: 'animation_physics', arguments: { action: 'create_pose_search_database', name: 'PSD_Combat', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_pose_search_database_traversal', toolName: 'animation_physics', arguments: { action: 'create_pose_search_database', name: 'PSD_Traversal', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: configure_motion_matching_combat', toolName: 'animation_physics', arguments: { action: 'configure_motion_matching', animBpPath: `${TEST_FOLDER}/ABP_Combat`, databasePath: `${TEST_FOLDER}/PSD_Combat` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_trajectory_prediction_combat', toolName: 'animation_physics', arguments: { action: 'add_trajectory_prediction', animBpPath: `${TEST_FOLDER}/ABP_Combat` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_motion_matching_goal_run', toolName: 'animation_physics', arguments: { action: 'set_motion_matching_goal', actorName: 'Player', goal: { velocity: { x: 600, y: 0, z: 0 }, facingDirection: { x: 1, y: 0, z: 0 } } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_motion_matching_goal_strafe', toolName: 'animation_physics', arguments: { action: 'set_motion_matching_goal', actorName: 'Player', goal: { velocity: { x: 0, y: 400, z: 0 }, facingDirection: { x: 1, y: 0, z: 0 } } }, expected: 'success|not found' },
  { scenario: 'AnimPhys: get_motion_matching_state_player', toolName: 'animation_physics', arguments: { action: 'get_motion_matching_state', actorName: 'Player' }, expected: 'success|not found' },

  // More Animation Modifier & ML
  { scenario: 'AnimPhys: create_animation_modifier_curves', toolName: 'animation_physics', arguments: { action: 'create_animation_modifier', name: 'AM_AddCurves', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: create_animation_modifier_sync', toolName: 'animation_physics', arguments: { action: 'create_animation_modifier', name: 'AM_SyncMarkers', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: setup_ml_deformer_face', toolName: 'animation_physics', arguments: { action: 'setup_ml_deformer', name: 'MLD_Face', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: configure_ml_deformer_training_high', toolName: 'animation_physics', arguments: { action: 'configure_ml_deformer_training', assetPath: `${TEST_FOLDER}/MLD_Face`, iterations: 5000 }, expected: 'success|not found' },

  // More CHAOS Destruction Extended
  { scenario: 'AnimPhys: chaos_create_geometry_collection_column', toolName: 'animation_physics', arguments: { action: 'chaos_create_geometry_collection', name: 'GC_Column', path: TEST_FOLDER, meshPath: '/Engine/BasicShapes/Cylinder' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_geometry_collection_floor', toolName: 'animation_physics', arguments: { action: 'chaos_create_geometry_collection', name: 'GC_Floor', path: TEST_FOLDER, meshPath: '/Engine/BasicShapes/Plane' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_fracture_uniform_high', toolName: 'animation_physics', arguments: { action: 'chaos_fracture_uniform', assetPath: `${TEST_FOLDER}/GC_Column`, siteCount: 100 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_fracture_clustered_nested', toolName: 'animation_physics', arguments: { action: 'chaos_fracture_clustered', assetPath: `${TEST_FOLDER}/GC_Column`, clusterCount: 8, siteCount: 50 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_damage_thresholds_low', toolName: 'animation_physics', arguments: { action: 'chaos_set_damage_thresholds', assetPath: `${TEST_FOLDER}/GC_Column`, damageThreshold: 50 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_remove_on_break_fast', toolName: 'animation_physics', arguments: { action: 'chaos_set_remove_on_break', assetPath: `${TEST_FOLDER}/GC_Column`, removeOnBreak: true, removeDelay: 2.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_field_system_actor_shockwave', toolName: 'animation_physics', arguments: { action: 'chaos_create_field_system_actor', actorName: 'FS_Shockwave', location: { x: 500, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'AnimPhys: chaos_add_field_radial_vector_strong', toolName: 'animation_physics', arguments: { action: 'chaos_add_field_radial_vector', actorName: 'FS_Shockwave', magnitude: 5000 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_field_strain_high', toolName: 'animation_physics', arguments: { action: 'chaos_add_field_strain', actorName: 'FS_Shockwave', magnitude: 2000 }, expected: 'success|not found' },

  // More CHAOS Vehicles Extended
  { scenario: 'AnimPhys: chaos_create_wheeled_vehicle_bp_truck', toolName: 'animation_physics', arguments: { action: 'chaos_create_wheeled_vehicle_bp', name: 'BP_Truck', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_wheeled_vehicle_bp_bike', toolName: 'animation_physics', arguments: { action: 'chaos_create_wheeled_vehicle_bp', name: 'BP_Motorcycle', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_vehicle_wheel_truck_fl', toolName: 'animation_physics', arguments: { action: 'chaos_add_vehicle_wheel', blueprintPath: `${TEST_FOLDER}/BP_Truck`, wheelName: 'FrontLeft', boneName: 'Wheel_FL' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_vehicle_wheel_truck_fr', toolName: 'animation_physics', arguments: { action: 'chaos_add_vehicle_wheel', blueprintPath: `${TEST_FOLDER}/BP_Truck`, wheelName: 'FrontRight', boneName: 'Wheel_FR' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_vehicle_wheel_truck_rl', toolName: 'animation_physics', arguments: { action: 'chaos_add_vehicle_wheel', blueprintPath: `${TEST_FOLDER}/BP_Truck`, wheelName: 'RearLeft', boneName: 'Wheel_RL' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_add_vehicle_wheel_truck_rr', toolName: 'animation_physics', arguments: { action: 'chaos_add_vehicle_wheel', blueprintPath: `${TEST_FOLDER}/BP_Truck`, wheelName: 'RearRight', boneName: 'Wheel_RR' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_engine_setup_truck', toolName: 'animation_physics', arguments: { action: 'chaos_configure_engine_setup', blueprintPath: `${TEST_FOLDER}/BP_Truck`, maxRPM: 4500, maxTorque: 800 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_transmission_setup_truck', toolName: 'animation_physics', arguments: { action: 'chaos_configure_transmission_setup', blueprintPath: `${TEST_FOLDER}/BP_Truck`, gearCount: 8, finalDriveRatio: 4.0 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_configure_suspension_setup_truck', toolName: 'animation_physics', arguments: { action: 'chaos_configure_suspension_setup', blueprintPath: `${TEST_FOLDER}/BP_Truck`, springRate: 400, dampingRatio: 0.6 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_vehicle_mass_truck', toolName: 'animation_physics', arguments: { action: 'chaos_set_vehicle_mass', blueprintPath: `${TEST_FOLDER}/BP_Truck`, mass: 5000 }, expected: 'success|not found' },

  // More CHAOS Cloth Extended
  { scenario: 'AnimPhys: chaos_create_cloth_config_dress', toolName: 'animation_physics', arguments: { action: 'chaos_create_cloth_config', name: 'CC_Dress', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_create_cloth_config_hair', toolName: 'animation_physics', arguments: { action: 'chaos_create_cloth_config', name: 'CC_Hair', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_gravity_high', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_gravity', clothConfigPath: `${TEST_FOLDER}/CC_Dress`, gravityScale: 1.5, useGravityOverride: false }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_damping_light', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_damping', clothConfigPath: `${TEST_FOLDER}/CC_Dress`, damping: 0.005 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_stiffness_soft', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_stiffness', clothConfigPath: `${TEST_FOLDER}/CC_Dress`, stretchStiffness: 0.5, bendStiffness: 0.3 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: chaos_set_cloth_aerodynamics_wind', toolName: 'animation_physics', arguments: { action: 'chaos_set_cloth_aerodynamics', clothConfigPath: `${TEST_FOLDER}/CC_Dress`, dragCoefficient: 0.8, liftCoefficient: 0.6 }, expected: 'success|not found' },

  // More Blend Node Operations
  { scenario: 'AnimPhys: add_blend_node_aim', toolName: 'animation_physics', arguments: { action: 'add_blend_node', animBpPath: `${TEST_FOLDER}/ABP_Test`, blendType: 'AimOffset', nodeName: 'AimOffsetNode', x: 400, y: 200 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_blend_node_additive', toolName: 'animation_physics', arguments: { action: 'add_blend_node', animBpPath: `${TEST_FOLDER}/ABP_Test`, blendType: 'ApplyAdditive', nodeName: 'AdditiveNode', x: 600, y: 200 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_blend_node_twoBone', toolName: 'animation_physics', arguments: { action: 'add_blend_node', animBpPath: `${TEST_FOLDER}/ABP_Test`, blendType: 'TwoBoneIK', nodeName: 'HandIK', x: 800, y: 200 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_cached_pose_lowerBody', toolName: 'animation_physics', arguments: { action: 'add_cached_pose', animBpPath: `${TEST_FOLDER}/ABP_Test`, cacheName: 'LowerBodyPose' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_slot_node_upperBody', toolName: 'animation_physics', arguments: { action: 'add_slot_node', animBpPath: `${TEST_FOLDER}/ABP_Test`, slotName: 'UpperBodySlot' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_layered_blend_per_bone_upper', toolName: 'animation_physics', arguments: { action: 'add_layered_blend_per_bone', animBpPath: `${TEST_FOLDER}/ABP_Test`, layerSetup: [{ boneName: 'spine_02', depth: 0 }, { boneName: 'spine_03', depth: 1 }] }, expected: 'success|not found' },

  // More Notify Operations
  { scenario: 'AnimPhys: add_notify_footstep', toolName: 'animation_physics', arguments: { action: 'add_notify', animationPath: `${TEST_FOLDER}/AS_Run`, notifyClass: 'AnimNotify_PlayFootstep', time: 0.25 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_notify_footstep2', toolName: 'animation_physics', arguments: { action: 'add_notify', animationPath: `${TEST_FOLDER}/AS_Run`, notifyClass: 'AnimNotify_PlayFootstep', time: 0.75 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_notify_state_weapontrail', toolName: 'animation_physics', arguments: { action: 'add_notify_state', animationPath: `${TEST_FOLDER}/AM_Attack`, notifyClass: 'AnimNotifyState_WeaponTrail', startFrame: 15, endFrame: 35 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_sync_marker_leftfoot', toolName: 'animation_physics', arguments: { action: 'add_sync_marker', assetPath: `${TEST_FOLDER}/AS_Run`, markerName: 'LeftFootDown', frame: 8 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: add_sync_marker_rightfoot', toolName: 'animation_physics', arguments: { action: 'add_sync_marker', assetPath: `${TEST_FOLDER}/AS_Run`, markerName: 'RightFootDown', frame: 22 }, expected: 'success|not found' },

  // More Root Motion & Additive
  { scenario: 'AnimPhys: set_root_motion_settings_roll', toolName: 'animation_physics', arguments: { action: 'set_root_motion_settings', assetPath: `${TEST_FOLDER}/AS_Roll`, enableRootMotion: true, forceRootLock: true }, expected: 'success|not found' },
  { scenario: 'AnimPhys: set_additive_settings_overlay', toolName: 'animation_physics', arguments: { action: 'set_additive_settings', assetPath: `${TEST_FOLDER}/AS_Aim`, additiveAnimType: 'MeshSpace', basePoseType: 'RefPose' }, expected: 'success|not found' },

  // More Ragdoll & Physics Extended
  { scenario: 'AnimPhys: setup_ragdoll_enemy', toolName: 'animation_physics', arguments: { action: 'setup_ragdoll', skeletalMeshPath: `${TEST_FOLDER}/SK_Enemy` }, expected: 'success|not found' },
  { scenario: 'AnimPhys: activate_ragdoll_enemy', toolName: 'animation_physics', arguments: { action: 'activate_ragdoll', actorName: 'Enemy_01' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: blend_ragdoll_to_animation_fast', toolName: 'animation_physics', arguments: { action: 'blend_ragdoll_to_animation', actorName: 'Enemy_01', blendTime: 0.25 }, expected: 'success|not found' },
  { scenario: 'AnimPhys: configure_ragdoll_profile_ragdoll', toolName: 'animation_physics', arguments: { action: 'configure_ragdoll_profile', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, profileName: 'FullRagdoll' }, expected: 'success|not found' },
  { scenario: 'AnimPhys: setup_physics_simulation_arm', toolName: 'animation_physics', arguments: { action: 'setup_physics_simulation', actorName: 'Player', boneName: 'upperarm_r' }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_EFFECT (74 actions) - FULL COVERAGE
// ============================================================================
const manageEffectTests = [
  // === Niagara Systems Creation ===
  { scenario: 'Effect: create_niagara_system', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'NS_Fire', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: create_niagara_emitter', toolName: 'manage_effect', arguments: { action: 'create_niagara_emitter', name: 'NE_Flames', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: get_niagara_info', toolName: 'manage_effect', arguments: { action: 'get_niagara_info', systemPath: `${TEST_FOLDER}/NS_Fire` }, expected: 'success|not found' },
  { scenario: 'Effect: validate_niagara_system', toolName: 'manage_effect', arguments: { action: 'validate_niagara_system', systemPath: `${TEST_FOLDER}/NS_Fire` }, expected: 'success|not found' },
  
  // === Emitter Management ===
  { scenario: 'Effect: add_emitter_to_system', toolName: 'manage_effect', arguments: { action: 'add_emitter_to_system', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterPath: `${TEST_FOLDER}/NE_Flames` }, expected: 'success|not found' },
  { scenario: 'Effect: set_emitter_properties', toolName: 'manage_effect', arguments: { action: 'set_emitter_properties', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', emitterProperties: { scalabilityMode: 'Self' } }, expected: 'success|not found' },
  
  // === Spawn Modules ===
  { scenario: 'Effect: add_spawn_rate_module', toolName: 'manage_effect', arguments: { action: 'add_spawn_rate_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', spawnRate: 100 }, expected: 'success|not found' },
  { scenario: 'Effect: add_spawn_burst_module', toolName: 'manage_effect', arguments: { action: 'add_spawn_burst_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Sparks', burstCount: 50, burstTime: 0 }, expected: 'success|not found' },
  { scenario: 'Effect: add_spawn_per_unit_module', toolName: 'manage_effect', arguments: { action: 'add_spawn_per_unit_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Trail', spawnPerUnit: 10 }, expected: 'success|not found' },
  
  // === Particle Initialization ===
  { scenario: 'Effect: add_initialize_particle_module', toolName: 'manage_effect', arguments: { action: 'add_initialize_particle_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', lifetime: 1.5, mass: 1.0 }, expected: 'success|not found' },
  { scenario: 'Effect: add_particle_state_module', toolName: 'manage_effect', arguments: { action: 'add_particle_state_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames' }, expected: 'success|not found' },
  
  // === Force Modules ===
  { scenario: 'Effect: add_force_module', toolName: 'manage_effect', arguments: { action: 'add_force_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Smoke', forceType: 'Gravity', forceStrength: -980 }, expected: 'success|not found' },
  { scenario: 'Effect: add_velocity_module', toolName: 'manage_effect', arguments: { action: 'add_velocity_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', velocityMode: 'Linear', velocity: { x: 0, y: 0, z: 200 } }, expected: 'success|not found' },
  { scenario: 'Effect: add_acceleration_module', toolName: 'manage_effect', arguments: { action: 'add_acceleration_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Sparks', acceleration: { x: 0, y: 0, z: -100 } }, expected: 'success|not found' },
  
  // === Size & Color Modules ===
  { scenario: 'Effect: add_size_module', toolName: 'manage_effect', arguments: { action: 'add_size_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', sizeMode: 'Uniform', uniformSize: 50 }, expected: 'success|not found' },
  { scenario: 'Effect: add_color_module', toolName: 'manage_effect', arguments: { action: 'add_color_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', colorMode: 'LinearGradient', colorMin: { r: 1, g: 0.5, b: 0, a: 1 }, colorMax: { r: 1, g: 0, b: 0, a: 0 } }, expected: 'success|not found' },
  
  // === Renderers ===
  { scenario: 'Effect: add_sprite_renderer_module', toolName: 'manage_effect', arguments: { action: 'add_sprite_renderer_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', materialPath: '/Engine/EngineMaterials/DefaultParticle' }, expected: 'success|not found' },
  { scenario: 'Effect: add_mesh_renderer_module', toolName: 'manage_effect', arguments: { action: 'add_mesh_renderer_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Sparks', meshPath: '/Engine/BasicShapes/Sphere' }, expected: 'success|not found' },
  { scenario: 'Effect: add_ribbon_renderer_module', toolName: 'manage_effect', arguments: { action: 'add_ribbon_renderer_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Trail', materialPath: '/Engine/EngineMaterials/DefaultParticle', ribbonWidth: 20 }, expected: 'success|not found' },
  { scenario: 'Effect: add_light_renderer_module', toolName: 'manage_effect', arguments: { action: 'add_light_renderer_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', lightRadius: 200, lightIntensity: 10 }, expected: 'success|not found' },
  
  // === Collision & Kill ===
  { scenario: 'Effect: add_collision_module', toolName: 'manage_effect', arguments: { action: 'add_collision_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Sparks', collisionMode: 'GPU', restitution: 0.3, friction: 0.5 }, expected: 'success|not found' },
  { scenario: 'Effect: add_kill_particles_module', toolName: 'manage_effect', arguments: { action: 'add_kill_particles_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Smoke', killCondition: 'Box', killBox: { min: { x: -1000, y: -1000, z: 0 }, max: { x: 1000, y: 1000, z: 500 } } }, expected: 'success|not found' },
  { scenario: 'Effect: add_camera_offset_module', toolName: 'manage_effect', arguments: { action: 'add_camera_offset_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', cameraOffset: 5 }, expected: 'success|not found' },
  
  // === User Parameters ===
  { scenario: 'Effect: add_user_parameter', toolName: 'manage_effect', arguments: { action: 'add_user_parameter', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.Intensity', parameterType: 'Float', value: 1.0 }, expected: 'success|not found' },
  { scenario: 'Effect: set_parameter_value', toolName: 'manage_effect', arguments: { action: 'set_parameter_value', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.Intensity', parameterValue: 2.0 }, expected: 'success|not found' },
  { scenario: 'Effect: bind_parameter_to_source', toolName: 'manage_effect', arguments: { action: 'bind_parameter_to_source', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.Intensity', sourceBinding: 'Actor.RelativeScale3D.X' }, expected: 'success|not found' },
  { scenario: 'Effect: get_niagara_parameters', toolName: 'manage_effect', arguments: { action: 'get_niagara_parameters', systemPath: `${TEST_FOLDER}/NS_Fire` }, expected: 'success|not found' },
  { scenario: 'Effect: set_niagara_variable', toolName: 'manage_effect', arguments: { action: 'set_niagara_variable', actorName: 'Fire_01', parameterName: 'User.Intensity', value: 3.0 }, expected: 'success|not found' },
  { scenario: 'Effect: set_niagara_parameter', toolName: 'manage_effect', arguments: { action: 'set_niagara_parameter', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.Scale', value: 1.5 }, expected: 'success|not found' },
  
  // === Data Interfaces ===
  { scenario: 'Effect: add_skeletal_mesh_data_interface', toolName: 'manage_effect', arguments: { action: 'add_skeletal_mesh_data_interface', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube' }, expected: 'success|not found' },
  { scenario: 'Effect: add_static_mesh_data_interface', toolName: 'manage_effect', arguments: { action: 'add_static_mesh_data_interface', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Sparks', staticMeshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Effect: add_spline_data_interface', toolName: 'manage_effect', arguments: { action: 'add_spline_data_interface', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Trail' }, expected: 'success|not found' },
  { scenario: 'Effect: add_audio_spectrum_data_interface', toolName: 'manage_effect', arguments: { action: 'add_audio_spectrum_data_interface', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'AudioReactive' }, expected: 'success|not found' },
  { scenario: 'Effect: add_collision_query_data_interface', toolName: 'manage_effect', arguments: { action: 'add_collision_query_data_interface', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Sparks' }, expected: 'success|not found' },
  { scenario: 'Effect: add_data_interface', toolName: 'manage_effect', arguments: { action: 'add_data_interface', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', className: 'SkeletalMesh' }, expected: 'success|not found' },
  { scenario: 'Effect: create_niagara_data_interface', toolName: 'manage_effect', arguments: { action: 'create_niagara_data_interface', name: 'NDI_Custom', path: TEST_FOLDER, className: 'Custom' }, expected: 'success|not found' },
  
  // === Events ===
  { scenario: 'Effect: add_event_generator', toolName: 'manage_effect', arguments: { action: 'add_event_generator', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', eventName: 'OnDeath' }, expected: 'success|not found' },
  { scenario: 'Effect: add_event_receiver', toolName: 'manage_effect', arguments: { action: 'add_event_receiver', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Sparks', eventName: 'OnDeath', spawnOnEvent: true, eventSpawnCount: 5 }, expected: 'success|not found' },
  { scenario: 'Effect: configure_event_payload', toolName: 'manage_effect', arguments: { action: 'configure_event_payload', systemPath: `${TEST_FOLDER}/NS_Fire`, eventName: 'OnDeath', eventPayload: [{ name: 'Velocity', type: 'Vector' }] }, expected: 'success|not found' },
  
  // === GPU & Simulation ===
  { scenario: 'Effect: enable_gpu_simulation', toolName: 'manage_effect', arguments: { action: 'enable_gpu_simulation', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', gpuEnabled: true }, expected: 'success|not found' },
  { scenario: 'Effect: add_simulation_stage', toolName: 'manage_effect', arguments: { action: 'add_simulation_stage', systemPath: `${TEST_FOLDER}/NS_Fire`, stageName: 'CustomStage', stageIterationSource: 'Particles' }, expected: 'success|not found' },
  { scenario: 'Effect: configure_gpu_simulation', toolName: 'manage_effect', arguments: { action: 'configure_gpu_simulation', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', fixedBoundsEnabled: true }, expected: 'success|not found' },
  { scenario: 'Effect: configure_niagara_determinism', toolName: 'manage_effect', arguments: { action: 'configure_niagara_determinism', systemPath: `${TEST_FOLDER}/NS_Fire`, deterministicEnabled: true }, expected: 'success|not found' },
  { scenario: 'Effect: configure_niagara_lod', toolName: 'manage_effect', arguments: { action: 'configure_niagara_lod', systemPath: `${TEST_FOLDER}/NS_Fire`, lodDistance: 1000, lodBias: 0 }, expected: 'success|not found' },
  
  // === Runtime Control ===
  { scenario: 'Effect: spawn_niagara', toolName: 'manage_effect', arguments: { action: 'spawn_niagara', actorName: 'Fire_01', systemPath: `${TEST_FOLDER}/NS_Fire`, location: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Effect: activate', toolName: 'manage_effect', arguments: { action: 'activate', actorName: 'Fire_01' }, expected: 'success|not found' },
  { scenario: 'Effect: activate_effect', toolName: 'manage_effect', arguments: { action: 'activate_effect', actorName: 'Fire_01' }, expected: 'success|not found' },
  { scenario: 'Effect: deactivate', toolName: 'manage_effect', arguments: { action: 'deactivate', actorName: 'Fire_01' }, expected: 'success|not found' },
  { scenario: 'Effect: reset', toolName: 'manage_effect', arguments: { action: 'reset', actorName: 'Fire_01' }, expected: 'success|not found' },
  { scenario: 'Effect: advance_simulation', toolName: 'manage_effect', arguments: { action: 'advance_simulation', actorName: 'Fire_01', time: 0.5 }, expected: 'success|not found' },
  
  // === Debug Shapes ===
  { scenario: 'Effect: draw_debug_sphere', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'sphere', location: { x: 0, y: 0, z: 100 }, size: 50, color: { r: 1, g: 0, b: 0 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_box', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'box', location: { x: 200, y: 0, z: 100 }, size: 50, color: { r: 0, g: 1, b: 0 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_line', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'line', location: { x: 0, y: 0, z: 0 }, size: 500, color: { r: 0, g: 0, b: 1 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_arrow', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'arrow', location: { x: 0, y: 100, z: 0 }, size: 100, color: { r: 1, g: 1, b: 0 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_capsule', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'capsule', location: { x: 400, y: 0, z: 100 }, size: 50, color: { r: 1, g: 0, b: 1 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_cone', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'cone', location: { x: 600, y: 0, z: 100 }, size: 50, color: { r: 0, g: 1, b: 1 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_cylinder', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'cylinder', location: { x: 800, y: 0, z: 100 }, size: 50, color: { r: 1, g: 0.5, b: 0 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_plane', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'plane', location: { x: 0, y: 200, z: 50 }, size: 200, color: { r: 0.5, g: 0.5, b: 0.5 } }, expected: 'success' },
  { scenario: 'Effect: clear_debug_shapes', toolName: 'manage_effect', arguments: { action: 'clear_debug_shapes' }, expected: 'success' },
  { scenario: 'Effect: list_debug_shapes', toolName: 'manage_effect', arguments: { action: 'list_debug_shapes' }, expected: 'success' },
  
  // === Advanced Features ===
  { scenario: 'Effect: create_niagara_module', toolName: 'manage_effect', arguments: { action: 'create_niagara_module', name: 'NM_CustomForce', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Effect: add_niagara_script', toolName: 'manage_effect', arguments: { action: 'add_niagara_script', modulePath: `${TEST_FOLDER}/NM_CustomForce`, stage: 'Update' }, expected: 'success|not found' },
  { scenario: 'Effect: add_niagara_module', toolName: 'manage_effect', arguments: { action: 'add_niagara_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', modulePath: `${TEST_FOLDER}/NM_CustomForce` }, expected: 'success|not found' },
  { scenario: 'Effect: connect_niagara_pins', toolName: 'manage_effect', arguments: { action: 'connect_niagara_pins', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', sourceNode: 'SpawnRate', pinName: 'SpawnRate', linkedTo: 'User.Intensity' }, expected: 'success|not found' },
  { scenario: 'Effect: remove_niagara_node', toolName: 'manage_effect', arguments: { action: 'remove_niagara_node', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', nodeName: 'OldModule' }, expected: 'success|not found' },
  
  // === Fluids ===
  { scenario: 'Effect: setup_niagara_fluids', toolName: 'manage_effect', arguments: { action: 'setup_niagara_fluids', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Smoke', fluidType: '3D' }, expected: 'success|not found' },
  { scenario: 'Effect: create_fluid_simulation', toolName: 'manage_effect', arguments: { action: 'create_fluid_simulation', name: 'NS_Fluid', path: TEST_FOLDER, fluidType: '2D' }, expected: 'success|not found' },
  { scenario: 'Effect: add_chaos_integration', toolName: 'manage_effect', arguments: { action: 'add_chaos_integration', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Debris' }, expected: 'success|not found' },
  
  // === Export/Import ===
  { scenario: 'Effect: create_niagara_sim_cache', toolName: 'manage_effect', arguments: { action: 'create_niagara_sim_cache', name: 'NSC_Fire', path: TEST_FOLDER, systemPath: `${TEST_FOLDER}/NS_Fire` }, expected: 'success|not found' },
  { scenario: 'Effect: export_niagara_system', toolName: 'manage_effect', arguments: { action: 'export_niagara_system', systemPath: `${TEST_FOLDER}/NS_Fire`, exportPath: `${TEST_FOLDER}/Export_Fire.json` }, expected: 'success|not found' },
  { scenario: 'Effect: import_niagara_module', toolName: 'manage_effect', arguments: { action: 'import_niagara_module', importPath: `${TEST_FOLDER}/Module.json`, path: TEST_FOLDER, name: 'NM_Imported' }, expected: 'success|not found' },
  { scenario: 'Effect: batch_compile_niagara', toolName: 'manage_effect', arguments: { action: 'batch_compile_niagara', systemPaths: [`${TEST_FOLDER}/NS_Fire`, `${TEST_FOLDER}/NS_Smoke`] }, expected: 'success|not found' },
  
  // === Legacy/Other ===
  { scenario: 'Effect: particle', toolName: 'manage_effect', arguments: { action: 'particle', preset: '/Engine/Particles/P_Explosion', location: { x: 0, y: 0, z: 0 }, scale: 1.0 }, expected: 'success|not found' },
  { scenario: 'Effect: niagara', toolName: 'manage_effect', arguments: { action: 'niagara', systemPath: `${TEST_FOLDER}/NS_Fire`, location: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Effect: create_dynamic_light', toolName: 'manage_effect', arguments: { action: 'create_dynamic_light', actorName: 'DynLight_01', location: { x: 0, y: 0, z: 200 }, color: { r: 1, g: 0.5, b: 0 } }, expected: 'success' },
  { scenario: 'Effect: create_volumetric_fog', toolName: 'manage_effect', arguments: { action: 'create_volumetric_fog', actorName: 'VolFog_01', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'Effect: create_particle_trail', toolName: 'manage_effect', arguments: { action: 'create_particle_trail', actorName: 'Trail_01', systemPath: `${TEST_FOLDER}/NS_Fire` }, expected: 'success|not found' },
  { scenario: 'Effect: create_environment_effect', toolName: 'manage_effect', arguments: { action: 'create_environment_effect', name: 'Rain', location: { x: 0, y: 0, z: 500 } }, expected: 'success|not found' },
  { scenario: 'Effect: create_impact_effect', toolName: 'manage_effect', arguments: { action: 'create_impact_effect', surfaceType: 'Metal', location: { x: 100, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'Effect: create_niagara_ribbon', toolName: 'manage_effect', arguments: { action: 'create_niagara_ribbon', name: 'NR_Trail', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Effect: cleanup', toolName: 'manage_effect', arguments: { action: 'cleanup' }, expected: 'success' },
  
  // === EXTENDED MANAGE_EFFECT (Additional ~80 tests for comprehensive Niagara coverage) ===
  // More Niagara System Variations
  { scenario: 'Effect: create_niagara_system_smoke', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'NS_Smoke', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: create_niagara_system_explosion', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'NS_Explosion', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: create_niagara_system_rain', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'NS_Rain', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: create_niagara_system_snow', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'NS_Snow', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: create_niagara_system_dust', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'NS_Dust', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: create_niagara_system_sparks', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'NS_Sparks', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: create_niagara_emitter_flames', toolName: 'manage_effect', arguments: { action: 'create_niagara_emitter', name: 'NE_SmokeCloud', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Effect: create_niagara_emitter_debris', toolName: 'manage_effect', arguments: { action: 'create_niagara_emitter', name: 'NE_Debris', path: TEST_FOLDER }, expected: 'success|already exists' },
  
  // More Spawn Module Variations
  { scenario: 'Effect: add_spawn_rate_module_high', toolName: 'manage_effect', arguments: { action: 'add_spawn_rate_module', systemPath: `${TEST_FOLDER}/NS_Rain`, emitterName: 'Drops', spawnRate: 500 }, expected: 'success|not found' },
  { scenario: 'Effect: add_spawn_rate_module_low', toolName: 'manage_effect', arguments: { action: 'add_spawn_rate_module', systemPath: `${TEST_FOLDER}/NS_Snow`, emitterName: 'Flakes', spawnRate: 50 }, expected: 'success|not found' },
  { scenario: 'Effect: add_spawn_burst_module_explosion', toolName: 'manage_effect', arguments: { action: 'add_spawn_burst_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Debris', burstCount: 200, burstTime: 0 }, expected: 'success|not found' },
  { scenario: 'Effect: add_spawn_burst_module_secondary', toolName: 'manage_effect', arguments: { action: 'add_spawn_burst_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Sparks', burstCount: 100, burstTime: 0.05 }, expected: 'success|not found' },
  { scenario: 'Effect: add_spawn_per_unit_module_trail', toolName: 'manage_effect', arguments: { action: 'add_spawn_per_unit_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'SmokeTrail', spawnPerUnit: 5 }, expected: 'success|not found' },
  
  // More Force Module Variations
  { scenario: 'Effect: add_force_module_wind', toolName: 'manage_effect', arguments: { action: 'add_force_module', systemPath: `${TEST_FOLDER}/NS_Smoke`, emitterName: 'Cloud', forceType: 'Wind', forceStrength: 200 }, expected: 'success|not found' },
  { scenario: 'Effect: add_force_module_vortex', toolName: 'manage_effect', arguments: { action: 'add_force_module', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', forceType: 'Vortex', forceStrength: 150 }, expected: 'success|not found' },
  { scenario: 'Effect: add_force_module_curl', toolName: 'manage_effect', arguments: { action: 'add_force_module', systemPath: `${TEST_FOLDER}/NS_Smoke`, emitterName: 'Cloud', forceType: 'Curl', forceStrength: 50 }, expected: 'success|not found' },
  { scenario: 'Effect: add_velocity_module_cone', toolName: 'manage_effect', arguments: { action: 'add_velocity_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Debris', velocityMode: 'Cone', velocity: { x: 0, y: 0, z: 500 } }, expected: 'success|not found' },
  { scenario: 'Effect: add_velocity_module_spherical', toolName: 'manage_effect', arguments: { action: 'add_velocity_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Sparks', velocityMode: 'Spherical', velocity: { x: 0, y: 0, z: 300 } }, expected: 'success|not found' },
  { scenario: 'Effect: add_acceleration_module_drag', toolName: 'manage_effect', arguments: { action: 'add_acceleration_module', systemPath: `${TEST_FOLDER}/NS_Smoke`, emitterName: 'Cloud', acceleration: { x: 0, y: 0, z: 50 } }, expected: 'success|not found' },
  
  // More Size & Color Variations
  { scenario: 'Effect: add_size_module_curve', toolName: 'manage_effect', arguments: { action: 'add_size_module', systemPath: `${TEST_FOLDER}/NS_Smoke`, emitterName: 'Cloud', sizeMode: 'Curve', uniformSize: 100 }, expected: 'success|not found' },
  { scenario: 'Effect: add_size_module_nonuniform', toolName: 'manage_effect', arguments: { action: 'add_size_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Debris', sizeMode: 'NonUniform', uniformSize: 30 }, expected: 'success|not found' },
  { scenario: 'Effect: add_color_module_gradient', toolName: 'manage_effect', arguments: { action: 'add_color_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Core', colorMode: 'RadialGradient', colorMin: { r: 1, g: 1, b: 0.5, a: 1 }, colorMax: { r: 1, g: 0.2, b: 0, a: 0.5 } }, expected: 'success|not found' },
  { scenario: 'Effect: add_color_module_random', toolName: 'manage_effect', arguments: { action: 'add_color_module', systemPath: `${TEST_FOLDER}/NS_Sparks`, emitterName: 'Sparks', colorMode: 'Random', colorMin: { r: 1, g: 0.5, b: 0, a: 1 }, colorMax: { r: 1, g: 1, b: 0.5, a: 1 } }, expected: 'success|not found' },
  
  // More Renderer Variations
  { scenario: 'Effect: add_sprite_renderer_module_smoke', toolName: 'manage_effect', arguments: { action: 'add_sprite_renderer_module', systemPath: `${TEST_FOLDER}/NS_Smoke`, emitterName: 'Cloud', materialPath: '/Engine/EngineMaterials/DefaultParticle' }, expected: 'success|not found' },
  { scenario: 'Effect: add_mesh_renderer_module_debris', toolName: 'manage_effect', arguments: { action: 'add_mesh_renderer_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Debris', meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Effect: add_mesh_renderer_module_rocks', toolName: 'manage_effect', arguments: { action: 'add_mesh_renderer_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Rocks', meshPath: '/Engine/BasicShapes/Cone' }, expected: 'success|not found' },
  { scenario: 'Effect: add_ribbon_renderer_module_trail', toolName: 'manage_effect', arguments: { action: 'add_ribbon_renderer_module', systemPath: `${TEST_FOLDER}/NS_Sparks`, emitterName: 'Trail', materialPath: '/Engine/EngineMaterials/DefaultParticle', ribbonWidth: 10 }, expected: 'success|not found' },
  { scenario: 'Effect: add_light_renderer_module_explosion', toolName: 'manage_effect', arguments: { action: 'add_light_renderer_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Core', lightRadius: 500, lightIntensity: 50 }, expected: 'success|not found' },
  
  // More Collision & Kill Variations
  { scenario: 'Effect: add_collision_module_rain', toolName: 'manage_effect', arguments: { action: 'add_collision_module', systemPath: `${TEST_FOLDER}/NS_Rain`, emitterName: 'Drops', collisionMode: 'CPU', restitution: 0.1, friction: 0.8 }, expected: 'success|not found' },
  { scenario: 'Effect: add_collision_module_debris', toolName: 'manage_effect', arguments: { action: 'add_collision_module', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Debris', collisionMode: 'GPU', restitution: 0.5, friction: 0.6 }, expected: 'success|not found' },
  { scenario: 'Effect: add_kill_particles_module_height', toolName: 'manage_effect', arguments: { action: 'add_kill_particles_module', systemPath: `${TEST_FOLDER}/NS_Rain`, emitterName: 'Drops', killCondition: 'Height', killBox: { min: { x: -10000, y: -10000, z: -100 }, max: { x: 10000, y: 10000, z: 0 } } }, expected: 'success|not found' },
  
  // More User Parameter Variations
  { scenario: 'Effect: add_user_parameter_color', toolName: 'manage_effect', arguments: { action: 'add_user_parameter', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.FlameColor', parameterType: 'LinearColor', value: { r: 1, g: 0.5, b: 0, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Effect: add_user_parameter_vector', toolName: 'manage_effect', arguments: { action: 'add_user_parameter', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.WindDirection', parameterType: 'Vector', value: { x: 1, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Effect: add_user_parameter_bool', toolName: 'manage_effect', arguments: { action: 'add_user_parameter', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.IsActive', parameterType: 'Bool', value: true }, expected: 'success|not found' },
  { scenario: 'Effect: set_parameter_value_color', toolName: 'manage_effect', arguments: { action: 'set_parameter_value', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.FlameColor', parameterValue: { r: 0, g: 0.5, b: 1, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Effect: bind_parameter_to_source_scale', toolName: 'manage_effect', arguments: { action: 'bind_parameter_to_source', systemPath: `${TEST_FOLDER}/NS_Fire`, parameterName: 'User.Intensity', sourceBinding: 'Actor.RelativeScale3D.Z' }, expected: 'success|not found' },
  
  // More Data Interface Variations
  { scenario: 'Effect: add_skeletal_mesh_data_interface_character', toolName: 'manage_effect', arguments: { action: 'add_skeletal_mesh_data_interface', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'CharacterFire', skeletalMeshPath: '/Engine/EngineMeshes/Mannequin' }, expected: 'success|not found' },
  { scenario: 'Effect: add_static_mesh_data_interface_ground', toolName: 'manage_effect', arguments: { action: 'add_static_mesh_data_interface', systemPath: `${TEST_FOLDER}/NS_Dust`, emitterName: 'Surface', staticMeshPath: '/Engine/BasicShapes/Plane' }, expected: 'success|not found' },
  { scenario: 'Effect: add_spline_data_interface_path', toolName: 'manage_effect', arguments: { action: 'add_spline_data_interface', systemPath: `${TEST_FOLDER}/NS_Sparks`, emitterName: 'PathSparks' }, expected: 'success|not found' },
  { scenario: 'Effect: add_collision_query_data_interface_ground', toolName: 'manage_effect', arguments: { action: 'add_collision_query_data_interface', systemPath: `${TEST_FOLDER}/NS_Rain`, emitterName: 'Drops' }, expected: 'success|not found' },
  { scenario: 'Effect: add_data_interface_render_target', toolName: 'manage_effect', arguments: { action: 'add_data_interface', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', className: 'RenderTarget2D' }, expected: 'success|not found' },
  { scenario: 'Effect: add_data_interface_vector_field', toolName: 'manage_effect', arguments: { action: 'add_data_interface', systemPath: `${TEST_FOLDER}/NS_Smoke`, emitterName: 'Cloud', className: 'VectorField' }, expected: 'success|not found' },
  
  // More Event Variations
  { scenario: 'Effect: add_event_generator_collision', toolName: 'manage_effect', arguments: { action: 'add_event_generator', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'Debris', eventName: 'OnCollision' }, expected: 'success|not found' },
  { scenario: 'Effect: add_event_generator_spawn', toolName: 'manage_effect', arguments: { action: 'add_event_generator', systemPath: `${TEST_FOLDER}/NS_Fire`, emitterName: 'Flames', eventName: 'OnSpawn' }, expected: 'success|not found' },
  { scenario: 'Effect: add_event_receiver_sparks', toolName: 'manage_effect', arguments: { action: 'add_event_receiver', systemPath: `${TEST_FOLDER}/NS_Explosion`, emitterName: 'SecondaryExplosion', eventName: 'OnDeath', spawnOnEvent: true, eventSpawnCount: 10 }, expected: 'success|not found' },
  { scenario: 'Effect: configure_event_payload_position', toolName: 'manage_effect', arguments: { action: 'configure_event_payload', systemPath: `${TEST_FOLDER}/NS_Explosion`, eventName: 'OnCollision', eventPayload: [{ name: 'Position', type: 'Vector' }, { name: 'Normal', type: 'Vector' }] }, expected: 'success|not found' },
  
  // More GPU Simulation Variations
  { scenario: 'Effect: enable_gpu_simulation_smoke', toolName: 'manage_effect', arguments: { action: 'enable_gpu_simulation', systemPath: `${TEST_FOLDER}/NS_Smoke`, emitterName: 'Cloud', gpuEnabled: true }, expected: 'success|not found' },
  { scenario: 'Effect: enable_gpu_simulation_rain', toolName: 'manage_effect', arguments: { action: 'enable_gpu_simulation', systemPath: `${TEST_FOLDER}/NS_Rain`, emitterName: 'Drops', gpuEnabled: true }, expected: 'success|not found' },
  { scenario: 'Effect: add_simulation_stage_update', toolName: 'manage_effect', arguments: { action: 'add_simulation_stage', systemPath: `${TEST_FOLDER}/NS_Fire`, stageName: 'UpdatePhysics', stageIterationSource: 'Particles' }, expected: 'success|not found' },
  { scenario: 'Effect: add_simulation_stage_output', toolName: 'manage_effect', arguments: { action: 'add_simulation_stage', systemPath: `${TEST_FOLDER}/NS_Fire`, stageName: 'OutputStage', stageIterationSource: 'DataInterface' }, expected: 'success|not found' },
  { scenario: 'Effect: configure_gpu_simulation_bounds', toolName: 'manage_effect', arguments: { action: 'configure_gpu_simulation', systemPath: `${TEST_FOLDER}/NS_Rain`, emitterName: 'Drops', fixedBoundsEnabled: true }, expected: 'success|not found' },
  { scenario: 'Effect: configure_niagara_determinism_enabled', toolName: 'manage_effect', arguments: { action: 'configure_niagara_determinism', systemPath: `${TEST_FOLDER}/NS_Explosion`, deterministicEnabled: true }, expected: 'success|not found' },
  { scenario: 'Effect: configure_niagara_lod_far', toolName: 'manage_effect', arguments: { action: 'configure_niagara_lod', systemPath: `${TEST_FOLDER}/NS_Fire`, lodDistance: 5000, lodBias: 1 }, expected: 'success|not found' },
  
  // More Runtime Control Variations
  { scenario: 'Effect: spawn_niagara_smoke', toolName: 'manage_effect', arguments: { action: 'spawn_niagara', actorName: 'Smoke_01', systemPath: `${TEST_FOLDER}/NS_Smoke`, location: { x: 100, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Effect: spawn_niagara_explosion', toolName: 'manage_effect', arguments: { action: 'spawn_niagara', actorName: 'Explosion_01', systemPath: `${TEST_FOLDER}/NS_Explosion`, location: { x: 200, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Effect: activate_smoke', toolName: 'manage_effect', arguments: { action: 'activate', actorName: 'Smoke_01' }, expected: 'success|not found' },
  { scenario: 'Effect: deactivate_smoke', toolName: 'manage_effect', arguments: { action: 'deactivate', actorName: 'Smoke_01' }, expected: 'success|not found' },
  { scenario: 'Effect: reset_explosion', toolName: 'manage_effect', arguments: { action: 'reset', actorName: 'Explosion_01' }, expected: 'success|not found' },
  { scenario: 'Effect: advance_simulation_smoke', toolName: 'manage_effect', arguments: { action: 'advance_simulation', actorName: 'Smoke_01', time: 2.0 }, expected: 'success|not found' },
  { scenario: 'Effect: set_niagara_variable_smoke', toolName: 'manage_effect', arguments: { action: 'set_niagara_variable', actorName: 'Smoke_01', parameterName: 'User.Density', value: 2.5 }, expected: 'success|not found' },
  
  // More Module Variations
  { scenario: 'Effect: create_niagara_module_vortex', toolName: 'manage_effect', arguments: { action: 'create_niagara_module', name: 'NM_VortexForce', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Effect: create_niagara_module_attraction', toolName: 'manage_effect', arguments: { action: 'create_niagara_module', name: 'NM_PointAttraction', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Effect: add_niagara_script_spawn', toolName: 'manage_effect', arguments: { action: 'add_niagara_script', modulePath: `${TEST_FOLDER}/NM_VortexForce`, stage: 'Spawn' }, expected: 'success|not found' },
  { scenario: 'Effect: add_niagara_module_vortex', toolName: 'manage_effect', arguments: { action: 'add_niagara_module', systemPath: `${TEST_FOLDER}/NS_Smoke`, emitterName: 'Cloud', modulePath: `${TEST_FOLDER}/NM_VortexForce` }, expected: 'success|not found' },
  
  // More Fluid Variations
  { scenario: 'Effect: setup_niagara_fluids_2d', toolName: 'manage_effect', arguments: { action: 'setup_niagara_fluids', systemPath: `${TEST_FOLDER}/NS_Water`, emitterName: 'Surface', fluidType: '2D' }, expected: 'success|not found' },
  { scenario: 'Effect: create_fluid_simulation_3d', toolName: 'manage_effect', arguments: { action: 'create_fluid_simulation', name: 'NS_Gas', path: TEST_FOLDER, fluidType: '3D' }, expected: 'success|not found' },
  
  // More Debug Shape Variations
  { scenario: 'Effect: draw_debug_sphere_large', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'sphere', location: { x: 1000, y: 0, z: 100 }, size: 200, color: { r: 0, g: 1, b: 0 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_box_rotated', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'box', location: { x: 1200, y: 0, z: 100 }, size: 100, color: { r: 0, g: 0, b: 1 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_arrow_long', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'arrow', location: { x: 0, y: 500, z: 0 }, size: 500, color: { r: 1, g: 0, b: 0 } }, expected: 'success' },
  { scenario: 'Effect: draw_debug_cone_wide', toolName: 'manage_effect', arguments: { action: 'debug_shape', shape: 'cone', location: { x: 1400, y: 0, z: 100 }, size: 100, color: { r: 1, g: 0.5, b: 0.5 } }, expected: 'success' },
];

// ============================================================================
// MANAGE_CHARACTER (78 actions) - FULL COVERAGE
// ============================================================================
const manageCharacterTests = [
  // === Character Blueprint Creation ===
  { scenario: 'Char: create_character_blueprint', toolName: 'manage_character', arguments: { action: 'create_character_blueprint', name: 'BP_PlayerChar', path: TEST_FOLDER, parentClass: 'Character' }, expected: 'success|already exists' },
  { scenario: 'Char: get_character_info', toolName: 'manage_character', arguments: { action: 'get_character_info', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  
  // === Component Configuration ===
  { scenario: 'Char: configure_capsule_component', toolName: 'manage_character', arguments: { action: 'configure_capsule_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, capsuleRadius: 42, capsuleHalfHeight: 96 }, expected: 'success|not found' },
  { scenario: 'Char: configure_mesh_component', toolName: 'manage_character', arguments: { action: 'configure_mesh_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube', animBlueprintPath: `${TEST_FOLDER}/ABP_Test` }, expected: 'success|not found' },
  { scenario: 'Char: configure_camera_component', toolName: 'manage_character', arguments: { action: 'configure_camera_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, springArmLength: 300, cameraUsePawnControlRotation: true }, expected: 'success|not found' },
  
  // === Movement Configuration ===
  { scenario: 'Char: configure_movement_speeds', toolName: 'manage_character', arguments: { action: 'configure_movement_speeds', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, walkSpeed: 400, runSpeed: 600, sprintSpeed: 1000, crouchSpeed: 200 }, expected: 'success|not found' },
  { scenario: 'Char: configure_jump', toolName: 'manage_character', arguments: { action: 'configure_jump', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, jumpHeight: 600, airControl: 0.3, doubleJumpEnabled: true, maxJumpCount: 2 }, expected: 'success|not found' },
  { scenario: 'Char: configure_rotation', toolName: 'manage_character', arguments: { action: 'configure_rotation', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, orientToMovement: true, rotationRate: 540 }, expected: 'success|not found' },
  { scenario: 'Char: add_custom_movement_mode', toolName: 'manage_character', arguments: { action: 'add_custom_movement_mode', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, modeName: 'Grappling', modeId: 1 }, expected: 'success|not found' },
  { scenario: 'Char: configure_nav_movement', toolName: 'manage_character', arguments: { action: 'configure_nav_movement', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, navAgentRadius: 42, navAgentHeight: 192, avoidanceEnabled: true }, expected: 'success|not found' },
  { scenario: 'Char: set_movement_mode', toolName: 'manage_character', arguments: { action: 'set_movement_mode', actorName: 'Player_01', movementMode: 'Flying' }, expected: 'success|not found' },
  
  // === Advanced Movement ===
  { scenario: 'Char: setup_mantling', toolName: 'manage_character', arguments: { action: 'setup_mantling', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, mantleHeight: 200, mantleReachDistance: 100 }, expected: 'success|not found' },
  { scenario: 'Char: setup_vaulting', toolName: 'manage_character', arguments: { action: 'setup_vaulting', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, vaultHeight: 100, vaultDepth: 50 }, expected: 'success|not found' },
  { scenario: 'Char: setup_climbing', toolName: 'manage_character', arguments: { action: 'setup_climbing', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, climbSpeed: 200, climbableTag: 'Climbable' }, expected: 'success|not found' },
  { scenario: 'Char: setup_sliding', toolName: 'manage_character', arguments: { action: 'setup_sliding', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, slideSpeed: 800, slideDuration: 1.0, slideCooldown: 0.5 }, expected: 'success|not found' },
  { scenario: 'Char: setup_wall_running', toolName: 'manage_character', arguments: { action: 'setup_wall_running', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, wallRunSpeed: 600, wallRunDuration: 2.0, wallRunGravityScale: 0.3 }, expected: 'success|not found' },
  { scenario: 'Char: setup_grappling', toolName: 'manage_character', arguments: { action: 'setup_grappling', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, grappleRange: 2000, grappleSpeed: 1500, grappleTargetTag: 'GrapplePoint' }, expected: 'success|not found' },
  { scenario: 'Char: configure_mantle_vault', toolName: 'manage_character', arguments: { action: 'configure_mantle_vault', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, mantleEnabled: true, vaultEnabled: true, mantleMinHeight: 50, mantleMaxHeight: 200 }, expected: 'success|not found' },
  
  // === Locomotion States (Wave 3) ===
  { scenario: 'Char: configure_locomotion_state', toolName: 'manage_character', arguments: { action: 'configure_locomotion_state', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, locomotionState: 'Sprinting', walkSpeed: 1000 }, expected: 'success|not found' },
  { scenario: 'Char: get_character_stats_snapshot', toolName: 'manage_character', arguments: { action: 'get_character_stats_snapshot', actorName: 'Player_01', includeStats: ['Health', 'Stamina', 'Speed'] }, expected: 'success|not found' },
  
  // === Footstep System ===
  { scenario: 'Char: setup_footstep_system', toolName: 'manage_character', arguments: { action: 'setup_footstep_system', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, footstepEnabled: true, footstepSocketLeft: 'foot_l', footstepSocketRight: 'foot_r' }, expected: 'success|not found' },
  { scenario: 'Char: map_surface_to_sound', toolName: 'manage_character', arguments: { action: 'map_surface_to_sound', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, surfaceType: 'Metal', footstepSoundPath: '/Engine/Sounds/Footstep_Metal' }, expected: 'success|not found' },
  { scenario: 'Char: configure_footstep_fx', toolName: 'manage_character', arguments: { action: 'configure_footstep_fx', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, surfaceType: 'Dirt', footstepParticlePath: '/Engine/Particles/P_Dust' }, expected: 'success|not found' },
  { scenario: 'Char: configure_footstep_system', toolName: 'manage_character', arguments: { action: 'configure_footstep_system', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, footstepVolumeScale: 1.0, footstepInterval: 0.3 }, expected: 'success|not found' },
  
  // === Interaction System ===
  { scenario: 'Char: create_interaction_component', toolName: 'manage_character', arguments: { action: 'create_interaction_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: configure_interaction_trace', toolName: 'manage_character', arguments: { action: 'configure_interaction_trace', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, traceDistance: 300, traceRadius: 30 }, expected: 'success|not found' },
  { scenario: 'Char: configure_interaction_widget', toolName: 'manage_character', arguments: { action: 'configure_interaction_widget', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, widgetPath: `${TEST_FOLDER}/WBP_Interaction` }, expected: 'success|not found' },
  { scenario: 'Char: add_interaction_events', toolName: 'manage_character', arguments: { action: 'add_interaction_events', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: create_interactable_interface', toolName: 'manage_character', arguments: { action: 'create_interactable_interface', name: 'IInteractable', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: query_interaction_targets', toolName: 'manage_character', arguments: { action: 'query_interaction_targets', actorName: 'Player_01', interactionRange: 300 }, expected: 'success|not found' },
  
  // === Interactive Objects ===
  { scenario: 'Char: create_door_actor', toolName: 'manage_character', arguments: { action: 'create_door_actor', name: 'BP_Door', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_door_properties', toolName: 'manage_character', arguments: { action: 'configure_door_properties', blueprintPath: `${TEST_FOLDER}/BP_Door`, openAngle: 90, openSpeed: 2.0 }, expected: 'success|not found' },
  { scenario: 'Char: create_switch_actor', toolName: 'manage_character', arguments: { action: 'create_switch_actor', name: 'BP_Switch', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_switch_properties', toolName: 'manage_character', arguments: { action: 'configure_switch_properties', blueprintPath: `${TEST_FOLDER}/BP_Switch`, switchType: 'Toggle' }, expected: 'success|not found' },
  { scenario: 'Char: create_chest_actor', toolName: 'manage_character', arguments: { action: 'create_chest_actor', name: 'BP_Chest', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_chest_properties', toolName: 'manage_character', arguments: { action: 'configure_chest_properties', blueprintPath: `${TEST_FOLDER}/BP_Chest`, slotCount: 10 }, expected: 'success|not found' },
  { scenario: 'Char: create_lever_actor', toolName: 'manage_character', arguments: { action: 'create_lever_actor', name: 'BP_Lever', path: TEST_FOLDER }, expected: 'success|not found' },
  
  // === Destructibles ===
  { scenario: 'Char: setup_destructible_mesh', toolName: 'manage_character', arguments: { action: 'setup_destructible_mesh', blueprintPath: `${TEST_FOLDER}/BP_Barrel`, meshPath: '/Engine/BasicShapes/Cylinder' }, expected: 'success|not found' },
  { scenario: 'Char: configure_destruction_levels', toolName: 'manage_character', arguments: { action: 'configure_destruction_levels', blueprintPath: `${TEST_FOLDER}/BP_Barrel`, levels: [{ threshold: 50, mesh: 'Damaged' }, { threshold: 0, mesh: 'Destroyed' }] }, expected: 'success|not found' },
  { scenario: 'Char: configure_destruction_effects', toolName: 'manage_character', arguments: { action: 'configure_destruction_effects', blueprintPath: `${TEST_FOLDER}/BP_Barrel`, particlePath: '/Engine/Particles/P_Explosion', soundPath: '/Engine/Sounds/S_Explosion' }, expected: 'success|not found' },
  { scenario: 'Char: configure_destruction_damage', toolName: 'manage_character', arguments: { action: 'configure_destruction_damage', blueprintPath: `${TEST_FOLDER}/BP_Barrel`, maxHealth: 100 }, expected: 'success|not found' },
  { scenario: 'Char: add_destruction_component', toolName: 'manage_character', arguments: { action: 'add_destruction_component', blueprintPath: `${TEST_FOLDER}/BP_Barrel` }, expected: 'success|not found' },
  
  // === Triggers ===
  { scenario: 'Char: create_trigger_actor', toolName: 'manage_character', arguments: { action: 'create_trigger_actor', name: 'BP_TriggerZone', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_trigger_events', toolName: 'manage_character', arguments: { action: 'configure_trigger_events', blueprintPath: `${TEST_FOLDER}/BP_TriggerZone` }, expected: 'success|not found' },
  { scenario: 'Char: configure_trigger_filter', toolName: 'manage_character', arguments: { action: 'configure_trigger_filter', blueprintPath: `${TEST_FOLDER}/BP_TriggerZone`, filterClass: 'Character', filterTag: 'Player' }, expected: 'success|not found' },
  { scenario: 'Char: configure_trigger_response', toolName: 'manage_character', arguments: { action: 'configure_trigger_response', blueprintPath: `${TEST_FOLDER}/BP_TriggerZone`, responseType: 'PlaySound' }, expected: 'success|not found' },
  { scenario: 'Char: get_interaction_info', toolName: 'manage_character', arguments: { action: 'get_interaction_info', blueprintPath: `${TEST_FOLDER}/BP_Door` }, expected: 'success|not found' },
  
  // === Inventory System (inv_ prefix) ===
  { scenario: 'Char: inv_create_item_data_asset', toolName: 'manage_character', arguments: { action: 'inv_create_item_data_asset', name: 'DA_Sword', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_set_item_properties', toolName: 'manage_character', arguments: { action: 'inv_set_item_properties', assetPath: `${TEST_FOLDER}/DA_Sword`, displayName: 'Iron Sword', stackable: false, maxStackSize: 1 }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_item_category', toolName: 'manage_character', arguments: { action: 'inv_create_item_category', name: 'Weapons', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_assign_item_category', toolName: 'manage_character', arguments: { action: 'inv_assign_item_category', itemPath: `${TEST_FOLDER}/DA_Sword`, categoryPath: `${TEST_FOLDER}/Weapons` }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_inventory_component', toolName: 'manage_character', arguments: { action: 'inv_create_inventory_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_inventory_slots', toolName: 'manage_character', arguments: { action: 'inv_configure_inventory_slots', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, slotCount: 20 }, expected: 'success|not found' },
  { scenario: 'Char: inv_add_inventory_functions', toolName: 'manage_character', arguments: { action: 'inv_add_inventory_functions', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_inventory_events', toolName: 'manage_character', arguments: { action: 'inv_configure_inventory_events', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: inv_set_inventory_replication', toolName: 'manage_character', arguments: { action: 'inv_set_inventory_replication', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, replicated: true }, expected: 'success|not found' },
  { scenario: 'Char: configure_inventory_slot', toolName: 'manage_character', arguments: { action: 'configure_inventory_slot', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, slotIndex: 0, slotType: 'Primary', slotCapacity: 1 }, expected: 'success|not found' },
  { scenario: 'Char: batch_add_inventory_items', toolName: 'manage_character', arguments: { action: 'batch_add_inventory_items', actorName: 'Player_01', itemDataAssets: [`${TEST_FOLDER}/DA_Sword`, `${TEST_FOLDER}/DA_Potion`], autoStack: true }, expected: 'success|not found' },
  
  // === Pickup System ===
  { scenario: 'Char: inv_create_pickup_actor', toolName: 'manage_character', arguments: { action: 'inv_create_pickup_actor', name: 'BP_Pickup', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_pickup_interaction', toolName: 'manage_character', arguments: { action: 'inv_configure_pickup_interaction', blueprintPath: `${TEST_FOLDER}/BP_Pickup`, autoPickup: false }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_pickup_respawn', toolName: 'manage_character', arguments: { action: 'inv_configure_pickup_respawn', blueprintPath: `${TEST_FOLDER}/BP_Pickup`, respawnEnabled: true, respawnTime: 30 }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_pickup_effects', toolName: 'manage_character', arguments: { action: 'inv_configure_pickup_effects', blueprintPath: `${TEST_FOLDER}/BP_Pickup`, pickupParticlePath: '/Engine/Particles/P_Pickup' }, expected: 'success|not found' },
  
  // === Equipment System ===
  { scenario: 'Char: inv_create_equipment_component', toolName: 'manage_character', arguments: { action: 'inv_create_equipment_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: inv_define_equipment_slots', toolName: 'manage_character', arguments: { action: 'inv_define_equipment_slots', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, slots: ['MainHand', 'OffHand', 'Head', 'Chest'] }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_equipment_effects', toolName: 'manage_character', arguments: { action: 'inv_configure_equipment_effects', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: inv_add_equipment_functions', toolName: 'manage_character', arguments: { action: 'inv_add_equipment_functions', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_equipment_visuals', toolName: 'manage_character', arguments: { action: 'inv_configure_equipment_visuals', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Char: configure_equipment_socket', toolName: 'manage_character', arguments: { action: 'configure_equipment_socket', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, socketName: 'WeaponSocket', socketBone: 'hand_r', socketOffset: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
  
  // === Loot & Crafting ===
  { scenario: 'Char: inv_create_loot_table', toolName: 'manage_character', arguments: { action: 'inv_create_loot_table', name: 'LT_Enemy', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_add_loot_entry', toolName: 'manage_character', arguments: { action: 'inv_add_loot_entry', lootTablePath: `${TEST_FOLDER}/LT_Enemy`, itemPath: `${TEST_FOLDER}/DA_Sword`, dropChance: 0.1 }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_loot_drop', toolName: 'manage_character', arguments: { action: 'inv_configure_loot_drop', lootTablePath: `${TEST_FOLDER}/LT_Enemy`, minItems: 1, maxItems: 3 }, expected: 'success|not found' },
  { scenario: 'Char: inv_set_loot_quality_tiers', toolName: 'manage_character', arguments: { action: 'inv_set_loot_quality_tiers', lootTablePath: `${TEST_FOLDER}/LT_Enemy`, tiers: ['Common', 'Uncommon', 'Rare', 'Epic', 'Legendary'] }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_crafting_recipe', toolName: 'manage_character', arguments: { action: 'inv_create_crafting_recipe', name: 'CR_SteelSword', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_recipe_requirements', toolName: 'manage_character', arguments: { action: 'inv_configure_recipe_requirements', recipePath: `${TEST_FOLDER}/CR_SteelSword`, requirements: [{ item: `${TEST_FOLDER}/DA_Iron`, count: 3 }] }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_crafting_station', toolName: 'manage_character', arguments: { action: 'inv_create_crafting_station', name: 'BP_Anvil', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_add_crafting_component', toolName: 'manage_character', arguments: { action: 'inv_add_crafting_component', blueprintPath: `${TEST_FOLDER}/BP_Anvil` }, expected: 'success|not found' },
  { scenario: 'Char: inv_get_inventory_info', toolName: 'manage_character', arguments: { action: 'inv_get_inventory_info', actorName: 'Player_01' }, expected: 'success|not found' },
  
  // === Status Effects ===
  { scenario: 'Char: apply_status_effect', toolName: 'manage_character', arguments: { action: 'apply_status_effect', actorName: 'Player_01', statusEffectId: 'Burning', effectDuration: 5.0, effectMagnitude: 10 }, expected: 'success|not found' },
  
  // === EXTENDED MANAGE_CHARACTER (Additional ~80 tests for comprehensive coverage) ===
  // More Character Blueprint Variations
  { scenario: 'Char: create_character_blueprint_enemy', toolName: 'manage_character', arguments: { action: 'create_character_blueprint', name: 'BP_Enemy', path: TEST_FOLDER, parentClass: 'Character' }, expected: 'success|already exists' },
  { scenario: 'Char: create_character_blueprint_npc', toolName: 'manage_character', arguments: { action: 'create_character_blueprint', name: 'BP_NPC', path: TEST_FOLDER, parentClass: 'Character' }, expected: 'success|already exists' },
  { scenario: 'Char: create_character_blueprint_boss', toolName: 'manage_character', arguments: { action: 'create_character_blueprint', name: 'BP_Boss', path: TEST_FOLDER, parentClass: 'Character' }, expected: 'success|already exists' },
  { scenario: 'Char: get_character_info_enemy', toolName: 'manage_character', arguments: { action: 'get_character_info', blueprintPath: `${TEST_FOLDER}/BP_Enemy` }, expected: 'success|not found' },
  
  // More Component Configurations
  { scenario: 'Char: configure_capsule_component_large', toolName: 'manage_character', arguments: { action: 'configure_capsule_component', blueprintPath: `${TEST_FOLDER}/BP_Boss`, capsuleRadius: 80, capsuleHalfHeight: 150 }, expected: 'success|not found' },
  { scenario: 'Char: configure_capsule_component_small', toolName: 'manage_character', arguments: { action: 'configure_capsule_component', blueprintPath: `${TEST_FOLDER}/BP_NPC`, capsuleRadius: 35, capsuleHalfHeight: 88 }, expected: 'success|not found' },
  { scenario: 'Char: configure_camera_component_fps', toolName: 'manage_character', arguments: { action: 'configure_camera_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, springArmLength: 0, cameraUsePawnControlRotation: true }, expected: 'success|not found' },
  { scenario: 'Char: configure_camera_component_tps', toolName: 'manage_character', arguments: { action: 'configure_camera_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, springArmLength: 500, cameraUsePawnControlRotation: false }, expected: 'success|not found' },
  
  // More Movement Configurations
  { scenario: 'Char: configure_movement_speeds_slow', toolName: 'manage_character', arguments: { action: 'configure_movement_speeds', blueprintPath: `${TEST_FOLDER}/BP_NPC`, walkSpeed: 200, runSpeed: 400, sprintSpeed: 600, crouchSpeed: 100 }, expected: 'success|not found' },
  { scenario: 'Char: configure_movement_speeds_fast', toolName: 'manage_character', arguments: { action: 'configure_movement_speeds', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, walkSpeed: 500, runSpeed: 800, sprintSpeed: 1200, crouchSpeed: 250 }, expected: 'success|not found' },
  { scenario: 'Char: configure_jump_high', toolName: 'manage_character', arguments: { action: 'configure_jump', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, jumpHeight: 1000, airControl: 0.5, doubleJumpEnabled: true, maxJumpCount: 3 }, expected: 'success|not found' },
  { scenario: 'Char: configure_rotation_fast', toolName: 'manage_character', arguments: { action: 'configure_rotation', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, orientToMovement: true, rotationRate: 720 }, expected: 'success|not found' },
  { scenario: 'Char: add_custom_movement_mode_swimming', toolName: 'manage_character', arguments: { action: 'add_custom_movement_mode', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, modeName: 'Swimming', modeId: 2 }, expected: 'success|not found' },
  { scenario: 'Char: add_custom_movement_mode_ladder', toolName: 'manage_character', arguments: { action: 'add_custom_movement_mode', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, modeName: 'Ladder', modeId: 3 }, expected: 'success|not found' },
  { scenario: 'Char: set_movement_mode_swimming', toolName: 'manage_character', arguments: { action: 'set_movement_mode', actorName: 'Player_01', movementMode: 'Swimming' }, expected: 'success|not found' },
  { scenario: 'Char: set_movement_mode_walking', toolName: 'manage_character', arguments: { action: 'set_movement_mode', actorName: 'Player_01', movementMode: 'Walking' }, expected: 'success|not found' },
  
  // More Advanced Movement
  { scenario: 'Char: setup_mantling_high', toolName: 'manage_character', arguments: { action: 'setup_mantling', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, mantleHeight: 300, mantleReachDistance: 150 }, expected: 'success|not found' },
  { scenario: 'Char: setup_climbing_fast', toolName: 'manage_character', arguments: { action: 'setup_climbing', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, climbSpeed: 400, climbableTag: 'Ladder' }, expected: 'success|not found' },
  { scenario: 'Char: setup_sliding_long', toolName: 'manage_character', arguments: { action: 'setup_sliding', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, slideSpeed: 1000, slideDuration: 2.0, slideCooldown: 1.0 }, expected: 'success|not found' },
  { scenario: 'Char: setup_wall_running_long', toolName: 'manage_character', arguments: { action: 'setup_wall_running', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, wallRunSpeed: 800, wallRunDuration: 3.0, wallRunGravityScale: 0.2 }, expected: 'success|not found' },
  
  // More Locomotion States
  { scenario: 'Char: configure_locomotion_state_crouching', toolName: 'manage_character', arguments: { action: 'configure_locomotion_state', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, locomotionState: 'Crouching', walkSpeed: 200 }, expected: 'success|not found' },
  { scenario: 'Char: configure_locomotion_state_prone', toolName: 'manage_character', arguments: { action: 'configure_locomotion_state', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, locomotionState: 'Prone', walkSpeed: 50 }, expected: 'success|not found' },
  { scenario: 'Char: get_character_stats_snapshot_enemy', toolName: 'manage_character', arguments: { action: 'get_character_stats_snapshot', actorName: 'Enemy_01', includeStats: ['Health', 'Armor', 'DamageMultiplier'] }, expected: 'success|not found' },
  
  // More Footstep System
  { scenario: 'Char: map_surface_to_sound_concrete', toolName: 'manage_character', arguments: { action: 'map_surface_to_sound', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, surfaceType: 'Concrete', footstepSoundPath: '/Engine/Sounds/Footstep_Concrete' }, expected: 'success|not found' },
  { scenario: 'Char: map_surface_to_sound_grass', toolName: 'manage_character', arguments: { action: 'map_surface_to_sound', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, surfaceType: 'Grass', footstepSoundPath: '/Engine/Sounds/Footstep_Grass' }, expected: 'success|not found' },
  { scenario: 'Char: map_surface_to_sound_wood', toolName: 'manage_character', arguments: { action: 'map_surface_to_sound', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, surfaceType: 'Wood', footstepSoundPath: '/Engine/Sounds/Footstep_Wood' }, expected: 'success|not found' },
  { scenario: 'Char: map_surface_to_sound_water', toolName: 'manage_character', arguments: { action: 'map_surface_to_sound', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, surfaceType: 'Water', footstepSoundPath: '/Engine/Sounds/Footstep_Water' }, expected: 'success|not found' },
  { scenario: 'Char: configure_footstep_fx_water', toolName: 'manage_character', arguments: { action: 'configure_footstep_fx', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, surfaceType: 'Water', footstepParticlePath: '/Engine/Particles/P_Splash' }, expected: 'success|not found' },
  
  // More Interaction System
  { scenario: 'Char: configure_interaction_trace_long', toolName: 'manage_character', arguments: { action: 'configure_interaction_trace', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, traceDistance: 500, traceRadius: 50 }, expected: 'success|not found' },
  { scenario: 'Char: query_interaction_targets_far', toolName: 'manage_character', arguments: { action: 'query_interaction_targets', actorName: 'Player_01', interactionRange: 500 }, expected: 'success|not found' },
  
  // More Interactive Objects
  { scenario: 'Char: create_door_actor_sliding', toolName: 'manage_character', arguments: { action: 'create_door_actor', name: 'BP_SlidingDoor', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_door_properties_sliding', toolName: 'manage_character', arguments: { action: 'configure_door_properties', blueprintPath: `${TEST_FOLDER}/BP_SlidingDoor`, openAngle: 0, openSpeed: 3.0 }, expected: 'success|not found' },
  { scenario: 'Char: create_switch_actor_pressure', toolName: 'manage_character', arguments: { action: 'create_switch_actor', name: 'BP_PressurePlate', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_switch_properties_pressure', toolName: 'manage_character', arguments: { action: 'configure_switch_properties', blueprintPath: `${TEST_FOLDER}/BP_PressurePlate`, switchType: 'Momentary' }, expected: 'success|not found' },
  { scenario: 'Char: create_chest_actor_large', toolName: 'manage_character', arguments: { action: 'create_chest_actor', name: 'BP_LargeChest', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_chest_properties_large', toolName: 'manage_character', arguments: { action: 'configure_chest_properties', blueprintPath: `${TEST_FOLDER}/BP_LargeChest`, slotCount: 30 }, expected: 'success|not found' },
  
  // More Destructibles
  { scenario: 'Char: setup_destructible_mesh_crate', toolName: 'manage_character', arguments: { action: 'setup_destructible_mesh', blueprintPath: `${TEST_FOLDER}/BP_Crate`, meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Char: configure_destruction_levels_crate', toolName: 'manage_character', arguments: { action: 'configure_destruction_levels', blueprintPath: `${TEST_FOLDER}/BP_Crate`, levels: [{ threshold: 50, mesh: 'Damaged' }, { threshold: 0, mesh: 'Destroyed' }] }, expected: 'success|not found' },
  { scenario: 'Char: add_destruction_component_crate', toolName: 'manage_character', arguments: { action: 'add_destruction_component', blueprintPath: `${TEST_FOLDER}/BP_Crate` }, expected: 'success|not found' },
  
  // More Triggers
  { scenario: 'Char: create_trigger_actor_damage', toolName: 'manage_character', arguments: { action: 'create_trigger_actor', name: 'BP_DamageZone', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_trigger_response_damage', toolName: 'manage_character', arguments: { action: 'configure_trigger_response', blueprintPath: `${TEST_FOLDER}/BP_DamageZone`, responseType: 'ApplyDamage' }, expected: 'success|not found' },
  { scenario: 'Char: create_trigger_actor_heal', toolName: 'manage_character', arguments: { action: 'create_trigger_actor', name: 'BP_HealZone', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: configure_trigger_response_heal', toolName: 'manage_character', arguments: { action: 'configure_trigger_response', blueprintPath: `${TEST_FOLDER}/BP_HealZone`, responseType: 'ApplyHeal' }, expected: 'success|not found' },
  
  // More Inventory System
  { scenario: 'Char: inv_create_item_data_asset_potion', toolName: 'manage_character', arguments: { action: 'inv_create_item_data_asset', name: 'DA_HealthPotion', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_item_data_asset_armor', toolName: 'manage_character', arguments: { action: 'inv_create_item_data_asset', name: 'DA_Armor', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_item_data_asset_key', toolName: 'manage_character', arguments: { action: 'inv_create_item_data_asset', name: 'DA_Key', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_set_item_properties_potion', toolName: 'manage_character', arguments: { action: 'inv_set_item_properties', assetPath: `${TEST_FOLDER}/DA_HealthPotion`, displayName: 'Health Potion', stackable: true, maxStackSize: 10 }, expected: 'success|not found' },
  { scenario: 'Char: inv_set_item_properties_armor', toolName: 'manage_character', arguments: { action: 'inv_set_item_properties', assetPath: `${TEST_FOLDER}/DA_Armor`, displayName: 'Steel Armor', stackable: false, maxStackSize: 1 }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_item_category_consumables', toolName: 'manage_character', arguments: { action: 'inv_create_item_category', name: 'Consumables', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_item_category_armor', toolName: 'manage_character', arguments: { action: 'inv_create_item_category', name: 'Armor', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_item_category_keys', toolName: 'manage_character', arguments: { action: 'inv_create_item_category', name: 'QuestItems', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_assign_item_category_potion', toolName: 'manage_character', arguments: { action: 'inv_assign_item_category', itemPath: `${TEST_FOLDER}/DA_HealthPotion`, categoryPath: `${TEST_FOLDER}/Consumables` }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_inventory_slots_large', toolName: 'manage_character', arguments: { action: 'inv_configure_inventory_slots', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, slotCount: 50 }, expected: 'success|not found' },
  { scenario: 'Char: configure_inventory_slot_quickslot', toolName: 'manage_character', arguments: { action: 'configure_inventory_slot', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, slotIndex: 1, slotType: 'Quickslot', slotCapacity: 1 }, expected: 'success|not found' },
  { scenario: 'Char: batch_add_inventory_items_potions', toolName: 'manage_character', arguments: { action: 'batch_add_inventory_items', actorName: 'Player_01', itemDataAssets: [`${TEST_FOLDER}/DA_HealthPotion`, `${TEST_FOLDER}/DA_HealthPotion`, `${TEST_FOLDER}/DA_HealthPotion`], autoStack: true }, expected: 'success|not found' },
  
  // More Pickup System
  { scenario: 'Char: inv_create_pickup_actor_weapon', toolName: 'manage_character', arguments: { action: 'inv_create_pickup_actor', name: 'BP_WeaponPickup', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_pickup_interaction_auto', toolName: 'manage_character', arguments: { action: 'inv_configure_pickup_interaction', blueprintPath: `${TEST_FOLDER}/BP_WeaponPickup`, autoPickup: true }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_pickup_respawn_fast', toolName: 'manage_character', arguments: { action: 'inv_configure_pickup_respawn', blueprintPath: `${TEST_FOLDER}/BP_Pickup`, respawnEnabled: true, respawnTime: 10 }, expected: 'success|not found' },
  
  // More Equipment System
  { scenario: 'Char: inv_define_equipment_slots_full', toolName: 'manage_character', arguments: { action: 'inv_define_equipment_slots', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, slots: ['MainHand', 'OffHand', 'Head', 'Chest', 'Legs', 'Feet', 'Gloves', 'Amulet', 'Ring1', 'Ring2'] }, expected: 'success|not found' },
  { scenario: 'Char: configure_equipment_socket_offhand', toolName: 'manage_character', arguments: { action: 'configure_equipment_socket', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, socketName: 'ShieldSocket', socketBone: 'hand_l', socketOffset: { x: 0, y: 5, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Char: configure_equipment_socket_back', toolName: 'manage_character', arguments: { action: 'configure_equipment_socket', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, socketName: 'BackSocket', socketBone: 'spine_03', socketOffset: { x: -20, y: 0, z: 0 } }, expected: 'success|not found' },
  
  // More Loot & Crafting
  { scenario: 'Char: inv_create_loot_table_boss', toolName: 'manage_character', arguments: { action: 'inv_create_loot_table', name: 'LT_Boss', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_add_loot_entry_legendary', toolName: 'manage_character', arguments: { action: 'inv_add_loot_entry', lootTablePath: `${TEST_FOLDER}/LT_Boss`, itemPath: `${TEST_FOLDER}/DA_LegendarySword`, dropChance: 0.05 }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_loot_drop_boss', toolName: 'manage_character', arguments: { action: 'inv_configure_loot_drop', lootTablePath: `${TEST_FOLDER}/LT_Boss`, minItems: 3, maxItems: 5 }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_crafting_recipe_armor', toolName: 'manage_character', arguments: { action: 'inv_create_crafting_recipe', name: 'CR_SteelArmor', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_configure_recipe_requirements_armor', toolName: 'manage_character', arguments: { action: 'inv_configure_recipe_requirements', recipePath: `${TEST_FOLDER}/CR_SteelArmor`, requirements: [{ item: `${TEST_FOLDER}/DA_Iron`, count: 10 }, { item: `${TEST_FOLDER}/DA_Leather`, count: 5 }] }, expected: 'success|not found' },
  { scenario: 'Char: inv_create_crafting_station_forge', toolName: 'manage_character', arguments: { action: 'inv_create_crafting_station', name: 'BP_Forge', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Char: inv_add_crafting_component_forge', toolName: 'manage_character', arguments: { action: 'inv_add_crafting_component', blueprintPath: `${TEST_FOLDER}/BP_Forge` }, expected: 'success|not found' },
  { scenario: 'Char: inv_get_inventory_info_enemy', toolName: 'manage_character', arguments: { action: 'inv_get_inventory_info', actorName: 'Enemy_01' }, expected: 'success|not found' },
  
  // More Status Effects
  { scenario: 'Char: apply_status_effect_poison', toolName: 'manage_character', arguments: { action: 'apply_status_effect', actorName: 'Player_01', statusEffectId: 'Poisoned', effectDuration: 10.0, effectMagnitude: 5 }, expected: 'success|not found' },
  { scenario: 'Char: apply_status_effect_stunned', toolName: 'manage_character', arguments: { action: 'apply_status_effect', actorName: 'Enemy_01', statusEffectId: 'Stunned', effectDuration: 2.0, effectMagnitude: 0 }, expected: 'success|not found' },
  { scenario: 'Char: apply_status_effect_buffed', toolName: 'manage_character', arguments: { action: 'apply_status_effect', actorName: 'Player_01', statusEffectId: 'Strengthened', effectDuration: 30.0, effectMagnitude: 25 }, expected: 'success|not found' },
  { scenario: 'Char: apply_status_effect_slowed', toolName: 'manage_character', arguments: { action: 'apply_status_effect', actorName: 'Enemy_01', statusEffectId: 'Slowed', effectDuration: 5.0, effectMagnitude: 50 }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_COMBAT (67 actions) - FULL COVERAGE
// ============================================================================
const manageCombatTests = [
  // === Weapon Creation ===
  { scenario: 'Combat: create_weapon_blueprint', toolName: 'manage_combat', arguments: { action: 'create_weapon_blueprint', name: 'BP_Rifle', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: configure_weapon_mesh', toolName: 'manage_combat', arguments: { action: 'configure_weapon_mesh', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, weaponMeshPath: '/Engine/BasicShapes/Cylinder' }, expected: 'success|not found' },
  { scenario: 'Combat: configure_weapon_sockets', toolName: 'manage_combat', arguments: { action: 'configure_weapon_sockets', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, muzzleSocketName: 'Muzzle', ejectionSocketName: 'Ejection' }, expected: 'success|not found' },
  { scenario: 'Combat: set_weapon_stats', toolName: 'manage_combat', arguments: { action: 'set_weapon_stats', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, baseDamage: 25, fireRate: 600, range: 5000 }, expected: 'success|not found' },
  
  // === Hitscan & Projectile Configuration ===
  { scenario: 'Combat: configure_hitscan', toolName: 'manage_combat', arguments: { action: 'configure_hitscan', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, hitscanEnabled: true, traceChannel: 'Weapon' }, expected: 'success|not found' },
  { scenario: 'Combat: configure_projectile', toolName: 'manage_combat', arguments: { action: 'configure_projectile', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, projectileClass: `${TEST_FOLDER}/BP_Bullet` }, expected: 'success|not found' },
  { scenario: 'Combat: configure_spread_pattern', toolName: 'manage_combat', arguments: { action: 'configure_spread_pattern', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, spreadPattern: 'Random', spread: 2.0, spreadIncrease: 0.5, spreadRecovery: 2.0 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_recoil_pattern', toolName: 'manage_combat', arguments: { action: 'configure_recoil_pattern', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, recoilPitch: 0.5, recoilYaw: 0.2, recoilRecovery: 5.0 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_aim_down_sights', toolName: 'manage_combat', arguments: { action: 'configure_aim_down_sights', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, adsEnabled: true, adsFov: 60, adsSpeed: 0.2, adsSpreadMultiplier: 0.3 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_weapon_trace', toolName: 'manage_combat', arguments: { action: 'configure_weapon_trace', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, traceChannel: 'Weapon', traceRadius: 1.0 }, expected: 'success|not found' },
  
  // === Projectile ===
  { scenario: 'Combat: create_projectile_blueprint', toolName: 'manage_combat', arguments: { action: 'create_projectile_blueprint', name: 'BP_Bullet', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: configure_projectile_movement', toolName: 'manage_combat', arguments: { action: 'configure_projectile_movement', blueprintPath: `${TEST_FOLDER}/BP_Bullet`, projectileSpeed: 10000, projectileGravityScale: 0.1, projectileLifespan: 3.0 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_projectile_collision', toolName: 'manage_combat', arguments: { action: 'configure_projectile_collision', blueprintPath: `${TEST_FOLDER}/BP_Bullet`, collisionRadius: 5, bounceEnabled: false }, expected: 'success|not found' },
  { scenario: 'Combat: configure_projectile_homing', toolName: 'manage_combat', arguments: { action: 'configure_projectile_homing', blueprintPath: `${TEST_FOLDER}/BP_Missile`, homingEnabled: true, homingAcceleration: 5000, homingTargetTag: 'Enemy' }, expected: 'success|not found' },
  { scenario: 'Combat: create_projectile_pool', toolName: 'manage_combat', arguments: { action: 'create_projectile_pool', blueprintPath: `${TEST_FOLDER}/BP_Bullet`, poolSize: 50 }, expected: 'success|not found' },
  
  // === Damage System ===
  { scenario: 'Combat: create_damage_type', toolName: 'manage_combat', arguments: { action: 'create_damage_type', name: 'DT_Bullet', path: TEST_FOLDER, damageCategory: 'Physical' }, expected: 'success|already exists' },
  { scenario: 'Combat: configure_damage_execution', toolName: 'manage_combat', arguments: { action: 'configure_damage_execution', damageTypeName: 'DT_Bullet', damageImpulse: 500, criticalMultiplier: 2.0 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_hitbox_component', toolName: 'manage_combat', arguments: { action: 'setup_hitbox_component', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, hitboxBoneName: 'head', hitboxType: 'Sphere', hitboxSize: { radius: 15 }, isDamageZoneHead: true, damageMultiplier: 2.0 }, expected: 'success|not found' },
  { scenario: 'Combat: apply_damage_with_effects', toolName: 'manage_combat', arguments: { action: 'apply_damage_with_effects', targetActor: 'Enemy_01', baseDamage: 50, damageType: `${TEST_FOLDER}/DT_Bullet`, instigator: 'Player_01' }, expected: 'success|not found' },
  
  // === Reload & Ammo ===
  { scenario: 'Combat: setup_reload_system', toolName: 'manage_combat', arguments: { action: 'setup_reload_system', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, magazineSize: 30, reloadTime: 2.0 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_ammo_system', toolName: 'manage_combat', arguments: { action: 'setup_ammo_system', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, ammoType: 'Rifle', maxAmmo: 180, startingAmmo: 90 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_attachment_system', toolName: 'manage_combat', arguments: { action: 'setup_attachment_system', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, attachmentSlots: [{ slotName: 'Scope', socketName: 'Rail_Top' }, { slotName: 'Grip', socketName: 'Rail_Bottom' }] }, expected: 'success|not found' },
  { scenario: 'Combat: setup_weapon_switching', toolName: 'manage_combat', arguments: { action: 'setup_weapon_switching', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, switchInTime: 0.5, switchOutTime: 0.3 }, expected: 'success|not found' },
  
  // === Weapon Effects ===
  { scenario: 'Combat: configure_muzzle_flash', toolName: 'manage_combat', arguments: { action: 'configure_muzzle_flash', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, muzzleFlashParticlePath: '/Engine/Particles/P_MuzzleFlash', muzzleFlashScale: 1.0 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_tracer', toolName: 'manage_combat', arguments: { action: 'configure_tracer', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, tracerParticlePath: '/Engine/Particles/P_Tracer', tracerSpeed: 20000 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_impact_effects', toolName: 'manage_combat', arguments: { action: 'configure_impact_effects', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, impactParticlePath: '/Engine/Particles/P_Impact', impactSoundPath: '/Engine/Sounds/S_Impact' }, expected: 'success|not found' },
  { scenario: 'Combat: configure_shell_ejection', toolName: 'manage_combat', arguments: { action: 'configure_shell_ejection', blueprintPath: `${TEST_FOLDER}/BP_Rifle`, shellMeshPath: '/Engine/BasicShapes/Cylinder', shellEjectionForce: 200, shellLifespan: 5.0 }, expected: 'success|not found' },
  
  // === Melee Combat ===
  { scenario: 'Combat: create_melee_trace', toolName: 'manage_combat', arguments: { action: 'create_melee_trace', blueprintPath: `${TEST_FOLDER}/BP_Sword`, meleeTraceStartSocket: 'Blade_Start', meleeTraceEndSocket: 'Blade_End', meleeTraceRadius: 10 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_melee_trace', toolName: 'manage_combat', arguments: { action: 'configure_melee_trace', blueprintPath: `${TEST_FOLDER}/BP_Sword`, meleeTraceChannel: 'Weapon', meleeTraceRadius: 15 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_combo_system', toolName: 'manage_combat', arguments: { action: 'configure_combo_system', blueprintPath: `${TEST_FOLDER}/BP_Sword`, comboWindowTime: 0.5, maxComboCount: 3, comboAnimations: [`${TEST_FOLDER}/AM_Slash1`, `${TEST_FOLDER}/AM_Slash2`, `${TEST_FOLDER}/AM_Slash3`] }, expected: 'success|not found' },
  { scenario: 'Combat: create_combo_sequence', toolName: 'manage_combat', arguments: { action: 'create_combo_sequence', name: 'CS_SwordCombo', path: TEST_FOLDER, maxComboCount: 4 }, expected: 'success|not found' },
  { scenario: 'Combat: create_hit_pause', toolName: 'manage_combat', arguments: { action: 'create_hit_pause', blueprintPath: `${TEST_FOLDER}/BP_Sword`, hitPauseDuration: 0.1, hitPauseTimeDilation: 0.01 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_hit_reaction', toolName: 'manage_combat', arguments: { action: 'configure_hit_reaction', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, hitReactionMontage: `${TEST_FOLDER}/AM_HitReact`, hitReactionStunTime: 0.3 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_parry_block_system', toolName: 'manage_combat', arguments: { action: 'setup_parry_block_system', blueprintPath: `${TEST_FOLDER}/BP_Sword`, parryWindowStart: 0.1, parryWindowEnd: 0.3, blockDamageReduction: 0.8 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_block_parry', toolName: 'manage_combat', arguments: { action: 'configure_block_parry', blueprintPath: `${TEST_FOLDER}/BP_Shield`, blockDamageReduction: 0.9, blockStaminaCost: 10 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_weapon_trails', toolName: 'manage_combat', arguments: { action: 'configure_weapon_trails', blueprintPath: `${TEST_FOLDER}/BP_Sword`, weaponTrailParticlePath: '/Engine/Particles/P_Trail', weaponTrailStartSocket: 'Blade_Start', weaponTrailEndSocket: 'Blade_End' }, expected: 'success|not found' },
  
  // === Info Query ===
  { scenario: 'Combat: get_combat_info', toolName: 'manage_combat', arguments: { action: 'get_combat_info', blueprintPath: `${TEST_FOLDER}/BP_Rifle` }, expected: 'success|not found' },
  { scenario: 'Combat: get_combat_stats', toolName: 'manage_combat', arguments: { action: 'get_combat_stats', actorName: 'Player_01' }, expected: 'success|not found' },
  
  // === GAS - Ability System Component ===
  { scenario: 'Combat: add_ability_system_component', toolName: 'manage_combat', arguments: { action: 'add_ability_system_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Combat: configure_asc', toolName: 'manage_combat', arguments: { action: 'configure_asc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, replicationMode: 'Mixed' }, expected: 'success|not found' },
  
  // === GAS - Attribute Set ===
  { scenario: 'Combat: create_attribute_set', toolName: 'manage_combat', arguments: { action: 'create_attribute_set', name: 'AS_Character', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: add_attribute', toolName: 'manage_combat', arguments: { action: 'add_attribute', attributeSetPath: `${TEST_FOLDER}/AS_Character`, attributeName: 'Health', defaultValue: 100 }, expected: 'success|not found' },
  { scenario: 'Combat: set_attribute_base_value', toolName: 'manage_combat', arguments: { action: 'set_attribute_base_value', actorName: 'Player_01', attributeName: 'Health', value: 100 }, expected: 'success|not found' },
  { scenario: 'Combat: set_attribute_clamping', toolName: 'manage_combat', arguments: { action: 'set_attribute_clamping', attributeSetPath: `${TEST_FOLDER}/AS_Character`, attributeName: 'Health', minValue: 0, maxValue: 100 }, expected: 'success|not found' },
  
  // === GAS - Gameplay Ability ===
  { scenario: 'Combat: create_gameplay_ability', toolName: 'manage_combat', arguments: { action: 'create_gameplay_ability', name: 'GA_Fireball', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: set_ability_tags', toolName: 'manage_combat', arguments: { action: 'set_ability_tags', abilityPath: `${TEST_FOLDER}/GA_Fireball`, tags: ['Ability.Spell.Fire'] }, expected: 'success|not found' },
  { scenario: 'Combat: set_ability_costs', toolName: 'manage_combat', arguments: { action: 'set_ability_costs', abilityPath: `${TEST_FOLDER}/GA_Fireball`, costAttribute: 'Mana', costValue: 25 }, expected: 'success|not found' },
  { scenario: 'Combat: set_ability_cooldown', toolName: 'manage_combat', arguments: { action: 'set_ability_cooldown', abilityPath: `${TEST_FOLDER}/GA_Fireball`, cooldownDuration: 3.0 }, expected: 'success|not found' },
  { scenario: 'Combat: set_ability_targeting', toolName: 'manage_combat', arguments: { action: 'set_ability_targeting', abilityPath: `${TEST_FOLDER}/GA_Fireball`, targetingType: 'AimFromPlayer', range: 2000 }, expected: 'success|not found' },
  { scenario: 'Combat: add_ability_task', toolName: 'manage_combat', arguments: { action: 'add_ability_task', abilityPath: `${TEST_FOLDER}/GA_Fireball`, taskType: 'PlayMontageAndWait' }, expected: 'success|not found' },
  { scenario: 'Combat: set_activation_policy', toolName: 'manage_combat', arguments: { action: 'set_activation_policy', abilityPath: `${TEST_FOLDER}/GA_Fireball`, policy: 'OnInputPressed' }, expected: 'success|not found' },
  { scenario: 'Combat: set_instancing_policy', toolName: 'manage_combat', arguments: { action: 'set_instancing_policy', abilityPath: `${TEST_FOLDER}/GA_Fireball`, policy: 'InstancedPerActor' }, expected: 'success|not found' },
  { scenario: 'Combat: grant_gas_ability', toolName: 'manage_combat', arguments: { action: 'grant_gas_ability', actorName: 'Player_01', abilityPath: `${TEST_FOLDER}/GA_Fireball` }, expected: 'success|not found' },
  
  // === GAS - Gameplay Effect ===
  { scenario: 'Combat: create_gameplay_effect', toolName: 'manage_combat', arguments: { action: 'create_gameplay_effect', name: 'GE_Damage', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: set_effect_duration', toolName: 'manage_combat', arguments: { action: 'set_effect_duration', effectPath: `${TEST_FOLDER}/GE_Damage`, durationType: 'Instant' }, expected: 'success|not found' },
  { scenario: 'Combat: add_effect_modifier', toolName: 'manage_combat', arguments: { action: 'add_effect_modifier', effectPath: `${TEST_FOLDER}/GE_Damage`, attribute: 'Health', modifierOp: 'Additive', magnitude: -25 }, expected: 'success|not found' },
  { scenario: 'Combat: set_modifier_magnitude', toolName: 'manage_combat', arguments: { action: 'set_modifier_magnitude', effectPath: `${TEST_FOLDER}/GE_Damage`, modifierIndex: 0, magnitude: -50 }, expected: 'success|not found' },
  { scenario: 'Combat: add_effect_execution_calculation', toolName: 'manage_combat', arguments: { action: 'add_effect_execution_calculation', effectPath: `${TEST_FOLDER}/GE_Damage`, calculationClass: 'DamageExecution' }, expected: 'success|not found' },
  { scenario: 'Combat: add_effect_cue', toolName: 'manage_combat', arguments: { action: 'add_effect_cue', effectPath: `${TEST_FOLDER}/GE_Damage`, cueTag: 'GameplayCue.Damage' }, expected: 'success|not found' },
  { scenario: 'Combat: set_effect_stacking', toolName: 'manage_combat', arguments: { action: 'set_effect_stacking', effectPath: `${TEST_FOLDER}/GE_Burning`, stackingType: 'AggregateBySource', stackLimit: 5 }, expected: 'success|not found' },
  { scenario: 'Combat: set_effect_tags', toolName: 'manage_combat', arguments: { action: 'set_effect_tags', effectPath: `${TEST_FOLDER}/GE_Damage`, grantedTags: ['State.Damaged'], blockedTags: ['State.Invincible'] }, expected: 'success|not found' },
  { scenario: 'Combat: configure_gas_effect', toolName: 'manage_combat', arguments: { action: 'configure_gas_effect', effectPath: `${TEST_FOLDER}/GE_Burning`, duration: 5.0, period: 0.5 }, expected: 'success|not found' },
  
  // === GAS - Gameplay Cue ===
  { scenario: 'Combat: create_gameplay_cue_notify', toolName: 'manage_combat', arguments: { action: 'create_gameplay_cue_notify', name: 'GCN_Burning', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: configure_cue_trigger', toolName: 'manage_combat', arguments: { action: 'configure_cue_trigger', cuePath: `${TEST_FOLDER}/GCN_Burning`, triggerType: 'OnActive' }, expected: 'success|not found' },
  { scenario: 'Combat: set_cue_effects', toolName: 'manage_combat', arguments: { action: 'set_cue_effects', cuePath: `${TEST_FOLDER}/GCN_Burning`, particlePath: '/Engine/Particles/P_Fire', soundPath: '/Engine/Sounds/S_Fire' }, expected: 'success|not found' },
  
  // === GAS - Tags ===
  { scenario: 'Combat: add_tag_to_asset', toolName: 'manage_combat', arguments: { action: 'add_tag_to_asset', assetPath: `${TEST_FOLDER}/GA_Fireball`, tag: 'Ability.Magic.Fire' }, expected: 'success|not found' },
  { scenario: 'Combat: get_gas_info', toolName: 'manage_combat', arguments: { action: 'get_gas_info', actorName: 'Player_01' }, expected: 'success|not found' },
  
  // === EXTENDED MANAGE_COMBAT (Additional ~80 tests for comprehensive coverage) ===
  // More Weapon Variations
  { scenario: 'Combat: create_weapon_blueprint_shotgun', toolName: 'manage_combat', arguments: { action: 'create_weapon_blueprint', name: 'BP_Shotgun', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: create_weapon_blueprint_pistol', toolName: 'manage_combat', arguments: { action: 'create_weapon_blueprint', name: 'BP_Pistol', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: create_weapon_blueprint_sniper', toolName: 'manage_combat', arguments: { action: 'create_weapon_blueprint', name: 'BP_Sniper', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: create_weapon_blueprint_sword', toolName: 'manage_combat', arguments: { action: 'create_weapon_blueprint', name: 'BP_Sword', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: configure_weapon_mesh_pistol', toolName: 'manage_combat', arguments: { action: 'configure_weapon_mesh', blueprintPath: `${TEST_FOLDER}/BP_Pistol`, weaponMeshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Combat: set_weapon_stats_shotgun', toolName: 'manage_combat', arguments: { action: 'set_weapon_stats', blueprintPath: `${TEST_FOLDER}/BP_Shotgun`, baseDamage: 15, fireRate: 60, range: 1500 }, expected: 'success|not found' },
  { scenario: 'Combat: set_weapon_stats_sniper', toolName: 'manage_combat', arguments: { action: 'set_weapon_stats', blueprintPath: `${TEST_FOLDER}/BP_Sniper`, baseDamage: 100, fireRate: 30, range: 15000 }, expected: 'success|not found' },
  { scenario: 'Combat: set_weapon_stats_pistol', toolName: 'manage_combat', arguments: { action: 'set_weapon_stats', blueprintPath: `${TEST_FOLDER}/BP_Pistol`, baseDamage: 20, fireRate: 300, range: 3000 }, expected: 'success|not found' },
  
  // More Hitscan & Projectile Configurations
  { scenario: 'Combat: configure_hitscan_sniper', toolName: 'manage_combat', arguments: { action: 'configure_hitscan', blueprintPath: `${TEST_FOLDER}/BP_Sniper`, hitscanEnabled: true, traceChannel: 'Sniper' }, expected: 'success|not found' },
  { scenario: 'Combat: configure_spread_pattern_shotgun', toolName: 'manage_combat', arguments: { action: 'configure_spread_pattern', blueprintPath: `${TEST_FOLDER}/BP_Shotgun`, spreadPattern: 'Fixed', spread: 8.0, spreadIncrease: 0, spreadRecovery: 0 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_recoil_pattern_pistol', toolName: 'manage_combat', arguments: { action: 'configure_recoil_pattern', blueprintPath: `${TEST_FOLDER}/BP_Pistol`, recoilPitch: 0.3, recoilYaw: 0.1, recoilRecovery: 8.0 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_aim_down_sights_sniper', toolName: 'manage_combat', arguments: { action: 'configure_aim_down_sights', blueprintPath: `${TEST_FOLDER}/BP_Sniper`, adsEnabled: true, adsFov: 30, adsSpeed: 0.3, adsSpreadMultiplier: 0.1 }, expected: 'success|not found' },
  
  // More Projectile Variations
  { scenario: 'Combat: create_projectile_blueprint_rocket', toolName: 'manage_combat', arguments: { action: 'create_projectile_blueprint', name: 'BP_Rocket', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: create_projectile_blueprint_arrow', toolName: 'manage_combat', arguments: { action: 'create_projectile_blueprint', name: 'BP_Arrow', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: configure_projectile_movement_rocket', toolName: 'manage_combat', arguments: { action: 'configure_projectile_movement', blueprintPath: `${TEST_FOLDER}/BP_Rocket`, projectileSpeed: 3000, projectileGravityScale: 0, projectileLifespan: 5.0 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_projectile_movement_arrow', toolName: 'manage_combat', arguments: { action: 'configure_projectile_movement', blueprintPath: `${TEST_FOLDER}/BP_Arrow`, projectileSpeed: 5000, projectileGravityScale: 0.5, projectileLifespan: 10.0 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_projectile_collision_rocket', toolName: 'manage_combat', arguments: { action: 'configure_projectile_collision', blueprintPath: `${TEST_FOLDER}/BP_Rocket`, collisionRadius: 20, bounceEnabled: false }, expected: 'success|not found' },
  { scenario: 'Combat: configure_projectile_homing_missile', toolName: 'manage_combat', arguments: { action: 'configure_projectile_homing', blueprintPath: `${TEST_FOLDER}/BP_Rocket`, homingEnabled: true, homingAcceleration: 10000, homingTargetTag: 'Vehicle' }, expected: 'success|not found' },
  { scenario: 'Combat: create_projectile_pool_rocket', toolName: 'manage_combat', arguments: { action: 'create_projectile_pool', blueprintPath: `${TEST_FOLDER}/BP_Rocket`, poolSize: 20 }, expected: 'success|not found' },
  { scenario: 'Combat: create_projectile_pool_arrow', toolName: 'manage_combat', arguments: { action: 'create_projectile_pool', blueprintPath: `${TEST_FOLDER}/BP_Arrow`, poolSize: 30 }, expected: 'success|not found' },
  
  // More Damage System
  { scenario: 'Combat: create_damage_type_fire', toolName: 'manage_combat', arguments: { action: 'create_damage_type', name: 'DT_Fire', path: TEST_FOLDER, damageCategory: 'Elemental' }, expected: 'success|already exists' },
  { scenario: 'Combat: create_damage_type_explosive', toolName: 'manage_combat', arguments: { action: 'create_damage_type', name: 'DT_Explosive', path: TEST_FOLDER, damageCategory: 'Physical' }, expected: 'success|already exists' },
  { scenario: 'Combat: create_damage_type_poison', toolName: 'manage_combat', arguments: { action: 'create_damage_type', name: 'DT_Poison', path: TEST_FOLDER, damageCategory: 'DoT' }, expected: 'success|already exists' },
  { scenario: 'Combat: configure_damage_execution_fire', toolName: 'manage_combat', arguments: { action: 'configure_damage_execution', damageTypeName: 'DT_Fire', damageImpulse: 100, criticalMultiplier: 1.5 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_damage_execution_explosive', toolName: 'manage_combat', arguments: { action: 'configure_damage_execution', damageTypeName: 'DT_Explosive', damageImpulse: 2000, criticalMultiplier: 1.0 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_hitbox_component_body', toolName: 'manage_combat', arguments: { action: 'setup_hitbox_component', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, hitboxBoneName: 'spine_03', hitboxType: 'Capsule', hitboxSize: { radius: 25, halfHeight: 40 }, isDamageZoneHead: false, damageMultiplier: 1.0 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_hitbox_component_legs', toolName: 'manage_combat', arguments: { action: 'setup_hitbox_component', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, hitboxBoneName: 'thigh_l', hitboxType: 'Capsule', hitboxSize: { radius: 12, halfHeight: 25 }, isDamageZoneHead: false, damageMultiplier: 0.75 }, expected: 'success|not found' },
  { scenario: 'Combat: apply_damage_with_effects_fire', toolName: 'manage_combat', arguments: { action: 'apply_damage_with_effects', targetActor: 'Enemy_01', baseDamage: 30, damageType: `${TEST_FOLDER}/DT_Fire`, instigator: 'Player_01' }, expected: 'success|not found' },
  { scenario: 'Combat: apply_damage_with_effects_explosive', toolName: 'manage_combat', arguments: { action: 'apply_damage_with_effects', targetActor: 'Enemy_02', baseDamage: 100, damageType: `${TEST_FOLDER}/DT_Explosive`, instigator: 'Player_01' }, expected: 'success|not found' },
  
  // More Reload & Ammo
  { scenario: 'Combat: setup_reload_system_shotgun', toolName: 'manage_combat', arguments: { action: 'setup_reload_system', blueprintPath: `${TEST_FOLDER}/BP_Shotgun`, magazineSize: 8, reloadTime: 0.5 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_reload_system_sniper', toolName: 'manage_combat', arguments: { action: 'setup_reload_system', blueprintPath: `${TEST_FOLDER}/BP_Sniper`, magazineSize: 5, reloadTime: 3.0 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_ammo_system_shotgun', toolName: 'manage_combat', arguments: { action: 'setup_ammo_system', blueprintPath: `${TEST_FOLDER}/BP_Shotgun`, ammoType: 'Shells', maxAmmo: 40, startingAmmo: 24 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_ammo_system_sniper', toolName: 'manage_combat', arguments: { action: 'setup_ammo_system', blueprintPath: `${TEST_FOLDER}/BP_Sniper`, ammoType: 'SniperRounds', maxAmmo: 30, startingAmmo: 15 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_attachment_system_pistol', toolName: 'manage_combat', arguments: { action: 'setup_attachment_system', blueprintPath: `${TEST_FOLDER}/BP_Pistol`, attachmentSlots: [{ slotName: 'Suppressor', socketName: 'Muzzle' }] }, expected: 'success|not found' },
  { scenario: 'Combat: setup_weapon_switching_fast', toolName: 'manage_combat', arguments: { action: 'setup_weapon_switching', blueprintPath: `${TEST_FOLDER}/BP_Pistol`, switchInTime: 0.3, switchOutTime: 0.2 }, expected: 'success|not found' },
  
  // More Weapon Effects
  { scenario: 'Combat: configure_muzzle_flash_shotgun', toolName: 'manage_combat', arguments: { action: 'configure_muzzle_flash', blueprintPath: `${TEST_FOLDER}/BP_Shotgun`, muzzleFlashParticlePath: '/Engine/Particles/P_MuzzleFlash_Large', muzzleFlashScale: 1.5 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_tracer_sniper', toolName: 'manage_combat', arguments: { action: 'configure_tracer', blueprintPath: `${TEST_FOLDER}/BP_Sniper`, tracerParticlePath: '/Engine/Particles/P_Tracer_Sniper', tracerSpeed: 50000 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_impact_effects_explosive', toolName: 'manage_combat', arguments: { action: 'configure_impact_effects', blueprintPath: `${TEST_FOLDER}/BP_Rocket`, impactParticlePath: '/Engine/Particles/P_Explosion', impactSoundPath: '/Engine/Sounds/S_Explosion' }, expected: 'success|not found' },
  { scenario: 'Combat: configure_shell_ejection_shotgun', toolName: 'manage_combat', arguments: { action: 'configure_shell_ejection', blueprintPath: `${TEST_FOLDER}/BP_Shotgun`, shellMeshPath: '/Engine/BasicShapes/Cylinder', shellEjectionForce: 150, shellLifespan: 8.0 }, expected: 'success|not found' },
  
  // More Melee Combat
  { scenario: 'Combat: create_melee_trace_axe', toolName: 'manage_combat', arguments: { action: 'create_melee_trace', blueprintPath: `${TEST_FOLDER}/BP_Axe`, meleeTraceStartSocket: 'Handle', meleeTraceEndSocket: 'Blade', meleeTraceRadius: 20 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_melee_trace_fist', toolName: 'manage_combat', arguments: { action: 'configure_melee_trace', blueprintPath: `${TEST_FOLDER}/BP_Fist`, meleeTraceChannel: 'Melee', meleeTraceRadius: 8 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_combo_system_axe', toolName: 'manage_combat', arguments: { action: 'configure_combo_system', blueprintPath: `${TEST_FOLDER}/BP_Axe`, comboWindowTime: 0.6, maxComboCount: 2, comboAnimations: [`${TEST_FOLDER}/AM_AxeSlash1`, `${TEST_FOLDER}/AM_AxeSlash2`] }, expected: 'success|not found' },
  { scenario: 'Combat: create_combo_sequence_katana', toolName: 'manage_combat', arguments: { action: 'create_combo_sequence', name: 'CS_KatanaCombo', path: TEST_FOLDER, maxComboCount: 5 }, expected: 'success|not found' },
  { scenario: 'Combat: create_hit_pause_heavy', toolName: 'manage_combat', arguments: { action: 'create_hit_pause', blueprintPath: `${TEST_FOLDER}/BP_Axe`, hitPauseDuration: 0.15, hitPauseTimeDilation: 0.02 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_hit_reaction_stagger', toolName: 'manage_combat', arguments: { action: 'configure_hit_reaction', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, hitReactionMontage: `${TEST_FOLDER}/AM_Stagger`, hitReactionStunTime: 0.5 }, expected: 'success|not found' },
  { scenario: 'Combat: setup_parry_block_system_shield', toolName: 'manage_combat', arguments: { action: 'setup_parry_block_system', blueprintPath: `${TEST_FOLDER}/BP_Shield`, parryWindowStart: 0.05, parryWindowEnd: 0.25, blockDamageReduction: 0.95 }, expected: 'success|not found' },
  { scenario: 'Combat: configure_weapon_trails_axe', toolName: 'manage_combat', arguments: { action: 'configure_weapon_trails', blueprintPath: `${TEST_FOLDER}/BP_Axe`, weaponTrailParticlePath: '/Engine/Particles/P_TrailWide', weaponTrailStartSocket: 'Handle', weaponTrailEndSocket: 'Blade' }, expected: 'success|not found' },
  
  // More GAS Abilities
  { scenario: 'Combat: create_gameplay_ability_dash', toolName: 'manage_combat', arguments: { action: 'create_gameplay_ability', name: 'GA_Dash', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: create_gameplay_ability_heal', toolName: 'manage_combat', arguments: { action: 'create_gameplay_ability', name: 'GA_Heal', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: create_gameplay_ability_shield', toolName: 'manage_combat', arguments: { action: 'create_gameplay_ability', name: 'GA_Shield', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: set_ability_tags_dash', toolName: 'manage_combat', arguments: { action: 'set_ability_tags', abilityPath: `${TEST_FOLDER}/GA_Dash`, tags: ['Ability.Movement.Dash'] }, expected: 'success|not found' },
  { scenario: 'Combat: set_ability_costs_heal', toolName: 'manage_combat', arguments: { action: 'set_ability_costs', abilityPath: `${TEST_FOLDER}/GA_Heal`, costAttribute: 'Mana', costValue: 50 }, expected: 'success|not found' },
  { scenario: 'Combat: set_ability_cooldown_dash', toolName: 'manage_combat', arguments: { action: 'set_ability_cooldown', abilityPath: `${TEST_FOLDER}/GA_Dash`, cooldownDuration: 5.0 }, expected: 'success|not found' },
  { scenario: 'Combat: add_ability_task_wait', toolName: 'manage_combat', arguments: { action: 'add_ability_task', abilityPath: `${TEST_FOLDER}/GA_Dash`, taskType: 'WaitDelay' }, expected: 'success|not found' },
  { scenario: 'Combat: grant_gas_ability_dash', toolName: 'manage_combat', arguments: { action: 'grant_gas_ability', actorName: 'Player_01', abilityPath: `${TEST_FOLDER}/GA_Dash` }, expected: 'success|not found' },
  { scenario: 'Combat: grant_gas_ability_heal', toolName: 'manage_combat', arguments: { action: 'grant_gas_ability', actorName: 'Player_01', abilityPath: `${TEST_FOLDER}/GA_Heal` }, expected: 'success|not found' },
  
  // More GAS Attributes
  { scenario: 'Combat: add_attribute_mana', toolName: 'manage_combat', arguments: { action: 'add_attribute', attributeSetPath: `${TEST_FOLDER}/AS_Character`, attributeName: 'Mana', defaultValue: 100 }, expected: 'success|not found' },
  { scenario: 'Combat: add_attribute_stamina', toolName: 'manage_combat', arguments: { action: 'add_attribute', attributeSetPath: `${TEST_FOLDER}/AS_Character`, attributeName: 'Stamina', defaultValue: 100 }, expected: 'success|not found' },
  { scenario: 'Combat: add_attribute_armor', toolName: 'manage_combat', arguments: { action: 'add_attribute', attributeSetPath: `${TEST_FOLDER}/AS_Character`, attributeName: 'Armor', defaultValue: 0 }, expected: 'success|not found' },
  { scenario: 'Combat: set_attribute_base_value_mana', toolName: 'manage_combat', arguments: { action: 'set_attribute_base_value', actorName: 'Player_01', attributeName: 'Mana', value: 150 }, expected: 'success|not found' },
  { scenario: 'Combat: set_attribute_clamping_stamina', toolName: 'manage_combat', arguments: { action: 'set_attribute_clamping', attributeSetPath: `${TEST_FOLDER}/AS_Character`, attributeName: 'Stamina', minValue: 0, maxValue: 200 }, expected: 'success|not found' },
  
  // More GAS Effects
  { scenario: 'Combat: create_gameplay_effect_burning', toolName: 'manage_combat', arguments: { action: 'create_gameplay_effect', name: 'GE_Burning', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: create_gameplay_effect_heal', toolName: 'manage_combat', arguments: { action: 'create_gameplay_effect', name: 'GE_Heal', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: create_gameplay_effect_buff', toolName: 'manage_combat', arguments: { action: 'create_gameplay_effect', name: 'GE_AttackBuff', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: set_effect_duration_burning', toolName: 'manage_combat', arguments: { action: 'set_effect_duration', effectPath: `${TEST_FOLDER}/GE_Burning`, durationType: 'HasDuration' }, expected: 'success|not found' },
  { scenario: 'Combat: add_effect_modifier_heal', toolName: 'manage_combat', arguments: { action: 'add_effect_modifier', effectPath: `${TEST_FOLDER}/GE_Heal`, attribute: 'Health', modifierOp: 'Additive', magnitude: 50 }, expected: 'success|not found' },
  { scenario: 'Combat: add_effect_modifier_buff', toolName: 'manage_combat', arguments: { action: 'add_effect_modifier', effectPath: `${TEST_FOLDER}/GE_AttackBuff`, attribute: 'AttackPower', modifierOp: 'Multiply', magnitude: 1.25 }, expected: 'success|not found' },
  { scenario: 'Combat: set_effect_stacking_buff', toolName: 'manage_combat', arguments: { action: 'set_effect_stacking', effectPath: `${TEST_FOLDER}/GE_AttackBuff`, stackingType: 'AggregateByTarget', stackLimit: 3 }, expected: 'success|not found' },
  
  // More GAS Cues
  { scenario: 'Combat: create_gameplay_cue_notify_heal', toolName: 'manage_combat', arguments: { action: 'create_gameplay_cue_notify', name: 'GCN_Heal', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Combat: configure_cue_trigger_heal', toolName: 'manage_combat', arguments: { action: 'configure_cue_trigger', cuePath: `${TEST_FOLDER}/GCN_Heal`, triggerType: 'OnExecute' }, expected: 'success|not found' },
  { scenario: 'Combat: set_cue_effects_heal', toolName: 'manage_combat', arguments: { action: 'set_cue_effects', cuePath: `${TEST_FOLDER}/GCN_Heal`, particlePath: '/Engine/Particles/P_Heal', soundPath: '/Engine/Sounds/S_Heal' }, expected: 'success|not found' },
  { scenario: 'Combat: add_tag_to_asset_heal', toolName: 'manage_combat', arguments: { action: 'add_tag_to_asset', assetPath: `${TEST_FOLDER}/GA_Heal`, tag: 'Ability.Support.Heal' }, expected: 'success|not found' },
  
  // More Combat Info
  { scenario: 'Combat: get_combat_info_shotgun', toolName: 'manage_combat', arguments: { action: 'get_combat_info', blueprintPath: `${TEST_FOLDER}/BP_Shotgun` }, expected: 'success|not found' },
  { scenario: 'Combat: get_combat_stats_enemy', toolName: 'manage_combat', arguments: { action: 'get_combat_stats', actorName: 'Enemy_01' }, expected: 'success|not found' },
  { scenario: 'Combat: get_gas_info_enemy', toolName: 'manage_combat', arguments: { action: 'get_gas_info', actorName: 'Enemy_01' }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_AI (103 actions) - FULL COVERAGE
// ============================================================================
const manageAITests = [
  // === AI Controller ===
  { scenario: 'AI: create_ai_controller', toolName: 'manage_ai', arguments: { action: 'create_ai_controller', name: 'AIC_Enemy', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: assign_behavior_tree', toolName: 'manage_ai', arguments: { action: 'assign_behavior_tree', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, behaviorTreePath: `${TEST_FOLDER}/BT_Enemy` }, expected: 'success|not found' },
  { scenario: 'AI: assign_blackboard', toolName: 'manage_ai', arguments: { action: 'assign_blackboard', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, blackboardPath: `${TEST_FOLDER}/BB_Enemy` }, expected: 'success|not found' },
  { scenario: 'AI: get_ai_info', toolName: 'manage_ai', arguments: { action: 'get_ai_info', controllerPath: `${TEST_FOLDER}/AIC_Enemy` }, expected: 'success|not found' },
  
  // === Blackboard ===
  { scenario: 'AI: create_blackboard_asset', toolName: 'manage_ai', arguments: { action: 'create_blackboard_asset', name: 'BB_Enemy', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: add_blackboard_key_object', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'TargetActor', keyType: 'Object', baseObjectClass: 'Actor' }, expected: 'success|not found' },
  { scenario: 'AI: add_blackboard_key_vector', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'PatrolLocation', keyType: 'Vector' }, expected: 'success|not found' },
  { scenario: 'AI: add_blackboard_key_float', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'DistanceToTarget', keyType: 'Float' }, expected: 'success|not found' },
  { scenario: 'AI: add_blackboard_key_bool', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'IsAggro', keyType: 'Bool' }, expected: 'success|not found' },
  { scenario: 'AI: add_blackboard_key_int', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'AmmoCount', keyType: 'Int' }, expected: 'success|not found' },
  { scenario: 'AI: add_blackboard_key_enum', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'CombatState', keyType: 'Enum', enumClass: 'ECombatState' }, expected: 'success|not found' },
  { scenario: 'AI: set_key_instance_synced', toolName: 'manage_ai', arguments: { action: 'set_key_instance_synced', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'TargetActor', isInstanceSynced: true }, expected: 'success|not found' },
  
  // === Behavior Tree ===
  { scenario: 'AI: create_behavior_tree', toolName: 'manage_ai', arguments: { action: 'create_behavior_tree', name: 'BT_Enemy', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: add_composite_selector', toolName: 'manage_ai', arguments: { action: 'add_composite_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, compositeType: 'Selector', nodeName: 'MainSelector' }, expected: 'success|not found' },
  { scenario: 'AI: add_composite_sequence', toolName: 'manage_ai', arguments: { action: 'add_composite_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, compositeType: 'Sequence', nodeName: 'AttackSequence', parentNodeId: 'MainSelector' }, expected: 'success|not found' },
  { scenario: 'AI: add_composite_parallel', toolName: 'manage_ai', arguments: { action: 'add_composite_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, compositeType: 'Parallel', nodeName: 'PatrolParallel' }, expected: 'success|not found' },
  { scenario: 'AI: add_task_moveto', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'MoveTo', parentNodeId: 'AttackSequence' }, expected: 'success|not found' },
  { scenario: 'AI: add_task_wait', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'Wait', parentNodeId: 'PatrolSequence', nodeProperties: { waitTime: 2.0 } }, expected: 'success|not found' },
  { scenario: 'AI: add_task_playanim', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'PlayAnimation', parentNodeId: 'AttackSequence' }, expected: 'success|not found' },
  { scenario: 'AI: add_task_runeqs', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'RunEQSQuery', parentNodeId: 'FindCoverSequence' }, expected: 'success|not found' },
  { scenario: 'AI: add_task_setbbvalue', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'SetBlackboardValue', parentNodeId: 'AttackSequence' }, expected: 'success|not found' },
  { scenario: 'AI: add_task_custom', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'Custom', customTaskClass: `${TEST_FOLDER}/BTT_FireWeapon`, parentNodeId: 'AttackSequence' }, expected: 'success|not found' },
  { scenario: 'AI: add_decorator_blackboard', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'AttackSequence', decoratorType: 'Blackboard', keyName: 'TargetActor' }, expected: 'success|not found' },
  { scenario: 'AI: add_decorator_cooldown', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'AttackSequence', decoratorType: 'Cooldown', nodeProperties: { cooldownTime: 2.0 } }, expected: 'success|not found' },
  { scenario: 'AI: add_decorator_loop', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'PatrolSequence', decoratorType: 'Loop' }, expected: 'success|not found' },
  { scenario: 'AI: add_service_focus', toolName: 'manage_ai', arguments: { action: 'add_service', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'MainSelector', serviceType: 'DefaultFocus' }, expected: 'success|not found' },
  { scenario: 'AI: add_service_runeqs', toolName: 'manage_ai', arguments: { action: 'add_service', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'MainSelector', serviceType: 'RunEQS' }, expected: 'success|not found' },
  { scenario: 'AI: configure_bt_node', toolName: 'manage_ai', arguments: { action: 'configure_bt_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'MoveTo_1', nodeProperties: { acceptableRadius: 50 } }, expected: 'success|not found' },
  { scenario: 'AI: debug_behavior_tree', toolName: 'manage_ai', arguments: { action: 'debug_behavior_tree', actorName: 'Enemy_01' }, expected: 'success|not found' },
  
  // === BT Graph Operations ===
  { scenario: 'AI: bt_add_node', toolName: 'manage_ai', arguments: { action: 'bt_add_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeType: 'Selector', x: 100, y: 200 }, expected: 'success|not found' },
  { scenario: 'AI: bt_connect_nodes', toolName: 'manage_ai', arguments: { action: 'bt_connect_nodes', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, parentNodeId: 'Root', childNodeId: 'MainSelector' }, expected: 'success|not found' },
  { scenario: 'AI: bt_remove_node', toolName: 'manage_ai', arguments: { action: 'bt_remove_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'OldNode' }, expected: 'success|not found' },
  { scenario: 'AI: bt_break_connections', toolName: 'manage_ai', arguments: { action: 'bt_break_connections', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'MainSelector' }, expected: 'success|not found' },
  { scenario: 'AI: bt_set_node_properties', toolName: 'manage_ai', arguments: { action: 'bt_set_node_properties', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'Wait_1', btNodeProperties: { waitTime: 3.0 } }, expected: 'success|not found' },
  
  // === EQS ===
  { scenario: 'AI: create_eqs_query', toolName: 'manage_ai', arguments: { action: 'create_eqs_query', name: 'EQS_FindCover', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: add_eqs_generator_grid', toolName: 'manage_ai', arguments: { action: 'add_eqs_generator', queryPath: `${TEST_FOLDER}/EQS_FindCover`, generatorType: 'SimpleGrid', generatorSettings: { gridSize: 1000, spacesBetween: 100 } }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_generator_donut', toolName: 'manage_ai', arguments: { action: 'add_eqs_generator', queryPath: `${TEST_FOLDER}/EQS_FindCover`, generatorType: 'Donut', generatorSettings: { innerRadius: 100, outerRadius: 500 } }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_generator_actors', toolName: 'manage_ai', arguments: { action: 'add_eqs_generator', queryPath: `${TEST_FOLDER}/EQS_FindTarget`, generatorType: 'ActorsOfClass', generatorSettings: { actorClass: 'Character' } }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_context', toolName: 'manage_ai', arguments: { action: 'add_eqs_context', queryPath: `${TEST_FOLDER}/EQS_FindCover`, contextType: 'Querier' }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_test_distance', toolName: 'manage_ai', arguments: { action: 'add_eqs_test', queryPath: `${TEST_FOLDER}/EQS_FindCover`, testType: 'Distance', testSettings: { scoringEquation: 'InverseLinear' } }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_test_trace', toolName: 'manage_ai', arguments: { action: 'add_eqs_test', queryPath: `${TEST_FOLDER}/EQS_FindCover`, testType: 'Trace', testSettings: { filterType: 'Range' } }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_test_pathfinding', toolName: 'manage_ai', arguments: { action: 'add_eqs_test', queryPath: `${TEST_FOLDER}/EQS_FindCover`, testType: 'Pathfinding' }, expected: 'success|not found' },
  { scenario: 'AI: configure_test_scoring', toolName: 'manage_ai', arguments: { action: 'configure_test_scoring', queryPath: `${TEST_FOLDER}/EQS_FindCover`, testIndex: 0, testSettings: { scoringEquation: 'Linear', clampMin: 0, clampMax: 1 } }, expected: 'success|not found' },
  { scenario: 'AI: query_eqs_results', toolName: 'manage_ai', arguments: { action: 'query_eqs_results', actorName: 'Enemy_01', queryPath: `${TEST_FOLDER}/EQS_FindCover` }, expected: 'success|not found' },
  
  // === Perception ===
  { scenario: 'AI: add_ai_perception_component', toolName: 'manage_ai', arguments: { action: 'add_ai_perception_component', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, dominantSense: 'Sight' }, expected: 'success|not found' },
  { scenario: 'AI: configure_sight_config', toolName: 'manage_ai', arguments: { action: 'configure_sight_config', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, sightConfig: { sightRadius: 3000, loseSightRadius: 3500, peripheralVisionAngle: 90 } }, expected: 'success|not found' },
  { scenario: 'AI: configure_hearing_config', toolName: 'manage_ai', arguments: { action: 'configure_hearing_config', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, hearingConfig: { hearingRange: 2000, detectFriendly: false } }, expected: 'success|not found' },
  { scenario: 'AI: configure_damage_sense_config', toolName: 'manage_ai', arguments: { action: 'configure_damage_sense_config', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, damageConfig: { maxAge: 10.0 } }, expected: 'success|not found' },
  { scenario: 'AI: set_perception_team', toolName: 'manage_ai', arguments: { action: 'set_perception_team', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, teamId: 2 }, expected: 'success|not found' },
  { scenario: 'AI: get_ai_perception_data', toolName: 'manage_ai', arguments: { action: 'get_ai_perception_data', actorName: 'Enemy_01' }, expected: 'success|not found' },
  
  // === State Tree ===
  { scenario: 'AI: create_state_tree', toolName: 'manage_ai', arguments: { action: 'create_state_tree', name: 'ST_Enemy', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: add_state_tree_state_idle', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'Idle' }, expected: 'success|not found' },
  { scenario: 'AI: add_state_tree_state_patrol', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'Patrol' }, expected: 'success|not found' },
  { scenario: 'AI: add_state_tree_state_combat', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'Combat' }, expected: 'success|not found' },
  { scenario: 'AI: add_state_tree_transition', toolName: 'manage_ai', arguments: { action: 'add_state_tree_transition', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, fromState: 'Idle', toState: 'Patrol', transitionCondition: 'TimeElapsed' }, expected: 'success|not found' },
  { scenario: 'AI: configure_state_tree_task', toolName: 'manage_ai', arguments: { action: 'configure_state_tree_task', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'Idle', stateTaskClass: 'WaitTask' }, expected: 'success|not found' },
  { scenario: 'AI: configure_state_tree_node', toolName: 'manage_ai', arguments: { action: 'configure_state_tree_node', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'Combat', nodeProperties: { priority: 10 } }, expected: 'success|not found' },
  { scenario: 'AI: bind_statetree', toolName: 'manage_ai', arguments: { action: 'bind_statetree', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, stateTreePath: `${TEST_FOLDER}/ST_Enemy` }, expected: 'success|not found' },
  { scenario: 'AI: get_statetree_state', toolName: 'manage_ai', arguments: { action: 'get_statetree_state', actorName: 'Enemy_01' }, expected: 'success|not found' },
  { scenario: 'AI: trigger_statetree_transition', toolName: 'manage_ai', arguments: { action: 'trigger_statetree_transition', actorName: 'Enemy_01', toState: 'Combat' }, expected: 'success|not found' },
  { scenario: 'AI: list_statetree_states', toolName: 'manage_ai', arguments: { action: 'list_statetree_states', stateTreePath: `${TEST_FOLDER}/ST_Enemy` }, expected: 'success|not found' },
  
  // === Smart Objects ===
  { scenario: 'AI: create_smart_object_definition', toolName: 'manage_ai', arguments: { action: 'create_smart_object_definition', name: 'SOD_Bench', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: add_smart_object_slot', toolName: 'manage_ai', arguments: { action: 'add_smart_object_slot', definitionPath: `${TEST_FOLDER}/SOD_Bench`, slotOffset: { x: 0, y: 0, z: 50 } }, expected: 'success|not found' },
  { scenario: 'AI: configure_slot_behavior', toolName: 'manage_ai', arguments: { action: 'configure_slot_behavior', definitionPath: `${TEST_FOLDER}/SOD_Bench`, slotIndex: 0, slotBehaviorDefinition: 'Sit', slotActivityTags: ['Activity.Rest'] }, expected: 'success|not found' },
  { scenario: 'AI: add_smart_object_component', toolName: 'manage_ai', arguments: { action: 'add_smart_object_component', blueprintPath: `${TEST_FOLDER}/BP_Bench`, definitionPath: `${TEST_FOLDER}/SOD_Bench` }, expected: 'success|not found' },
  { scenario: 'AI: create_smart_object', toolName: 'manage_ai', arguments: { action: 'create_smart_object', name: 'SO_ChairA', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AI: query_smart_objects', toolName: 'manage_ai', arguments: { action: 'query_smart_objects', actorName: 'NPC_01', activityTag: 'Activity.Rest', searchRadius: 500 }, expected: 'success|not found' },
  { scenario: 'AI: claim_smart_object', toolName: 'manage_ai', arguments: { action: 'claim_smart_object', actorName: 'NPC_01', smartObjectActor: 'Bench_01' }, expected: 'success|not found' },
  { scenario: 'AI: release_smart_object', toolName: 'manage_ai', arguments: { action: 'release_smart_object', actorName: 'NPC_01' }, expected: 'success|not found' },
  { scenario: 'AI: configure_smart_object', toolName: 'manage_ai', arguments: { action: 'configure_smart_object', smartObjectActor: 'Bench_01', slotEnabled: true }, expected: 'success|not found' },
  
  // === Mass Entity (Crowd) ===
  { scenario: 'AI: create_mass_entity_config', toolName: 'manage_ai', arguments: { action: 'create_mass_entity_config', name: 'MEC_Crowd', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AI: configure_mass_entity', toolName: 'manage_ai', arguments: { action: 'configure_mass_entity', configPath: `${TEST_FOLDER}/MEC_Crowd`, massTraits: ['MassMovement', 'MassVisualization'] }, expected: 'success|not found' },
  { scenario: 'AI: add_mass_spawner', toolName: 'manage_ai', arguments: { action: 'add_mass_spawner', blueprintPath: `${TEST_FOLDER}/BP_MassSpawner`, spawnerSettings: { entityCount: 100, spawnRadius: 1000 } }, expected: 'success|not found' },
  { scenario: 'AI: spawn_mass_entity', toolName: 'manage_ai', arguments: { action: 'spawn_mass_entity', configPath: `${TEST_FOLDER}/MEC_Crowd`, location: { x: 0, y: 0, z: 0 }, count: 50 }, expected: 'success|not found' },
  { scenario: 'AI: spawn_mass_ai_entities', toolName: 'manage_ai', arguments: { action: 'spawn_mass_ai_entities', configPath: `${TEST_FOLDER}/MEC_Crowd`, spawnLocation: { x: 0, y: 0, z: 0 }, count: 100 }, expected: 'success|not found' },
  { scenario: 'AI: destroy_mass_entity', toolName: 'manage_ai', arguments: { action: 'destroy_mass_entity', entityId: 'Entity_001' }, expected: 'success|not found' },
  { scenario: 'AI: query_mass_entities', toolName: 'manage_ai', arguments: { action: 'query_mass_entities', location: { x: 0, y: 0, z: 0 }, radius: 500 }, expected: 'success' },
  { scenario: 'AI: set_mass_entity_fragment', toolName: 'manage_ai', arguments: { action: 'set_mass_entity_fragment', entityId: 'Entity_001', fragmentType: 'Transform', value: { x: 100, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'AI: configure_mass_ai_fragment', toolName: 'manage_ai', arguments: { action: 'configure_mass_ai_fragment', configPath: `${TEST_FOLDER}/MEC_Crowd`, fragmentType: 'Movement', settings: { maxSpeed: 400 } }, expected: 'success|not found' },
  
  // === Navigation ===
  { scenario: 'AI: configure_nav_mesh_settings', toolName: 'manage_ai', arguments: { action: 'configure_nav_mesh_settings', agentRadius: 42, agentHeight: 192, cellSize: 10 }, expected: 'success' },
  { scenario: 'AI: set_nav_agent_properties', toolName: 'manage_ai', arguments: { action: 'set_nav_agent_properties', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, agentRadius: 42, agentHeight: 192, agentStepHeight: 45 }, expected: 'success|not found' },
  { scenario: 'AI: rebuild_navigation', toolName: 'manage_ai', arguments: { action: 'rebuild_navigation' }, expected: 'success' },
  { scenario: 'AI: create_nav_modifier_component', toolName: 'manage_ai', arguments: { action: 'create_nav_modifier_component', blueprintPath: `${TEST_FOLDER}/BP_NavBlocker` }, expected: 'success|not found' },
  { scenario: 'AI: set_nav_area_class', toolName: 'manage_ai', arguments: { action: 'set_nav_area_class', blueprintPath: `${TEST_FOLDER}/BP_NavBlocker`, areaClass: 'NavArea_Obstacle' }, expected: 'success|not found' },
  { scenario: 'AI: configure_nav_area_cost', toolName: 'manage_ai', arguments: { action: 'configure_nav_area_cost', areaClass: 'NavArea_Grass', areaCost: 2.0 }, expected: 'success' },
  { scenario: 'AI: create_nav_link_proxy', toolName: 'manage_ai', arguments: { action: 'create_nav_link_proxy', actorName: 'NavLink_Jump', startPoint: { x: 0, y: 0, z: 0 }, endPoint: { x: 300, y: 0, z: 100 } }, expected: 'success' },
  { scenario: 'AI: configure_nav_link', toolName: 'manage_ai', arguments: { action: 'configure_nav_link', linkName: 'NavLink_Jump', direction: 'BothWays' }, expected: 'success|not found' },
  { scenario: 'AI: set_nav_link_type', toolName: 'manage_ai', arguments: { action: 'set_nav_link_type', linkName: 'NavLink_Jump', linkType: 'smart' }, expected: 'success|not found' },
  { scenario: 'AI: create_smart_link', toolName: 'manage_ai', arguments: { action: 'create_smart_link', actorName: 'SmartLink_Ladder', startPoint: { x: 0, y: 0, z: 0 }, endPoint: { x: 0, y: 0, z: 300 } }, expected: 'success' },
  { scenario: 'AI: configure_smart_link_behavior', toolName: 'manage_ai', arguments: { action: 'configure_smart_link_behavior', linkName: 'SmartLink_Ladder', behaviorDefinition: 'Climb' }, expected: 'success|not found' },
  { scenario: 'AI: get_navigation_info', toolName: 'manage_ai', arguments: { action: 'get_navigation_info' }, expected: 'success' },
  
  // === AI NPC Plugins (ConvAI, Inworld, Audio2Face) ===
  { scenario: 'AI: create_convai_character', toolName: 'manage_ai', arguments: { action: 'create_convai_character', name: 'BP_ConvaiNPC', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AI: configure_character_backstory', toolName: 'manage_ai', arguments: { action: 'configure_character_backstory', blueprintPath: `${TEST_FOLDER}/BP_ConvaiNPC`, backstory: 'A friendly tavern keeper.' }, expected: 'success|not found' },
  { scenario: 'AI: configure_character_voice', toolName: 'manage_ai', arguments: { action: 'configure_character_voice', blueprintPath: `${TEST_FOLDER}/BP_ConvaiNPC`, voiceId: 'en-US-male-1' }, expected: 'success|not found' },
  { scenario: 'AI: get_convai_info', toolName: 'manage_ai', arguments: { action: 'get_convai_info' }, expected: 'success' },
  { scenario: 'AI: create_inworld_character', toolName: 'manage_ai', arguments: { action: 'create_inworld_character', name: 'BP_InworldNPC', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'AI: get_inworld_info', toolName: 'manage_ai', arguments: { action: 'get_inworld_info' }, expected: 'success' },
  { scenario: 'AI: configure_audio2face', toolName: 'manage_ai', arguments: { action: 'configure_audio2face', blueprintPath: `${TEST_FOLDER}/BP_NPC`, enabled: true }, expected: 'success|not found' },
  { scenario: 'AI: get_audio2face_status', toolName: 'manage_ai', arguments: { action: 'get_audio2face_status' }, expected: 'success' },
  { scenario: 'AI: get_ai_npc_info', toolName: 'manage_ai', arguments: { action: 'get_ai_npc_info' }, expected: 'success' },
  { scenario: 'AI: list_available_ai_backends', toolName: 'manage_ai', arguments: { action: 'list_available_ai_backends' }, expected: 'success' },
  
  // === AI Assistant ===
  { scenario: 'AI: ai_assistant_query', toolName: 'manage_ai', arguments: { action: 'ai_assistant_query', query: 'How do I create a patrol behavior?' }, expected: 'success' },
  { scenario: 'AI: ai_assistant_explain_feature', toolName: 'manage_ai', arguments: { action: 'ai_assistant_explain_feature', feature: 'EQS' }, expected: 'success' },
  { scenario: 'AI: ai_assistant_suggest_fix', toolName: 'manage_ai', arguments: { action: 'ai_assistant_suggest_fix', issue: 'AI not moving to target' }, expected: 'success' },
  
  // === Extended AI (Expanded Coverage) ===
  // More Blackboard Keys
  { scenario: 'AI: add_blackboard_key_rotator', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'LastKnownDirection', keyType: 'Rotator' }, expected: 'success|not found' },
  { scenario: 'AI: add_blackboard_key_name', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'CurrentState', keyType: 'Name' }, expected: 'success|not found' },
  { scenario: 'AI: add_blackboard_key_class', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'TargetClass', keyType: 'Class' }, expected: 'success|not found' },
  { scenario: 'AI: remove_blackboard_key', toolName: 'manage_ai', arguments: { action: 'remove_blackboard_key', blackboardPath: `${TEST_FOLDER}/BB_Enemy`, keyName: 'OldKey' }, expected: 'success|not found' },
  { scenario: 'AI: set_blackboard_value_vector', toolName: 'manage_ai', arguments: { action: 'set_blackboard_value', actorName: 'Enemy_01', keyName: 'PatrolLocation', value: { x: 100, y: 200, z: 0 } }, expected: 'success|not found' },
  { scenario: 'AI: set_blackboard_value_float', toolName: 'manage_ai', arguments: { action: 'set_blackboard_value', actorName: 'Enemy_01', keyName: 'DistanceToTarget', value: 500.0 }, expected: 'success|not found' },
  { scenario: 'AI: set_blackboard_value_bool', toolName: 'manage_ai', arguments: { action: 'set_blackboard_value', actorName: 'Enemy_01', keyName: 'IsAggro', value: true }, expected: 'success|not found' },
  { scenario: 'AI: get_blackboard_value', toolName: 'manage_ai', arguments: { action: 'get_blackboard_value', actorName: 'Enemy_01', keyName: 'TargetActor' }, expected: 'success|not found' },
  { scenario: 'AI: clear_blackboard_value', toolName: 'manage_ai', arguments: { action: 'clear_blackboard_value', actorName: 'Enemy_01', keyName: 'TargetActor' }, expected: 'success|not found' },
  
  // More BT Tasks
  { scenario: 'AI: add_task_rotatetoface', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'RotateToFaceBBEntry', parentNodeId: 'AttackSequence' }, expected: 'success|not found' },
  { scenario: 'AI: add_task_makenoises', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'MakeNoise', parentNodeId: 'PatrolSequence' }, expected: 'success|not found' },
  { scenario: 'AI: add_task_finishwithresult', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'FinishWithResult', parentNodeId: 'FailSequence', nodeProperties: { result: 'Failed' } }, expected: 'success|not found' },
  { scenario: 'AI: add_task_pushevent', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, taskType: 'PushPawnEventOnBlackboard', parentNodeId: 'EventSequence' }, expected: 'success|not found' },
  
  // More BT Decorators
  { scenario: 'AI: add_decorator_timelimit', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'SearchSequence', decoratorType: 'TimeLimit', nodeProperties: { limitTime: 10.0 } }, expected: 'success|not found' },
  { scenario: 'AI: add_decorator_forceresult', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'OptionalSequence', decoratorType: 'ForceSuccess' }, expected: 'success|not found' },
  { scenario: 'AI: add_decorator_cone', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'AttackSequence', decoratorType: 'ConeCheck', nodeProperties: { coneHalfAngle: 45 } }, expected: 'success|not found' },
  { scenario: 'AI: add_decorator_compare', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: `${TEST_FOLDER}/BT_Enemy`, nodeId: 'ChaseSequence', decoratorType: 'CompareBBEntries', nodeProperties: { keyA: 'DistanceToTarget', keyB: 'AttackRange', operator: 'Less' } }, expected: 'success|not found' },
  
  // More EQS Operations
  { scenario: 'AI: add_eqs_generator_composite', toolName: 'manage_ai', arguments: { action: 'add_eqs_generator', queryPath: `${TEST_FOLDER}/EQS_FindCover`, generatorType: 'Composite' }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_generator_onecircle', toolName: 'manage_ai', arguments: { action: 'add_eqs_generator', queryPath: `${TEST_FOLDER}/EQS_FindCover`, generatorType: 'OnCircle', generatorSettings: { circleRadius: 500 } }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_test_dot', toolName: 'manage_ai', arguments: { action: 'add_eqs_test', queryPath: `${TEST_FOLDER}/EQS_FindCover`, testType: 'Dot', testSettings: { lineA: 'QuerierToItem', lineB: 'QuerierToContext' } }, expected: 'success|not found' },
  { scenario: 'AI: add_eqs_test_gameplaytags', toolName: 'manage_ai', arguments: { action: 'add_eqs_test', queryPath: `${TEST_FOLDER}/EQS_FindTarget`, testType: 'GameplayTags', testSettings: { tagQuery: 'Character.Enemy' } }, expected: 'success|not found' },
  { scenario: 'AI: remove_eqs_test', toolName: 'manage_ai', arguments: { action: 'remove_eqs_test', queryPath: `${TEST_FOLDER}/EQS_FindCover`, testIndex: 0 }, expected: 'success|not found' },
  { scenario: 'AI: run_eqs_query', toolName: 'manage_ai', arguments: { action: 'run_eqs_query', actorName: 'Enemy_01', queryPath: `${TEST_FOLDER}/EQS_FindCover` }, expected: 'success|not found' },
  
  // More Perception Operations
  { scenario: 'AI: add_ai_perception_sense_touch', toolName: 'manage_ai', arguments: { action: 'add_ai_perception_sense', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, senseType: 'Touch' }, expected: 'success|not found' },
  { scenario: 'AI: add_ai_perception_sense_team', toolName: 'manage_ai', arguments: { action: 'add_ai_perception_sense', controllerPath: `${TEST_FOLDER}/AIC_Enemy`, senseType: 'Team' }, expected: 'success|not found' },
  { scenario: 'AI: set_auto_possess_ai', toolName: 'manage_ai', arguments: { action: 'set_auto_possess_ai', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, autoRepossess: 'Spawned' }, expected: 'success|not found' },
  { scenario: 'AI: forget_actor', toolName: 'manage_ai', arguments: { action: 'forget_actor', controllerName: 'AIC_Enemy_01', targetActor: 'Player' }, expected: 'success|not found' },
  { scenario: 'AI: request_sense_update', toolName: 'manage_ai', arguments: { action: 'request_sense_update', controllerName: 'AIC_Enemy_01', senseType: 'Sight' }, expected: 'success|not found' },
  
  // More State Tree Operations
  { scenario: 'AI: add_state_tree_state_flee', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'Flee' }, expected: 'success|not found' },
  { scenario: 'AI: add_state_tree_state_search', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'Search' }, expected: 'success|not found' },
  { scenario: 'AI: add_state_tree_state_dead', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'Dead' }, expected: 'success|not found' },
  { scenario: 'AI: add_state_tree_transition_combat_flee', toolName: 'manage_ai', arguments: { action: 'add_state_tree_transition', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, fromState: 'Combat', toState: 'Flee', transitionCondition: 'LowHealth' }, expected: 'success|not found' },
  { scenario: 'AI: add_state_tree_transition_patrol_combat', toolName: 'manage_ai', arguments: { action: 'add_state_tree_transition', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, fromState: 'Patrol', toState: 'Combat', transitionCondition: 'TargetSeen' }, expected: 'success|not found' },
  { scenario: 'AI: remove_state_tree_state', toolName: 'manage_ai', arguments: { action: 'remove_state_tree_state', stateTreePath: `${TEST_FOLDER}/ST_Enemy`, stateName: 'OldState' }, expected: 'success|not found' },
  
  // Smart Objects
  { scenario: 'AI: create_smart_object_definition', toolName: 'manage_ai', arguments: { action: 'create_smart_object_definition', name: 'SOD_Chair', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: add_smart_object_slot', toolName: 'manage_ai', arguments: { action: 'add_smart_object_slot', definitionPath: `${TEST_FOLDER}/SOD_Chair`, slotName: 'Sit', slotType: 'Interaction' }, expected: 'success|not found' },
  { scenario: 'AI: configure_slot_behavior', toolName: 'manage_ai', arguments: { action: 'configure_slot_behavior', definitionPath: `${TEST_FOLDER}/SOD_Chair`, slotName: 'Sit', behavior: 'SitDown' }, expected: 'success|not found' },
  { scenario: 'AI: add_smart_object_component', toolName: 'manage_ai', arguments: { action: 'add_smart_object_component', blueprintPath: `${TEST_FOLDER}/BP_Chair`, definitionPath: `${TEST_FOLDER}/SOD_Chair` }, expected: 'success|not found' },
  { scenario: 'AI: create_smart_object', toolName: 'manage_ai', arguments: { action: 'create_smart_object', name: 'SO_Bench', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: query_smart_objects', toolName: 'manage_ai', arguments: { action: 'query_smart_objects', actorName: 'Enemy_01', radius: 1000, slotType: 'Interaction' }, expected: 'success|not found' },
  { scenario: 'AI: claim_smart_object', toolName: 'manage_ai', arguments: { action: 'claim_smart_object', actorName: 'Enemy_01', smartObjectName: 'SO_Bench', slotName: 'Sit' }, expected: 'success|not found' },
  { scenario: 'AI: release_smart_object', toolName: 'manage_ai', arguments: { action: 'release_smart_object', actorName: 'Enemy_01' }, expected: 'success|not found' },
  { scenario: 'AI: configure_smart_object', toolName: 'manage_ai', arguments: { action: 'configure_smart_object', smartObjectName: 'SO_Bench', enabled: true, priority: 5 }, expected: 'success|not found' },
  
  // Mass Entity Operations
  { scenario: 'AI: create_mass_entity_config', toolName: 'manage_ai', arguments: { action: 'create_mass_entity_config', name: 'MEC_Crowd', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: configure_mass_entity', toolName: 'manage_ai', arguments: { action: 'configure_mass_entity', configPath: `${TEST_FOLDER}/MEC_Crowd`, entityTraits: ['Movable', 'Simulated'] }, expected: 'success|not found' },
  { scenario: 'AI: add_mass_spawner', toolName: 'manage_ai', arguments: { action: 'add_mass_spawner', spawnerName: 'CrowdSpawner', configPath: `${TEST_FOLDER}/MEC_Crowd`, spawnCount: 100 }, expected: 'success|not found' },
  { scenario: 'AI: spawn_mass_entity', toolName: 'manage_ai', arguments: { action: 'spawn_mass_entity', spawnerName: 'CrowdSpawner' }, expected: 'success|not found' },
  { scenario: 'AI: destroy_mass_entity', toolName: 'manage_ai', arguments: { action: 'destroy_mass_entity', entityId: 'Entity_001' }, expected: 'success|not found' },
  { scenario: 'AI: query_mass_entities', toolName: 'manage_ai', arguments: { action: 'query_mass_entities', location: { x: 0, y: 0, z: 0 }, radius: 1000 }, expected: 'success' },
  { scenario: 'AI: set_mass_entity_fragment', toolName: 'manage_ai', arguments: { action: 'set_mass_entity_fragment', entityId: 'Entity_001', fragmentName: 'Transform', fragmentValue: { x: 100, y: 200, z: 0 } }, expected: 'success|not found' },
  { scenario: 'AI: configure_mass_ai_fragment', toolName: 'manage_ai', arguments: { action: 'configure_mass_ai_fragment', configPath: `${TEST_FOLDER}/MEC_Crowd`, behaviorConfig: { avoidance: true, groupBehavior: true } }, expected: 'success|not found' },
  { scenario: 'AI: spawn_mass_ai_entities', toolName: 'manage_ai', arguments: { action: 'spawn_mass_ai_entities', configPath: `${TEST_FOLDER}/MEC_Crowd`, count: 50, location: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
  
  // AI NPC Plugins
  { scenario: 'AI: get_ai_npc_info', toolName: 'manage_ai', arguments: { action: 'get_ai_npc_info' }, expected: 'success' },
  { scenario: 'AI: configure_ace_emotions', toolName: 'manage_ai', arguments: { action: 'configure_ace_emotions', actorName: 'NPC_01', emotionSettings: { baseEmotion: 'Neutral', emotionIntensity: 0.5 } }, expected: 'success|not found|not installed' },
  { scenario: 'AI: get_ace_info', toolName: 'manage_ai', arguments: { action: 'get_ace_info' }, expected: 'success|not installed' },
];

// ============================================================================
// MANAGE_NETWORKING (73 actions) - FULL COVERAGE
// ============================================================================
const manageNetworkingTests = [
  // === Replication ===
  { scenario: 'Net: enable_replication', toolName: 'manage_networking', arguments: { action: 'enable_replication', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Net: set_replicate_movement', toolName: 'manage_networking', arguments: { action: 'set_replicate_movement', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, replicate: true }, expected: 'success|not found' },
  { scenario: 'Net: replicate_variable', toolName: 'manage_networking', arguments: { action: 'replicate_variable', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, variableName: 'Health', condition: 'COND_OwnerOnly' }, expected: 'success|not found' },
  { scenario: 'Net: set_net_update_frequency', toolName: 'manage_networking', arguments: { action: 'set_net_update_frequency', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, frequency: 100 }, expected: 'success|not found' },
  { scenario: 'Net: set_min_net_update_frequency', toolName: 'manage_networking', arguments: { action: 'set_min_net_update_frequency', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, frequency: 2 }, expected: 'success|not found' },
  { scenario: 'Net: set_net_priority', toolName: 'manage_networking', arguments: { action: 'set_net_priority', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, priority: 3.0 }, expected: 'success|not found' },
  
  // === RPCs ===
  { scenario: 'Net: add_server_rpc', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'ServerFireWeapon', rpcType: 'Server', reliable: true }, expected: 'success|not found' },
  { scenario: 'Net: add_client_rpc', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'ClientPlayHitReaction', rpcType: 'Client', reliable: false }, expected: 'success|not found' },
  { scenario: 'Net: add_multicast_rpc', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'MulticastPlayDeathAnim', rpcType: 'NetMulticast', reliable: true }, expected: 'success|not found' },
  
  // === Game Mode ===
  { scenario: 'Net: create_game_mode', toolName: 'manage_networking', arguments: { action: 'create_game_mode', name: 'GM_Deathmatch', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Net: set_default_pawn', toolName: 'manage_networking', arguments: { action: 'set_default_pawn', gameModePath: `${TEST_FOLDER}/GM_Deathmatch`, pawnClass: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Net: set_player_controller', toolName: 'manage_networking', arguments: { action: 'set_player_controller', gameModePath: `${TEST_FOLDER}/GM_Deathmatch`, controllerClass: '/Script/Engine.PlayerController' }, expected: 'success|not found' },
  { scenario: 'Net: set_hud_class', toolName: 'manage_networking', arguments: { action: 'set_hud_class', gameModePath: `${TEST_FOLDER}/GM_Deathmatch`, hudClass: `${TEST_FOLDER}/WBP_HUD` }, expected: 'success|not found' },
  
  // === Game State ===
  { scenario: 'Net: create_game_state', toolName: 'manage_networking', arguments: { action: 'create_game_state', name: 'GS_Deathmatch', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Net: add_game_state_variable', toolName: 'manage_networking', arguments: { action: 'add_game_state_variable', gameStatePath: `${TEST_FOLDER}/GS_Deathmatch`, variableName: 'MatchTimeRemaining', variableType: 'Float', replicated: true }, expected: 'success|not found' },
  
  // === Player State ===
  { scenario: 'Net: create_player_state', toolName: 'manage_networking', arguments: { action: 'create_player_state', name: 'PS_Player', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Net: add_player_state_variable', toolName: 'manage_networking', arguments: { action: 'add_player_state_variable', playerStatePath: `${TEST_FOLDER}/PS_Player`, variableName: 'Score', variableType: 'Int32', replicated: true }, expected: 'success|not found' },
  
  // === Sessions ===
  { scenario: 'Net: create_session', toolName: 'manage_networking', arguments: { action: 'create_session', sessionName: 'TestSession', maxPlayers: 8, isLan: true }, expected: 'success' },
  { scenario: 'Net: find_sessions', toolName: 'manage_networking', arguments: { action: 'find_sessions', isLan: true }, expected: 'success' },
  { scenario: 'Net: join_session', toolName: 'manage_networking', arguments: { action: 'join_session', sessionName: 'TestSession' }, expected: 'success|not found' },
  { scenario: 'Net: destroy_session', toolName: 'manage_networking', arguments: { action: 'destroy_session', sessionName: 'TestSession' }, expected: 'success|not found' },
  
  // === Network Simulation ===
  { scenario: 'Net: simulate_lag', toolName: 'manage_networking', arguments: { action: 'simulate_lag', minLatency: 50, maxLatency: 100 }, expected: 'success' },
  { scenario: 'Net: simulate_packet_loss', toolName: 'manage_networking', arguments: { action: 'simulate_packet_loss', inPercent: 5, outPercent: 5 }, expected: 'success' },
  { scenario: 'Net: disable_net_simulation', toolName: 'manage_networking', arguments: { action: 'disable_net_simulation' }, expected: 'success' },
  
  // === Prediction ===
  { scenario: 'Net: enable_client_prediction', toolName: 'manage_networking', arguments: { action: 'enable_client_prediction', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar` }, expected: 'success|not found' },
  { scenario: 'Net: configure_correction', toolName: 'manage_networking', arguments: { action: 'configure_correction', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, correctionThreshold: 1.0, smoothingTime: 0.1 }, expected: 'success|not found' },
  
  // === Extended Networking (Expanded Coverage) ===
  // More Replication Settings
  { scenario: 'Net: replicate_variable_health', toolName: 'manage_networking', arguments: { action: 'replicate_variable', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, variableName: 'MaxHealth', condition: 'COND_InitialOnly' }, expected: 'success|not found' },
  { scenario: 'Net: replicate_variable_ammo', toolName: 'manage_networking', arguments: { action: 'replicate_variable', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, variableName: 'AmmoCount', condition: 'COND_SimulatedOnly' }, expected: 'success|not found' },
  { scenario: 'Net: replicate_variable_score', toolName: 'manage_networking', arguments: { action: 'replicate_variable', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, variableName: 'Score', condition: 'COND_AutonomousOnly' }, expected: 'success|not found' },
  { scenario: 'Net: set_replicate_component', toolName: 'manage_networking', arguments: { action: 'set_replicate_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, componentName: 'InventoryComponent', replicate: true }, expected: 'success|not found' },
  { scenario: 'Net: configure_net_relevancy', toolName: 'manage_networking', arguments: { action: 'configure_net_relevancy', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, alwaysRelevant: false, onlyRelevantToOwner: false, relevancyDistance: 5000 }, expected: 'success|not found' },
  { scenario: 'Net: configure_dormancy', toolName: 'manage_networking', arguments: { action: 'configure_dormancy', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, initialDormancy: 'DORM_DormantAll', netDormancy: 'DORM_Awake' }, expected: 'success|not found' },
  
  // More RPC Types
  { scenario: 'Net: add_rpc_unreliable', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'ServerUpdatePosition', rpcType: 'Server', reliable: false }, expected: 'success|not found' },
  { scenario: 'Net: add_rpc_validation', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'ServerSpendCurrency', rpcType: 'Server', reliable: true, withValidation: true }, expected: 'success|not found' },
  
  // More Game Mode Settings
  { scenario: 'Net: set_game_state_class', toolName: 'manage_networking', arguments: { action: 'set_game_state_class', gameModePath: `${TEST_FOLDER}/GM_Deathmatch`, gameStateClass: `${TEST_FOLDER}/GS_Deathmatch` }, expected: 'success|not found' },
  { scenario: 'Net: set_player_state_class', toolName: 'manage_networking', arguments: { action: 'set_player_state_class', gameModePath: `${TEST_FOLDER}/GM_Deathmatch`, playerStateClass: `${TEST_FOLDER}/PS_Player` }, expected: 'success|not found' },
  { scenario: 'Net: set_spectator_class', toolName: 'manage_networking', arguments: { action: 'set_spectator_class', gameModePath: `${TEST_FOLDER}/GM_Deathmatch`, spectatorClass: '/Script/Engine.SpectatorPawn' }, expected: 'success|not found' },
  { scenario: 'Net: configure_match_settings', toolName: 'manage_networking', arguments: { action: 'configure_match_settings', gameModePath: `${TEST_FOLDER}/GM_Deathmatch`, matchDuration: 600, warmupDuration: 30 }, expected: 'success|not found' },
  
  // More Session Operations
  { scenario: 'Net: update_session', toolName: 'manage_networking', arguments: { action: 'update_session', sessionName: 'TestSession', maxPlayers: 16 }, expected: 'success|not found' },
  { scenario: 'Net: start_session', toolName: 'manage_networking', arguments: { action: 'start_session', sessionName: 'TestSession' }, expected: 'success|not found' },
  { scenario: 'Net: end_session', toolName: 'manage_networking', arguments: { action: 'end_session', sessionName: 'TestSession' }, expected: 'success|not found' },
  { scenario: 'Net: get_session_state', toolName: 'manage_networking', arguments: { action: 'get_session_state', sessionName: 'TestSession' }, expected: 'success|not found' },
  { scenario: 'Net: get_session_players', toolName: 'manage_networking', arguments: { action: 'get_session_players', sessionName: 'TestSession' }, expected: 'success|not found' },
  { scenario: 'Net: kick_player', toolName: 'manage_networking', arguments: { action: 'kick_player', playerId: 'Player_01', reason: 'AFK' }, expected: 'success|not found' },
  
  // Team Settings
  { scenario: 'Net: configure_team_settings', toolName: 'manage_networking', arguments: { action: 'configure_team_settings', gameModePath: `${TEST_FOLDER}/GM_Deathmatch`, numTeams: 2, teamSize: 5 }, expected: 'success|not found' },
  { scenario: 'Net: assign_team', toolName: 'manage_networking', arguments: { action: 'assign_team', playerId: 'Player_01', teamId: 1 }, expected: 'success|not found' },
  { scenario: 'Net: get_team_members', toolName: 'manage_networking', arguments: { action: 'get_team_members', teamId: 1 }, expected: 'success|not found' },
  
  // Network Info
  { scenario: 'Net: get_networking_info', toolName: 'manage_networking', arguments: { action: 'get_networking_info' }, expected: 'success' },
  { scenario: 'Net: get_rpc_statistics', toolName: 'manage_networking', arguments: { action: 'get_rpc_statistics' }, expected: 'success' },
  { scenario: 'Net: get_net_role_info', toolName: 'manage_networking', arguments: { action: 'get_net_role_info', actorName: 'Player' }, expected: 'success|not found' },
  
  // Prediction Settings
  { scenario: 'Net: configure_prediction_settings', toolName: 'manage_networking', arguments: { action: 'configure_prediction_settings', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, maxPredictionPing: 150, smoothNetUpdateEnabled: true }, expected: 'success|not found' },
  
  // === EXTENDED MANAGE_NETWORKING (Additional ~80 tests for comprehensive coverage) ===
  // More Replication Configurations
  { scenario: 'Net: enable_replication_enemy', toolName: 'manage_networking', arguments: { action: 'enable_replication', blueprintPath: `${TEST_FOLDER}/BP_Enemy` }, expected: 'success|not found' },
  { scenario: 'Net: enable_replication_projectile', toolName: 'manage_networking', arguments: { action: 'enable_replication', blueprintPath: `${TEST_FOLDER}/BP_Projectile` }, expected: 'success|not found' },
  { scenario: 'Net: set_replicate_movement_vehicle', toolName: 'manage_networking', arguments: { action: 'set_replicate_movement', blueprintPath: `${TEST_FOLDER}/BP_Vehicle`, replicate: true }, expected: 'success|not found' },
  { scenario: 'Net: replicate_variable_position', toolName: 'manage_networking', arguments: { action: 'replicate_variable', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, variableName: 'Position', condition: 'COND_None' }, expected: 'success|not found' },
  { scenario: 'Net: replicate_variable_velocity', toolName: 'manage_networking', arguments: { action: 'replicate_variable', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, variableName: 'Velocity', condition: 'COND_SimulatedOrPhysics' }, expected: 'success|not found' },
  { scenario: 'Net: set_net_update_frequency_high', toolName: 'manage_networking', arguments: { action: 'set_net_update_frequency', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, frequency: 200 }, expected: 'success|not found' },
  { scenario: 'Net: set_net_update_frequency_low', toolName: 'manage_networking', arguments: { action: 'set_net_update_frequency', blueprintPath: `${TEST_FOLDER}/BP_Pickup`, frequency: 10 }, expected: 'success|not found' },
  { scenario: 'Net: set_min_net_update_frequency_enemy', toolName: 'manage_networking', arguments: { action: 'set_min_net_update_frequency', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, frequency: 5 }, expected: 'success|not found' },
  { scenario: 'Net: set_net_priority_player', toolName: 'manage_networking', arguments: { action: 'set_net_priority', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, priority: 5.0 }, expected: 'success|not found' },
  { scenario: 'Net: set_net_priority_enemy', toolName: 'manage_networking', arguments: { action: 'set_net_priority', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, priority: 2.0 }, expected: 'success|not found' },
  
  // More RPC Configurations
  { scenario: 'Net: add_rpc_server_damage', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'ServerApplyDamage', rpcType: 'Server', reliable: true }, expected: 'success|not found' },
  { scenario: 'Net: add_rpc_client_respawn', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'ClientRespawn', rpcType: 'Client', reliable: true }, expected: 'success|not found' },
  { scenario: 'Net: add_rpc_multicast_effect', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'MulticastPlayEffect', rpcType: 'NetMulticast', reliable: false }, expected: 'success|not found' },
  { scenario: 'Net: add_rpc_server_useitem', toolName: 'manage_networking', arguments: { action: 'add_rpc', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, functionName: 'ServerUseItem', rpcType: 'Server', reliable: true, withValidation: true }, expected: 'success|not found' },
  
  // More Game Mode Configurations
  { scenario: 'Net: create_game_mode_ctf', toolName: 'manage_networking', arguments: { action: 'create_game_mode', name: 'GM_CTF', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Net: create_game_mode_tdm', toolName: 'manage_networking', arguments: { action: 'create_game_mode', name: 'GM_TeamDeathmatch', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Net: set_default_pawn_ctf', toolName: 'manage_networking', arguments: { action: 'set_default_pawn', gameModePath: `${TEST_FOLDER}/GM_CTF`, pawnClass: `${TEST_FOLDER}/BP_CTFPlayer` }, expected: 'success|not found' },
  { scenario: 'Net: set_player_controller_ctf', toolName: 'manage_networking', arguments: { action: 'set_player_controller', gameModePath: `${TEST_FOLDER}/GM_CTF`, controllerClass: `${TEST_FOLDER}/PC_CTF` }, expected: 'success|not found' },
  { scenario: 'Net: set_hud_class_ctf', toolName: 'manage_networking', arguments: { action: 'set_hud_class', gameModePath: `${TEST_FOLDER}/GM_CTF`, hudClass: `${TEST_FOLDER}/WBP_CTFHUD` }, expected: 'success|not found' },
  { scenario: 'Net: set_game_state_class_ctf', toolName: 'manage_networking', arguments: { action: 'set_game_state_class', gameModePath: `${TEST_FOLDER}/GM_CTF`, gameStateClass: `${TEST_FOLDER}/GS_CTF` }, expected: 'success|not found' },
  { scenario: 'Net: configure_match_settings_ctf', toolName: 'manage_networking', arguments: { action: 'configure_match_settings', gameModePath: `${TEST_FOLDER}/GM_CTF`, matchDuration: 900, warmupDuration: 60 }, expected: 'success|not found' },
  
  // More Game/Player State Configurations
  { scenario: 'Net: create_game_state_ctf', toolName: 'manage_networking', arguments: { action: 'create_game_state', name: 'GS_CTF', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Net: add_game_state_variable_flags', toolName: 'manage_networking', arguments: { action: 'add_game_state_variable', gameStatePath: `${TEST_FOLDER}/GS_CTF`, variableName: 'TeamFlags', variableType: 'Array', replicated: true }, expected: 'success|not found' },
  { scenario: 'Net: add_game_state_variable_scores', toolName: 'manage_networking', arguments: { action: 'add_game_state_variable', gameStatePath: `${TEST_FOLDER}/GS_CTF`, variableName: 'TeamScores', variableType: 'Array', replicated: true }, expected: 'success|not found' },
  { scenario: 'Net: create_player_state_ctf', toolName: 'manage_networking', arguments: { action: 'create_player_state', name: 'PS_CTFPlayer', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Net: add_player_state_variable_captures', toolName: 'manage_networking', arguments: { action: 'add_player_state_variable', playerStatePath: `${TEST_FOLDER}/PS_CTFPlayer`, variableName: 'FlagCaptures', variableType: 'Int32', replicated: true }, expected: 'success|not found' },
  { scenario: 'Net: add_player_state_variable_returns', toolName: 'manage_networking', arguments: { action: 'add_player_state_variable', playerStatePath: `${TEST_FOLDER}/PS_CTFPlayer`, variableName: 'FlagReturns', variableType: 'Int32', replicated: true }, expected: 'success|not found' },
  
  // More Session Configurations
  { scenario: 'Net: create_session_dedicated', toolName: 'manage_networking', arguments: { action: 'create_session', sessionName: 'DedicatedServer', maxPlayers: 32, isLan: false }, expected: 'success' },
  { scenario: 'Net: create_session_private', toolName: 'manage_networking', arguments: { action: 'create_session', sessionName: 'PrivateMatch', maxPlayers: 4, isLan: true }, expected: 'success' },
  { scenario: 'Net: find_sessions_online', toolName: 'manage_networking', arguments: { action: 'find_sessions', isLan: false }, expected: 'success' },
  { scenario: 'Net: join_session_quick', toolName: 'manage_networking', arguments: { action: 'join_session', sessionName: 'QuickMatch' }, expected: 'success|not found' },
  { scenario: 'Net: update_session_map', toolName: 'manage_networking', arguments: { action: 'update_session', sessionName: 'DedicatedServer', maxPlayers: 64 }, expected: 'success|not found' },
  
  // More Network Simulation
  { scenario: 'Net: simulate_lag_high', toolName: 'manage_networking', arguments: { action: 'simulate_lag', minLatency: 200, maxLatency: 300 }, expected: 'success' },
  { scenario: 'Net: simulate_packet_loss_high', toolName: 'manage_networking', arguments: { action: 'simulate_packet_loss', inPercent: 15, outPercent: 15 }, expected: 'success' },
  { scenario: 'Net: simulate_lag_low', toolName: 'manage_networking', arguments: { action: 'simulate_lag', minLatency: 10, maxLatency: 20 }, expected: 'success' },
  
  // More Prediction Configurations
  { scenario: 'Net: enable_client_prediction_enemy', toolName: 'manage_networking', arguments: { action: 'enable_client_prediction', blueprintPath: `${TEST_FOLDER}/BP_Enemy` }, expected: 'success|not found' },
  { scenario: 'Net: configure_correction_smooth', toolName: 'manage_networking', arguments: { action: 'configure_correction', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, correctionThreshold: 0.5, smoothingTime: 0.2 }, expected: 'success|not found' },
  { scenario: 'Net: configure_correction_fast', toolName: 'manage_networking', arguments: { action: 'configure_correction', blueprintPath: `${TEST_FOLDER}/BP_Projectile`, correctionThreshold: 5.0, smoothingTime: 0.05 }, expected: 'success|not found' },
  
  // More Relevancy & Dormancy
  { scenario: 'Net: configure_net_relevancy_pickup', toolName: 'manage_networking', arguments: { action: 'configure_net_relevancy', blueprintPath: `${TEST_FOLDER}/BP_Pickup`, alwaysRelevant: false, onlyRelevantToOwner: false, relevancyDistance: 2000 }, expected: 'success|not found' },
  { scenario: 'Net: configure_net_relevancy_important', toolName: 'manage_networking', arguments: { action: 'configure_net_relevancy', blueprintPath: `${TEST_FOLDER}/BP_Objective`, alwaysRelevant: true, onlyRelevantToOwner: false }, expected: 'success|not found' },
  { scenario: 'Net: configure_dormancy_static', toolName: 'manage_networking', arguments: { action: 'configure_dormancy', blueprintPath: `${TEST_FOLDER}/BP_Building`, initialDormancy: 'DORM_DormantAll', netDormancy: 'DORM_DormantAll' }, expected: 'success|not found' },
  { scenario: 'Net: configure_dormancy_dynamic', toolName: 'manage_networking', arguments: { action: 'configure_dormancy', blueprintPath: `${TEST_FOLDER}/BP_Door`, initialDormancy: 'DORM_DormantAll', netDormancy: 'DORM_Awake' }, expected: 'success|not found' },
  
  // More Component Replication
  { scenario: 'Net: set_replicate_component_weapon', toolName: 'manage_networking', arguments: { action: 'set_replicate_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, componentName: 'WeaponComponent', replicate: true }, expected: 'success|not found' },
  { scenario: 'Net: set_replicate_component_health', toolName: 'manage_networking', arguments: { action: 'set_replicate_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, componentName: 'HealthComponent', replicate: true }, expected: 'success|not found' },
  { scenario: 'Net: set_replicate_component_ability', toolName: 'manage_networking', arguments: { action: 'set_replicate_component', blueprintPath: `${TEST_FOLDER}/BP_PlayerChar`, componentName: 'AbilitySystemComponent', replicate: true }, expected: 'success|not found' },
  
  // More Team Settings
  { scenario: 'Net: configure_team_settings_4teams', toolName: 'manage_networking', arguments: { action: 'configure_team_settings', gameModePath: `${TEST_FOLDER}/GM_FreeForAll`, numTeams: 0, teamSize: 1 }, expected: 'success|not found' },
  { scenario: 'Net: assign_team_2', toolName: 'manage_networking', arguments: { action: 'assign_team', playerId: 'Player_02', teamId: 2 }, expected: 'success|not found' },
  { scenario: 'Net: get_team_members_2', toolName: 'manage_networking', arguments: { action: 'get_team_members', teamId: 2 }, expected: 'success|not found' },
  
  // More Network Info
  { scenario: 'Net: get_networking_info_detailed', toolName: 'manage_networking', arguments: { action: 'get_networking_info' }, expected: 'success' },
  { scenario: 'Net: get_rpc_statistics_detailed', toolName: 'manage_networking', arguments: { action: 'get_rpc_statistics' }, expected: 'success' },
  { scenario: 'Net: get_net_role_info_enemy', toolName: 'manage_networking', arguments: { action: 'get_net_role_info', actorName: 'Enemy_01' }, expected: 'success|not found' },
  { scenario: 'Net: get_net_role_info_pickup', toolName: 'manage_networking', arguments: { action: 'get_net_role_info', actorName: 'Pickup_01' }, expected: 'success|not found' },
  
  // More Prediction Settings
  { scenario: 'Net: configure_prediction_settings_enemy', toolName: 'manage_networking', arguments: { action: 'configure_prediction_settings', blueprintPath: `${TEST_FOLDER}/BP_Enemy`, maxPredictionPing: 100, smoothNetUpdateEnabled: false }, expected: 'success|not found' },
  { scenario: 'Net: configure_prediction_settings_vehicle', toolName: 'manage_networking', arguments: { action: 'configure_prediction_settings', blueprintPath: `${TEST_FOLDER}/BP_Vehicle`, maxPredictionPing: 200, smoothNetUpdateEnabled: true }, expected: 'success|not found' },
  
  // More Spectator Settings
  { scenario: 'Net: set_spectator_class_ctf', toolName: 'manage_networking', arguments: { action: 'set_spectator_class', gameModePath: `${TEST_FOLDER}/GM_CTF`, spectatorClass: `${TEST_FOLDER}/BP_CTFSpectator` }, expected: 'success|not found' },
  { scenario: 'Net: set_player_state_class_ctf', toolName: 'manage_networking', arguments: { action: 'set_player_state_class', gameModePath: `${TEST_FOLDER}/GM_CTF`, playerStateClass: `${TEST_FOLDER}/PS_CTFPlayer` }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_GAMEPLAY_SYSTEMS (50 actions) - FULL COVERAGE
// ============================================================================
const manageGameplaySystemsTests = [
  // === Targeting ===
  { scenario: 'GS: create_targeting_preset', toolName: 'manage_gameplay_systems', arguments: { action: 'create_targeting_preset', name: 'TP_Enemies', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GS: configure_targeting', toolName: 'manage_gameplay_systems', arguments: { action: 'configure_targeting', presetPath: `${TEST_FOLDER}/TP_Enemies`, maxDistance: 3000, fov: 60 }, expected: 'success|not found' },
  
  // === Checkpoints ===
  { scenario: 'GS: create_checkpoint', toolName: 'manage_gameplay_systems', arguments: { action: 'create_checkpoint', checkpointId: 'CP_Start', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'GS: save_checkpoint', toolName: 'manage_gameplay_systems', arguments: { action: 'save_checkpoint', checkpointId: 'CP_Start' }, expected: 'success|not found' },
  { scenario: 'GS: load_checkpoint', toolName: 'manage_gameplay_systems', arguments: { action: 'load_checkpoint', checkpointId: 'CP_Start' }, expected: 'success|not found' },
  { scenario: 'GS: list_checkpoints', toolName: 'manage_gameplay_systems', arguments: { action: 'list_checkpoints' }, expected: 'success' },
  
  // === Objectives ===
  { scenario: 'GS: create_objective', toolName: 'manage_gameplay_systems', arguments: { action: 'create_objective', objectiveId: 'OBJ_FindKey', displayText: 'Find the Key', type: 'Primary' }, expected: 'success' },
  { scenario: 'GS: activate_objective', toolName: 'manage_gameplay_systems', arguments: { action: 'activate_objective', objectiveId: 'OBJ_FindKey' }, expected: 'success|not found' },
  { scenario: 'GS: complete_objective', toolName: 'manage_gameplay_systems', arguments: { action: 'complete_objective', objectiveId: 'OBJ_FindKey' }, expected: 'success|not found' },
  { scenario: 'GS: fail_objective', toolName: 'manage_gameplay_systems', arguments: { action: 'fail_objective', objectiveId: 'OBJ_FindKey' }, expected: 'success|not found' },
  { scenario: 'GS: get_active_objectives', toolName: 'manage_gameplay_systems', arguments: { action: 'get_active_objectives' }, expected: 'success' },
  
  // === Photo Mode ===
  { scenario: 'GS: enable_photo_mode', toolName: 'manage_gameplay_systems', arguments: { action: 'enable_photo_mode' }, expected: 'success' },
  { scenario: 'GS: configure_photo_mode', toolName: 'manage_gameplay_systems', arguments: { action: 'configure_photo_mode', fovRange: { min: 20, max: 120 }, rollRange: { min: -90, max: 90 } }, expected: 'success' },
  { scenario: 'GS: disable_photo_mode', toolName: 'manage_gameplay_systems', arguments: { action: 'disable_photo_mode' }, expected: 'success' },
  { scenario: 'GS: take_photo', toolName: 'manage_gameplay_systems', arguments: { action: 'take_photo', filename: 'Screenshot_01', format: 'PNG' }, expected: 'success' },
  
  // === Dialogue ===
  { scenario: 'GS: create_dialogue_tree', toolName: 'manage_gameplay_systems', arguments: { action: 'create_dialogue_tree', name: 'DT_NPC', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GS: add_dialogue_node', toolName: 'manage_gameplay_systems', arguments: { action: 'add_dialogue_node', treePath: `${TEST_FOLDER}/DT_NPC`, nodeId: 'Greeting', text: 'Hello traveler!', speaker: 'NPC' }, expected: 'success|not found' },
  { scenario: 'GS: add_dialogue_choice', toolName: 'manage_gameplay_systems', arguments: { action: 'add_dialogue_choice', treePath: `${TEST_FOLDER}/DT_NPC`, parentNodeId: 'Greeting', choiceText: 'Hello!', targetNodeId: 'Response1' }, expected: 'success|not found' },
  { scenario: 'GS: start_dialogue', toolName: 'manage_gameplay_systems', arguments: { action: 'start_dialogue', treePath: `${TEST_FOLDER}/DT_NPC`, npcActor: 'NPC_01', playerActor: 'Player' }, expected: 'success|not found' },
  
  // === Time of Day ===
  { scenario: 'GS: set_time_of_day', toolName: 'manage_gameplay_systems', arguments: { action: 'set_time_of_day', hours: 12, minutes: 0 }, expected: 'success' },
  { scenario: 'GS: set_day_cycle_speed', toolName: 'manage_gameplay_systems', arguments: { action: 'set_day_cycle_speed', speed: 60.0 }, expected: 'success' },
  { scenario: 'GS: pause_day_cycle', toolName: 'manage_gameplay_systems', arguments: { action: 'pause_day_cycle' }, expected: 'success' },
  
  // === Extended Gameplay Systems (Expanded Coverage) ===
  // More Targeting Operations
  { scenario: 'GS: create_targeting_preset_allies', toolName: 'manage_gameplay_systems', arguments: { action: 'create_targeting_preset', name: 'TP_Allies', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GS: configure_targeting_cone', toolName: 'manage_gameplay_systems', arguments: { action: 'configure_targeting', presetPath: `${TEST_FOLDER}/TP_Enemies`, maxDistance: 5000, fov: 120, targetingMode: 'Cone' }, expected: 'success|not found' },
  { scenario: 'GS: configure_targeting_priority', toolName: 'manage_gameplay_systems', arguments: { action: 'configure_targeting_priority', presetPath: `${TEST_FOLDER}/TP_Enemies`, priorityMode: 'Closest', losRequired: true }, expected: 'success|not found' },
  { scenario: 'GS: add_targeting_filter', toolName: 'manage_gameplay_systems', arguments: { action: 'add_targeting_filter', presetPath: `${TEST_FOLDER}/TP_Enemies`, filterType: 'Faction', filterValue: 'Enemy' }, expected: 'success|not found' },
  { scenario: 'GS: get_current_target', toolName: 'manage_gameplay_systems', arguments: { action: 'get_current_target', actorName: 'Player' }, expected: 'success|not found' },
  { scenario: 'GS: set_target', toolName: 'manage_gameplay_systems', arguments: { action: 'set_target', actorName: 'Player', targetActor: 'Enemy_01' }, expected: 'success|not found' },
  { scenario: 'GS: clear_target', toolName: 'manage_gameplay_systems', arguments: { action: 'clear_target', actorName: 'Player' }, expected: 'success|not found' },
  
  // More Checkpoint Operations
  { scenario: 'GS: create_checkpoint_boss', toolName: 'manage_gameplay_systems', arguments: { action: 'create_checkpoint', checkpointId: 'CP_Boss', location: { x: 5000, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'GS: create_checkpoint_secret', toolName: 'manage_gameplay_systems', arguments: { action: 'create_checkpoint', checkpointId: 'CP_Secret', location: { x: 1000, y: 2000, z: 500 } }, expected: 'success' },
  { scenario: 'GS: configure_checkpoint_data', toolName: 'manage_gameplay_systems', arguments: { action: 'configure_checkpoint_data', checkpointId: 'CP_Start', saveData: { health: 100, ammo: 50 } }, expected: 'success|not found' },
  { scenario: 'GS: delete_checkpoint', toolName: 'manage_gameplay_systems', arguments: { action: 'delete_checkpoint', checkpointId: 'CP_Secret' }, expected: 'success|not found' },
  { scenario: 'GS: get_latest_checkpoint', toolName: 'manage_gameplay_systems', arguments: { action: 'get_latest_checkpoint' }, expected: 'success' },
  
  // More Objective Operations
  { scenario: 'GS: create_objective_secondary', toolName: 'manage_gameplay_systems', arguments: { action: 'create_objective', objectiveId: 'OBJ_CollectGems', displayText: 'Collect all gems', type: 'Secondary' }, expected: 'success' },
  { scenario: 'GS: create_objective_hidden', toolName: 'manage_gameplay_systems', arguments: { action: 'create_objective', objectiveId: 'OBJ_Secret', displayText: 'Find the secret room', type: 'Hidden' }, expected: 'success' },
  { scenario: 'GS: create_objective_chain', toolName: 'manage_gameplay_systems', arguments: { action: 'create_objective_chain', chainId: 'OC_MainQuest', objectives: ['OBJ_FindKey', 'OBJ_OpenDoor', 'OBJ_DefeatBoss'] }, expected: 'success' },
  { scenario: 'GS: update_objective_progress', toolName: 'manage_gameplay_systems', arguments: { action: 'update_objective_progress', objectiveId: 'OBJ_CollectGems', currentProgress: 5, targetProgress: 10 }, expected: 'success|not found' },
  { scenario: 'GS: set_objective_marker', toolName: 'manage_gameplay_systems', arguments: { action: 'set_objective_marker', objectiveId: 'OBJ_FindKey', markerLocation: { x: 1000, y: 500, z: 100 } }, expected: 'success|not found' },
  { scenario: 'GS: get_completed_objectives', toolName: 'manage_gameplay_systems', arguments: { action: 'get_completed_objectives' }, expected: 'success' },
  
  // More Photo Mode Operations
  { scenario: 'GS: set_photo_camera_position', toolName: 'manage_gameplay_systems', arguments: { action: 'set_photo_camera_position', location: { x: 0, y: 0, z: 200 }, rotation: { pitch: -30, yaw: 0, roll: 0 } }, expected: 'success' },
  { scenario: 'GS: set_photo_focal_length', toolName: 'manage_gameplay_systems', arguments: { action: 'set_photo_focal_length', focalLength: 50 }, expected: 'success' },
  { scenario: 'GS: set_photo_aperture', toolName: 'manage_gameplay_systems', arguments: { action: 'set_photo_aperture', aperture: 2.8 }, expected: 'success' },
  { scenario: 'GS: add_photo_filter', toolName: 'manage_gameplay_systems', arguments: { action: 'add_photo_filter', filterType: 'Sepia' }, expected: 'success' },
  { scenario: 'GS: hide_photo_ui', toolName: 'manage_gameplay_systems', arguments: { action: 'hide_photo_ui' }, expected: 'success' },
  { scenario: 'GS: show_photo_ui', toolName: 'manage_gameplay_systems', arguments: { action: 'show_photo_ui' }, expected: 'success' },
  
  // More Dialogue Operations
  { scenario: 'GS: create_dialogue_tree_quest', toolName: 'manage_gameplay_systems', arguments: { action: 'create_dialogue_tree', name: 'DT_QuestGiver', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GS: add_dialogue_node_response', toolName: 'manage_gameplay_systems', arguments: { action: 'add_dialogue_node', treePath: `${TEST_FOLDER}/DT_NPC`, nodeId: 'Response1', text: 'Welcome to the village!', speaker: 'NPC' }, expected: 'success|not found' },
  { scenario: 'GS: add_dialogue_choice_bye', toolName: 'manage_gameplay_systems', arguments: { action: 'add_dialogue_choice', treePath: `${TEST_FOLDER}/DT_NPC`, parentNodeId: 'Greeting', choiceText: 'Goodbye', targetNodeId: 'Farewell' }, expected: 'success|not found' },
  { scenario: 'GS: add_dialogue_condition', toolName: 'manage_gameplay_systems', arguments: { action: 'add_dialogue_condition', treePath: `${TEST_FOLDER}/DT_NPC`, nodeId: 'QuestOffer', conditionType: 'HasItem', conditionValue: 'GoldCoin' }, expected: 'success|not found' },
  { scenario: 'GS: create_dialogue_node', toolName: 'manage_gameplay_systems', arguments: { action: 'create_dialogue_node', treePath: `${TEST_FOLDER}/DT_NPC`, nodeId: 'Farewell', text: 'Safe travels!', speaker: 'NPC' }, expected: 'success|not found' },
  { scenario: 'GS: end_dialogue', toolName: 'manage_gameplay_systems', arguments: { action: 'end_dialogue' }, expected: 'success' },
  { scenario: 'GS: get_dialogue_state', toolName: 'manage_gameplay_systems', arguments: { action: 'get_dialogue_state' }, expected: 'success' },
  
  // More Time Operations
  { scenario: 'GS: set_time_of_day_morning', toolName: 'manage_gameplay_systems', arguments: { action: 'set_time_of_day', hours: 6, minutes: 30 }, expected: 'success' },
  { scenario: 'GS: set_time_of_day_evening', toolName: 'manage_gameplay_systems', arguments: { action: 'set_time_of_day', hours: 18, minutes: 0 }, expected: 'success' },
  { scenario: 'GS: set_time_of_day_night', toolName: 'manage_gameplay_systems', arguments: { action: 'set_time_of_day', hours: 22, minutes: 0 }, expected: 'success' },
  { scenario: 'GS: resume_day_cycle', toolName: 'manage_gameplay_systems', arguments: { action: 'resume_day_cycle' }, expected: 'success' },
  { scenario: 'GS: get_current_time', toolName: 'manage_gameplay_systems', arguments: { action: 'get_current_time' }, expected: 'success' },
  
  // Save System
  { scenario: 'GS: configure_save_system', toolName: 'manage_gameplay_systems', arguments: { action: 'configure_save_system', autoSaveInterval: 300, maxSaveSlots: 10 }, expected: 'success' },
  { scenario: 'GS: set_game_state', toolName: 'manage_gameplay_systems', arguments: { action: 'set_game_state', stateKey: 'CurrentLevel', stateValue: 'Level_01' }, expected: 'success' },
  { scenario: 'GS: get_gameplay_systems_info', toolName: 'manage_gameplay_systems', arguments: { action: 'get_gameplay_systems_info' }, expected: 'success' },
  
  // Minimap
  { scenario: 'GS: configure_minimap_icon', toolName: 'manage_gameplay_systems', arguments: { action: 'configure_minimap_icon', actorName: 'Player', iconType: 'Player', iconColor: { r: 0, g: 1, b: 0, a: 1 } }, expected: 'success|not found' },
  
  // Quest System
  { scenario: 'GS: create_quest_stage', toolName: 'manage_gameplay_systems', arguments: { action: 'create_quest_stage', questId: 'Q_MainQuest', stageId: 'Stage_01', description: 'Talk to the village elder' }, expected: 'success' },
  
  // Localization
  { scenario: 'GS: configure_localization_entry', toolName: 'manage_gameplay_systems', arguments: { action: 'configure_localization_entry', key: 'UI_MainMenu', textValue: 'Main Menu', language: 'en' }, expected: 'success' },
];

// ============================================================================
// MANAGE_GAMEPLAY_PRIMITIVES (62 actions) - FULL COVERAGE
// ============================================================================
const manageGameplayPrimitivesTests = [
  // === State Machines ===
  { scenario: 'GP: create_state_machine', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_state_machine', name: 'SM_Door', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: add_state_closed', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Door`, stateName: 'Closed' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_open', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Door`, stateName: 'Open' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_locked', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Door`, stateName: 'Locked' }, expected: 'success|not found' },
  { scenario: 'GP: add_transition_closed_open', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_transition', smPath: `${TEST_FOLDER}/SM_Door`, fromState: 'Closed', toState: 'Open', eventName: 'Interact' }, expected: 'success|not found' },
  { scenario: 'GP: add_transition_open_closed', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_transition', smPath: `${TEST_FOLDER}/SM_Door`, fromState: 'Open', toState: 'Closed', eventName: 'Interact' }, expected: 'success|not found' },
  { scenario: 'GP: set_initial_state', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_initial_state', smPath: `${TEST_FOLDER}/SM_Door`, stateName: 'Closed' }, expected: 'success|not found' },
  { scenario: 'GP: fire_event', toolName: 'manage_gameplay_primitives', arguments: { action: 'fire_event', actorName: 'Door_01', eventName: 'Interact' }, expected: 'success|not found' },
  
  // === Values ===
  { scenario: 'GP: create_value_definition_int', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value_definition', name: 'VD_Currency', path: TEST_FOLDER, valueType: 'Integer', defaultValue: 0 }, expected: 'success|already exists' },
  { scenario: 'GP: create_value_definition_float', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value_definition', name: 'VD_Stamina', path: TEST_FOLDER, valueType: 'Float', defaultValue: 100.0 }, expected: 'success|already exists' },
  { scenario: 'GP: set_value', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_value', actorName: 'Player', valueName: 'Currency', value: 100 }, expected: 'success|not found' },
  { scenario: 'GP: get_value', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_value', actorName: 'Player', valueName: 'Currency' }, expected: 'success|not found' },
  { scenario: 'GP: add_value', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_value', actorName: 'Player', valueName: 'Currency', amount: 50 }, expected: 'success|not found' },
  
  // === Factions ===
  { scenario: 'GP: create_faction_heroes', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', name: 'FC_Heroes', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: create_faction_villains', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', name: 'FC_Villains', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: create_faction_neutral', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', name: 'FC_Neutral', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: set_faction_relation_hostile', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_faction_relation', faction1: `${TEST_FOLDER}/FC_Heroes`, faction2: `${TEST_FOLDER}/FC_Villains`, relation: 'Hostile' }, expected: 'success|not found' },
  { scenario: 'GP: set_faction_relation_neutral', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_faction_relation', faction1: `${TEST_FOLDER}/FC_Heroes`, faction2: `${TEST_FOLDER}/FC_Neutral`, relation: 'Neutral' }, expected: 'success|not found' },
  { scenario: 'GP: assign_faction', toolName: 'manage_gameplay_primitives', arguments: { action: 'assign_faction', actorName: 'Player', factionPath: `${TEST_FOLDER}/FC_Heroes` }, expected: 'success|not found' },
  { scenario: 'GP: check_faction_relation', toolName: 'manage_gameplay_primitives', arguments: { action: 'check_faction_relation', actor1: 'Player', actor2: 'Enemy_01' }, expected: 'success|not found' },
  
  // === Zones ===
  { scenario: 'GP: create_zone_safe', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneName: 'SafeZone', location: { x: 0, y: 0, z: 0 }, extent: { x: 500, y: 500, z: 200 } }, expected: 'success' },
  { scenario: 'GP: create_zone_damage', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneName: 'DamageZone', location: { x: 1000, y: 0, z: 0 }, extent: { x: 300, y: 300, z: 100 } }, expected: 'success' },
  { scenario: 'GP: add_zone_effect_heal', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_zone_effect', zoneName: 'SafeZone', effectType: 'HealOverTime', value: 10 }, expected: 'success|not found' },
  { scenario: 'GP: add_zone_effect_damage', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_zone_effect', zoneName: 'DamageZone', effectType: 'DamageOverTime', value: 5 }, expected: 'success|not found' },
  { scenario: 'GP: check_actor_in_zone', toolName: 'manage_gameplay_primitives', arguments: { action: 'check_actor_in_zone', actorName: 'Player', zoneName: 'SafeZone' }, expected: 'success|not found' },
  
  // === Conditions ===
  { scenario: 'GP: create_condition_hasitem', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'COND_HasKey', path: TEST_FOLDER, conditionType: 'HasItem' }, expected: 'success|already exists' },
  { scenario: 'GP: create_condition_level', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'COND_Level10', path: TEST_FOLDER, conditionType: 'MinLevel' }, expected: 'success|already exists' },
  { scenario: 'GP: configure_condition', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_condition', conditionPath: `${TEST_FOLDER}/COND_HasKey`, itemPath: `${TEST_FOLDER}/Item_Key`, quantity: 1 }, expected: 'success|not found' },
  { scenario: 'GP: evaluate_condition', toolName: 'manage_gameplay_primitives', arguments: { action: 'evaluate_condition', actorName: 'Player', conditionPath: `${TEST_FOLDER}/COND_HasKey` }, expected: 'success|not found' },
  
  // === Spawners ===
  { scenario: 'GP: create_spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_spawner', spawnerName: 'EnemySpawner', location: { x: 1000, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'GP: configure_spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_spawner', spawnerName: 'EnemySpawner', actorClass: `${TEST_FOLDER}/BP_Enemy`, maxSpawned: 5, spawnInterval: 10.0 }, expected: 'success|not found' },
  { scenario: 'GP: start_spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'start_spawner', spawnerName: 'EnemySpawner' }, expected: 'success|not found' },
  { scenario: 'GP: stop_spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'stop_spawner', spawnerName: 'EnemySpawner' }, expected: 'success|not found' },
  
  // === Interactables ===
  { scenario: 'GP: create_interactable', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_interactable', name: 'IA_Chest', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: configure_interactable', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_interactable', interactablePath: `${TEST_FOLDER}/IA_Chest`, interactionDistance: 200, interactionTime: 1.0 }, expected: 'success|not found' },
  { scenario: 'GP: add_interactable_event', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_interactable_event', interactablePath: `${TEST_FOLDER}/IA_Chest`, eventType: 'OnInteract', functionName: 'OpenChest' }, expected: 'success|not found' },
  
  // === Extended Gameplay Primitives (Expanded Coverage) ===
  // More State Machine Operations
  { scenario: 'GP: add_state_opening', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Door`, stateName: 'Opening' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_closing', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Door`, stateName: 'Closing' }, expected: 'success|not found' },
  { scenario: 'GP: add_transition_locked_closed', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_transition', smPath: `${TEST_FOLDER}/SM_Door`, fromState: 'Locked', toState: 'Closed', eventName: 'Unlock' }, expected: 'success|not found' },
  { scenario: 'GP: add_transition_closed_locked', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_transition', smPath: `${TEST_FOLDER}/SM_Door`, fromState: 'Closed', toState: 'Locked', eventName: 'Lock' }, expected: 'success|not found' },
  { scenario: 'GP: set_initial_state', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_initial_state', smPath: `${TEST_FOLDER}/SM_Door`, stateName: 'Closed' }, expected: 'success|not found' },
  { scenario: 'GP: remove_state', toolName: 'manage_gameplay_primitives', arguments: { action: 'remove_state', smPath: `${TEST_FOLDER}/SM_Door`, stateName: 'Opening' }, expected: 'success|not found' },
  { scenario: 'GP: remove_transition', toolName: 'manage_gameplay_primitives', arguments: { action: 'remove_transition', smPath: `${TEST_FOLDER}/SM_Door`, fromState: 'Closed', toState: 'Open' }, expected: 'success|not found' },
  { scenario: 'GP: get_current_state', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_current_state', actorName: 'Door_01' }, expected: 'success|not found' },
  { scenario: 'GP: force_state', toolName: 'manage_gameplay_primitives', arguments: { action: 'force_state', actorName: 'Door_01', stateName: 'Open' }, expected: 'success|not found' },
  
  // More Value Containers
  { scenario: 'GP: create_value_float', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value', valueId: 'PlayerSpeed', valueType: 'Float', initialValue: 600.0 }, expected: 'success|already exists' },
  { scenario: 'GP: create_value_string', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value', valueId: 'PlayerName', valueType: 'String', initialValue: 'Hero' }, expected: 'success|already exists' },
  { scenario: 'GP: create_value_vector', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value', valueId: 'SpawnLocation', valueType: 'Vector', initialValue: { x: 0, y: 0, z: 100 } }, expected: 'success|already exists' },
  { scenario: 'GP: create_value_bool', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value', valueId: 'IsAlive', valueType: 'Bool', initialValue: true }, expected: 'success|already exists' },
  { scenario: 'GP: modify_value_add', toolName: 'manage_gameplay_primitives', arguments: { action: 'modify_value', valueId: 'PlayerHealth', operation: 'Add', amount: 10 }, expected: 'success|not found' },
  { scenario: 'GP: modify_value_multiply', toolName: 'manage_gameplay_primitives', arguments: { action: 'modify_value', valueId: 'PlayerHealth', operation: 'Multiply', amount: 1.5 }, expected: 'success|not found' },
  { scenario: 'GP: clamp_value', toolName: 'manage_gameplay_primitives', arguments: { action: 'clamp_value', valueId: 'PlayerHealth', min: 0, max: 100 }, expected: 'success|not found' },
  
  // More Faction Operations
  { scenario: 'GP: create_faction_neutral', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', factionId: 'Neutral', displayName: 'Neutral Faction' }, expected: 'success|already exists' },
  { scenario: 'GP: create_faction_hostile', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', factionId: 'Hostile', displayName: 'Hostile Faction' }, expected: 'success|already exists' },
  { scenario: 'GP: set_faction_relationship_hostile', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_faction_relationship', factionA: 'PlayerFaction', factionB: 'Hostile', relationship: 'Hostile' }, expected: 'success|not found' },
  { scenario: 'GP: set_faction_relationship_neutral', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_faction_relationship', factionA: 'PlayerFaction', factionB: 'Neutral', relationship: 'Neutral' }, expected: 'success|not found' },
  { scenario: 'GP: get_faction_relationship', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_faction_relationship', factionA: 'PlayerFaction', factionB: 'EnemyFaction' }, expected: 'success|not found' },
  { scenario: 'GP: get_faction_members', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_faction_members', factionId: 'PlayerFaction' }, expected: 'success|not found' },
  
  // More Zone Operations
  { scenario: 'GP: create_zone_healing', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneId: 'Zone_Healing', location: { x: 500, y: 500, z: 0 }, size: { x: 300, y: 300, z: 200 }, zoneType: 'Effect' }, expected: 'success' },
  { scenario: 'GP: create_zone_damage', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneId: 'Zone_Damage', location: { x: 1000, y: 500, z: 0 }, size: { x: 200, y: 200, z: 150 }, zoneType: 'Effect' }, expected: 'success' },
  { scenario: 'GP: create_zone_spawn', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneId: 'Zone_Spawn', location: { x: 0, y: 0, z: 0 }, size: { x: 500, y: 500, z: 200 }, zoneType: 'Spawn' }, expected: 'success' },
  { scenario: 'GP: get_zone_actors', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_zone_actors', zoneId: 'Zone_SafeArea' }, expected: 'success|not found' },
  { scenario: 'GP: is_actor_in_zone', toolName: 'manage_gameplay_primitives', arguments: { action: 'is_actor_in_zone', actorName: 'Player', zoneId: 'Zone_SafeArea' }, expected: 'success|not found' },
  { scenario: 'GP: set_zone_enabled', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_zone_enabled', zoneId: 'Zone_SafeArea', enabled: false }, expected: 'success|not found' },
  
  // More Condition Operations
  { scenario: 'GP: create_condition_and', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'COND_CanEnter', path: TEST_FOLDER, conditionType: 'AND' }, expected: 'success|already exists' },
  { scenario: 'GP: create_condition_or', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'COND_AnyKey', path: TEST_FOLDER, conditionType: 'OR' }, expected: 'success|already exists' },
  { scenario: 'GP: add_subcondition', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_subcondition', conditionPath: `${TEST_FOLDER}/COND_CanEnter`, subconditionPath: `${TEST_FOLDER}/COND_HasKey` }, expected: 'success|not found' },
  { scenario: 'GP: add_value_check', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_value_check', conditionPath: `${TEST_FOLDER}/COND_HasKey`, valueId: 'KeyCount', operator: 'GreaterThan', compareValue: 0 }, expected: 'success|not found' },
  
  // More Spawner Operations
  { scenario: 'GP: create_spawner_wave', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_spawner', spawnerName: 'WaveSpawner', location: { x: 2000, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'GP: configure_spawner_random', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_spawner', spawnerName: 'WaveSpawner', actorClass: `${TEST_FOLDER}/BP_Enemy`, maxSpawned: 10, spawnInterval: 5.0, randomizeLocation: true }, expected: 'success|not found' },
  { scenario: 'GP: reset_spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'reset_spawner', spawnerName: 'EnemySpawner' }, expected: 'success|not found' },
  { scenario: 'GP: get_spawner_count', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_spawner_count', spawnerName: 'EnemySpawner' }, expected: 'success|not found' },
  
  // More Interactable Operations
  { scenario: 'GP: create_interactable_lever', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_interactable', name: 'IA_Lever', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: create_interactable_button', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_interactable', name: 'IA_Button', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: add_interactable_highlight', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_interactable_event', interactablePath: `${TEST_FOLDER}/IA_Chest`, eventType: 'OnHighlight', functionName: 'ShowOutline' }, expected: 'success|not found' },
  { scenario: 'GP: set_interactable_enabled', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_interactable_enabled', interactablePath: `${TEST_FOLDER}/IA_Chest`, enabled: true }, expected: 'success|not found' },
  { scenario: 'GP: get_nearby_interactables', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_nearby_interactables', actorName: 'Player', radius: 500 }, expected: 'success|not found' },
  
  // === EXTENDED GAMEPLAY PRIMITIVES (Additional ~100 tests for comprehensive coverage) ===
  // More State Machine Variations
  { scenario: 'GP: create_state_machine_elevator', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_state_machine', name: 'SM_Elevator', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: create_state_machine_turret', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_state_machine', name: 'SM_Turret', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: add_state_elevator_floor1', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Elevator`, stateName: 'Floor1' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_elevator_floor2', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Elevator`, stateName: 'Floor2' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_elevator_floor3', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Elevator`, stateName: 'Floor3' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_elevator_moving', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Elevator`, stateName: 'Moving' }, expected: 'success|not found' },
  { scenario: 'GP: add_transition_elevator_1to2', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_transition', smPath: `${TEST_FOLDER}/SM_Elevator`, fromState: 'Floor1', toState: 'Moving', eventName: 'CallFloor2' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_turret_idle', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Turret`, stateName: 'Idle' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_turret_scanning', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Turret`, stateName: 'Scanning' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_turret_firing', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Turret`, stateName: 'Firing' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_turret_disabled', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state', smPath: `${TEST_FOLDER}/SM_Turret`, stateName: 'Disabled' }, expected: 'success|not found' },
  { scenario: 'GP: fire_event_turret', toolName: 'manage_gameplay_primitives', arguments: { action: 'fire_event', actorName: 'Turret_01', eventName: 'TargetDetected' }, expected: 'success|not found' },
  { scenario: 'GP: get_current_state_turret', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_current_state', actorName: 'Turret_01' }, expected: 'success|not found' },
  
  // More Value Containers
  { scenario: 'GP: create_value_damage', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value', valueId: 'BaseDamage', valueType: 'Float', initialValue: 25.0 }, expected: 'success|already exists' },
  { scenario: 'GP: create_value_armor', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value', valueId: 'Armor', valueType: 'Float', initialValue: 0.0 }, expected: 'success|already exists' },
  { scenario: 'GP: create_value_level', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value', valueId: 'CharacterLevel', valueType: 'Int', initialValue: 1 }, expected: 'success|already exists' },
  { scenario: 'GP: create_value_exp', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value', valueId: 'Experience', valueType: 'Int', initialValue: 0 }, expected: 'success|already exists' },
  { scenario: 'GP: modify_value_subtract', toolName: 'manage_gameplay_primitives', arguments: { action: 'modify_value', valueId: 'PlayerHealth', operation: 'Subtract', amount: 25 }, expected: 'success|not found' },
  { scenario: 'GP: modify_value_divide', toolName: 'manage_gameplay_primitives', arguments: { action: 'modify_value', valueId: 'PlayerHealth', operation: 'Divide', amount: 2 }, expected: 'success|not found' },
  { scenario: 'GP: set_value', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_value', valueId: 'PlayerHealth', value: 100 }, expected: 'success|not found' },
  { scenario: 'GP: get_value', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_value', valueId: 'PlayerHealth' }, expected: 'success|not found' },
  
  // More Faction Operations
  { scenario: 'GP: create_faction_wildlife', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', factionId: 'Wildlife', displayName: 'Wildlife' }, expected: 'success|already exists' },
  { scenario: 'GP: create_faction_undead', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', factionId: 'Undead', displayName: 'The Undead' }, expected: 'success|already exists' },
  { scenario: 'GP: set_faction_relationship_wildlife', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_faction_relationship', factionA: 'PlayerFaction', factionB: 'Wildlife', relationship: 'Neutral' }, expected: 'success|not found' },
  { scenario: 'GP: set_faction_relationship_undead', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_faction_relationship', factionA: 'Wildlife', factionB: 'Undead', relationship: 'Hostile' }, expected: 'success|not found' },
  { scenario: 'GP: assign_faction', toolName: 'manage_gameplay_primitives', arguments: { action: 'assign_faction', actorName: 'Wolf_01', factionId: 'Wildlife' }, expected: 'success|not found' },
  { scenario: 'GP: get_actor_faction', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_actor_faction', actorName: 'Wolf_01' }, expected: 'success|not found' },
  
  // More Zone Operations
  { scenario: 'GP: create_zone_spawn_player', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneName: 'PlayerSpawnZone', location: { x: 0, y: 0, z: 100 }, extent: { x: 200, y: 200, z: 50 } }, expected: 'success' },
  { scenario: 'GP: create_zone_spawn_enemy', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneName: 'EnemySpawnZone', location: { x: 2000, y: 0, z: 100 }, extent: { x: 500, y: 500, z: 100 } }, expected: 'success' },
  { scenario: 'GP: create_zone_objective', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneName: 'ObjectiveZone', location: { x: 5000, y: 0, z: 100 }, extent: { x: 300, y: 300, z: 200 } }, expected: 'success' },
  { scenario: 'GP: add_zone_effect_speedboost', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_zone_effect', zoneName: 'SpeedZone', effectType: 'SpeedModifier', value: 1.5 }, expected: 'success|not found' },
  { scenario: 'GP: add_zone_effect_slow', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_zone_effect', zoneName: 'SlowZone', effectType: 'SpeedModifier', value: 0.5 }, expected: 'success|not found' },
  { scenario: 'GP: remove_zone_effect', toolName: 'manage_gameplay_primitives', arguments: { action: 'remove_zone_effect', zoneName: 'SafeZone', effectType: 'HealOverTime' }, expected: 'success|not found' },
  { scenario: 'GP: set_zone_enabled', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_zone_enabled', zoneName: 'DamageZone', enabled: false }, expected: 'success|not found' },
  { scenario: 'GP: get_zone_info', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_zone_info', zoneName: 'SafeZone' }, expected: 'success|not found' },
  
  // More Condition Operations
  { scenario: 'GP: create_condition_health', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'COND_LowHealth', path: TEST_FOLDER, conditionType: 'ValueCheck' }, expected: 'success|already exists' },
  { scenario: 'GP: create_condition_time', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'COND_DayTime', path: TEST_FOLDER, conditionType: 'TimeOfDay' }, expected: 'success|already exists' },
  { scenario: 'GP: create_condition_quest', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'COND_QuestComplete', path: TEST_FOLDER, conditionType: 'QuestState' }, expected: 'success|already exists' },
  { scenario: 'GP: configure_condition_health', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_condition', conditionPath: `${TEST_FOLDER}/COND_LowHealth`, valueId: 'PlayerHealth', operator: 'LessThan', compareValue: 25 }, expected: 'success|not found' },
  { scenario: 'GP: configure_condition_time', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_condition', conditionPath: `${TEST_FOLDER}/COND_DayTime`, startHour: 6, endHour: 18 }, expected: 'success|not found' },
  { scenario: 'GP: evaluate_condition_health', toolName: 'manage_gameplay_primitives', arguments: { action: 'evaluate_condition', actorName: 'Player', conditionPath: `${TEST_FOLDER}/COND_LowHealth` }, expected: 'success|not found' },
  
  // More Spawner Operations
  { scenario: 'GP: create_spawner_wave', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_spawner', spawnerName: 'WaveSpawner', location: { x: 3000, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'GP: create_spawner_pickup', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_spawner', spawnerName: 'PickupSpawner', location: { x: 500, y: 500, z: 0 } }, expected: 'success' },
  { scenario: 'GP: configure_spawner_wave', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_spawner', spawnerName: 'WaveSpawner', actorClass: `${TEST_FOLDER}/BP_Enemy`, maxSpawned: 20, spawnInterval: 2.0 }, expected: 'success|not found' },
  { scenario: 'GP: configure_spawner_pickup', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_spawner', spawnerName: 'PickupSpawner', actorClass: `${TEST_FOLDER}/BP_Pickup`, maxSpawned: 3, spawnInterval: 30.0 }, expected: 'success|not found' },
  { scenario: 'GP: start_spawner_wave', toolName: 'manage_gameplay_primitives', arguments: { action: 'start_spawner', spawnerName: 'WaveSpawner' }, expected: 'success|not found' },
  { scenario: 'GP: pause_spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'pause_spawner', spawnerName: 'WaveSpawner' }, expected: 'success|not found' },
  { scenario: 'GP: resume_spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'resume_spawner', spawnerName: 'WaveSpawner' }, expected: 'success|not found' },
  { scenario: 'GP: reset_spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'reset_spawner', spawnerName: 'WaveSpawner' }, expected: 'success|not found' },
  { scenario: 'GP: get_spawner_count', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_spawner_count', spawnerName: 'WaveSpawner' }, expected: 'success|not found' },
  { scenario: 'GP: get_spawner_info', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_spawner_info', spawnerName: 'WaveSpawner' }, expected: 'success|not found' },
  
  // More Interactable Operations
  { scenario: 'GP: create_interactable_terminal', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_interactable', name: 'IA_Terminal', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: create_interactable_pickup', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_interactable', name: 'IA_PickupItem', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: create_interactable_npc', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_interactable', name: 'IA_NPCTalk', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GP: configure_interactable_terminal', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_interactable', interactablePath: `${TEST_FOLDER}/IA_Terminal`, interactionDistance: 150, interactionTime: 0 }, expected: 'success|not found' },
  { scenario: 'GP: configure_interactable_npc', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_interactable', interactablePath: `${TEST_FOLDER}/IA_NPCTalk`, interactionDistance: 250, interactionTime: 0 }, expected: 'success|not found' },
  { scenario: 'GP: add_interactable_event_use', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_interactable_event', interactablePath: `${TEST_FOLDER}/IA_Terminal`, eventType: 'OnInteract', functionName: 'UseTerminal' }, expected: 'success|not found' },
  { scenario: 'GP: add_interactable_event_talk', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_interactable_event', interactablePath: `${TEST_FOLDER}/IA_NPCTalk`, eventType: 'OnInteract', functionName: 'StartDialogue' }, expected: 'success|not found' },
  { scenario: 'GP: set_interactable_enabled_terminal', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_interactable_enabled', interactablePath: `${TEST_FOLDER}/IA_Terminal`, enabled: true }, expected: 'success|not found' },
  { scenario: 'GP: get_interactable_info', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_interactable_info', interactablePath: `${TEST_FOLDER}/IA_Chest` }, expected: 'success|not found' },
  
  // Timer Operations
  { scenario: 'GP: create_timer', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_timer', timerId: 'RespawnTimer', duration: 5.0 }, expected: 'success' },
  { scenario: 'GP: start_timer', toolName: 'manage_gameplay_primitives', arguments: { action: 'start_timer', timerId: 'RespawnTimer' }, expected: 'success|not found' },
  { scenario: 'GP: pause_timer', toolName: 'manage_gameplay_primitives', arguments: { action: 'pause_timer', timerId: 'RespawnTimer' }, expected: 'success|not found' },
  { scenario: 'GP: resume_timer', toolName: 'manage_gameplay_primitives', arguments: { action: 'resume_timer', timerId: 'RespawnTimer' }, expected: 'success|not found' },
  { scenario: 'GP: reset_timer', toolName: 'manage_gameplay_primitives', arguments: { action: 'reset_timer', timerId: 'RespawnTimer' }, expected: 'success|not found' },
  { scenario: 'GP: get_timer_remaining', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_timer_remaining', timerId: 'RespawnTimer' }, expected: 'success|not found' },
  
  // Score/Points Operations
  { scenario: 'GP: add_score', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_score', playerId: 'Player_01', points: 100 }, expected: 'success|not found' },
  { scenario: 'GP: subtract_score', toolName: 'manage_gameplay_primitives', arguments: { action: 'subtract_score', playerId: 'Player_01', points: 25 }, expected: 'success|not found' },
  { scenario: 'GP: get_score', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_score', playerId: 'Player_01' }, expected: 'success|not found' },
  { scenario: 'GP: reset_score', toolName: 'manage_gameplay_primitives', arguments: { action: 'reset_score', playerId: 'Player_01' }, expected: 'success|not found' },
  { scenario: 'GP: get_leaderboard', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_leaderboard', maxEntries: 10 }, expected: 'success' },
  
  // Additional Timer Variations
  { scenario: 'GP: create_timer_game_over', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_timer', timerId: 'GameOverTimer', duration: 300.0 }, expected: 'success' },
  { scenario: 'GP: create_timer_match', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_timer', timerId: 'MatchTimer', duration: 600.0 }, expected: 'success' },
  { scenario: 'GP: create_timer_cooldown', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_timer', timerId: 'AbilityCooldown', duration: 3.0 }, expected: 'success' },
  { scenario: 'GP: start_timer_game_over', toolName: 'manage_gameplay_primitives', arguments: { action: 'start_timer', timerId: 'GameOverTimer' }, expected: 'success|not found' },
  { scenario: 'GP: start_timer_match', toolName: 'manage_gameplay_primitives', arguments: { action: 'start_timer', timerId: 'MatchTimer' }, expected: 'success|not found' },
  
  // Additional Score Variations
  { scenario: 'GP: add_score_player2', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_score', playerId: 'Player_02', points: 250 }, expected: 'success|not found' },
  { scenario: 'GP: add_score_team_red', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_score', playerId: 'TeamRed', points: 500 }, expected: 'success|not found' },
  { scenario: 'GP: add_score_team_blue', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_score', playerId: 'TeamBlue', points: 350 }, expected: 'success|not found' },
  { scenario: 'GP: get_score_player2', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_score', playerId: 'Player_02' }, expected: 'success|not found' },
  { scenario: 'GP: reset_score_team_red', toolName: 'manage_gameplay_primitives', arguments: { action: 'reset_score', playerId: 'TeamRed' }, expected: 'success|not found' },
  
  // Additional State Machine Variations
  { scenario: 'GP: create_state_machine_ai', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_state_machine', name: 'SM_AIBehavior', path: TEST_FOLDER, states: ['Idle', 'Patrol', 'Chase', 'Attack', 'Flee'] }, expected: 'success|already exists' },
  { scenario: 'GP: create_state_machine_vehicle', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_state_machine', name: 'SM_VehicleState', path: TEST_FOLDER, states: ['Parked', 'Starting', 'Driving', 'Reversing', 'Stopped'] }, expected: 'success|already exists' },
  { scenario: 'GP: add_state_machine_transition_ai', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state_machine_transition', stateMachinePath: `${TEST_FOLDER}/SM_AIBehavior`, fromState: 'Patrol', toState: 'Chase', conditionName: 'CanSeePlayer' }, expected: 'success|not found' },
  { scenario: 'GP: add_state_machine_transition_flee', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_state_machine_transition', stateMachinePath: `${TEST_FOLDER}/SM_AIBehavior`, fromState: 'Attack', toState: 'Flee', conditionName: 'HealthLow' }, expected: 'success|not found' },
  
  // Additional Zone Variations
  { scenario: 'GP: create_zone_capture', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', name: 'Zone_CapturePoint', path: TEST_FOLDER, zoneType: 'Objective' }, expected: 'success|already exists' },
  { scenario: 'GP: create_zone_safe', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', name: 'Zone_SafeArea', path: TEST_FOLDER, zoneType: 'Gameplay' }, expected: 'success|already exists' },
  { scenario: 'GP: create_zone_death', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', name: 'Zone_DeathZone', path: TEST_FOLDER, zoneType: 'Trigger' }, expected: 'success|already exists' },
  { scenario: 'GP: configure_zone_capture', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_zone', zonePath: `${TEST_FOLDER}/Zone_CapturePoint`, captureTime: 15.0 }, expected: 'success|not found' },
  { scenario: 'GP: add_zone_event_capture', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_zone_event', zonePath: `${TEST_FOLDER}/Zone_CapturePoint`, eventType: 'OnCaptured', functionName: 'PointCaptured' }, expected: 'success|not found' },
  
  // Additional Condition Variations
  { scenario: 'GP: create_condition_stamina', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'Cond_HasStamina', path: TEST_FOLDER, conditionType: 'Resource' }, expected: 'success|already exists' },
  { scenario: 'GP: create_condition_weapon', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', name: 'Cond_HasWeapon', path: TEST_FOLDER, conditionType: 'Inventory' }, expected: 'success|already exists' },
  { scenario: 'GP: configure_condition_stamina', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_condition', conditionPath: `${TEST_FOLDER}/Cond_HasStamina`, threshold: 20, operator: 'GreaterThan' }, expected: 'success|not found' },
  
  // Additional Value Container Variations
  { scenario: 'GP: create_value_container_mana', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value_container', name: 'Val_Mana', path: TEST_FOLDER, valueType: 'Float', initialValue: 100.0 }, expected: 'success|already exists' },
  { scenario: 'GP: create_value_container_gold', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value_container', name: 'Val_Gold', path: TEST_FOLDER, valueType: 'Integer', initialValue: 0 }, expected: 'success|already exists' },
  { scenario: 'GP: set_value_container_gold', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_value_container_value', containerPath: `${TEST_FOLDER}/Val_Gold`, value: 1000 }, expected: 'success|not found' },
];

// ============================================================================
// EXPORTS
// ============================================================================
export const gameplayToolsTests = [
  ...animationPhysicsTests,
  ...manageEffectTests,
  ...manageCharacterTests,
  ...manageCombatTests,
  ...manageAITests,
  ...manageNetworkingTests,
  ...manageGameplaySystemsTests,
  ...manageGameplayPrimitivesTests,
];

// Main execution
const main = async () => {
  console.log('='.repeat(80));
  console.log('GAMEPLAY TOOLS INTEGRATION TESTS - EXPANDED');
  console.log('Tools: animation_physics, manage_effect, manage_character, manage_combat,');
  console.log('       manage_ai, manage_networking, manage_gameplay_systems, manage_gameplay_primitives');
  console.log(`Total Test Cases: ${gameplayToolsTests.length}`);
  console.log('='.repeat(80));
  
  try {
    await runToolTests('gameplay-tools', gameplayToolsTests);
  } catch (error) {
    console.error('Test execution failed:', error);
    process.exit(1);
  }
};

// Run if executed directly
if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  main();
}
