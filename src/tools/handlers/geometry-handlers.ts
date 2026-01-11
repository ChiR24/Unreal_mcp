/**
 * Geometry Script handlers for Phase 6
 *
 * Routes all geometry actions to the C++ automation bridge.
 * Uses UE Geometry Script plugin for mesh operations.
 */
import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import { executeAutomationRequest, normalizeLocation } from './common-handlers.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';

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

/**
 * Normalize geometry arguments before sending to C++
 */
function normalizeGeometryArgs(action: string, args: HandlerArgs): Record<string, unknown> {
  const normalized: Record<string, unknown> = { ...args, subAction: action };

  // Normalize location/position parameters
  if (args.location) {
    normalized.location = normalizeLocation(args.location);
  }
  if (args.position) {
    normalized.position = normalizeLocation(args.position);
  }
  if (args.center) {
    normalized.center = normalizeLocation(args.center);
  }

  // Normalize dimensions for primitives
  if (args.dimensions && Array.isArray(args.dimensions)) {
    normalized.dimensions = args.dimensions.map((v: unknown) => Number(v) || 0);
  }

  // Normalize axis vectors
  if (args.axis && Array.isArray(args.axis)) {
    normalized.axis = args.axis.map((v: unknown) => Number(v) || 0);
  }

  return normalized;
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
): Promise<HandlerResult> {
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
  } catch (error: unknown) {
    const err = error instanceof Error ? error : new Error(String(error));
    return {
      success: false,
      error: 'GEOMETRY_ERROR',
      message: err.message,
      action
    };
  }
}
