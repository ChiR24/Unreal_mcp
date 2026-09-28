/**
 * Per-action JSON-schema property fragments for manage_gas.
 */
import type { JsonObject } from '../../../model.js';
import type { PropertyMap } from '../properties.js';
import { str, num } from '../../shared/schema-props.js';

const tags = (desc: string): JsonObject => ({
  type: 'array',
  items: { type: 'string' },
  description: desc,
});
const choice = (desc: string, values: readonly string[]): JsonObject => ({
  type: 'string',
  enum: [...values],
  description: desc,
});

export const GAS_P: PropertyMap = {
  componentName: str('AbilitySystemComponent name (defaults to AbilitySystemComponent).'),
  replicationMode: choice('ASC replication mode.', ['Full', 'Minimal', 'Mixed']),

  defaultValue: num('Initial value for the added attribute.'),
  baseValue: num('Base value for the attribute.'),

  abilityTags: tags('Gameplay tags granted to this ability.'),
  cancelAbilitiesWithTag: tags('Tags of abilities cancelled when this activates.'),
  blockAbilitiesWithTag: tags('Tags of abilities blocked while this is active.'),
  activationRequiredTags: tags('Tags required to activate this ability.'),
  activationBlockedTags: tags('Tags that block activation of this ability.'),
  costEffectPath: str('Canonical /Game path to the cost Gameplay Effect.'),
  cooldownEffectPath: str('Canonical /Game path to the cooldown Gameplay Effect.'),
  activationPolicy: choice('Net execution policy: where the ability activates.', [
    'LocalOnly', 'LocalPredicted', 'ServerOnly', 'ServerInitiated',
  ]),
  instancingPolicy: choice('How the ability is instanced.', [
    'NonInstanced', 'InstancedPerActor', 'InstancedPerExecution',
  ]),

  durationType: choice('Effect duration type.', ['Instant', 'Infinite', 'HasDuration']),
  period: num('Period in seconds for periodic effects.'),
  modifierOperation: choice('Modifier operation applied to the attribute.', [
    'Add', 'Multiply', 'Divide', 'Override',
  ]),
  modifierMagnitude: num('Magnitude of the modifier.'),
  targetAttribute: str('Target attribute captured by the modifier.'),
  modifierIndex: num('Zero-based index of the modifier to edit.'),
  magnitudeCalculationType: choice('How the modifier magnitude is calculated.', [
    'ScalableFloat', 'AttributeBased', 'SetByCaller', 'CustomCalculationClass',
  ]),
  setByCallerTag: str('Gameplay tag keying a SetByCaller magnitude.'),

  stackingType: choice('Stacking aggregation for the effect.', [
    'None', 'AggregateBySource', 'AggregateByTarget',
  ]),
  stackLimitCount: num('Maximum stack count.'),
  stackDurationRefreshPolicy: choice('When stack duration refreshes.', [
    'RefreshOnSuccessfulApplication', 'NeverRefresh',
  ]),
  stackPeriodResetPolicy: choice('When the stack period resets.', [
    'ResetOnSuccessfulApplication', 'NeverReset',
  ]),
  stackExpirationPolicy: choice('What happens when a stack expires.', [
    'ClearEntireStack', 'RemoveSingleStackAndRefreshDuration', 'RefreshDuration',
  ]),
  grantedTags: tags('Tags granted while the effect is active.'),
  applicationRequiredTags: tags('Tags required to apply this effect.'),
  removalTags: tags('Tags that cause this effect to be removed.'),
  immunityTags: tags('Tags that make a target immune to this effect.'),


  setPath: str('Canonical /Game ability set asset path.'),
  abilityClass: str('GameplayAbility class path; accepted wherever abilityPath is.'),
};
