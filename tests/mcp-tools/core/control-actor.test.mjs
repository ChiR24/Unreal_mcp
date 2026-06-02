#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

const MAIN_ACTOR = `MCP_CoreActor_${ts}`;
const DELETE_ACTOR = `MCP_DeleteActor_${ts}`;
const DESTROY_ACTOR = `MCP_DestroyActor_${ts}`;
const TAG_DELETE_ACTOR = `MCP_TagDeleteActor_${ts}`;
const DUPLICATE_ACTOR = `MCP_DuplicateActor_${ts}`;
const DUPLICATE_COPY = `MCP_DuplicateActorCopy_${ts}`;
const MESH_ACTOR = `MCP_MeshActor_${ts}`;
const PARENT_ACTOR = `MCP_ParentActor_${ts}`;
const CHILD_ACTOR = `MCP_ChildActor_${ts}`;
const BP_NAME = `BP_ControlActor_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;
const BP_ACTOR = `MCP_BlueprintActor_${ts}`;
const TAG = `MCPControlActorTag_${ts}`;
const DELETE_TAG = `MCPDeleteTag_${ts}`;
const COMPONENT_NAME = `MCPPointLight_${ts}`;

const cubeSpawn = (scenario, actorName, location) => ({
  scenario,
  toolName: 'control_actor',
  arguments: {
    action: 'spawn',
    classPath: '/Engine/BasicShapes/Cube',
    actorName,
    location,
  },
  expected: 'success|already exists',
});

const actorArgs = (action, extra = {}) => ({ action, actorName: MAIN_ACTOR, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create actor blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: BP_NAME, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  cubeSpawn('Setup: spawn main test actor', MAIN_ACTOR, { x: 0, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn delete test actor', DELETE_ACTOR, { x: 120, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn destroy test actor', DESTROY_ACTOR, { x: 240, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn tag-delete test actor', TAG_DELETE_ACTOR, { x: 360, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn duplicate test actor', DUPLICATE_ACTOR, { x: 480, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn attach parent actor', PARENT_ACTOR, { x: 600, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn attach child actor', CHILD_ACTOR, { x: 720, y: 0, z: 100 }),
  { scenario: 'Setup: tag actor for delete_by_tag', toolName: 'control_actor', arguments: { action: 'add_tag', actorName: TAG_DELETE_ACTOR, tag: DELETE_TAG }, expected: 'success|already exists' },

  // === SPAWN / DELETE ===
  { scenario: 'ACTION: spawn', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: `MCP_SpawnSphere_${ts}`, location: { x: 0, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_actor', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Engine/BasicShapes/Cylinder', actorName: `MCP_SpawnCylinder_${ts}`, location: { x: 120, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_actor with meshPath', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName: MESH_ACTOR, location: { x: 360, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_blueprint', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: BP_PATH, actorName: BP_ACTOR, location: { x: 240, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'DELETE: delete', toolName: 'control_actor', arguments: { action: 'delete', actorName: DELETE_ACTOR }, expected: 'success|not found' },
  { scenario: 'DELETE: destroy_actor', toolName: 'control_actor', arguments: { action: 'destroy_actor', actorName: DESTROY_ACTOR }, expected: 'success|not found' },
  { scenario: 'DELETE: delete_by_tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: DELETE_TAG }, expected: 'success|not found' },

  // === TRANSFORM / PHYSICS ===
  { scenario: 'ACTION: duplicate', toolName: 'control_actor', arguments: { action: 'duplicate', actorName: DUPLICATE_ACTOR, newName: DUPLICATE_COPY, offset: { x: 50, y: 0, z: 0 } }, expected: 'success|already exists' },
  { scenario: 'CONFIG: set_transform', toolName: 'control_actor', arguments: actorArgs('set_transform', { location: { x: 10, y: 20, z: 130 }, rotation: { x: 0, y: 0, z: 15 }, scale: { x: 1.1, y: 1.1, z: 1.1 } }), expected: 'success' },
  { scenario: 'ACTION: teleport_actor', toolName: 'control_actor', arguments: actorArgs('teleport_actor', { location: { x: 20, y: 30, z: 140 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_location', toolName: 'control_actor', arguments: actorArgs('set_actor_location', { location: { x: 30, y: 40, z: 150 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_rotation', toolName: 'control_actor', arguments: actorArgs('set_actor_rotation', { rotation: { x: 0, y: 45, z: 0 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_scale', toolName: 'control_actor', arguments: actorArgs('set_actor_scale', { scale: { x: 1.25, y: 1.25, z: 1.25 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_transform', toolName: 'control_actor', arguments: actorArgs('set_actor_transform', { location: { x: 40, y: 50, z: 160 }, rotation: { x: 0, y: 0, z: 30 }, scale: { x: 1, y: 1, z: 1 } }), expected: 'success' },
  { scenario: 'INFO: get_transform', toolName: 'control_actor', arguments: actorArgs('get_transform'), expected: 'success' },
  { scenario: 'INFO: get_actor_transform', toolName: 'control_actor', arguments: actorArgs('get_actor_transform'), expected: 'success' },
  { scenario: 'CONFIG: set_visibility', toolName: 'control_actor', arguments: actorArgs('set_visibility', { visible: true }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_visible', toolName: 'control_actor', arguments: actorArgs('set_actor_visible', { visible: true }), expected: 'success' },
  { scenario: 'ACTION: apply_force', toolName: 'control_actor', arguments: actorArgs('apply_force', { force: { x: 0, y: 0, z: 2500 } }), expected: 'success' },

  // === COMPONENTS ===
  { scenario: 'ADD: add_component', toolName: 'control_actor', arguments: actorArgs('add_component', { componentType: '/Script/Engine.PointLightComponent', componentName: COMPONENT_NAME, properties: { Intensity: 1250 } }), expected: 'success|already exists' },
  { scenario: 'CONFIG: set_component_properties', toolName: 'control_actor', arguments: actorArgs('set_component_properties', { componentName: COMPONENT_NAME, properties: { Intensity: 1800 } }), expected: 'success' },
  { scenario: 'CONFIG: set_component_property', toolName: 'control_actor', arguments: actorArgs('set_component_property', { componentName: COMPONENT_NAME, propertyName: 'Intensity', value: 950 }), expected: 'success' },
  { scenario: 'INFO: get_component_property', toolName: 'control_actor', arguments: actorArgs('get_component_property', { componentName: COMPONENT_NAME, propertyName: 'Intensity' }), expected: 'success' },
  { scenario: 'INFO: get_components', toolName: 'control_actor', arguments: actorArgs('get_components'), expected: 'success' },
  { scenario: 'INFO: get_actor_components', toolName: 'control_actor', arguments: actorArgs('get_actor_components'), expected: 'success' },
  { scenario: 'INFO: get_actor_bounds', toolName: 'control_actor', arguments: actorArgs('get_actor_bounds'), expected: 'success' },
  { scenario: 'DELETE: remove_component', toolName: 'control_actor', arguments: actorArgs('remove_component', { componentName: COMPONENT_NAME }), expected: 'success|not found' },

  // === TAGS / SEARCH ===
  { scenario: 'ADD: add_tag', toolName: 'control_actor', arguments: actorArgs('add_tag', { tag: TAG }), expected: 'success|already exists' },
  { scenario: 'INFO: find_by_tag', toolName: 'control_actor', arguments: { action: 'find_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'INFO: find_actors_by_tag', toolName: 'control_actor', arguments: { action: 'find_actors_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'INFO: find_by_name', toolName: 'control_actor', arguments: { action: 'find_by_name', name: MAIN_ACTOR }, expected: 'success' },
  { scenario: 'INFO: find_actors_by_name', toolName: 'control_actor', arguments: { action: 'find_actors_by_name', name: MAIN_ACTOR }, expected: 'success' },
  { scenario: 'INFO: find_by_class', toolName: 'control_actor', arguments: { action: 'find_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'INFO: find_actors_by_class', toolName: 'control_actor', arguments: { action: 'find_actors_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'DELETE: remove_tag', toolName: 'control_actor', arguments: actorArgs('remove_tag', { tag: TAG }), expected: 'success|not found' },
  { scenario: 'ACTION: list', toolName: 'control_actor', arguments: { action: 'list', limit: 20, filter: 'MCP_' }, expected: 'success' },

  // === MISC ===
  { scenario: 'CONFIG: set_blueprint_variables', toolName: 'control_actor', arguments: actorArgs('set_blueprint_variables', { variables: { InitialLifeSpan: 0 } }), expected: 'success' },
  { scenario: 'CREATE: create_snapshot', toolName: 'control_actor', arguments: actorArgs('create_snapshot', { snapshotName: `Snapshot_${ts}` }), expected: 'success|already exists' },
  { scenario: 'ACTION: attach', toolName: 'control_actor', arguments: { action: 'attach', childActor: CHILD_ACTOR, parentActor: PARENT_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: detach', toolName: 'control_actor', arguments: { action: 'detach', actorName: CHILD_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: attach_actor', toolName: 'control_actor', arguments: { action: 'attach_actor', childActor: CHILD_ACTOR, parentActor: PARENT_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: detach_actor', toolName: 'control_actor', arguments: { action: 'detach_actor', actorName: CHILD_ACTOR }, expected: 'success' },
  { scenario: 'CONFIG: set_actor_collision', toolName: 'control_actor', arguments: actorArgs('set_actor_collision', { collisionEnabled: true }), expected: 'success' },
  { scenario: 'ACTION: call_actor_function', toolName: 'control_actor', arguments: actorArgs('call_actor_function', { functionName: 'SetActorTickEnabled', arguments: [true] }), expected: 'success|FUNCTION_NOT_FOUND' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete spawned actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [MAIN_ACTOR, DUPLICATE_ACTOR, DUPLICATE_COPY, MESH_ACTOR, PARENT_ACTOR, CHILD_ACTOR, BP_ACTOR, `MCP_SpawnSphere_${ts}`, `MCP_SpawnCylinder_${ts}`] }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('control-actor', testCases);
