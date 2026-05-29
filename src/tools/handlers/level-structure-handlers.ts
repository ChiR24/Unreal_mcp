/**
 * Level Structure Handlers (Phase 23)
 *
 * Complete level and world structure management including:
 * - Levels: create levels, sublevels, streaming, bounds
 * - World Partition: grid configuration, data layers, HLOD
 * - Level Blueprint: open, add nodes, connect nodes
 * - Level Instances: packed level actors, level instances
 *
 * @module level-structure-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { createSubActionDispatcher, normalizePathFields } from './common-handlers.js';


/**
 * Validates level name for invalid characters and length.
 * Returns null if valid, error message if invalid.
 */
function validateLevelName(levelName: string): string | null {
  if (!levelName || levelName.trim() === '') {
    return 'levelName cannot be empty';
  }

  // Check length (max 255 chars for filesystem compatibility)
  if (levelName.length > 255) {
    return 'levelName exceeds maximum length of 255 characters';
  }

  // Check for invalid characters that would cause issues
  // These characters are not allowed in Windows filenames and UE asset names
  const invalidChars = /[\\/:*?"<>|]/;
  if (invalidChars.test(levelName)) {
    return 'levelName contains invalid characters. Cannot use: \\ / : * ? " < > |';
  }

  // Check for leading/trailing spaces
  if (levelName !== levelName.trim()) {
    return 'levelName cannot have leading or trailing spaces';
  }

  // Check for reserved Windows filenames
  const reservedNames = /^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i;
  if (reservedNames.test(levelName)) {
    return 'levelName cannot be a reserved Windows device name: ' + levelName;
  }

  return null;
}

function normalizeLevelNamePath(args: Record<string, unknown>): Record<string, unknown> {
  const levelName = args.levelName;
  if (typeof levelName !== 'string' || !/[\\/]/.test(levelName)) return {};
  return {
    levelName: normalizePathFields({ levelName }, ['levelName']).levelName
  };
}

/**
 * Handles all level structure actions for the manage_level_structure tool.
 */
export async function handleLevelStructureTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { argsRecord, sendRequest } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_level_structure',
    domainName: 'level structure',
    pathFields: [
      'levelPath', 'sublevelPath', 'levelAssetPath', 'hlodLayerPath',
      'actorPath', 'parentLevel', 'dataLayerAssetPath'
    ]
  });

  switch (action) {
    // ========================================================================
    // Levels (5 actions)
    // ========================================================================
    case 'create_level': {
      // Validate levelName before sending to C++ handler
      const levelName = String(argsRecord.levelName || '');
      const validationError = validateLevelName(levelName);
      if (validationError) {
        return {
          success: false,
          error: 'INVALID_ARGUMENT',
          message: validationError,
          action
        };
      }
      return sendRequest('create_level');
    }

    case 'create_sublevel':
      return sendRequest('create_sublevel');

    case 'configure_level_streaming':
      return sendRequest('configure_level_streaming', normalizeLevelNamePath(argsRecord));

    case 'set_streaming_distance':
      return sendRequest('set_streaming_distance', normalizeLevelNamePath(argsRecord));

    case 'configure_level_bounds':
      return sendRequest('configure_level_bounds');

    // ========================================================================
    // World Partition (6 actions)
    // ========================================================================
    case 'enable_world_partition':
      return sendRequest('enable_world_partition');

    case 'configure_grid_size':
      return sendRequest('configure_grid_size');

    case 'create_data_layer':
      return sendRequest('create_data_layer');

    case 'assign_actor_to_data_layer':
      return sendRequest('assign_actor_to_data_layer');

    case 'configure_hlod_layer':
      return sendRequest('configure_hlod_layer');

    case 'create_minimap_volume':
      return sendRequest('create_minimap_volume');

    // ========================================================================
    // Level Blueprint (3 actions)
    // ========================================================================
    case 'open_level_blueprint':
      return sendRequest('open_level_blueprint');

    case 'add_level_blueprint_node':
      return sendRequest('add_level_blueprint_node');

    case 'connect_level_blueprint_nodes':
      return sendRequest('connect_level_blueprint_nodes');

    // ========================================================================
    // Level Instances (2 actions)
    // ========================================================================
    case 'create_level_instance':
      return sendRequest('create_level_instance');

    case 'create_packed_level_actor':
      return sendRequest('create_packed_level_actor');

    // ========================================================================
    // Utility (1 action)
    // ========================================================================
    case 'get_level_structure_info':
      return sendRequest('get_level_structure_info');

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown level structure action: ${action}`
      });
  }
}
