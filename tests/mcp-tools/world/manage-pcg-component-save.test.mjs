import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const suffix = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const runRoot = `/Game/MCP_PCG_Validation/ComponentSave_${suffix}`;
const mapPath = `${runRoot}/Map`;
const graphPathValue = `${runRoot}/Graph`;
const graphObjectPath = `${graphPathValue}.Graph`;
const actorName = `MCP_PCG_Save_${suffix}`;
const componentAName = `MCP_PCG_Save_A_${suffix}`;
const componentBName = `MCP_PCG_Save_B_${suffix}`;
const componentASeed = 77;
const componentBSeed = 83;
const captured = key => '${captured:' + key + '}';
const graphPath = captured('graphPath');
const initialRevision = captured('initialRevision');
const authoredRevision = captured('authoredRevision');
const graphOutput = captured('graphOutput');
const graphOutputPin = captured('graphOutputPin');
const componentAInitialPath = captured('componentAInitialPath');
const componentBInitialPath = captured('componentBInitialPath');
const componentAPath = captured('componentAPath');
const componentBPath = captured('componentBPath');

const pcg = (scenario, arguments_, extra = {}) => ({
  scenario, toolName: 'manage_pcg', arguments: arguments_, expected: 'success', abortOnFailure: true, ...extra,
});
const actor = (scenario, arguments_, extra = {}) => ({
  scenario, toolName: 'control_actor', arguments: arguments_, expected: 'success', abortOnFailure: true, ...extra,
});
const inspect = (scenario, arguments_, extra = {}) => ({
  scenario, toolName: 'inspect', arguments: arguments_, expected: 'success', abortOnFailure: true, ...extra,
});
const editComponent = (scenario, edit, componentName, properties) => actor(scenario, {
  action: 'edit_component', edit, actorName, componentName,
  ...(edit === 'add' ? { componentType: 'PCGComponent' } : {}),
  ...(properties ? { properties } : {}),
});
const point = (x, y, z, rotation, scale3d, density, seed) => ({
  Transform: { translation: { x, y, z }, rotation, scale3d }, Density: density, Seed: seed,
});
const pointA = {
  location: { x: 123, y: -234, z: 45 }, rotation: { x: 0, y: 0, z: 0.6, w: 0.8 },
  scale: { x: 1.25, y: 0.75, z: 1.5 }, density: 0.3125, seed: -73,
};
const pointB = {
  location: { x: 613, y: -727, z: 89 }, rotation: { x: 0, y: 0, z: -0.8, w: 0.6 },
  scale: { x: 0.5, y: 1.75, z: 2.25 }, density: 0.8125, seed: 911,
};
const propertyAssertions = (expectedGraph, seed) => [
  { path: 'structuredContent.result.properties', length: 5 },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'Graph', propertyPath: 'GraphInstance.Graph', value: expectedGraph } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'GenerationTrigger', value: 'GenerateOnDemand' } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'bRegenerateInEditor', value: false } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'bActivated', value: true } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'Seed', value: seed } },
];
const noOutputAssertions = [
  { path: 'structuredContent.result.generating', equals: false },
  { path: 'structuredContent.result.cleaning', equals: false },
  { path: 'structuredContent.result.instanceCount', equals: 0 },
  { path: 'structuredContent.result.outputDataCount', equals: 0 },
  { path: 'structuredContent.result.outputPointCount', equals: 0 },
  { path: 'structuredContent.result.outputs', length: 0 },
];
const graphRevisionRead = scenario => pcg(scenario, {
  action: 'inspect_pcg', view: 'graph', graphPath, includeSettings: true, limit: 8, edgeLimit: 8,
}, { assertions: [
  { path: 'structuredContent.result.revision', equals: authoredRevision },
  { path: 'structuredContent.result.items', length: 3 },
  { path: 'structuredContent.result.edges', length: 1 },
] });
const componentRead = (scenario, componentPath, expectedGraph, seed) => inspect(scenario, {
  action: 'get_property', objectPath: componentPath,
  propertyNames: ['GraphInstance.Graph', 'GenerationTrigger', 'bRegenerateInEditor', 'bActivated', 'Seed'],
}, { assertions: propertyAssertions(expectedGraph, seed) });
const componentInventory = (scenario, componentName, pathCapture, nameCapture) => inspect(scenario, {
  action: 'get_components', actorName, componentNames: [componentName],
}, {
  assertions: [
    { path: 'structuredContent.result.components', length: 1 },
    { path: 'structuredContent.result.components.0.name', equals: componentName },
    { path: 'structuredContent.result.components.0.class', equals: '/Script/PCG.PCGComponent' },
  ],
  captureResult: [
    { key: nameCapture, fromField: 'result.components.0.name' },
    { key: pathCapture, fromField: 'result.components.0.path' },
  ],
});
const emptyComponentReads = (label, componentPath) => [
  pcg(`Prove ${label} has no pre-generation work, instances, or outputs`, {
    action: 'get_pcg_component_results', componentPath, limit: 8, sampleLimit: 0,
  }, { assertions: noOutputAssertions }),
  pcg(`Prove ${label} has no pre-generation root datasets`, {
    action: 'list_pcg_data', componentPath, offset: 0, limit: 4,
  }, { assertions: [
    { path: 'structuredContent.result.items', length: 0 },
    { path: 'structuredContent.result.total', equals: 0 },
  ] }),
];

