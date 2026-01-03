/**
 * Gameplay Ability System (GAS) Handlers (Phase 13)
 *
 * Complete GAS implementation including:
 * - Ability System Components & Attributes
 * - Gameplay Abilities
 * - Gameplay Effects
 * - Gameplay Cues
 *
 * @module gas-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all GAS actions for the manage_gas tool.
 */
export async function handleGASTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<Record<string, unknown>> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_gas',
      payload as HandlerArgs,
      `Automation bridge not available for GAS action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // =========================================================================
    // 13.1 Components & Attributes (6 actions)
    // =========================================================================

    case 'add_ability_system_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('add_ability_system_component');
    }

    case 'configure_asc': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_asc');
    }

    case 'create_attribute_set': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_attribute_set');
    }

    case 'add_attribute': {
      requireNonEmptyString(argsRecord.attributeSetPath, 'attributeSetPath', 'Missing required parameter: attributeSetPath');
      requireNonEmptyString(argsRecord.attributeName, 'attributeName', 'Missing required parameter: attributeName');
      return sendRequest('add_attribute');
    }

    case 'set_attribute_base_value': {
      requireNonEmptyString(argsRecord.attributeSetPath, 'attributeSetPath', 'Missing required parameter: attributeSetPath');
      requireNonEmptyString(argsRecord.attributeName, 'attributeName', 'Missing required parameter: attributeName');
      return sendRequest('set_attribute_base_value');
    }

    case 'set_attribute_clamping': {
      requireNonEmptyString(argsRecord.attributeSetPath, 'attributeSetPath', 'Missing required parameter: attributeSetPath');
      requireNonEmptyString(argsRecord.attributeName, 'attributeName', 'Missing required parameter: attributeName');
      return sendRequest('set_attribute_clamping');
    }

    // =========================================================================
    // 13.2 Gameplay Abilities (7 actions)
    // =========================================================================

    case 'create_gameplay_ability': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_gameplay_ability');
    }

    case 'set_ability_tags': {
      requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
      return sendRequest('set_ability_tags');
    }

    case 'set_ability_costs': {
      requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
      return sendRequest('set_ability_costs');
    }

    case 'set_ability_cooldown': {
      requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
      return sendRequest('set_ability_cooldown');
    }

    case 'set_ability_targeting': {
      requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
      return sendRequest('set_ability_targeting');
    }

    case 'add_ability_task': {
      requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
      requireNonEmptyString(argsRecord.taskType, 'taskType', 'Missing required parameter: taskType');
      return sendRequest('add_ability_task');
    }

    case 'set_activation_policy': {
      requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
      return sendRequest('set_activation_policy');
    }

    case 'set_instancing_policy': {
      requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
      return sendRequest('set_instancing_policy');
    }

    // =========================================================================
    // 13.3 Gameplay Effects (8 actions)
    // =========================================================================

    case 'create_gameplay_effect': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_gameplay_effect');
    }

    case 'set_effect_duration': {
      requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
      return sendRequest('set_effect_duration');
    }

    case 'add_effect_modifier': {
      requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
      requireNonEmptyString(argsRecord.attributeName, 'attributeName', 'Missing required parameter: attributeName');
      return sendRequest('add_effect_modifier');
    }

    case 'set_modifier_magnitude': {
      requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
      return sendRequest('set_modifier_magnitude');
    }

    case 'add_effect_execution_calculation': {
      requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
      requireNonEmptyString(argsRecord.calculationClass, 'calculationClass', 'Missing required parameter: calculationClass');
      return sendRequest('add_effect_execution_calculation');
    }

    case 'add_effect_cue': {
      requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
      requireNonEmptyString(argsRecord.cueTag, 'cueTag', 'Missing required parameter: cueTag');
      return sendRequest('add_effect_cue');
    }

    case 'set_effect_stacking': {
      requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
      return sendRequest('set_effect_stacking');
    }

    case 'set_effect_tags': {
      requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
      return sendRequest('set_effect_tags');
    }

    // =========================================================================
    // 13.4 Gameplay Cues (4 actions)
    // =========================================================================

    case 'create_gameplay_cue_notify': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.cueType, 'cueType', 'Missing required parameter: cueType');
      return sendRequest('create_gameplay_cue_notify');
    }

    case 'configure_cue_trigger': {
      requireNonEmptyString(argsRecord.cuePath, 'cuePath', 'Missing required parameter: cuePath');
      return sendRequest('configure_cue_trigger');
    }

    case 'set_cue_effects': {
      requireNonEmptyString(argsRecord.cuePath, 'cuePath', 'Missing required parameter: cuePath');
      return sendRequest('set_cue_effects');
    }

    case 'add_tag_to_asset': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.tagName, 'tagName', 'Missing required parameter: tagName');
      return sendRequest('add_tag_to_asset');
    }

    // =========================================================================
    // 13.5 Utility (1 action)
    // =========================================================================

    case 'get_gas_info': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('get_gas_info');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown GAS action: ${action}`
      });
  }
}
