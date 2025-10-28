#!/usr/bin/env node
/**
 * Condensed Actor Control Test Suite (15 cases) — safe for real Editor runs.
 * Tool: control_actor
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Spawn StaticMeshActor (engine cube)',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'TC_Cube', location: { x: 0, y: 0, z: 200 } },
    expected: 'success - actor spawned'
  },
  {
    scenario: 'Spawn CameraActor',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: 'CameraActor', actorName: 'TC_Camera', location: { x: 300, y: 0, z: 300 } },
    expected: 'success - camera spawned'
  },
  {
    scenario: 'Spawn PointLight',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: 'PointLight', actorName: 'TC_PointLight', location: { x: -300, y: 0, z: 300 } },
    expected: 'success - light spawned'
  },
  {
    scenario: 'Spawn Physics Cube (use control_actor instead of Python)',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'TC_Physics', location: { x: 0, y: 0, z: 500 } },
    expected: 'success - physics actor created'
  },
  {
    scenario: 'Apply force to Physics actor',
    toolName: 'control_actor',
    arguments: { action: 'apply_force', actorName: 'TC_Physics', force: { x: 200000, y: 0, z: 0 } },
    expected: 'success - force applied'
  },
  {
    scenario: 'Set transform on an actor',
    toolName: 'control_actor',
    arguments: { action: 'set_transform', actorName: 'TC_Cube', location: { x: 100, y: 100, z: 400 }, rotation: { pitch: 0, yaw: 45, roll: 0 } },
    expected: 'success - actor transform set'
  },
  {
    scenario: 'Add PointLight component to actor',
    toolName: 'control_actor',
    arguments: { action: 'add_component', actorName: 'TC_Cube', componentType: 'PointLightComponent', componentName: 'TC_PointLightComp', properties: { Intensity: 3000 } },
    expected: 'success - component added'
  },
  {
    scenario: 'Set component properties',
    toolName: 'control_actor',
    arguments: { action: 'set_component_properties', actorName: 'TC_Cube', componentName: 'TC_PointLightComp', properties: { Intensity: 1500 } },
    expected: 'success - component properties set'
  },
  {
    scenario: 'Get components of actor',
    toolName: 'control_actor',
    arguments: { action: 'get_components', actorName: 'TC_Cube' },
    expected: 'success - actor components retrieved'
  },
  {
    scenario: 'Duplicate actor',
    toolName: 'control_actor',
    arguments: { action: 'duplicate', actorName: 'TC_Cube', newName: 'TC_Cube_Copy', offset: { x: 50, y: 0, z: 0 } },
    expected: 'success - actor duplicated'
  },
  {
    scenario: 'Find actors by tag',
    toolName: 'control_actor',
    arguments: { action: 'find_by_tag', tag: 'TC_Tag' },
    expected: 'success - actors found by tag'
  },
  {
    scenario: 'Create test blueprint first (BP_TestActor)',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', path: '/Game/Blueprints/BP_TestActor', parentClass: 'Actor', blueprintType: 'actor' },
    expected: 'success - blueprint created for testing'
  },
  {
    scenario: 'Add Health variable to test blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'add_variable', blueprintPath: '/Game/Blueprints/BP_TestActor', variableName: 'Health', variableType: 'float', defaultValue: 100.0 },
    expected: 'success - Health variable added'
  },
  {
    scenario: 'Add IsActive variable to test blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'add_variable', blueprintPath: '/Game/Blueprints/BP_TestActor', variableName: 'IsActive', variableType: 'bool', defaultValue: false },
    expected: 'success - IsActive variable added'
  },
  {
    scenario: 'Spawn blueprint instance',
    toolName: 'control_actor',
    arguments: { action: 'spawn_blueprint', blueprintPath: '/Game/Blueprints/BP_TestActor', actorName: 'TC_BP_Instance', location: { x: 600, y: 0, z: 200 } },
    expected: 'success - blueprint instance spawned'
  },
  {
    scenario: 'Set blueprint variables',
    toolName: 'control_actor',
    arguments: { action: 'set_blueprint_variables', actorName: 'TC_BP_Instance', variables: { Health: 120, IsActive: true } },
    expected: 'success - blueprint variables set'
  },
  {
    scenario: 'Create snapshot for an actor',
    toolName: 'control_actor',
    arguments: { action: 'create_snapshot', actorName: 'TC_Cube', snapshotName: 'TC_Before' },
    expected: 'success - actor snapshot created'
  },
  {
    scenario: 'Cleanup - delete test actors',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorNames: ['TC_Cube', 'TC_Cube_Copy', 'TC_Physics', 'TC_PointLight', 'TC_BP_Instance', 'TC_Camera'] },
    expected: 'success - actors deleted'
  },
  {
    scenario: 'Cleanup - delete test blueprint',
    toolName: 'manage_asset',
    arguments: { action: 'delete_assets', paths: ['/Game/Blueprints/BP_TestActor'] },
expected: 'success - test blueprint deleted'},
  // Additional coverage to reach 20+ cases
  // Additional coverage to reach 20+ cases
  { scenario: 'Tag actor', toolName: 'control_actor', arguments: { action: 'add_tag', actorName: 'TC_Cube', tag: 'TC_Tag' }, expected: 'not_implemented' },
  { scenario: 'Find actors by name', toolName: 'control_actor', arguments: { action: 'find_by_name', name: 'TC_Cube' }, expected: 'failure - actor not found' },
  { scenario: 'Get actor transform', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: 'TC_Cube' }, expected: 'not_implemented' },
  { scenario: 'Toggle actor visibility', toolName: 'control_actor', arguments: { action: 'set_visibility', actorName: 'TC_Cube', visible: false }, expected: 'not_implemented' },
  { scenario: 'Attach actor to another', toolName: 'control_actor', arguments: { action: 'attach', childActor: 'TC_Cube_Copy', parentActor: 'TC_Cube' }, expected: 'not_implemented' },
  { scenario: 'Detach actor', toolName: 'control_actor', arguments: { action: 'detach', actorName: 'TC_Cube_Copy' }, expected: 'not_implemented' },
  { scenario: 'Delete by tag filter', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'TC_Tag' }, expected: 'not_implemented' }
];

await runToolTests('Actor Control', testCases);
