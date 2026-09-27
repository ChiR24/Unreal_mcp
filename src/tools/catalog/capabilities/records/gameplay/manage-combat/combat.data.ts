/**
 * manage_combat records: weapon base and firing modes, projectiles, damage
 * types, damage execution, hitboxes, attachments and introspection.
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
import type { CapabilityRecordSource } from '../../../model.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { C } from './combat-properties.js';

const T = 'manage_combat';
const F = 'combat';
const WEAPON = '/Game/BP_Rifle';
const PROJECTILE = '/Game/BP_Bullet';

export const COMBAT_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_weapon_blueprint`, action: 'create_weapon_blueprint', family: F,
    topics: ['weapon', 'gun', 'weapon blueprint', 'firearm', 'new weapon'],
    summary: 'Create a weapon Blueprint asset with its base stats.', whenToUse: ['A new weapon actor must be authored.'], whenNotToUse: ['Use create_projectile_blueprint for the projectile it fires.'],
    inputProps: { name: P.name, path: P.path, baseDamage: C.baseDamage, fireRate: C.fireRate, range: C.range, spread: C.spread }, required: ['name'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { blueprintPath: P.blueprintPath, baseDamage: C.baseDamage, fireRate: C.fireRate }, outputRequired: ['blueprintPath'],
    exampleInput: { action: 'create_weapon_blueprint', name: 'BP_Rifle', path: '/Game/Weapons', baseDamage: 37, fireRate: 480 }, exampleOutput: { success: true, message: 'Weapon Blueprint created', blueprintPath: WEAPON, baseDamage: 37, fireRate: 480 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_weapon_mesh`, action: 'configure_weapon_mesh', family: F,
    summary: 'Assign the weapon skeletal/static mesh.', whenToUse: ['The weapon mesh must change.'], whenNotToUse: ['Use configure_weapon_sockets for attach points.'],
    inputProps: { blueprintPath: P.blueprintPath, weaponMeshPath: C.weaponMeshPath }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { meshPath: P.meshPath }, outputRequired: [],
    exampleInput: { action: 'configure_weapon_mesh', blueprintPath: WEAPON, weaponMeshPath: '/Engine/BasicShapes/Cube.Cube' }, exampleOutput: { success: true, message: 'Weapon mesh configured', meshPath: '/Engine/BasicShapes/Cube.Cube' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_projectile`, action: 'configure_projectile', family: F,
    summary: 'Point the weapon at a projectile class and launch speed.', whenToUse: ['The weapon spawns projectiles.'], whenNotToUse: ['Use configure_hitscan for instant hits.'],
    inputProps: { blueprintPath: P.blueprintPath, projectileClass: C.projectileClass, projectileSpeed: C.projectileSpeed }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { projectileSpeed: C.projectileSpeed }, outputRequired: [],
    exampleInput: { action: 'configure_projectile', blueprintPath: WEAPON, projectileClass: '/Script/Engine.Actor', projectileSpeed: 4500 }, exampleOutput: { success: true, message: 'Projectile configured', projectileSpeed: 4500 } }),
  buildRecord({ parentTool: T, id: `${T}.create_projectile_blueprint`, action: 'create_projectile_blueprint', family: F,
    summary: 'Create a projectile Blueprint with movement and collision.', whenToUse: ['A projectile actor must be authored.'], whenNotToUse: ['Use create_weapon_blueprint for the weapon that fires it.'],
    inputProps: { name: P.name, path: P.path, projectileSpeed: C.projectileSpeed, projectileGravityScale: C.projectileGravityScale, collisionRadius: C.collisionRadius, projectileMeshPath: C.projectileMeshPath }, required: ['name'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { blueprintPath: P.blueprintPath, projectileMeshPath: C.projectileMeshPath }, outputRequired: ['blueprintPath'],
    exampleInput: { action: 'create_projectile_blueprint', name: 'BP_Bullet', path: '/Game/Weapons', projectileSpeed: 6000, projectileGravityScale: 0, collisionRadius: 8 }, exampleOutput: { success: true, message: 'Projectile Blueprint created', blueprintPath: PROJECTILE } }),
  buildRecord({ parentTool: T, id: `${T}.configure_projectile_movement`, action: 'configure_projectile_movement', family: F,
    summary: 'Configure projectile speed, gravity scale and lifespan.', whenToUse: ['Projectile flight must change.'], whenNotToUse: ['Use configure_projectile_collision for impact response.'],
    inputProps: { blueprintPath: P.blueprintPath, projectileSpeed: C.projectileSpeed, projectileGravityScale: C.projectileGravityScale, projectileLifespan: C.projectileLifespan }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_projectile_movement', blueprintPath: PROJECTILE, projectileSpeed: 6500, projectileGravityScale: 0.5, projectileLifespan: 4 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_projectile_collision`, action: 'configure_projectile_collision', family: F,
    summary: 'Configure projectile collision radius and bounce response.', whenToUse: ['Impact or bounce behaviour must change.'], whenNotToUse: ['Use configure_projectile_movement for flight.'],
    inputProps: { blueprintPath: P.blueprintPath, collisionRadius: C.collisionRadius, bounceEnabled: C.bounceEnabled, bounceVelocityRatio: C.bounceVelocityRatio }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_projectile_collision', blueprintPath: PROJECTILE, collisionRadius: 12, bounceEnabled: true, bounceVelocityRatio: 0.5 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_projectile_homing`, action: 'configure_projectile_homing', family: F,
    summary: 'Configure projectile homing and its turn acceleration.', whenToUse: ['The projectile should track a target.'], whenNotToUse: ['Use configure_projectile_movement for ballistic flight.'],
    inputProps: { blueprintPath: P.blueprintPath, homingEnabled: C.homingEnabled, homingAcceleration: C.homingAcceleration }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_projectile_homing', blueprintPath: PROJECTILE, homingEnabled: true, homingAcceleration: 15000 } }),
  buildRecord({ parentTool: T, id: `${T}.create_damage_type`, action: 'create_damage_type', family: F,
    summary: 'Create a DamageType Blueprint asset.', whenToUse: ['A custom damage classification is needed.'], whenNotToUse: ['Use configure_damage_execution to tune multipliers.'],
    inputProps: { name: P.name, path: P.path }, required: ['name'],
    effect: 'write', latency: 'interactive', resources: 'low',
    outputProps: { damageTypePath: P.damageTypePath }, outputRequired: ['damageTypePath'],
    exampleInput: { action: 'create_damage_type', name: 'DT_Fire', path: '/Game/Damage' }, exampleOutput: { success: true, message: 'DamageType created', damageTypePath: '/Game/Damage/DT_Fire.DT_Fire' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_hitbox_component`, action: 'setup_hitbox_component', family: F,
    summary: 'Add a shaped hitbox component bound to a bone.', whenToUse: ['A per-bone damage volume is needed.'], whenNotToUse: ['Use configure_hit_detection to retune an existing hitbox.'],
    inputProps: { blueprintPath: P.blueprintPath, hitboxBoneName: C.hitboxBoneName, hitboxType: C.hitboxType, hitboxSize: C.hitboxSize, isDamageZoneHead: C.isDamageZoneHead, damageMultiplier: C.damageMultiplier }, required: ['blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'low',
    outputProps: { hitboxType: C.hitboxType, hitboxSize: C.hitboxSize }, outputRequired: [],
    exampleInput: { action: 'setup_hitbox_component', blueprintPath: WEAPON, hitboxBoneName: 'spine_03', hitboxType: 'Box', hitboxSize: { extent: { x: 12, y: 18, z: 22 } }, isDamageZoneHead: true, damageMultiplier: 2 }, exampleOutput: { success: true, message: 'Hitbox component set up', hitboxType: 'Box' } }),
  buildRecord({ parentTool: T, id: `${T}.setup_attachment_system`, action: 'setup_attachment_system', family: F,
    summary: 'Create attachment slots and their scene components.', whenToUse: ['Modular optics or grips are needed.'], whenNotToUse: ['Use setup_weapon_switching to swap whole weapons.'],
    inputProps: { blueprintPath: P.blueprintPath, attachmentSlots: C.attachmentSlots }, required: ['blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { attachmentSlots: C.attachmentSlots }, outputRequired: [],
    exampleInput: { action: 'setup_attachment_system', blueprintPath: WEAPON, attachmentSlots: [{ slotName: 'Optic', socketName: 'OpticSocket', allowedTypes: ['Scope'] }] } }),
  buildRecord({ parentTool: T, id: `${T}.get_combat_info`, action: 'get_combat_info', family: F,
    summary: 'Read combat configuration back from a Blueprint.', whenToUse: ['Inspect an authored weapon or projectile.'], whenNotToUse: ['Mutating an asset; use the configure actions.'],
    inputProps: { blueprintPath: P.blueprintPath }, required: ['blueprintPath'],
    effect: 'read', latency: 'instant', resources: 'low',
    outputProps: { combatInfo: C.combatInfo }, outputRequired: [],
    exampleInput: { action: 'get_combat_info', blueprintPath: WEAPON }, exampleOutput: { success: true, message: 'Combat info', combatInfo: { parentClass: 'Actor', hasWeaponMesh: true } } }),
  buildRecord({ parentTool: T, id: `${T}.setup_damage_type`, action: 'setup_damage_type', family: F,
    summary: 'Create a DamageType Blueprint and return its path.', whenToUse: ['A damage class is needed by name and folder.'], whenNotToUse: ['Use configure_damage_execution to tune multipliers.'],
    inputProps: { name: P.name, path: P.path }, required: ['name'],
    effect: 'write', latency: 'interactive', resources: 'low',
    outputProps: { damageTypePath: P.damageTypePath }, outputRequired: ['damageTypePath'],
    exampleInput: { action: 'setup_damage_type', name: 'DT_Ice', path: '/Game/Damage' }, exampleOutput: { success: true, message: 'Damage type set up', damageTypePath: '/Game/Damage/DT_Ice.DT_Ice' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_hit_detection`, action: 'configure_hit_detection', family: F,
    summary: 'Retune hitbox shape and damage multiplier on a Blueprint.', whenToUse: ['An existing hitbox must change shape or scaling.'], whenNotToUse: ['Use setup_hitbox_component to add a bone-bound volume.'],
    inputProps: { blueprintPath: P.blueprintPath, hitboxType: C.hitboxType, damageMultiplier: C.damageMultiplier }, required: ['blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { hitboxType: C.hitboxType }, outputRequired: [],
    exampleInput: { action: 'configure_hit_detection', blueprintPath: WEAPON, hitboxType: 'Sphere', damageMultiplier: 1.5 }, exampleOutput: { success: true, message: 'Hit detection configured', hitboxType: 'Sphere' } }),
  buildRecord({ parentTool: T, id: `${T}.get_combat_stats`, action: 'get_combat_stats', family: F,
    summary: 'Read combat stats back from a Blueprint.', whenToUse: ['Inspect authored weapon stats.'], whenNotToUse: ['Use set_weapon_stats to change them.'],
    inputProps: { blueprintPath: P.blueprintPath }, required: ['blueprintPath'],
    effect: 'read', latency: 'instant', resources: 'low',
    outputProps: { combatInfo: C.combatInfo }, outputRequired: [],
    exampleInput: { action: 'get_combat_stats', blueprintPath: WEAPON }, exampleOutput: { success: true, message: 'Combat stats', combatInfo: { parentClass: 'Actor', baseDamage: 45 } } }),
];
