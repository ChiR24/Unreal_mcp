#!/usr/bin/env node
/**
 * Comprehensive Inspect Test Suite
 * Tool: inspect
 * Coverage: All 17 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // === BOOTSTRAP ===
  {
    scenario: 'Bootstrap - spawn InspectActor',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'Inspect_A', location: { x: 0, y: 0, z: 200 } },
    expected: 'success - bootstrap created'
  },
  {
    scenario: 'Bootstrap - spawn second actor',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: 'Inspect_B', location: { x: 100, y: 0, z: 200 } },
    expected: 'success'
  },

  // === INSPECT OBJECT ===
  {
    scenario: 'Inspect actor by name',
    toolName: 'inspect',
    arguments: { action: 'inspect_object', name: 'Inspect_A' },
    expected: 'success - object inspected'
  },
  {
    scenario: 'Inspect actor by path',
    toolName: 'inspect',
    arguments: { action: 'inspect_object', objectPath: '/Engine/BasicShapes/Cube' },
    expected: 'success'
  },

  // === INSPECT CLASS ===
  {
    scenario: 'Inspect class (StaticMeshActor)',
    toolName: 'inspect',
    arguments: { action: 'inspect_class', className: 'StaticMeshActor' },
    expected: 'success'
  },
  {
    scenario: 'Inspect class (Actor)',
    toolName: 'inspect',
    arguments: { action: 'inspect_class', className: 'Actor' },
    expected: 'success'
  },
  {
    scenario: 'Inspect class by path',
    toolName: 'inspect',
    arguments: { action: 'inspect_class', classPath: '/Script/Engine.Actor' },
    expected: 'success'
  },

  // === LIST OBJECTS ===
  {
    scenario: 'List objects (all)',
    toolName: 'inspect',
    arguments: { action: 'list_objects' },
    expected: 'success'
  },
  {
    scenario: 'List objects (filtered)',
    toolName: 'inspect',
    arguments: { action: 'list_objects', filter: 'Inspect_' },
    expected: 'success'
  },

  // === FIND BY CLASS ===
  {
    scenario: 'Find by class (StaticMeshActor)',
    toolName: 'inspect',
    arguments: { action: 'find_by_class', className: 'StaticMeshActor' },
    expected: 'success'
  },
  {
    scenario: 'Find by class (PointLight)',
    toolName: 'inspect',
    arguments: { action: 'find_by_class', className: 'PointLight' },
    expected: 'success'
  },

  // === GET/SET PROPERTY ===
  {
    scenario: 'Get actor location property',
    toolName: 'inspect',
    arguments: { action: 'get_property', name: 'Inspect_A', propertyName: 'ActorLocation' },
    expected: 'success - property retrieved'
  },
  {
    scenario: 'Get actor hidden property',
    toolName: 'inspect',
    arguments: { action: 'get_property', name: 'Inspect_A', propertyName: 'bHidden' },
    expected: 'success'
  },
  {
    scenario: 'Set actor hidden property (true)',
    toolName: 'inspect',
    arguments: { action: 'set_property', name: 'Inspect_A', propertyName: 'bHidden', value: true },
    expected: 'success - property set'
  },
  {
    scenario: 'Set actor hidden property (false)',
    toolName: 'inspect',
    arguments: { action: 'set_property', name: 'Inspect_A', propertyName: 'bHidden', value: false },
    expected: 'success'
  },

  // === GET COMPONENTS ===
  {
    scenario: 'Get components on actor',
    toolName: 'inspect',
    arguments: { action: 'get_components', name: 'Inspect_A' },
    expected: 'success - components returned'
  },
  {
    scenario: 'Get components on second actor',
    toolName: 'inspect',
    arguments: { action: 'get_components', name: 'Inspect_B' },
    expected: 'success'
  },

  // === GET/SET COMPONENT PROPERTY ===
  {
    scenario: 'Get component property (StaticMesh)',
    toolName: 'inspect',
    arguments: { action: 'get_component_property', name: 'Inspect_A', componentName: 'StaticMeshComponent', propertyName: 'StaticMesh' },
    expected: 'success - component property retrieved'
  },
  {
    scenario: 'Set component property (Mobility)',
    toolName: 'inspect',
    arguments: { action: 'set_component_property', name: 'Inspect_A', componentName: 'StaticMeshComponent', propertyName: 'Mobility', value: 'Movable' },
    expected: 'success - component property set'
  },
  {
    scenario: 'Get component property (Visibility)',
    toolName: 'inspect',
    arguments: { action: 'get_component_property', name: 'Inspect_A', componentName: 'StaticMeshComponent', propertyName: 'bVisible' },
    expected: 'success'
  },

  // === GET BOUNDING BOX ===
  {
    scenario: 'Get bounding box',
    toolName: 'inspect',
    arguments: { action: 'get_bounding_box', name: 'Inspect_A' },
    expected: 'success'
  },
  {
    scenario: 'Get bounding box (second actor)',
    toolName: 'inspect',
    arguments: { action: 'get_bounding_box', name: 'Inspect_B' },
    expected: 'success'
  },

  // === METADATA ===
  {
    scenario: 'Get actor metadata',
    toolName: 'inspect',
    arguments: { action: 'get_metadata', name: 'Inspect_A' },
    expected: 'success - metadata returned'
  },

  // === TAGS ===
  {
    scenario: 'Add tag to actor',
    toolName: 'inspect',
    arguments: { action: 'add_tag', name: 'Inspect_A', tag: 'InspectTC' },
    expected: 'success - tag added'
  },
  {
    scenario: 'Add second tag',
    toolName: 'inspect',
    arguments: { action: 'add_tag', name: 'Inspect_A', tag: 'TestTag' },
    expected: 'success'
  },
  {
    scenario: 'Find actors by tag',
    toolName: 'inspect',
    arguments: { action: 'find_by_tag', tag: 'InspectTC' },
    expected: 'success - actors found'
  },

  // === SNAPSHOTS ===
  {
    scenario: 'Create actor snapshot',
    toolName: 'inspect',
    arguments: { action: 'create_snapshot', name: 'Inspect_A', snapshotName: 'Inspect_A_Snap' },
    expected: 'success - snapshot created'
  },
  {
    scenario: 'Restore actor snapshot',
    toolName: 'inspect',
    arguments: { action: 'restore_snapshot', name: 'Inspect_A', snapshotName: 'Inspect_A_Snap' },
    expected: 'success - snapshot restored'
  },

  // === EXPORT ===
  {
    scenario: 'Export actor to JSON',
    toolName: 'inspect',
    arguments: { action: 'export', name: 'Inspect_A', format: 'JSON', outputPath: './tests/reports/inspect_actor_export.json' },
    expected: 'success - actor exported'
  },
  {
    scenario: 'Export second actor',
    toolName: 'inspect',
    arguments: { action: 'export', name: 'Inspect_B', format: 'JSON', outputPath: './tests/reports/inspect_actor_b_export.json' },
    expected: 'success'
  },

  // === DELETE OBJECT ===
  {
    scenario: 'Delete second actor',
    toolName: 'inspect',
    arguments: { action: 'delete_object', name: 'Inspect_B' },
    expected: 'success'
  },
  {
    scenario: 'Verify second actor deleted',
    toolName: 'inspect',
    arguments: { action: 'list_objects', filter: 'Inspect_B' },
    expected: 'success'
  },
  {
    scenario: 'Delete first actor',
    toolName: 'inspect',
    arguments: { action: 'delete_object', name: 'Inspect_A' },
    expected: 'success - actor deleted'
  },
  {
    scenario: 'Verify first actor deleted',
    toolName: 'inspect',
    arguments: { action: 'list_objects', filter: 'Inspect_A' },
    expected: 'success - deleted or not found'
  },

  // === ERROR CASES ===
  {
    scenario: 'Error: Invalid object path',
    toolName: 'inspect',
    arguments: { action: 'inspect_object', objectPath: '/Invalid/Path' },
    expected: 'not_found|OBJECT_NOT_FOUND'
  },
  {
    scenario: 'Error: Set invalid property',
    toolName: 'inspect',
    arguments: { action: 'set_property', name: 'NonExistentActor', propertyName: 'InvalidProp', value: 1 },
    expected: 'error|unknown_property|OBJECT_NOT_FOUND'
  },
  {
    scenario: 'Error: Find invalid class',
    toolName: 'inspect',
    arguments: { action: 'find_by_class', className: 'InvalidClass' },
    expected: 'no_results|success'
  },
  {
    scenario: 'Error: Get property non-existent actor',
    toolName: 'inspect',
    arguments: { action: 'get_property', name: 'NonExistentActor', propertyName: 'ActorLocation' },
    expected: 'error|not_found'
  },
  {
    scenario: 'Error: Get component non-existent',
    toolName: 'inspect',
    arguments: { action: 'get_component_property', name: 'Inspect_A', componentName: 'NonExistentComponent', propertyName: 'Value' },
    expected: 'error|not_found'
  },
  {
    scenario: 'Error: Get bounding box deleted actor',
    toolName: 'inspect',
    arguments: { action: 'get_bounding_box', name: 'Inspect_A' },
    expected: 'error|ACTOR_NOT_FOUND'
  },
  {
    scenario: 'Error: Create snapshot deleted actor',
    toolName: 'inspect',
    arguments: { action: 'create_snapshot', name: 'Inspect_A', snapshotName: 'FailSnap' },
    expected: 'error|not_found'
  },

  // === EDGE CASES ===
  {
    scenario: 'Edge: Empty tag add',
    toolName: 'inspect',
    arguments: { action: 'add_tag', name: 'Inspect_A', tag: '' },
    expected: 'success|handled|error'
  },
  {
    scenario: 'Edge: Delete already deleted object',
    toolName: 'inspect',
    arguments: { action: 'delete_object', name: 'Inspect_A' },
    expected: 'error|NOT_FOUND'
  }
];

await runToolTests('Inspect', testCases);
