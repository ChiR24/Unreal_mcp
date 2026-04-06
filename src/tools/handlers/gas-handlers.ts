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
import { sanitizeAssetName, sanitizePath } from '../../utils/validation.js';

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
  // The C++ handler expects 'blueprintPath' for most actions, so we map
  // the semantic parameter names (abilityPath, effectPath, etc.) to blueprintPath
  const sendRequest = async (subAction: string, blueprintPathParam?: string): Promise<Record<string, unknown>> => {
    const payload: Record<string, unknown> = { ...argsRecord, subAction };
    
    // Map semantic path parameters to blueprintPath for C++ handler compatibility
    // C++ HandleManageGASAction checks 'BlueprintPath' (case-sensitive in JSON)
    if (blueprintPathParam && typeof argsRecord[blueprintPathParam] === 'string') {
      payload.blueprintPath = argsRecord[blueprintPathParam];
    }
    
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
    return sendRequest('add_attribute', 'attributeSetPath');
  }

  case 'set_attribute_base_value': {
    requireNonEmptyString(argsRecord.attributeSetPath, 'attributeSetPath', 'Missing required parameter: attributeSetPath');
    requireNonEmptyString(argsRecord.attributeName, 'attributeName', 'Missing required parameter: attributeName');
    return sendRequest('set_attribute_base_value', 'attributeSetPath');
  }

  case 'set_attribute_clamping': {
    requireNonEmptyString(argsRecord.attributeSetPath, 'attributeSetPath', 'Missing required parameter: attributeSetPath');
    requireNonEmptyString(argsRecord.attributeName, 'attributeName', 'Missing required parameter: attributeName');
    return sendRequest('set_attribute_clamping', 'attributeSetPath');
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
    return sendRequest('set_ability_tags', 'abilityPath');
  }

  case 'set_ability_costs': {
    requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
    return sendRequest('set_ability_costs', 'abilityPath');
  }

  case 'set_ability_cooldown': {
    requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
    return sendRequest('set_ability_cooldown', 'abilityPath');
  }

  case 'set_ability_targeting': {
    requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
    return sendRequest('set_ability_targeting', 'abilityPath');
  }

  case 'add_ability_task': {
    requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
    requireNonEmptyString(argsRecord.taskType, 'taskType', 'Missing required parameter: taskType');
    return sendRequest('add_ability_task', 'abilityPath');
  }

  case 'set_activation_policy': {
    requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
    return sendRequest('set_activation_policy', 'abilityPath');
  }

  case 'set_instancing_policy': {
    requireNonEmptyString(argsRecord.abilityPath, 'abilityPath', 'Missing required parameter: abilityPath');
    return sendRequest('set_instancing_policy', 'abilityPath');
  }

    // =========================================================================
    // 13.3 Gameplay Effects (8 actions)
    // =========================================================================

    case 'create_gameplay_effect': {
      const requestedName = requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');

      const requestedPath = typeof argsRecord.path === 'string' && argsRecord.path.trim().length > 0
        ? argsRecord.path
        : '/Game';
      const normalizedPath = sanitizePath(requestedPath);
      const normalizedName = sanitizeAssetName(requestedName);

      // Pass normalized values to C++ to ensure consistency
      // C++ handles duplicate detection, type verification, and reusedExisting flag
      const payload = { ...argsRecord, name: normalizedName, path: normalizedPath, subAction: 'create_gameplay_effect' };
      const result = await executeAutomationRequest(
        tools,
        'manage_gas',
        payload as HandlerArgs,
        'Automation bridge not available for GAS action: create_gameplay_effect',
        { timeoutMs }
      );
      return cleanObject(result) as Record<string, unknown>;
    }

  case 'set_effect_duration': {
    requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
    return sendRequest('set_effect_duration', 'effectPath');
  }

  case 'add_effect_modifier': {
    requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
    requireNonEmptyString(argsRecord.attributeName, 'attributeName', 'Missing required parameter: attributeName');
    return sendRequest('add_effect_modifier', 'effectPath');
  }

  case 'set_modifier_magnitude': {
    requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
    return sendRequest('set_modifier_magnitude', 'effectPath');
  }

  case 'add_effect_execution_calculation': {
    requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
    requireNonEmptyString(argsRecord.calculationClass, 'calculationClass', 'Missing required parameter: calculationClass');
    return sendRequest('add_effect_execution_calculation', 'effectPath');
  }

  case 'add_effect_cue': {
    requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
    requireNonEmptyString(argsRecord.cueTag, 'cueTag', 'Missing required parameter: cueTag');
    return sendRequest('add_effect_cue', 'effectPath');
  }

  case 'set_effect_stacking': {
    requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
    return sendRequest('set_effect_stacking', 'effectPath');
  }

  case 'set_effect_tags': {
    requireNonEmptyString(argsRecord.effectPath, 'effectPath', 'Missing required parameter: effectPath');
    return sendRequest('set_effect_tags', 'effectPath');
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
    return sendRequest('configure_cue_trigger', 'cuePath');
  }

  case 'set_cue_effects': {
    requireNonEmptyString(argsRecord.cuePath, 'cuePath', 'Missing required parameter: cuePath');
    return sendRequest('set_cue_effects', 'cuePath');
  }

  case 'add_tag_to_asset': {
    requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
    // C++ expects 'tag' parameter, but test uses 'tagName' - map it
    const tagValue = typeof argsRecord.tagName === 'string' ? argsRecord.tagName : 
                     (typeof argsRecord.tag === 'string' ? argsRecord.tag : '');
    if (!tagValue) {
      return cleanObject({
        success: false,
        error: 'INVALID_ARGUMENT',
        message: 'Missing required parameter: tagName or tag'
      });
    }
    const payload: Record<string, unknown> = { ...argsRecord, tag: tagValue, subAction: 'add_tag_to_asset' };
    const result = await executeAutomationRequest(
      tools,
      'manage_gas',
      payload as HandlerArgs,
      'Automation bridge not available for GAS action: add_tag_to_asset',
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
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
