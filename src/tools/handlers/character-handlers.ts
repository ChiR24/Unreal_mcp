/**
 * Character & Movement System Handlers (Phase 14)
 *
 * Complete character setup with advanced movement including:
 * - Character Creation & Components
 * - Movement Component Configuration
 * - Advanced Movement (mantling, vaulting, climbing, etc.)
 * - Footstep System
 *
 * @module character-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all character & movement actions for the manage_character tool.
 */
export async function handleCharacterTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<HandlerResult> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_character',
      payload as HandlerArgs,
      `Automation bridge not available for character action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };

  switch (action) {
    // =========================================================================
    // 14.1 Character Creation (4 actions)
    // =========================================================================

    case 'create_character_blueprint': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_character_blueprint');
    }

    case 'configure_capsule_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_capsule_component');
    }

    case 'configure_mesh_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_mesh_component');
    }

    case 'configure_camera_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_camera_component');
    }

    // =========================================================================
    // 14.2 Movement Component (5 actions)
    // =========================================================================

    case 'configure_movement_speeds': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_movement_speeds');
    }

    case 'configure_jump': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_jump');
    }

    case 'configure_rotation': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_rotation');
    }

    case 'add_custom_movement_mode': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.modeName, 'modeName', 'Missing required parameter: modeName');
      return sendRequest('add_custom_movement_mode');
    }

    case 'configure_nav_movement': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_nav_movement');
    }

    // =========================================================================
    // 14.3 Advanced Movement (6 actions)
    // =========================================================================

    case 'setup_mantling': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_mantling');
    }

    case 'setup_vaulting': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_vaulting');
    }

    case 'setup_climbing': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_climbing');
    }

    case 'setup_sliding': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_sliding');
    }

    case 'setup_wall_running': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_wall_running');
    }

    case 'setup_grappling': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_grappling');
    }

    // =========================================================================
    // 14.4 Footsteps System (3 actions)
    // =========================================================================

    case 'setup_footstep_system': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_footstep_system');
    }

    case 'map_surface_to_sound': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.surfaceType, 'surfaceType', 'Missing required parameter: surfaceType');
      return sendRequest('map_surface_to_sound');
    }

    case 'configure_footstep_fx': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_footstep_fx');
    }

    // =========================================================================
    // Utility (1 action)
    // =========================================================================

    case 'get_character_info': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('get_character_info');
    }

    // =========================================================================
    // Wave 3.21-3.30: Character System Actions (10 actions)
    // =========================================================================

    case 'configure_locomotion_state': {
      // 3.21: Configure locomotion state machine
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_locomotion_state');
    }

    case 'query_interaction_targets': {
      // 3.22: Query available interactions
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('query_interaction_targets');
    }

    case 'configure_inventory_slot': {
      // 3.23: Configure inventory slot
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      if (typeof argsRecord.slotIndex !== 'number') {
        throw new Error('manage_character:configure_inventory_slot requires slotIndex (number)');
      }
      return sendRequest('configure_inventory_slot');
    }

    case 'batch_add_inventory_items': {
      // 3.24: Add multiple items at once
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      if (!Array.isArray(argsRecord.itemDataAssets)) {
        throw new Error('manage_character:batch_add_inventory_items requires itemDataAssets (array)');
      }
      return sendRequest('batch_add_inventory_items');
    }

    case 'configure_equipment_socket': {
      // 3.25: Configure equipment attachment
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.socketName, 'socketName', 'Missing required parameter: socketName');
      return sendRequest('configure_equipment_socket');
    }

    case 'get_character_stats_snapshot': {
      // 3.26: Get all stats snapshot
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('get_character_stats_snapshot');
    }

    case 'apply_status_effect': {
      // 3.27: Apply status effect to character
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      requireNonEmptyString(argsRecord.statusEffectId, 'statusEffectId', 'Missing required parameter: statusEffectId');
      return sendRequest('apply_status_effect');
    }

    case 'configure_footstep_system': {
      // 3.28: Configure footstep sounds/VFX
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_footstep_system');
    }

    case 'set_movement_mode': {
      // 3.29: Set character movement mode
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      requireNonEmptyString(argsRecord.movementMode, 'movementMode', 'Missing required parameter: movementMode');
      return sendRequest('set_movement_mode');
    }

    case 'configure_mantle_vault': {
      // 3.30: Configure mantle/vault system
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_mantle_vault');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown character action: ${action}`
      });
  }
}
