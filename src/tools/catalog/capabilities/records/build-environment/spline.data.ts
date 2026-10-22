/**
 * Spline family records (22 actions).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'spline';
const WU = ['A spline actor or spline mesh must be created or modified.'];
// A spline edit targets actorName (label, name or object path) or, when that is
// omitted, actorPath.
const ACTOR_PATH = { ...P.actorPath, description: 'Object path of the spline actor; used when actorName is omitted.' };
// A point is placed relative to the spline actor: an ocean shoreline read as world points was set off by the actor's location.
const SPLINE_POSITION = { ...P.position, description: 'Point position relative to the spline actor {x, y, z}: its local space, the location get_splines_info with actorName reports (worldLocation is where the point lies in the level).' };
// Every template reads the same inputs (a wall and a fence also read height); fence posts go through scatter_meshes_along_spline.
const TEMPLATE = {
  name: { ...P.name, description: 'Label of the new spline actor (default <Kind>_Spline, e.g. Wall_Spline).' },
  location: { ...P.location, description: 'World location of the spline actor {x, y, z} (default 0,0,0); points are relative to it.' },
  meshPath: { ...P.meshPath, description: 'Static mesh drawn along each span, its length along X (default: the engine cube; the engine cylinder for pipe and cable, both in the engine\'s grey checker until materialPath gives them a material). Refused with MESH_NOT_FOUND before anything is created when it does not load.' },
  points: { ...P.points, description: 'The route relative to location, at least 2 points {position, arriveTangent, leaveTangent, rotation, scale}. With fewer, a 4-point demo route: (0,0), (500,0), (1000,200), (1500,200).' },
  materialPath: { ...P.materialPath, description: 'Material for slot 0 of every span (default: the mesh\'s own). The editor flags a material Used with Spline Meshes the first time one draws it, leaving the material unsaved: save it with the level, or a packaged game draws these spans in the checker. Refused with MATERIAL_NOT_FOUND before anything is created when it does not load.' },
  width: { ...P.width, description: 'Size across the route in world units, over 0: a road or river slab\'s width (default 400), a wall\'s or fence\'s thickness (default 20 / 10), a pipe\'s or cable\'s diameter (default 50 / 4). With meshPath, omit it to keep the mesh\'s own width.' },
  closedLoop: { ...P.closedLoop, description: 'Join the last point back to the first, adding one span (default false).' },
};
const STANDING_TEMPLATE = {
  ...TEMPLATE,
  height: { type: 'number', description: 'Height in world units, over 0 (default 300 for a wall, 100 for a fence); the mesh\'s bottom stands on the route. With meshPath, omit it to keep the mesh\'s own height.' },
};
const num = (description: string) => ({ type: 'number', description });
const TEMPLATE_OUT = {
  actorName: { type: 'string', description: 'Label of the new spline actor.' },
  pointCount: num('Points on the route.'),
  splineLength: num('Length of the route in world units.'),
  meshSegments: num('Mesh spans drawn: one per pair of neighbouring points, one more for a closed loop. Each collides (BlockAll).'),
  meshPath: { type: 'string', description: 'Object path of the mesh drawn along the route.' },
  width: num('Size across the route in world units, as drawn.'),
  height: num('Size up from the route in world units, as drawn (the diameter for a pipe or cable).'),
};
const TEMPLATE_WHEN = (what: string): string[] => [`${what} must follow a route through the level: one call lays the spline and the mesh along it, with collision.`];

export const SPLINE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_spline_actor', action: 'create_spline_actor', family: F,
    summary: 'Place a spline actor labelled name (default SplineActor) at location and rotation, with points (each position in the spline actor\'s local space (subtract its location from a world point)) and bClosedLoop to join the last point to the first. splineType sets every point: Linear, Curve (default), Constant, CurveClamped or CurveCustomTangent; another name is refused and nothing is placed.',
    whenToUse: WU, whenNotToUse: ['A static mesh path should be used instead.'],
    inputProps: { initialPoints: P.initialPoints, name: P.name, location: P.location, rotation: P.rotation, points: P.points, bClosedLoop: P.bClosedLoop,
      splineType: { ...P.splineType, description: 'Point type for every point: Linear, Curve (default), Constant, CurveClamped or CurveCustomTangent.' } },
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_spline_actor', name: 'Spline_1', location: { x: 0, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.add_spline_point', action: 'add_spline_point', family: F,
    summary: 'Add a point at position, in the spline actor\'s local space (subtract its location from a world point), to an actor\'s first spline component: appended, or inserted at index. pointType (default Curve) and arriveTangent and leaveTangent shape it; an unknown point type or a missing position is refused. One undo step.',
    whenToUse: WU, whenNotToUse: ['The spline should be recreated with all points.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, position: SPLINE_POSITION, index: P.index,
      arriveTangent: P.arriveTangent, leaveTangent: P.leaveTangent, pointType: P.pointType },
    required: ['position'], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'add_spline_point', actorName: 'Spline_1', position: { x: 100, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.remove_spline_point', action: 'remove_spline_point', family: F,
    summary: 'Remove the point at pointIndex from an actor\'s first spline component, in one undo step; the points after it move down one index. Not saved.',
    whenToUse: WU, whenNotToUse: ['The spline should be recreated without the point.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex },
    required: ['pointIndex'], requiredOneOf: ['actorName', 'actorPath'], effect: 'destructive', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'remove_spline_point', actorName: 'Spline_1', pointIndex: 2 },
  }),
  buildRecord({
    id: 'build_environment.set_spline_point_position', action: 'set_spline_point_position', family: F,
    summary: 'Move the point at pointIndex of an actor\'s first spline component to position, in the spline actor\'s local space (subtract its location from a world point). Both are required; an index past the last point is INVALID_INDEX. One undo step.',
    whenToUse: WU, whenNotToUse: ['The point should be moved interactively.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex, position: SPLINE_POSITION },
    required: ['pointIndex', 'position'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_point_position', actorName: 'Spline_1', pointIndex: 0, position: { x: 0, y: 0, z: 100 } },
  }),
  buildRecord({
    id: 'build_environment.set_spline_point_tangents', action: 'set_spline_point_tangents', family: F,
    summary: 'Set the arrive and leave tangents of the point at pointIndex, in the spline actor\'s local space (subtract its location from a world point): a lone arriveTangent sets both, a lone leaveTangent keeps the arrive one. One undo step.',
    whenToUse: WU, whenNotToUse: ['Default tangents are sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex,
      arriveTangent: P.arriveTangent, leaveTangent: P.leaveTangent },
    required: ['pointIndex'], requiredOneOf: ['arriveTangent', 'leaveTangent'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_point_tangents', actorName: 'Spline_1', pointIndex: 0, arriveTangent: { x: 50, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.set_spline_point_rotation', action: 'set_spline_point_rotation', family: F,
    summary: 'Set the rotation of the point at pointIndex to pointRotation, in the spline actor\'s local space; both are required. One undo step.',
    whenToUse: WU, whenNotToUse: ['Default rotation is sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex, pointRotation: P.pointRotation },
    required: ['pointIndex', 'pointRotation'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_point_rotation', actorName: 'Spline_1', pointIndex: 0, pointRotation: { pitch: 0, yaw: 45, roll: 0 } },
  }),
  buildRecord({
    id: 'build_environment.set_spline_point_scale', action: 'set_spline_point_scale', family: F,
    summary: 'Set the scale (pointScale) of the point at pointIndex, which a spline mesh along it follows; both are required. One undo step.',
    whenToUse: WU, whenNotToUse: ['Default scale is sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex, pointScale: P.pointScale },
    required: ['pointIndex', 'pointScale'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_point_scale', actorName: 'Spline_1', pointIndex: 0, pointScale: { x: 1, y: 1, z: 1 } },
  }),
  buildRecord({
    id: 'build_environment.set_spline_type', action: 'set_spline_type', family: F,
    summary: 'Set the type of the point at pointIndex, or of every point when pointIndex is left out: Linear, Curve (default), Constant, CurveClamped or CurveCustomTangent; another name is refused. The reply names the type set and pointsAffected. One undo step.',
    whenToUse: WU, whenNotToUse: ['Default curve type is sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex, splineType: P.splineType },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_type', actorName: 'Spline_1', pointIndex: 0, splineType: 'Curve' },
  }),
  buildRecord({
    id: 'build_environment.create_spline_mesh_component', action: 'create_spline_mesh_component', family: F,
    summary: 'Create a spline mesh component on an actor.',
    whenToUse: WU, whenNotToUse: ['A static mesh should be used instead.'],
    inputProps: { blueprintPath: P.blueprintPath, save: P.save, actorName: P.actorName, actorPath: ACTOR_PATH, componentName: P.componentName, meshPath: P.meshPath, forwardAxis: P.forwardAxis },
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_spline_mesh_component', actorName: 'Spline_1', meshPath: '/Game/Meshes/SM_Road' },
  }),
  buildRecord({
    id: 'build_environment.set_spline_mesh_asset', action: 'set_spline_mesh_asset', family: F,
    summary: 'Set the static mesh asset for a spline mesh component.',
    whenToUse: WU, whenNotToUse: ['The mesh should be recreated.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, componentName: P.componentName, meshPath: P.meshPath },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_mesh_asset', actorName: 'Spline_1', meshPath: '/Game/Meshes/SM_Road_v2' },
  }),
  buildRecord({
    id: 'build_environment.configure_spline_mesh_axis', action: 'configure_spline_mesh_axis', family: F,
    summary: 'Set which local axis of its mesh a spline mesh component lays along the spline (forwardAxis X, the default, Y or Z, any case; another name is refused before anything changes). The reply gives the axis the component holds; the level is left unsaved.',
    whenToUse: WU, whenNotToUse: ['Default axis is sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, componentName: P.componentName, forwardAxis: P.forwardAxis },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_spline_mesh_axis', actorName: 'Spline_1', forwardAxis: 'X' },
  }),
  buildRecord({
    id: 'build_environment.set_spline_mesh_material', action: 'set_spline_mesh_material', family: F,
    summary: 'Set the material on a spline mesh component.',
    whenToUse: WU, whenNotToUse: ['Default material is sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, componentName: P.componentName, materialPath: P.materialPath, materialIndex: P.materialIndex },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_mesh_material', actorName: 'Spline_1', materialPath: '/Game/Materials/M_Road' },
  }),
  buildRecord({
    id: 'build_environment.scatter_meshes_along_spline', action: 'scatter_meshes_along_spline', family: F,
    summary: 'Scatter static meshes along a spline path.',
    whenToUse: WU, whenNotToUse: ['Manual placement is sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, meshPath: P.meshPath,
      spacing: P.spacing, alignToSpline: P.alignToSpline, randomizeRotation: P.randomizeRotation, randomizeScale: P.randomizeScale,
      useRandomOffset: P.useRandomOffset, randomOffsetRange: P.randomOffsetRange, minScale: P.minScale, maxScale: P.maxScale, rotationRange: P.rotationRange },
    required: [], effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'scatter_meshes_along_spline', actorName: 'Spline_1', meshPath: '/Game/Meshes/SM_Tree', spacing: 200 },
  }),
  buildRecord({
    id: 'build_environment.configure_mesh_spacing', action: 'configure_mesh_spacing', family: F,
    summary: 'Store the spacing scatter_meshes_along_spline uses for a spline actor (actorName; with none, for every spline in the level): spacing in cm between meshes (default 100), useRandomOffset and randomOffsetRange (cm). Nothing is placed until scatter runs, and a value sent to scatter wins over the stored one. Only the fields sent are stored, a call with none is refused, and the reply gives what scatter will read.',
    whenToUse: WU, whenNotToUse: ['Default spacing is sufficient.'],
    inputProps: { useRandomOffset: P.useRandomOffset, randomOffsetRange: P.randomOffsetRange, actorName: P.actorName, actorPath: ACTOR_PATH, spacing: P.spacing },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_mesh_spacing', actorName: 'Spline_1', spacing: 150 },
  }),
  buildRecord({
    id: 'build_environment.configure_mesh_randomization', action: 'configure_mesh_randomization', family: F,
    summary: 'Store the randomization scatter_meshes_along_spline uses for a spline actor (or, with no actorName, for every spline in the level): randomizeScale between minScale and maxScale (defaults 0.8 and 1.2), randomizeRotation within rotationRange degrees (default 360). Nothing is placed until scatter runs; only the fields sent are stored (a scale sent alone is checked against the stored other), a call with none is refused, and the reply gives what scatter will read.',
    whenToUse: WU, whenNotToUse: ['Uniform placement is desired.'],
    inputProps: { rotationRange: P.rotationRange, actorName: P.actorName, actorPath: ACTOR_PATH,
      randomizeRotation: P.randomizeRotation, randomizeScale: P.randomizeScale, minScale: P.minScale, maxScale: P.maxScale },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_mesh_randomization', actorName: 'Spline_1', randomizeRotation: true },
  }),
  buildRecord({
    id: 'build_environment.create_road_spline', action: 'create_road_spline', family: F,
    summary: 'Lay a road along a route: a slab drawn along the spline, 400 wide and 8 thick by default (the engine cube), centred on the route; meshPath swaps in a road mesh, fitted to width. Every span collides, so vehicles and characters stay on it.',
    whenToUse: TEMPLATE_WHEN('A road or path'),
    whenNotToUse: ['A bare spline with no mesh is wanted (create_spline kind actor).', 'The road should shape the terrain: landscape splines deform the landscape.'],
    inputProps: TEMPLATE, outputProps: TEMPLATE_OUT,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_road_spline', name: 'Road_1', width: 600, closedLoop: true, points: [{ position: { x: 0, y: 0, z: 0 } }, { position: { x: 2000, y: 0, z: 0 } }, { position: { x: 2000, y: 2000, z: 0 } }, { position: { x: 0, y: 2000, z: 0 } }] },
  }),
  buildRecord({
    id: 'build_environment.create_river_spline', action: 'create_river_spline', family: F,
    summary: 'Lay a river bed along a route: a flat slab drawn along the spline, 400 wide and 8 thick by default, centred on the route; give it a water-looking material with materialPath. It is a mesh: no buoyancy, waves or flow.',
    whenToUse: TEMPLATE_WHEN('A river or channel surface'),
    whenNotToUse: ['Real water (buoyancy, waves, flow) is wanted: a Water plugin river body.'],
    inputProps: TEMPLATE, outputProps: TEMPLATE_OUT,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_river_spline', name: 'River_1', width: 800 },
  }),
  buildRecord({
    id: 'build_environment.create_fence_spline', action: 'create_fence_spline', family: F,
    summary: 'Run a fence along a route: a panel 10 thick and 100 high by default, its bottom standing on the spline; meshPath swaps in a rail or panel mesh, fitted to width and height. Posts: scatter_meshes_along_spline on the new actor.',
    whenToUse: TEMPLATE_WHEN('A fence or railing'),
    whenNotToUse: ['A few fence pieces placed by hand are enough.'],
    inputProps: STANDING_TEMPLATE, outputProps: TEMPLATE_OUT,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_fence_spline', name: 'Fence_1', height: 120 },
  }),
  buildRecord({
    id: 'build_environment.create_wall_spline', action: 'create_wall_spline', family: F,
    summary: 'Build a wall along a route: 20 thick and 300 high by default, its bottom standing on the spline; meshPath swaps in a wall mesh, fitted to width (thickness) and height. Every span collides.',
    whenToUse: TEMPLATE_WHEN('A wall, barrier or enclosure'),
    whenNotToUse: ['A few wall pieces placed by hand are enough.'],
    inputProps: STANDING_TEMPLATE, outputProps: TEMPLATE_OUT,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_wall_spline', name: 'Wall_1', width: 30, height: 250, points: [{ position: { x: 0, y: 0, z: 0 } }, { position: { x: 1500, y: 0, z: 0 } }] },
  }),
  buildRecord({
    id: 'build_environment.create_cable_spline', action: 'create_cable_spline', family: F,
    summary: 'Hang a cable along a route: a round tube 4 across by default (the engine cylinder), centred on the route. It does not sag or swing: shape the sag with points, or use a cable component for physics.',
    whenToUse: TEMPLATE_WHEN('A cable, wire or rope'),
    whenNotToUse: ['The cable must sag and swing with physics: a cable component.'],
    inputProps: TEMPLATE, outputProps: TEMPLATE_OUT,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_cable_spline', name: 'Cable_1', points: [{ position: { x: 0, y: 0, z: 500 } }, { position: { x: 600, y: 0, z: 380 } }, { position: { x: 1200, y: 0, z: 500 } }] },
  }),
  buildRecord({
    id: 'build_environment.create_pipe_spline', action: 'create_pipe_spline', family: F,
    summary: 'Run a pipe along a route: a round tube 50 across by default (the engine cylinder), centred on the route; width is the diameter. meshPath swaps in a pipe mesh (its length along X), fitted to width.',
    whenToUse: TEMPLATE_WHEN('A pipe, duct or tube'),
    whenNotToUse: ['A few straight pipe pieces placed by hand are enough.'],
    inputProps: TEMPLATE, outputProps: TEMPLATE_OUT,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_pipe_spline', name: 'Pipe_1', width: 40 },
  }),
  buildRecord({
    id: 'build_environment.get_splines_info', action: 'get_splines_info', family: F,
    topics: ['point count and length', 'closed loop', 'world points'],
    summary: 'List the spline actors in the level with each one\'s point count, length and world points (first 64); with actorName, one spline\'s points (location relative to the actor, the space spline edits take, and worldLocation), types, length and closed-loop flag.',
    whenToUse: ['Spline actors must be enumerated.'],
    whenNotToUse: ['A specific spline path is already known.'],
    inputProps: { actorName: P.actorName },
    outputProps: { splines: { type: 'array', description: 'Spline info.', items: { type: 'object', description: 'Spline actor info.', additionalProperties: true, 'x-unreal-reflection-boundary': true } } },
    outputRequired: ['splines'],
    effect: 'read', latency: 'instant', resources: 'low',
    exampleInput: { action: 'get_splines_info' },
    exampleOutput: { success: true, splines: [] },
  }),
];
