/**
 * Object, property, and class introspection records (12 actions).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const D = 'inspect';

/**
 * What HandleInspectObjectAction actually returns
 * (Private/Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectObject.cpp).
 *
 * These MUST be declared. McpProjectCanonicalOutput keeps only the properties a
 * record declares, so while this set was missing the native gateway stripped the
 * entire payload and every inspect_object call — plus its four aliases — answered
 * `{success, message}` and nothing else, for a capability whose whole purpose is
 * "returning detailed properties".
 */
const INSPECT_OBJECT_OUTPUT = {
  objectName: { type: 'string', description: 'Object name.' },
  objectPath: { type: 'string', description: 'Full object path of the inspected object.' },
  class: { type: 'string', description: 'Class name of the inspected object.' },
  className: { type: 'string', description: 'Class name of the inspected object (alias of class).' },
  classPath: { type: 'string', description: 'Full /Script class path of the inspected object.' },
  actorLabel: { type: 'string', description: 'Editor display label. Actors only.' },
  isActor: { type: 'boolean', description: 'True when the object is a world actor rather than an asset.' },
  isStaticMesh: { type: 'boolean', description: 'True when the object is a StaticMesh asset.' },
  isSceneComponent: { type: 'boolean', description: 'True when the object is a scene component.' },
  isSelected: { type: 'boolean', description: 'True when the actor is selected in the editor.' },
  isHidden: { type: 'boolean', description: 'True when the actor is hidden in the editor viewport.' },
  isVisible: { type: 'boolean', description: 'True when the object is visible.' },
  isActive: { type: 'boolean', description: 'True when the component is active.' },
  staticMesh: { type: 'string', description: 'Static mesh asset path assigned to the object, when it has one.' },
  location: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'World location as {x, y, z}.' },
  rotation: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'World rotation as {pitch, yaw, roll} in degrees.' },
  scale: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'World scale as {x, y, z}.' },
  transform: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Combined transform: location, rotation and scale.' },
  components: { type: 'array', description: 'Attached components with their names, classes and transforms.' },
  componentCount: { type: 'number', description: 'Number of attached components.' },
  tags: { type: 'array', items: { type: 'string' }, description: 'Actor tags.' },
};

