import { readFileSync, realpathSync } from 'node:fs';
import path from 'node:path';
import { runToolTests } from '../../test-runner.mjs';

const VALIDATION_ASSET_ROOT = '/Game/MCP_PCG_Validation/';
const isStaticAudit = Array.isArray(globalThis.__capturedToolSuites);
const staticAuditManifest = {
  root: '/Game/MCP_PCG_Validation/StaticAudit',
  mapPath: '/Game/MCP_PCG_Validation/StaticAudit/Map',
  graphPath: '/Game/MCP_PCG_Validation/StaticAudit/Graph',
  subgraphPath: '/Game/MCP_PCG_Validation/StaticAudit/Transform',
  blueprintPath: '/Game/MCP_PCG_Validation/StaticAudit/BP_TaggedElement',
  actorName: 'StaticAuditActor',
  componentPath: '/Game/MCP_PCG_Validation/StaticAudit/Map.Map:PersistentLevel.StaticAuditActor.PCGComponent',
  nodeIds: ['AuditSubgraph', 'AuditBlueprint', 'AuditSpline', 'AuditSampler', 'AuditFilter', 'AuditCube', 'AuditSphere', 'AuditCylinder', 'AuditActors'],
  subgraphNodeIds: ['AuditTransform'],
};

function requireText(value, field) {
  if (typeof value !== 'string' || value.trim() !== value || value.length === 0 || /[\u0000-\u001f\u007f]/.test(value)) {
    throw new Error(`PCG cold-load manifest has an invalid ${field}.`);
  }
  return value;
}

function requireAssetPath(value, field) {
  const assetPath = requireText(value, field);
  if (!assetPath.startsWith(VALIDATION_ASSET_ROOT) || assetPath.includes('\\') || assetPath.includes('//') ||
      assetPath.split('/').some(segment => segment === '.' || segment === '..') || assetPath.endsWith('/')) {
    throw new Error(`PCG cold-load manifest ${field} must stay under ${VALIDATION_ASSET_ROOT}.`);
  }
  return assetPath;
}

