/**
 * Per-action JSON-schema property fragments for manage_gas.
 *
 * Sharded map private to the manage_gas record family: these fragments name the
 * exact parameters the GAS surface accepts, so each record declares its real
 * contract instead of a generic placeholder. Grounded in
 * src/tools/handlers/gas/gas-action-routes.ts (required fields),
 * gas-payload-normalization.ts (the TS-facing spelling of every aliased field),
 * and the native GAS domain under
 * plugins/McpAutomationBridge/.../Private/Domains/GAS/ (accepted fields and
 * defaults). Enum members mirror the native NormalizeGASToken inputs.
 *
 * Only the canonical TS-facing spelling is declared. Where the bridge also
 * accepts a shorter native alias (`operation`, `magnitude`, `value`,
 * `magnitudeType`, `stackLimit`, `targetingType`, `targetingRange`, `policy`,
 * `particleSystem`, `sound`, `cameraShake`), the normalization layer derives it
 * from the canonical name, so declaring the alias too would double-count one
 * parameter. The exceptions are the three the native side reads directly as
 * first-class fallbacks: `tag` (add_tag_to_asset reads it for `tagName`),
 * `abilityClass` (accepted wherever `abilityPath` is), and `actorPath`
 * (accepted wherever `blueprintPath` is) — each stays declared here.
 */
import type { JsonObject } from '../../../index.js';
import type { PropertyMap } from '../properties.js';

const str = (desc: string): JsonObject => ({ type: 'string', description: desc });
const num = (desc: string): JsonObject => ({ type: 'number', description: desc });
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

  attributeType: choice('Predefined attribute type or Custom.', [
    'Health', 'MaxHealth', 'Mana', 'MaxMana', 'Stamina', 'MaxStamina',
    'Damage', 'Armor', 'AttackPower', 'MoveSpeed', 'Custom',
  ]),
  defaultValue: num('Initial value for the added attribute.'),
  baseValue: num('Base value for the attribute.'),
  clampMode: choice('Attribute clamping mode.', ['None', 'Min', 'Max', 'MinMax']),

  abilityTags: tags('Gameplay tags granted to this ability.'),
  cancelAbilitiesWithTag: tags('Tags of abilities cancelled when this activates.'),
  blockAbilitiesWithTag: tags('Tags of abilities blocked while this is active.'),
  activationRequiredTags: tags('Tags required to activate this ability.'),
  activationBlockedTags: tags('Tags that block activation of this ability.'),
  costEffectPath: str('Canonical /Game path to the cost Gameplay Effect.'),
  cooldownEffectPath: str('Canonical /Game path to the cooldown Gameplay Effect.'),
  targetingMode: choice('Targeting mode for the ability.', [
    'None', 'SingleTarget', 'AOE', 'Directional', 'Ground', 'ActorPlacement',
  ]),
  targetRange: num('Maximum targeting range in world units.'),
  aoeRadius: num('Area-of-effect radius in world units.'),
  activationPolicy: choice('When the ability activates.', [
    'OnInputPressed', 'WhileInputActive', 'OnSpawn', 'OnGiven',
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

  triggerType: choice('When the gameplay cue triggers.', [
    'OnActive', 'WhileActive', 'Executed', 'OnRemove',
  ]),
  particleSystemPath: str('Canonical /Game particle system asset path.'),
  soundPath: str('Canonical /Game sound asset path.'),
  cameraShakePath: str('Canonical /Game camera shake asset path.'),
  decalPath: str('Canonical /Game decal material asset path.'),

  setPath: str('Canonical /Game ability set asset path.'),
  setName: str('Display name recorded on the ability set.'),
  abilityClass: str('GameplayAbility class path; accepted wherever abilityPath is.'),
  actorPath: str('Canonical /Game actor Blueprint path; accepted wherever blueprintPath is.'),
  abilityLevel: num('Level the ability is granted at.'),
  inputID: num('Input id bound to the granted ability; -1 leaves it unbound.'),
};
