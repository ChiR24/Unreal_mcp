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
    scenario: 'Get actor transform',
    toolName: 'control_actor',
    arguments: { action: 'get_transform', actorName: 'TC_Cube' },
    expected: 'success - actor transform retrieved'
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
    scenario: 'Toggle actor visibility',
    toolName: 'control_actor',
    arguments: { action: 'set_visibility', actorName: 'TC_Cube', visible: false },
    expected: 'success - actor visibility updated'
  },
  {
    scenario: 'Duplicate actor',
    toolName: 'control_actor',
    arguments: { action: 'duplicate', actorName: 'TC_Cube', newName: 'TC_Cube_Copy', offset: { x: 50, y: 0, z: 0 } },
    expected: 'success - actor duplicated'
  },
  {
    scenario: 'Attach actor to another',
    toolName: 'control_actor',
    arguments: { action: 'attach', childActor: 'TC_Cube_Copy', parentActor: 'TC_Cube' },
    expected: 'success - actor attached'
  },
  {
    scenario: 'Detach actor',
    toolName: 'control_actor',
    arguments: { action: 'detach', actorName: 'TC_Cube_Copy' },
    expected: 'success - actor detached'
  },
  {
    scenario: 'Tag actor',
    toolName: 'control_actor',
    arguments: { action: 'add_tag', actorName: 'TC_Cube', tag: 'TC_Tag' },
    expected: 'success - actor tagged'
  },
  {
    scenario: 'Find actors by tag',
    toolName: 'control_actor',
    arguments: { action: 'find_by_tag', tag: 'TC_Tag' },
    expected: 'success - actors found by tag'
  },
  {
    scenario: 'Find actors by name',
    toolName: 'control_actor',
    arguments: { action: 'find_by_name', name: 'TC_Cube' },
    expected: 'success - actors found by name'
  },
  {
    scenario: 'Create snapshot for an actor',
    toolName: 'control_actor',
    arguments: { action: 'create_snapshot', actorName: 'TC_Cube', snapshotName: 'TC_Before' },
    expected: 'success - actor snapshot created'
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
    scenario: 'Delete by tag filter',
    toolName: 'control_actor',
    arguments: { action: 'delete_by_tag', tag: 'TC_Tag' },
    expected: 'success - actors deleted by tag'
  },
  // Real-World Scenario: Composite Actor (Parent-Child)
  {
    scenario: 'Composite Actor - Spawn Parent (Car)',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'TC_Car_Body', location: { x: 1000, y: 0, z: 100 } },
    expected: 'success'
  },
  {
    scenario: 'Composite Actor - Spawn Child (Wheel)',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cylinder', actorName: 'TC_Car_Wheel', location: { x: 1050, y: 50, z: 50 } },
    expected: 'success'
  },
  {
    scenario: 'Composite Actor - Attach Wheel to Body',
    toolName: 'control_actor',
    arguments: { action: 'attach', childActor: 'TC_Car_Wheel', parentActor: 'TC_Car_Body' },
    expected: 'success'
  },
  {
    scenario: 'Composite Actor - Move Parent',
    toolName: 'control_actor',
    arguments: { action: 'set_transform', actorName: 'TC_Car_Body', location: { x: 1200, y: 0, z: 100 } },
    expected: 'success'
  },

  // Real-World Scenario: Batch Operations
  {
    scenario: 'Batch Ops - Spawn Crowd 1',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'TC_Crowd_1', location: { x: 2000, y: 0, z: 0 } },
    expected: 'success'
  },
  {
    scenario: 'Batch Ops - Spawn Crowd 2',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'TC_Crowd_2', location: { x: 2100, y: 0, z: 0 } },
    expected: 'success'
  },
  {
    scenario: 'Batch Ops - Tag Crowd',
    toolName: 'control_actor',
    arguments: { action: 'add_tag', actorName: 'TC_Crowd_1', tag: 'TC_Crowd' },
    expected: 'success'
  },
  {
    scenario: 'Batch Ops - Tag Crowd 2',
    toolName: 'control_actor',
    arguments: { action: 'add_tag', actorName: 'TC_Crowd_2', tag: 'TC_Crowd' },
    expected: 'success'
  },
  {
    scenario: 'Batch Ops - Find by Tag',
    toolName: 'control_actor',
    arguments: { action: 'find_by_tag', tag: 'TC_Crowd' },
    expected: 'success'
  },

  // Cleanup
  {
    scenario: 'Cleanup - delete test actors',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorNames: ['TC_Cube_Copy', 'TC_Physics', 'TC_PointLight', 'TC_BP_Instance', 'TC_Camera', 'TC_Car_Body', 'TC_Car_Wheel', 'TC_Crowd_1', 'TC_Crowd_2'] },
    expected: 'success - actors deleted'
  },
  {
    scenario: 'Cleanup - delete test blueprint',
    toolName: 'manage_asset',
    arguments: { action: 'delete_assets', paths: ['/Game/Blueprints/BP_TestActor'] },
    expected: 'success - test blueprint deleted'
  },
  {
    scenario: "Error: Invalid action",
    toolName: "control_actor",
    arguments: { action: "invalid_action" },
    expected: "error|validation|unknown_action"
  },
  {
    scenario: "Error: Spawn invalid class",
    toolName: "control_actor",
    arguments: { action: "spawn", classPath: "InvalidClassDoesNotExist", actorName: "TestInvalid" },
    expected: "error|class_not_found"
  },
  {
    scenario: "Edge: Zero force (no-op)",
    toolName: "control_actor",
    arguments: { action: "apply_force", actorName: "TC_Physics", force: { x: 0, y: 0, z: 0 } },
    expected: "success"
  },
  {
    scenario: "Border: Extreme location",
    toolName: "control_actor",
    arguments: { action: "spawn", classPath: "/Engine/BasicShapes/Cube", actorName: "TC_Extreme", location: { x: 1000000, y: 1000000, z: 1000000 } },
    expected: "success"
  },
  {
    scenario: "Error: Non-existent actor",
    toolName: "control_actor",
    arguments: { action: "delete", actorName: "NonExistentActor" },
    expected: "not_found|error"
  },
  {
    scenario: "Edge: Empty tag array (find)",
    toolName: "control_actor",
    arguments: { action: "find_by_tag", tag: "" },
    expected: "success|empty"
  },
  {
    scenario: "Timeout test (short timeout fail)",
    toolName: "control_actor",
    arguments: { action: "spawn", classPath: "StaticMeshActor", timeoutMs: 100 },
    expected: "timeout|error"
  },
  {
    scenario: "Batch delete empty array",
    toolName: "control_actor",
    arguments: { action: "delete", actorNames: [] },
    expected: "success|no_op"
  }
];

await runToolTests('Actor Control', testCases);
