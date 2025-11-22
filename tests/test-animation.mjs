#!/usr/bin/env node
/**
 * Condensed Animation & Physics Test Suite (15 cases) — safe operations.
 * Tool: animation_physics
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Create simple animation blueprint (best-effort)', toolName: 'animation_physics', arguments: { action: 'create_animation_bp', name: 'ABP_TC', skeletonPath: '/Game/Characters/SK_Mannequin_Skeleton', savePath: '/Game/Animations' }, expected: 'success or skeleton not found' },
  { scenario: 'Play montage (no-op safe)', toolName: 'animation_physics', arguments: { action: 'play_montage', actorName: 'SkeletalCharacter', montagePath: '/Game/Animations/AM_Test', playRate: 1.0 }, expected: 'success or asset/actor not found' },
  { scenario: 'Setup ragdoll (best-effort)', toolName: 'animation_physics', arguments: { action: 'setup_ragdoll', skeletonPath: '/Game/Characters/SK_Mannequin_Skeleton', actorName: 'RagdollCharacter', physicsAssetName: 'PA_Mannequin', blendWeight: 1.0 }, expected: 'success or skeleton not found' },
  { scenario: 'Create blend space (safe)', toolName: 'animation_physics', arguments: { action: 'create_blend_space', blendSpaceName: 'TC_Locomotion', xAxis: 'Speed', yAxis: 'Direction', animations: [] }, expected: 'success - blend space created' },
  { scenario: 'Create state machine skeleton', toolName: 'animation_physics', arguments: { action: 'create_state_machine', machineName: 'TC_States', states: ['Idle', 'Run'], transitions: [] }, expected: 'success' },
  { scenario: 'Create simple IK setup', toolName: 'animation_physics', arguments: { action: 'setup_ik', actorName: 'Character', ikBones: ['LeftFoot', 'RightFoot'], enableFootPlacement: true }, expected: 'success' },
  { scenario: 'Create procedural animation', toolName: 'animation_physics', arguments: { action: 'create_procedural_anim', systemName: 'TC_Procedural', baseAnimation: '/Game/Animations/BasePose', modifiers: [] }, expected: 'success' },
  { scenario: 'Create animation blueprint minimal', toolName: 'animation_physics', arguments: { action: 'create_anim_blueprint', blueprintName: 'TC_AnimBP', parentClass: 'AnimInstance', targetSkeleton: '/Game/Characters/SK_Mannequin_Skeleton', stateMachines: [] }, expected: 'success or skeleton not found' },
  { scenario: 'Activate ragdoll trigger (no-op)', toolName: 'animation_physics', arguments: { action: 'activate_ragdoll', actorName: 'Character', triggerCondition: 'Health <= 0', blendTime: 0.2 }, expected: 'success' },
  { scenario: 'Create blend tree', toolName: 'animation_physics', arguments: { action: 'create_blend_tree', treeName: 'TC_UpperBody', blendType: 'Layered', basePose: '/Game/Animations/BasePose', additiveAnimations: [] }, expected: 'success' },
  { scenario: 'Retargeting sequence (best-effort)', toolName: 'animation_physics', arguments: { action: 'setup_retargeting', sourceSkeleton: '/Game/Characters/SK_Mannequin_Skeleton', targetSkeleton: '/Game/Characters/SK_Mannequin_Skeleton', assets: ['/Game/Animations/Idle'], savePath: '/Game/Animations/Retargeted', overwrite: true }, expected: 'success or not connected' },
  { scenario: 'Setup physics simulation (best-effort)', toolName: 'animation_physics', arguments: { action: 'setup_physics_simulation', skeletonPath: '/Game/Characters/SK_Mannequin_Skeleton', savePath: '/Game/Physics', physicsAssetName: 'PA_TC', assignToMesh: false }, expected: 'success or asset not found' },
  { scenario: 'Create simple animation asset', toolName: 'animation_physics', arguments: { action: 'create_animation_asset', name: 'ANIM_TC', skeletonPath: '/Game/Characters/SK_Mannequin_Skeleton', savePath: '/Game/Animations', assetType: 'sequence' }, expected: 'success or skeleton not found' },
  { scenario: 'Play and stop a short montage (best-effort)', toolName: 'animation_physics', arguments: { action: 'play_montage', actorName: 'SkeletalCharacter', montagePath: '/Game/Animations/AM_Test', playRate: 1.0 }, expected: 'success or asset/actor not found' },
  { scenario: 'Cleanup animation artifacts', toolName: 'animation_physics', arguments: { action: 'cleanup', artifacts: ['ABP_TC', 'TC_AnimBP', 'ANIM_TC'] }, expected: 'success' },
  // Real-World Scenario: Animation Setup
  { scenario: 'Anim Setup - Create Montage', toolName: 'animation_physics', arguments: { action: 'create_animation_asset', name: 'AM_Setup', skeletonPath: '/Game/Characters/SK_Mannequin_Skeleton', savePath: '/Game/Animations', assetType: 'montage' }, expected: 'success or skeleton not found' },
  { scenario: 'Anim Setup - Add Notify', toolName: 'animation_physics', arguments: { action: 'add_notify', assetPath: '/Game/Animations/AM_Setup', notifyName: 'PlaySound', time: 0.5 }, expected: 'success or not found' },

  // Cleanup
  { scenario: 'Cleanup - Delete Montage', toolName: 'manage_asset', arguments: { action: 'delete', assetPaths: ['/Game/Animations/AM_Setup'] }, expected: 'success' },

  // Expanded Test Cases
  {
    scenario: "Error: Invalid skeleton path",
    toolName: "animation_physics",
    arguments: { action: "create_animation_bp", skeletonPath: "/Invalid/Skeleton", name: "TestBP" },
    expected: "not_found|error"
  },
  {
    scenario: "Edge: Play rate 0 (pause)",
    toolName: "animation_physics",
    arguments: { action: "play_montage", actorName: "TestActor", montagePath: "/Valid/Montage", playRate: 0 },
    expected: "success"
  },
  {
    scenario: "Border: Negative RPM vehicle",
    toolName: "animation_physics",
    arguments: { action: "configure_vehicle", vehicleName: "TestVehicle", vehicleType: "Car", engine: { maxRPM: -1000 } },
    expected: "success|clamped"
  },
  {
    scenario: "Error: Empty wheels array",
    toolName: "animation_physics",
    arguments: { action: "configure_vehicle", vehicleName: "Test", vehicleType: "Car", wheels: [] },
    expected: "error|validation"
  },
  {
    scenario: "Edge: No plugin deps",
    toolName: "animation_physics",
    arguments: { action: "configure_vehicle", vehicleName: "Test", pluginDependencies: ["InvalidPlugin"] },
    expected: "error|plugin_missing"
  }
];

await runToolTests('Animation & Physics', testCases);
