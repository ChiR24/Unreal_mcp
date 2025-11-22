#!/usr/bin/env node
/**
 * Condensed Effects & Visual Test Suite (15 cases)
 * Tool: create_effect
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Spawn basic particle (engine)', toolName: 'create_effect', arguments: { action: 'particle', preset: 'Default', location: { x: 0, y: 0, z: 100 } }, expected: 'success - particle spawned' },
  { scenario: 'Spawn smoke particle', toolName: 'create_effect', arguments: { action: 'particle', preset: 'Smoke', location: { x: 200, y: 0, z: 100 } }, expected: 'success - smoke spawned' },
  { scenario: 'Spawn explosion particle', toolName: 'create_effect', arguments: { action: 'particle', preset: 'Explosion', location: { x: 400, y: 0, z: 100 } }, expected: 'success - explosion spawned' },
  { scenario: 'Create debug line', toolName: 'create_effect', arguments: { action: 'debug_shape', shapeType: 'Line', start: { x: 0, y: 0, z: 0 }, end: { x: 1000, y: 0, z: 0 }, color: { r: 1, g: 0, b: 0, a: 1 } }, expected: 'success - debug drawn' },
  { scenario: 'Create debug box', toolName: 'create_effect', arguments: { action: 'debug_shape', shapeType: 'Box', location: { x: 0, y: 0, z: 100 }, extent: { x: 100, y: 100, z: 100 } }, expected: 'success - debug box' },
  { scenario: 'Create debug sphere', toolName: 'create_effect', arguments: { action: 'debug_shape', shapeType: 'Sphere', location: { x: 500, y: 500, z: 100 }, radius: 150 }, expected: 'success - debug sphere' },
  { scenario: 'Spawn a simple Niagara system (safe path)', toolName: 'create_effect', arguments: { action: 'niagara', systemPath: '/Engine/EngineNiagara.NI_Default', location: { x: 0, y: 0, z: 200 }, autoActivate: true }, expected: 'success - niagara spawned', timeout: 180000 },
  { scenario: 'Spawn niagara with parameters (best-effort)', toolName: 'create_effect', arguments: { action: 'niagara', systemPath: '/Engine/EngineNiagara.NI_Default', parameters: { FloatParam: 1.0 }, location: { x: 100, y: 100, z: 200 } }, expected: 'success - niagara params applied' },
  { scenario: 'Create volumetric fog (lightweight)', toolName: 'create_effect', arguments: { action: 'create_volumetric_fog', fogName: 'TC_Fog', bounds: { min: { x: -500, y: -500, z: 0 }, max: { x: 500, y: 500, z: 500 } }, density: 0.01 }, expected: 'success - fog created' },
  { scenario: 'Create dynamic light pulse', toolName: 'create_effect', arguments: { action: 'create_dynamic_light', lightName: 'TC_Pulse', lightType: 'Point', location: { x: 0, y: 0, z: 300 }, intensity: 2000, pulse: { enabled: true, frequency: 1.0 } }, expected: 'success - dynamic light created' },
  { scenario: 'Create particle trail simple', toolName: 'create_effect', arguments: { action: 'create_particle_trail', trailName: 'TC_Trail', emitterCount: 2, particleCount: 50, lifetime: 2.0 }, expected: 'success - particle trail created' },
  { scenario: 'Create environmental effect', toolName: 'create_effect', arguments: { action: 'create_environment_effect', effectType: 'Leaves', count: 20, spawnArea: { min: { x: -500, y: -500, z: 0 }, max: { x: 500, y: 500, z: 200 } } }, expected: 'success - environment effect' },
  { scenario: 'Create impact effect (best-effort)', toolName: 'create_effect', arguments: { action: 'create_impact_effect', surfaceType: 'Default', location: { x: 100, y: 100, z: 100 }, effects: {} }, expected: 'success - impact effect' },
  { scenario: 'Create niagara ribbon (best-effort)', toolName: 'create_effect', arguments: { action: 'create_niagara_ribbon', ribbonName: 'TC_Ribbon', startPoint: { x: -200, y: 0, z: 200 }, endPoint: { x: 200, y: 0, z: 200 }, width: 10, segments: 10 }, expected: 'success - ribbon created' },
  { scenario: 'Cleanup effect actors', toolName: 'create_effect', arguments: { action: 'cleanup', filter: 'TC_' }, expected: 'success - cleanup performed' },
  // Additional
  // Real-World Scenario: Particle System Config
  { scenario: 'Niagara - Create System', toolName: 'create_effect', arguments: { action: 'niagara', systemPath: '/Engine/EngineNiagara.NI_Default', location: { x: 1000, y: 0, z: 100 }, autoActivate: false }, expected: 'success' },
  { scenario: 'Niagara - Override Parameter', toolName: 'create_effect', arguments: { action: 'set_niagara_parameter', systemName: 'NI_Default', parameterName: 'User.FloatParam', parameterType: 'Float', value: 2.5, isUserParameter: true }, expected: 'success' },
  { scenario: 'Niagara - Activate', toolName: 'create_effect', arguments: { action: 'activate_effect', actorName: 'NI_Default' }, expected: 'success' },
  {
    scenario: "Error: Invalid Niagara system",
    toolName: "create_effect",
    arguments: { action: "niagara", systemPath: "/Invalid/System" },
    expected: "not_found"
  },
  {
    scenario: "Edge: Scale 0",
    toolName: "create_effect",
    arguments: { action: "niagara", systemPath: "/Valid", scale: 0 },
    expected: "success"
  },
  {
    scenario: "Border: Invalid param type",
    toolName: "create_effect",
    arguments: { action: "set_niagara_parameter", systemName: "Test", parameterName: "FloatParam", parameterType: "Invalid", value: 1 },
    expected: "error"
  },
  {
    scenario: "Edge: Empty filter cleanup",
    toolName: "create_effect",
    arguments: { action: "cleanup", filter: "" },
    expected: "success|no_op"
  }
];

await runToolTests('Effects & Visual', testCases);
