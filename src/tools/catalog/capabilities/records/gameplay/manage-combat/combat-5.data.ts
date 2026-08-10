/**
 * manage_combat records — part 5 of 5 (introspection, the damage-type/hit-
 * detection aliases, and the damage/heal/shield/armor authoring actions).
 *
 * Parameters mirror the field literals read by
 * McpAutomationBridge_CombatHandlersInfo.cpp, DamageTypes.cpp,
 * DamageExecution.cpp, HealthRuntime.cpp and DefenseRuntime.cpp.
 *
 * apply_damage / heal / create_shield / modify_armor are Blueprint AUTHORING
 * actions despite their runtime-sounding names: the native handlers load the
 * Blueprint, call AddBlueprintVariableCombat, compile and McpSafeAssetSave. No
 * live PIE actor is mutated, so they stay editorStates 'edit' like every other
 * action on this tool.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { C } from './combat-properties.js';

const T = 'manage_combat';
const F = 'combat';
const WEAPON = '/Game/BP_Rifle';
const CHAR = '/Game/BP_Char';

export const COMBAT_5: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.get_combat_info`, action: 'get_combat_info', family: F,
    summary: 'Read combat configuration back from a Blueprint.', whenToUse: ['Inspect an authored weapon or projectile.'], whenNotToUse: ['Mutating an asset; use the configure actions.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath }, required: ['action', 'blueprintPath'],
    effect: 'read', latency: 'instant', resources: 'low',
    outputProps: { combatInfo: C.combatInfo }, outputRequired: [],
    exampleInput: { action: 'get_combat_info', blueprintPath: WEAPON }, exampleOutput: { success: true, message: 'Combat info', combatInfo: { parentClass: 'Actor', hasWeaponMesh: true } } }),
  buildRecord({ parentTool: T, id: `${T}.setup_damage_type`, action: 'setup_damage_type', family: F,
    summary: 'Create a DamageType Blueprint and return its path.', whenToUse: ['A damage class is needed by name and folder.'], whenNotToUse: ['Use configure_damage_execution to tune multipliers.'],
    inputProps: { action: P.action, name: P.name, path: P.path }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low',
    outputProps: { damageTypePath: P.damageTypePath }, outputRequired: ['damageTypePath'],
    exampleInput: { action: 'setup_damage_type', name: 'DT_Ice', path: '/Game/Damage' }, exampleOutput: { success: true, message: 'Damage type set up', damageTypePath: '/Game/Damage/DT_Ice.DT_Ice' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_hit_detection`, action: 'configure_hit_detection', family: F,
    summary: 'Retune hitbox shape and damage multiplier on a Blueprint.', whenToUse: ['An existing hitbox must change shape or scaling.'], whenNotToUse: ['Use setup_hitbox_component to add a bone-bound volume.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, hitboxType: C.hitboxType, damageMultiplier: C.damageMultiplier }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { hitboxType: C.hitboxType }, outputRequired: [],
    exampleInput: { action: 'configure_hit_detection', blueprintPath: WEAPON, hitboxType: 'Sphere', damageMultiplier: 1.5 }, exampleOutput: { success: true, message: 'Hit detection configured', hitboxType: 'Sphere' } }),
  buildRecord({ parentTool: T, id: `${T}.get_combat_stats`, action: 'get_combat_stats', family: F,
    summary: 'Read combat stats back from a Blueprint.', whenToUse: ['Inspect authored weapon stats.'], whenNotToUse: ['Use set_weapon_stats to change them.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath }, required: ['action', 'blueprintPath'],
    effect: 'read', latency: 'instant', resources: 'low',
    outputProps: { combatInfo: C.combatInfo }, outputRequired: [],
    exampleInput: { action: 'get_combat_stats', blueprintPath: WEAPON }, exampleOutput: { success: true, message: 'Combat stats', combatInfo: { parentClass: 'Actor', baseDamage: 45 } } }),
  buildRecord({ parentTool: T, id: `${T}.create_damage_effect`, action: 'create_damage_effect', family: F,
    summary: 'Create a damage-over-time effect Blueprint asset.', whenToUse: ['A reusable damage effect asset is needed.'], whenNotToUse: ['Use apply_damage to author a one-shot damage value.'],
    inputProps: { action: P.action, name: P.name, path: P.path, duration: P.duration, damagePerSecond: C.damagePerSecond, effectType: C.effectType }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low',
    outputProps: { blueprintPath: P.blueprintPath, duration: P.duration }, outputRequired: ['blueprintPath'],
    exampleInput: { action: 'create_damage_effect', name: 'DE_Burn', path: '/Game/Damage', duration: 8, damagePerSecond: 15, effectType: 'DamageOverTime' }, exampleOutput: { success: true, message: 'Damage effect created', blueprintPath: '/Game/Damage/DE_Burn', duration: 8 } }),
  buildRecord({ parentTool: T, id: `${T}.apply_damage`, action: 'apply_damage', family: F,
    summary: 'Author damage application data on a Blueprint asset (amount/type). Does NOT mutate a live PIE actor.',
    whenToUse: ['Configure how much/what-type damage a Blueprint applies.'], whenNotToUse: ['Run live damage in PIE (handled by gameplay code).'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, damageAmount: C.damageAmount, damageType: C.damageType }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    normalizationClass: 'F_OBSOLETE_VERSION_SPECIFIC', normalizationRationale: 'Authoring of damage config on a Blueprint asset; distinct from heal/shield/armor (combat-handlers.ts apply_damage branch).',
    outputProps: { damageAmount: C.damageAmount, damageType: C.damageType }, outputRequired: ['damageAmount'],
    exampleInput: { action: 'apply_damage', blueprintPath: WEAPON, damageAmount: 33, damageType: 'Fire' }, exampleOutput: { success: true, message: 'Damage config applied', damageAmount: 33, damageType: 'Fire' } }),
  buildRecord({ parentTool: T, id: `${T}.heal`, action: 'heal', family: F,
    summary: 'Author healing data on a Blueprint asset (amount). Does NOT restore a live PIE actor health.',
    whenToUse: ['Configure how much a Blueprint heals.'], whenNotToUse: ['Run live healing in PIE (handled by gameplay code).'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, healAmount: C.healAmount, maxHealth: C.maxHealth }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    normalizationClass: 'F_OBSOLETE_VERSION_SPECIFIC', normalizationRationale: 'Authoring of heal config on a Blueprint asset; distinct from apply_damage/shield/armor (combat-handlers.ts heal branch).',
    outputProps: { healAmount: C.healAmount, maxHealth: C.maxHealth }, outputRequired: [],
    exampleInput: { action: 'heal', blueprintPath: CHAR, healAmount: 25, maxHealth: 125 }, exampleOutput: { success: true, message: 'Heal config applied', healAmount: 25, maxHealth: 125 } }),
  buildRecord({ parentTool: T, id: `${T}.create_shield`, action: 'create_shield', family: F,
    summary: 'Author a shield component/data on a Blueprint asset. Distinct from armor (absorbing vs mitigating).',
    whenToUse: ['Configure a shield on a Blueprint.'], whenNotToUse: ['Use modify_armor for damage mitigation.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, shieldAmount: C.shieldAmount, maxShield: C.maxShield, shieldRegenRate: C.shieldRegenRate, shieldRegenDelay: C.shieldRegenDelay }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    normalizationClass: 'F_OBSOLETE_VERSION_SPECIFIC', normalizationRationale: 'Authoring of shield config on a Blueprint asset; distinct from apply_damage/heal/armor (combat-handlers.ts create_shield branch).',
    outputProps: { shieldAmount: C.shieldAmount, maxShield: C.maxShield, shieldRegenDelay: C.shieldRegenDelay }, outputRequired: [],
    exampleInput: { action: 'create_shield', blueprintPath: CHAR, shieldAmount: 60, maxShield: 120, shieldRegenRate: 6, shieldRegenDelay: 3 }, exampleOutput: { success: true, message: 'Shield config created', shieldAmount: 60, maxShield: 120 } }),
  buildRecord({ parentTool: T, id: `${T}.modify_armor`, action: 'modify_armor', family: F,
    summary: 'Author armor values on a Blueprint asset (mitigation). Distinct from shield (armor reduces, shield absorbs).',
    whenToUse: ['Configure armor mitigation on a Blueprint.'], whenNotToUse: ['Use create_shield for absorption.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, armorValue: C.armorValue, damageReduction: C.damageReduction }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    normalizationClass: 'F_OBSOLETE_VERSION_SPECIFIC', normalizationRationale: 'Authoring of armor config on a Blueprint asset; distinct from apply_damage/heal/shield (combat-handlers.ts modify_armor branch).',
    outputProps: { armorValue: C.armorValue, damageReduction: C.damageReduction }, outputRequired: ['armorValue'],
    exampleInput: { action: 'modify_armor', blueprintPath: CHAR, armorValue: 75, damageReduction: 0.25 }, exampleOutput: { success: true, message: 'Armor config modified', armorValue: 75, damageReduction: 0.25 } }),
];
