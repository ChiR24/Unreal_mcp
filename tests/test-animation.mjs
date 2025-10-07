#!/usr/bin/env node
/**
 * Animation & Physics Test Suite - Expanded
 * Tool: animation_physics
 * Actions: create_animation_bp, play_montage, setup_ragdoll, configure_vehicle
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Create animation blueprint',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_animation_bp',
      name: 'ABP_Character',
      skeletonPath: '/Game/Characters/SK_Mannequin_Skeleton',
      savePath: '/Game/Animations'
    },
    expected: 'success - animation BP created'
  },
  {
    scenario: 'Create animation BP for custom skeleton',
    toolName: 'animation_physics',
    arguments: {
      action: 'create_animation_bp',
      name: 'ABP_Monster',
      skeletonPath: '/Game/Characters/SK_Monster_Skeleton',
      savePath: '/Game/Animations/Monster'
    },
    expected: 'success - monster animation BP created'
  },
  {
    scenario: 'Play attack montage',
    toolName: 'animation_physics',
    arguments: {
      action: 'play_montage',
      actorName: 'Character_0',
      montagePath: '/Game/Animations/AM_Attack',
      playRate: 1.0
    },
    expected: 'success - montage playing'
  },
  {
    scenario: 'Play death montage with blend',
    toolName: 'animation_physics',
    arguments: {
      action: 'play_montage',
      actorName: 'Character_0',
      montagePath: '/Game/Animations/AM_Death',
      playRate: 0.8,
      blendInTime: 0.2
    },
    expected: 'success - death animation started'
  },
  {
    scenario: 'Play jump montage fast',
    toolName: 'animation_physics',
    arguments: {
      action: 'play_montage',
      actorName: 'Character_0',
      montagePath: '/Game/Animations/AM_Jump',
      playRate: 1.5
    },
    expected: 'success - jump montage playing'
  },
  {
    scenario: 'Setup full ragdoll',
    toolName: 'animation_physics',
    arguments: {
      action: 'setup_ragdoll',
      actorName: 'Character1',
      physicsAssetName: 'PA_Mannequin',
      blendWeight: 1.0
    },
    expected: 'success - ragdoll enabled'
  },
  {
    scenario: 'Setup partial ragdoll blend',
    toolName: 'animation_physics',
    arguments: {
      action: 'setup_ragdoll',
      actorName: 'Character1',
      physicsAssetName: 'PA_Mannequin',
      blendWeight: 0.5
    },
    expected: 'success - ragdoll blended'
  },
  {
    scenario: 'Configure car vehicle physics',
    toolName: 'animation_physics',
    arguments: {
      action: 'configure_vehicle',
      vehicleName: 'BP_VehicleCar',
      vehicleType: 'Car',
      wheels: [
        { name: 'FrontLeft', radius: 35, width: 25, mass: 20 },
        { name: 'FrontRight', radius: 35, width: 25, mass: 20 }
      ]
    },
    expected: 'success - vehicle configured'
  },
  {
    scenario: 'Configure motorcycle vehicle',
    toolName: 'animation_physics',
    arguments: {
      action: 'configure_vehicle',
      vehicleName: 'BP_Motorcycle',
      vehicleType: 'Motorcycle',
      wheels: [
        { name: 'Front', radius: 30, width: 15, mass: 15 },
        { name: 'Rear', radius: 30, width: 20, mass: 18 }
      ]
    },
    expected: 'success - motorcycle configured'
  },
  {
    scenario: 'Configure tank vehicle',
    toolName: 'animation_physics',
    arguments: {
      action: 'configure_vehicle',
      vehicleName: 'BP_Tank',
      vehicleType: 'Tracked',
      wheels: []
    },
    expected: 'success - tank configured'
  }
];

await runToolTests('Animation & Physics', testCases);