function validateManifest(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error('PCG cold-load manifest must contain a JSON object.');
  }
  const manifest = {
    root: requireAssetPath(value.root, 'root'),
    mapPath: requireAssetPath(value.mapPath, 'mapPath'),
    graphPath: requireAssetPath(value.graphPath, 'graphPath'),
    subgraphPath: requireAssetPath(value.subgraphPath, 'subgraphPath'),
    blueprintPath: requireAssetPath(value.blueprintPath, 'blueprintPath'),
    actorName: requireText(value.actorName, 'actorName'),
    componentPath: requireAssetPath(value.componentPath, 'componentPath'),
    nodeIds: value.nodeIds,
    subgraphNodeIds: value.subgraphNodeIds,
    ...(value.getterNodeId === undefined ? {} : { getterNodeId: requireText(value.getterNodeId, 'getterNodeId') }),
    ...(value.getterGuid === undefined ? {} : { getterGuid: requireText(value.getterGuid, 'getterGuid') }),
    ...(value.parameterDefaults === undefined ? {} : { parameterDefaults: value.parameterDefaults }),
    ...(value.parameterIds === undefined ? {} : { parameterIds: value.parameterIds }),
    ...(value.componentOverride === undefined ? {} : { componentOverride: value.componentOverride }),
    ...(value.sampleY === undefined ? {} : { sampleY: value.sampleY }),
    ...(value.exactIntegerDefault === undefined ? {} : { exactIntegerDefault: value.exactIntegerDefault,
      exactIntegerOverride: value.exactIntegerOverride, exactIntegerArray: value.exactIntegerArray }),
  };
  if (manifest.exactIntegerDefault !== undefined &&
      (manifest.exactIntegerDefault !== '9223372036854775807' || manifest.exactIntegerOverride !== '-9223372036854775808' ||
       JSON.stringify(manifest.exactIntegerArray) !== JSON.stringify(['9007199254740993', '-9223372036854775808', 7]))) {
    throw new Error('PCG cold-load manifest has invalid exact Int64 expectations.');
  }
  for (const field of ['mapPath', 'graphPath', 'subgraphPath', 'blueprintPath', 'componentPath']) {
    if (!manifest[field].startsWith(`${manifest.root}/`)) throw new Error('All persistence paths must belong to the same owned RunRoot.');
  }
  const basename = path.posix.basename(manifest.mapPath);
  const mapObjectPath = `${manifest.mapPath}.${basename}:PersistentLevel.`;
  if (!manifest.componentPath.startsWith(mapObjectPath) || !manifest.componentPath.includes(`.${manifest.actorName}.`)) {
    throw new Error('PCG cold-load componentPath does not identify actorName in the selected map.');
  }
  for (const [field, ids] of [['nodeIds', manifest.nodeIds], ['subgraphNodeIds', manifest.subgraphNodeIds]]) {
    if (!Array.isArray(ids) || ids.some(id => typeof id !== 'string' || !id.trim() || id === 'input' || id === 'output') ||
        new Set(ids).size !== ids.length) {
      throw new Error(`PCG cold-load manifest ${field} must contain unique ordinary graph node IDs.`);
    }
  }
  const expectedParentNodeCount = 9 + (manifest.getterNodeId ? 1 : 0);
  if (manifest.nodeIds.length !== expectedParentNodeCount || manifest.subgraphNodeIds.length !== 1) {
    throw new Error('PCG cold-load manifest does not describe the complete A09 parent graph and transform subgraph.');
  }
  if (manifest.getterNodeId && !manifest.nodeIds.includes(manifest.getterNodeId)) {
    throw new Error('PCG cold-load getterNodeId must be present in nodeIds.');
  }
  if (manifest.getterGuid && !manifest.getterNodeId) {
    throw new Error('PCG cold-load getterGuid requires getterNodeId.');
  }
  if (manifest.parameterDefaults !== undefined) {
    const defaults = manifest.parameterDefaults;
    if (!defaults || typeof defaults !== 'object' || Array.isArray(defaults) ||
        Object.entries(defaults).some(([name, number]) => !name || typeof number !== 'number' || !Number.isFinite(number))) {
      throw new Error('PCG cold-load parameterDefaults must map names to finite numbers.');
    }
  }
  if (manifest.parameterIds !== undefined && (!manifest.parameterIds || typeof manifest.parameterIds !== 'object' ||
      Array.isArray(manifest.parameterIds) || Object.values(manifest.parameterIds).some(id => typeof id !== 'string' || !/^[A-Fa-f0-9-]{36}$/.test(id)))) {
    throw new Error('PCG cold-load parameterIds must contain native GUID strings.');
  }
  if (manifest.componentOverride !== undefined &&
      (typeof manifest.componentOverride !== 'number' || !Number.isFinite(manifest.componentOverride))) {
    throw new Error('PCG cold-load componentOverride must be a finite number.');
  }
  if (manifest.sampleY !== undefined && (!Array.isArray(manifest.sampleY) || manifest.sampleY.length !== 3 ||
      manifest.sampleY.some(row => !Array.isArray(row) || row.length !== 4 || row.some(number => typeof number !== 'number' || !Number.isFinite(number))))) {
    throw new Error('PCG cold-load sampleY must contain all twelve finite instance coordinates.');
  }
  return manifest;
}

function readPersistenceManifest() {
  const projectName = process.env.UNREAL_MCP_TEST_PROJECT_NAME;
  const projectDir = process.env.UE_PROJECT_PATH;
  if (!projectName || !projectName.trim() || projectName.trim() !== projectName || !projectDir || !projectDir.trim()) {
    throw new Error('PCG cold-load requires explicit UNREAL_MCP_TEST_PROJECT_NAME and UE_PROJECT_PATH identity.');
  }
  const paths = /^[A-Za-z]:[\\/]/.test(projectDir) ? path.win32 : path.posix;
  if (!paths.isAbsolute(projectDir)) throw new Error('UE_PROJECT_PATH must be absolute for PCG cold-load validation.');
  const projectRoot = /\.uproject$/i.test(projectDir) ? paths.dirname(projectDir) : projectDir;
  const savedRoot = paths.resolve(projectRoot, 'Saved', 'PCGValidation');
  const manifestPath = process.env.UNREAL_MCP_PCG_PERSISTENCE_MANIFEST;
  if (!manifestPath || !paths.isAbsolute(manifestPath) || paths.extname(manifestPath).toLowerCase() !== '.json') {
    throw new Error('Set UNREAL_MCP_PCG_PERSISTENCE_MANIFEST to a JSON file under the selected project Saved/PCGValidation directory.');
  }
  const resolvedSavedRoot = realpathSync(savedRoot);
  const resolvedManifestPath = realpathSync(manifestPath);
  const relativeManifestPath = paths.relative(resolvedSavedRoot, resolvedManifestPath);
  if (!relativeManifestPath || relativeManifestPath === '..' || relativeManifestPath.startsWith(`..${paths.sep}`) || paths.isAbsolute(relativeManifestPath)) {
    throw new Error('PCG cold-load manifest resolves outside the selected project Saved/PCGValidation directory.');
  }
  let decoded;
  try {
    decoded = JSON.parse(readFileSync(resolvedManifestPath, 'utf8'));
  } catch {
    throw new Error('PCG cold-load persistence manifest is missing or is not valid JSON.');
  }
  return validateManifest(decoded);
}

