import { randomUUID } from 'node:crypto';
import process from 'node:process';
import { runToolTests } from '../../test-runner.mjs';

const stamp = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const graphPath = `/Game/MCP_PCG_Validation/SpawnerTypes_${stamp}/Graph`;
const actorName = `MCP_PCG_SpawnerTypes_${stamp}`;
const meshAttribute = `MCP_MeshPath_${stamp.replaceAll('_', '')}`;
const captured = key => '${captured:' + key + '}';
const componentPath = captured('component');
const settingsClass = '/Script/PCG.PCGStaticMeshSpawnerSettings';
const weightedClass = '/Script/PCG.PCGMeshSelectorWeighted';
const byAttributeClass = '/Script/PCG.PCGMeshSelectorByAttribute';
const packerClass = '/Script/PCG.PCGInstanceDataPackerByAttribute';
const ismClass = '/Script/Engine.InstancedStaticMeshComponent';
const selectorText = 'PCGBegin($Density)PCGEnd';
const cubePath = '/Engine/BasicShapes/Cube.Cube';
const spherePath = '/Engine/BasicShapes/Sphere.Sphere';
const point = (x, y, z, density, seed) => ({
  Transform: {
    translation: { x, y, z },
    rotation: { x: 0, y: 0, z: 0, w: 1 },
    scale3d: { x: 1, y: 1, z: 1 },
  },
  Density: density,
  Seed: seed,
});
const pcg = (scenario, action, args, extra = {}) => ({ scenario, toolName: 'manage_pcg',
  arguments: { action, ...args }, expected: 'success', ...extra });
