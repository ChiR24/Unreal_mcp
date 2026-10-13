/**
 * Gameplay behaviours written into an Actor Blueprint from the plugin's recipe files
 * (McpBlueprintBehaviour::Author): health, damage on touch, pickups and sprint. Folded
 * into blueprint.add_behaviour (selector behaviour). Every one is all or nothing,
 * re-runnable (a second run replaces the first), compiled and saved.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'behaviour';
const DOMAIN = 'blueprint';

const BEHAVIOUR_OUTPUT = {
  compiled: { type: 'boolean', description: 'The Blueprint compiled with the behaviour.' },
  saved: { type: 'boolean', description: 'The Blueprint was saved.' },
  variables: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'The variables it uses: {name, status added, redefaulted or kept}.' },
  functions: { type: 'array', items: { type: 'string' }, description: 'Functions it added, which other logic can call.' },
  dispatchers: { type: 'array', items: { type: 'string' }, description: 'Event dispatchers it added, which other logic can bind.' },
} as const;

const OVERLAP_NOTE = 'It fires on overlap, so the actor needs a component that overlaps pawns (a sphere or box with the Trigger or OverlapAllDynamic profile).';

export const BEHAVIOURS_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.add_health',
    action: 'add_health',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['health', 'take damage', 'player health', 'enemy health', 'die when health is zero'],
    summary: 'Give an actor health: Health and MaxHealth variables, damage from any source (ApplyDamage, a pain causing volume, a projectile) lowers it and negative damage heals, a Heal function, OnHealthChanged and OnDied dispatchers, and optional destroy on death.',
    whenToUse: ['A player, enemy or breakable object must take damage and die; bind OnHealthChanged to a health bar.'],
    whenNotToUse: ['The project uses the Gameplay Ability System for attributes (use manage_gas).'],
    inputProps: {
      blueprintPath: P.blueprintPath,
      maxHealth: { type: 'number', description: 'Starting and largest health (default 100).' },
      destroyOnDeath: { type: 'boolean', description: 'Destroy the actor when its health reaches 0 (default false; OnDied fires either way).' },
    },
    required: ['blueprintPath'],
    outputProps: BEHAVIOUR_OUTPUT,
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'medium',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_health', blueprintPath: '/Game/Blueprints/BP_Enemy', maxHealth: 50, destroyOnDeath: true },
  }),
  buildRecord({
    id: 'blueprint.add_damage_on_touch',
    action: 'add_damage_on_touch',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['damage on touch', 'hazard', 'spikes'],
    summary: `Make an actor damage whatever touches it: ApplyDamage with TouchDamage on each overlap, which an actor with health loses (a negative amount heals). ${OVERLAP_NOTE}`,
    whenToUse: ['Spikes, lava, an enemy body or a fire must hurt what touches it.'],
    whenNotToUse: ['A whole area must hurt or kill (use manage_level_structure create_volume with a pain causing or kill Z volume).'],
    inputProps: {
      blueprintPath: P.blueprintPath,
      damage: { type: 'number', description: 'Damage on each touch (default 10; negative heals).' },
    },
    required: ['blueprintPath'],
    outputProps: BEHAVIOUR_OUTPUT,
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'medium',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_damage_on_touch', blueprintPath: '/Game/Blueprints/BP_Spikes', damage: 25 },
  }),
  buildRecord({
    id: 'blueprint.add_pickup',
    action: 'add_pickup',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['pickup', 'health pickup', 'collectible', 'coin', 'collect coins', 'item pickup'],
    summary: `Make an actor a pickup: a pawn that touches it collects it, which heals the pawn (heal) or adds to a number variable of the collector class (coins, keys, ammo), plays a sound and destroys the pickup. ${OVERLAP_NOTE}`,
    whenToUse: ['A health pack, coin, key or ammo box must be collected by walking into it.'],
    whenNotToUse: ['The item must go into an inventory system (use manage_inventory).'],
    inputProps: {
      blueprintPath: P.blueprintPath,
      heal: { type: 'number', description: 'Health given to the pawn that collects it; it reaches an actor with health (add_health) as negative damage.' },
      collectorClass: { type: 'string', description: 'Instead of heal: the class that collects it (a Blueprint path such as /Game/Blueprints/BP_Player), whose variableName goes up by amount.' },
      variableName: { type: 'string', description: 'With collectorClass: the collector\'s Int or Float variable (Coins).' },
      amount: { type: 'number', description: 'With collectorClass: added each time (default 1).' },
      soundPath: { type: 'string', description: 'Sound asset played where the pickup was.' },
    },
    required: ['blueprintPath'],
    outputProps: BEHAVIOUR_OUTPUT,
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'medium',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_pickup', blueprintPath: '/Game/Blueprints/BP_Coin', collectorClass: '/Game/Blueprints/BP_Player', variableName: 'Coins', amount: 1 },
  }),
  buildRecord({
    id: 'blueprint.add_sprint',
    action: 'add_sprint',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['sprint', 'hold shift to sprint', 'sprint key', 'faster while a key is held'],
    summary: 'Give a Character a sprint: while the key is held its MaxWalkSpeed is sprintSpeed, and on release the walk speed it had at BeginPlay comes back. The key gets an Input Action and a mapping, registered when the character is possessed.',
    whenToUse: ['The player must run faster while holding a key.'],
    whenNotToUse: ['The walk speed itself must change (use manage_character configure_character walkSpeed).'],
    inputProps: {
      blueprintPath: P.blueprintPath,
      sprintSpeed: { type: 'number', description: 'MaxWalkSpeed while sprinting, cm/s (default 1000).' },
      key: { type: 'string', description: 'Key held to sprint (default LeftShift; Gamepad_LeftThumbstick and other key names work).' },
      inputActionPath: { type: 'string', description: 'An Input Action to use instead of a new IA_Sprint next to the Blueprint.' },
      mappingContextPath: { type: 'string', description: 'The Input Mapping Context the key goes in (the project\'s own); default the one the character already registers, else /Game/Input/IMC_MCP.' },
    },
    required: ['blueprintPath'],
    outputProps: BEHAVIOUR_OUTPUT,
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'medium',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_sprint', blueprintPath: '/Game/Blueprints/BP_Player', sprintSpeed: 900 },
  }),
];