const manifest = validateManifest(isStaticAudit ? staticAuditManifest : readPersistenceManifest());
const isolatedProject = {
  projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME,
  projectDir: process.env.UE_PROJECT_PATH,
  editorBaseDir: process.env.UE_EDITOR_BASE_DIR,
  engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION,
};
const ordinaryNodeClasses = [
  'PCGSubgraphSettings',
  'PCGBlueprintSettings',
  'PCGGetSplineSettings',
  'PCGSplineSamplerSettings',
  'PCGAttributeFilteringRangeSettings',
  'PCGStaticMeshSpawnerSettings',
  'PCGStaticMeshSpawnerSettings',
  'PCGStaticMeshSpawnerSettings',
  'PCGSpawnActorSettings',
];
const subgraphAssertions = [
  { path: 'structuredContent.result.total', equals: manifest.subgraphNodeIds.length + 2 },
  { path: 'structuredContent.result.items', length: manifest.subgraphNodeIds.length + 2 },
  { path: 'structuredContent.result.items.0.input', equals: true },
  { path: 'structuredContent.result.items.1.nodeId', equals: manifest.subgraphNodeIds[0] },
  { path: 'structuredContent.result.items.1.settingsClass', includes: 'PCGTransformPointsSettings' },
  { path: 'structuredContent.result.items.1.settings.Seed', equals: 12345 },
  { path: 'structuredContent.result.items.1.settings.OffsetMin.1', approximately: 10, tolerance: 0.001 },
  { path: 'structuredContent.result.items.1.settings.OffsetMax.1', approximately: 30, tolerance: 0.001 },
  { path: 'structuredContent.result.items.2.output', equals: true },
];
const graphReadCases = [
  { scenario: 'Read saved parent totals and the input node without a full settings dump', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath: manifest.graphPath, includeSettings: false, limit: 1, edgeLimit: 1 },
    expected: 'success', abortOnFailure: true, assertions: [
      { path: 'structuredContent.result.total', equals: manifest.nodeIds.length + 2 },
      { path: 'structuredContent.result.items.0.input', equals: true },
      { path: 'structuredContent.result.edgeTotal', gte: 12 },
    ] },
  ...manifest.nodeIds.map((nodeId, index) => ({
    scenario: `Read persisted authored settings for parent node ${index + 1}`, toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath: manifest.graphPath, offset: index + 1, includeSettings: true, limit: 1, edgeLimit: 1 },
    expected: 'success', abortOnFailure: true, assertions: [
      { path: 'structuredContent.result.total', equals: manifest.nodeIds.length + 2 },
      { path: 'structuredContent.result.items.0.nodeId', equals: nodeId },
      ...(ordinaryNodeClasses[index] ? [{ path: 'structuredContent.result.items.0.settingsClass', includes: ordinaryNodeClasses[index] }] : []),
      ...(index === 1 ? [{ path: 'structuredContent.result.items.0.settings.BlueprintElementType', equals: `${manifest.blueprintPath}.BP_TaggedElement_C` }] : []),
      ...(index === 3 ? [{ path: 'structuredContent.result.items.0.settings.SamplerParams', includes: 'MCPDistance' },
        { path: 'structuredContent.result.items.0.settings.SamplerParams', includes: 'DistanceIncrement=100' }] : []),
      ...(index === 4 ? [{ path: 'structuredContent.result.items.0.settings.TargetAttribute', equals: 'PCGBegin(MCPDistance)PCGEnd' },
        { path: 'structuredContent.result.items.0.settings.MinThreshold', includes: 'DoubleValue=200' },
        { path: 'structuredContent.result.items.0.settings.MaxThreshold', includes: 'DoubleValue=500' }] : []),
      ...(nodeId === manifest.getterNodeId ? [{ path: 'structuredContent.result.items.0.settingsClass', includes: 'PCGUserParameterGetSettings' },
        { path: 'structuredContent.result.items.0.bindings.PropertyName', equals: 'Spacing' },
        ...(manifest.getterGuid ? [{ path: 'structuredContent.result.items.0.bindings.PropertyGuid', equals: manifest.getterGuid }] : [])] : []),
    ],
  })),
  { scenario: 'Read the saved parent output node at its stable source-order offset', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath: manifest.graphPath, includeSettings: false, offset: manifest.nodeIds.length + 1, limit: 1, edgeLimit: 1 },
    expected: 'success', abortOnFailure: true, assertions: [{ path: 'structuredContent.result.items.0.output', equals: true }] },
];

