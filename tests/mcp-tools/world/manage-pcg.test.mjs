#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const BASIC_TEST_FOLDER = '/Game/MCPTest/PCG';
const BASIC_GRAPH_PATH = `${BASIC_TEST_FOLDER}/PCG_Test_${ts}`;
const BASIC_SUBGRAPH_PATH = `${BASIC_TEST_FOLDER}/PCG_Test_Subgraph_${ts}`;
const BASIC_REROUTE_NODE = `Reroute_${ts}`;
const BASIC_UPDATED_REROUTE_NODE = `${BASIC_REROUTE_NODE}_Updated`;
const BASIC_PCG_ACTOR = `PCGExecutor_${ts}`;

const REAL_TEST_FOLDER = `/Game/MCPTest/PCGRealWorld_${ts}`;
const REAL_GRAPH_NAME = `PCG_RW_Graph_${ts}`;
const REAL_GRAPH_PATH = `${REAL_TEST_FOLDER}/${REAL_GRAPH_NAME}`;
const PATH_NAME_GRAPH = `${REAL_TEST_FOLDER}/PCG_RW_PathName_${ts}`;
const ASSET_ALIAS_GRAPH = `${REAL_TEST_FOLDER}/PCG_RW_AssetAlias_${ts}`;
const REAL_SUBGRAPH_PATH = `${REAL_TEST_FOLDER}/PCG_RW_Subgraph_${ts}`;
const REAL_LEVEL_NAME = `PCG_RW_Level_${ts}`;
const EXEC_ACTOR = `PCG_RW_Executor_${ts}`;
const SECOND_ACTOR = `PCG_RW_SecondExecutor_${ts}`;
const BOUNDSLESS_ACTOR = `PCG_RW_Boundsless_${ts}`;
const COMPONENT_NAME = `PCG_RW_Component_${ts}`;
const PATH_CREATE_COMPONENT = `PCG_RW_PathCreateComponent_${ts}`;
const MESH_SAMPLER_NODE = `PCG_RW_MeshSampler_${ts}`;
const STATIC_MESH_SPAWNER_NODE = `PCG_RW_StaticMeshSpawner_${ts}`;
const ACTOR_SPAWNER_NODE = `PCG_RW_ActorSpawner_${ts}`;
const TEXTURE_NODE = `PCG_RW_Texture_${ts}`;
const REAL_REROUTE_NODE = `PCG_RW_Reroute_${ts}`;
const REAL_UPDATED_REROUTE_NODE = `${REAL_REROUTE_NODE}_Updated`;

const pcgOptionalExpected = { successPattern: 'PCG', errorPattern: 'PCG_PLUGIN' };
const pcgExpected = { successPattern: 'PCG' };

const basicNodeActions = [
  'add_landscape_data_node',
  'add_spline_data_node',
  'add_volume_data_node',
  'add_actor_data_node',
  'add_texture_data_node',
  'add_surface_sampler',
  'add_mesh_sampler',
  'add_spline_sampler',
  'add_volume_sampler',
  'add_bounds_modifier',
  'add_density_filter',
  'add_height_filter',
  'add_slope_filter',
  'add_distance_filter',
  'add_bounds_filter',
  'add_self_pruning',
  'add_transform_points',
  'add_project_to_surface',
  'add_copy_points',
  'add_merge_points',
  'add_static_mesh_spawner',
  'add_actor_spawner',
  'add_spline_spawner'
];

const basicNodeActionCases = basicNodeActions.map((action, index) => ({
  scenario: `CREATE: ${action}`,
  toolName: 'manage_pcg',
  arguments: {
    action,
    graphPath: BASIC_GRAPH_PATH,
    nodeName: `${action}_${ts}`,
    x: 240 + (index % 6) * 160,
    y: 160 + Math.floor(index / 6) * 120,
    save: false
  },
  expected: pcgOptionalExpected
}));

