import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const stamp = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const graphPathValue = `/Game/MCP_PCG_Validation/ComponentSetup_${stamp}/Graph`;
const actorName = `MCP_PCG_ComponentSetup_${stamp}`;
const componentAName = `MCP_PCG_Setup_A_${stamp}`;
const componentBName = `MCP_PCG_Setup_B_${stamp}`;
const captured = key => '${captured:' + key + '}';
const graphPath = captured('ownedGraph');
const graphObjectPath = `${graphPathValue}.Graph`;
const componentAPath = captured('componentA');
const componentBPath = captured('componentB');
const graphOutput = captured('graphOutput');
const graphOutputPin = captured('graphOutputPin');
const authoredRevision = captured('authoredRevision');
const operationId = captured('generationOperation');
const datasetId = captured('rootDataset');
const dataViewId = captured('pointView');

const pcg = (scenario, arguments_, extra = {}) => ({
  scenario,
  toolName: 'manage_pcg',
  arguments: arguments_,
  expected: 'success',
  abortOnFailure: true,
  ...extra,
});
const actor = (scenario, arguments_, extra = {}) => ({
  scenario,
  toolName: 'control_actor',
  arguments: arguments_,
  expected: 'success',
  abortOnFailure: true,
  ...extra,
});
const inspect = (scenario, arguments_, extra = {}) => ({
  scenario,
  toolName: 'inspect',
  arguments: arguments_,
  expected: 'success',
  abortOnFailure: true,
  ...extra,
});
const addOrEditComponent = (scenario, edit, fields) => actor(scenario, {
  action: 'edit_component', edit, ...fields,
});
const point = (x, y, z, rotation, scale3d, density, seed) => ({
  Transform: { translation: { x, y, z }, rotation, scale3d },
  Density: density,
  Seed: seed,
});
const readbackAssertions = expectedGraph => [
  { path: 'structuredContent.result.properties', length: 5 },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'Graph', propertyPath: 'GraphInstance.Graph', value: expectedGraph } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'GenerationTrigger', value: 'GenerateOnDemand' } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'bRegenerateInEditor', value: false } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'bActivated', value: true } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'Seed', value: 77 } },
];
const componentPropertyRead = (scenario, componentPath, expectedGraph) => inspect(scenario, {
  action: 'get_property',
  objectPath: componentPath,
  propertyNames: ['GraphInstance.Graph', 'GenerationTrigger', 'bRegenerateInEditor', 'bActivated', 'Seed'],
}, { assertions: readbackAssertions(expectedGraph) });
const graphRevisionRead = scenario => pcg(scenario, {
  action: 'inspect_pcg', view: 'graph', graphPath, includeSettings: true, limit: 8, edgeLimit: 8,
}, { assertions: [
  { path: 'structuredContent.result.revision', equals: authoredRevision },
  { path: 'structuredContent.result.items', length: 3 },
  { path: 'structuredContent.result.edges', length: 1 },
] });
const noOutputAssertions = [
  { path: 'structuredContent.result.generating', equals: false },
  { path: 'structuredContent.result.cleaning', equals: false },
  { path: 'structuredContent.result.instanceCount', equals: 0 },
  { path: 'structuredContent.result.outputDataCount', equals: 0 },
  { path: 'structuredContent.result.outputPointCount', equals: 0 },
  { path: 'structuredContent.result.outputs', length: 0 },
];
const pointTransformAssertions = (row, pointValue) => [
  ...['x', 'y', 'z'].map(axis => ({
    path: `structuredContent.result.rows.${row}.fields.transform.location.${axis}`,
    approximately: pointValue.location[axis], tolerance: 0.0001,
  })),
  ...['x', 'y', 'z', 'w'].map(axis => ({
    path: `structuredContent.result.rows.${row}.fields.transform.rotation.${axis}`,
    approximately: pointValue.rotation[axis], tolerance: 0.000001,
  })),
  ...['x', 'y', 'z'].map(axis => ({
    path: `structuredContent.result.rows.${row}.fields.transform.scale.${axis}`,
    approximately: pointValue.scale[axis], tolerance: 0.000001,
  })),
  { path: `structuredContent.result.rows.${row}.fields.density`, approximately: pointValue.density, tolerance: 0.000001 },
  { path: `structuredContent.result.rows.${row}.fields.seed`, equals: pointValue.seed },
];
const pointA = {
  location: { x: 123, y: -234, z: 45 },
  rotation: { x: 0, y: 0, z: 0.6, w: 0.8 },
  scale: { x: 1.25, y: 0.75, z: 1.5 },
  density: 0.3125,
  seed: -73,
};
const pointB = {
  location: { x: 613, y: -727, z: 89 },
  rotation: { x: 0, y: 0, z: -0.8, w: 0.6 },
  scale: { x: 0.5, y: 1.75, z: 2.25 },
  density: 0.8125,
  seed: 911,
};

