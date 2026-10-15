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
// Every template reads the same inputs; fence posts go through scatter_meshes_along_spline.
const TEMPLATE = {
  name: P.name, location: P.location, meshPath: P.meshPath, points: P.points,
  materialPath: P.materialPath, width: { ...P.width, description: 'Width of the deformed mesh ribbon in world units (default 400).' }, closedLoop: P.closedLoop,
};

export const SPLINE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_spline_actor', action: 'create_spline_actor', family: F,
    summary: 'Create a spline actor in the level.',
    whenToUse: WU, whenNotToUse: ['A static mesh path should be used instead.'],
    inputProps: { initialPoints: P.initialPoints, name: P.name, location: P.location, rotation: P.rotation, points: P.points, bClosedLoop: P.bClosedLoop,
      splineType: { ...P.splineType, description: 'Point type for every point: Linear, Curve (default), Constant, CurveClamped or CurveCustomTangent.' } },
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_spline_actor', name: 'Spline_1', location: { x: 0, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.add_spline_point', action: 'add_spline_point', family: F,
    summary: 'Add a point to an existing spline.',
    whenToUse: WU, whenNotToUse: ['The spline should be recreated with all points.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, position: SPLINE_POSITION, index: P.index,
      arriveTangent: P.arriveTangent, leaveTangent: P.leaveTangent, pointType: P.pointType },
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'add_spline_point', actorName: 'Spline_1', position: { x: 100, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.remove_spline_point', action: 'remove_spline_point', family: F,
    summary: 'Remove a point from a spline by index.',
    whenToUse: WU, whenNotToUse: ['The spline should be recreated without the point.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex },
    required: [], effect: 'destructive', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'remove_spline_point', actorName: 'Spline_1', pointIndex: 2 },
  }),
  buildRecord({
    id: 'build_environment.set_spline_point_position', action: 'set_spline_point_position', family: F,
    summary: 'Set the position of a spline point by index.',
    whenToUse: WU, whenNotToUse: ['The point should be moved interactively.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex, position: SPLINE_POSITION },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_point_position', actorName: 'Spline_1', pointIndex: 0, position: { x: 0, y: 0, z: 100 } },
  }),
  buildRecord({
    id: 'build_environment.set_spline_point_tangents', action: 'set_spline_point_tangents', family: F,
    summary: 'Set the arrive and leave tangents of a spline point (a lone arriveTangent sets both).',
    whenToUse: WU, whenNotToUse: ['Default tangents are sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex,
      arriveTangent: P.arriveTangent, leaveTangent: P.leaveTangent },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_point_tangents', actorName: 'Spline_1', pointIndex: 0, arriveTangent: { x: 50, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.set_spline_point_rotation', action: 'set_spline_point_rotation', family: F,
    summary: 'Set the rotation of a spline point.',
    whenToUse: WU, whenNotToUse: ['Default rotation is sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex, pointRotation: P.pointRotation },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_point_rotation', actorName: 'Spline_1', pointIndex: 0, pointRotation: { pitch: 0, yaw: 45, roll: 0 } },
  }),
  buildRecord({
    id: 'build_environment.set_spline_point_scale', action: 'set_spline_point_scale', family: F,
    summary: 'Set the scale of a spline point.',
    whenToUse: WU, whenNotToUse: ['Default scale is sufficient.'],
    inputProps: { actorName: P.actorName, actorPath: ACTOR_PATH, pointIndex: P.pointIndex, pointScale: P.pointScale },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'set_spline_point_scale', actorName: 'Spline_1', pointIndex: 0, pointScale: { x: 1, y: 1, z: 1 } },
  }),
  buildRecord({
    id: 'build_environment.set_spline_type', action: 'set_spline_type', family: F,
    summary: 'Set the spline point type (Linear, Curve, Constant, etc.).',
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
    summary: 'Set the forward axis of a spline mesh component.',
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
    summary: 'Configure mesh spacing along a spline.',
    whenToUse: WU, whenNotToUse: ['Default spacing is sufficient.'],
    inputProps: { useRandomOffset: P.useRandomOffset, randomOffsetRange: P.randomOffsetRange, actorName: P.actorName, actorPath: ACTOR_PATH, spacing: P.spacing },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_mesh_spacing', actorName: 'Spline_1', spacing: 150 },
  }),
  buildRecord({
    id: 'build_environment.configure_mesh_randomization', action: 'configure_mesh_randomization', family: F,
    summary: 'Configure randomization for meshes scattered along a spline.',
    whenToUse: WU, whenNotToUse: ['Uniform placement is desired.'],
    inputProps: { rotationRange: P.rotationRange, actorName: P.actorName, actorPath: ACTOR_PATH,
      randomizeRotation: P.randomizeRotation, randomizeScale: P.randomizeScale, minScale: P.minScale, maxScale: P.maxScale },
    required: [], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_mesh_randomization', actorName: 'Spline_1', randomizeRotation: true },
  }),
  buildRecord({
    id: 'build_environment.create_road_spline', action: 'create_road_spline', family: F,
    summary: 'Create a road spline with mesh and width.',
    whenToUse: WU, whenNotToUse: ['A generic spline should be used.'],
    inputProps: TEMPLATE,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_road_spline', name: 'Road_1', meshPath: '/Game/Meshes/SM_Road', materialPath: '/Game/Materials/M_Road', width: 500, closedLoop: true },
  }),
  buildRecord({
    id: 'build_environment.create_river_spline', action: 'create_river_spline', family: F,
    summary: 'Create a river spline with mesh and width.',
    whenToUse: WU, whenNotToUse: ['A water body actor should be used instead.'],
    inputProps: TEMPLATE,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_river_spline', name: 'River_1', width: 800 },
  }),
  buildRecord({
    id: 'build_environment.create_fence_spline', action: 'create_fence_spline', family: F,
    summary: 'Create a fence spline: a mesh deformed along the route (place posts with scatter_meshes_along_spline).',
    whenToUse: WU, whenNotToUse: ['Manual fence placement is sufficient.'],
    inputProps: TEMPLATE,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_fence_spline', name: 'Fence_1', meshPath: '/Game/Meshes/SM_FenceRail' },
  }),
  buildRecord({
    id: 'build_environment.create_wall_spline', action: 'create_wall_spline', family: F,
    summary: 'Create a wall spline with mesh and height.',
    whenToUse: WU, whenNotToUse: ['Manual wall placement is sufficient.'],
    inputProps: TEMPLATE,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_wall_spline', name: 'Wall_1', meshPath: '/Game/Meshes/SM_Wall', width: 300 },
  }),
  buildRecord({
    id: 'build_environment.create_cable_spline', action: 'create_cable_spline', family: F,
    summary: 'Create a cable spline between two points.',
    whenToUse: WU, whenNotToUse: ['A physics cable component should be used.'],
    inputProps: TEMPLATE,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_cable_spline', name: 'Cable_1' },
  }),
  buildRecord({
    id: 'build_environment.create_pipe_spline', action: 'create_pipe_spline', family: F,
    summary: 'Create a pipe spline with mesh and radius.',
    whenToUse: WU, whenNotToUse: ['Manual pipe placement is sufficient.'],
    inputProps: TEMPLATE,
    required: [], effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_pipe_spline', name: 'Pipe_1', meshPath: '/Game/Meshes/SM_Pipe', width: 50 },
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