const basicCoverageCases = [
  { scenario: 'Setup: create PCG test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: BASIC_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn PCG execution actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName: BASIC_PCG_ACTOR, location: { x: 0, y: 0, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_pcg_graph', toolName: 'manage_pcg', arguments: { action: 'create_pcg_graph', graphPath: BASIC_GRAPH_PATH, overwrite: true, save: false }, expected: pcgOptionalExpected },
  { scenario: 'CREATE: create_pcg_subgraph', toolName: 'manage_pcg', arguments: { action: 'create_pcg_subgraph', subgraphPath: BASIC_SUBGRAPH_PATH, parentGraphPath: BASIC_GRAPH_PATH, nodeName: `Subgraph_${ts}`, overwrite: true, save: false }, expected: pcgOptionalExpected },
  { scenario: 'CREATE: add_pcg_node', toolName: 'manage_pcg', arguments: { action: 'add_pcg_node', graphPath: BASIC_GRAPH_PATH, settingsClass: 'PCGRerouteSettings', nodeName: BASIC_REROUTE_NODE, x: 120, y: 80, save: false }, expected: pcgOptionalExpected },
  ...basicNodeActionCases,
  { scenario: 'ACTION: connect_pcg_pins', toolName: 'manage_pcg', arguments: { action: 'connect_pcg_pins', graphPath: BASIC_GRAPH_PATH, sourceNodeId: 'input', targetNodeId: 'output', save: false }, expected: pcgOptionalExpected },
  { scenario: 'CONFIG: set_pcg_node_settings', toolName: 'manage_pcg', arguments: { action: 'set_pcg_node_settings', graphPath: BASIC_GRAPH_PATH, nodeId: BASIC_REROUTE_NODE, title: BASIC_UPDATED_REROUTE_NODE, save: false }, expected: pcgOptionalExpected },
  { scenario: 'EXECUTE: execute_pcg_graph', toolName: 'manage_pcg', arguments: { action: 'execute_pcg_graph', graphPath: BASIC_GRAPH_PATH, actorName: BASIC_PCG_ACTOR, componentName: `PCGComponent_${ts}`, createComponent: true, force: true, save: false }, expected: pcgOptionalExpected },
  { scenario: 'CONFIG: set_pcg_partition_grid_size', toolName: 'manage_pcg', arguments: { action: 'set_pcg_partition_grid_size', gridSize: 3200, scope: 'world', save: false }, expected: pcgOptionalExpected },
  { scenario: 'Cleanup: delete PCG execution actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: BASIC_PCG_ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete PCG test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: BASIC_TEST_FOLDER, force: true }, expected: 'success|not found' }
];

const realWorldCoverageCases = [
  {
    scenario: 'Setup: create real-world PCG folder',
    toolName: 'manage_asset',
    arguments: { action: 'create_folder', path: REAL_TEST_FOLDER },
    expected: 'success|already exists'
  },
  {
    scenario: 'Setup: spawn bounds-valid execution actor',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: '/Script/Engine.StaticMeshActor',
      meshPath: '/Engine/BasicShapes/Cube.Cube',
      actorName: EXEC_ACTOR,
      location: { x: 0, y: 0, z: 120 }
    },
    expected: 'success|already exists',
    assertions: [{ path: 'structuredContent.result.actorName', equals: EXEC_ACTOR, label: 'execution actor spawned with requested label' }]
  },
  {
    scenario: 'Setup: spawn second bounds-valid execution actor',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: '/Script/Engine.StaticMeshActor',
      meshPath: '/Engine/BasicShapes/Sphere.Sphere',
      actorName: SECOND_ACTOR,
      location: { x: 240, y: 0, z: 120 }
    },
    expected: 'success|already exists',
    assertions: [{ path: 'structuredContent.result.actorName', equals: SECOND_ACTOR, label: 'second execution actor spawned with requested label' }]
  },
  {
    scenario: 'Setup: spawn boundsless execution actor',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: '/Script/Engine.Actor',
      actorName: BOUNDSLESS_ACTOR,
      location: { x: 480, y: 0, z: 120 }
    },
    expected: 'success|already exists',
    assertions: [{ path: 'structuredContent.result.actorName', equals: BOUNDSLESS_ACTOR, label: 'boundsless execution actor spawned with requested label' }]
  },
  {
    scenario: 'CREATE: saved PCG graph asset for real workflow',
    toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', graphPath: REAL_GRAPH_PATH, overwrite: true, save: true },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.assetPath', equals: REAL_GRAPH_PATH, label: 'graph created at requested asset path' },
      { path: 'structuredContent.result.saved', equals: true, label: 'graph save path exercised' },
      { path: 'structuredContent.result.existsAfter', equals: true, label: 'graph exists after creation' }
    ]
  },
  {
    scenario: 'VERIFY: saved PCG graph exists through asset surface',
    toolName: 'manage_asset',
    arguments: { action: 'exists', assetPath: REAL_GRAPH_PATH },
    expected: 'success',
    assertions: [{ path: 'structuredContent.data.result.exists', equals: true, label: 'saved graph asset can be found by asset surface' }]
  },
  {
    scenario: 'CREATE: graph from path plus name fallback',
    toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', path: REAL_TEST_FOLDER, name: `PCG_RW_PathName_${ts}`, overwrite: true, save: false },
    expected: pcgExpected,
    assertions: [{ path: 'structuredContent.result.assetPath', equals: PATH_NAME_GRAPH, label: 'path/name fallback created expected graph path' }]
  },
  {
    scenario: 'CREATE: graph from assetPath alias',
    toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', assetPath: ASSET_ALIAS_GRAPH, overwrite: true, save: false },
    expected: pcgExpected,
    assertions: [{ path: 'structuredContent.result.assetPath', equals: ASSET_ALIAS_GRAPH, label: 'assetPath alias created expected graph path' }]
  },
  {
    scenario: 'ERROR: duplicate graph without overwrite is rejected',
    toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', graphPath: REAL_GRAPH_PATH, overwrite: false, save: false },
    expected: { errorPattern: 'ASSET_ALREADY_EXISTS' }
  },
  {
    scenario: 'CREATE: subgraph asset attached to parent graph',
    toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_subgraph', subgraphPath: REAL_SUBGRAPH_PATH, parentGraphPath: REAL_GRAPH_PATH, nodeName: `PCG_RW_SubgraphNode_${ts}`, overwrite: true, save: true },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.subgraphPath', equals: REAL_SUBGRAPH_PATH, label: 'subgraph created at requested path' },
      { path: 'structuredContent.result.parentGraphPath', equals: REAL_GRAPH_PATH, label: 'subgraph node attached to parent graph' },
      { path: 'structuredContent.result.parentSaved', equals: true, label: 'parent graph saved after subgraph attachment' }
    ]
  },
  {
    scenario: 'VERIFY: saved PCG subgraph exists through asset surface',
    toolName: 'manage_asset',
    arguments: { action: 'exists', assetPath: REAL_SUBGRAPH_PATH },
    expected: 'success',
    assertions: [{ path: 'structuredContent.data.result.exists', equals: true, label: 'saved subgraph asset can be found by asset surface' }]
  },
  {
    scenario: 'ADD: mesh sampler with real engine mesh setting',
    toolName: 'manage_pcg',
    arguments: { action: 'add_mesh_sampler', graphPath: REAL_GRAPH_PATH, nodeName: MESH_SAMPLER_NODE, meshPath: '/Engine/BasicShapes/Cube.Cube', x: 160, y: 120, save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.nodeType', equals: 'PCGPointFromMeshSettings', label: 'mesh sampler resolves PCG settings class' },
      { path: 'structuredContent.result.settingsApplied', equals: 1, label: 'meshPath convenience setting applied' }
    ]
  },
  {
    scenario: 'ADD: static mesh spawner with real engine mesh setting',
    toolName: 'manage_pcg',
    arguments: { action: 'add_static_mesh_spawner', graphPath: REAL_GRAPH_PATH, nodeName: STATIC_MESH_SPAWNER_NODE, meshPath: '/Engine/BasicShapes/Sphere.Sphere', x: 240, y: 260, save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.nodeType', equals: 'PCGStaticMeshSpawnerSettings', label: 'static mesh spawner resolves PCG settings class' },
      { path: 'structuredContent.result.settingsApplied', equals: 1, label: 'static mesh spawner meshPath convenience setting applied' }
    ]
  },
  {
    scenario: 'ADD: actor spawner with real class setting',
    toolName: 'manage_pcg',
    arguments: { action: 'add_actor_spawner', graphPath: REAL_GRAPH_PATH, nodeName: ACTOR_SPAWNER_NODE, actorClass: '/Script/Engine.StaticMeshActor', classPath: '/Script/Engine.StaticMeshActor', x: 320, y: 120, save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.nodeType', equals: 'PCGSpawnActorSettings', label: 'actor spawner resolves PCG settings class' },
      { path: 'structuredContent.result.settingsApplied', equals: 1, label: 'actorClass convenience setting applied' }
    ]
  },
  {
    scenario: 'ADD: texture data node with real engine texture setting',
    toolName: 'manage_pcg',
    arguments: { action: 'add_texture_data_node', graphPath: REAL_GRAPH_PATH, nodeName: TEXTURE_NODE, texturePath: '/Engine/EngineResources/DefaultTexture.DefaultTexture', x: 480, y: 120, save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.nodeType', equals: 'PCGTextureSamplerSettings', label: 'texture data resolves PCG settings class' },
      { path: 'structuredContent.result.settingsApplied', equals: 1, label: 'texturePath convenience setting applied' }
    ]
  },
  {
    scenario: 'CONFIG: update mesh sampler through top-level meshPath',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_node_settings', graphPath: REAL_GRAPH_PATH, nodeId: MESH_SAMPLER_NODE, meshPath: '/Engine/BasicShapes/Sphere.Sphere', save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.nodeType', equals: 'PCGPointFromMeshSettings', label: 'meshPath mutation targets mesh sampler' },
      { path: 'structuredContent.result.settingsApplied', equals: 1, label: 'top-level meshPath applied during settings mutation' }
    ]
  },
  {
    scenario: 'CONFIG: update actor spawner through top-level classPath',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_node_settings', graphPath: REAL_GRAPH_PATH, nodeId: ACTOR_SPAWNER_NODE, classPath: '/Script/Engine.Actor', save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.nodeType', equals: 'PCGSpawnActorSettings', label: 'classPath mutation targets actor spawner' },
      { path: 'structuredContent.result.settingsApplied', equals: 1, label: 'top-level classPath applied during settings mutation' }
    ]
  },
  {
    scenario: 'CONFIG: update texture node through top-level texturePath',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_node_settings', graphPath: REAL_GRAPH_PATH, nodeId: TEXTURE_NODE, texturePath: '/Engine/EngineResources/DefaultTexture.DefaultTexture', save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.nodeType', equals: 'PCGTextureSamplerSettings', label: 'texturePath mutation targets texture sampler' },
      { path: 'structuredContent.result.settingsApplied', equals: 1, label: 'top-level texturePath applied during settings mutation' }
    ]
  },
  {
    scenario: 'CONFIG: apply positive settings object to existing PCG node',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_node_settings', graphPath: REAL_GRAPH_PATH, nodeId: MESH_SAMPLER_NODE, settings: { bEnabled: true }, save: true },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.nodeType', equals: 'PCGPointFromMeshSettings', label: 'settings mutation targets mesh sampler' },
      { path: 'structuredContent.result.settingsApplied', equals: 1, label: 'freeform settings object applied' },
      { path: 'structuredContent.result.saved', equals: true, label: 'settings mutation saved the graph' }
    ]
  },
  {
    scenario: 'ACTION: connect authored mesh sampler to authored actor spawner',
    toolName: 'manage_pcg',
    arguments: { action: 'connect_pcg_pins', graphPath: REAL_GRAPH_PATH, sourceNodeId: MESH_SAMPLER_NODE, targetNodeId: ACTOR_SPAWNER_NODE, save: true },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.sourceNodeId', equals: 'PointFromMesh_0', label: 'authored mesh sampler resolved by title' },
      { path: 'structuredContent.result.targetNodeId', equals: 'SpawnActor_0', label: 'authored actor spawner resolved by title' },
      { path: 'structuredContent.result.saved', equals: true, label: 'authored node connection saved the graph' }
    ]
  },
  {
    scenario: 'ADD: reroute node for rename and settings workflow',
    toolName: 'manage_pcg',
    arguments: { action: 'add_pcg_node', graphPath: REAL_GRAPH_PATH, nodeType: 'PCGRerouteSettings', nodeName: REAL_REROUTE_NODE, posX: 640, posY: 120, save: false },
    expected: pcgExpected,
    assertions: [{ path: 'structuredContent.result.nodeType', equals: 'PCGRerouteSettings', label: 'generic node settings class resolved' }]
  },
  {
    scenario: 'CONFIG: rename existing PCG node',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_node_settings', graphPath: REAL_GRAPH_PATH, nodeId: REAL_REROUTE_NODE, title: REAL_UPDATED_REROUTE_NODE, save: true },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.title', equals: REAL_UPDATED_REROUTE_NODE, label: 'node title updated and read back' },
      { path: 'structuredContent.result.saved', equals: true, label: 'node rename saved the graph' }
    ]
  },
  {
    scenario: 'ACTION: connect default graph pins',
    toolName: 'manage_pcg',
    arguments: { action: 'connect_pcg_pins', graphPath: REAL_GRAPH_PATH, sourceNodeId: 'input', targetNodeId: 'output', outputName: 'In', targetPin: 'Out', inputName: 'Out', save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.sourceNodeId', equals: 'DefaultInputNode', label: 'input alias resolves to default input node' },
      { path: 'structuredContent.result.targetNodeId', equals: 'DefaultOutputNode', label: 'output alias resolves to default output node' }
    ]
  },
  {
    scenario: 'EXECUTE: create PCG component and generate on mesh actor',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath: REAL_GRAPH_PATH, actorName: EXEC_ACTOR, componentName: COMPONENT_NAME, createComponent: true, force: true, wait: false, timeoutMs: 120000, save: false },
    expected: pcgExpected,
    captureResult: { key: 'pcgComponentPath', fromField: 'result.componentPath' },
    assertions: [
      { path: 'structuredContent.result.componentName', equals: COMPONENT_NAME, label: 'PCG component created with requested name' },
      { path: 'structuredContent.result.force', equals: true, label: 'force generation flag honored' }
    ]
  },
  {
    scenario: 'CONFIG: set component-scoped generation grid size',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_partition_grid_size', scope: 'component', actorName: EXEC_ACTOR, componentName: COMPONENT_NAME, gridSize: 6400, save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.scope', equals: 'component', label: 'component grid size path exercised' },
      { path: 'structuredContent.result.gridSize', equals: 6400, label: 'component grid size applied' }
    ]
  },
  {
    scenario: 'EXECUTE: reuse existing PCG component by full componentPath',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath: REAL_GRAPH_PATH, actorName: EXEC_ACTOR, componentPath: '${captured:pcgComponentPath}', createComponent: false, force: true, save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.componentName', equals: COMPONENT_NAME, label: 'existing PCG component reused by full path' },
      { path: 'structuredContent.result.force', equals: true, label: 'force generation flag honored on path reuse' }
    ]
  },
  {
    scenario: 'EXECUTE: componentPath stays a selector when creating a new component',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath: REAL_GRAPH_PATH, actorName: SECOND_ACTOR, componentPath: `MissingComponentPath_${ts}`, componentName: PATH_CREATE_COMPONENT, createComponent: true, force: true, save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.componentName', equals: PATH_CREATE_COMPONENT, label: 'new component uses componentName instead of selector path' },
      { path: 'structuredContent.result.saved', equals: false, label: 'save=false does not persist the level' }
    ]
  },
  {
    scenario: 'EXECUTE: same graph on a second mesh actor',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath: REAL_GRAPH_PATH, actorName: SECOND_ACTOR, componentName: `${COMPONENT_NAME}_Second`, createComponent: true, force: true, save: false },
    expected: pcgExpected,
    assertions: [{ path: 'structuredContent.result.componentName', equals: `${COMPONENT_NAME}_Second`, label: 'second actor gets independent PCG component' }]
  },
  {
    scenario: 'CONFIG: set world partition grid size',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_partition_grid_size', scope: 'world', gridSize: 12800, save: false },
    expected: pcgExpected,
    assertions: [
      { path: 'structuredContent.result.scope', equals: 'world', label: 'world grid size path exercised' },
      { path: 'structuredContent.result.gridSize', equals: 12800, label: 'world grid size applied' }
    ]
  },
  {
    scenario: 'ERROR: invalid partition grid scope is rejected',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_partition_grid_size', scope: 'invalid_scope', gridSize: 6400, save: false },
    expected: { errorPattern: 'INVALID_ARGUMENT' }
  },
  {
    scenario: 'ERROR: non-string partition grid scope is rejected',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_partition_grid_size', scope: 123, gridSize: 6400, save: false },
    expected: { errorPattern: 'INVALID_ARGUMENT' }
  },
  {
    scenario: 'ERROR: component grid size requires a component selector',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_partition_grid_size', scope: 'component', gridSize: 6400, save: false },
    expected: { errorPattern: 'INVALID_ARGUMENT' }
  },
  {
    scenario: 'ERROR: execute with missing actor reports actor_not_found',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath: REAL_GRAPH_PATH, actorName: `PCG_RW_Missing_${ts}`, componentName: `MissingComponent_${ts}`, createComponent: true, save: false },
    expected: { errorPattern: 'ACTOR_NOT_FOUND' }
  },
  {
    scenario: 'ERROR: execute without actor or component selector is rejected',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath: REAL_GRAPH_PATH, createComponent: false, force: true, save: false },
    expected: { errorPattern: 'INVALID_ARGUMENT' }
  },
  {
    scenario: 'ERROR: execute on boundsless actor reports generation_not_scheduled',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath: REAL_GRAPH_PATH, actorName: BOUNDSLESS_ACTOR, componentName: `PCG_RW_BoundslessComponent_${ts}`, createComponent: true, force: true, save: false },
    expected: { errorPattern: 'GENERATION_NOT_SCHEDULED' }
  },
  {
    scenario: 'ERROR: component grid size with missing component reports component_not_found',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_partition_grid_size', scope: 'component', actorName: EXEC_ACTOR, componentName: `MissingComponent_${ts}`, gridSize: 3200, save: false },
    expected: { errorPattern: 'COMPONENT_NOT_FOUND' }
  },
  {
    scenario: 'ERROR: invalid settings property is rejected',
    toolName: 'manage_pcg',
    arguments: { action: 'add_pcg_node', graphPath: REAL_GRAPH_PATH, settingsClass: 'PCGRerouteSettings', nodeName: `InvalidSettings_${ts}`, settings: { DefinitelyNotAProperty: true }, save: false },
    expected: { errorPattern: 'INVALID_SETTINGS' }
  },
  {
    scenario: 'ERROR: invalid pin name is rejected',
    toolName: 'manage_pcg',
    arguments: { action: 'connect_pcg_pins', graphPath: REAL_GRAPH_PATH, sourceNodeId: 'input', targetNodeId: 'output', sourcePin: `MissingPin_${ts}`, save: false },
    expected: { errorPattern: 'PIN_NOT_FOUND' }
  },
  {
    scenario: 'Cleanup: delete first PCG execution actor',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorName: EXEC_ACTOR },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete second PCG execution actor',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorName: SECOND_ACTOR },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete boundsless PCG execution actor',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorName: BOUNDSLESS_ACTOR },
    expected: 'success|not found'
  },
  {
    scenario: 'Setup: create saved level for world partition persistence',
    toolName: 'manage_level',
    arguments: {
      action: 'create_level',
      levelName: REAL_LEVEL_NAME,
      levelPath: REAL_TEST_FOLDER,
      template: '/Engine/Maps/Templates/OpenWorld',
      useWorldPartition: false,
      saveDirtyPackages: true
    },
    expected: 'success|already exists'
  },
  {
    scenario: 'SAVE: world partition grid size persists on current saved level',
    toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_partition_grid_size', scope: 'world', gridSize: 9600, save: true },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.gridSize', equals: 9600, label: 'world partition grid size applied' },
      { path: 'structuredContent.result.saved', equals: true, label: 'saved level persisted the grid size' }
    ]
  },
  {
    scenario: 'Cleanup: delete real-world PCG folder',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: REAL_TEST_FOLDER, force: true },
    expected: 'success|not found'
  }
];

runToolTests('manage-pcg', [...basicCoverageCases, ...realWorldCoverageCases]);