const meshPaths = ['/Engine/BasicShapes/Sphere.Sphere', '/Engine/BasicShapes/Cube.Cube', '/Engine/BasicShapes/Cylinder.Cylinder'];
const transformAssertions = meshPaths.flatMap((meshPath, row) => [
  { path: `structuredContent.result.components.${row}.meshPath`, equals: meshPath },
  { path: `structuredContent.result.components.${row}.sampleTransforms`, length: 4 },
  ...[200, 300, 400, 500].flatMap((x, index) => Object.entries({
    'location.x': [x, 0.0001], 'location.y': [manifest.sampleY?.[row]?.[index] ?? 20, manifest.sampleY ? 0.0001 : 10.0001],
    'location.z': [20, 0.0001], 'rotation.pitch': [0, 0.0001], 'rotation.yaw': [45, 0.0001], 'rotation.roll': [0, 0.0001],
    'scale.x': [1, 0.00001], 'scale.y': [2, 0.00001], 'scale.z': [3, 0.00001],
  }).map(([field, [approximately, tolerance]]) => ({
    path: `structuredContent.result.components.${row}.sampleTransforms.${index}.${field}`, approximately, tolerance,
  }))),
]);
const inventoryAssertions = [
  { path: 'structuredContent.result.generating', equals: false },
  { path: 'structuredContent.result.cleaning', equals: false },
  { path: 'structuredContent.result.instanceCount', equals: 12 },
  { path: 'structuredContent.result.ismComponentCount', equals: 2 },
  { path: 'structuredContent.result.hismComponentCount', equals: 1 },
  { path: 'structuredContent.result.managedRootActorCount', equals: 4 },
  { path: 'structuredContent.result.outputDataCount', equals: 4 },
  { path: 'structuredContent.result.outputPointCount', equals: 16 },
  { path: 'structuredContent.result.outputs', length: 4 },
  ...[0, 1, 2, 3].flatMap(index => [{ path: `structuredContent.result.outputs.${index}.pointCount`, equals: 4 },
    { path: `structuredContent.result.outputs.${index}.tags.0`, equals: 'MCP_CustomNode' }]),
  { path: 'structuredContent.result.components', minLength: 3 },
  { path: 'structuredContent.result.actors', minLength: 4 },
  { path: 'structuredContent.result.outputs', minLength: 2 },
  { path: 'structuredContent.result.outputs.0.tags.0', equals: 'MCP_CustomNode' },
  { path: 'structuredContent.result.outputs.1.tags.0', equals: 'MCP_CustomNode' },
  ...transformAssertions,
];
const captured = key => '${captured:' + key + '}';
const actorCaptures = [0, 1, 2, 3].map(index => ({ key: `coldActor${index}`, fromField: `result.actors.${index}.actorPath` }));
const verifyActors = scenario => ({ scenario, toolName: 'system_control',
  arguments: { action: 'execute_python', code: `import unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
component = unreal.find_object(None, ${JSON.stringify(manifest.componentPath)})
actors = [unreal.find_object(None, actor_path) for actor_path in [${[0, 1, 2, 3].map(index => `'${captured(`coldActor${index}`)}'`).join(', ')}]]
assert all(actor is not None and actor.get_world() == component.get_world() for actor in actors)
actors.sort(key=lambda actor: actor.get_actor_location().x)
expected_y = ${JSON.stringify(manifest.sampleY?.[0] ?? [20, 20, 20, 20])}
for index, actor in enumerate(actors):
    position, rotation, scale = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
    assert abs(position.x - (200 + index * 100)) < 0.0001 and abs(position.z - 20) < 0.0001
    assert abs(position.y - expected_y[index]) < ${manifest.sampleY ? '0.0001' : '10.0001'}
    assert abs(rotation.pitch) < 0.0001 and abs(rotation.yaw - 45) < 0.0001 and abs(rotation.roll) < 0.0001
    assert abs(scale.x - 1) < 0.00001 and abs(scale.y - 2) < 0.00001 and abs(scale.z - 3) < 0.00001
` }, expected: 'success', abortOnFailure: true });
const cases = [
  { scenario: 'Load the persisted fixture map through the safe level lifecycle action', toolName: 'manage_level',
    arguments: { action: 'load', levelPath: manifest.mapPath }, expected: 'success', timeoutMs: 30000, abortOnFailure: true },
  { scenario: 'Wait for map-load PCG generation to reach native idle with the saved A09 products', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath: manifest.componentPath, limit: 100, sampleLimit: 16 },
    expected: 'success', timeoutMs: 30000, pollUntil: { timeoutMs: 30000, intervalMs: 1000 }, abortOnFailure: true,
    assertions: inventoryAssertions, captureResult: actorCaptures },
  verifyActors('Verify all persisted managed actor transforms in coordinate order'),
  ...graphReadCases,
  { scenario: 'Verify the persisted subgraph reference and all three native mesh selector descriptors', toolName: 'system_control',
    arguments: { action: 'execute_python', code: `import unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
graph = unreal.load_asset(${JSON.stringify(manifest.graphPath)})
nodes = {node.get_name(): node for node in graph.get_editor_property('Nodes')}
settings = nodes[${JSON.stringify(manifest.nodeIds[0])}].get_settings()
instance = settings.get_editor_property('SubgraphInstance')
assert instance.get_editor_property('Graph').get_path_name().split('.')[0] == ${JSON.stringify(manifest.subgraphPath)}
for node_name, mesh_path, component_class in ${JSON.stringify([
  [manifest.nodeIds[5], '/Engine/BasicShapes/Cube.Cube', '/Script/Engine.InstancedStaticMeshComponent'],
  [manifest.nodeIds[6], '/Engine/BasicShapes/Sphere.Sphere', '/Script/Engine.HierarchicalInstancedStaticMeshComponent'],
  [manifest.nodeIds[7], '/Engine/BasicShapes/Cylinder.Cylinder', '/Script/Engine.InstancedStaticMeshComponent'],
])}:
    selector = nodes[node_name].get_settings().get_editor_property('MeshSelectorParameters')
    entries = selector.get_editor_property('MeshEntries')
    assert len(entries) == 1
    descriptor = entries[0].get_editor_property('Descriptor')
    assert descriptor.get_editor_property('StaticMesh').get_path_name() == mesh_path
    assert descriptor.get_editor_property('ComponentClass').get_path_name() == component_class
` }, expected: 'success', abortOnFailure: true },
  { scenario: 'Read the saved seeded transform child graph in source order', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath: manifest.subgraphPath, includeSettings: true, offset: 0, limit: 100,
      edgeOffset: 0, edgeLimit: 1000 }, expected: 'success', timeoutMs: 30000, abortOnFailure: true,
    assertions: subgraphAssertions },
  ...(manifest.getterGuid || manifest.parameterDefaults ? Object.entries(manifest.parameterDefaults ?? { Spacing: 0 }).map(([name, defaultValue]) => ({
    scenario: `Read persisted graph parameter default ${name}`,
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_parameters', target: 'graph', graphPath: manifest.graphPath, name, offset: 0, limit: 1 },
    expected: 'success', abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.parameters.0.name', equals: name },
      ...(manifest.parameterDefaults?.[name] === undefined ? [] : [{ path: 'structuredContent.result.parameters.0.value', equals: defaultValue }]),
      ...(manifest.parameterIds?.[name] ? [{ path: 'structuredContent.result.parameters.0.id', equals: manifest.parameterIds[name] }] : []),
    ],
  })) : []),
  ...(manifest.componentOverride === undefined ? [] : [{
    scenario: 'Read the persisted Spacing component override independently of its graph default',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_parameters', target: 'component', componentPath: manifest.componentPath, name: 'Spacing', offset: 0, limit: 1 },
    expected: 'success', abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.parameters.0.value', equals: manifest.componentOverride },
      { path: 'structuredContent.result.parameters.0.overridden', equals: true },
    ],
  }]),
  ...(manifest.exactIntegerDefault === undefined ? [] : [
    { scenario: 'Read the exact persisted full-range Int64 graph default', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_parameters', graphPath: manifest.graphPath, name: 'ExactInteger', limit: 1 },
      expected: 'success', abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.parameters.0.value', equals: manifest.exactIntegerDefault },
        { path: 'structuredContent.result.parameters.0.defaultValue', equals: manifest.exactIntegerDefault },
      ] },
    { scenario: 'Read the exact persisted full-range Int64 component override', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_parameters', target: 'component', componentPath: manifest.componentPath, name: 'ExactInteger', limit: 1 },
      expected: 'success', abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.parameters.0.value', equals: manifest.exactIntegerOverride },
        { path: 'structuredContent.result.parameters.0.defaultValue', equals: manifest.exactIntegerDefault },
        { path: 'structuredContent.result.parameters.0.overridden', equals: true },
      ] },
    { scenario: 'Read every exact persisted Int64 array element', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_parameters', graphPath: manifest.graphPath, name: 'ExactIntegerArray', limit: 1 },
      expected: 'success', abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.parameters.0.value', length: manifest.exactIntegerArray.length },
        ...manifest.exactIntegerArray.flatMap((value, index) => [
          { path: 'structuredContent.result.parameters.0.value.' + index, equals: value },
          { path: 'structuredContent.result.parameters.0.defaultValue.' + index, equals: value },
        ]),
      ] },
  ]),
  { scenario: 'Regenerate only the exact persisted component and require native success', toolName: 'manage_pcg',
    arguments: { action: 'generate_pcg_graph', componentPath: manifest.componentPath, timeoutSeconds: 30, save: false },
    expected: 'success', timeoutMs: 30000, abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.completed', equals: true },
      { path: 'structuredContent.result.succeeded', equals: true },
      { path: 'structuredContent.result.engineStillRunning', equals: false },
      { path: 'structuredContent.result.state', equals: 'succeeded' },
    ] },
  { scenario: 'Verify regenerated ISM/HISM, managed actors and custom output tags at native idle', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath: manifest.componentPath, limit: 100, sampleLimit: 16 },
    expected: 'success', timeoutMs: 30000, pollUntil: { timeoutMs: 30000, intervalMs: 1000 }, abortOnFailure: true,
    assertions: inventoryAssertions, captureResult: actorCaptures },
  verifyActors('Verify all regenerated managed actor transforms after cold load'),
  ...(process.env.UNREAL_MCP_PCG_VERIFY_EDITOR === '1' ? [
    { scenario: 'Capture original authored position and Seed before the complex editor lifecycle', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_graph', graphPath: manifest.graphPath, nodeId: manifest.nodeIds[0], includeSettings: true, limit: 1, edgeLimit: 1 },
      expected: 'success', abortOnFailure: true, captureResult: [
        { key: 'uiRevision', fromField: 'result.revision' }, { key: 'uiX', fromField: 'result.items.0.x' },
        { key: 'uiY', fromField: 'result.items.0.y' }, { key: 'uiSeed', fromField: 'result.items.0.settings.Seed' },
      ] },
    { scenario: 'Open only the persisted complex parent graph and verify its model mapping', toolName: 'manage_pcg',
      arguments: { action: 'sync_pcg_editor_graph', graphPath: manifest.graphPath, openEditor: true, save: false },
      expected: 'success', abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.available', equals: true }, { path: 'structuredContent.result.consistent', equals: true },
        { path: 'structuredContent.result.mappedNodes', equals: manifest.nodeIds.length + 2 },
        { path: 'structuredContent.result.revision', equals: captured('uiRevision') },
      ] },
    { scenario: 'Move the original subgraph node while the complex editor is open', toolName: 'manage_pcg',
      arguments: { action: 'move_pcg_node', graphPath: manifest.graphPath, nodeId: manifest.nodeIds[0], x: 480, y: 960, save: false },
      expected: 'success', abortOnFailure: true },
    { scenario: 'Open editor mapping uses the same native node identity and edited position', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_editor_graph', graphPath: manifest.graphPath, nodeId: manifest.nodeIds[0], limit: 1, edgeLimit: 1 },
      expected: 'success', abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.items.0.nodeId', equals: manifest.nodeIds[0] },
        { path: 'structuredContent.result.items.0.x', equals: 480 }, { path: 'structuredContent.result.items.0.y', equals: 960 },
        { path: 'structuredContent.result.editorConsistent', equals: true },
      ] },
    { scenario: 'Edit native settings while the complex graph editor is open', toolName: 'manage_pcg',
      arguments: { action: 'set_pcg_node_settings', graphPath: manifest.graphPath, nodeId: manifest.nodeIds[0], settings: { Seed: 31415 }, save: false },
      expected: 'success', abortOnFailure: true },
    { scenario: 'Close only this owned graph editor through its public subsystem', toolName: 'system_control',
      arguments: { action: 'execute_python', code: `import unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
graph = unreal.load_asset(${JSON.stringify(manifest.graphPath)})
assert unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).close_all_editors_for_asset(graph) >= 1
` }, expected: 'success', abortOnFailure: true },
    { scenario: 'The closed editor retains a consistent backend model mapping', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_editor_graph', graphPath: manifest.graphPath, nodeId: manifest.nodeIds[0], limit: 1, edgeLimit: 1 },
      expected: 'success', abortOnFailure: true, assertions: [{ path: 'structuredContent.result.editorConsistent', equals: true }] },
    { scenario: 'Reopen only this complex graph through the public subsystem', toolName: 'system_control',
      arguments: { action: 'execute_python', code: `import unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
graph = unreal.load_asset(${JSON.stringify(manifest.graphPath)})
assert unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([graph])
` }, expected: 'success', abortOnFailure: true },
    { scenario: 'Reopened editor still maps every original complex graph node', toolName: 'manage_pcg',
      arguments: { action: 'sync_pcg_editor_graph', graphPath: manifest.graphPath, save: false }, expected: 'success', abortOnFailure: true,
      assertions: [{ path: 'structuredContent.result.consistent', equals: true },
        { path: 'structuredContent.result.mappedNodes', equals: manifest.nodeIds.length + 2 }] },
    { scenario: 'Native settings remain editable after reopening, then restore the original Seed', toolName: 'manage_pcg',
      arguments: { action: 'set_pcg_node_settings', graphPath: manifest.graphPath, nodeId: manifest.nodeIds[0], settings: { Seed: captured('uiSeed') }, save: false },
      expected: 'success', abortOnFailure: true },
    { scenario: 'Restore only this test’s authored layout change after reopening', toolName: 'manage_pcg',
      arguments: { action: 'move_pcg_node', graphPath: manifest.graphPath, nodeId: manifest.nodeIds[0], x: captured('uiX'), y: captured('uiY'), save: false },
      expected: 'success', abortOnFailure: true },
    { scenario: 'Closing and reopening plus compensating edits retain the original authored revision', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_graph', graphPath: manifest.graphPath, limit: 1, edgeLimit: 1 }, expected: 'success', abortOnFailure: true,
      assertions: [{ path: 'structuredContent.result.revision', equals: captured('uiRevision') }] },
    { scenario: 'Close only the owned graph editor after all API-backed lifecycle checks', toolName: 'system_control',
      arguments: { action: 'execute_python', code: `import unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
graph = unreal.load_asset(${JSON.stringify(manifest.graphPath)})
assert unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).close_all_editors_for_asset(graph) >= 1
` }, expected: 'success', abortOnFailure: true },
    { scenario: 'Wait for any automatic native generation after restored editor settings', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_component_results', componentPath: manifest.componentPath, sampleLimit: 0 }, expected: 'success',
      pollUntil: { timeoutMs: 30000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.generating', equals: false }, { path: 'structuredContent.result.cleaning', equals: false },
      ] },
  ] : []),
];

runToolTests('manage_pcg_cold_load', cases, { isolatedProject, foldTwins: false });
