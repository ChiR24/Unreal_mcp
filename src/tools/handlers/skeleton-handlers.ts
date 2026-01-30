/**
 * Skeleton and Rigging handlers for Phase 7
 *
 * Routes all skeleton actions to the C++ automation bridge.
 * Handles skeleton structure, sockets, physics assets, skin weights, and morph targets.
 */
import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import { executeAutomationRequest, normalizeLocation } from './common-handlers.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';

// Valid actions for manage_skeleton tool
// These must match C++ dispatcher in McpAutomationBridge_SkeletonHandlers.cpp
const SKELETON_ACTIONS = [
  // 7.1 Skeleton Creation
  'create_skeleton', 'add_bone', 'remove_bone', 'rename_bone',
  'set_bone_transform', 'set_bone_parent',
  'create_virtual_bone',
  'create_socket', 'configure_socket',
  // 7.2 Skin Weights
  'auto_skin_weights', 'set_vertex_weights',
  'normalize_weights', 'prune_weights',
  'copy_weights', 'mirror_weights',
  // 7.3 Physics Asset
  'create_physics_asset',
  'add_physics_body', 'configure_physics_body',
  'add_physics_constraint', 'configure_constraint_limits',
  // 7.4 Cloth Setup (Basic)
  'bind_cloth_to_skeletal_mesh', 'assign_cloth_asset_to_mesh',
  // 7.5 Morph Targets
  'create_morph_target', 'set_morph_target_deltas', 'import_morph_targets',
  // Utils
  'get_skeleton_info', 'list_bones', 'list_sockets', 'list_physics_bodies', 'list_skeletal_meshes'
] as const;

type SkeletonAction = (typeof SKELETON_ACTIONS)[number];

/**
 * Normalize skeleton arguments before sending to C++
 */
function normalizeSkeletonArgs(action: string, args: HandlerArgs): Record<string, unknown> {
  const normalized: Record<string, unknown> = { ...args, subAction: action };

  // Normalize location/position parameters
  if (args.location) {
    normalized.location = normalizeLocation(args.location);
  }
  if (args.position) {
    normalized.position = normalizeLocation(args.position);
  }
  if (args.translation) {
    normalized.translation = normalizeLocation(args.translation);
  }

  // Normalize rotation (array or object)
  if (args.rotation) {
    if (Array.isArray(args.rotation)) {
      const [pitch, yaw, roll] = args.rotation.map((v: unknown) => Number(v) || 0);
      normalized.rotation = { pitch, yaw, roll };
    } else if (typeof args.rotation === 'object' && args.rotation !== null) {
      normalized.rotation = args.rotation;
    }
  }

  // Normalize scale
  if (args.scale) {
    if (Array.isArray(args.scale)) {
      const [x, y, z] = args.scale.map((v: unknown) => Number(v) || 1);
      normalized.scale = { x, y, z };
    } else if (typeof args.scale === 'object' && args.scale !== null) {
      normalized.scale = args.scale;
    } else if (typeof args.scale === 'number') {
      normalized.scale = { x: args.scale, y: args.scale, z: args.scale };
    }
  }

  // Normalize bone weights array
  if (args.weights && Array.isArray(args.weights)) {
    normalized.weights = args.weights.map((w: unknown) => {
      if (typeof w === 'object' && w !== null) {
        const wObj = w as HandlerResult;
        return {
          boneIndex: Number(wObj.boneIndex ?? wObj.bone ?? 0),
          boneName: String(wObj.boneName ?? wObj.name ?? ''),
          weight: Number(wObj.weight ?? 0)
        };
      }
      return { weight: Number(w) || 0 };
    });
  }

  // Normalize constraint limits
  if (args.limits && typeof args.limits === 'object' && args.limits !== null) {
    const limits = args.limits as HandlerResult;
    normalized.limits = {
      swing1LimitAngle: Number(limits.swing1LimitAngle ?? limits.swing1 ?? 45),
      swing2LimitAngle: Number(limits.swing2LimitAngle ?? limits.swing2 ?? 45),
      twistLimitAngle: Number(limits.twistLimitAngle ?? limits.twist ?? 45),
      swing1Motion: String(limits.swing1Motion ?? 'Limited'),
      swing2Motion: String(limits.swing2Motion ?? 'Limited'),
      twistMotion: String(limits.twistMotion ?? 'Limited')
    };
  }

  return normalized;
}

/**
 * Handle all skeleton-related tool actions
 *
 * @param action - The skeleton action to perform
 * @param args - Arguments for the action
 * @param tools - Tool interface with automation bridge
 * @returns Response from C++ handler
 */
export async function handleSkeletonTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  // Validate action
  if (!SKELETON_ACTIONS.includes(action as SkeletonAction)) {
    return {
      success: false,
      error: 'INVALID_ACTION',
      message: `Unknown skeleton action: ${action}. Valid actions: ${SKELETON_ACTIONS.join(', ')}`
    };
  }

  // Normalize arguments
  const normalizedArgs = normalizeSkeletonArgs(action, args);

  // Route to C++ handler
  try {
    const response = await executeAutomationRequest(
      tools,
      'manage_skeleton',
      normalizedArgs,
      `Automation bridge not available for skeleton action: ${action}`
    );

    return cleanObject(response as HandlerResult);
  } catch (error: unknown) {
    const err = error instanceof Error ? error : new Error(String(error));
    return {
      success: false,
      error: 'SKELETON_ERROR',
      message: err.message
    };
  }
}