const cases = [
  {
    scenario: 'Setup: create and own this run’s unique unsaved native PCG graph',
    toolName: 'manage_pcg',
    arguments: { action: 'edit_pcg_graph', edit: 'create', graphPath: graphPathValue, save: false },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.created', equals: true },
      { path: 'structuredContent.result.graphPath', equals: graphPathValue },
      { path: 'structuredContent.result.committed', equals: true },
      { path: 'structuredContent.result.saved', equals: false },
    ],
    captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }],
  },
  {
    scenario: 'Setup: spawn the uniquely owned bounds-valid StaticMeshActor',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: '/Script/Engine.StaticMeshActor',
      meshPath: '/Engine/BasicShapes/Cube.Cube',
      actorName,
      location: { x: 0, y: 0, z: 0 },
      scale: { x: 20, y: 20, z: 2 },
    },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.existsAfter', equals: true },
      { path: 'structuredContent.result.actorName', equals: actorName },
    ],
  },
  addOrEditComponent('Create the first fresh native PCG component through the generic component editor', 'add', {
    actorName, componentType: 'PCGComponent', componentName: componentAName,
  }),
  addOrEditComponent('Create the second fresh native PCG component through the generic component editor', 'add', {
    actorName, componentType: 'PCGComponent', componentName: componentBName,
  }),
  addOrEditComponent('Set the first component to manual generation with deterministic seed 77', 'set_properties', {
    actorName,
    componentName: componentAName,
    properties: { GenerationTrigger: 'GenerateOnDemand', bRegenerateInEditor: false, bActivated: true, Seed: 77 },
  }),
  addOrEditComponent('Set the second component to manual generation with deterministic seed 77', 'set_properties', {
    actorName,
    componentName: componentBName,
    properties: { GenerationTrigger: 'GenerateOnDemand', bRegenerateInEditor: false, bActivated: true, Seed: 77 },
  }),
  inspect('Discover only the first exact PCGComponent and capture its returned component path', {
    action: 'get_components', actorName, componentNames: [componentAName],
  }, {
    assertions: [
      { path: 'structuredContent.result.components', length: 1 },
      { path: 'structuredContent.result.components.0.name', equals: componentAName },
      { path: 'structuredContent.result.components.0.class', equals: '/Script/PCG.PCGComponent' },
    ],
    captureResult: [{ key: 'componentA', fromField: 'result.components.0.path' }],
  }),
  inspect('Discover only the second exact PCGComponent and capture its returned component path', {
    action: 'get_components', actorName, componentNames: [componentBName],
  }, {
    assertions: [
      { path: 'structuredContent.result.components', length: 1 },
      { path: 'structuredContent.result.components.0.name', equals: componentBName },
      { path: 'structuredContent.result.components.0.class', equals: '/Script/PCG.PCGComponent' },
    ],
    captureResult: [{ key: 'componentB', fromField: 'result.components.0.path' }],
  }),
  {
    scenario: 'Discover the created graph’s actual output node and its native input pin',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, limit: 8, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
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
  pcg('Author two deterministic CreatePoints rows and connect them to the discovered output pin', {
    action: 'batch_edit_pcg_graph',
    graphPath,
    expectedGraphRevision: captured('initialRevision'),
    save: false,
    operations: [
      {
        op: 'add_node',
        settingsClass: 'PCGCreatePointsSettings',
        tempId: 'points',
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
      { path: 'structuredContent.result.saved', equals: false },
    ],
    captureResult: [{ key: 'pointsNode', fromField: 'result.nodeIds.points' }],
  }),
  {
    scenario: 'Verify the authored CreatePoints node and exact connection to the discovered output pin',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, limit: 8, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 3 },
      { path: 'structuredContent.result.edges', length: 1 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: captured('pointsNode'), sourcePin: 'Out', targetNodeId: graphOutput, targetPin: graphOutputPin,
      } },
    ],
    captureResult: [{ key: 'authoredRevision', fromField: 'result.revision' }],
  },
  graphRevisionRead('Prove repeated authored graph reads are stable before either component binding'),
  actor('Bind the first exact component through its native SetGraph UFUNCTION', {
    action: 'call_actor_function', actorName, componentName: componentAName,
    functionName: 'SetGraph', arguments: { InGraph: graphObjectPath },
  }, {
    consent: { capability: 'control_actor.call_actor_function', acknowledge: 'elevated' },
  }),
  graphRevisionRead('Prove binding the first component preserves the authored graph revision'),
  actor('Bind the second exact component through its native SetGraph UFUNCTION', {
    action: 'call_actor_function', actorName, componentName: componentBName,
    functionName: 'SetGraph', arguments: { InGraph: graphObjectPath },
  }, {
    consent: { capability: 'control_actor.call_actor_function', acknowledge: 'elevated' },
  }),
  graphRevisionRead('Prove binding the second component preserves the authored graph revision'),
  componentPropertyRead('Verify the first exact graph binding and manual flags after SetGraph, before execution', componentAPath, graphObjectPath),
  componentPropertyRead('Verify the second exact graph binding and manual flags after SetGraph, before execution', componentBPath, graphObjectPath),
  graphRevisionRead('Prove exact component property reads preserve the authored graph revision'),
  actor('Deliberately refuse SetGraph for a nonexistent component without reaching a mutable target', {
    action: 'call_actor_function', actorName, componentName: `${componentAName}_NotPresent`,
    functionName: 'SetGraph', arguments: { InGraph: graphObjectPath },
  }, {
    expected: { condition: 'error', errorPattern: 'COMPONENT_NOT_FOUND' },
    consent: { capability: 'control_actor.call_actor_function', acknowledge: 'elevated' },
  }),
  componentPropertyRead('Prove the refused target left the first bound component unchanged', componentAPath, graphObjectPath),
  componentPropertyRead('Prove the refused target left the second bound component unchanged', componentBPath, graphObjectPath),
  ...[
    ['first', componentAPath],
    ['second', componentBPath],
  ].flatMap(([label, componentPath]) => [
    pcg(`Prove the ${label} bound component has no pre-execution work, outputs, or instances`, {
      action: 'get_pcg_component_results', componentPath, limit: 8, sampleLimit: 0,
    }, { assertions: noOutputAssertions }),
    pcg(`Prove the ${label} bound component has no pre-execution root datasets`, {
      action: 'list_pcg_data', componentPath, offset: 0, limit: 4,
    }, { assertions: [
      { path: 'structuredContent.result.items', length: 0 },
      { path: 'structuredContent.result.total', equals: 0 },
    ] }),
  ]),
  pcg('Explicitly generate only the already-bound first component, with a bounded native wait', {
    action: 'generate_pcg_graph',
    componentPath: componentAPath,
    force: true,
    timeoutSeconds: 25,
    expectedGraphRevision: authoredRevision,
    save: false,
  }, {
    timeoutMs: 30000,
    captureResult: [{ key: 'generationOperation', fromField: 'result.operationId' }],
  }),
  pcg('Poll the supported native operation status until the first component generation is terminal', {
    action: 'get_pcg_operation_status', componentPath: componentAPath, operationId,
  }, {
    timeoutMs: 30000,
    pollUntil: { timeoutMs: 25000, intervalMs: 1000 },
    assertions: [
      { path: 'structuredContent.result.completed', equals: true },
      { path: 'structuredContent.result.succeeded', equals: true },
      { path: 'structuredContent.result.engineStillRunning', equals: false },
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.nativeGenerated', equals: true },
    ],
  }),
  pcg('Verify native generation produced exactly two root points on the discovered output pin', {
    action: 'get_pcg_component_results', componentPath: componentAPath, limit: 8, sampleLimit: 0,
  }, {
    assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.cleaning', equals: false },
      { path: 'structuredContent.result.instanceCount', equals: 0 },
      { path: 'structuredContent.result.outputDataCount', equals: 1 },
      { path: 'structuredContent.result.outputPointCount', equals: 2 },
      { path: 'structuredContent.result.outputs', length: 1 },
      { path: 'structuredContent.result.outputs.0.pin', equals: graphOutputPin },
      { path: 'structuredContent.result.outputs.0.pointCount', equals: 2 },
    ],
  }),
  pcg('Discover the exact root point dataset from the first generated component', {
    action: 'list_pcg_data', componentPath: componentAPath, offset: 0, limit: 4,
  }, {
    assertions: [
      { path: 'structuredContent.result.items', length: 1 },
      { path: 'structuredContent.result.total', equals: 1 },
      { path: 'structuredContent.result.items.0.pin', equals: graphOutputPin },
      { path: 'structuredContent.result.items.0.available', equals: true },
    ],
    captureResult: [{ key: 'rootDataset', fromField: 'result.items.0.datasetId' }],
  }),
  pcg('Create a bounded typed view of the generated root transforms, densities, and seeds', {
    action: 'inspect_pcg', view: 'create_data_view',
    componentPath: componentAPath,
    datasetId,
    fields: ['transform', 'density', 'seed'],
    attributes: [],
    sourceOffset: 0,
    sourceLimit: 2,
  }, {
    assertions: [
      { path: 'structuredContent.result.projection.domain', equals: 'Elements' },
      { path: 'structuredContent.result.projection.sourceTotal', equals: 2 },
      { path: 'structuredContent.result.projection.rowCount', equals: 2 },
      { path: 'structuredContent.result.projection.fields', length: 3 },
    ],
    captureResult: [{ key: 'pointView', fromField: 'result.viewId' }],
  }),
  pcg('Read and verify every point location, quaternion component, scale axis, density, and seed', {
    action: 'read_pcg_data', componentPath: componentAPath, viewId: dataViewId, offset: 0, limit: 2,
  }, {
    assertions: [
      { path: 'structuredContent.result.rows', length: 2 },
      { path: 'structuredContent.result.rows.0.sourceIndex', equals: 0 },
      ...pointTransformAssertions(0, pointA),
      { path: 'structuredContent.result.rows.1.sourceIndex', equals: 1 },
      ...pointTransformAssertions(1, pointB),
    ],
  }),
  pcg('Prove generation left the second bound component without generated outputs or work', {
    action: 'get_pcg_component_results', componentPath: componentBPath, limit: 8, sampleLimit: 0,
  }, { assertions: noOutputAssertions }),
  pcg('Prove generation left the second bound component without root datasets', {
    action: 'list_pcg_data', componentPath: componentBPath, offset: 0, limit: 4,
  }, { assertions: [
    { path: 'structuredContent.result.items', length: 0 },
    { path: 'structuredContent.result.total', equals: 0 },
  ] }),
  pcg('Release the retained point view using the canonical view family call', {
    action: 'inspect_pcg', view: 'release_data_view', componentPath: componentAPath, handleId: dataViewId,
  }, { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg('Release the root dataset handle using the canonical view family call', {
    action: 'inspect_pcg', view: 'release_data_view', componentPath: componentAPath, handleId: datasetId,
  }, { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg('Clean only the first component’s generated resources with its current authored graph revision', {
    action: 'control_pcg_execution', control: 'cleanup', componentPath: componentAPath,
    expectedGraphRevision: authoredRevision, timeoutSeconds: 25, save: false,
  }, {
    timeoutMs: 30000,
    consent: { capability: 'manage_pcg.control_pcg_execution', acknowledge: 'explicit' },
    assertions: [
      { path: 'structuredContent.result.completed', equals: true },
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.engineStillRunning', equals: false },
      { path: 'structuredContent.result.nativeCleaned', equals: true },
    ],
  }),
  ...[
    ['first', componentAPath],
    ['second', componentBPath],
  ].flatMap(([label, componentPath]) => [
    pcg(`Verify the ${label} component is idle and has no managed instances or outputs after cleanup`, {
      action: 'get_pcg_component_results', componentPath, limit: 8, sampleLimit: 0,
    }, { pollUntil: { timeoutMs: 20000, intervalMs: 1000 }, assertions: noOutputAssertions }),
    pcg(`Verify the ${label} component has no remaining root datasets after cleanup`, {
      action: 'list_pcg_data', componentPath, offset: 0, limit: 4,
    }, { assertions: [
      { path: 'structuredContent.result.items', length: 0 },
      { path: 'structuredContent.result.total', equals: 0 },
    ] }),
  ]),
  actor('Cleanup: delete only this run’s uniquely named actor with explicit consent', {
    action: 'delete', actorNames: [actorName],
  }, { consent: { capability: 'control_actor.delete', acknowledge: 'explicit' } }),
  {
    scenario: 'Cleanup: delete only the graph asset proven created by this run',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: graphPath },
    expected: 'success',
    abortOnFailure: true,
    consent: { capability: 'asset.delete', acknowledge: 'elevated' },
  },
];

// Bounded recipe only: this does not test auto-generation across arbitrary graph
// classes, Blueprint components, graph reassignment, persistence, or project-wide discovery.
runToolTests('manage_pcg_component_setup', cases, {
  isolatedProject: {
    projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME,
    projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR,
    engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION,
  },
});
