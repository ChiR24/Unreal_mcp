/**
 * manage_combat records — part 1 of 5 (weapon base and firing modes).
 *
 * Each record declares exactly the parameters its action consumes: the field
 * literals read by McpAutomationBridge_CombatHandlersWeaponCore.cpp,
 * WeaponStats.cpp, WeaponFiring.cpp and WeaponHandling.cpp, forwarded by
 * combat-handlers.ts. That handler validates `name` for creation actions and
 * `blueprintPath` for every other action, so those are the required fields —
 * there is no per-asset-kind path parameter on this tool.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { C } from './combat-properties.js';

const T = 'manage_combat';
const F = 'combat';
const WEAPON = '/Game/BP_Rifle';

export const COMBAT_1: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_weapon_blueprint`, action: 'create_weapon_blueprint', family: F,
    topics: ['weapon', 'gun', 'weapon blueprint', 'firearm', 'new weapon'],
    summary: 'Create a weapon Blueprint asset with its base stats.', whenToUse: ['A new weapon actor must be authored.'], whenNotToUse: ['Use create_projectile_blueprint for the projectile it fires.'],
    inputProps: { action: P.action, name: P.name, path: P.path, baseDamage: C.baseDamage, fireRate: C.fireRate, range: C.range, spread: C.spread }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    outputProps: { blueprintPath: P.blueprintPath, baseDamage: C.baseDamage, fireRate: C.fireRate }, outputRequired: ['blueprintPath'],
    exampleInput: { action: 'create_weapon_blueprint', name: 'BP_Rifle', path: '/Game/Weapons', baseDamage: 37, fireRate: 480 }, exampleOutput: { success: true, message: 'Weapon Blueprint created', blueprintPath: WEAPON, baseDamage: 37, fireRate: 480 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_weapon_mesh`, action: 'configure_weapon_mesh', family: F,
    summary: 'Assign the weapon skeletal/static mesh.', whenToUse: ['The weapon mesh must change.'], whenNotToUse: ['Use configure_weapon_sockets for attach points.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, weaponMeshPath: C.weaponMeshPath }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { meshPath: P.meshPath }, outputRequired: [],
    exampleInput: { action: 'configure_weapon_mesh', blueprintPath: WEAPON, weaponMeshPath: '/Engine/BasicShapes/Cube.Cube' }, exampleOutput: { success: true, message: 'Weapon mesh configured', meshPath: '/Engine/BasicShapes/Cube.Cube' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_weapon_sockets`, action: 'configure_weapon_sockets', family: F,
    summary: 'Name the muzzle and shell ejection sockets.', whenToUse: ['Muzzle or ejection attach points are needed.'], whenNotToUse: ['Use configure_weapon_mesh to swap the mesh.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, muzzleSocketName: C.muzzleSocketName, ejectionSocketName: C.ejectionSocketName }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { muzzleSocket: C.muzzleSocket }, outputRequired: [],
    exampleInput: { action: 'configure_weapon_sockets', blueprintPath: WEAPON, muzzleSocketName: 'MuzzleSocket', ejectionSocketName: 'ShellSocket' }, exampleOutput: { success: true, message: 'Weapon sockets configured', muzzleSocket: 'MuzzleSocket' } }),
  buildRecord({ parentTool: T, id: `${T}.set_weapon_stats`, action: 'set_weapon_stats', family: F,
    summary: 'Set weapon damage, fire rate, range and spread.', whenToUse: ['Weapon stats must change after creation.'], whenNotToUse: ['Use create_weapon_blueprint to author the asset.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, baseDamage: C.baseDamage, fireRate: C.fireRate, range: C.range, spread: C.spread }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { baseDamage: C.baseDamage }, outputRequired: [],
    exampleInput: { action: 'set_weapon_stats', blueprintPath: WEAPON, baseDamage: 45, fireRate: 540, range: 11000, spread: 1 }, exampleOutput: { success: true, message: 'Weapon stats set', baseDamage: 45 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_hitscan`, action: 'configure_hitscan', family: F,
    summary: 'Configure instant-hit firing and its trace channel.', whenToUse: ['The weapon should hit instantly.'], whenNotToUse: ['Use configure_projectile for spawned projectiles.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, hitscanEnabled: C.hitscanEnabled, traceChannel: C.traceChannel, range: C.range }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { range: C.range }, outputRequired: [],
    exampleInput: { action: 'configure_hitscan', blueprintPath: WEAPON, hitscanEnabled: true, traceChannel: 'Visibility', range: 12000 }, exampleOutput: { success: true, message: 'Hitscan configured', range: 12000 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_projectile`, action: 'configure_projectile', family: F,
    summary: 'Point the weapon at a projectile class and launch speed.', whenToUse: ['The weapon spawns projectiles.'], whenNotToUse: ['Use configure_hitscan for instant hits.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, projectileClass: C.projectileClass, projectileSpeed: C.projectileSpeed }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { projectileSpeed: C.projectileSpeed }, outputRequired: [],
    exampleInput: { action: 'configure_projectile', blueprintPath: WEAPON, projectileClass: '/Script/Engine.Actor', projectileSpeed: 4500 }, exampleOutput: { success: true, message: 'Projectile configured', projectileSpeed: 4500 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_spread_pattern`, action: 'configure_spread_pattern', family: F,
    summary: 'Configure the spread pattern and its bloom/recovery.', whenToUse: ['Shotgun or bloom spread is needed.'], whenNotToUse: ['Use configure_recoil_pattern for camera kick.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, spreadPattern: C.spreadPattern, spreadIncrease: C.spreadIncrease, spreadRecovery: C.spreadRecovery }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { patternType: C.patternType }, outputRequired: [],
    exampleInput: { action: 'configure_spread_pattern', blueprintPath: WEAPON, spreadPattern: 'Shotgun', spreadIncrease: 0.5, spreadRecovery: 2 }, exampleOutput: { success: true, message: 'Spread pattern configured', patternType: 'Shotgun' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_recoil_pattern`, action: 'configure_recoil_pattern', family: F,
    summary: 'Configure per-shot recoil and its recovery.', whenToUse: ['Recoil kick is needed.'], whenNotToUse: ['Use configure_spread_pattern for cone spread.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, recoilPitch: C.recoilPitch, recoilYaw: C.recoilYaw, recoilRecovery: C.recoilRecovery }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { recoilYaw: C.recoilYaw }, outputRequired: [],
    exampleInput: { action: 'configure_recoil_pattern', blueprintPath: WEAPON, recoilPitch: 2, recoilYaw: 0.5, recoilRecovery: 6 }, exampleOutput: { success: true, message: 'Recoil pattern configured', recoilYaw: 0.5 } }),
  buildRecord({ parentTool: T, id: `${T}.configure_aim_down_sights`, action: 'configure_aim_down_sights', family: F,
    summary: 'Configure aim-down-sights FOV, speed and spread scaling.', whenToUse: ['ADS behaviour is needed.'], whenNotToUse: ['Use configure_spread_pattern for hip-fire spread.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, adsEnabled: C.adsEnabled, adsFov: C.adsFov, adsSpeed: C.adsSpeed, adsSpreadMultiplier: C.adsSpreadMultiplier }, required: ['action', 'blueprintPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    outputProps: { adsFov: C.adsFov }, outputRequired: [],
    exampleInput: { action: 'configure_aim_down_sights', blueprintPath: WEAPON, adsEnabled: true, adsFov: 55, adsSpeed: 0.5, adsSpreadMultiplier: 0.5 }, exampleOutput: { success: true, message: 'ADS configured', adsFov: 55 } }),
];
