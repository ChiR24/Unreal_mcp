// Fold specs for manage_gas. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_GAS_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_gas_asset', selector: 'kind',
    summary: 'Create a Gameplay Ability System asset: gameplay ability, gameplay effect, attribute set, ability set, gameplay cue notify, execution calculation.',
    topics: ['gameplay ability', 'gameplay effect', 'attribute set', 'ability set', 'gameplay cue', 'execution calculation'],
    members: {
      gameplay_ability: 'create_gameplay_ability', gameplay_effect: 'create_gameplay_effect', attribute_set: 'create_attribute_set', ability_set: 'create_ability_set',
      gameplay_cue_notify: 'create_gameplay_cue_notify', execution_calculation: 'create_execution_calculation',
    },
  },
  {
    primary: 'configure_ability', selector: 'setting',
    summary: 'Configure a gameplay ability: tags, cooldown, costs, targeting, activation and instancing policies, tasks; add it to a set or grant it to an actor.',
    topics: ['ability tags', 'ability cooldown', 'ability cost', 'ability targeting', 'activation policy', 'grant ability', 'ability task'],
    members: {
      tags: 'set_ability_tags', cooldown: 'set_ability_cooldown', costs: 'set_ability_costs', targeting: 'set_ability_targeting', activation_policy: 'set_activation_policy',
      instancing_policy: 'set_instancing_policy', add_task: 'add_ability_task', add_to_set: 'add_ability', grant: 'grant_ability',
    },
  },
  {
    primary: 'configure_gameplay_effect', selector: 'setting',
    summary: 'Configure a gameplay effect: duration, stacking, tags, modifiers and magnitudes, execution calculations, cues.',
    topics: ['effect duration', 'effect stacking', 'effect modifier', 'effect tags', 'gameplay cue'],
    members: {
      duration: 'set_effect_duration', stacking: 'set_effect_stacking', tags: 'set_effect_tags', add_modifier: 'add_effect_modifier', modifier_magnitude: 'set_modifier_magnitude',
      add_execution_calculation: 'add_effect_execution_calculation', add_cue: 'add_effect_cue',
    },
  },
  {
    primary: 'configure_attribute_set', selector: 'setting',
    summary: 'Configure an attribute set: add an attribute, set its base value or clamping.',
    topics: ['attribute', 'attribute base value', 'attribute clamping'],
    members: { add_attribute: 'add_attribute', base_value: 'set_attribute_base_value', clamping: 'set_attribute_clamping' },
  },
  {
    primary: 'configure_asc', selector: 'setting',
    summary: 'Add an Ability System Component to a Blueprint, or configure its replication mode.',
    topics: ['ability system component', 'asc replication'],
    members: { configure: 'configure_asc', add_component: 'add_ability_system_component' },
  },
  {
    primary: 'configure_gameplay_cue', selector: 'setting',
    summary: 'Configure a gameplay cue notify: trigger type or its effects (particles, sound, camera shake, decal).',
    members: { trigger: 'configure_cue_trigger', effects: 'set_cue_effects' },
  },
];
