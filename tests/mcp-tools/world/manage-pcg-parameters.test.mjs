import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const runRoot = `/Game/MCP_PCG_Validation/Parameters_${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const graphPath = `${runRoot}/Graph`;
const instancePath = `${runRoot}/Instance`;
const captured = key => '${captured:' + key + '}';
const read = (scenario, name, extra = {}) => ({ scenario, toolName: 'manage_pcg',
  arguments: { action: 'get_pcg_parameters', target: 'graph', graphPath, name, offset: 0, limit: 1 }, expected: 'success', ...extra });
const instanceFixture = `import unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
assert not unreal.EditorAssetLibrary.does_asset_exist('${instancePath}')
factory = unreal.PCGGraphInstanceFactory()
instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Instance', '${runRoot}', unreal.PCGGraphInstance, factory)
assert instance is not None
instance.set_editor_property('Graph', unreal.load_asset('${graphPath}'))
assert instance.get_editor_property('Graph').get_path_name() == '${graphPath}.Graph'
assert instance.get_path_name() == '${instancePath}.Instance'
`;
const typedParameters = [
  { name: 'BoolValue', type: 'Bool', value: true, readback: true },
  { name: 'BoolList', type: 'Bool', container: 'array', value: [true, false], readback: true },
  { name: 'ByteValue', type: 'Byte', value: 7, readback: 7 },
  { name: 'ByteList', type: 'Byte', container: 'array', value: [7, 8], readback: 7 },
  { name: 'Int32Value', type: 'Int32', value: 123, readback: 123 },
  { name: 'Int64Value', type: 'Int64', value: 1234567890123, readback: 1234567890123 },
  { name: 'Int64List', type: 'Int64', container: 'array', value: [123, 456], readback: 123 },
  { name: 'Int64Maximum', type: 'Int64', value: '9223372036854775807', readback: '9223372036854775807' },
  { name: 'Int64Minimum', type: 'Int64', value: '-9223372036854775808', readback: '-9223372036854775808' },
  { name: 'Int64Safe', type: 'Int64', value: 9007199254740991, readback: 9007199254740991 },
  { name: 'Int64NegativeSafe', type: 'Int64', value: -9007199254740991, readback: -9007199254740991 },
  { name: 'Int64ExactList', type: 'Int64', container: 'array',
    value: ['9007199254740993', '-9223372036854775808', '9223372036854775807', 9007199254740991, 0],
    readbackValues: ['9007199254740993', '-9223372036854775808', '9223372036854775807', 9007199254740991, 0] },
  { name: 'FloatValue', type: 'Float', value: 1.25, readback: 1.25 },
  { name: 'FloatList', type: 'Float', container: 'array', value: [1.25, 2.5], readback: 1.25 },
  { name: 'DoubleList', type: 'Double', container: 'array', value: [2.5, 5.25], readback: 2.5 },
  { name: 'NameValue', type: 'Name', value: 'NativeName', readback: 'NativeName' },
  { name: 'NameList', type: 'Name', container: 'array', value: ['NativeName', 'SecondName'], readback: 'NativeName' },
  { name: 'StringValue', type: 'String', value: 'Native string', readback: 'Native string' },
  { name: 'StringList', type: 'String', container: 'array', value: ['Native string', 'Second string'], readback: 'Native string' },
  { name: 'EnumValue', type: 'Enum', typeObject: '/Script/Engine.ECollisionChannel', value: 'ECC_WorldStatic', readback: 'ECC_WorldStatic' },
  { name: 'EnumList', type: 'Enum', typeObject: '/Script/Engine.ECollisionChannel', container: 'array', value: ['ECC_WorldStatic', 'ECC_WorldDynamic'], readback: 'ECC_WorldStatic' },
  { name: 'VectorList', type: 'Struct', typeObject: '/Script/CoreUObject.Vector', container: 'array', value: [{ x: 1, y: 2, z: 3 }, { x: 4, y: 5, z: 6 }], readback: [1, 2, 3] },
  { name: 'RotatorValue', type: 'Struct', typeObject: '/Script/CoreUObject.Rotator', value: { pitch: 10, yaw: 20, roll: 30 }, readback: [10, 20, 30] },
  { name: 'RotatorList', type: 'Struct', typeObject: '/Script/CoreUObject.Rotator', container: 'array', value: [{ pitch: 10, yaw: 20, roll: 30 }, { pitch: 0, yaw: 45, roll: 0 }], readback: [10, 20, 30] },
  { name: 'TransformValue', type: 'Struct', typeObject: '/Script/CoreUObject.Transform', value: { rotation: { x: 0, y: 0, z: 0, w: 1 }, translation: { x: 1, y: 2, z: 3 }, scale3d: { x: 1, y: 1, z: 1 } } },
  { name: 'TransformList', type: 'Struct', typeObject: '/Script/CoreUObject.Transform', container: 'array', value: [
    { rotation: { x: 0, y: 0, z: 0, w: 1 }, translation: { x: 1, y: 2, z: 3 }, scale3d: { x: 1, y: 1, z: 1 } },
    { rotation: { x: 0, y: 0.7071068, z: 0, w: 0.7071068 }, translation: { x: 4, y: 5, z: 6 }, scale3d: { x: 2, y: 2, z: 2 } },
  ] },
  { name: 'ObjectValue', type: 'Object', typeObject: '/Script/Engine.StaticMesh', value: '/Engine/BasicShapes/Cube.Cube', readback: '/Engine/BasicShapes/Cube.Cube' },
  { name: 'ObjectList', type: 'Object', typeObject: '/Script/Engine.StaticMesh', container: 'array', value: ['/Engine/BasicShapes/Cube.Cube', '/Engine/BasicShapes/Sphere.Sphere'], readback: '/Engine/BasicShapes/Cube.Cube' },
  { name: 'ClassValue', type: 'Class', typeObject: '/Script/Engine.Actor', value: '/Script/Engine.StaticMeshActor', readback: '/Script/Engine.StaticMeshActor' },
  { name: 'ClassList', type: 'Class', typeObject: '/Script/Engine.Actor', container: 'array', value: ['/Script/Engine.StaticMeshActor', '/Script/Engine.StaticMeshActor'], readback: '/Script/Engine.StaticMeshActor' },
  { name: 'SoftObjectValue', type: 'SoftObject', typeObject: '/Script/Engine.StaticMesh', value: '/Engine/BasicShapes/Cube.Cube', readback: '/Engine/BasicShapes/Cube.Cube' },
  { name: 'SoftObjectList', type: 'SoftObject', typeObject: '/Script/Engine.StaticMesh', container: 'array', value: ['/Engine/BasicShapes/Cube.Cube', '/Engine/BasicShapes/Sphere.Sphere'], readback: '/Engine/BasicShapes/Cube.Cube' },
  { name: 'SoftClassValue', type: 'SoftClass', typeObject: '/Script/Engine.Actor', value: '/Script/Engine.StaticMeshActor', readback: '/Script/Engine.StaticMeshActor' },
  { name: 'SoftClassList', type: 'SoftClass', typeObject: '/Script/Engine.Actor', container: 'array', value: ['/Script/Engine.StaticMeshActor', '/Script/Engine.StaticMeshActor'], readback: '/Script/Engine.StaticMeshActor' },
];
const typedParameterCases = typedParameters.flatMap(parameter => {
  const container = parameter.container ?? 'none';
  const arguments_ = { action: 'add_pcg_parameter', graphPath, name: parameter.name, type: parameter.type,
    ...(parameter.typeObject ? { typeObject: parameter.typeObject } : {}), container, value: parameter.value, save: false };
  const assertions = [{ path: 'structuredContent.result.parameters.0.name', equals: parameter.name }];
  if (container === 'array') assertions.push({ path: 'structuredContent.result.parameters.0.value', length: parameter.value.length });
  if (parameter.readback !== undefined) {
    const valuePath = `structuredContent.result.parameters.0.value${container === 'array' ? '.0' : ''}`;
    if (Array.isArray(parameter.readback)) {
      parameter.readback.forEach((value, index) => assertions.push({ path: `${valuePath}.${index}`, equals: value }));
    } else assertions.push({ path: valuePath, equals: parameter.readback });
  }
  if (parameter.name.startsWith('Transform')) {
    assertions.push({ path: `structuredContent.result.parameters.0.value${container === 'array' ? '.0' : ''}`,
      includes: 'Translation=(X=1.000000,Y=2.000000,Z=3.000000)' });
  }
  if (parameter.type === 'Int64') {
    if (container === 'none') assertions.push({ path: 'structuredContent.result.parameters.0.defaultValue', equals: parameter.readback });
    const expected = parameter.readbackValues ?? (container === 'array' ? parameter.value : undefined);
    expected?.forEach((value, index) => {
      assertions.push({ path: 'structuredContent.result.parameters.0.value.' + index, equals: value });
      assertions.push({ path: 'structuredContent.result.parameters.0.defaultValue.' + index, equals: value });
    });
  }
  return [
    { scenario: `Add ${container} ${parameter.type} PCG parameter ${parameter.name}`, toolName: 'manage_pcg', arguments: arguments_, expected: 'success',
      ...(parameter.type === 'Int64' && container === 'none' ? { assertions: [{ path: 'structuredContent.result.parameters',
        includesObject: { name: parameter.name, value: parameter.readback, defaultValue: parameter.readback } }] } : {}) },
    read(`Read back ${parameter.type} PCG parameter ${parameter.name}`, parameter.name, { assertions }),
  ];
});
const cases = [
  { scenario: 'Setup: create an exclusively owned parameter graph', toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', graphPath, save: false }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.created', equals: true }],
    captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }] },
  { scenario: 'Add a typed graph default with the explicit parameter family selector', toolName: 'manage_pcg',
    arguments: { action: 'edit_pcg_parameters', parameterEdit: 'add', target: 'graph', graphPath, name: 'Spacing', type: 'Double', container: 'none', value: 100, save: false }, expected: 'success' },
  read('Read the stable parameter ID and exact default', 'Spacing', {
    assertions: [{ path: 'structuredContent.result.parameters.0.value', equals: 100 }],
    captureResult: [{ key: 'spacingId', fromField: 'result.parameters.0.id' }, { key: 'before', fromField: 'result.revision' }],
  }),
  { scenario: 'Set a graph default against its observed revision', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_parameter', target: 'graph', graphPath, name: 'Spacing', value: 200, expectedGraphRevision: captured('before'), save: false }, expected: 'success' },
  { scenario: 'Add a bounded native array default', toolName: 'manage_pcg',
    arguments: { action: 'add_pcg_parameter', graphPath, name: 'Seeds', type: 'Int32', container: 'array', value: [1, 2, 3], save: false }, expected: 'success' },
  { scenario: 'Add a native Vector parameter using typeObject', toolName: 'manage_pcg',
    arguments: { action: 'add_pcg_parameter', graphPath, name: 'Offset', type: 'Struct', typeObject: '/Script/CoreUObject.Vector', value: { x: 1, y: 2, z: 3 }, save: false }, expected: 'success' },
  ...typedParameterCases,
  read('Capture exact Int64 state before unsafe numeric writes', 'Int64Value', {
    captureResult: [{ key: 'beforeUnsafeInt64', fromField: 'result.revision' }],
  }),
  ...[9007199254740992, -9007199254740992].flatMap(value => [
    { scenario: `Reject unsafe numeric Int64 scalar ${value}`, toolName: 'manage_pcg',
      arguments: { action: 'set_pcg_parameter', graphPath, name: 'Int64Value', value, save: false },
      expected: { condition: 'error', errorPattern: 'INVALID_PARAMETER_VALUE' } },
    { scenario: `Reject unsafe numeric Int64 array element ${value} after a valid prefix`, toolName: 'manage_pcg',
      arguments: { action: 'set_pcg_parameter', graphPath, name: 'Int64List', value: [1, value], save: false },
      expected: { condition: 'error', errorPattern: 'INVALID_PARAMETER_VALUE' } },
  ]),
  read('Rejected unsafe numbers preserve the Int64 default and revision', 'Int64Value', { assertions: [
    { path: 'structuredContent.result.revision', equals: captured('beforeUnsafeInt64') },
    { path: 'structuredContent.result.parameters.0.value', equals: 1234567890123 },
    { path: 'structuredContent.result.parameters.0.defaultValue', equals: 1234567890123 },
  ] }),
  read('Rejected unsafe numbers preserve the entire Int64 array and revision', 'Int64List', { assertions: [
    { path: 'structuredContent.result.revision', equals: captured('beforeUnsafeInt64') },
    { path: 'structuredContent.result.parameters.0.value', length: 2 },
    { path: 'structuredContent.result.parameters.0.value.0', equals: 123 },
    { path: 'structuredContent.result.parameters.0.value.1', equals: 456 },
  ] }),
  ...['none', 'array'].map(container => ({ scenario: `Reject unsafe numeric initial Int64 ${container} without adding schema`, toolName: 'manage_pcg',
    arguments: { action: 'add_pcg_parameter', graphPath, name: 'UnsafeNumber', type: 'Int64', container,
      value: container === 'array' ? [1, 9007199254740992] : 9007199254740992, save: false },
    expected: { condition: 'error', errorPattern: 'INVALID_PARAMETER_VALUE' } })),
  read('Unsafe numeric initial values leave no parameter or revision change', 'UnsafeNumber', { assertions: [
    { path: 'structuredContent.result.revision', equals: captured('beforeUnsafeInt64') },
    { path: 'structuredContent.result.parameters', length: 0 },
    { path: 'structuredContent.result.total', equals: 0 },
  ] }),
  read('Capture a boolean array before a rejected element', 'BoolList', { captureResult: [{ key: 'beforeInvalidBoolArray', fromField: 'result.revision' }] }),
  { scenario: 'An invalid boolean array element cannot partially replace the source array', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_parameter', graphPath, name: 'BoolList', value: [true, 'invalid-bool'], save: false },
    expected: { condition: 'error', errorPattern: 'INVALID_PARAMETER_VALUE' } },
  read('A refused boolean array edit leaves its value and revision unchanged', 'BoolList', { assertions: [
    { path: 'structuredContent.result.revision', equals: captured('beforeInvalidBoolArray') },
    { path: 'structuredContent.result.parameters.0.value', length: 2 },
    { path: 'structuredContent.result.parameters.0.value.0', equals: true },
    { path: 'structuredContent.result.parameters.0.value.1', equals: false },
  ] }),
  read('Capture state before a rejected value', 'Spacing', { captureResult: [{ key: 'beforeInvalid', fromField: 'result.revision' }] }),
  { scenario: 'A bad value cannot partially change a graph default', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_parameter', graphPath, name: 'Spacing', value: 'invalid-double', save: false },
    expected: { condition: 'error', errorPattern: 'INVALID_PARAMETER_VALUE' } },
  read('Verify the failed parameter edit left the graph unchanged', 'Spacing', { assertions: [
    { path: 'structuredContent.result.revision', equals: captured('beforeInvalid') },
    { path: 'structuredContent.result.parameters.0.value', equals: 200 },
  ] }),
  { scenario: 'An invalid array element cannot silently become zero', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_parameter', graphPath, name: 'Seeds', value: [1, 'invalid-integer', 3], save: false },
    expected: { condition: 'error', errorPattern: 'INVALID_PARAMETER_VALUE' } },
  read('A refused array edit leaves all parameter values unchanged', 'Seeds', { assertions: [
    { path: 'structuredContent.result.revision', equals: captured('beforeInvalid') },
    { path: 'structuredContent.result.parameters.0.value', length: 3 },
    { path: 'structuredContent.result.parameters.0.value.0', equals: 1 },
    { path: 'structuredContent.result.parameters.0.value.1', equals: 2 },
    { path: 'structuredContent.result.parameters.0.value.2', equals: 3 },
  ] }),
  { scenario: 'Setup: create an exclusively owned graph-instance asset with the native factory', toolName: 'system_control',
    arguments: { action: 'execute_python', code: instanceFixture }, expected: 'success' },
  { scenario: 'Read graph-instance inheritance by its exact asset path', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_parameters', target: 'graphInstance', graphInstancePath: instancePath, name: 'Spacing', offset: 0, limit: 1 }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.parameters.0.value', equals: 200 }, { path: 'structuredContent.result.parameters.0.overridden', equals: false }] },
  ...[
    { name: 'Int64Value', invalid: 9007199254740992, expected: [1234567890123] },
    { name: 'Int64List', invalid: [1, -9007199254740992], expected: [123, 456] },
  ].flatMap(parameter => [
    { scenario: `Reject unsafe numeric instance override ${parameter.name}`, toolName: 'manage_pcg',
      arguments: { action: 'set_pcg_parameter', target: 'graphInstance', graphInstancePath: instancePath,
        name: parameter.name, value: parameter.invalid, save: false },
      expected: { condition: 'error', errorPattern: 'INVALID_PARAMETER_VALUE' } },
    { scenario: `Refused unsafe override preserves inheritance for ${parameter.name}`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_parameters', target: 'graphInstance', graphInstancePath: instancePath,
        name: parameter.name, offset: 0, limit: 1 }, expected: 'success',
      assertions: [{ path: 'structuredContent.result.parameters.0.overridden', equals: false },
        ...(parameter.name === 'Int64List' ? [{ path: 'structuredContent.result.parameters.0.value', length: 2 }] : []),
        ...parameter.expected.map((value, index) => ({
          path: `structuredContent.result.parameters.0.value${parameter.name === 'Int64List' ? `.${index}` : ''}`, equals: value,
        }))] },
  ]),
  { scenario: 'Change a complex graph default after creating the instance', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_parameter', target: 'graph', graphPath, name: 'Offset', value: { x: 4, y: 5, z: 6 }, save: false }, expected: 'success' },
  read('Read the updated complex graph default', 'Offset', { assertions: [
    { path: 'structuredContent.result.parameters.0.value.0', equals: 4 },
    { path: 'structuredContent.result.parameters.0.value.1', equals: 5 },
    { path: 'structuredContent.result.parameters.0.value.2', equals: 6 },
  ] }),
  { scenario: 'An unmodified instance inherits the updated complex default', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_parameters', target: 'graphInstance', graphInstancePath: instancePath, name: 'Offset', offset: 0, limit: 1 }, expected: 'success',
    assertions: [
      { path: 'structuredContent.result.parameters.0.value.0', equals: 4 },
      { path: 'structuredContent.result.parameters.0.value.1', equals: 5 },
      { path: 'structuredContent.result.parameters.0.value.2', equals: 6 },
      { path: 'structuredContent.result.parameters.0.overridden', equals: false },
    ] },
  { scenario: 'Override only the complex instance value', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_parameter', target: 'graphInstance', graphInstancePath: instancePath, name: 'Offset', value: { x: 9, y: 8, z: 7 }, save: false }, expected: 'success' },
  { scenario: 'Read the complex instance override independently', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_parameters', target: 'graphInstance', graphInstancePath: instancePath, name: 'Offset', offset: 0, limit: 1 }, expected: 'success',
    assertions: [
      { path: 'structuredContent.result.parameters.0.value.0', equals: 9 },
      { path: 'structuredContent.result.parameters.0.value.1', equals: 8 },
      { path: 'structuredContent.result.parameters.0.value.2', equals: 7 },
      { path: 'structuredContent.result.parameters.0.overridden', equals: true },
    ] },
  read('The instance override does not change the complex graph default', 'Offset', { assertions: [
    { path: 'structuredContent.result.parameters.0.value.0', equals: 4 },
    { path: 'structuredContent.result.parameters.0.value.1', equals: 5 },
    { path: 'structuredContent.result.parameters.0.value.2', equals: 6 },
  ] }),
  { scenario: 'Reset the complex instance override and resume default inheritance', toolName: 'manage_pcg',
    arguments: { action: 'reset_pcg_parameter_override', target: 'graphInstance', graphInstancePath: instancePath, name: 'Offset', save: false }, expected: 'success' },
  { scenario: 'A reset complex instance override inherits the current default', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_parameters', target: 'graphInstance', graphInstancePath: instancePath, name: 'Offset', offset: 0, limit: 1 }, expected: 'success',
    assertions: [
      { path: 'structuredContent.result.parameters.0.value.0', equals: 4 },
      { path: 'structuredContent.result.parameters.0.value.1', equals: 5 },
      { path: 'structuredContent.result.parameters.0.value.2', equals: 6 },
      { path: 'structuredContent.result.parameters.0.overridden', equals: false },
    ] },
  { scenario: 'Override a single graph instance without changing the default', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_parameter', target: 'graphInstance', graphInstancePath: instancePath, name: 'Spacing', value: 333, save: false }, expected: 'success' },
  { scenario: 'Reset the instance override and resume inheritance', toolName: 'manage_pcg',
    arguments: { action: 'reset_pcg_parameter_override', target: 'graphInstance', graphInstancePath: instancePath, name: 'Spacing', save: false }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.parameters', includesObject: { name: 'Spacing', value: 200, overridden: false } }] },
  { scenario: 'Bind a native getter to the stable graph parameter', toolName: 'manage_pcg',
    arguments: { action: 'add_pcg_node', graphPath, settingsClass: 'PCGUserParameterGetSettings', parameterName: 'Spacing', save: false }, expected: 'success',
    captureResult: [{ key: 'getter', fromField: 'result.nodeId' }] },
  { scenario: 'Rename through the native API while retaining references', toolName: 'manage_pcg',
    arguments: { action: 'rename_pcg_parameter', graphPath, name: 'Spacing', newName: 'PointSpacing', save: false }, expected: 'success' },
  read('Rename keeps the parameter GUID', 'PointSpacing', { assertions: [{ path: 'structuredContent.result.parameters.0.id', equals: captured('spacingId') }],
    captureResult: [{ key: 'beforeRemoval', fromField: 'result.revision' }] }),
  { scenario: 'A referenced parameter cannot be removed implicitly', toolName: 'manage_pcg',
    arguments: { action: 'remove_pcg_parameter', target: 'graph', graphPath, name: 'PointSpacing', expectedGraphRevision: captured('beforeRemoval'), save: false },
    consent: { capability: 'manage_pcg.remove_pcg_parameter', acknowledge: 'explicit' },
    expected: { condition: 'error', errorPattern: 'PARAMETER_IN_USE' } },
  read('Refused removal leaves the entire graph unchanged', 'PointSpacing', { assertions: [{ path: 'structuredContent.result.revision', equals: captured('beforeRemoval') }] }),
  { scenario: 'Remove an unused parameter without cascading nodes', toolName: 'manage_pcg',
    arguments: { action: 'remove_pcg_parameter', graphPath, name: 'Seeds', save: false }, expected: 'success',
    consent: { capability: 'manage_pcg.remove_pcg_parameter', acknowledge: 'explicit' } },
  { scenario: 'Cleanup: delete only the uniquely named instance created above', toolName: 'manage_asset',
    arguments: { action: 'delete', path: instancePath }, expected: 'success',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' } },
  { scenario: 'Cleanup: delete only the graph proven created by this run', toolName: 'manage_asset',
    arguments: { action: 'delete', path: captured('ownedGraph') }, expected: 'success',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' } },
];

runToolTests('manage_pcg_parameters', cases, {
  isolatedProject: { projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME, projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR, engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION },
  foldTwins: false,
});
