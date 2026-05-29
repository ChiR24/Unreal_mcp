/**
 * Geometry Script handlers for Phase 6
 *
 * Routes all geometry actions to the C++ automation bridge.
 * Uses UE Geometry Script plugin for mesh operations.
 */
import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import { executeAutomationRequest, normalizeLocation, normalizePathFields } from './common-handlers.js';
import type { HandlerArgs } from '../../types/handler-types.js';

// Valid actions for manage_geometry tool
// These must match C++ dispatcher in McpAutomationBridge_GeometryHandlers.cpp
const GEOMETRY_ACTIONS = [
  // Primitives (basic)
  'create_box', 'create_sphere', 'create_cylinder', 'create_cone', 'create_capsule',
  'create_torus', 'create_plane', 'create_disc',
  // Primitives (additional)
  'create_stairs', 'create_spiral_stairs', 'create_ring',
  'create_arch', 'create_pipe', 'create_ramp',
  // Booleans
  'boolean_union', 'boolean_subtract', 'boolean_intersection',
  'boolean_trim', 'self_union',
  // Modeling operations
  'extrude', 'inset', 'outset', 'bevel', 'offset_faces', 'shell', 'revolve', 'chamfer',
  'extrude_along_spline', 'bridge', 'loft', 'sweep',
  'duplicate_along_spline', 'loop_cut', 'edge_split', 'quadrangulate',
  // Deformers
  'bend', 'twist', 'taper', 'noise_deform', 'smooth', 'relax',
  'stretch', 'spherify', 'cylindrify', 'lattice_deform', 'displace_by_texture',
  // Topology operations
  'triangulate', 'poke',
  // Mesh processing
  'simplify_mesh', 'subdivide', 'remesh_uniform', 'merge_vertices', 'remesh_voxel',
  // Mesh repair
  'weld_vertices', 'fill_holes', 'remove_degenerates',
  // UVs
  'auto_uv', 'project_uv', 'transform_uvs', 'unwrap_uv', 'pack_uv_islands',
  // Normals/Tangents
  'recalculate_normals', 'flip_normals', 'recompute_tangents',
  // Collision
  'generate_collision', 'generate_complex_collision', 'simplify_collision',
  // LOD operations
  'generate_lods', 'set_lod_settings', 'set_lod_screen_sizes', 'convert_to_nanite',
  // Transform operations
  'mirror', 'array_linear', 'array_radial',
  // Export/conversion
  'convert_to_static_mesh',
  // Utils
  'get_mesh_info'
] as const;

type GeometryAction = (typeof GEOMETRY_ACTIONS)[number];

const GEOMETRY_PATH_FIELDS = ['assetPath', 'outputPath', 'path', 'texturePath'] as const;

function copyAlias(
  normalized: Record<string, unknown>,
  args: HandlerArgs,
  from: string,
  to: string
): void {
  if (normalized[to] === undefined && args[from] !== undefined) {
    normalized[to] = args[from];
  }
}

function getNumberArg(args: HandlerArgs, key: string): number | undefined {
  const value = args[key];
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

function finiteNumberOrZero(value: unknown): number {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : 0;
}

function normalizeFiniteLocation(value: unknown): [number, number, number] | undefined {
  const normalized = normalizeLocation(value);
  return normalized
    ? [finiteNumberOrZero(normalized[0]), finiteNumberOrZero(normalized[1]), finiteNumberOrZero(normalized[2])]
    : undefined;
}

function normalizeFiniteNumberArray(values: unknown[]): number[] {
  return values.map(finiteNumberOrZero);
}

function copyUvPair(
  normalized: Record<string, unknown>,
  args: HandlerArgs,
  from: string,
  uKey: string,
  vKey: string
): void {
  const value = args[from];
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return;
  }

  const uv = value as Record<string, unknown>;
  if (normalized[uKey] === undefined && typeof uv.u === 'number' && Number.isFinite(uv.u)) {
    normalized[uKey] = uv.u;
  }
  if (normalized[vKey] === undefined && typeof uv.v === 'number' && Number.isFinite(uv.v)) {
    normalized[vKey] = uv.v;
  }
}