const editNode = (scenario, settings, extra = {}) => pcg(scenario, 'edit_pcg_graph', {
  edit: 'set_node_settings', graphPath, nodeId: captured('spawnerNode'), settings, save: false,
}, extra);
const rejectSettings = (scenario, settings) => editNode(scenario, settings, {
  expected: { condition: 'error', errorPattern: 'INVALID_SETTINGS' },
});
const weightedStateAssertions = () => [
  { path: 'structuredContent.result.items', length: 1 },
  { path: 'structuredContent.result.items.0.settingsClass', equals: settingsClass },
  { path: 'structuredContent.result.items.0.settings.MeshSelectorType', equals: weightedClass },
  { path: 'structuredContent.result.items.0.settings.MeshSelectorParameters', equals: captured('weightedSelectorPath') },
];
const verifyWeightedState = (scenario, revision) => [
  { scenario, toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'graph', graphPath, nodeId: captured('spawnerNode'), includeSettings: true },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.revision', equals: captured(revision) },
      ...weightedStateAssertions(),
    ] },
  { scenario: `${scenario}: inspect the preserved old weighted selector entry through its captured object path`, toolName: 'inspect',
    arguments: { action: 'get_property', objectPath: captured('weightedSelectorPath'), propertyName: 'MeshEntries' },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.value', length: 1 },
      { path: 'structuredContent.result.value.0', includes: 'Sphere.Sphere' },
      { path: 'structuredContent.result.value.0', includes: 'Weight=7' },
    ] },
];
const invalidByAttributeSettings = (additional = {}) => ({
  'MeshSelectorParameters.AttributeName': meshAttribute,
  'MeshSelectorParameters.TemplateDescriptor.ComponentClass': ismClass,
  'InstanceDataPackerParameters.AttributeSelectors': [selectorText],
  InstanceDataPackerType: packerClass,
  MeshSelectorType: byAttributeClass,
  ...additional,
});
const waitForGeneration = (scenario, operation = captured('operation')) => ({
  scenario, toolName: 'manage_pcg',
  arguments: { action: 'get_pcg_operation_status', componentPath, operationId: operation },
  expected: 'success', timeoutMs: 30000,
  pollUntil: { timeoutMs: 25000, intervalMs: 1000 },
  assertions: [
    { path: 'structuredContent.result.completed', equals: true },
    { path: 'structuredContent.result.engineStillRunning', equals: false },
    { path: 'structuredContent.result.state', equals: 'succeeded' },
    { path: 'structuredContent.result.nativeGenerated', equals: true },
  ],
});
const readGeneratedData = (label, index, attributeValue, density, seed, x, y, z) => [
  { scenario: `${label}: create a typed one-row view for the exact generated branch ${index}`, toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'create_data_view', componentPath, datasetId: captured(`dataset${index}`),
      fields: ['transform', 'density', 'seed'], attributes: [meshAttribute], sourceOffset: 0, sourceLimit: 1 },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.projection.domain', equals: 'Elements' },
      { path: 'structuredContent.result.projection.sourceTotal', equals: 1 },
      { path: 'structuredContent.result.projection.rowCount', equals: 1 },
      { path: 'structuredContent.result.projection.attributes.0', equals: meshAttribute },
    ], captureResult: [{ key: `dataView${index}`, fromField: 'result.viewId' }] },
  { scenario: `${label}: read the real mesh path and authored point fields for branch ${index}`, toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'data_rows', componentPath, viewId: captured(`dataView${index}`), offset: 0, limit: 1 },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.rows', length: 1 },
      { path: 'structuredContent.result.rows.0.sourceIndex', equals: 0 },
      { path: 'structuredContent.result.rows.0.fields.transform.location.x', approximately: x, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.0.fields.transform.location.y', approximately: y, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.0.fields.transform.location.z', approximately: z, tolerance: 0.0001 },
      { path: 'structuredContent.result.rows.0.fields.density', approximately: density, tolerance: 0.000001 },
      { path: 'structuredContent.result.rows.0.fields.seed', equals: seed },
      { path: `structuredContent.result.rows.0.attributes.${meshAttribute}`, equals: attributeValue },
    ] },
  pcg(`${label}: release branch ${index}'s immutable view`, 'inspect_pcg',
    { view: 'release_data_view', componentPath, handleId: captured(`dataView${index}`) },
    { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg(`${label}: release branch ${index}'s dataset root`, 'inspect_pcg',
    { view: 'release_data_view', componentPath, handleId: captured(`dataset${index}`) },
    { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
];
const readCustomData = (scenario, componentKey, expectedFloatCount, expectedValues) => ({
  scenario, toolName: 'inspect',
  arguments: { action: 'get_property', objectPath: captured(componentKey),
    propertyNames: ['NumCustomDataFloats', 'PerInstanceSMCustomData'] },
  expected: 'success', assertions: [
    { path: 'structuredContent.result.properties', length: 2 },
    { path: 'structuredContent.result.properties.0.propertyName', equals: 'NumCustomDataFloats' },
    { path: 'structuredContent.result.properties.0.value', equals: expectedFloatCount },
    { path: 'structuredContent.result.properties.1.propertyName', equals: 'PerInstanceSMCustomData' },
    { path: 'structuredContent.result.properties.1.value', length: expectedValues.length },
    ...expectedValues.map((value, index) => ({
      path: `structuredContent.result.properties.1.value.${index}`, approximately: value, tolerance: 0.000001,
    })),
  ],
});
const cases = [
  { scenario: 'Setup: create only this UUID-owned PCG graph without saving', toolName: 'manage_pcg',
    arguments: { action: 'edit_pcg_graph', edit: 'create', graphPath, save: false }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.created', equals: true }],
    captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }] },
  { scenario: 'Discover the graph output node ID and the empty authored revision', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'graph', graphPath, includeSettings: false, offset: 0, limit: 2 },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.items', length: 2 },
      { path: 'structuredContent.result.items.0.input', equals: true },
      { path: 'structuredContent.result.items.1.output', equals: true },
    ], captureResult: [
      { key: 'graphOutput', fromField: 'result.items.1.nodeId' },
      { key: 'emptyRevision', fromField: 'result.revision' },
    ] },
  { scenario: 'Author two distinct point branches, SoftObjectPath attributes, and one default weighted spawner atomically', toolName: 'manage_pcg',
    arguments: { action: 'batch_edit_pcg_graph', graphPath, expectedGraphRevision: captured('emptyRevision'), save: false,
      operations: [
        { op: 'add_node', settingsClass: 'PCGCreatePointsSettings', tempId: 'cubePoints', settings: {
          bCullPointsOutsideVolume: false, PointsToCreate: [point(10, 20, 30, 0.25, -17)],
        } },
        { op: 'add_node', settingsClass: 'PCGAddAttributeSettings', tempId: 'cubeMesh', settings: {
          OutputTarget: { Selection: 'Attribute', AttributeName: meshAttribute },
          AttributeTypes: { Type: 'SoftObjectPath', SoftObjectPathValue: cubePath },
        } },
        { op: 'add_node', settingsClass: 'PCGCreatePointsSettings', tempId: 'spherePoints', settings: {
          bCullPointsOutsideVolume: false, PointsToCreate: [point(40, 50, 60, 0.75, 29)],
        } },
        { op: 'add_node', settingsClass: 'PCGAddAttributeSettings', tempId: 'sphereMesh', settings: {
          OutputTarget: { Selection: 'Attribute', AttributeName: meshAttribute },
          AttributeTypes: { Type: 'SoftObjectPath', SoftObjectPathValue: spherePath },
        } },
        { op: 'add_node', settingsClass, tempId: 'spawner' },
        { op: 'connect', sourceNodeId: 'cubePoints', sourcePin: 'Out', targetNodeId: 'cubeMesh', targetPin: 'In' },
        { op: 'connect', sourceNodeId: 'cubeMesh', sourcePin: 'Out', targetNodeId: 'spawner', targetPin: 'In' },
        { op: 'connect', sourceNodeId: 'spherePoints', sourcePin: 'Out', targetNodeId: 'sphereMesh', targetPin: 'In' },
        { op: 'connect', sourceNodeId: 'sphereMesh', sourcePin: 'Out', targetNodeId: 'spawner', targetPin: 'In' },
        { op: 'connect', sourceNodeId: 'spawner', sourcePin: 'Out', targetNodeId: captured('graphOutput'), targetPin: 'Out' },
      ] }, expected: 'success',
    captureResult: [{ key: 'cubePointsNode', fromField: 'result.nodeIds.cubePoints' },
      { key: 'cubeMeshNode', fromField: 'result.nodeIds.cubeMesh' },
      { key: 'spherePointsNode', fromField: 'result.nodeIds.spherePoints' },
      { key: 'sphereMeshNode', fromField: 'result.nodeIds.sphereMesh' },
      { key: 'spawnerNode', fromField: 'result.nodeIds.spawner' },
      { key: 'authoredRevision', fromField: 'result.revision' }] },
  { scenario: 'Discover native selector and packer class constraints on the exact spawner instance', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'node_schema', graphPath, nodeId: captured('spawnerNode'), offset: 0, limit: 32 },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.nodeId', equals: captured('spawnerNode') },
      { path: 'structuredContent.result.settingsClass', equals: settingsClass },
      { path: 'structuredContent.result.schemaSource', equals: 'existing_node_instance_metadata_only' },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'MeshSelectorType', type: 'Class',
        typeDetails: { classConstraintPath: '/Script/PCG.PCGMeshSelectorBase' } } },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'InstanceDataPackerType', type: 'Class',
        typeDetails: { classConstraintPath: '/Script/PCG.PCGInstanceDataPackerBase' } } },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'MeshSelectorParameters', readOnly: true, instancedReference: true } },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'InstanceDataPackerParameters', readOnly: true, instancedReference: true } },
    ] },
  { scenario: 'Discover the existing initialized weighted selector and its MeshEntries struct descriptor', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'node_schema', graphPath, nodeId: captured('spawnerNode'),
      scopePath: 'MeshSelectorParameters', offset: 0, limit: 32 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.scopeKind', equals: 'owned_object' },
      { path: 'structuredContent.result.scopeType', equals: weightedClass },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'MeshEntries', type: 'Array',
        typeDetails: { container: 'array', element: { structPath: '/Script/PCG.PCGMeshSelectorWeightedEntry' } } } },
    ] },
  editNode('Initialize a known weighted baseline entry on the existing native selector', {
    MeshSelectorType: weightedClass,
    'MeshSelectorParameters.MeshEntries': [{ Descriptor: { StaticMesh: spherePath, ComponentClass: ismClass }, Weight: 7 }],
  }, { assertions: [{ path: 'structuredContent.result.committed', equals: true }] }),
  { scenario: 'Capture the nonempty graph revision and exact actual selector object path before negative preflights', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'graph', graphPath, nodeId: captured('spawnerNode'), includeSettings: true },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.items', length: 1 },
      { path: 'structuredContent.result.items.0.settingsClass', equals: settingsClass },
      { path: 'structuredContent.result.items.0.settings.MeshSelectorType', equals: weightedClass },
    ], captureResult: [
      { key: 'baselineRevision', fromField: 'result.revision' },
      { key: 'weightedSelectorPath', fromField: 'result.items.0.settings.MeshSelectorParameters' },
    ] },

  rejectSettings('Reject an unknown selector class before changing the graph', {
    MeshSelectorType: '/Script/PCG.MCP_MissingMeshSelector',
  }),
  ...verifyWeightedState('Unknown selector class refusal preserves revision, actual selector type, and entry', 'baselineRevision'),
  rejectSettings('Reject the abstract selector base class', { MeshSelectorType: '/Script/PCG.PCGMeshSelectorBase' }),
  ...verifyWeightedState('Abstract selector refusal preserves the weighted baseline', 'baselineRevision'),
  rejectSettings('Reject a packer class assigned to the selector class property', {
    MeshSelectorType: packerClass,
  }),
  ...verifyWeightedState('Wrong selector family refusal preserves the weighted baseline', 'baselineRevision'),
  rejectSettings('Reject a selector class assigned to the packer class property', {
    InstanceDataPackerType: weightedClass,
  }),
  ...verifyWeightedState('Wrong packer family refusal preserves the weighted baseline', 'baselineRevision'),
  rejectSettings('Reject direct writes to the read-only selector instance pointer', {
    MeshSelectorParameters: byAttributeClass,
  }),
  ...verifyWeightedState('Direct selector pointer refusal preserves the weighted baseline', 'baselineRevision'),
  rejectSettings('Reject direct writes to the read-only packer instance pointer', {
    InstanceDataPackerParameters: { AttributeSelectors: [selectorText] },
  }),
  ...verifyWeightedState('Direct packer pointer refusal preserves the weighted baseline', 'baselineRevision'),
  rejectSettings('Reject an unknown nested property after requesting both type changes',
    invalidByAttributeSettings({ MCP_UnknownSpawnerField: 1 })),
  ...verifyWeightedState('Mixed invalid property refusal rolls back both type changes and the prior entry', 'baselineRevision'),
  rejectSettings('Reject a malformed selector array element after a valid selector prefix', {
    ...invalidByAttributeSettings(),
    'InstanceDataPackerParameters.AttributeSelectors': [selectorText, '$Density'],
  }),
  ...verifyWeightedState('Malformed selector element refusal preserves the complete weighted baseline', 'baselineRevision'),

  editNode('Switch types and configure dependencies in one order-independent structured settings call', {
    'MeshSelectorParameters.AttributeName': meshAttribute,
    'MeshSelectorParameters.TemplateDescriptor.ComponentClass': ismClass,
    'InstanceDataPackerParameters.AttributeSelectors': [selectorText],
    InstanceDataPackerType: packerClass,
    MeshSelectorType: byAttributeClass,
  }, { assertions: [{ path: 'structuredContent.result.committed', equals: true }] }),
  { scenario: 'Discover the newly initialized By Attribute selector instance', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'node_schema', graphPath, nodeId: captured('spawnerNode'),
      scopePath: 'MeshSelectorParameters', offset: 0, limit: 32 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.scopeType', equals: byAttributeClass },
      { path: 'structuredContent.result.scopeKind', equals: 'owned_object' },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'AttributeName', type: 'Name' } },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'TemplateDescriptor' } },
    ] },
  { scenario: 'Discover the newly initialized By Attribute instance packer selector array', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'node_schema', graphPath, nodeId: captured('spawnerNode'),
      scopePath: 'InstanceDataPackerParameters', offset: 0, limit: 32 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.scopeType', equals: packerClass },
      { path: 'structuredContent.result.schema.items', includesObject: { name: 'AttributeSelectors', type: 'Array',
        typeDetails: { container: 'array', element: { structPath: '/Script/PCG.PCGAttributePropertyInputSelector' } } } },
    ] },
  { scenario: 'Capture the initialized By Attribute selector and packer object paths after staged configuration', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'graph', graphPath, nodeId: captured('spawnerNode'), includeSettings: true },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.items.0.settings.MeshSelectorType', equals: byAttributeClass },
      { path: 'structuredContent.result.items.0.settings.InstanceDataPackerType', equals: packerClass },
    ], captureResult: [
      { key: 'byAttributeSelectorPath', fromField: 'result.items.0.settings.MeshSelectorParameters' },
      { key: 'byAttributePackerPath', fromField: 'result.items.0.settings.InstanceDataPackerParameters' },
    ] },
  { scenario: 'Read AttributeName and TemplateDescriptor from the actual By Attribute selector object', toolName: 'inspect',
    arguments: { action: 'get_property', objectPath: captured('byAttributeSelectorPath'),
      propertyNames: ['AttributeName', 'TemplateDescriptor'] }, expected: 'success', assertions: [
      { path: 'structuredContent.result.properties', length: 2 },
      { path: 'structuredContent.result.properties.0.propertyName', equals: 'AttributeName' },
      { path: 'structuredContent.result.properties.0.value', equals: meshAttribute },
      { path: 'structuredContent.result.properties.1.propertyName', equals: 'TemplateDescriptor' },
      { path: 'structuredContent.result.properties.1.value', includes: ismClass },
    ] },
  { scenario: 'Read the complete native ImportTextItem selector from the actual packer instance', toolName: 'inspect',
    arguments: { action: 'get_property', objectPath: captured('byAttributePackerPath'), propertyName: 'AttributeSelectors' },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.value', length: 1 },
      { path: 'structuredContent.result.value.0', includes: selectorText },
    ] },

  { scenario: 'Setup: spawn one unique bounds-valid owner for this graph', toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Script/Engine.StaticMeshActor', meshPath: cubePath, actorName,
      location: { x: 0, y: 0, z: 0 }, scale: { x: 20, y: 20, z: 2 } }, expected: 'success' },
  { scenario: 'Execute the complete two-branch SoftObjectPath and packing graph', toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath, actorName, componentName: 'SpawnerTypesComponent',
      createComponent: true, save: false }, expected: 'success',
    captureResult: [
      { key: 'component', fromField: 'result.componentPath' },
      { key: 'operation', fromField: 'result.operationId' },
    ] },
  waitForGeneration('Wait for the initial By Attribute and packing operation'),
  { scenario: 'Read exact managed mesh paths, transforms, and total count from the first native generation', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'component_results', componentPath, limit: 8, sampleLimit: 4 }, expected: 'success',
    assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.instanceCount', equals: 2 },
      { path: 'structuredContent.result.outputPointCount', equals: 2 },
      { path: 'structuredContent.result.components', length: 2 },
      { path: 'structuredContent.result.components', includesObject: { meshPath: cubePath, instanceCount: 1,
        kind: 'ISM', sampleTransforms: { length: 1 } } },
      { path: 'structuredContent.result.components', includesObject: { meshPath: spherePath, instanceCount: 1,
        kind: 'ISM', sampleTransforms: { length: 1 } } },
      { path: 'structuredContent.result.components', includesObject: { meshPath: cubePath,
        sampleTransforms: { 0: { location: { x: 10, y: 20, z: 30 } } } } },
      { path: 'structuredContent.result.components', includesObject: { meshPath: spherePath,
        sampleTransforms: { 0: { location: { x: 40, y: 50, z: 60 } } } } },
    ] },
  { scenario: 'List the two actual generated point-data roots for the branch-value readback', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'data', componentPath, offset: 0, limit: 4 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.items', length: 2 },
      { path: 'structuredContent.result.total', equals: 2 },
      { path: 'structuredContent.result.items.0.available', equals: true },
      { path: 'structuredContent.result.items.1.available', equals: true },
    ], captureResult: [
      { key: 'dataset0', fromField: 'result.items.0.datasetId' },
      { key: 'dataset1', fromField: 'result.items.1.datasetId' },
    ] },
  ...readGeneratedData('Initial By Attribute generation', 0, cubePath, 0.25, -17, 10, 20, 30),
  ...readGeneratedData('Initial By Attribute generation', 1, spherePath, 0.75, 29, 40, 50, 60),
  { scenario: 'Regenerate identical selector and packer inputs without accumulating managed resources', toolName: 'manage_pcg',
    arguments: { action: 'generate_pcg_graph', componentPath, force: true, timeoutSeconds: 30, save: false },
    expected: 'success', timeoutMs: 45000, assertions: [
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.nativeGenerated', equals: true },
      { path: 'structuredContent.result.resources.instanceCount', equals: 2 },
    ], captureResult: [{ key: 'operation', fromField: 'result.operationId' }] },
  waitForGeneration('Wait for the repeated native generation before resolving current ISM paths'),
  { scenario: 'Resolve fresh managed ISM component paths after regeneration and confirm no duplicate instances', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'component_results', componentPath, limit: 8, sampleLimit: 4 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.instanceCount', equals: 2 },
      { path: 'structuredContent.result.components', length: 2 },
      { path: 'structuredContent.result.components', includesObject: { meshPath: cubePath, instanceCount: 1 } },
      { path: 'structuredContent.result.components', includesObject: { meshPath: spherePath, instanceCount: 1 } },
      { path: 'structuredContent.result.components', includesObject: { meshPath: cubePath,
        sampleTransforms: { 0: { location: { x: 10, y: 20, z: 30 } } } } },
      { path: 'structuredContent.result.components', includesObject: { meshPath: spherePath,
        sampleTransforms: { 0: { location: { x: 40, y: 50, z: 60 } } } } },
    ], captureResult: [
      { key: 'cubeComponentAfterRepeat', fromField: 'result.components', where: { path: 'meshPath', equals: cubePath }, selectField: 'componentPath' },
      { key: 'sphereComponentAfterRepeat', fromField: 'result.components', where: { path: 'meshPath', equals: spherePath }, selectField: 'componentPath' },
    ] },
  readCustomData('Read packing on the freshly resolved Cube ISM after regeneration', 'cubeComponentAfterRepeat', 1, [0.25]),
  readCustomData('Read packing on the freshly resolved Sphere ISM after regeneration', 'sphereComponentAfterRepeat', 1, [0.75]),

  editNode('Switch back to weighted meshes and disable the current packer in the same staged settings call', {
    'MeshSelectorParameters.MeshEntries': [
      { Descriptor: { StaticMesh: cubePath, ComponentClass: ismClass }, Weight: 1 },
      { Descriptor: { StaticMesh: spherePath, ComponentClass: ismClass }, Weight: 0 },
    ],
    InstanceDataPackerType: null,
    MeshSelectorType: weightedClass,
  }, { assertions: [{ path: 'structuredContent.result.committed', equals: true }] }),
  { scenario: 'Verify weighted selector reinitialization and the null packer type before generation', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'graph', graphPath, nodeId: captured('spawnerNode'), includeSettings: true },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.items.0.settings.MeshSelectorType', equals: weightedClass },
      { path: 'structuredContent.result.items.0.settings.InstanceDataPackerType', equals: null },
    ], captureResult: [
      { key: 'weightedSelectorPathAfterSwitch', fromField: 'result.items.0.settings.MeshSelectorParameters' },
      { key: 'weightedRevision', fromField: 'result.revision' },
    ] },
  { scenario: 'Read the newly weighted selector MeshEntries values through its actual owned object path', toolName: 'inspect',
    arguments: { action: 'get_property', objectPath: captured('weightedSelectorPathAfterSwitch'), propertyName: 'MeshEntries' },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.value', length: 2 },
      { path: 'structuredContent.result.value.0', includes: 'Cube.Cube' },
      { path: 'structuredContent.result.value.0', includes: 'Weight=1' },
      { path: 'structuredContent.result.value.1', includes: 'Sphere.Sphere' },
      { path: 'structuredContent.result.value.1', includes: 'Weight=0' },
    ] },
  { scenario: 'Regenerate through the weighted selector with a deterministic alternate mesh and no packer', toolName: 'manage_pcg',
    arguments: { action: 'generate_pcg_graph', componentPath, force: true, timeoutSeconds: 30, save: false },
    expected: 'success', timeoutMs: 45000, assertions: [
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.nativeGenerated', equals: true },
      { path: 'structuredContent.result.resources.instanceCount', equals: 2 },
    ], captureResult: [{ key: 'operation', fromField: 'result.operationId' }] },
  waitForGeneration('Wait for deterministic weighted Cube generation'),
  { scenario: 'Read the weighted Cube path and reacquire its current ISM component path', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'component_results', componentPath, limit: 8, sampleLimit: 4 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.instanceCount', equals: 2 },
      { path: 'structuredContent.result.components', length: 1 },
      { path: 'structuredContent.result.components.0.meshPath', equals: cubePath },
      { path: 'structuredContent.result.components.0.instanceCount', equals: 2 },
      { path: 'structuredContent.result.components.0.sampleTransforms', length: 2 },
      { path: 'structuredContent.result.components.0.sampleTransforms', includesObject: { location: { x: 10, y: 20, z: 30 } } },
      { path: 'structuredContent.result.components.0.sampleTransforms', includesObject: { location: { x: 40, y: 50, z: 60 } } },
    ], captureResult: [{ key: 'weightedCubeComponent', fromField: 'result.components.0.componentPath' }] },
  readCustomData('Confirm the disabled packer leaves the weighted Cube ISM without custom data', 'weightedCubeComponent', 0, []),

  editNode('Return to By Attribute before exercising the legacy meshPath selector replacement', {
    'MeshSelectorParameters.AttributeName': meshAttribute,
    'MeshSelectorParameters.TemplateDescriptor.ComponentClass': ismClass,
    InstanceDataPackerType: null,
    MeshSelectorType: byAttributeClass,
  }, { assertions: [{ path: 'structuredContent.result.committed', equals: true }] }),
  pcg('Legacy meshPath convenience alone switches By Attribute back to a one-entry weighted selector', 'edit_pcg_graph', {
    edit: 'set_node_settings', graphPath, nodeId: captured('spawnerNode'), meshPath: spherePath, save: false,
  }, { assertions: [{ path: 'structuredContent.result.committed', equals: true }] }),
  { scenario: 'Verify the convenience meshPath switched the actual selector instance back to Weighted', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'node_schema', graphPath, nodeId: captured('spawnerNode'),
      scopePath: 'MeshSelectorParameters', offset: 0, limit: 32 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.scopeType', equals: weightedClass },
      { path: 'structuredContent.result.scopeKind', equals: 'owned_object' },
    ] },
  { scenario: 'Capture the single-entry convenience selector’s actual object path', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'graph', graphPath, nodeId: captured('spawnerNode'), includeSettings: true },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.items.0.settings.MeshSelectorType', equals: weightedClass },
    ], captureResult: [{ key: 'convenienceSelectorPath', fromField: 'result.items.0.settings.MeshSelectorParameters' }] },
  { scenario: 'Verify legacy meshPath preserved the one-entry Sphere convenience value', toolName: 'inspect',
    arguments: { action: 'get_property', objectPath: captured('convenienceSelectorPath'), propertyName: 'MeshEntries' },
    expected: 'success', assertions: [
      { path: 'structuredContent.result.value', length: 1 },
      { path: 'structuredContent.result.value.0', includes: 'Sphere.Sphere' },
      { path: 'structuredContent.result.value.0', includes: 'Weight=1' },
    ] },
  { scenario: 'Run the existing single-entry meshPath convenience through real native generation', toolName: 'manage_pcg',
    arguments: { action: 'generate_pcg_graph', componentPath, force: true, timeoutSeconds: 30, save: false },
    expected: 'success', timeoutMs: 45000, assertions: [
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.nativeGenerated', equals: true },
      { path: 'structuredContent.result.resources.instanceCount', equals: 2 },
    ], captureResult: [{ key: 'operation', fromField: 'result.operationId' }] },
  waitForGeneration('Wait for the convenience Sphere generation'),
  { scenario: 'Final managed readback proves meshPath convenience chose Sphere without resource accumulation', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'component_results', componentPath, limit: 8, sampleLimit: 4 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.instanceCount', equals: 2 },
      { path: 'structuredContent.result.components', length: 1 },
      { path: 'structuredContent.result.components.0.meshPath', equals: spherePath },
      { path: 'structuredContent.result.components.0.instanceCount', equals: 2 },
      { path: 'structuredContent.result.components.0.sampleTransforms', length: 2 },
      { path: 'structuredContent.result.components.0.sampleTransforms', includesObject: { location: { x: 10, y: 20, z: 30 } } },
      { path: 'structuredContent.result.components.0.sampleTransforms', includesObject: { location: { x: 40, y: 50, z: 60 } } },
    ] },
  { scenario: 'Require native idle before owned component cleanup', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'component_results', componentPath, sampleLimit: 0 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.cleaning', equals: false },
    ] },
  pcg('Cleanup with explicit consent: remove only this graph component’s managed resources', 'control_pcg_execution',
    { control: 'cleanup', componentPath, timeoutSeconds: 20, save: false }, { timeoutMs: 30000,
      consent: { capability: 'manage_pcg.control_pcg_execution', acknowledge: 'explicit' },
      assertions: [{ path: 'structuredContent.result.state', equals: 'succeeded' }] }),
  { scenario: 'Verify native cleanup leaves zero managed instances before deleting the owner', toolName: 'manage_pcg',
    arguments: { action: 'inspect_pcg', view: 'component_results', componentPath, sampleLimit: 0 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.cleaning', equals: false },
      { path: 'structuredContent.result.instanceCount', equals: 0 },
    ] },
  { scenario: 'Cleanup: delete only this suite’s UUID-named actor with explicit consent', toolName: 'control_actor',
    arguments: { action: 'delete', actorNames: [actorName] }, expected: 'success',
    consent: { capability: 'control_actor.delete', acknowledge: 'explicit' } },
  { scenario: 'Cleanup: delete only the UUID-owned graph created by this suite', toolName: 'manage_asset',
    arguments: { action: 'delete', path: captured('ownedGraph') }, expected: 'success',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' } },
];

runToolTests('manage_pcg_spawner_types', cases.map(testCase => ({ ...testCase, abortOnFailure: true })), {
  isolatedProject: { projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME, projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR, engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION },
});
