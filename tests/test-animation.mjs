#!/usr/bin/env node
/**
 * Condensed Animation & Physics Test Suite (15 cases) — safe operations.
 * Tool: animation_physics
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Create simple animation blueprint (best-effort)', toolName: 'animation_physics', arguments: { action: 'create_animation_bp', name: 'ABP_TC', skeletonPath: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', savePath: '/Game/Animations' }, expected: 'success or not found' },
  { scenario: 'Play montage (no-op safe)', toolName: 'animation_physics', arguments: { action: 'play_montage', actorName: 'SkeletalCharacter', montagePath: '/Game/Animations/AM_Test', playRate: 1.0 }, expected: 'success or not found' },
  { scenario: 'Setup ragdoll (best-effort)', toolName: 'animation_physics', arguments: { action: 'setup_ragdoll', skeletonPath: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', actorName: 'RagdollCharacter', physicsAssetName: 'PA_Mannequin', blendWeight: 1.0 }, expected: 'success or not found' },
  { scenario: 'Create blend space (safe)', toolName: 'animation_physics', arguments: { action: 'create_blend_space', name: 'TC_Locomotion', skeletonPath: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', horizontalAxis: { name: 'Speed', minValue: 0, maxValue: 100 }, verticalAxis: { name: 'Direction', minValue: -180, maxValue: 180 }, animations: [] }, expected: 'success - blend space created' },
  { scenario: 'Create state machine skeleton', toolName: 'animation_physics', arguments: { action: 'create_state_machine', machineName: 'TC_States', states: [{ name: 'Idle' }, { name: 'Run' }], transitions: [] }, expected: 'success' },
  { scenario: 'Create simple IK setup', toolName: 'animation_physics', arguments: { action: 'setup_ik', actorName: 'Character', chain: { rootBone: 'pelvis', endBone: 'foot_l' }, effector: { targetActor: 'TargetActor' } }, expected: 'success' },
  { scenario: 'Create procedural animation', toolName: 'animation_physics', arguments: { action: 'create_procedural_anim', systemName: 'TC_Procedural', baseAnimation: '/Game/Animations/BasePose', settings: {} }, expected: 'success' },
  { scenario: 'Create animation blueprint minimal', toolName: 'animation_physics', arguments: { action: 'create_animation_bp', blueprintName: 'TC_AnimBP', parentClass: 'AnimInstance', targetSkeleton: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', stateMachines: [] }, expected: 'success or not found' },
  // { scenario: 'Activate ragdoll trigger (no-op)', toolName: 'animation_physics', arguments: { action: 'activate_ragdoll', actorName: 'Character', blendTime: 0.2 }, expected: 'success' },
  { scenario: 'Create blend tree', toolName: 'animation_physics', arguments: { action: 'create_blend_tree', treeName: 'TC_UpperBody', basePose: '/Game/Animations/BasePose', additiveAnimations: [] }, expected: 'success' },
  { scenario: 'Retargeting sequence (best-effort)', toolName: 'animation_physics', arguments: { action: 'setup_retargeting', sourceSkeleton: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', targetSkeleton: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', assets: ['/Game/Animations/Idle'], savePath: '/Game/Animations/Retargeted', overwrite: true }, expected: 'success or not connected or not found' },
  { scenario: 'Setup physics simulation (best-effort)', toolName: 'animation_physics', arguments: { action: 'setup_physics_simulation', skeletonPath: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', savePath: '/Game/Physics', physicsAssetName: 'PA_TC', assignToMesh: false }, expected: 'success or not found' },
  { scenario: 'Create simple animation asset', toolName: 'animation_physics', arguments: { action: 'create_animation_asset', name: 'ANIM_TC', skeletonPath: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', savePath: '/Game/Animations', assetType: 'sequence' }, expected: 'success or not found' },
  { scenario: 'Play and stop a short montage (best-effort)', toolName: 'animation_physics', arguments: { action: 'play_montage', actorName: 'SkeletalCharacter', montagePath: '/Game/Animations/AM_Test', playRate: 1.0 }, expected: 'success or not found' },
  { scenario: 'Cleanup animation artifacts', toolName: 'animation_physics', arguments: { action: 'cleanup', artifacts: ['ABP_TC', 'TC_AnimBP', 'ANIM_TC'] }, expected: 'success or no matching' },
  { scenario: 'Anim Setup - Create Montage', toolName: 'animation_physics', arguments: { action: 'create_animation_asset', name: 'AM_Setup', skeletonPath: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Skeleton', savePath: '/Game/Animations', assetType: 'montage' }, expected: 'success or not found' },
  { scenario: 'Anim Setup - Add Notify', toolName: 'animation_physics', arguments: { action: 'add_notify', assetPath: '/Game/Animations/AM_Setup', notifyName: 'AnimNotify_PlaySound', time: 0.5 }, expected: 'success or not found or AUTOMATION_BRIDGE_UNAVAILABLE' },
  { scenario: 'Cleanup - Delete Montage', toolName: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/Animations/AM_Setup' }, expected: 'success or not found' },
  { scenario: 'Error: Invalid skeleton path', toolName: 'animation_physics', arguments: { action: 'create_animation_asset', assetName: 'InvalidAnim', skeletonPath: '/Game/Invalid/Skeleton' }, expected: 'error|asset_not_found' },
  { scenario: 'Edge: Play rate 0 (pause)', toolName: 'animation_physics', arguments: { action: 'play_montage', assetPath: '/Game/Animations/AM_Setup', playRate: 0 }, expected: 'success|no_op' },
  { scenario: 'Border: Negative RPM vehicle', toolName: 'animation_physics', arguments: { action: 'configure_vehicle', vehicleName: 'TestVehicle', maxRPM: -1000 }, expected: 'success|clamped' },
  { scenario: 'Error: Empty wheels array', toolName: 'animation_physics', arguments: { action: 'configure_vehicle', vehicleName: 'TestVehicle', wheels: [] }, expected: 'success|default_wheels' },
  { scenario: 'Edge: No plugin deps', toolName: 'animation_physics', arguments: { action: 'configure_vehicle', vehicleName: 'TestVehicle', plugins: ['InvalidPlugin'] }, expected: 'error|plugin_missing|MISSING_ENGINE_PLUGINS' }
];

await runToolTests('Animation & Physics', testCases);
