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
import { createSubActionDispatcher, requireNonEmptyString } from './common-handlers.js';
import { sanitizeAssetName, normalizeAndSanitizeAssetPath } from '../../utils/validation.js';

function toLowerSnakeCase(value: string): string {
  return value
    .replace(/([a-z0-9])([A-Z])/g, '$1_$2')
    .replace(/[\s-]+/g, '_')
    .toLowerCase();
}

function copyIfString(payload: Record<string, unknown>, fromKey: string, toKey: string): void {
  if (typeof payload[toKey] === 'string') return;

  const value = payload[fromKey];
  if (typeof value === 'string' && value.trim().length > 0) {
    payload[toKey] = value;
  }
}

function copyIfNumber(payload: Record<string, unknown>, fromKey: string, toKey: string): void {
  if (typeof payload[toKey] === 'number') return;

  const value = payload[fromKey];
  if (typeof value === 'number') {
    payload[toKey] = value;
  }
}

function copyIfArray(payload: Record<string, unknown>, fromKey: string, toKey: string): void {
  if (Array.isArray(payload[toKey])) return;

  const value = payload[fromKey];
  if (Array.isArray(value)) {
    payload[toKey] = value;
  }
}

function normalizeEnumAlias(payload: Record<string, unknown>, key: string): void {
  const value = payload[key];
  if (typeof value === 'string' && value.trim().length > 0) {
    payload[key] = toLowerSnakeCase(value);
  }
}

function normalizeCueTrigger(value: string): string {
  switch (value) {
    case 'OnActive':
      return 'on_active';
    case 'WhileActive':
      return 'while_active';
    case 'Executed':
      return 'on_execute';
    case 'OnRemove':
      return 'on_remove';
    default:
      return toLowerSnakeCase(value);
  }
}

function normalizeGASPayloadForBridge(payload: Record<string, unknown>, subAction: string): Record<string, unknown> {
  switch (subAction) {
    case 'configure_asc':
      normalizeEnumAlias(payload, 'replicationMode');
      break;

    case 'set_ability_tags':
      copyIfArray(payload, 'cancelAbilitiesWithTag', 'cancelAbilitiesWithTags');
      copyIfArray(payload, 'blockAbilitiesWithTag', 'blockAbilitiesWithTags');
      break;

    case 'set_ability_targeting':
      copyIfString(payload, 'targetingMode', 'targetingType');
      copyIfNumber(payload, 'targetRange', 'targetingRange');
      normalizeEnumAlias(payload, 'targetingType');
      break;

    case 'set_activation_policy':
      copyIfString(payload, 'activationPolicy', 'policy');
      normalizeEnumAlias(payload, 'policy');
      break;

    case 'set_instancing_policy':
      copyIfString(payload, 'instancingPolicy', 'policy');
      normalizeEnumAlias(payload, 'policy');
      break;

    case 'create_gameplay_effect':
    case 'set_effect_duration':
      normalizeEnumAlias(payload, 'durationType');
      break;

    case 'add_effect_modifier':
      copyIfString(payload, 'modifierOperation', 'operation');
      copyIfNumber(payload, 'modifierMagnitude', 'magnitude');
      normalizeEnumAlias(payload, 'operation');
      break;

    case 'set_modifier_magnitude':
      copyIfString(payload, 'magnitudeCalculationType', 'magnitudeType');
      copyIfNumber(payload, 'modifierMagnitude', 'value');
      normalizeEnumAlias(payload, 'magnitudeType');
      break;

    case 'set_effect_stacking':
      copyIfNumber(payload, 'stackLimitCount', 'stackLimit');
      normalizeEnumAlias(payload, 'stackingType');
      normalizeEnumAlias(payload, 'stackDurationRefreshPolicy');
      normalizeEnumAlias(payload, 'stackPeriodResetPolicy');
      normalizeEnumAlias(payload, 'stackExpirationPolicy');
      break;

    case 'configure_cue_trigger': {
      const triggerType = payload.triggerType;
      if (typeof triggerType === 'string') {
        payload.triggerType = normalizeCueTrigger(triggerType);
      }
      break;
    }

    case 'set_cue_effects':
      copyIfString(payload, 'particleSystemPath', 'particleSystem');
      copyIfString(payload, 'soundPath', 'sound');
      copyIfString(payload, 'cameraShakePath', 'cameraShake');
      copyIfString(payload, 'decalPath', 'decal');
      break;

    default:
      break;
  }

  return payload;
}


/**
 * Handles all GAS actions for the manage_gas tool.
 */
export async function handleGASTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { argsRecord, sendRequest: sendSubAction } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_gas',
    domainName: 'GAS',
    pathFields: [
      'blueprintPath',
      'attributeSetPath',
      'abilityPath',
      'effectPath',
      'cuePath',
      'assetPath',
      'costEffectPath',
      'cooldownEffectPath',
      'decalPath',
      'path'
    ],
    preparePayload: normalizeGASPayloadForBridge
  });

  const sendRequest = async (subAction: string, blueprintPathParam?: string): Promise<Record<string, unknown>> => {
    const extraPayload = blueprintPathParam && typeof argsRecord[blueprintPathParam] === 'string'
      ? { blueprintPath: argsRecord[blueprintPathParam] }
      : undefined;
    return sendSubAction(subAction, extraPayload);
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
      const normalizedPath = normalizeAndSanitizeAssetPath(requestedPath);
      const normalizedName = sanitizeAssetName(requestedName);

      return sendSubAction('create_gameplay_effect', { name: normalizedName, path: normalizedPath });
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
    return sendSubAction('add_tag_to_asset', { tag: tagValue });
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
