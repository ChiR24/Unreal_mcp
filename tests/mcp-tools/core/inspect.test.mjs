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
const ENGINE_DEFAULT_TEXTURE = '/Engine/EngineResources/DefaultTexture.DefaultTexture';

const inspectActor = (action, extra = {}) => ({ action, actorName: ACTOR, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create inspect blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: BP_NAME, savePath: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn inspect actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: ACTOR, location: { x: 0, y: 0, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn delete actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: DELETE_ACTOR, location: { x: 180, y: 0, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'Setup: add inspect component', toolName: 'control_actor', arguments: { action: 'add_component', actorName: ACTOR, componentType: '/Script/Engine.PointLightComponent', componentName: COMPONENT, properties: { Intensity: 900 } }, expected: 'success|already exists' },

  // === OBJECT / ASSET INSPECTION ===
  { scenario: 'INFO: inspect_object actor', toolName: 'inspect', arguments: inspectActor('inspect_object'), expected: 'success' },
  { scenario: 'INFO: get_actor_details', toolName: 'inspect', arguments: inspectActor('get_actor_details'), expected: 'success' },
  { scenario: 'INFO: get_bounding_box falls back from empty actorName to name', toolName: 'inspect', arguments: { action: 'get_bounding_box', actorName: '', name: ACTOR }, expected: 'success' },
  { scenario: 'INFO: get_blueprint_details', toolName: 'inspect', arguments: { action: 'get_blueprint_details', objectPath: BP_PATH, blueprintPath: BP_PATH }, expected: 'success' },
  // Each slot says what LOD0 draws with it (triangles, and where in mesh space), so a slot can be matched to a part of the mesh.
  { scenario: 'INFO: get_mesh_details', toolName: 'inspect', arguments: { action: 'get_mesh_details', objectPath: TEST_MESH }, expected: 'success', assertions: [{ path: 'structuredContent.result.materialSlots.0.triangles', gte: 1, label: 'slot 0 draws triangles in LOD0' }] },
  { scenario: 'INFO: get_texture_details', toolName: 'inspect', arguments: { action: 'get_texture_details', objectPath: ENGINE_DEFAULT_TEXTURE }, expected: 'success' },
  { scenario: 'INFO: get_material_details', toolName: 'inspect', arguments: { action: 'get_material_details', objectPath: TEST_MATERIAL }, expected: 'success' },
  { scenario: 'INFO: get_level_details', toolName: 'inspect', arguments: { action: 'get_level_details' }, expected: 'success' },
  { scenario: 'INFO: raycast_mesh meets the cube face and misses above it', toolName: 'inspect', arguments: { action: 'raycast_mesh', meshPath: '/Engine/BasicShapes/Cube', rays: [{ origin: { x: 200, y: 0, z: 0 }, direction: { x: -1, y: 0, z: 0 } }, { origin: { x: 200, y: 0, z: 200 }, direction: { x: -2, y: 0, z: 0 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.hitCount', equals: 1, label: 'one ray hits' }, { path: 'structuredContent.result.hits.0.distance', approximately: 150, tolerance: 0.01, label: 'the +X face is 50 cm out' }, { path: 'structuredContent.result.hits.1.hit', equals: false, label: 'the ray above misses' }] },
  { scenario: 'ERROR: raycast_mesh refuses a texture', toolName: 'inspect', arguments: { action: 'raycast_mesh', meshPath: ENGINE_DEFAULT_TEXTURE, rays: [{ origin: { x: 0, y: 0, z: 100 }, direction: { x: 0, y: 0, z: -1 } }] }, expected: 'error|NOT_FOUND' },
  { scenario: 'ERROR: get_mesh_details refuses a texture', toolName: 'inspect', arguments: { action: 'get_mesh_details', objectPath: ENGINE_DEFAULT_TEXTURE }, expected: 'error|TYPE_MISMATCH' },
  { scenario: 'INFO: get_component_details', toolName: 'inspect', arguments: { action: 'get_component_details', actorName: ACTOR, componentName: COMPONENT }, expected: 'success' },

  // === PROPERTIES / COMPONENTS ===
  { scenario: 'CONFIG: set_property', toolName: 'inspect', arguments: inspectActor('set_property', { propertyName: 'InitialLifeSpan', value: 0 }), expected: 'success' },
  { scenario: 'INFO: get_property', toolName: 'inspect', arguments: inspectActor('get_property', { propertyName: 'InitialLifeSpan' }), expected: 'success' },
  { scenario: 'CONFIG: set_property with only propertyPath, left clean', toolName: 'inspect', arguments: inspectActor('set_property', { propertyPath: 'InitialLifeSpan', value: 0, markDirty: false }), expected: 'success' },
  { scenario: 'CONFIG: set_property watches a property after the write', toolName: 'inspect', arguments: inspectActor('set_property', { propertyName: 'InitialLifeSpan', value: 0, markDirty: false, watch: { propertyName: 'InitialLifeSpan', durationSeconds: 0.1, intervalSeconds: 0 } }), expected: 'success', assertions: [{ path: 'structuredContent.result.watch.propertyName', equals: 'InitialLifeSpan', label: 'the watch comes back with the reply' }] },
  { scenario: 'ERROR: set_property refuses a watch that does not resolve, before writing', toolName: 'inspect', arguments: inspectActor('set_property', { propertyName: 'InitialLifeSpan', value: 0, markDirty: false, watch: { propertyName: 'NoSuchWatchedProperty' } }), expected: 'error|PROPERTY_NOT_FOUND' },
  { scenario: 'INFO: get_property via name/propertyPath aliases', toolName: 'inspect', arguments: { action: 'get_property', name: ACTOR, propertyPath: 'InitialLifeSpan' }, expected: 'success', assertions: [{ path: 'structuredContent.result.value', equals: 0, label: 'propertyPath alias reads property set through propertyName' }] },
  { scenario: 'INFO: get_components', toolName: 'inspect', arguments: inspectActor('get_components'), expected: 'success' },
  { scenario: 'INFO: get_components narrowed by componentNames', toolName: 'inspect', arguments: inspectActor('get_components', { componentNames: ['NoSuchComponent'] }), expected: 'success', assertions: [{ path: 'structuredContent.result.missingComponents.0', equals: 'NoSuchComponent', label: 'a name no component has is reported' }] },
  { scenario: 'INFO: get_component_property', toolName: 'inspect', arguments: { action: 'get_component_property', actorName: ACTOR, componentName: COMPONENT, propertyName: 'Intensity' }, expected: 'success' },
  { scenario: 'CONFIG: set_component_property', toolName: 'inspect', arguments: { action: 'set_component_property', actorName: ACTOR, componentName: COMPONENT, propertyName: 'Intensity', value: 1200 }, expected: 'success' },
  { scenario: 'CONFIG: set_component_property properties bag', toolName: 'inspect', arguments: { action: 'set_component_property', actorName: ACTOR, componentName: COMPONENT, properties: { Intensity: 1300, 'LightmassSettings.ShadowExponent': 2 } }, expected: 'success' },

  // === CLASS / CDO / LISTING ===
  { scenario: 'INFO: inspect_class', toolName: 'inspect', arguments: { action: 'inspect_class', className: 'StaticMeshActor' }, expected: 'success', assertions: [{ path: 'structuredContent.result.properties', minLength: 1, label: 'the class property list comes back' }] },
  { scenario: 'INFO: inspect_class via classPath alias', toolName: 'inspect', arguments: { action: 'inspect_class', classPath: '/Script/Engine.StaticMeshActor' }, expected: 'success', assertions: [{ path: 'structuredContent.result.classPath', equals: '/Script/Engine.StaticMeshActor', label: 'classPath alias resolves inspected class' }] },
  { scenario: 'INFO: inspect_cdo', toolName: 'inspect', arguments: { action: 'inspect_cdo', blueprintPath: BP_PATH, detailed: true }, expected: 'success' },
  { scenario: 'INFO: inspect_cdo narrowed by componentNames', toolName: 'inspect', arguments: { action: 'inspect_cdo', blueprintPath: BP_PATH, componentNames: ['NoSuchComponent'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.missingComponents.0', equals: 'NoSuchComponent', label: 'a name no component has is reported' }] },
  // A compile replaces a Blueprint's class and its default object and leaves the old pair in /Engine/Transient as REINST_*:
  // the write, the reply and the save belong to the NEW default object, and a variable's default survives that compile.
  { scenario: 'Setup: add an Int variable to the inspect blueprint', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'CdoCount', variableType: 'Int' }, expected: 'success|already exists' },
  { scenario: 'CONFIG: set_property on a Blueprint variable through blueprintPath', toolName: 'inspect', arguments: { action: 'set_property', blueprintPath: BP_PATH, propertyName: 'CdoCount', value: 3 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.blueprintCompiled', equals: true, label: 'the Blueprint was recompiled' },
    { path: 'structuredContent.result.actorClass', equals: `${BP_NAME}_C`, label: 'the reply names the current class, not a REINST_ copy' },
    { path: 'structuredContent.result.saved', equals: true, label: 'the Blueprint package is saved: the write reached the live default object' },
    { path: 'structuredContent.result.value', equals: 3, label: 'the value is read back off the recompiled default object' },
  ] },
  { scenario: 'VERIFY: the variable default kept the write through the compile', toolName: 'inspect', arguments: { action: 'get_property', blueprintPath: BP_PATH, propertyName: 'CdoCount' }, expected: 'success', assertions: [{ path: 'structuredContent.result.value', equals: 3, label: 'a Blueprint variable default survives the compile set_property ran' }] },
  { scenario: 'CONFIG: a second set_property through the Default__ path lands on the new default object too', toolName: 'inspect', arguments: { action: 'set_property', objectPath: `${BP_PATH}.Default__${BP_NAME}_C`, propertyName: 'CdoCount', value: 4 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.actorClass', equals: `${BP_NAME}_C`, label: 'the second write names the live class as well' },
    { path: 'structuredContent.result.value', equals: 4, label: 'the second write is read back' },
  ] },
  { scenario: 'VERIFY: get_property through the Default__ path reads the second write', toolName: 'inspect', arguments: { action: 'get_property', objectPath: `${BP_PATH}.Default__${BP_NAME}_C`, propertyName: 'CdoCount' }, expected: 'success', assertions: [{ path: 'structuredContent.result.value', equals: 4, label: 'the default object a spawn gets holds the second write' }] },
  { scenario: 'INFO: list_objects', toolName: 'inspect', arguments: { action: 'list_objects' }, expected: 'success' },
  { scenario: 'INFO: list_objects second page', toolName: 'inspect', arguments: { action: 'list_objects', limit: 5, offset: 5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.offset', equals: 5, label: 'offset is honoured' }] },
  { scenario: 'INFO: get_metadata', toolName: 'inspect', arguments: inspectActor('get_metadata'), expected: 'success' },

  // === TAGS / SNAPSHOTS / SEARCH ===
  { scenario: 'ADD: add_tag', toolName: 'inspect', arguments: inspectActor('add_tag', { tag: TAG }), expected: 'success|already exists' },
  { scenario: 'ADD: add_tag on several actors', toolName: 'inspect', arguments: { action: 'add_tag', actorNames: [ACTOR, DELETE_ACTOR], tag: TAG }, expected: 'success|already exists' },
  { scenario: 'INFO: find_by_tag', toolName: 'inspect', arguments: { action: 'find_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'CREATE: create_snapshot', toolName: 'inspect', arguments: inspectActor('create_snapshot', { snapshotName: SNAPSHOT }), expected: 'success|already exists' },
  { scenario: 'ACTION: restore_snapshot', toolName: 'inspect', arguments: inspectActor('restore_snapshot', { snapshotName: SNAPSHOT }), expected: 'success' },
  { scenario: 'ACTION: export', toolName: 'inspect', arguments: inspectActor('export', { format: 'T3D' }), expected: 'success' },
  { scenario: 'ACTION: export to a project file', toolName: 'inspect', arguments: inspectActor('export', { outputPath: 'Saved/MCPTest/InspectExport.t3d' }), expected: 'success' },
  { scenario: 'INFO: find_by_class', toolName: 'inspect', arguments: { action: 'find_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'INFO: get_bounding_box', toolName: 'inspect', arguments: inspectActor('get_bounding_box'), expected: 'success' },

  // === GLOBAL INSPECTION ===
  { scenario: 'INFO: get_project_settings', toolName: 'inspect', arguments: { action: 'get_project_settings' }, expected: 'success' },
  { scenario: 'INFO: get_world_settings', toolName: 'inspect', arguments: { action: 'get_world_settings' }, expected: 'success' },
  { scenario: 'INFO: get_viewport_info', toolName: 'inspect', arguments: { action: 'get_viewport_info' }, expected: 'success', assertions: [{ path: 'structuredContent.result.shadersCompiling', gte: 0, label: 'the viewport read says how many shader jobs are still compiling' }] },
  { scenario: 'INFO: get_selected_actors', toolName: 'inspect', arguments: { action: 'get_selected_actors' }, expected: 'success', assertions: [{ path: 'structuredContent.result.shadersCompiling', gte: 0, label: 'every editor-state read carries the shader count' }] },
  { scenario: 'INFO: get_scene_stats', toolName: 'inspect', arguments: { action: 'get_scene_stats' }, expected: 'success' },
  { scenario: 'INFO: get_performance_stats', toolName: 'inspect', arguments: { action: 'get_performance_stats' }, expected: 'success' },
  { scenario: 'INFO: get_memory_stats', toolName: 'inspect', arguments: { action: 'get_memory_stats' }, expected: 'success' },
  { scenario: 'INFO: get_editor_settings', toolName: 'inspect', arguments: { action: 'get_editor_settings' }, expected: 'success', assertions: [{ path: 'structuredContent.result.shadersCompiling', gte: 0, label: 'the editor settings read carries the shader count too' }] },
  { scenario: 'INFO: runtime_report', toolName: 'inspect', arguments: { action: 'runtime_report', actorName: ACTOR, componentNames: [COMPONENT], propertyNames: ['ActorLabel'] }, expected: 'success' },
  { scenario: 'INFO: pie_report', toolName: 'inspect', arguments: { action: 'pie_report', filter: ACTOR }, expected: 'success' },

  // === DESTRUCTIVE / CLEANUP ===
  { scenario: 'DELETE: delete_object', toolName: 'inspect', arguments: { action: 'delete_object', actorNames: [DELETE_ACTOR] }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete inspect actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete inspect blueprint', toolName: 'manage_asset', arguments: { action: 'delete_asset', assetPath: BP_PATH, force: true }, expected: 'success|not found' },
];

runToolTests('inspect', testCases, { folder: TEST_FOLDER });
