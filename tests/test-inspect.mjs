#!/usr/bin/env node
/**
 * Condensed Inspect Test Suite (15 cases) — safe for real Editor runs.
 * Tools used: control_actor (bootstrap), inspect
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // 1. Bootstrap: spawn a mesh actor for inspection (using control_actor instead of execute_python)
  { scenario: 'Bootstrap - spawn InspectActor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'Inspect_A', location: { x: 0, y: 0, z: 200 } }, expected: 'success - bootstrap created' },
  // 2. Inspect actor by name
  { scenario: 'Inspect actor by name', toolName: 'inspect', arguments: { action: 'inspect_object', name: 'Inspect_A' }, expected: 'success - object inspected' },
  // 3. Get a property from the actor
  { scenario: 'Get actor location property', toolName: 'inspect', arguments: { action: 'get_property', name: 'Inspect_A', propertyPath: 'ActorLocation' }, expected: 'success - property retrieved' },
  // 4. Set a property on the actor
  { scenario: 'Set actor hidden property', toolName: 'inspect', arguments: { action: 'set_property', name: 'Inspect_A', propertyPath: 'bHidden', value: true }, expected: 'success - property set' },
  // 5. Inspect component list
  { scenario: 'Get components on actor', toolName: 'inspect', arguments: { action: 'get_components', name: 'Inspect_A' }, expected: 'success - components returned' },
  // 6. Get component property
  { scenario: 'Get static mesh component mesh', toolName: 'inspect', arguments: { action: 'get_component_property', name: 'Inspect_A', componentName: 'StaticMeshComponent', propertyPath: 'StaticMesh' }, expected: 'success - component property retrieved' },
  // 7. Set component mobility
  { scenario: 'Set component mobility', toolName: 'inspect', arguments: { action: 'set_component_property', name: 'Inspect_A', componentName: 'StaticMeshComponent', propertyPath: 'Mobility', value: 'Movable' }, expected: 'success - component property set' },
  // 8. Query actor metadata (best-effort)
  { scenario: 'Get actor metadata', toolName: 'inspect', arguments: { action: 'get_metadata', name: 'Inspect_A' }, expected: 'success - metadata returned' },
  // 9. Add a tag to the actor
  { scenario: 'Add tag to actor', toolName: 'inspect', arguments: { action: 'add_tag', name: 'Inspect_A', tag: 'InspectTC' }, expected: 'success - tag added' },
  // 10. Find actors by tag
  { scenario: 'Find actors by tag', toolName: 'inspect', arguments: { action: 'find_by_tag', tag: 'InspectTC' }, expected: 'success - actors found' },
  // 11. Save actor state (best-effort)
  { scenario: 'Save actor snapshot', toolName: 'inspect', arguments: { action: 'create_snapshot', name: 'Inspect_A', snapshotName: 'Inspect_A_Snap' }, expected: 'success - snapshot created' },
  // 12. Restore actor state
  { scenario: 'Restore actor snapshot', toolName: 'inspect', arguments: { action: 'restore_snapshot', name: 'Inspect_A', snapshotName: 'Inspect_A_Snap' }, expected: 'success - snapshot restored' },
  // 13. Export actor to JSON
  { scenario: 'Export actor', toolName: 'inspect', arguments: { action: 'export', name: 'Inspect_A', format: 'JSON', outputPath: './tests/reports/inspect_actor_export.json' }, expected: 'success - actor exported' },
  // 14. Delete test actor via inspect
  { scenario: 'Delete actor', toolName: 'inspect', arguments: { action: 'delete_object', name: 'Inspect_A' }, expected: 'success - actor deleted' },
  // 15. Verify deletion with a simple list
  { scenario: 'Verify actor deleted', toolName: 'inspect', arguments: { action: 'list_objects', filter: 'Inspect_A' }, expected: 'success - deleted or not found' },
  // Additional
  { scenario: 'List objects', toolName: 'inspect', arguments: { action: 'list_objects', filter: 'Inspect_' }, expected: 'success or handled' },
  { scenario: 'Find by class', toolName: 'inspect', arguments: { action: 'find_by_class', className: 'StaticMeshActor' }, expected: 'success or handled' },
  { scenario: 'Get bounding box', toolName: 'inspect', arguments: { action: 'get_bounding_box', name: 'Inspect_A' }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'Inspect blueprint class (best-effort)', toolName: 'inspect', arguments: { action: 'inspect_class', classPath: '/Game/Blueprints/BP_Auto' }, expected: 'success or handled' },
  { scenario: 'Verify delete again', toolName: 'inspect', arguments: { action: 'delete_object', name: 'Inspect_A' }, expected: 'error|NOT_FOUND' },
  {
    scenario: "Error: Invalid object path",
    toolName: "inspect",
    arguments: { action: "inspect_object", objectPath: "/Invalid/Path" },
    expected: "not_found"
  },
  {
    scenario: "Edge: Set invalid property",
    toolName: "inspect",
    arguments: { action: "set_property", objectPath: "/Valid", propertyName: "InvalidProp", value: 1 },
    expected: "error|unknown_property"
  },
  {
    scenario: "Border: Empty tag add",
    toolName: "inspect",
    arguments: { action: "add_tag", objectPath: "/Valid", tag: "" },
    expected: "success|handled"
  },
  {
    scenario: "Error: Find invalid class",
    toolName: "inspect",
    arguments: { action: "find_by_class", className: "InvalidClass" },
    expected: "no_results|success"
  }
];

await runToolTests('Inspect', testCases);
