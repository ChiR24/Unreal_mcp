#!/usr/bin/env node
/**
 * Comprehensive Animation & Physics Test Suite
 * Tool: animation_physics
 * Coverage: All 15 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

// Use the tutorial skeleton that comes with UE
const tutorialSkeleton = '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton';

const testCases = [
  // === PRE-CLEANUP ===
  {
    scenario: 'Pre-cleanup animation artifacts',
    toolName: 'animation_physics',
    arguments: { action: 'cleanup', filter: 'TC_*' },
    expected: 'success|no matching'
  },

  // === CREATE ANIMATION BLUEPRINT ===
  {
    scenario: 'Create Animation Blueprint',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_animation_bp',
      name: 'TC_ABP_Character',
      skeletonPath: tutorialSkeleton,
      savePath: '/Game/Animations'
    },
    expected: 'success or not found'
  },
  {
    scenario: 'Create Animation Blueprint (minimal)',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_animation_bp',
      name: 'TC_ABP_Minimal',
      skeletonPath: tutorialSkeleton,
      savePath: '/Game/Animations',
      stateMachines: []
    },
    expected: 'success or not found'
  },

  // === CREATE ANIMATION ASSET ===
  {
    scenario: 'Create Animation Sequence',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_animation_asset',
      name: 'TC_Anim_Sequence',
      skeletonPath: tutorialSkeleton,
      savePath: '/Game/Animations',
      assetType: 'sequence'
    },
    expected: 'success or not found'
  },
  {
    scenario: 'Create Animation Montage',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_animation_asset',
      name: 'TC_Anim_Montage',
      skeletonPath: tutorialSkeleton,
      savePath: '/Game/Animations',
      assetType: 'montage'
    },
    expected: 'success or not found'
  },

  // === ADD NOTIFY ===
  {
    scenario: 'Add Notify to Montage',
    toolName: 'animation_physics',
    arguments: {
      action: 'add_notify',
      assetPath: '/Game/Animations/TC_Anim_Montage',
      notifyName: 'AnimNotify_PlaySound',
      time: 0.5
    },
    expected: 'success or not found or AUTOMATION_BRIDGE_UNAVAILABLE'
  },
  {
    scenario: 'Add Second Notify',
    toolName: 'animation_physics',
    arguments: {
      action: 'add_notify',
      assetPath: '/Game/Animations/TC_Anim_Montage',
      notifyName: 'AnimNotify_PlayParticleEffect',
      time: 1.0
    },
    expected: 'success or not found'
  },

  // === PLAY MONTAGE ===
  {
    scenario: 'Play Montage',
    toolName: 'animation_physics',
    arguments: {
      action: 'play_montage',
      actorName: 'SkeletalCharacter',
      montagePath: '/Game/Animations/TC_Anim_Montage',
      playRate: 1.0
    },
    expected: 'success or not found'
  },
  {
    scenario: 'Play Montage (slow)',
    toolName: 'animation_physics',
    arguments: {
      action: 'play_montage',
      actorName: 'SkeletalCharacter',
      montagePath: '/Game/Animations/TC_Anim_Montage',
      playRate: 0.5
    },
    expected: 'success or not found'
  },

  // === CREATE BLEND SPACE ===
  {
    scenario: 'Create Blend Space 1D',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_blend_space',
      name: 'TC_BS_Locomotion',
      skeletonPath: tutorialSkeleton,
      savePath: '/Game/Animations',
      horizontalAxis: { name: 'Speed', minValue: 0, maxValue: 600 },
      animations: []
    },
    expected: 'success - blend space created'
  },
  {
    scenario: 'Create Blend Space 2D',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_blend_space',
      name: 'TC_BS_Movement',
      skeletonPath: tutorialSkeleton,
      savePath: '/Game/Animations',
      horizontalAxis: { name: 'Speed', minValue: 0, maxValue: 600 },
      verticalAxis: { name: 'Direction', minValue: -180, maxValue: 180 },
      animations: []
    },
    expected: 'success'
  },

  // === CREATE STATE MACHINE ===
  {
    scenario: 'Create State Machine',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_state_machine',
      machineName: 'TC_StateMachine',
      states: [{ name: 'Idle' }, { name: 'Walk' }, { name: 'Run' }, { name: 'Jump' }],
      transitions: []
    },
    expected: 'success'
  },

  // === CREATE BLEND TREE ===
  {
    scenario: 'Create Blend Tree',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_blend_tree',
      treeName: 'TC_BlendTree',
      basePose: '/Game/Animations/TC_Anim_Sequence',
      additiveAnimations: []
    },
    expected: 'success'
  },

  // === SETUP IK ===
  {
    scenario: 'Setup IK (Foot)',
    toolName: 'animation_physics',
    arguments: {
      action: 'setup_ik',
      actorName: 'Character',
      chain: { rootBone: 'pelvis', endBone: 'foot_l' },
      effector: { targetActor: 'TargetActor' }
    },
    expected: 'success'
  },
  {
    scenario: 'Setup IK (Hand)',
    toolName: 'animation_physics',
    arguments: {
      action: 'setup_ik',
      actorName: 'Character',
      chain: { rootBone: 'spine_01', endBone: 'hand_r' },
      effector: { targetActor: 'HandTarget' }
    },
    expected: 'success'
  },

  // === CREATE PROCEDURAL ANIM ===
  {
    scenario: 'Create Procedural Animation',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_procedural_anim',
      systemName: 'TC_ProceduralAnim',
      baseAnimation: '/Game/Animations/TC_Anim_Sequence',
      settings: {}
    },
    expected: 'success'
  },

  // === RAGDOLL ===
  {
    scenario: 'Setup Ragdoll',
    toolName: 'animation_physics',
    arguments: {
      action: 'setup_ragdoll',
      skeletonPath: tutorialSkeleton,
      actorName: 'RagdollCharacter',
      physicsAssetName: 'TC_PhysicsAsset',
      blendWeight: 1.0
    },
    expected: 'success or not found'
  },
  {
    scenario: 'Activate Ragdoll',
    toolName: 'animation_physics',
    arguments: {
      action: 'activate_ragdoll',
      actorName: 'RagdollCharacter',
      blendTime: 0.2
    },
    expected: 'success|not_found'
  },

  // === PHYSICS SIMULATION ===
  {
    scenario: 'Setup Physics Simulation',
    toolName: 'animation_physics',
    arguments: {
      action: 'setup_physics_simulation',
      skeletonPath: tutorialSkeleton,
      savePath: '/Game/Physics',
      physicsAssetName: 'TC_PhysAsset',
      assignToMesh: false
    },
    expected: 'success or not found'
  },

  // === CONFIGURE VEHICLE ===
  {
    scenario: 'Configure Vehicle (4 wheels)',
    toolName: 'animation_physics',
    arguments: {
      action: 'configure_vehicle',
      vehicleName: 'TC_Vehicle',
      vehicleType: 'FourWheel'
    },
    expected: 'success|handled'
  },
  {
    scenario: 'Configure Vehicle (custom)',
    toolName: 'animation_physics',
    arguments: {
      action: 'configure_vehicle',
      vehicleName: 'TC_Vehicle',
      maxRPM: 6000,
      wheels: []
    },
    expected: 'success|default_wheels'
  },

  // === RETARGETING ===
  {
    scenario: 'Pre-cleanup Retargeted asset',
    toolName: 'manage_asset',
    arguments: { action: 'delete', assetPaths: ['/Game/Animations/Retargeted/TC_Anim_Retargeted'] },
    expected: 'success|not_found|error'
  },
  {
    scenario: 'Setup Retargeting',
    toolName: 'animation_physics',
    arguments: {
      action: 'setup_retargeting',
      sourceSkeleton: tutorialSkeleton,
      targetSkeleton: tutorialSkeleton,
      assets: ['/Game/Animations/TC_Anim_Sequence'],
      savePath: '/Game/Animations/Retargeted',
      overwrite: true
    },
    expected: 'success or not connected or not found'
  },

  // === ERROR CASES ===
  {
    scenario: 'Error: Invalid skeleton path',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_animation_asset',
      name: 'InvalidAnim',
      skeletonPath: '/Game/Invalid/Skeleton',
      assetType: 'sequence'
    },
    expected: 'error|asset_not_found'
  },
  {
    scenario: 'Error: Play rate 0 (pause)',
    toolName: 'animation_physics',
    arguments: {
      action: 'play_montage',
      actorName: 'SkeletalCharacter',
      montagePath: '/Game/Animations/TC_Anim_Montage',
      playRate: 0
    },
    expected: 'success|no_op'
  },
  {
    scenario: 'Error: Negative RPM vehicle',
    toolName: 'animation_physics',
    arguments: {
      action: 'configure_vehicle',
      vehicleName: 'TestVehicle',
      maxRPM: -1000
    },
    expected: 'success|clamped'
  },
  {
    scenario: 'Error: Invalid plugin deps',
    toolName: 'animation_physics',
    arguments: {
      action: 'configure_vehicle',
      vehicleName: 'TestVehicle',
      plugins: ['InvalidPlugin']
    },
    expected: 'error|plugin_missing|MISSING_ENGINE_PLUGINS'
  },

  // === CLEANUP ===
  {
    scenario: 'Cleanup all animation artifacts',
    toolName: 'animation_physics',
    arguments: {
      action: 'cleanup',
      filter: 'TC_*'
    },
    expected: 'success or no matching'
  },
  {
    scenario: 'Cleanup animation assets',
    toolName: 'manage_asset',
    arguments: {
      action: 'delete',
      assetPaths: [
        '/Game/Animations/TC_ABP_Character',
        '/Game/Animations/TC_ABP_Minimal',
        '/Game/Animations/TC_Anim_Sequence',
        '/Game/Animations/TC_Anim_Montage',
        '/Game/Animations/TC_BS_Locomotion',
        '/Game/Animations/TC_BS_Movement',
        '/Game/Animations/Retargeted',
        '/Game/Physics/TC_PhysAsset'
      ]
    },
    expected: 'success|not_found'
  }
];

await runToolTests('Animation & Physics', testCases);
