type GASRouteKind = 'generic' | 'create_gameplay_effect' | 'add_tag_to_asset';

export type GASActionRoute = {
  readonly kind: GASRouteKind;
  readonly requiredFields: readonly string[];
  readonly blueprintPathParam?: string;
};

const ATTRIBUTE_SET_ACTIONS = [
  'add_attribute',
  'set_attribute_base_value',
  'set_attribute_clamping'
] as const;

const ABILITY_PATH_ACTIONS = [
  'set_ability_tags',
  'set_ability_costs',
  'set_ability_cooldown',
  'set_ability_targeting',
  'set_activation_policy',
  'set_instancing_policy'
] as const;

const EFFECT_PATH_ACTIONS = [
  'set_effect_duration',
  'set_modifier_magnitude',
  'set_effect_stacking',
  'set_effect_tags'
] as const;

const CUE_PATH_ACTIONS = [
  'configure_cue_trigger',
  'set_cue_effects'
] as const;

const GAS_ACTION_ROUTES: Readonly<Record<string, GASActionRoute>> = {
  add_ability_system_component: genericRoute(['blueprintPath']),
  configure_asc: genericRoute(['blueprintPath']),
  create_attribute_set: genericRoute(['name']),
  create_gameplay_ability: genericRoute(['name']),
  add_ability_task: genericRoute(['abilityPath', 'taskType'], 'abilityPath'),
  create_gameplay_effect: specialRoute('create_gameplay_effect', ['name']),
  add_effect_modifier: genericRoute(['effectPath', 'attributeName'], 'effectPath'),
  add_effect_execution_calculation: genericRoute(['effectPath', 'calculationClass'], 'effectPath'),
  add_effect_cue: genericRoute(['effectPath', 'cueTag'], 'effectPath'),
  create_gameplay_cue_notify: genericRoute(['name', 'cueType']),
  add_tag_to_asset: specialRoute('add_tag_to_asset', ['assetPath']),
  get_gas_info: genericRoute(['assetPath']),
  create_ability_set: genericRoute([]),
  add_ability: genericRoute(['setPath']),
  grant_ability: genericRoute([]),
  create_execution_calculation: genericRoute(['name']),
  ...Object.fromEntries(ATTRIBUTE_SET_ACTIONS.map((action) => [action, genericRoute(['attributeSetPath', 'attributeName'], 'attributeSetPath')])),
  ...Object.fromEntries(ABILITY_PATH_ACTIONS.map((action) => [action, genericRoute(['abilityPath'], 'abilityPath')])),
  ...Object.fromEntries(EFFECT_PATH_ACTIONS.map((action) => [action, genericRoute(['effectPath'], 'effectPath')])),
  ...Object.fromEntries(CUE_PATH_ACTIONS.map((action) => [action, genericRoute(['cuePath'], 'cuePath')]))
};

export function getGASActionRoute(action: string): GASActionRoute | undefined {
  return GAS_ACTION_ROUTES[action];
}

function genericRoute(requiredFields: readonly string[], blueprintPathParam?: string): GASActionRoute {
  return { kind: 'generic', requiredFields, blueprintPathParam };
}

function specialRoute(kind: Exclude<GASRouteKind, 'generic'>, requiredFields: readonly string[]): GASActionRoute {
  return { kind, requiredFields };
}
