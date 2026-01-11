/**
 * PCG Handlers (Phase 27)
 *
 * Complete PCG (Procedural Content Generation) framework system including:
 * - Graph Management: create_pcg_graph, create_pcg_subgraph, add_pcg_node, connect_pcg_pins, set_pcg_node_settings
 * - Input Nodes: add_landscape_data_node, add_spline_data_node, add_volume_data_node, add_actor_data_node, add_texture_data_node
 * - Point Operations: surface_sampler, mesh_sampler, spline_sampler, volume_sampler, filters, modifiers
 * - Spawning: add_static_mesh_spawner, add_actor_spawner, add_spline_spawner
 * - Execution: execute_pcg_graph, set_pcg_partition_grid_size
 * - Utility: get_pcg_info
 *
 * @module pcg-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Normalize path fields to ensure they start with /Game/ and use forward slashes.
 * Returns a copy of the args with normalized paths.
 */
function normalizePathFields(args: Record<string, unknown>): Record<string, unknown> {
  const result = { ...args };
  const pathFields = [
    'graphPath', 'subgraphPath', 'actorPath', 'meshPath', 'texturePath',
    'landscapePath', 'splinePath', 'volumePath', 'actorClass'
  ];

  for (const field of pathFields) {
    const value = result[field];
    if (typeof value === 'string' && value.length > 0) {
      let normalized = value.replace(/\\/g, '/');
      // Replace /Content/ with /Game/ for common user mistake
      if (normalized.startsWith('/Content/')) {
        normalized = '/Game/' + normalized.slice('/Content/'.length);
      }
      // Allow /Script/ paths for built-in UE classes
      // Allow plugin paths like /MyPlugin/Assets to pass through unchanged
      if (!normalized.startsWith('/')) {
        normalized = '/Game/' + normalized;
      }
      result[field] = normalized;
    }
  }

  return result;
}

/**
 * Handles all PCG actions for the manage_pcg tool.
 */
export async function handlePCGTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  // Normalize path fields before sending to C++
  const argsRecord = normalizePathFields(args as Record<string, unknown>);
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<HandlerResult> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_pcg',
      payload as HandlerArgs,
      `Automation bridge not available for PCG action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };

  switch (action) {
    // ========================================================================
    // Graph Management (5 actions)
    // ========================================================================
    case 'create_pcg_graph':
      return sendRequest('create_pcg_graph');

    case 'create_pcg_subgraph':
      return sendRequest('create_pcg_subgraph');

    case 'add_pcg_node':
      return sendRequest('add_pcg_node');

    case 'connect_pcg_pins':
      return sendRequest('connect_pcg_pins');

    case 'set_pcg_node_settings':
      return sendRequest('set_pcg_node_settings');

    // ========================================================================
    // Input Nodes (5 actions)
    // ========================================================================
    case 'add_landscape_data_node':
      return sendRequest('add_landscape_data_node');

    case 'add_spline_data_node':
      return sendRequest('add_spline_data_node');

    case 'add_volume_data_node':
      return sendRequest('add_volume_data_node');

    case 'add_actor_data_node':
      return sendRequest('add_actor_data_node');

    case 'add_texture_data_node':
      return sendRequest('add_texture_data_node');

    // ========================================================================
    // Point Operations - Samplers (4 actions)
    // ========================================================================
    case 'add_surface_sampler':
      return sendRequest('add_surface_sampler');

    case 'add_mesh_sampler':
      return sendRequest('add_mesh_sampler');

    case 'add_spline_sampler':
      return sendRequest('add_spline_sampler');

    case 'add_volume_sampler':
      return sendRequest('add_volume_sampler');

    // ========================================================================
    // Point Operations - Filters/Modifiers (11 actions)
    // ========================================================================
    case 'add_bounds_modifier':
      return sendRequest('add_bounds_modifier');

    case 'add_density_filter':
      return sendRequest('add_density_filter');

    case 'add_height_filter':
      return sendRequest('add_height_filter');

    case 'add_slope_filter':
      return sendRequest('add_slope_filter');

    case 'add_distance_filter':
      return sendRequest('add_distance_filter');

    case 'add_bounds_filter':
      return sendRequest('add_bounds_filter');

    case 'add_self_pruning':
      return sendRequest('add_self_pruning');

    case 'add_transform_points':
      return sendRequest('add_transform_points');

    case 'add_project_to_surface':
      return sendRequest('add_project_to_surface');

    case 'add_copy_points':
      return sendRequest('add_copy_points');

    case 'add_merge_points':
      return sendRequest('add_merge_points');

    // ========================================================================
    // Spawning (3 actions)
    // ========================================================================
    case 'add_static_mesh_spawner':
      return sendRequest('add_static_mesh_spawner');

    case 'add_actor_spawner':
      return sendRequest('add_actor_spawner');

    case 'add_spline_spawner':
      return sendRequest('add_spline_spawner');

    // ========================================================================
    // Execution (2 actions)
    // ========================================================================
    case 'execute_pcg_graph':
      return sendRequest('execute_pcg_graph');

    case 'set_pcg_partition_grid_size':
      return sendRequest('set_pcg_partition_grid_size');

    // ========================================================================
    // Utility (1 action)
    // ========================================================================
    case 'get_pcg_info':
      return sendRequest('get_pcg_info');

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown PCG action: ${action}`
      });
  }
}