function normalizeGeometryAliases(
  action: string,
  args: HandlerArgs,
  normalized: Record<string, unknown>
): void {
  switch (action) {
    case 'create_sphere':
      copyAlias(normalized, args, 'radialSegments', 'subdivisions');
      copyAlias(normalized, args, 'numRings', 'subdivisions');
      break;
    case 'create_cylinder':
    case 'create_cone':
    case 'create_disc':
    case 'create_ring':
      copyAlias(normalized, args, 'numSides', 'segments');
      break;
    case 'create_capsule':
      copyAlias(normalized, args, 'radialSegments', 'segments');
      copyAlias(normalized, args, 'numRings', 'hemisphereSteps');
      copyAlias(normalized, args, 'heightSegments', 'hemisphereSteps');
      break;
    case 'create_torus':
      copyAlias(normalized, args, 'radius', 'majorRadius');
      copyAlias(normalized, args, 'innerRadius', 'minorRadius');
      copyAlias(normalized, args, 'numSides', 'majorSegments');
      copyAlias(normalized, args, 'radialSegments', 'minorSegments');
      copyAlias(normalized, args, 'numRings', 'minorSegments');
      break;
    case 'create_arch':
      copyAlias(normalized, args, 'radius', 'majorRadius');
      copyAlias(normalized, args, 'innerRadius', 'minorRadius');
      copyAlias(normalized, args, 'numSides', 'majorSteps');
      copyAlias(normalized, args, 'radialSegments', 'majorSteps');
      copyAlias(normalized, args, 'numRings', 'minorSteps');
      break;
    case 'create_pipe':
      copyAlias(normalized, args, 'radius', 'outerRadius');
      copyAlias(normalized, args, 'numSides', 'radialSteps');
      copyAlias(normalized, args, 'heightSegments', 'heightSteps');
      break;
    case 'create_spiral_stairs': {
      const numTurns = getNumberArg(args, 'numTurns');
      if (normalized.curveAngle === undefined && numTurns !== undefined) {
        normalized.curveAngle = numTurns * 360;
      }
      break;
    }
    case 'extrude':
    case 'inset':
    case 'outset':
    case 'bevel':
    case 'offset_faces':
    case 'chamfer':
      copyAlias(normalized, args, 'amount', 'distance');
      break;
    case 'simplify_mesh': {
      const reductionPercent = getNumberArg(args, 'reductionPercent');
      if (normalized.targetPercentage === undefined && reductionPercent !== undefined) {
        normalized.targetPercentage = Math.max(1, Math.min(100, 100 - reductionPercent));
      }
      break;
    }
    case 'remesh_voxel':
      copyAlias(normalized, args, 'targetEdgeLength', 'voxelSize');
      break;
    case 'generate_complex_collision':
      copyAlias(normalized, args, 'hullCount', 'maxHullCount');
      copyAlias(normalized, args, 'maxVerticesPerHull', 'maxHullVerts');
      break;
    case 'recalculate_normals':
      copyAlias(normalized, args, 'computeWeightedNormals', 'areaWeighted');
      copyAlias(normalized, args, 'hardEdgeAngle', 'splitAngle');
      break;
    case 'merge_vertices':
    case 'weld_vertices':
      copyAlias(normalized, args, 'weldDistance', 'tolerance');
      break;
    case 'set_lod_settings':
      copyAlias(normalized, args, 'reductionPercent', 'trianglePercent');
      break;
    case 'transform_uvs':
      copyUvPair(normalized, args, 'uvScale', 'scaleU', 'scaleV');
      copyUvPair(normalized, args, 'uvOffset', 'translateU', 'translateV');
      break;
    case 'convert_to_static_mesh':
    case 'convert_to_nanite':
      copyAlias(normalized, args, 'outputPath', 'assetPath');
      break;
  }
}

/**
 * Normalize geometry arguments before sending to C++
 */
function normalizeGeometryArgs(action: string, args: HandlerArgs): Record<string, unknown> {
  const normalized: Record<string, unknown> = { ...args, subAction: action };

  // Normalize location/position parameters
  if (args.location) {
    normalized.location = normalizeFiniteLocation(args.location);
  }
  if (args.position) {
    normalized.position = normalizeFiniteLocation(args.position);
  }
  if (args.center) {
    normalized.center = normalizeFiniteLocation(args.center);
  }

  // Normalize dimensions for primitives
  if (args.dimensions && Array.isArray(args.dimensions)) {
    normalized.dimensions = normalizeFiniteNumberArray(args.dimensions);
  }

  // Normalize axis vectors
  if (args.axis && Array.isArray(args.axis)) {
    normalized.axis = normalizeFiniteNumberArray(args.axis);
  }

  normalizeGeometryAliases(action, args, normalized);

  return normalizePathFields(normalized, GEOMETRY_PATH_FIELDS);
}

/**
 * Handle all geometry-related tool actions
 *
 * @param action - The geometry action to perform
 * @param args - Arguments for the action
 * @param tools - Tool interface with automation bridge
 * @returns Response from C++ handler
 */
export async function handleGeometryTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  // Validate action
  if (!GEOMETRY_ACTIONS.includes(action as GeometryAction)) {
    return {
      success: false,
      error: 'INVALID_ACTION',
      message: `Unknown geometry action: '${action}'. Valid actions: ${GEOMETRY_ACTIONS.join(', ')}`
    };
  }

  // Normalize args and forward to C++
  const normalizedArgs = normalizeGeometryArgs(action, args);

  try {
    const result = await executeAutomationRequest(
      tools,
      'manage_geometry',
      normalizedArgs,
      `Automation bridge not available for geometry action: ${action}`
    );
    return cleanObject(result) as Record<string, unknown>;
  } catch (error) {
    const err = error instanceof Error ? error : new Error(String(error));
    return {
      success: false,
      error: 'GEOMETRY_ERROR',
      message: err.message,
      action
    };
  }
}
