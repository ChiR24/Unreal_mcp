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
  { scenario: 'INFO: raycast_mesh takes {start, end} lines and stops each at its end', toolName: 'inspect', arguments: { action: 'raycast_mesh', meshPath: '/Engine/BasicShapes/Cube', rays: [{ start: { x: 200, y: 0, z: 0 }, end: { x: -200, y: 0, z: 0 } }, { start: { x: 200, y: 0, z: 0 }, end: { x: 100, y: 0, z: 0 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.hitCount', equals: 1, label: 'only the line that reaches the face hits' }, { path: 'structuredContent.result.hits.0.distance', approximately: 150, tolerance: 0.01, label: 'the +X face is 150 cm from start' }, { path: 'structuredContent.result.hits.1.hit', equals: false, label: 'the line ending short of the face misses' }] },
  { scenario: 'ERROR: raycast_mesh refuses a ray with neither form', toolName: 'inspect', arguments: { action: 'raycast_mesh', meshPath: '/Engine/BasicShapes/Cube', rays: [{ origin: { x: 200, y: 0, z: 0 } }] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: raycast_mesh refuses a texture', toolName: 'inspect', arguments: { action: 'raycast_mesh', meshPath: ENGINE_DEFAULT_TEXTURE, rays: [{ origin: { x: 0, y: 0, z: 100 }, direction: { x: 0, y: 0, z: -1 } }] }, expected: 'error|NOT_FOUND' },
  // A cube is one closed part; a plane is one open sheet with a single rim.
  { scenario: 'INFO: check_mesh finds the cube closed', toolName: 'inspect', arguments: { action: 'check_mesh', meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success', assertions: [{ path: 'structuredContent.result.closed', equals: true, label: 'the cube is watertight' }, { path: 'structuredContent.result.parts', equals: 1, label: 'in one part' }] },
  { scenario: 'INFO: check_mesh finds the plane open', toolName: 'inspect', arguments: { action: 'check_mesh', meshPath: '/Engine/BasicShapes/Plane' }, expected: 'success', assertions: [{ path: 'structuredContent.result.closed', equals: false, label: 'a plane is a sheet' }, { path: 'structuredContent.result.holes', equals: 1, label: 'with one rim' }] },
  { scenario: 'ERROR: check_mesh refuses a texture', toolName: 'inspect', arguments: { action: 'check_mesh', meshPath: ENGINE_DEFAULT_TEXTURE }, expected: 'error|NOT_FOUND' },
  { scenario: 'INFO: describe_view reads what a camera sees', toolName: 'inspect', arguments: { action: 'describe_view', location: { x: 0, y: 0, z: 400 }, rotation: { pitch: -60, yaw: 0, roll: 0 }, limit: 5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.background', gte: 0, label: 'the frame is accounted for' }] },
  { scenario: 'INFO: describe_view with actors says why a named actor is not seen', toolName: 'inspect', arguments: { action: 'describe_view', location: { x: 2000, y: 0, z: 400 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, actors: [ACTOR, 'NoSuchActorForDescribeView'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.notVisible.0.reason', equals: 'outside the frame', label: 'the cube behind the camera is outside the frame' }, { path: 'structuredContent.result.notFound.0', equals: 'NoSuchActorForDescribeView', label: 'an unknown name is listed' }] },
  { scenario: 'INFO: describe_view points names the cube under the middle of a shot straight down', toolName: 'inspect', arguments: { action: 'describe_view', location: { x: 0, y: 0, z: 400 }, rotation: { pitch: -90, yaw: 0, roll: 0 }, points: [{ x: 0.5, y: 0.5 }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.points.0.actorName', equals: ACTOR, label: 'the cube is drawn there' }, { path: 'structuredContent.result.points.0.hit.location.z', approximately: 170, tolerance: 1, label: 'the line meets its top at z 170' }] },
  { scenario: 'INFO: describe_view labels returns the frame with the listed actors numbered', toolName: 'inspect', arguments: { action: 'describe_view', location: { x: 0, y: 0, z: 400 }, rotation: { pitch: -60, yaw: 0, roll: 0 }, limit: 3, labels: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.mimeType', equals: 'image/png', label: 'the labelled frame came back as a PNG' }] },
  { scenario: 'INFO: capture_passes writes the id, depth and normal passes', toolName: 'inspect', arguments: { action: 'capture_passes', location: { x: 0, y: 0, z: 400 }, rotation: { pitch: -60, yaw: 0, roll: 0 }, passes: ['id', 'depth', 'normal'], width: 320, outputPath: 'Saved/MCPTest/Captures', name: 'PassesCase' }, expected: 'success', assertions: [{ path: 'structuredContent.result.width', equals: 320, label: 'the passes are as wide as asked' }] },
  { scenario: 'ERROR: capture_passes refuses an unknown pass', toolName: 'inspect', arguments: { action: 'capture_passes', passes: ['albedo'] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'INFO: raycast_world lands on the cube top and a line ending above it hits nothing', toolName: 'inspect', arguments: { action: 'raycast_world', channel: 'Visibility', traceComplex: false, rays: [{ start: { x: 0, y: 0, z: 400 }, end: { x: 0, y: 0, z: 0 } }, { start: { x: 0, y: 0, z: 400 }, end: { x: 0, y: 0, z: 300 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.hits.0.actorName', equals: ACTOR, label: 'the line lands on the cube' }, { path: 'structuredContent.result.hits.0.location.z', approximately: 170, tolerance: 0.5, label: 'its top is at z 170' }, { path: 'structuredContent.result.hits.1.hit', equals: false, label: 'the short line hits nothing' }] },
  { scenario: 'INFO: raycast_world passes through an ignored actor to the one behind it', toolName: 'inspect', arguments: { action: 'raycast_world', channel: 'Camera', traceComplex: true, ignoreActors: [ACTOR], rays: [{ start: { x: -300, y: 0, z: 120 }, end: { x: 400, y: 0, z: 120 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.hits.0.actorName', equals: DELETE_ACTOR, label: 'the sphere behind the ignored cube' }] },
  { scenario: 'ERROR: raycast_world refuses an unknown channel', toolName: 'inspect', arguments: { action: 'raycast_world', channel: `NoSuchChannel_${ts}`, rays: [{ start: { x: 0, y: 0, z: 400 }, end: { x: 0, y: 0, z: 0 } }] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: get_mesh_details refuses a texture', toolName: 'inspect', arguments: { action: 'get_mesh_details', objectPath: ENGINE_DEFAULT_TEXTURE }, expected: 'error|TYPE_MISMATCH' },
  { scenario: 'INFO: get_component_details', toolName: 'inspect', arguments: { action: 'get_component_details', actorName: ACTOR, componentName: COMPONENT }, expected: 'success' },
  // Property values come back only when asked for: every one a Niagara component declares was most of a 30 KB reply.
  { scenario: 'INFO: get_component_details reads the properties named', toolName: 'inspect', arguments: { action: 'get_component_details', actorName: ACTOR, componentName: COMPONENT, propertyNames: ['Intensity'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.propertyCount', equals: 1, label: 'only the property asked for' }] },
  { scenario: 'INFO: get_component_details detailed reads every property', toolName: 'inspect', arguments: { action: 'get_component_details', actorName: ACTOR, componentName: COMPONENT, detailed: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.totalPropertyCount', label: 'the full property list came back' }] },

  // === PROPERTIES / COMPONENTS ===
  { scenario: 'CONFIG: set_property', toolName: 'inspect', arguments: inspectActor('set_property', { propertyName: 'InitialLifeSpan', value: 0 }), expected: 'success' },
  { scenario: 'INFO: get_property', toolName: 'inspect', arguments: inspectActor('get_property', { propertyName: 'InitialLifeSpan' }), expected: 'success' },
  { scenario: 'CONFIG: set_property with only propertyPath, left clean', toolName: 'inspect', arguments: inspectActor('set_property', { propertyPath: 'InitialLifeSpan', value: 0, markDirty: false }), expected: 'success' },
  { scenario: 'CONFIG: set_property watches a property after the write', toolName: 'inspect', arguments: inspectActor('set_property', { propertyName: 'InitialLifeSpan', value: 0, markDirty: false, watch: { propertyName: 'InitialLifeSpan', durationSeconds: 0.1, intervalSeconds: 0 } }), expected: 'success', assertions: [{ path: 'structuredContent.result.watch.propertyName', equals: 'InitialLifeSpan', label: 'the watch comes back with the reply' }] },
  { scenario: 'CONFIG: set_property watch without propertyName samples the written property', toolName: 'inspect', arguments: inspectActor('set_property', { propertyName: 'InitialLifeSpan', value: 0, markDirty: false, watch: { durationSeconds: 0.1 } }), expected: 'success', assertions: [{ path: 'structuredContent.result.watch.propertyName', equals: 'InitialLifeSpan', label: 'the watch defaults to the property written' }] },
  { scenario: 'CONFIG: set_property objectPaths writes the same value on two actors in one call', toolName: 'inspect', arguments: { action: 'set_property', objectPaths: [ACTOR, DELETE_ACTOR], propertyName: 'InitialLifeSpan', value: 0, markDirty: false }, expected: 'success', assertions: [
    { path: 'structuredContent.result.applied', equals: 2, label: 'both targets took the write' },
    { path: 'structuredContent.result.targets', length: 2, label: 'one row per target' },
  ] },
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
  { scenario: 'CONFIG: set_property properties writes two defaults in one call', toolName: 'inspect', arguments: { action: 'set_property', blueprintPath: BP_PATH, properties: { CdoCount: 5, InitialLifeSpan: 12 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.applied', equals: 2, label: 'both writes applied' }] },
  { scenario: 'VERIFY: get_property on the Blueprint asset path reads its class defaults', toolName: 'inspect', arguments: { action: 'get_property', objectPath: BP_PATH, propertyName: 'CdoCount' }, expected: 'success', assertions: [{ path: 'structuredContent.result.value', equals: 5, label: 'the asset path reaches the default object, not the UBlueprint' }] },
  { scenario: 'INFO: get_property propertyNames reads several defaults in one call', toolName: 'inspect', arguments: { action: 'get_property', blueprintPath: BP_PATH, propertyNames: ['CdoCount', 'InitialLifeSpan', 'NoSuchProperty'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.properties', length: 2, label: 'both real properties answer' },
    { path: 'structuredContent.result.missingProperties.0', includes: 'NoSuchProperty', label: 'the miss is named, not dropped' },
  ] },
  { scenario: 'INFO: get_property objectPaths reads the same property from several objects', toolName: 'inspect', arguments: { action: 'get_property', objectPaths: [ACTOR, BP_PATH, `MCP_NoSuchActor_${ts}`], propertyName: 'InitialLifeSpan' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.targets', length: 3, label: 'one row per object asked' },
    { path: 'structuredContent.result.targets.1.objectPath', equals: BP_PATH, label: 'in the order given' },
    { path: 'structuredContent.result.targets.2.error', exists: true, label: 'an object that cannot be read says why, the others still answer' },
  ] },
  // A map default is a JSON object of key: value and reads back with each value; a set default is a JSON array.
  { scenario: 'Setup: add a map variable with a default', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'CdoStock', variableType: 'Map<Name,Integer>', defaultValue: { Apple: 3, Pear: 5 } }, expected: 'success' },
  { scenario: 'VERIFY: the map default reads back with its values', toolName: 'inspect', arguments: { action: 'get_property', blueprintPath: BP_PATH, propertyName: 'CdoStock' }, expected: 'success', assertions: [{ path: 'structuredContent.result.value.Pear', equals: 5, label: 'each value is read, not 0' }] },
  { scenario: 'ERROR: set_property refuses a map entry whose value does not fit', toolName: 'inspect', arguments: { action: 'set_property', blueprintPath: BP_PATH, propertyName: 'CdoStock', value: { Plum: 7, Fig: 'many' } }, expected: "error|'fig'" },
  { scenario: 'VERIFY: the refused map write left the map as it was', toolName: 'inspect', arguments: { action: 'get_property', blueprintPath: BP_PATH, propertyName: 'CdoStock' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.value.Apple', equals: 3, label: 'the old entries are still there' },
    { path: 'structuredContent.result.value.Plum', exists: false, label: 'no entry of the refused write landed' },
  ] },
  { scenario: 'CONFIG: set_property writes a map', toolName: 'inspect', arguments: { action: 'set_property', blueprintPath: BP_PATH, propertyName: 'CdoStock', value: { Plum: 7 } }, expected: 'success', assertions: [
    { path: 'structuredContent.result.value.Plum', equals: 7, label: 'the written entry is read back' },
    { path: 'structuredContent.result.value.Apple', exists: false, label: 'the write replaces the map, not merges into it' },
  ] },
  { scenario: 'Setup: add a set variable with a default', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'CdoTags', variableType: 'Set<Name>', defaultValue: ['Red', 'Blue'] }, expected: 'success' },
  { scenario: 'VERIFY: the set default reads back', toolName: 'inspect', arguments: { action: 'get_property', blueprintPath: BP_PATH, propertyName: 'CdoTags' }, expected: 'success', assertions: [{ path: 'structuredContent.result.value', length: 2, label: 'both elements are in the set' }] },
  { scenario: 'ERROR: set_property refuses a set value that repeats an element', toolName: 'inspect', arguments: { action: 'set_property', blueprintPath: BP_PATH, propertyName: 'CdoTags', value: ['Red', 'Red'] }, expected: 'error|same value' },
  // An array element goes through the importer every property uses, and a refused element leaves the array as it was.
  { scenario: 'Setup: add an enum array variable with a default', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'CdoModes', variableType: 'Array<Enum:/Script/Engine.EMovementMode>', defaultValue: ['MOVE_Walking', 'MOVE_Flying'] }, expected: 'success' },
  { scenario: 'VERIFY: the enum array default reads back by name', toolName: 'inspect', arguments: { action: 'get_property', blueprintPath: BP_PATH, propertyName: 'CdoModes' }, expected: 'success', assertions: [{ path: 'structuredContent.result.value.1', includes: 'Flying', label: 'an enum element is set by its name, not read as 0' }] },
  { scenario: 'ERROR: set_property refuses an array element that does not fit', toolName: 'inspect', arguments: { action: 'set_property', blueprintPath: BP_PATH, propertyName: 'CdoModes', value: ['MOVE_Walking', 'NotAMode'] }, expected: 'error|element 1' },
  { scenario: 'VERIFY: the refused array write left the array as it was', toolName: 'inspect', arguments: { action: 'get_property', blueprintPath: BP_PATH, propertyName: 'CdoModes' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.value', length: 2, label: 'still the two elements of the default' },
    { path: 'structuredContent.result.value.1', includes: 'Flying', label: 'the second element kept its value' },
  ] },
  { scenario: 'ERROR: set_property refuses text that is not a number for a float', toolName: 'inspect', arguments: { action: 'set_property', blueprintPath: BP_PATH, propertyName: 'InitialLifeSpan', value: 'soon' }, expected: 'error|requires a number' },
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
  { scenario: 'INFO: get_editor_focus reads what the user is working on', toolName: 'inspect', arguments: { action: 'get_editor_focus' }, expected: 'success', assertions: [{ path: 'structuredContent.result.shadersCompiling', gte: 0, label: 'every editor-state read carries the shader count' }, { path: 'structuredContent.result.openEditors', label: 'the open editors are listed' }] },
  { scenario: 'INFO: get_scene_stats', toolName: 'inspect', arguments: { action: 'get_scene_stats' }, expected: 'success' },
  { scenario: 'INFO: get_performance_stats', toolName: 'inspect', arguments: { action: 'get_performance_stats' }, expected: 'success' },
  { scenario: 'INFO: get_memory_stats', toolName: 'inspect', arguments: { action: 'get_memory_stats' }, expected: 'success' },
  { scenario: 'INFO: get_editor_settings', toolName: 'inspect', arguments: { action: 'get_editor_settings' }, expected: 'success', assertions: [{ path: 'structuredContent.result.shadersCompiling', gte: 0, label: 'the editor settings read carries the shader count too' }] },
  { scenario: 'INFO: runtime_report', toolName: 'inspect', arguments: { action: 'runtime_report', actorName: ACTOR, componentNames: [COMPONENT], propertyNames: ['ActorLabel'] }, expected: 'success' },
  { scenario: 'INFO: pie_report', toolName: 'inspect', arguments: { action: 'pie_report', filter: ACTOR }, expected: 'success' },
  { scenario: 'INFO: a bare runtime_report describes no actors', toolName: 'inspect', arguments: { action: 'runtime_report' }, expected: 'success', assertions: [{ path: 'structuredContent.result.message', includes: 'pass filter', label: 'it says how to list actors' }] },

  // === DESTRUCTIVE / CLEANUP ===
  { scenario: 'DELETE: delete_object', toolName: 'inspect', arguments: { action: 'delete_object', actorNames: [DELETE_ACTOR] }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete inspect actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete inspect blueprint', toolName: 'manage_asset', arguments: { action: 'delete_asset', assetPath: BP_PATH, force: true }, expected: 'success|not found' },
];

runToolTests('inspect', testCases, { folder: TEST_FOLDER });
