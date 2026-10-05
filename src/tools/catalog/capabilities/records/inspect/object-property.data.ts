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

/**
 * What a static or skeletal mesh asset adds
 * (Private/Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectAssetMesh.cpp):
 * the slot table, with what LOD0 draws in each slot.
 */
const MESH_DETAILS_OUTPUT = {
  ...INSPECT_OBJECT_OUTPUT,
  materialSlots: {
    type: 'array',
    items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    description: 'Mesh assets: every material slot as {slotIndex, slotName, material (asset path, empty for none), triangles and sections (what LOD0 draws with the slot), bounds (where that geometry is, in mesh space: origin is its center, extent its half size, with min, max, size and radius)}. bounds is absent for a slot LOD0 draws nothing with, and for every slot when slotBoundsAvailable is false. A Nanite mesh reports its fallback mesh. A skeletal mesh is read in its reference pose.',
  },
  materialSlotCount: { type: 'number', description: 'Mesh assets: how many material slots the mesh has.' },
  slotBoundsAvailable: { type: 'boolean', description: 'Mesh assets: false when the render data keeps no CPU copy of its vertices, so each slot has its triangle count but no bounds.' },
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
    summary: 'Inspect a static or skeletal mesh asset: size, bounds, pivot, LODs, and every material slot with the triangles and bounds of the geometry it covers. Any other object is refused with TYPE_MISMATCH.',
    topics: ['material slot bounds', 'which slot is which part'],
    whenToUse: ['A mesh asset\'s details must be read.', 'It is not known which material slot holds which part of a mesh (legs, hull, claws): each slot lists the triangles and mesh-space bounds of its LOD0 geometry.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, detailed: P.detailed, propertyNames: P.propertyNames },
    required: [],
    outputProps: MESH_DETAILS_OUTPUT,
    effect: 'read',
    exampleInput: { action: 'get_mesh_details', objectPath: '/Game/Meshes/SM_Cube' },
    exampleOutput: {
      success: true, message: 'Object inspected', objectName: 'SM_Cube', isStaticMesh: true, materialSlotCount: 1, slotBoundsAvailable: true,
      materialSlots: [{ slotIndex: 0, slotName: 'Cube', material: '/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial', triangles: 12, sections: 1, bounds: { origin: { x: 0, y: 0, z: 50 }, extent: { x: 50, y: 50, z: 50 } } }],
    },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'raycast_mesh', dispatchAction: 'raycast_mesh', domain: D, family: 'object',
    summary: 'Cast rays at a static mesh asset in its own local space and read where each one meets the surface: the point, the surface normal and the material slot. Places a part, a print or a decal on a curved surface without a placed actor or collision.',
    topics: ['raycast mesh', 'surface point', 'where is the surface', 'line trace mesh', 'surface normal'],
    whenToUse: ['A part, decal or print must sit exactly on a mesh surface (a chest, a hull, a rock) and the surface position at a given height or side is unknown; the decalRotation and location of a hit place a DecalComponent there directly.', 'A surface profile must be sampled: one ray per row or column across the area.'],
    whenNotToUse: ['Only the overall size or pivot of the mesh is needed (use get_mesh_details).', 'Parts of a Blueprint must be checked for sinking into each other (use control_actor.audit_placement).'],
    inputProps: {
      meshPath: { type: 'string', description: 'Static mesh asset path, e.g. /Game/Meshes/SM_Rock. Rays meet its source triangles, so a Nanite mesh answers at full detail.' },
      rays: {
        type: 'array', minItems: 1, maxItems: 256,
        items: {
          type: 'object', additionalProperties: false, required: ['origin', 'direction'],
          properties: {
            origin: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Ray start {x, y, z} in the mesh\'s local space, in cm; start outside the mesh to find its outer surface.' },
            direction: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Ray direction {x, y, z}; any length, it is normalized.' },
          },
        },
        description: 'Rays in the mesh\'s local space; each answers in hits at the same index.',
      },
    },
    required: ['meshPath', 'rays'],
    effect: 'read',
    exampleInput: { action: 'raycast_mesh', meshPath: '/Engine/BasicShapes/Cube', rays: [{ origin: { x: 200, y: 0, z: 50 }, direction: { x: -1, y: 0, z: 0 } }] },
    exampleOutput: { success: true, meshPath: '/Engine/BasicShapes/Cube', hitCount: 1, hits: [{ hit: true, location: { x: 50, y: 0, z: 50 }, normal: { x: 1, y: 0, z: 0 }, distance: 150, materialSlot: 'Cube' }] },
    outputProps: {
      meshPath: { type: 'string', description: 'The mesh the rays were cast at.' },
      hitCount: { type: 'number', description: 'How many rays met the surface.' },
      hits: {
        type: 'array',
        items: {
          type: 'object', additionalProperties: false,
          properties: {
            hit: { type: 'boolean', description: 'Whether this ray met the surface.' },
            location: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Nearest point where the ray meets the surface, {x, y, z} in local space.' },
            normal: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Surface normal there, turned to face back along the ray (from outside, the outward normal).' },
            distance: { type: 'number', description: 'Distance from the ray origin, in cm.' },
            materialSlot: { type: 'string', description: 'Name of the material slot whose triangle was hit.' },
            decalRotation: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Rotation {pitch, yaw, roll} for a DecalComponent placed at location in the same space, so its texture prints upright and readable on this surface: the decal projects into the surface along its X axis, and its DecalSize is (projection depth, half the print height, half the print width).' },
          },
        },
        description: 'One entry per ray, in order.',
      },
    },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'raycast_world', dispatchAction: 'raycast_world', domain: D, family: 'object',
    summary: 'Trace lines through the level against collision and read what each one hits first: the point, the surface normal, the distance, the actor and the component. Finds the ground under a point, checks a line of sight, and proves a wall, floor or volume really blocks. Traces the running game while Play In Editor runs.',
    topics: ['line trace', 'raycast world', 'what is below', 'ground height', 'line of sight', 'does it block', 'collision test', 'trace channel'],
    whenToUse: [
      'The ground height under a point, or what a jump or a drop would land on, must be known.',
      'A line of sight between two points (a camera, an enemy, the player) must be checked.',
      'A placed wall, floor, platform or blocking volume must be proven to block: a mesh or volume with no collision is traced straight through.',
    ],
    whenNotToUse: ['The surface of a mesh asset must be found without placing it (use raycast_mesh).', 'Only an actor\'s overall size is needed (use query_object lookup bounding_box).'],
    inputProps: {
      rays: {
        type: 'array', minItems: 1, maxItems: 256,
        items: {
          type: 'object', additionalProperties: false, required: ['start', 'end'],
          properties: {
            start: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Line start {x, y, z} in world space, in cm.' },
            end: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Line end {x, y, z} in world space; nothing past it is reported.' },
          },
        },
        description: 'Lines in world space; each answers in hits at the same index.',
      },
      channel: { type: 'string', description: 'Collision channel traced: Visibility (default), Camera, WorldStatic, WorldDynamic, Pawn, PhysicsBody, Vehicle, Destructible, or a project trace channel by its name. What blocks depends on each component\'s response to that channel: most meshes block Visibility, a character capsule blocks Pawn.' },
      traceComplex: { type: 'boolean', description: 'Trace the render triangles of meshes that allow it instead of their simple collision (default false: the shapes the game\'s movement collides with).' },
      ignoreActors: { type: 'array', items: { type: 'string' }, description: 'Actors (label, name or path) the lines pass through, such as the player pawn a line starts inside; an unknown name is refused.' },
    },
    required: ['rays'],
    effect: 'read',
    exampleInput: { action: 'raycast_world', rays: [{ start: { x: 0, y: 0, z: 1000 }, end: { x: 0, y: 0, z: -1000 } }] },
    exampleOutput: { success: true, hitCount: 1, hits: [{ hit: true, location: { x: 0, y: 0, z: 0 }, normal: { x: 0, y: 0, z: 1 }, distance: 1000, actorName: 'Floor', componentName: 'StaticMeshComponent0' }] },
    outputProps: {
      hitCount: { type: 'number', description: 'How many lines hit something.' },
      hits: {
        type: 'array',
        items: {
          type: 'object', additionalProperties: false,
          properties: {
            hit: { type: 'boolean', description: 'Whether this line hit something that blocks the channel.' },
            location: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Where the line first hits, {x, y, z} in world space.' },
            normal: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Surface normal there.' },
            distance: { type: 'number', description: 'Distance from start, in cm.' },
            actorName: { type: 'string', description: 'Label of the actor hit.' },
            componentName: { type: 'string', description: 'Component whose collision the line hit.' },
            startedInside: { type: 'boolean', description: 'True when start was already inside the collision it reports: the line begins in a wall, or in a pawn left out of ignoreActors.' },
          },
        },
        description: 'One entry per line, in order.',
      },
    },
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
    summary: 'Read a property value from a world actor, asset, or Blueprint class defaults (blueprintPath, or the Blueprint asset\'s own path), or several at once (propertyNames).',
    whenToUse: ['A single property value must be read.', 'Several properties of one target must be read in one call (propertyNames).'],
    whenNotToUse: ['All properties are needed; use inspect_object or inspect_cdo.'],
    // One requiredOneOf group only: the property name is the one the schema
    // enforces (propertyPath alone used to be refused by a required propertyName);
    // a missing target is refused by the handler, naming the four spellings.
    inputProps: {
      objectPath: P.runtimeObjectPath, actorName: P.actorName, name: P.name, blueprintPath: P.blueprintPath, propertyName: P.propertyName, propertyPath: P.propertyPath,
      propertyNames: { type: 'array', items: { type: 'string' }, minItems: 1, maxItems: 64, description: 'Several properties of the same target in one call, in place of propertyName: each read as a single call would (dotted paths too, such as CharacterMovement.MaxWalkSpeed). Answered under properties; a name that does not resolve is listed under missingProperties with the reason, and the others still answer.' },
    },
    required: [],
    requiredOneOf: ['propertyName', 'propertyPath', 'propertyNames'],
    effect: 'read',
    exampleInput: { action: 'get_property', objectPath: '/Game/Maps/Demo.Demo_PersistentLevel.PlayerStart_1', propertyName: 'ActorLabel' },
    exampleOutput: { success: true, message: 'Property read', value: 'PlayerStart_1' },
    outputProps: {
      value: P.valueRead,
      properties: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, 'x-unreal-reflection-boundary': true, description: 'With propertyNames: each property that resolved, as {propertyName (its own spelling), value}, plus propertyPath (the path as asked) when that differs, so two .Text reads stay apart.' },
      missingProperties: { type: 'array', items: { type: 'string' }, description: 'With propertyNames: each name that did not resolve, as "Name: reason".' },
    },
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'set_property', dispatchAction: 'set_property', domain: D, family: 'property',
    topics: ['write property', 'set value', 'change property', 'modify property', 'edit property', 'set field', 'set property on actor', 'set game instance variable'],
    summary: 'Write a property value on a world actor, asset, or Blueprint CDO, or several at once (properties).',
    whenToUse: ['A single property value must be written.', 'Several properties of one target must be written in one call (properties).'],
    whenNotToUse: ['The property is read-only or the target is a packed asset.'],
    inputProps: {
      objectPath: P.runtimeObjectPath, actorName: P.actorName, name: P.name, blueprintPath: P.blueprintPath, propertyName: P.propertyName, propertyPath: P.propertyPath, value: P.value, markDirty: P.markDirty,
      properties: {
        type: 'object',
        additionalProperties: true,
        'x-unreal-reflection-boundary': true,
        description: 'Several writes on the same target in place of propertyName and value: {name or dotted path: value}, e.g. {"BoxExtent": {"X": 20, "Y": 90, "Z": 100}, "CollisionProfileName": "OverlapAllDynamic"}. Each is written as a single call would be and reported under properties; the call fails naming any that did not apply. watch applies to single writes only.',
      },
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
    requiredOneOf: ['propertyName', 'propertyPath', 'properties'],
    effect: 'write', costLatency: 'interactive',
    outputProps: {
      watch: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'With watch: objectPath, propertyName, samples ({t, value}, kept when the value changed), sampleCount, changed (it took more than one value) and frames.' },
      properties: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, 'x-unreal-reflection-boundary': true, description: 'With properties: each write as {propertyName, applied, value read back}.' },
      applied: { type: 'number', description: 'With properties: how many writes applied.' },
      instancesUpdated: { type: 'number', description: 'A write to a class default or component template: how many placed copies that still held the old default took the new one, as the details panel does; copies that override the value keep theirs. Absent for any other target.' },
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
