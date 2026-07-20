/**
 * manage_combat records — part 2 of 5 (projectiles, damage types, damage
 * execution and hitboxes).
 *
 * Parameters mirror the field literals read by
 * McpAutomationBridge_CombatHandlersProjectiles.cpp, DamageTypes.cpp and
 * DamageExecution.cpp. create_projectile_blueprint and create_damage_type are
 * creation actions, so combat-handlers.ts requires `name`; the configure/setup
 * actions target an existing asset through `blueprintPath`.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { C } from './combat-properties.js';

const T = 'manage_combat';
const F = 'combat';
const PROJECTILE = '/Game/BP_Bullet';
const WEAPON = '/Game/BP_Rifle';

export const COMBAT_2: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_projectile_blueprint`, action: 'create_projectile_blueprint', family: F,
    summary: 'Create a projectile Blueprint with movement and collision.', whenToUse: ['A projectile actor must be authored.'], whenNotToUse: ['Use create_weapon_blueprint for the weapon that fires it.'],
    inputProps: { action: P.action, name: P.name, path: P.path, projectileSpeed: C.projectileSpeed, projectileGravityScale: C.projectileGravityScale, collisionRadius: C.collisionRadius, projectileMeshPath: C.projectileMeshPath }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { blueprintPath: P.blueprintPath, projectileMeshPath: C.projectileMeshPath }, outputRequired: ['blueprintPath'],
    exampleInput: { action: 'create_projectile_blueprint', name: 'BP_Bullet', path: '/Game/Weapons', projectileSpeed: 6000, projectileGravityScale: 0, collisionRadius: 8 }, exampleOutput: { success: true, message: 'Projectile Blueprint created', blueprintPath: PROJECTILE } }),
  buildRecord({ parentTool: T, id: `${T}.configure_projectile_movement`, action: 'configure_projectile_movement', family: F,
    summary: 'Configure projectile speed, gravity scale and lifespan.', whenToUse: ['Projectile flight must change.'], whenNotToUse: ['Use configure_projectile_collision for impact response.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, projectileSpeed: C.projectileSpeed, projectileGravityScale: C.projectileGravityScale, projectileLifespan: C.projectileLifespan }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_projectile_movement', blueprintPath: PROJECTILE, projectileSpeed: 6500, projectileGravityScale: 0.5, projectileLifespan: 4 }, exampleOutput: { success: true, message: 'Projectile movement configured' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_projectile_collision`, action: 'configure_projectile_collision', family: F,
    summary: 'Configure projectile collision radius and bounce response.', whenToUse: ['Impact or bounce behaviour must change.'], whenNotToUse: ['Use configure_projectile_movement for flight.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, collisionRadius: C.collisionRadius, bounceEnabled: C.bounceEnabled, bounceVelocityRatio: C.bounceVelocityRatio }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_projectile_collision', blueprintPath: PROJECTILE, collisionRadius: 12, bounceEnabled: true, bounceVelocityRatio: 0.5 }, exampleOutput: { success: true, message: 'Projectile collision configured' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_projectile_homing`, action: 'configure_projectile_homing', family: F,
    summary: 'Configure projectile homing and its turn acceleration.', whenToUse: ['The projectile should track a target.'], whenNotToUse: ['Use configure_projectile_movement for ballistic flight.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, homingEnabled: C.homingEnabled, homingAcceleration: C.homingAcceleration }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_projectile_homing', blueprintPath: PROJECTILE, homingEnabled: true, homingAcceleration: 15000 }, exampleOutput: { success: true, message: 'Projectile homing configured' } }),
  buildRecord({ parentTool: T, id: `${T}.create_damage_type`, action: 'create_damage_type', family: F,
    summary: 'Create a DamageType Blueprint asset.', whenToUse: ['A custom damage classification is needed.'], whenNotToUse: ['Use configure_damage_execution to tune multipliers.'],
    inputProps: { action: P.action, name: P.name, path: P.path }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low',
    outputProps: { damageTypePath: P.damageTypePath }, outputRequired: ['damageTypePath'],
    exampleInput: { action: 'create_damage_type', name: 'DT_Fire', path: '/Game/Damage' }, exampleOutput: { success: true, message: 'DamageType created', damageTypePath: '/Game/Damage/DT_Fire.DT_Fire' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_damage_execution`, action: 'configure_damage_execution', family: F,
    summary: 'Configure impulse and critical/headshot damage multipliers.', whenToUse: ['Damage scaling must change.'], whenNotToUse: ['Use create_damage_type to author a damage class.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, damageImpulse: C.damageImpulse, criticalMultiplier: C.criticalMultiplier, headshotMultiplier: C.headshotMultiplier }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { headshotMultiplier: C.headshotMultiplier }, outputRequired: [],
    exampleInput: { action: 'configure_damage_execution', blueprintPath: WEAPON, damageImpulse: 700, criticalMultiplier: 2, headshotMultiplier: 3 }, exampleOutput: { success: true, message: 'Damage execution configured', headshotMultiplier: 3 } }),
  buildRecord({ parentTool: T, id: `${T}.setup_hitbox_component`, action: 'setup_hitbox_component', family: F,
    summary: 'Add a shaped hitbox component bound to a bone.', whenToUse: ['A per-bone damage volume is needed.'], whenNotToUse: ['Use configure_hit_detection to retune an existing hitbox.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, hitboxBoneName: C.hitboxBoneName, hitboxType: C.hitboxType, hitboxSize: C.hitboxSize, isDamageZoneHead: C.isDamageZoneHead, damageMultiplier: C.damageMultiplier }, required: ['action', 'blueprintPath'],
    effect: 'write', latency: 'interactive', resources: 'low',
    outputProps: { hitboxType: C.hitboxType, hitboxSize: C.hitboxSize }, outputRequired: [],
    exampleInput: { action: 'setup_hitbox_component', blueprintPath: WEAPON, hitboxBoneName: 'spine_03', hitboxType: 'Box', hitboxSize: { extent: { x: 12, y: 18, z: 22 } }, isDamageZoneHead: true, damageMultiplier: 2 }, exampleOutput: { success: true, message: 'Hitbox component set up', hitboxType: 'Box' } }),
];
