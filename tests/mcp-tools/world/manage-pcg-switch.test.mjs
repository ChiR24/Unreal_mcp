import { randomUUID } from 'node:crypto';
import process from 'node:process';
import { runToolTests } from '../../test-runner.mjs';

const stamp = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const graphPath = `/Game/MCP_PCG_Validation/Switch_${stamp}/Graph`;
const actorName = `MCP_PCG_Switch_${stamp}`;
const optionA = `MCP_Switch_A_${stamp}`;
const optionB = `MCP_Switch_B_${stamp}`;
const optionC = `MCP_Switch_C_${stamp}`;
const captured = key => '${captured:' + key + '}';
const graphOutput = captured('graphOutput');
const graphOutputPin = captured('graphOutputPin');
const pointsNode = captured('pointsNode');
const switchNode = captured('switchNode');
const componentPath = captured('component');
const point = (x, y, z, density, seed) => ({
  Transform: {
    translation: { x, y, z },
    rotation: { x: 0, y: 0, z: 0, w: 1 },
    scale3d: { x: 1, y: 1, z: 1 },
  },
  Density: density,
  Seed: seed,
});
const pcg = (scenario, arguments_, extra = {}) => ({
  scenario,
  toolName: 'manage_pcg',
  arguments: arguments_,
  expected: 'success',
  ...extra,
});
const switchOptions = (first, second, selection) => ({
  SelectionMode: 'String',
  StringOptions: [first, second],
  StringSelection: selection,
});
const inspectSwitch = (scenario, revision, option1, option2, selection, extraAssertions = []) => ({
  scenario,
  toolName: 'manage_pcg',
  arguments: {
    action: 'inspect_pcg',
    view: 'graph',
    graphPath,
    nodeId: switchNode,
    includeSettings: true,
    offset: 0,
    limit: 8,
    edgeLimit: 8,
  },
  expected: 'success',
  abortOnFailure: true,
  assertions: [
    { path: 'structuredContent.result.revision', equals: revision },
    { path: 'structuredContent.result.items', length: 1 },
    { path: 'structuredContent.result.items.0.settingsClass', equals: '/Script/PCG.PCGSwitchSettings' },
    { path: 'structuredContent.result.items.0.settings.SelectionMode', equals: 'String' },
    { path: 'structuredContent.result.items.0.settings.StringOptions', length: 2 },
    { path: 'structuredContent.result.items.0.settings.StringOptions.0', equals: option1 },
    { path: 'structuredContent.result.items.0.settings.StringOptions.1', equals: option2 },
    { path: 'structuredContent.result.items.0.settings.StringSelection', equals: selection },
    { path: 'structuredContent.result.items.0.outputPins', length: 3 },
    { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: option1 } },
    { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: option2 } },
    { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: 'Default' } },
    ...extraAssertions,
  ],
});
const cases = [
  pcg('Setup: create this run’s exclusively owned unsaved PCG graph', {
    action: 'edit_pcg_graph',
    edit: 'create',
    graphPath,
    save: false,
  }, {
    assertions: [{ path: 'structuredContent.result.created', equals: true }],
    captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }],
  }),
  {
    scenario: 'Discover the actual initialized boundary IDs, default output pin, and authored revision',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, limit: 2, edgeLimit: 1, includeSettings: true },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.items', length: 2 },
      { path: 'structuredContent.result.items.0.input', equals: true },
      { path: 'structuredContent.result.items.1.output', equals: true },
      { path: 'structuredContent.result.items.1.inputPins', minLength: 1 },
      { path: 'structuredContent.result.items.1.inputPins.0.label', equals: 'Out' },
    ],
    captureResult: [
      { key: 'graphOutput', fromField: 'result.items.1.nodeId' },
      { key: 'graphOutputPin', fromField: 'result.items.1.inputPins.0.label' },
      { key: 'initialRevision', fromField: 'result.revision' },
    ],
  },
  pcg('Add one deterministic Create Points source and one native Switch before any component exists', {
    action: 'batch_edit_pcg_graph',
    graphPath,
    expectedGraphRevision: captured('initialRevision'),
    save: false,
    operations: [
      {
        op: 'add_node',
        settingsClass: '/Script/PCG.PCGCreatePointsSettings',
        tempId: 'points',
        settings: {
          bCullPointsOutsideVolume: false,
          PointsToCreate: [point(10, 20, 30, 0.25, -17), point(40, 50, 60, 0.75, 29)],
        },
      },
      {
        op: 'add_node',
        settingsClass: '/Script/PCG.PCGSwitchSettings',
        tempId: 'switch',
      },
      { op: 'connect', sourceNodeId: 'points', sourcePin: 'Out', targetNodeId: 'switch', targetPin: 'In' },
    ],
  }, {
    assertions: [
      { path: 'structuredContent.result.committed', equals: true },
      { path: 'structuredContent.result.saved', equals: false },
    ],
    captureResult: [
      { key: 'pointsNode', fromField: 'result.nodeIds.points' },
      { key: 'switchNode', fromField: 'result.nodeIds.switch' },
      { key: 'nodesRevision', fromField: 'result.revision' },
    ],
  }),
  pcg('Discover the actual existing Switch settings fields before configuring String mode', {
    action: 'inspect_pcg',
    view: 'node_schema',
    graphPath,
    nodeId: switchNode,
    offset: 0,
    limit: 32,
  }, {
    assertions: [
      { path: 'structuredContent.result.nodeId', equals: switchNode },
      { path: 'structuredContent.result.settingsClass', equals: '/Script/PCG.PCGSwitchSettings' },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'SelectionMode' } },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'StringOptions' } },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'StringSelection' } },
    ],
  }),
  pcg('Configure native String options A and B and select B through the structured writer', {
    action: 'edit_pcg_graph',
    edit: 'set_node_settings',
    graphPath,
    nodeId: switchNode,
    expectedGraphRevision: captured('nodesRevision'),
    settings: switchOptions(optionA, optionB, optionB),
    save: false,
  }),
  {
    scenario: 'Read back the actual B selection and the dynamic A, B, and Default output pins',
    toolName: 'manage_pcg',
    arguments: {
      action: 'inspect_pcg', view: 'graph', graphPath, nodeId: switchNode,
      includeSettings: true, offset: 0, limit: 8, edgeLimit: 8,
    },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 1 },
      { path: 'structuredContent.result.items.0.settingsClass', equals: '/Script/PCG.PCGSwitchSettings' },
      { path: 'structuredContent.result.items.0.settings.SelectionMode', equals: 'String' },
      { path: 'structuredContent.result.items.0.settings.StringOptions', length: 2 },
      { path: 'structuredContent.result.items.0.settings.StringOptions.0', equals: optionA },
      { path: 'structuredContent.result.items.0.settings.StringOptions.1', equals: optionB },
      { path: 'structuredContent.result.items.0.settings.StringSelection', equals: optionB },
      { path: 'structuredContent.result.items.0.outputPins', length: 3 },
      { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: optionA } },
      { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: optionB } },
      { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: 'Default' } },
    ],
    captureResult: [{ key: 'configuredRevision', fromField: 'result.revision' }],
  },
  pcg('Connect only the actual B output to the discovered graph output pin', {
    action: 'edit_pcg_graph',
    edit: 'connect_pins',
    graphPath,
    expectedGraphRevision: captured('configuredRevision'),
    sourceNodeId: switchNode,
    sourcePin: optionB,
    targetNodeId: graphOutput,
    targetPin: graphOutputPin,
    save: false,
  }, {
    assertions: [{ path: 'structuredContent.result.committed', equals: true }],
  }),
  {
    scenario: 'Verify the connected B edge and capture the exact pre-refusal graph state',
    toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'graph', graphPath, includeSettings: true, offset: 0, limit: 16, edgeLimit: 8 },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.edges', length: 2 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: pointsNode, sourcePin: 'Out', targetNodeId: switchNode, targetPin: 'In',
      } },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: switchNode, sourcePin: optionB, targetNodeId: graphOutput, targetPin: graphOutputPin,
      } },
    ],
    captureResult: [{ key: 'connectedRevision', fromField: 'result.revision' }],
  },
  pcg('Refuse removing connected option B while changing to option C', {
    action: 'edit_pcg_graph',
    edit: 'set_node_settings',
    graphPath,
    nodeId: switchNode,
    expectedGraphRevision: captured('connectedRevision'),
    settings: switchOptions(optionA, optionC, optionC),
    save: false,
  }, {
    expected: { condition: 'error', errorPattern: 'INVALID_SETTINGS' },
  }),
  {
    scenario: 'Prove the refusal preserved String options, B selection, dynamic pins, edge, and revision',
    toolName: 'manage_pcg',
    arguments: {
      action: 'inspect_pcg', view: 'graph', graphPath, nodeId: switchNode,
      includeSettings: true, offset: 0, limit: 8, edgeLimit: 8,
    },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.revision', equals: captured('connectedRevision') },
      { path: 'structuredContent.result.items.0.settings.SelectionMode', equals: 'String' },
      { path: 'structuredContent.result.items.0.settings.StringOptions', length: 2 },
      { path: 'structuredContent.result.items.0.settings.StringOptions.0', equals: optionA },
      { path: 'structuredContent.result.items.0.settings.StringOptions.1', equals: optionB },
      { path: 'structuredContent.result.items.0.settings.StringSelection', equals: optionB },
      { path: 'structuredContent.result.items.0.outputPins', length: 3 },
      { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: optionA } },
      { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: optionB } },
      { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: 'Default' } },
      { path: 'structuredContent.result.edges', length: 2 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: switchNode, sourcePin: optionB, targetNodeId: graphOutput, targetPin: graphOutputPin,
      } },
    ],
  },
  pcg('Explicitly disconnect only the owned B edge with destructive consent', {
    action: 'mutate_pcg_graph',
    mutation: 'disconnect',
    graphPath,
    expectedGraphRevision: captured('connectedRevision'),
    sourceNodeId: switchNode,
    sourcePin: optionB,
    targetNodeId: graphOutput,
    targetPin: graphOutputPin,
    save: false,
  }, {
    consent: { capability: 'manage_pcg.mutate_pcg_graph', acknowledge: 'explicit' },
    assertions: [{ path: 'structuredContent.result.committed', equals: true }],
  }),
  {
    scenario: 'Verify B is disconnected before changing its dynamic String option',
    toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'graph', graphPath, nodeId: switchNode, includeSettings: true, offset: 0, limit: 8, edgeLimit: 8 },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items.0.settings.StringOptions', length: 2 },
      { path: 'structuredContent.result.items.0.settings.StringOptions.1', equals: optionB },
      { path: 'structuredContent.result.items.0.settings.StringSelection', equals: optionB },
      { path: 'structuredContent.result.edges', length: 1 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: pointsNode, sourcePin: 'Out', targetNodeId: switchNode, targetPin: 'In',
      } },
    ],
    captureResult: [{ key: 'disconnectedRevision', fromField: 'result.revision' }],
  },
  pcg('Change the Switch options to A and C and select C before creating any component', {
    action: 'edit_pcg_graph',
    edit: 'set_node_settings',
    graphPath,
    nodeId: switchNode,
    expectedGraphRevision: captured('disconnectedRevision'),
    settings: switchOptions(optionA, optionC, optionC),
    save: false,
  }, {
    captureResult: [{ key: 'reconfiguredRevision', fromField: 'result.revision' }],
  }),
  inspectSwitch('Verify actual native C pin and C selection before reconnecting',
    captured('reconfiguredRevision'), optionA, optionC, optionC, [
      { path: 'structuredContent.result.edges', length: 1 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: pointsNode, sourcePin: 'Out', targetNodeId: switchNode, targetPin: 'In',
      } },
    ]),
  pcg('Reconnect only selected option C to the discovered graph output pin', {
    action: 'edit_pcg_graph',
    edit: 'connect_pins',
    graphPath,
    expectedGraphRevision: captured('reconfiguredRevision'),
    sourceNodeId: switchNode,
    sourcePin: optionC,
    targetNodeId: graphOutput,
    targetPin: graphOutputPin,
    save: false,
  }, {
    assertions: [{ path: 'structuredContent.result.committed', equals: true }],
    captureResult: [{ key: 'authoredRevision', fromField: 'result.revision' }],
  }),
  inspectSwitch('Verify the final graph selects C and has no Default or B route',
    captured('authoredRevision'), optionA, optionC, optionC, [
      { path: 'structuredContent.result.edges', length: 2 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: pointsNode, sourcePin: 'Out', targetNodeId: switchNode, targetPin: 'In',
      } },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: switchNode, sourcePin: optionC, targetNodeId: graphOutput, targetPin: graphOutputPin,
      } },
    ]),
  {
    scenario: 'Setup: spawn only this run’s bounds-valid actor after every graph edit is complete',
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
    assertions: [{ path: 'structuredContent.result.actorName', equals: actorName }],
  },
  {
    scenario: 'Execute the final C-only graph once on the owned actor and capture its component and operation',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath, actorName, componentName: 'PCGSwitchComponent', createComponent: true, save: false },
    expected: 'success',
    captureResult: [
      { key: 'component', fromField: 'result.componentPath' },
      { key: 'operation', fromField: 'result.operationId' },
    ],
  },
  {
    scenario: 'Wait for the exact native Switch operation to finish successfully',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_operation_status', componentPath, operationId: captured('operation') },
    expected: 'success',
    timeoutMs: 30000,
    pollUntil: { timeoutMs: 25000, intervalMs: 1000 },
    assertions: [
      { path: 'structuredContent.result.completed', equals: true },
      { path: 'structuredContent.result.engineStillRunning', equals: false },
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.nativeGenerated', equals: true },
    ],
  },
  pcg('Confirm the native root contains exactly two points after the C-only route', {
    action: 'inspect_pcg',
    view: 'component_results',
    componentPath,
    limit: 8,
    sampleLimit: 0,
  }, {
    assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.outputDataCount', equals: 1 },
      { path: 'structuredContent.result.outputPointCount', equals: 2 },
      { path: 'structuredContent.result.outputs', length: 1 },
      { path: 'structuredContent.result.outputs.0.pin', equals: graphOutputPin },
      { path: 'structuredContent.result.outputs.0.pointCount', equals: 2 },
    ],
  }),
  pcg('Find the actual generated root point dataset', {
    action: 'inspect_pcg',
    view: 'data',
    componentPath,
    offset: 0,
    limit: 4,
  }, {
    assertions: [
      { path: 'structuredContent.result.items', length: 1 },
      { path: 'structuredContent.result.total', equals: 1 },
      { path: 'structuredContent.result.items.0.pin', equals: graphOutputPin },
      { path: 'structuredContent.result.items.0.available', equals: true },
    ],
    captureResult: [{ key: 'dataset', fromField: 'result.items.0.datasetId' }],
  }),
  pcg('Create a bounded immutable CPU view over both root point rows', {
    action: 'inspect_pcg',
    view: 'create_data_view',
    componentPath,
    datasetId: captured('dataset'),
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
    captureResult: [{ key: 'dataView', fromField: 'result.viewId' }],
  }),
  pcg('Read exact transforms, densities, and seeds to prove C delivered both authored points', {
    action: 'inspect_pcg',
    view: 'data_rows',
    componentPath,
    viewId: captured('dataView'),
    offset: 0,
    limit: 2,
  }, {
    assertions: [
      { path: 'structuredContent.result.rows', length: 2 },
      { path: 'structuredContent.result.rowCount', equals: 2 },
      { path: 'structuredContent.result.hasMore', equals: false },
      { path: 'structuredContent.result.rows.0.sourceIndex', equals: 0 },
      { path: 'structuredContent.result.rows.0.fields.transform.location.x', approximately: 10, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.0.fields.transform.location.y', approximately: 20, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.0.fields.transform.location.z', approximately: 30, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.0.fields.transform.rotation.x', approximately: 0, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.transform.rotation.y', approximately: 0, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.transform.rotation.z', approximately: 0, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.transform.rotation.w', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.transform.scale.x', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.transform.scale.y', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.transform.scale.z', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.density', approximately: 0.25, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.seed', equals: -17 },
      { path: 'structuredContent.result.rows.1.sourceIndex', equals: 1 },
      { path: 'structuredContent.result.rows.1.fields.transform.location.x', approximately: 40, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.1.fields.transform.location.y', approximately: 50, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.1.fields.transform.location.z', approximately: 60, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.1.fields.transform.rotation.x', approximately: 0, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.rotation.y', approximately: 0, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.rotation.z', approximately: 0, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.rotation.w', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.scale.x', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.scale.y', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.scale.z', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.density', approximately: 0.75, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.seed', equals: 29 },
    ],
  }),
  pcg('Release the immutable point view before component cleanup', {
    action: 'inspect_pcg', view: 'release_data_view', componentPath, handleId: captured('dataView'),
  }, { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg('Release the root dataset handle before component cleanup', {
    action: 'inspect_pcg', view: 'release_data_view', componentPath, handleId: captured('dataset'),
  }, { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg('Cleanup only the generated resources on this component with exact destructive consent', {
    action: 'control_pcg_execution',
    control: 'cleanup',
    componentPath,
    expectedGraphRevision: captured('authoredRevision'),
    timeoutSeconds: 20,
    save: false,
  }, {
    timeoutMs: 30000,
    consent: { capability: 'manage_pcg.control_pcg_execution', acknowledge: 'explicit' },
    assertions: [{ path: 'structuredContent.result.state', equals: 'succeeded' }],
  }),
  {
    scenario: 'Verify cleanup left no instances, root outputs, or active native work',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 },
    expected: 'success',
    pollUntil: { timeoutMs: 20000, intervalMs: 1000 },
    assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.cleaning', equals: false },
      { path: 'structuredContent.result.instanceCount', equals: 0 },
      { path: 'structuredContent.result.outputDataCount', equals: 0 },
    ],
  },
  pcg('Verify no point-data roots remain after cleanup', {
    action: 'inspect_pcg', view: 'data', componentPath, offset: 0, limit: 4,
  }, {
    assertions: [
      { path: 'structuredContent.result.items', length: 0 },
      { path: 'structuredContent.result.total', equals: 0 },
    ],
  }),
  {
    scenario: 'Cleanup: delete only this run’s UUID-named actor with explicit consent',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorNames: [actorName] },
    expected: 'success',
    consent: { capability: 'control_actor.delete', acknowledge: 'explicit' },
  },
  {
    scenario: 'Cleanup: delete only the graph proven created by this run',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: captured('ownedGraph') },
    expected: 'success',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' },
  },
];

runToolTests('manage_pcg_switch', cases.map(testCase => ({ ...testCase, abortOnFailure: true })), {
  isolatedProject: {
    projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME,
    projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR,
    engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION,
  },
});
