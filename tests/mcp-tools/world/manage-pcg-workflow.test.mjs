import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const stamp = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const root = `/Game/MCP_PCG_Validation/Workflow_${stamp}`;
const keepFixture = process.env.UNREAL_MCP_PCG_KEEP_FIXTURE === '1';
const mapPath = `${root}/Map`;
const graphPath = `${root}/Graph`;
const subgraphPath = `${root}/Transform`;
const blueprintPath = `${root}/BP_TaggedElement`;
const actorName = `MCP_PCG_Workflow_${stamp}`;
const captured = key => '${captured:' + key + '}';
const componentPath = captured('component');
const pcg = (scenario, args, extra = {}) => ({ scenario, toolName: 'manage_pcg',
  arguments: { ...args, save: keepFixture }, expected: 'success', abortOnFailure: true, ...extra });
const read = (scenario, path, key) => ({ scenario, toolName: 'manage_pcg',
  arguments: { action: 'get_pcg_graph', graphPath: path }, expected: 'success', abortOnFailure: true,
  captureResult: [{ key, fromField: 'result.revision' }] });
// Inventory paths sort HISM_Sphere before ISM_Cube and ISM_Cylinder in this owned fixture.
const meshPaths = ['/Engine/BasicShapes/Sphere.Sphere', '/Engine/BasicShapes/Cube.Cube', '/Engine/BasicShapes/Cylinder.Cylinder'];
const transformAssertions = meshPaths.flatMap((meshPath, row) => [
  { path: `structuredContent.result.components.${row}.meshPath`, equals: meshPath },
  { path: `structuredContent.result.components.${row}.sampleTransforms`, length: 4 },
  ...[200, 300, 400, 500].flatMap((x, index) => Object.entries({
    'location.x': [x, 0.0001], 'location.y': [20, 10.0001], 'location.z': [20, 0.0001],
    'rotation.pitch': [0, 0.0001], 'rotation.yaw': [45, 0.0001], 'rotation.roll': [0, 0.0001],
    'scale.x': [1, 0.00001], 'scale.y': [2, 0.00001], 'scale.z': [3, 0.00001],
  }).map(([field, [approximately, tolerance]]) => ({
    path: `structuredContent.result.components.${row}.sampleTransforms.${index}.${field}`, approximately, tolerance,
  }))),
]);
const pointBoundarySettings = label => ({ Pins: [{ Label: label, AllowedTypes: {
  Ids: [{ Struct: '/Script/PCG.PCGDataTypeInfoPoint' }], CustomSubtype: -1,
} }] });
const cases = [
  ...(keepFixture ? [{ scenario: 'Create a uniquely owned saved map for independent-process verification', toolName: 'manage_level_structure',
    arguments: { action: 'create_level', levelName: 'Map', levelPath: root, bCreateWorldPartition: false, save: true, loadAfterCreate: true },
    expected: 'success', abortOnFailure: true }] : []),
  pcg('Create the exclusively owned parent graph', { action: 'create_pcg_graph', graphPath },
    { captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }] }),
  pcg('Create a reusable native subgraph and parent node', { action: 'create_pcg_subgraph', subgraphPath, parentGraphPath: graphPath },
    { captureResult: [{ key: 'ownedSubgraph', fromField: 'result.graphPath' }, { key: 'subgraphNode', fromField: 'result.nodeId' }] }),
  { scenario: 'Create the owned custom PCG Blueprint element', toolName: 'manage_blueprint',
    arguments: { action: 'create_blueprint', name: 'BP_TaggedElement', savePath: root,
      parentClass: '/Script/PCGValidation.PCGValidationBlueprintElement' }, expected: 'success', abortOnFailure: true },
  { scenario: 'Read stable native boundary node IDs before configuring their pins', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath: subgraphPath, limit: 2, edgeLimit: 1 }, expected: 'success', abortOnFailure: true,
    assertions: [{ path: 'structuredContent.result.items', length: 2 },
      { path: 'structuredContent.result.items.0.input', equals: true }, { path: 'structuredContent.result.items.1.output', equals: true }],
    captureResult: [{ key: 'subgraphInput', fromField: 'result.items.0.nodeId' }, { key: 'subgraphOutput', fromField: 'result.items.1.nodeId' },
      { key: 'boundaryBefore', fromField: 'result.revision' }] },
  pcg('Configure point-only subgraph input through the PCG write API', { action: 'set_pcg_node_settings', graphPath: subgraphPath,
    nodeId: captured('subgraphInput'), settings: pointBoundarySettings('In'), expectedGraphRevision: captured('boundaryBefore') },
    { captureResult: [{ key: 'boundaryAfterInput', fromField: 'result.revision' }] }),
  pcg('Configure point-only subgraph output through the PCG write API', { action: 'set_pcg_node_settings', graphPath: subgraphPath,
    nodeId: captured('subgraphOutput'), settings: pointBoundarySettings('Out'), expectedGraphRevision: captured('boundaryAfterInput') }),
  read('Capture the subgraph before random transform authoring', subgraphPath, 'subgraphRevision'),
  pcg('Author a seeded transform inside the real subgraph', { action: 'batch_edit_pcg_graph', graphPath: subgraphPath,
    expectedGraphRevision: captured('subgraphRevision'), operations: [
      { op: 'add_node', settingsClass: 'PCGTransformPointsSettings', tempId: 'transform', settings: {
        Seed: 12345, OffsetMin: { x: 0, y: 10, z: 20 }, OffsetMax: { x: 0, y: 30, z: 20 },
        RotationMin: { pitch: 0, yaw: 45, roll: 0 }, RotationMax: { pitch: 0, yaw: 45, roll: 0 },
        ScaleMin: { x: 1, y: 2, z: 3 }, ScaleMax: { x: 1, y: 2, z: 3 }, bUniformScale: false,
      } },
      { op: 'connect', sourceNodeId: 'input', sourcePin: 'In', targetNodeId: 'transform' },
      { op: 'connect', sourceNodeId: 'transform', targetNodeId: 'output', targetPin: 'Out' },
    ] }),
  pcg('Create the real Blueprint-execution node with an owned element instance', { action: 'add_pcg_node', graphPath,
    settingsClass: 'PCGBlueprintSettings', settings: { BlueprintElementType: `${blueprintPath}.BP_TaggedElement_C` } },
    { captureResult: [{ key: 'blueprintNode', fromField: 'result.nodeId' }] }),
  read('Capture parent revision after the subgraph and Blueprint are configured', graphPath, 'parentRevision'),
  pcg('Author spline distance filtering and two distinct mesh branches', { action: 'batch_edit_pcg_graph', graphPath,
    expectedGraphRevision: captured('parentRevision'), operations: [
      { op: 'add_node', settingsClass: 'PCGGetSplineSettings', tempId: 'spline' },
      { op: 'add_node', settingsClass: 'PCGSplineSamplerSettings', tempId: 'sampler', settings: {
        'SamplerParams.Mode': 'Distance', 'SamplerParams.DistanceIncrement': 100,
        'SamplerParams.bComputeDistance': true, 'SamplerParams.DistanceAttribute': 'MCPDistance', 'SamplerParams.bUnbounded': true,
      } },
      { op: 'add_node', settingsClass: 'PCGAttributeFilteringRangeSettings', tempId: 'filter', settings: {
        TargetAttribute: 'PCGBegin(MCPDistance)PCGEnd',
        'MinThreshold.bUseConstantThreshold': true, 'MinThreshold.AttributeTypes.DoubleValue': 200,
        'MaxThreshold.bUseConstantThreshold': true, 'MaxThreshold.AttributeTypes.DoubleValue': 500,
      } },
      { op: 'add_node', settingsClass: 'PCGStaticMeshSpawnerSettings', tempId: 'cube', meshPath: '/Engine/BasicShapes/Cube.Cube' },
      { op: 'add_node', settingsClass: 'PCGStaticMeshSpawnerSettings', tempId: 'sphere', meshPath: '/Engine/BasicShapes/Sphere.Sphere' },
      { op: 'add_node', settingsClass: 'PCGStaticMeshSpawnerSettings', tempId: 'cylinder', meshPath: '/Engine/BasicShapes/Cylinder.Cylinder' },
      { op: 'add_node', settingsClass: 'PCGSpawnActorSettings', tempId: 'actors', actorClass: '/Script/Engine.StaticMeshActor' },
      { op: 'connect', sourceNodeId: 'spline', targetNodeId: 'sampler', targetPin: 'Spline' },
      { op: 'connect', sourceNodeId: 'sampler', targetNodeId: 'filter' },
      { op: 'connect', sourceNodeId: 'filter', sourcePin: 'InsideFilter', targetNodeId: captured('subgraphNode'), targetPin: 'In' },
      { op: 'connect', sourceNodeId: captured('subgraphNode'), sourcePin: 'Out', targetNodeId: captured('blueprintNode') },
      { op: 'connect', sourceNodeId: captured('blueprintNode'), targetNodeId: 'cube' },
      { op: 'connect', sourceNodeId: captured('blueprintNode'), targetNodeId: 'sphere' },
      { op: 'connect', sourceNodeId: captured('blueprintNode'), targetNodeId: 'cylinder' },
      { op: 'connect', sourceNodeId: captured('blueprintNode'), targetNodeId: 'actors' },
      { op: 'connect', sourceNodeId: 'cylinder', targetNodeId: 'output' },
      { op: 'connect', sourceNodeId: 'actors', targetNodeId: 'output' },
      { op: 'connect', sourceNodeId: 'cube', targetNodeId: 'output' },
      { op: 'connect', sourceNodeId: 'sphere', targetNodeId: 'output' },
    ] }, { captureResult: [{ key: 'sphereNode', fromField: 'result.nodeIds.sphere' }] }),
  pcg('Configure the owned initialized selector for a real HISM branch', { action: 'set_pcg_node_settings', graphPath,
    nodeId: captured('sphereNode'), settings: {
      'MeshSelectorParameters.MeshEntries': [{ Descriptor: { StaticMesh: '/Engine/BasicShapes/Sphere.Sphere',
        ComponentClass: '/Script/Engine.HierarchicalInstancedStaticMeshComponent' }, Weight: 1 }],
    } }),
  ...(keepFixture ? [
    pcg('Persist a graph parameter default for cold-load inheritance', { action: 'add_pcg_parameter', graphPath,
      name: 'Spacing', type: 'Double', value: 200 }),
    pcg('Persist an exact full-range Int64 graph default', { action: 'add_pcg_parameter', graphPath,
      name: 'ExactInteger', type: 'Int64', value: '9223372036854775807' }),
    pcg('Persist a mixed exact Int64 array default', { action: 'add_pcg_parameter', graphPath,
      name: 'ExactIntegerArray', type: 'Int64', container: 'array', value: ['9007199254740993', '-9223372036854775808', 7] }),
    pcg('Persist a GUID-bound getter alongside the complex workflow', { action: 'add_pcg_node', graphPath,
      settingsClass: 'PCGUserParameterGetSettings', parameterName: 'Spacing' },
      { captureResult: [{ key: 'getterNodeId', fromField: 'result.nodeId' }] }),
    { scenario: 'Capture the persisted graph parameter identity before restart', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_parameters', graphPath, name: 'Spacing', limit: 1 }, expected: 'success', abortOnFailure: true,
      captureResult: [{ key: 'spacingId', fromField: 'result.parameters.0.id' }] },
    { scenario: 'Capture the actual serialized getter GUID before restart', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_graph', graphPath, nodeId: captured('getterNodeId'), includeSettings: true },
      expected: 'success', abortOnFailure: true, captureResult: [{ key: 'getterGuid', fromField: 'result.items.0.bindings.PropertyGuid' }] },
  ] : []),
  { scenario: 'Create only this run\'s linear thousand-unit spline owner', toolName: 'build_environment',
    arguments: { action: 'create_spline_actor', name: actorName, points: [{ position: { x: 0, y: 0, z: 0 } }, { position: { x: 1000, y: 0, z: 0 } }], splineType: 'Linear' },
    expected: 'success', abortOnFailure: true },
  pcg('Schedule the complete spline-filter-subgraph-Blueprint workflow', { action: 'execute_pcg_graph', graphPath, actorName, createComponent: true },
    { captureResult: [{ key: 'component', fromField: 'result.componentPath' }, { key: 'operation', fromField: 'result.operationId' }] }),
  { scenario: 'Wait for the actual native workflow to settle before inspection', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_operation_status', componentPath, operationId: captured('operation') }, expected: 'success',
    pollUntil: { timeoutMs: 20000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
      { path: 'structuredContent.result.completed', equals: true }, { path: 'structuredContent.result.engineStillRunning', equals: false },
      { path: 'structuredContent.result.state', equals: 'succeeded' },
    ] },
  ...(keepFixture ? [
    pcg('Persist an independent component graph-parameter override', { action: 'set_pcg_parameter', target: 'component',
      componentPath, name: 'Spacing', value: 333 }),
    pcg('Persist the exact full-range Int64 component override', { action: 'set_pcg_parameter', target: 'component',
      componentPath, name: 'ExactInteger', value: '-9223372036854775808' }),
    { scenario: 'Wait for any native auto-generation caused by the override', toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 }, expected: 'success',
      pollUntil: { timeoutMs: 20000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.generating', equals: false }, { path: 'structuredContent.result.cleaning', equals: false },
      ] },
  ] : []),
  { scenario: 'Four filtered points produce distinct ISM/HISM resources, managed actors and custom output tags', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, limit: 100, sampleLimit: 16 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.instanceCount', equals: 12 },
      { path: 'structuredContent.result.ismComponentCount', equals: 2 },
      { path: 'structuredContent.result.hismComponentCount', equals: 1 },
      { path: 'structuredContent.result.managedRootActorCount', equals: 4 },
      { path: 'structuredContent.result.outputDataCount', equals: 4 },
      { path: 'structuredContent.result.outputPointCount', equals: 16 },
      { path: 'structuredContent.result.outputs', length: 4 },
      ...[0, 1, 2, 3].flatMap(index => [{ path: `structuredContent.result.outputs.${index}.pointCount`, equals: 4 },
        { path: `structuredContent.result.outputs.${index}.tags.0`, equals: 'MCP_CustomNode' }]),
      { path: 'structuredContent.result.outputs.0.tags.0', equals: 'MCP_CustomNode' },
      { path: 'structuredContent.result.outputs.1.tags.0', equals: 'MCP_CustomNode' },
      ...transformAssertions,
    ], captureResult: [...[0, 1, 2, 3].map(index => ({ key: `actor${index}`, fromField: `result.actors.${index}.actorPath` })), ...meshPaths.flatMap((meshPath, mesh) => [0, 1, 2, 3].map(index => ({
      key: `randomY${mesh}_${index}`, fromField: 'result.components', where: { path: 'meshPath', equals: meshPath },
      selectField: `sampleTransforms.${index}.location.y`,
    }))) ] },
  { scenario: 'Verify all four managed actor transforms independently of actor name ordering', toolName: 'system_control',
    arguments: { action: 'execute_python', code: `import unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
component = unreal.find_object(None, '${captured('component')}')
actors = [unreal.find_object(None, actor_path) for actor_path in [${[0, 1, 2, 3].map(index => `'${captured(`actor${index}`)}'`).join(', ')}]]
assert all(actor is not None and actor.get_world() == component.get_world() for actor in actors)
actors.sort(key=lambda actor: actor.get_actor_location().x)
expected_y = [${[0, 1, 2, 3].map(index => captured(`randomY0_${index}`)).join(', ')}]
for index, actor in enumerate(actors):
    position, rotation, scale = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
    assert abs(position.x - (200 + index * 100)) < 0.0001 and abs(position.y - expected_y[index]) < 0.0001 and abs(position.z - 20) < 0.0001
    assert abs(rotation.pitch) < 0.0001 and abs(rotation.yaw - 45) < 0.0001 and abs(rotation.roll) < 0.0001
    assert abs(scale.x - 1) < 0.00001 and abs(scale.y - 2) < 0.00001 and abs(scale.z - 3) < 0.00001
` }, expected: 'success', abortOnFailure: true },
  { scenario: 'Bounded component and output pages retain complete inventory totals', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, offset: 0, limit: 1, sampleLimit: 16 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.components', length: 1 }, { path: 'structuredContent.result.actors', length: 1 },
      { path: 'structuredContent.result.componentsTruncated', equals: true }, { path: 'structuredContent.result.actorsTruncated', equals: true },
      { path: 'structuredContent.result.instanceCount', equals: 12 },
    ] },

  pcg('Regenerate the same fixed-seed workflow without accumulating resources', { action: 'generate_pcg_graph', componentPath, timeoutSeconds: 20 },
    { timeoutMs: 30000, assertions: [{ path: 'structuredContent.result.resources.instanceCount', equals: 12 }] }),
  { scenario: 'Every random transform remains identical for the fixed component and node seeds', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, limit: 100, sampleLimit: 16 }, expected: 'success', assertions:
      meshPaths.flatMap((meshPath, mesh) => [
        { path: `structuredContent.result.components.${mesh}.meshPath`, equals: meshPath },
        ...[0, 1, 2, 3].map(index => ({ path: `structuredContent.result.components.${mesh}.sampleTransforms.${index}.location.y`,
          approximately: captured(`randomY${mesh}_${index}`), tolerance: 0.0001 })),
      ]),
  },

  ...(!keepFixture ? [{ scenario: 'Clean only the generated workflow resources', toolName: 'manage_pcg',
    arguments: { action: 'cleanup_pcg_component', componentPath, timeoutSeconds: 20, save: false }, expected: 'success', timeoutMs: 30000,
    consent: { capability: 'manage_pcg.cleanup_pcg_component', acknowledge: 'explicit' } },
  { scenario: 'Require native idle and zero resources before removing the fixture', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 }, expected: 'success',
    pollUntil: { timeoutMs: 20000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
      { path: 'structuredContent.result.generating', equals: false }, { path: 'structuredContent.result.cleaning', equals: false },
      { path: 'structuredContent.result.instanceCount', equals: 0 },
    ] },
  { scenario: 'Delete only the owned idle spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [actorName] },
    expected: 'success', abortOnFailure: true, consent: { capability: 'control_actor.delete', acknowledge: 'explicit' } },
  ...[captured('ownedGraph'), captured('ownedSubgraph'), blueprintPath].map(asset => ({ scenario: `Delete this run's owned workflow asset ${asset}`,
    toolName: 'manage_asset', arguments: { action: 'delete', path: asset }, expected: 'success',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' } })),
  ] : [{ scenario: 'Require native idle before recording persistent evidence', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 }, expected: 'success',
    pollUntil: { timeoutMs: 20000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
      { path: 'structuredContent.result.generating', equals: false }, { path: 'structuredContent.result.cleaning', equals: false },
    ] },
  { scenario: 'Write only this owned persistent workflow manifest for a fresh editor process', toolName: 'system_control',
    arguments: { action: 'execute_python', code: `import unreal, json, pathlib
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
component = unreal.find_object(None, '${captured('component')}')
assert component is not None
assert component.get_world().get_path_name().split('.')[0] == '${mapPath}'
graph = unreal.load_asset('${graphPath}')
subgraph = unreal.load_asset('${subgraphPath}')
manifest = dict(root='${root}', mapPath='${mapPath}', graphPath='${graphPath}', subgraphPath='${subgraphPath}',
    blueprintPath='${blueprintPath}', actorName=component.get_owner().get_name(), componentPath=component.get_path_name(),
    getterNodeId='${captured('getterNodeId')}', getterGuid='${captured('getterGuid')}', parameterDefaults=dict(Spacing=200),
    parameterIds=dict(Spacing='${captured('spacingId')}'), componentOverride=333,
    exactIntegerDefault='9223372036854775807', exactIntegerOverride='-9223372036854775808',
    exactIntegerArray=['9007199254740993', '-9223372036854775808', 7],
    sampleY=[${meshPaths.map((_, mesh) => `[${[0, 1, 2, 3].map(index => captured(`randomY${mesh}_${index}`)).join(', ')}]`).join(', ')}],
    nodeIds=[node.get_name() for node in graph.get_editor_property('Nodes')],
    subgraphNodeIds=[node.get_name() for node in subgraph.get_editor_property('Nodes')])
output_dir = pathlib.Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'PCGValidation'
output_dir.mkdir(parents=True, exist_ok=True)
output_file = output_dir / 'workflow-persistence-${stamp}.json'
with output_file.open('x', encoding='utf-8') as stream:
    json.dump(manifest, stream, indent=2)
print('Owned persistence manifest: ' + str(output_file))
` }, expected: 'success', abortOnFailure: true }]),
];

runToolTests('manage_pcg_workflow', cases, {
  isolatedProject: { projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME, projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR, engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION },
  foldTwins: false,
});
