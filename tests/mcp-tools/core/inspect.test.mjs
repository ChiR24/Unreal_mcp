#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

const ACTOR = `MCP_InspectActor_${ts}`;
const DELETE_ACTOR = `MCP_InspectDelete_${ts}`;
const COMPONENT = `MCPInspectLight_${ts}`;
const TAG = `MCPInspectTag_${ts}`;
const SNAPSHOT = `MCPInspectSnapshot_${ts}`;
const BP_NAME = `BP_Inspect_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;

const TEST_MESH = '/Game/MCPTest/TestMesh';
const TEST_MATERIAL = '/Game/MCPTest/TestMat';
const TEST_LEVEL = '/Game/MCPTest/MainLevel';
const ENGINE_DEFAULT_TEXTURE = '/Engine/EngineResources/DefaultTexture.DefaultTexture';

const inspectActor = (action, extra = {}) => ({ action, actorName: ACTOR, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create inspect blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: BP_NAME, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn inspect actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: ACTOR, location: { x: 0, y: 0, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn delete actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: DELETE_ACTOR, location: { x: 180, y: 0, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'Setup: add inspect component', toolName: 'control_actor', arguments: { action: 'add_component', actorName: ACTOR, componentType: '/Script/Engine.PointLightComponent', componentName: COMPONENT, properties: { Intensity: 900 } }, expected: 'success|already exists' },

  // === OBJECT / ASSET INSPECTION ===
  { scenario: 'INFO: inspect_object actor', toolName: 'inspect', arguments: inspectActor('inspect_object'), expected: 'success' },
  { scenario: 'INFO: get_actor_details', toolName: 'inspect', arguments: inspectActor('get_actor_details'), expected: 'success' },
  { scenario: 'INFO: get_bounding_box falls back from empty actorName to name', toolName: 'inspect', arguments: { action: 'get_bounding_box', actorName: '', name: ACTOR }, expected: 'success' },
  { scenario: 'INFO: get_blueprint_details', toolName: 'inspect', arguments: { action: 'get_blueprint_details', objectPath: BP_PATH, blueprintPath: BP_PATH }, expected: 'success' },
  { scenario: 'INFO: get_mesh_details', toolName: 'inspect', arguments: { action: 'get_mesh_details', objectPath: TEST_MESH }, expected: 'success' },
  { scenario: 'INFO: get_texture_details', toolName: 'inspect', arguments: { action: 'get_texture_details', objectPath: ENGINE_DEFAULT_TEXTURE }, expected: 'success' },
  { scenario: 'INFO: get_material_details', toolName: 'inspect', arguments: { action: 'get_material_details', objectPath: TEST_MATERIAL }, expected: 'success' },
  { scenario: 'INFO: get_level_details', toolName: 'inspect', arguments: { action: 'get_level_details', objectPath: TEST_LEVEL }, expected: 'success' },
  { scenario: 'INFO: get_component_details', toolName: 'inspect', arguments: { action: 'get_component_details', actorName: ACTOR, componentName: COMPONENT }, expected: 'success' },

  // === PROPERTIES / COMPONENTS ===
  { scenario: 'CONFIG: set_property', toolName: 'inspect', arguments: inspectActor('set_property', { propertyName: 'InitialLifeSpan', value: 0 }), expected: 'success' },
  { scenario: 'INFO: get_property', toolName: 'inspect', arguments: inspectActor('get_property', { propertyName: 'InitialLifeSpan' }), expected: 'success' },
  { scenario: 'INFO: get_property via name/propertyPath aliases', toolName: 'inspect', arguments: { action: 'get_property', name: ACTOR, propertyPath: 'InitialLifeSpan' }, expected: 'success', assertions: [{ path: 'structuredContent.result.value', equals: 0, label: 'propertyPath alias reads property set through propertyName' }] },
  { scenario: 'INFO: get_components', toolName: 'inspect', arguments: inspectActor('get_components'), expected: 'success' },
  { scenario: 'INFO: get_component_property', toolName: 'inspect', arguments: { action: 'get_component_property', actorName: ACTOR, componentName: COMPONENT, propertyName: 'Intensity' }, expected: 'success' },
  { scenario: 'CONFIG: set_component_property', toolName: 'inspect', arguments: { action: 'set_component_property', actorName: ACTOR, componentName: COMPONENT, propertyName: 'Intensity', value: 1200 }, expected: 'success' },

  // === CLASS / CDO / LISTING ===
  { scenario: 'INFO: inspect_class', toolName: 'inspect', arguments: { action: 'inspect_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'INFO: inspect_class via classPath alias', toolName: 'inspect', arguments: { action: 'inspect_class', classPath: '/Script/Engine.StaticMeshActor' }, expected: 'success', assertions: [{ path: 'structuredContent.result.classPath', equals: '/Script/Engine.StaticMeshActor', label: 'classPath alias resolves inspected class' }] },
  { scenario: 'INFO: inspect_cdo', toolName: 'inspect', arguments: { action: 'inspect_cdo', blueprintPath: BP_PATH, detailed: true }, expected: 'success' },
  { scenario: 'INFO: list_objects', toolName: 'inspect', arguments: { action: 'list_objects' }, expected: 'success' },
  { scenario: 'INFO: get_metadata', toolName: 'inspect', arguments: inspectActor('get_metadata'), expected: 'success' },

  // === TAGS / SNAPSHOTS / SEARCH ===
  { scenario: 'ADD: add_tag', toolName: 'inspect', arguments: inspectActor('add_tag', { tag: TAG }), expected: 'success|already exists' },
  { scenario: 'INFO: find_by_tag', toolName: 'inspect', arguments: { action: 'find_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'CREATE: create_snapshot', toolName: 'inspect', arguments: inspectActor('create_snapshot', { snapshotName: SNAPSHOT }), expected: 'success|already exists' },
  { scenario: 'ACTION: restore_snapshot', toolName: 'inspect', arguments: inspectActor('restore_snapshot', { snapshotName: SNAPSHOT }), expected: 'success' },
  { scenario: 'ACTION: export', toolName: 'inspect', arguments: inspectActor('export'), expected: 'success' },
  { scenario: 'INFO: find_by_class', toolName: 'inspect', arguments: { action: 'find_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'INFO: get_bounding_box', toolName: 'inspect', arguments: inspectActor('get_bounding_box'), expected: 'success' },

  // === GLOBAL INSPECTION ===
  { scenario: 'INFO: get_project_settings', toolName: 'inspect', arguments: { action: 'get_project_settings' }, expected: 'success' },
  { scenario: 'INFO: get_world_settings', toolName: 'inspect', arguments: { action: 'get_world_settings' }, expected: 'success' },
  { scenario: 'INFO: get_viewport_info', toolName: 'inspect', arguments: { action: 'get_viewport_info' }, expected: 'success' },
  { scenario: 'INFO: get_selected_actors', toolName: 'inspect', arguments: { action: 'get_selected_actors' }, expected: 'success' },
  { scenario: 'INFO: get_scene_stats', toolName: 'inspect', arguments: { action: 'get_scene_stats' }, expected: 'success' },
  { scenario: 'INFO: get_performance_stats', toolName: 'inspect', arguments: { action: 'get_performance_stats' }, expected: 'success' },
  { scenario: 'INFO: get_memory_stats', toolName: 'inspect', arguments: { action: 'get_memory_stats' }, expected: 'success' },
  { scenario: 'INFO: get_editor_settings', toolName: 'inspect', arguments: { action: 'get_editor_settings' }, expected: 'success' },
  { scenario: 'INFO: runtime_report', toolName: 'inspect', arguments: { action: 'runtime_report', actorName: ACTOR, componentNames: [COMPONENT], propertyNames: ['ActorLabel'] }, expected: 'success' },
  { scenario: 'INFO: pie_report', toolName: 'inspect', arguments: { action: 'pie_report', filter: ACTOR }, expected: 'success' },

  // === DESTRUCTIVE / CLEANUP ===
  { scenario: 'DELETE: delete_object', toolName: 'inspect', arguments: { action: 'delete_object', actorName: DELETE_ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete inspect actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete inspect blueprint', toolName: 'manage_asset', arguments: { action: 'delete_asset', assetPath: BP_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('inspect', testCases);
