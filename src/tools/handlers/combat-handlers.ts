/**
 * Combat & Weapons System Handlers (Phase 15)
 *
 * Complete combat implementation including:
 * - Weapon Base (creation, mesh, sockets, stats)
 * - Firing Modes (hitscan, projectile, spread, recoil, ADS)
 * - Projectiles (creation, movement, collision, homing)
 * - Damage System (damage types, execution, hitboxes)
 * - Weapon Features (reload, ammo, attachments, switching)
 * - Effects (muzzle flash, tracer, impact, shell ejection)
 * - Melee Combat (traces, combos, hitstop, hit reactions, parry, block, trails)
 *
 * @module combat-handlers
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
 * Handles all combat & weapon actions for the manage_combat tool.
 */
export async function handleCombatTools(
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
      'manage_combat',
      payload as HandlerArgs,
      `Automation bridge not available for combat action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // =========================================================================
    // 15.1 Weapon Base (4 actions)
    // =========================================================================

    case 'create_weapon_blueprint': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_weapon_blueprint');
    }

    case 'configure_weapon_mesh': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_weapon_mesh');
    }

    case 'configure_weapon_sockets': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_weapon_sockets');
    }

    case 'set_weapon_stats': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('set_weapon_stats');
    }

    // =========================================================================
    // 15.2 Firing Modes (5 actions)
    // =========================================================================

    case 'configure_hitscan': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_hitscan');
    }

    case 'configure_projectile': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_projectile');
    }

    case 'configure_spread_pattern': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_spread_pattern');
    }

    case 'configure_recoil_pattern': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_recoil_pattern');
    }

    case 'configure_aim_down_sights': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_aim_down_sights');
    }

    // =========================================================================
    // 15.3 Projectiles (4 actions)
    // =========================================================================

    case 'create_projectile_blueprint': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_projectile_blueprint');
    }

    case 'configure_projectile_movement': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_projectile_movement');
    }

    case 'configure_projectile_collision': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_projectile_collision');
    }

    case 'configure_projectile_homing': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_projectile_homing');
    }

    // =========================================================================
    // 15.4 Damage System (3 actions)
    // =========================================================================

    case 'create_damage_type': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_damage_type');
    }

    case 'configure_damage_execution': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_damage_execution');
    }

    case 'setup_hitbox_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_hitbox_component');
    }

    // =========================================================================
    // 15.5 Weapon Features (4 actions)
    // =========================================================================

    case 'setup_reload_system': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_reload_system');
    }

    case 'setup_ammo_system': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_ammo_system');
    }

    case 'setup_attachment_system': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_attachment_system');
    }

    case 'setup_weapon_switching': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_weapon_switching');
    }

    // =========================================================================
    // 15.6 Effects (4 actions)
    // =========================================================================

    case 'configure_muzzle_flash': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_muzzle_flash');
    }

    case 'configure_tracer': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_tracer');
    }

    case 'configure_impact_effects': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_impact_effects');
    }

    case 'configure_shell_ejection': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_shell_ejection');
    }

    // =========================================================================
    // 15.7 Melee Combat (6 actions)
    // =========================================================================

    case 'create_melee_trace': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('create_melee_trace');
    }

    case 'configure_combo_system': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_combo_system');
    }

    case 'create_hit_pause': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('create_hit_pause');
    }

    case 'configure_hit_reaction': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_hit_reaction');
    }

    case 'setup_parry_block_system': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('setup_parry_block_system');
    }

    case 'configure_weapon_trails': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_weapon_trails');
    }

    // =========================================================================
    // Utility (1 action)
    // =========================================================================

    case 'get_combat_info': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('get_combat_info');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown combat action: ${action}`
      });
  }
}