const cases = [
  {
    scenario: 'Refuse to reuse a map that already exists at this unique run path',
    toolName: 'manage_asset', arguments: { action: 'validate', assetPath: mapPath },
    expected: { condition: 'error', errorPattern: 'ASSET_NOT_FOUND' }, abortOnFailure: true,
  },
  {
    scenario: 'Create and load the uniquely owned non-World-Partition map without saving unknown dirty packages',
    toolName: 'manage_level',
    arguments: {
      action: 'create_level', levelName: 'Map', levelPath: runRoot,
      useWorldPartition: false, saveDirtyPackages: false,
    },
    expected: 'success', abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.levelPath', equals: mapPath },
      { path: 'structuredContent.result.saved', equals: true },
      { path: 'structuredContent.result.loaded', equals: true },
      { path: 'structuredContent.result.currentLevelPath', equals: mapPath },
      { path: 'structuredContent.result.worldPartitionRequested', equals: false },
    ],
  },
  {
    scenario: 'Create and save the unique native PCG graph',
    toolName: 'manage_pcg',
    arguments: { action: 'edit_pcg_graph', edit: 'create', graphPath: graphPathValue, save: true },
    expected: 'success', abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.created', equals: true },
      { path: 'structuredContent.result.graphPath', equals: graphPathValue },
      { path: 'structuredContent.result.committed', equals: true },
      { path: 'structuredContent.result.saved', equals: true },
    ],
    captureResult: [{ key: 'graphPath', fromField: 'result.graphPath' }],
  },
  {
    scenario: 'Spawn this run’s bounds-valid StaticMeshActor into its selected map',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn', classPath: '/Script/Engine.StaticMeshActor',
      meshPath: '/Engine/BasicShapes/Cube.Cube', actorName,
      location: { x: 0, y: 0, z: 0 }, scale: { x: 20, y: 20, z: 2 },
    },
    expected: 'success', abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.existsAfter', equals: true },
      { path: 'structuredContent.result.actorName', equals: actorName },
    ],
    captureResult: [{ key: 'actorName', fromField: 'result.actorName' }],
  },
  {
    scenario: 'Read the new graph’s native input and output before authoring its points node',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, limit: 8, edgeLimit: 8, includeSettings: true },
    expected: 'success', abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 2 },
      { path: 'structuredContent.result.items.0.input', equals: true },
      { path: 'structuredContent.result.items.1.output', equals: true },
      { path: 'structuredContent.result.items.1.inputPins', minLength: 1 },
    ],
    captureResult: [
      { key: 'graphOutput', fromField: 'result.items', where: { path: 'output', equals: true }, selectField: 'nodeId' },
      { key: 'graphOutputPin', fromField: 'result.items', where: { path: 'output', equals: true }, selectField: 'inputPins.0.label' },
      { key: 'initialRevision', fromField: 'result.revision' },
    ],
  },
  pcg('Author the two deterministic CreatePoints rows and save them with an exact graph-revision precondition', {
    action: 'batch_edit_pcg_graph', graphPath, expectedGraphRevision: initialRevision, save: true,
    operations: [
      {
        op: 'add_node', settingsClass: 'PCGCreatePointsSettings', tempId: 'points',
        settings: {
          bCullPointsOutsideVolume: false,
          PointsToCreate: [
            point(pointA.location.x, pointA.location.y, pointA.location.z, pointA.rotation, pointA.scale, pointA.density, pointA.seed),
            point(pointB.location.x, pointB.location.y, pointB.location.z, pointB.rotation, pointB.scale, pointB.density, pointB.seed),
          ],
        },
      },
      { op: 'connect', sourceNodeId: 'points', sourcePin: 'Out', targetNodeId: graphOutput, targetPin: graphOutputPin },
    ],
  }, {
    assertions: [
      { path: 'structuredContent.result.committed', equals: true },
      { path: 'structuredContent.result.saved', equals: true },
    ],
    captureResult: [{ key: 'pointNodeId', fromField: 'result.nodeIds.points' }],
  }),
  {
    scenario: 'Read back the saved graph revision and exact points-node connection',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, limit: 8, edgeLimit: 8, includeSettings: true },
    expected: 'success', abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 3 },
      { path: 'structuredContent.result.items.0.input', equals: true },
      { path: 'structuredContent.result.items.2.output', equals: true },
      { path: 'structuredContent.result.edges', length: 1 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: captured('pointNodeId'), sourcePin: 'Out', targetNodeId: graphOutput, targetPin: graphOutputPin,
      } },
    ],
    captureResult: [{ key: 'authoredRevision', fromField: 'result.revision' }],
  },
  editComponent('Add the first native PCG component to this run’s actor', 'add', componentAName),
  editComponent('Add the second native PCG component to this run’s actor', 'add', componentBName),
  editComponent('Set the first component to on-demand mode with its distinct known seed', 'set_properties', componentAName, {
    GenerationTrigger: 'GenerateOnDemand', bRegenerateInEditor: false, bActivated: true, Seed: componentASeed,
  }),
  editComponent('Set the second component to on-demand mode with its distinct known seed', 'set_properties', componentBName, {
    GenerationTrigger: 'GenerateOnDemand', bRegenerateInEditor: false, bActivated: true, Seed: componentBSeed,
  }),
  componentInventory('Discover the first exact native PCG component before binding', componentAName, 'componentAInitialPath', 'componentANameBeforeSave'),
  componentInventory('Discover the second exact native PCG component before binding', componentBName, 'componentBInitialPath', 'componentBNameBeforeSave'),
  actor('Bind only the first exact component through native SetGraph', {
    action: 'call_actor_function', actorName, componentName: componentAName,
    functionName: 'SetGraph', arguments: { InGraph: graphObjectPath },
  }, { consent: { capability: 'control_actor.call_actor_function', acknowledge: 'elevated' } }),
  graphRevisionRead('Prove binding the first component preserved the saved graph revision'),
  actor('Bind only the second exact component through native SetGraph', {
    action: 'call_actor_function', actorName, componentName: componentBName,
    functionName: 'SetGraph', arguments: { InGraph: graphObjectPath },
  }, { consent: { capability: 'control_actor.call_actor_function', acknowledge: 'elevated' } }),
  graphRevisionRead('Prove binding the second component preserved the saved graph revision'),
  componentRead('Read the first native binding and settings before saving the map', componentAInitialPath, graphObjectPath, componentASeed),
  componentRead('Read the second native binding and settings before saving the map', componentBInitialPath, graphObjectPath, componentBSeed),
  graphRevisionRead('Prove pre-save binding reads preserved the saved graph revision'),
  ...emptyComponentReads('the first bound component', componentAInitialPath),
  ...emptyComponentReads('the second bound component', componentBInitialPath),
  graphRevisionRead('Prove all pre-generation observations left the saved graph revision unchanged'),
  {
    scenario: 'Save only this selected owned map, never all dirty packages',
    toolName: 'manage_level', arguments: { action: 'save', levelPath: mapPath },
    expected: 'success', abortOnFailure: true,
    assertions: [{ path: 'structuredContent.result.savedAssetPath', equals: mapPath }],
    captureResult: [{ key: 'mapPath', fromField: 'result.savedAssetPath' }],
  },
  graphRevisionRead('Prove saving only the selected map preserved the graph revision'),
  componentInventory('Re-enumerate and capture the first exact component path after map save', componentAName, 'componentAPath', 'componentAName'),
  componentInventory('Re-enumerate and capture the second exact component path after map save', componentBName, 'componentBPath', 'componentBName'),
  componentRead('Verify the first post-save graph binding, manual flags, and seed', componentAPath, graphObjectPath, componentASeed),
  componentRead('Verify the second post-save graph binding, manual flags, and seed', componentBPath, graphObjectPath, componentBSeed),
  ...emptyComponentReads('the first saved bound component', componentAPath),
  ...emptyComponentReads('the second saved bound component', componentBPath),
  graphRevisionRead('Final stability check after saved component paths and settings were re-read'),
];

// The saved map, graph, actor, and components are intentional inputs to the
// separate cold-load suite. This suite does not clean or replace them.
runToolTests('manage_pcg_component_save', cases, {
  isolatedProject: {
    projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME,
    projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR,
    engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION,
  },
});
