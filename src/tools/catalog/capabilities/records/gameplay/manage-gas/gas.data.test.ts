/**
 * Per-action contract test for the manage_gas capability records.
 *
 * This locks the exact input contract of each of the 27 actions. It exists
 * because the parent union alone cannot catch a regression that replaces a real
 * per-action parameter with a generic placeholder: the union still "has some
 * property", so a union-level assertion stays green while every individual
 * action silently loses its contract. Asserting the full per-action sets is the
 * only check that fails on that.
 *
 * The expected sets below are the handler-valid contract: required fields come
 * from src/tools/handlers/gas/gas-action-routes.ts, optional fields from the
 * fields the matching native GAS handler reads.
 */
import { describe, expect, it } from 'vitest';
import { createCapabilityRecord } from '../../../parser.js';
import { GAS_RECORDS } from './gas.data.js';

type Contract = { readonly required: readonly string[]; readonly optional: readonly string[] };

const CONTRACTS: Readonly<Record<string, Contract>> = {
  add_ability_system_component: { required: ['blueprintPath'], optional: ['componentName'] },
  configure_asc: { required: ['blueprintPath'], optional: ['componentName', 'replicationMode'] },
  create_attribute_set: { required: ['name'], optional: ['path'] },
  add_attribute: { required: ['attributeSetPath', 'attributeName'], optional: ['attributeType', 'defaultValue'] },
  set_attribute_base_value: { required: ['attributeSetPath', 'attributeName'], optional: ['baseValue'] },
  set_attribute_clamping: { required: ['attributeSetPath', 'attributeName'], optional: ['clampMode', 'minValue', 'maxValue'] },
  create_gameplay_ability: { required: ['name'], optional: ['path'] },
  set_ability_tags: { required: ['abilityPath'], optional: ['abilityTags', 'cancelAbilitiesWithTag', 'blockAbilitiesWithTag', 'activationRequiredTags', 'activationBlockedTags'] },
  set_ability_costs: { required: ['abilityPath'], optional: ['costEffectPath'] },
  set_ability_cooldown: { required: ['abilityPath'], optional: ['cooldownEffectPath'] },
  set_ability_targeting: { required: ['abilityPath'], optional: ['targetingMode', 'targetRange', 'aoeRadius'] },
  add_ability_task: { required: ['abilityPath', 'taskType'], optional: [] },
  set_activation_policy: { required: ['abilityPath'], optional: ['activationPolicy'] },
  set_instancing_policy: { required: ['abilityPath'], optional: ['instancingPolicy'] },
  create_gameplay_effect: { required: ['name'], optional: ['path', 'durationType'] },
  set_effect_duration: { required: ['effectPath'], optional: ['durationType', 'duration', 'period'] },
  add_effect_modifier: { required: ['effectPath', 'attributeName'], optional: ['modifierOperation', 'modifierMagnitude', 'targetAttribute'] },
  set_modifier_magnitude: { required: ['effectPath'], optional: ['modifierIndex', 'modifierMagnitude', 'magnitudeCalculationType', 'setByCallerTag'] },
  add_effect_execution_calculation: { required: ['effectPath', 'calculationClass'], optional: [] },
  add_effect_cue: { required: ['effectPath', 'cueTag'], optional: [] },
  set_effect_stacking: { required: ['effectPath'], optional: ['stackingType', 'stackLimitCount', 'stackDurationRefreshPolicy', 'stackPeriodResetPolicy', 'stackExpirationPolicy'] },
  set_effect_tags: { required: ['effectPath'], optional: ['grantedTags', 'applicationRequiredTags', 'removalTags', 'immunityTags'] },
  create_gameplay_cue_notify: { required: ['name', 'cueType'], optional: ['path', 'cueTag'] },
  configure_cue_trigger: { required: ['cuePath'], optional: ['triggerType'] },
  set_cue_effects: { required: ['cuePath'], optional: ['particleSystemPath', 'soundPath', 'cameraShakePath', 'decalPath'] },
  add_tag_to_asset: { required: ['assetPath'], optional: ['tagName', 'tag'] },
  get_gas_info: { required: ['assetPath'], optional: [] },
};

const RECORDS = GAS_RECORDS.map((source) => createCapabilityRecord(source));
const byAction = new Map(RECORDS.map((record) => [String(record.legacyIds[0].action), record]));

describe('manage_gas capability records', () => {
  it('declares exactly 27 actions, each routed to manage_gas', () => {
    expect(RECORDS).toHaveLength(27);
    expect(new Set(byAction.keys()).size).toBe(27);
    for (const record of RECORDS) {
      expect(record.routing.parentTool).toBe('manage_gas');
      expect(record.routing.dispatchAction).toBe(record.legacyIds[0].action);
    }
  });

  it('covers every action named in the handler contract table', () => {
    expect([...byAction.keys()].sort()).toEqual(Object.keys(CONTRACTS).sort());
  });

  it.each(Object.keys(CONTRACTS))('declares the exact input contract for %s', (action) => {
    const record = byAction.get(action);
    const contract = CONTRACTS[action];
    if (!record || !contract) {
      throw new Error(`No record or contract declared for manage_gas action: ${action}`);
    }
    const schema = record.schemas.input;

    expect([...(schema.required ?? [])].sort()).toEqual([...contract.required].sort());
    expect(Object.keys(schema.properties ?? {}).sort())
      .toEqual([...contract.required, ...contract.optional].sort());
  });

  it('never reintroduces a generic placeholder in place of a real parameter', () => {
    // These are the names the union regression collapsed real parameters onto.
    // `properties`/`params` are catch-all objects no GAS handler reads; `magnitude`,
    // `modifierOp` and `value` are bridge-side spellings, not declared inputs.
    const placeholders = ['num_', 'radius', 'speed', 'properties', 'params', 'magnitude', 'modifierOp', 'value'];
    for (const record of RECORDS) {
      const declared = Object.keys(record.schemas.input.properties ?? {});
      expect(declared.filter((name) => placeholders.includes(name))).toEqual([]);
    }
  });
});
