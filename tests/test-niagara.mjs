#!/usr/bin/env node
/**
 * Condensed Effects & Visual Test Suite (15 cases)
 * Tool: manage_effect
 */

import { runToolTests } from './test-runner.mjs';

const systemPath = '/Game/Effects/Niagara/TC_System';

const testCases = [
  // 1. Create real Niagara System asset first
  { scenario: 'Create real Niagara System', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'TC_System', template: 'Fountain' }, expected: 'success - system created' },

  { scenario: 'Spawn basic particle (engine)', toolName: 'manage_effect', arguments: { action: 'particle', preset: '/Engine/BasicShapes/Cube', location: { x: 0, y: 0, z: 100 } }, expected: 'success - particle spawned' },
  { scenario: 'Spawn smoke particle', toolName: 'manage_effect', arguments: { action: 'particle', preset: '/Engine/BasicShapes/Cube', location: { x: 200, y: 0, z: 100 } }, expected: 'success - smoke spawned' },
  { scenario: 'Spawn explosion particle', toolName: 'manage_effect', arguments: { action: 'particle', preset: '/Engine/BasicShapes/Cube', location: { x: 400, y: 0, z: 100 } }, expected: 'success - explosion spawned' },
  { scenario: 'Create debug line', toolName: 'manage_effect', arguments: { action: 'debug_shape', shapeType: 'Line', start: { x: 0, y: 0, z: 0 }, end: { x: 1000, y: 0, z: 0 }, color: { r: 1, g: 0, b: 0, a: 1 } }, expected: 'success - debug drawn' },
  { scenario: 'Create debug box', toolName: 'manage_effect', arguments: { action: 'debug_shape', shapeType: 'Box', location: { x: 0, y: 0, z: 100 }, extent: { x: 100, y: 100, z: 100 } }, expected: 'success - debug box' },
  { scenario: 'Create debug sphere', toolName: 'manage_effect', arguments: { action: 'debug_shape', shapeType: 'Sphere', location: { x: 500, y: 500, z: 100 }, radius: 150 }, expected: 'success - debug sphere' },

  // Use the created system path
  { scenario: 'Spawn a simple Niagara system (safe path)', toolName: 'manage_effect', arguments: { action: 'niagara', systemPath: systemPath, location: { x: 0, y: 0, z: 200 }, autoActivate: true, name: 'TC_System_Actor' }, expected: 'success - niagara spawned', timeout: 180000 },
  { scenario: 'Spawn niagara with parameters (best-effort)', toolName: 'manage_effect', arguments: { action: 'niagara', systemPath: systemPath, parameters: { FloatParam: 1.0 }, location: { x: 100, y: 100, z: 200 } }, expected: 'success - niagara params applied' },
  { scenario: 'Create volumetric fog (lightweight)', toolName: 'manage_effect', arguments: { action: 'create_volumetric_fog', fogName: 'TC_Fog', bounds: { min: { x: -500, y: -500, z: 0 }, max: { x: 500, y: 500, z: 500 } }, density: 0.01, systemPath: systemPath }, expected: 'success - fog created' },
  { scenario: 'Create dynamic light pulse', toolName: 'manage_effect', arguments: { action: 'create_dynamic_light', lightName: 'TC_Pulse', lightType: 'Point', location: { x: 0, y: 0, z: 300 }, intensity: 2000, pulse: { enabled: true, frequency: 1.0 } }, expected: 'success - dynamic light created' },
  { scenario: 'Create particle trail simple', toolName: 'manage_effect', arguments: { action: 'create_particle_trail', trailName: 'TC_Trail', emitterCount: 2, particleCount: 50, lifetime: 2.0, systemPath: systemPath }, expected: 'success - particle trail created' },
  { scenario: 'Create environmental effect', toolName: 'manage_effect', arguments: { action: 'create_environment_effect', effectType: 'Leaves', count: 20, spawnArea: { min: { x: -500, y: -500, z: 0 }, max: { x: 500, y: 500, z: 200 } }, systemPath: systemPath }, expected: 'success - environment effect' },
  { scenario: 'Create impact effect (best-effort)', toolName: 'manage_effect', arguments: { action: 'create_impact_effect', surfaceType: 'Default', location: { x: 100, y: 100, z: 100 }, effects: {}, systemPath: systemPath }, expected: 'success - impact effect' },
  { scenario: 'Create niagara ribbon (best-effort)', toolName: 'manage_effect', arguments: { action: 'create_niagara_ribbon', ribbonName: 'TC_Ribbon', startPoint: { x: -200, y: 0, z: 200 }, endPoint: { x: 200, y: 0, z: 200 }, width: 10, segments: 10, systemPath: systemPath }, expected: 'success - ribbon created' },

  { scenario: 'Cleanup effect actors', toolName: 'manage_effect', arguments: { action: 'cleanup', filter: 'TC_' }, expected: 'success - cleanup performed' },
  { scenario: 'Niagara - Create System (redundant check)', toolName: 'manage_effect', arguments: { action: 'niagara', systemPath: systemPath, location: { x: 1000, y: 0, z: 100 }, autoActivate: false }, expected: 'success' },
  { scenario: 'Niagara - Override Parameter', toolName: 'manage_effect', arguments: { action: 'set_niagara_parameter', systemName: 'TC_System', parameterName: 'User.FloatParam', parameterType: 'Float', value: 2.5, isUserParameter: true }, expected: 'error' },
  { scenario: 'Niagara - Activate', toolName: 'manage_effect', arguments: { action: 'activate_effect', actorName: 'TC_System' }, expected: 'error' },
  {
    scenario: "Error: Invalid Niagara system",
    toolName: "manage_effect",
    arguments: { action: "niagara", systemPath: "/Invalid/System" },
    expected: "SYSTEM_NOT_FOUND"
  },
  {
    scenario: "Edge: Scale 0",
    toolName: "manage_effect",
    arguments: { action: "niagara", systemPath: systemPath, scale: 0 },
    expected: "success"
  },
  {
    scenario: "Border: Invalid param type",
    toolName: "manage_effect",
    arguments: { action: "set_niagara_parameter", systemName: "Test", parameterName: "FloatParam", parameterType: "Invalid", value: 1 },
    expected: "error"
  },
  {
    scenario: "Edge: Empty filter cleanup",
    toolName: "manage_effect",
    arguments: { action: "cleanup", filter: "" },
    expected: "success|no_op"
  },
  // --- New Test Cases (+20) ---
  {
    scenario: "Create Empty System",
    toolName: "manage_effect",
    arguments: { action: "create_niagara_system", name: "NS_Empty_TC", path: "/Game/Effects/Niagara" },
    expected: "success"
  },
  {
    scenario: "Create System from Emitters",
    toolName: "manage_effect",
    arguments: { action: "create_niagara_system", name: "NS_FromEmitters_TC", path: "/Game/Effects/Niagara", emitterAssets: ["/Engine/Niagara/Emitters/Fountain"] },
    expected: "success"
  },
  // SKIPPED: Add/Remove Emitter not implemented in C++ yet
  // {
  //   scenario: "Add Emitter to System",
  //   toolName: "manage_effect",
  //   arguments: { action: "add_emitter", systemPath: "/Game/Effects/Niagara/NS_Empty_TC", emitterPath: "/Engine/Niagara/Emitters/OmniDirectionalBurst" },
  //   expected: "success"
  // },
  // {
  //   scenario: "Remove Emitter from System",
  //   toolName: "manage_effect",
  //   arguments: { action: "remove_emitter", systemPath: "/Game/Effects/Niagara/NS_Empty_TC", emitterName: "OmniDirectionalBurst" },
  //   expected: "success"
  // },
  {
    scenario: "Re-spawn TC_System_Actor for parameter tests",
    toolName: "manage_effect",
    arguments: { action: "niagara", systemPath: systemPath, location: { x: 0, y: 0, z: 200 }, autoActivate: true, name: "TC_System_Actor" },
    expected: "success"
  },
  {
    scenario: "Set Float Parameter",
    toolName: "manage_effect",
    arguments: { action: "set_niagara_parameter", actorName: "TC_System_Actor", parameterName: "User.SpawnRate", value: 50.0, type: "float" },
    expected: "success"
  },
  {
    scenario: "Set Color Parameter",
    toolName: "manage_effect",
    arguments: { action: "set_niagara_parameter", actorName: "TC_System_Actor", parameterName: "User.Color", value: [1.0, 0.0, 0.0, 1.0], type: "color" },
    expected: "success"
  },
  /*
  {
    scenario: "Set Vector Parameter",
    toolName: "manage_effect",
    arguments: { action: "set_niagara_parameter", actorName: "TC_System_Actor", parameterName: "User.Position", value: [100.0, 0.0, 0.0], type: "vector" },
    expected: "success"
  },
  */
  {
    scenario: "Set Bool Parameter",
    toolName: "manage_effect",
    arguments: { action: "set_niagara_parameter", actorName: "TC_System_Actor", parameterName: "User.Enabled", value: true, type: "bool" },
    expected: "success"
  },
  {
    scenario: "Activate System Actor",
    toolName: "manage_effect",
    arguments: { action: "activate", actorName: "TC_System_Actor" },
    expected: "success"
  },
  {
    scenario: "Deactivate System Actor",
    toolName: "manage_effect",
    arguments: { action: "deactivate", actorName: "TC_System_Actor" },
    expected: "success"
  },
  {
    scenario: "Reset System Simulation",
    toolName: "manage_effect",
    arguments: { action: "reset", actorName: "TC_System_Actor" },
    expected: "success"
  },
  {
    scenario: "Advance Simulation",
    toolName: "manage_effect",
    arguments: { action: "advance_simulation", actorName: "TC_System_Actor", deltaTime: 0.1, steps: 1 },
    expected: "success"
  },
  {
    scenario: "Spawn at absolute location",
    toolName: "manage_effect",
    arguments: { action: "niagara", systemPath: systemPath, location: { x: 500, y: 500, z: 500 }, rotation: { pitch: 0, yaw: 90, roll: 0 }, scale: { x: 2, y: 2, z: 2 } },
    expected: "success"
  },
  {
    scenario: "Spawn attached to actor",
    toolName: "manage_effect",
    arguments: { action: "niagara", systemPath: systemPath, attachToActor: "TC_Cube", attachPoint: "RootComponent" },
    expected: "success"
  },
  {
    scenario: "Error: Add invalid emitter",
    toolName: "manage_effect",
    arguments: { action: "add_emitter", systemPath: systemPath, emitterPath: "/Game/InvalidEmitter" },
    expected: "error|not_found"
  },
  {
    scenario: "Error: Set invalid parameter type",
    toolName: "manage_effect",
    arguments: { action: "set_niagara_parameter", systemPath: systemPath, parameterName: "User.SpawnRate", value: "invalid", type: "float" },
    expected: "error"
  },
  {
    scenario: "Edge: Advance simulation 0 time",
    toolName: "manage_effect",
    arguments: { action: "advance_simulation", actorName: "TC_System_Actor", deltaTime: 0 },
    expected: "success|no_op"
  },
  {
    scenario: "Cleanup Niagara Tests",
    toolName: "manage_asset",
    arguments: {
      action: "delete", assetPaths: [
        "/Game/Effects/Niagara/NS_Empty_TC",
        "/Game/Effects/Niagara/NS_FromEmitters_TC"
      ]
    },
    expected: "success"
  }
];

await runToolTests('Effects & Visual', testCases);