export const OBJECT_PROPERTY_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'inspect_object', dispatchAction: 'inspect_object', domain: D, family: 'object',
    topics: ['inspect actor', 'object details', 'actor properties', 'dump object', 'all properties', 'introspect object'],
    aliases: ['inspect.inspect_actor'],
    summary: 'Inspect a world actor or asset object by path, returning detailed properties.',
    whenToUse: ['An object\'s properties and structure must be read.'],
    whenNotToUse: ['A Blueprint CDO without a spawned actor is needed; use inspect_cdo.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, componentName: P.componentName, detailed: P.detailed, propertyNames: P.propertyNames },
    required: [],
    outputProps: INSPECT_OBJECT_OUTPUT,
    effect: 'read',
    exampleInput: { action: 'inspect_object', objectPath: '/Game/Maps/Demo.Demo_PersistentLevel.PlayerStart_1' },
    exampleOutput: { success: true, message: 'Object inspected', objectName: 'PlayerStart_1', class: 'PlayerStart', isActor: true, location: { x: 0, y: 0, z: 100 } },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_actor_details', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a world actor (alias of inspect_object).',
    whenToUse: ['A world actor\'s details must be read using the get_actor_details verb.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, componentName: P.componentName, detailed: P.detailed, propertyNames: P.propertyNames },
    required: [],
    outputProps: INSPECT_OBJECT_OUTPUT,
    effect: 'read',
    exampleInput: { action: 'get_actor_details', actorName: 'PlayerStart_1' },
    exampleOutput: { success: true, message: 'Object inspected', objectName: 'PlayerStart_1', isActor: true },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_blueprint_details', dispatchAction: 'blueprint_get', domain: D, family: 'object',
    topics: ['compile status', 'parent class', 'graph node counts'],
    summary: 'Read a Blueprint asset without spawning it: parent class, type, compile status, variables with defaults, functions, events, graph node counts, and components with transforms, meshes and materials.',
    whenToUse: ['A Blueprint asset\'s structure must be read without spawning an actor.'],
    whenNotToUse: ['A world actor is in scope; use inspect_object.'],
    inputProps: { objectPath: P.objectPath, blueprintPath: P.blueprintPath },
    required: [],
    effect: 'read',
    exampleInput: { action: 'get_blueprint_details', blueprintPath: '/Game/Blueprints/BP_Test' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_mesh_details', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a static or skeletal mesh asset: size, bounds, pivot, LODs and material slots. Any other object is refused with TYPE_MISMATCH.',
    whenToUse: ['A mesh asset\'s details must be read.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, detailed: P.detailed, propertyNames: P.propertyNames },
    required: [],
    outputProps: INSPECT_OBJECT_OUTPUT,
    effect: 'read',
    exampleInput: { action: 'get_mesh_details', objectPath: '/Game/Meshes/SM_Cube' },
    exampleOutput: { success: true, message: 'Object inspected', objectName: 'SM_Cube', isStaticMesh: true },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_texture_details', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a texture asset: dimensions, format and settings. Any other object is refused with TYPE_MISMATCH.',
    whenToUse: ['A texture asset\'s details must be read.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, detailed: P.detailed, propertyNames: P.propertyNames },
    required: [],
    outputProps: INSPECT_OBJECT_OUTPUT,
    effect: 'read',
    exampleInput: { action: 'get_texture_details', objectPath: '/Game/Textures/T_Base' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_material_details', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a material or material instance: blend mode, parameters and textures. Any other object is refused with TYPE_MISMATCH.',
    whenToUse: ['A material asset\'s details must be read.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, detailed: P.detailed, propertyNames: P.propertyNames },
    required: [],
    outputProps: INSPECT_OBJECT_OUTPUT,
    effect: 'read',
    exampleInput: { action: 'get_material_details', objectPath: '/Game/Materials/M_Base' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_level_details', dispatchAction: 'get_world_settings', domain: D, family: 'object',
    summary: 'Inspect the current level/world summary (TS normalizes to get_world_settings).',
    whenToUse: ['The current level\'s world settings summary must be read.'],
    whenNotToUse: ['A specific actor is in scope; use inspect_object.'],
    inputProps: {},
    required: [],
    effect: 'read',
    exampleInput: { action: 'get_level_details' },
    exampleOutput: { success: true, message: 'World settings', worldName: 'Demo' },
    outputProps: { worldName: { type: 'string', description: 'Current world name.' } },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_property', dispatchAction: 'get_property', domain: D, family: 'property',
    topics: ['read property', 'property value', 'get value', 'read field', 'actor property', 'object property', 'game instance variable', 'read live widget property', 'widget value during play'],
    summary: 'Read a property value from a world actor, asset, or Blueprint CDO.',
    whenToUse: ['A single property value must be read.'],
    whenNotToUse: ['All properties are needed; use inspect_object or inspect_cdo.'],
    // One requiredOneOf group only: the property name is the one the schema
    // enforces (propertyPath alone used to be refused by a required propertyName);
    // a missing target is refused by the handler, naming the four spellings.
    inputProps: { objectPath: P.runtimeObjectPath, actorName: P.actorName, name: P.name, blueprintPath: P.blueprintPath, propertyName: P.propertyName, propertyPath: P.propertyPath },
    required: [],
    requiredOneOf: ['propertyName', 'propertyPath'],
    effect: 'read',
    exampleInput: { action: 'get_property', objectPath: '/Game/Maps/Demo.Demo_PersistentLevel.PlayerStart_1', propertyName: 'ActorLabel' },
    exampleOutput: { success: true, message: 'Property read', value: 'PlayerStart_1' },
    outputProps: { value: P.value },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'set_property', dispatchAction: 'set_property', domain: D, family: 'property',
    topics: ['write property', 'set value', 'change property', 'modify property', 'edit property', 'set field', 'set property on actor', 'set game instance variable'],
    summary: 'Write a property value on a world actor, asset, or Blueprint CDO.',
    whenToUse: ['A single property value must be written.'],
    whenNotToUse: ['The property is read-only or the target is a packed asset.'],
    inputProps: {
      objectPath: P.runtimeObjectPath, actorName: P.actorName, name: P.name, blueprintPath: P.blueprintPath, propertyName: P.propertyName, propertyPath: P.propertyPath, value: P.value, markDirty: P.markDirty,
      // A UMG pop set off by a write ran on real time and ended between two calls; LivesPop was only
      // provable by slowing the asset 50x.
      watch: {
        type: 'object',
        description: 'Sample a property right after the write, in the same call, to see what the write sets off before it ends: a HUD pop or fade runs on real time, even with PIE paused, and is over before a later call can read it. The reply lists each value the property took with its time t in seconds (Lives on GameInstance with watch {objectPath: "WBP_HUD", propertyName: "LivesBox.RenderTransform"}).',
        properties: {
          propertyName: { type: 'string', description: 'The property to sample; a dotted path reaches through widgets and structs (LivesBox.RenderTransform).' },
          objectPath: { type: 'string', description: 'The object to sample, as objectPath takes it (GameInstance, a live widget such as WBP_HUD); omitted, the object written.' },
          durationSeconds: { type: 'number', description: 'Real seconds to sample (default 0.5, at most 20).' },
          intervalSeconds: { type: 'number', description: 'Seconds between samples (default 0: every frame); a sample is kept only when the value changed.' },
        },
        required: ['propertyName'],
        additionalProperties: false,
      },
    },
    required: [],
    requiredOneOf: ['propertyName', 'propertyPath'],
    effect: 'write', costLatency: 'interactive',
    outputProps: {
      watch: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'With watch: objectPath, propertyName, samples ({t, value}, kept when the value changed), sampleCount, changed (it took more than one value) and frames.' },
      // Writing a Blueprint CDO only reaches instances spawned later once the
      // class is rebuilt, so the caller is told whether that recompile happened.
      blueprintCompiled: { type: 'boolean', description: 'True when the target was a Blueprint CDO and the Blueprint was recompiled, so the value now applies to newly spawned instances. The reply (value, actorPath, actorClass) is read back from the recompiled class default object and the Blueprint package is saved; a variable the Blueprint declares keeps the value as its default, and the call fails with PROPERTY_SET_FAILED, never success, when the compile did not keep it. A Default__ objectPath resolves to the Blueprint\'s current default object; a copy a compile left behind (a REINST_ class, or a Blueprint object in /Engine/Transient with no game running) fails with STALE_TARGET. False for plain world actors and assets, where no compile is involved.' },
      // A material expression's write reaches material instances only through the rebuilt material.
      materialRebuilt: { type: 'boolean', description: 'True when the target lives inside a material or material function (a material expression such as a TextureObjectParameter, as <material>.<material>:<nodeName>) and that material was rebuilt, so its parameter lists, which every material instance reads, show the write at once (a new ParameterName, a new default Texture). Absent for any other target.' },
    },
    exampleInput: { action: 'set_property', objectPath: '/Game/Maps/Demo.Demo_PersistentLevel.PlayerStart_1', propertyName: 'ActorLabel', value: 'Spawn_01' },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'inspect_class', dispatchAction: 'inspect_class', domain: D, family: 'class',
    topics: ['class info', 'class metadata', 'reflection', 'class properties', 'uclass'],
    summary: 'Inspect a UClass: metadata, parent, default CDO properties.',
    whenToUse: ['A class\'s hierarchy and defaults must be read.'],
    whenNotToUse: ['A specific instance is in scope; use inspect_object.'],
    inputProps: { className: P.className, classPath: P.classPath },
    required: ['className'],
    effect: 'read',
    exampleInput: { action: 'inspect_class', className: 'PointLight' },
    exampleOutput: { success: true, message: 'Class inspected', className: 'PointLight', classPath: '/Script/Engine.PointLight', parentClass: 'Light' },
    outputProps: {
      className: P.className,
      classPath: { type: 'string', description: 'Full /Script path of the resolved class.' },
      parentClass: { type: 'string', description: 'Immediate super-class name ("None" when the class has no super).' },
      // The handler always sent these; undeclared, they were projected away, and once folded beside
      // inspect_cdo the array met its object-typed `properties` and every class inspect failed.
      parentClassPath: { type: 'string', description: 'Full path of the super-class.' },
      module: { type: 'string', description: 'Module that declares a C++ class (empty for a Blueprint class).' },
      isNative: { type: 'boolean', description: 'True for a C++ class.' },
      isBlueprintGenerated: { type: 'boolean', description: 'True for a Blueprint-generated class.' },
      generatedBy: { type: 'string', description: 'Blueprint asset that generated the class, when it is Blueprint-generated.' },
      ancestors: { type: 'array', items: { type: 'string' }, description: 'Super-classes from the parent up to Object.' },
      flags: { type: 'object', properties: { abstract: { type: 'boolean' }, blueprintable: { type: 'boolean' }, blueprintType: { type: 'boolean' }, deprecated: { type: 'boolean' }, transient: { type: 'boolean' }, config: { type: 'boolean' }, interface: { type: 'boolean' }, isActor: { type: 'boolean' }, isComponent: { type: 'boolean' } }, additionalProperties: false, description: 'Class flags as booleans.' },
      properties: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'kind=class: every property the class declares or inherits (name, type, category, declaredIn, editable, blueprintVisible, deprecated). kind=cdo: the requested property values by name.' },
      propertyCount: { type: 'number', description: 'Properties the class has, including any past the 200 listed.' },
      propertiesTruncated: { type: 'boolean', description: 'True when the class has more than the 200 properties listed.' },
      defaultProperties: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Default value of each property on the class default object, as text.' },
      defaultObjectPath: { type: 'string', description: 'Path of the class default object.' },
      functionCount: { type: 'number', description: 'Functions the class declares or inherits.' },
    },
    outputRequired: [],
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'inspect_cdo', dispatchAction: 'inspect_cdo', domain: D, family: 'class',
    summary: 'Inspect a Blueprint Class Default Object (CDO) and its default components without spawning an actor.',
    whenToUse: ['A Blueprint\'s default properties and components must be read.'],
    whenNotToUse: ['A spawned world actor is in scope; use inspect_object.'],
    inputProps: {
      blueprintPath: P.blueprintPath, componentName: P.componentName, propertyNames: P.propertyNames, detailed: P.detailed,
      componentNames: { type: 'array', items: { type: 'string' }, description: 'Return only these components in components, by name (case-insensitive); a name that matches none is listed under missingComponents.' },
    },
    required: ['blueprintPath'],
    effect: 'read',
    // The handler (McpAutomationBridge_PropertyHandlersCdoInspection.cpp) emits
    // every field below. With NO outputProps declared, output projection kept
    // only {success, message} and the capability answered "CDO inspection
    // completed" carrying nothing — leaving Blueprint defaults unreadable.
    outputProps: {
      className: { type: 'string', description: 'Generated class name of the inspected CDO.' },
      classPath: { type: 'string', description: 'Full path of the generated class.' },
      blueprintPath: P.blueprintPath,
      parentClass: { type: 'string', description: 'Parent class name.' },
      componentCount: { type: 'number', description: 'Number of default components on the CDO.' },
      cdoProperties: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Default property values on the Class Default Object.' },
      properties: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Requested property values.' },
      components: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Default component descriptors (name, class, attachParent).' },
      missingComponents: { type: 'array', items: { type: 'string' }, description: 'componentNames that matched no component.' },
    },
    outputRequired: [],
    exampleInput: { action: 'inspect_cdo', blueprintPath: '/Game/Blueprints/BP_Test' },
    exampleOutput: { success: true, message: 'CDO inspected', className: 'BP_Test_C', componentCount: 3 },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'inspect_struct', dispatchAction: 'inspect_struct', domain: D, family: 'class',
    summary: 'Inspect a UserDefinedStruct layout (member names, types, defaults) read-only.',
    whenToUse: ['A Blueprint Struct\'s member layout must be read.'],
    whenNotToUse: ['Struct values must be read or written; use manage_asset struct actions.'],
    inputProps: { structPath: P.structPath },
    required: ['structPath'],
    effect: 'read',
    // The native handler (McpAutomationBridge_InspectStruct.cpp) returns its
    // findings nested under `result`; with no outputProps declared the output
    // projection stripped the entire nested object and the capability answered
    // bare success — a layout reader that returned nothing.
    exampleInput: { action: 'inspect_struct', structPath: '/Game/Structs/S_Test' },
    exampleOutput: { success: true, message: 'Struct inspected', result: { structName: 'S_Test', memberCount: 3 } },
    outputProps: {
      result: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Struct layout: structName, structPath, parentStruct(Path), isRowStruct, isUserDefined, members[] (name/type/default/tooltip/guid/metadata/innerStruct), memberCount.' },
    },
    outputRequired: [],
  }),
];
