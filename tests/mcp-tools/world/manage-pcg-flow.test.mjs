import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const stamp = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const subgraphPath = `/Game/MCP_PCG_Validation/Flow_${stamp}/Subgraph`;
const actorName = `MCP_PCG_Flow_${stamp}`;
const pinA = 'MCP_Flow_A';
const pinB = 'MCP_Flow_B';
const pinC = 'MCP_Flow_C';
const captured = key => '${captured:' + key + '}';
const graphPath = captured('ownedGraph');
const graphOutput = captured('graphOutput');
const branchNode = captured('branchNode');
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
const pointPin = label => ({
  Label: label,
  AllowedTypes: {
    Ids: [{ Struct: '/Script/PCG.PCGDataTypeInfoPoint' }],
    CustomSubtype: -1,
  },
});
const outputBoundarySettings = (...labels) => ({ Pins: labels.map(pointPin) });
const pcg = (scenario, arguments_, extra = {}) => ({
  scenario,
  toolName: 'manage_pcg',
  arguments: arguments_,
  expected: 'success',
  abortOnFailure: true,
  ...extra,
});
const cases = [
  {
    scenario: 'Setup: create this run’s standalone native PCG subgraph asset',
    toolName: 'manage_pcg',
    arguments: { action: 'edit_pcg_graph', edit: 'create_subgraph', subgraphPath, save: false },
    expected: 'success',
    abortOnFailure: true,
    assertions: [{ path: 'structuredContent.result.created', equals: true }],
    captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }],
  },
  {
    scenario: 'Discover initialized graph boundaries, actual pins, and the authored revision',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, limit: 2, edgeLimit: 1, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 2 },
      { path: 'structuredContent.result.items.0.input', equals: true },
      { path: 'structuredContent.result.items.1.output', equals: true },
      { path: 'structuredContent.result.items.1.inputPins', minLength: 1 },
      { path: 'structuredContent.result.items.1.settings.$class', equals: '/Script/PCG.PCGGraphInputOutputSettings' },
    ],
    captureResult: [
      { key: 'graphInput', fromField: 'result.items.0.nodeId' },
      { key: 'graphOutput', fromField: 'result.items.1.nodeId' },
      { key: 'initialRevision', fromField: 'result.revision' },
    ],
  },
  {
    scenario: 'Discover the existing output-boundary Pins property before editing its interface',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_node_schema', graphPath, nodeId: graphOutput, limit: 32 },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.nodeId', equals: graphOutput },
      { path: 'structuredContent.result.settingsClass', equals: '/Script/PCG.PCGGraphInputOutputSettings' },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'Pins', type: 'Array' } },
    ],
  },
  pcg('Author two native point-only output interface pins on the owned subgraph', {
    action: 'edit_pcg_graph', edit: 'set_node_settings',
    graphPath,
    nodeId: graphOutput,
    expectedGraphRevision: captured('initialRevision'),
    settings: outputBoundarySettings(pinA, pinB),
    save: false,
  }),
  {
    scenario: 'Verify the authored interface labels and capture the exact pre-batch revision',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, nodeId: graphOutput, limit: 1, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 1 },
      { path: 'structuredContent.result.items.0.output', equals: true },
      { path: 'structuredContent.result.items.0.settings.Pins', length: 2 },
      { path: 'structuredContent.result.items.0.settings.Pins.0', includes: pinA },
      { path: 'structuredContent.result.items.0.settings.Pins.1', includes: pinB },
      { path: 'structuredContent.result.items.0.inputPins', length: 2 },
      { path: 'structuredContent.result.items.0.inputPins', includesObject: { label: pinA } },
      { path: 'structuredContent.result.items.0.inputPins', includesObject: { label: pinB } },
    ],
    captureResult: [{ key: 'interfaceRevision', fromField: 'result.revision' }],
  },
  pcg('Author deterministic points and a native Branch routed to Output B', {
    action: 'batch_edit_pcg_graph',
    graphPath,
    expectedGraphRevision: captured('interfaceRevision'),
    save: false,
    operations: [
      {
        op: 'add_node',
        settingsClass: 'PCGCreatePointsSettings',
        tempId: 'points',
        settings: {
          bCullPointsOutsideVolume: false,
          PointsToCreate: [point(10, 20, 30, 0.25, -17), point(40, 50, 60, 0.75, 29)],
        },
      },
      {
        op: 'add_node',
        settingsClass: 'PCGBranchSettings',
        tempId: 'branch',
        settings: { bOutputToB: true },
      },
      { op: 'connect', sourceNodeId: 'points', sourcePin: 'Out', targetNodeId: 'branch', targetPin: 'In' },
      { op: 'connect', sourceNodeId: 'branch', sourcePin: 'Output B', targetNodeId: graphOutput, targetPin: pinB },
    ],
  }, {
    assertions: [
      { path: 'structuredContent.result.committed', equals: true },
      { path: 'structuredContent.result.saved', equals: false },
    ],
    captureResult: [
      { key: 'pointsNode', fromField: 'result.nodeIds.points' },
      { key: 'branchNode', fromField: 'result.nodeIds.branch' },
    ],
  }),
  {
    scenario: 'Read the native Branch instance and verify its actual fixed flow pins',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, nodeId: branchNode, limit: 1, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 1 },
      { path: 'structuredContent.result.items.0.settingsClass', equals: '/Script/PCG.PCGBranchSettings' },
      { path: 'structuredContent.result.items.0.settings.bOutputToB', equals: true },
      { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: 'Output A' } },
      { path: 'structuredContent.result.items.0.outputPins', includesObject: { label: 'Output B' } },
    ],
  },
  {
    scenario: 'Verify the connected multi-pin interface and capture its exact revision',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, nodeId: graphOutput, limit: 1, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items.0.settings.Pins', length: 2 },
      { path: 'structuredContent.result.items.0.settings.Pins.0', includes: pinA },
      { path: 'structuredContent.result.items.0.settings.Pins.1', includes: pinB },
      { path: 'structuredContent.result.items.0.inputPins', length: 2 },
      { path: 'structuredContent.result.items.0.inputPins', includesObject: { label: pinA } },
      { path: 'structuredContent.result.items.0.inputPins', includesObject: { label: pinB } },
      { path: 'structuredContent.result.edges', length: 2 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: captured('pointsNode'), sourcePin: 'Out', targetNodeId: branchNode, targetPin: 'In',
      } },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: branchNode, sourcePin: 'Output B', targetNodeId: graphOutput, targetPin: pinB,
      } },
    ],
    captureResult: [{ key: 'connectedRevision', fromField: 'result.revision' }],
  },
  pcg('Refuse removing a connected output pin during staged settings preflight', {
    action: 'edit_pcg_graph', edit: 'set_node_settings',
    graphPath,
    nodeId: graphOutput,
    expectedGraphRevision: captured('connectedRevision'),
    settings: outputBoundarySettings(pinA, pinC),
    save: false,
  }, {
    expected: { condition: 'error', errorPattern: 'INVALID_SETTINGS' },
  }),
  {
    scenario: 'Prove the rejected edit left the exact authored pins, edge, and revision unchanged',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, nodeId: graphOutput, limit: 1, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.revision', equals: captured('connectedRevision') },
      { path: 'structuredContent.result.items.0.settings.Pins', length: 2 },
      { path: 'structuredContent.result.items.0.settings.Pins.0', includes: pinA },
      { path: 'structuredContent.result.items.0.settings.Pins.1', includes: pinB },
      { path: 'structuredContent.result.items.0.inputPins', includesObject: { label: pinB } },
      { path: 'structuredContent.result.edges', length: 2 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: branchNode, sourcePin: 'Output B', targetNodeId: graphOutput, targetPin: pinB,
      } },
    ],
  },
  pcg('Explicitly disconnect the owned Output B edge with destructive consent', {
    action: 'mutate_pcg_graph', mutation: 'disconnect',
    graphPath,
    sourceNodeId: branchNode,
    sourcePin: 'Output B',
    targetNodeId: graphOutput,
    targetPin: pinB,
    save: false,
  }, {
    consent: { capability: 'manage_pcg.mutate_pcg_graph', acknowledge: 'explicit' },
    assertions: [{ path: 'structuredContent.result.committed', equals: true }],
  }),
  {
    scenario: 'Verify the explicit disconnect removed only the intended edge before editing the pin',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, nodeId: graphOutput, limit: 1, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items.0.settings.Pins', length: 2 },
      { path: 'structuredContent.result.items.0.settings.Pins.1', includes: pinB },
      { path: 'structuredContent.result.edges', length: 1 },
    ],
    captureResult: [{ key: 'disconnectedRevision', fromField: 'result.revision' }],
  },
  pcg('Replace the disconnected interface pin through the structured settings writer', {
    action: 'edit_pcg_graph', edit: 'set_node_settings',
    graphPath,
    nodeId: graphOutput,
    expectedGraphRevision: captured('disconnectedRevision'),
    settings: outputBoundarySettings(pinA, pinC),
    save: false,
  }),
  {
    scenario: 'Read back the changed interface labels and capture the reconnect revision',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, nodeId: graphOutput, limit: 1, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items.0.settings.Pins', length: 2 },
      { path: 'structuredContent.result.items.0.settings.Pins.0', includes: pinA },
      { path: 'structuredContent.result.items.0.settings.Pins.1', includes: pinC },
      { path: 'structuredContent.result.items.0.inputPins', length: 2 },
      { path: 'structuredContent.result.items.0.inputPins', includesObject: { label: pinA } },
      { path: 'structuredContent.result.items.0.inputPins', includesObject: { label: pinC } },
      { path: 'structuredContent.result.edges', length: 1 },
    ],
    captureResult: [{ key: 'reconfiguredRevision', fromField: 'result.revision' }],
  },
  pcg('Reconnect the Branch Output B stream to the newly authored interface pin', {
    action: 'edit_pcg_graph', edit: 'connect_pins',
    graphPath,
    expectedGraphRevision: captured('reconfiguredRevision'),
    sourceNodeId: branchNode,
    sourcePin: 'Output B',
    targetNodeId: graphOutput,
    targetPin: pinC,
    save: false,
  }, {
    assertions: [{ path: 'structuredContent.result.committed', equals: true }],
  }),
  {
    scenario: 'Verify the final native edge targets the replacement output pin',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, nodeId: graphOutput, limit: 1, edgeLimit: 8, includeSettings: true },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items.0.settings.Pins.1', includes: pinC },
      { path: 'structuredContent.result.edges', length: 2 },
      { path: 'structuredContent.result.edges', includesObject: {
        sourceNodeId: branchNode, sourcePin: 'Output B', targetNodeId: graphOutput, targetPin: pinC,
      } },
    ],
    captureResult: [{ key: 'authoredRevision', fromField: 'result.revision' }],
  },
  {
    scenario: 'Setup: spawn the uniquely owned bounds-valid generation actor',
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
    assertions: [{ path: 'structuredContent.result.actorName', equals: actorName }],
  },
  {
    scenario: 'Execute the fully rewired subgraph on only this actor and capture its owned component',
    toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath, actorName, componentName: 'PCGFlowComponent', createComponent: true, save: false },
    expected: 'success',
    abortOnFailure: true,
    captureResult: [
      { key: 'component', fromField: 'result.componentPath' },
      { key: 'operation', fromField: 'result.operationId' },
    ],
  },
  {
    scenario: 'Wait for native Branch flow execution to complete successfully',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_operation_status', componentPath, operationId: captured('operation') },
    expected: 'success',
    timeoutMs: 30000,
    abortOnFailure: true,
    pollUntil: { timeoutMs: 25000, intervalMs: 1000 },
    assertions: [
      { path: 'structuredContent.result.completed', equals: true },
      { path: 'structuredContent.result.engineStillRunning', equals: false },
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.nativeGenerated', equals: true },
    ],
  },
  {
    scenario: 'Confirm the real component root contains two points on only the replacement pin',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, limit: 8, sampleLimit: 0 },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.outputDataCount', equals: 1 },
      { path: 'structuredContent.result.outputPointCount', equals: 2 },
      { path: 'structuredContent.result.outputs', length: 1 },
      { path: 'structuredContent.result.outputs.0.pin', equals: pinC },
      { path: 'structuredContent.result.outputs.0.pointCount', equals: 2 },
    ],
  },
  {
    scenario: 'Discover the actual root point dataset produced on the replacement interface pin',
    toolName: 'manage_pcg',
    arguments: { action: 'list_pcg_data', componentPath, offset: 0, limit: 4 },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 1 },
      { path: 'structuredContent.result.total', equals: 1 },
      { path: 'structuredContent.result.items.0.pin', equals: pinC },
      { path: 'structuredContent.result.items.0.available', equals: true },
    ],
    captureResult: [{ key: 'dataset', fromField: 'result.items.0.datasetId' }],
  },
  pcg('Create a bounded immutable two-row CPU view of the actual root point data', {
    action: 'create_pcg_data_view',
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
  pcg('Read exact point coordinates, rotations, scales, densities, and seeds from the retained view', {
    action: 'read_pcg_data',
    componentPath,
    viewId: captured('dataView'),
    offset: 0,
    limit: 2,
  }, {
    assertions: [
      { path: 'structuredContent.result.rows', length: 2 },
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
      { path: 'structuredContent.result.rows.1.fields.transform.rotation.w', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.scale.x', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.scale.y', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.transform.scale.z', approximately: 1, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.density', approximately: 0.75, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.1.fields.seed', equals: 29 },
    ],
  }),
  pcg('Release the retained immutable point view before component cleanup', {
    action: 'release_pcg_data_view', componentPath, handleId: captured('dataView'),
  }, { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg('Release the root dataset handle before component cleanup', {
    action: 'release_pcg_data_view', componentPath, handleId: captured('dataset'),
  }, { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg('Cleanup only the generated resources on this run’s owned PCG component', {
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
    scenario: 'Verify native cleanup left no managed instances, outputs, or active work',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 },
    expected: 'success',
    abortOnFailure: true,
    pollUntil: { timeoutMs: 20000, intervalMs: 1000 },
    assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.cleaning', equals: false },
      { path: 'structuredContent.result.instanceCount', equals: 0 },
      { path: 'structuredContent.result.outputDataCount', equals: 0 },
    ],
  },
  {
    scenario: 'Verify no root datasets remain after native cleanup',
    toolName: 'manage_pcg',
    arguments: { action: 'list_pcg_data', componentPath, offset: 0, limit: 4 },
    expected: 'success',
    abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 0 },
      { path: 'structuredContent.result.total', equals: 0 },
    ],
  },
  {
    scenario: 'Cleanup: delete only this run’s uniquely named actor with explicit consent',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorNames: [actorName] },
    expected: 'success',
    abortOnFailure: true,
    consent: { capability: 'control_actor.delete', acknowledge: 'explicit' },
  },
  {
    scenario: 'Cleanup: delete only the subgraph asset proven created by this run',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: graphPath },
    expected: 'success',
    abortOnFailure: true,
    consent: { capability: 'asset.delete', acknowledge: 'elevated' },
  },
];

// Scope stop: this standalone child interface does not test parent propagation,
// loop/feedback execution, save/cold-load persistence, Undo, or native editor visuals.
// Switch/Select option-cache notification semantics require separate behavioral coverage.
runToolTests('manage_pcg_flow', cases);
