#!/usr/bin/env node
/**
 * manage_combat Tool Integration Tests
 * Covers all 39 actions with real Blueprint state captured from creation.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayCombat_${ts}`;
const TEST_ACTOR = `TestCombatActor_${ts}`;
const WEAPON_NAME = `BP_MCP_Weapon_${ts}`;
const PROJECTILE_NAME = `BP_MCP_Projectile_${ts}`;
const DAMAGE_TYPE_NAME = `BP_MCP_DamageType_${ts}`;
const DAMAGE_EFFECT_NAME = `BP_MCP_DamageEffect_${ts}`;
const ALIAS_DAMAGE_TYPE_NAME = `BP_MCP_DamageAlias_${ts}`;

const weaponPath = '${captured:weaponPath}';
const projectilePath = '${captured:projectilePath}';

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },

  // === WEAPON BASE ===
  {
    scenario: 'CREATE: create_weapon_blueprint',
    toolName: 'manage_combat',
    arguments: { action: 'create_weapon_blueprint', name: WEAPON_NAME, path: TEST_FOLDER, baseDamage: 37, fireRate: 480, range: 9000, spread: 1.5 },
    expected: 'success',
    captureResult: { key: 'weaponPath', fromField: 'result.blueprintPath' },
    assertions: [
      { path: 'structuredContent.result.baseDamage', equals: 37, label: 'weapon base damage applied at creation' },
      { path: 'structuredContent.result.fireRate', equals: 480, label: 'weapon fire rate applied at creation' }
    ]
  },
  { scenario: 'CONFIG: configure_weapon_mesh', toolName: 'manage_combat', arguments: { action: 'configure_weapon_mesh', blueprintPath: weaponPath, weaponMeshPath: '/Engine/BasicShapes/Cube.Cube' }, expected: 'success', assertions: [{ path: 'structuredContent.result.meshPath', equals: '/Engine/BasicShapes/Cube.Cube', label: 'weapon mesh path applied' }] },
  { scenario: 'CONFIG: configure_weapon_sockets', toolName: 'manage_combat', arguments: { action: 'configure_weapon_sockets', blueprintPath: weaponPath, muzzleSocketName: 'MuzzleSocket', ejectionSocketName: 'ShellSocket' }, expected: 'success', assertions: [{ path: 'structuredContent.result.muzzleSocket', equals: 'MuzzleSocket', label: 'muzzle socket name applied' }] },
  { scenario: 'CONFIG: set_weapon_stats', toolName: 'manage_combat', arguments: { action: 'set_weapon_stats', blueprintPath: weaponPath, baseDamage: 45, fireRate: 540, range: 11000, spread: 1 }, expected: 'success', assertions: [{ path: 'structuredContent.result.baseDamage', equals: 45, label: 'weapon stat damage applied' }] },

  // === FIRING MODES ===
  { scenario: 'CONFIG: configure_hitscan', toolName: 'manage_combat', arguments: { action: 'configure_hitscan', blueprintPath: weaponPath, hitscanEnabled: true, traceChannel: 'Visibility', range: 12000 }, expected: 'success', assertions: [{ path: 'structuredContent.result.range', equals: 12000, label: 'hitscan range applied' }] },
  { scenario: 'CONFIG: configure_projectile', toolName: 'manage_combat', arguments: { action: 'configure_projectile', blueprintPath: weaponPath, projectileClass: '/Script/Engine.Actor', projectileSpeed: 4500 }, expected: 'success', assertions: [{ path: 'structuredContent.result.projectileSpeed', equals: 4500, label: 'weapon projectile speed applied' }] },
  { scenario: 'CONFIG: configure_spread_pattern', toolName: 'manage_combat', arguments: { action: 'configure_spread_pattern', blueprintPath: weaponPath, spreadPattern: 'Shotgun', spreadIncrease: 0.5, spreadRecovery: 2 }, expected: 'success', assertions: [{ path: 'structuredContent.result.patternType', equals: 'Shotgun', label: 'spread pattern applied' }] },
  { scenario: 'CONFIG: configure_recoil_pattern', toolName: 'manage_combat', arguments: { action: 'configure_recoil_pattern', blueprintPath: weaponPath, recoilPitch: 2, recoilYaw: 0.5, recoilRecovery: 6 }, expected: 'success', assertions: [{ path: 'structuredContent.result.recoilYaw', equals: 0.5, label: 'recoil yaw applied' }] },
  { scenario: 'CONFIG: configure_aim_down_sights', toolName: 'manage_combat', arguments: { action: 'configure_aim_down_sights', blueprintPath: weaponPath, adsEnabled: true, adsFov: 55, adsSpeed: 0.5, adsSpreadMultiplier: 0.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.adsFov', equals: 55, label: 'ADS FOV applied' }] },

  // === PROJECTILES ===
  {
    scenario: 'CREATE: create_projectile_blueprint',
    toolName: 'manage_combat',
    arguments: { action: 'create_projectile_blueprint', name: PROJECTILE_NAME, path: TEST_FOLDER, projectileSpeed: 6000, projectileGravityScale: 0, collisionRadius: 8, projectileMeshPath: '/Engine/BasicShapes/Sphere.Sphere' },
    expected: 'success',
    captureResult: { key: 'projectilePath', fromField: 'result.blueprintPath' },
    assertions: [{ path: 'structuredContent.result.projectileMeshPath', equals: '/Engine/BasicShapes/Sphere.Sphere', label: 'projectile mesh path consumed' }, { path: 'structuredContent.result.projectileMeshLoaded', equals: true, label: 'projectile mesh asset loaded' }, { path: 'structuredContent.result.existsAfter', equals: true, label: 'projectile blueprint exists after creation' }]
  },
  { scenario: 'CONFIG: configure_projectile_movement', toolName: 'manage_combat', arguments: { action: 'configure_projectile_movement', blueprintPath: projectilePath, projectileSpeed: 6500, projectileGravityScale: 0.5, projectileLifespan: 4 }, expected: 'success', assertions: [{ path: 'structuredContent.result.existsAfter', equals: true, label: 'projectile exists after movement configuration' }] },
  { scenario: 'CONFIG: configure_projectile_collision', toolName: 'manage_combat', arguments: { action: 'configure_projectile_collision', blueprintPath: projectilePath, collisionRadius: 12, bounceEnabled: true, bounceVelocityRatio: 0.5 }, expected: 'success' },
  { scenario: 'CONFIG: configure_projectile_homing', toolName: 'manage_combat', arguments: { action: 'configure_projectile_homing', blueprintPath: projectilePath, homingEnabled: true, homingAcceleration: 15000 }, expected: 'success' },
  { scenario: 'VERIFY: projectile combat info', toolName: 'manage_combat', arguments: { action: 'get_combat_info', blueprintPath: projectilePath }, expected: 'success', assertions: [{ path: 'structuredContent.result.combatInfo.hasProjectileMovement', equals: true, label: 'projectile movement component present' }, { path: 'structuredContent.result.combatInfo.hasCollision', equals: true, label: 'projectile collision component present' }] },

  // === DAMAGE SYSTEM ===
  { scenario: 'CREATE: create_damage_type', toolName: 'manage_combat', arguments: { action: 'create_damage_type', name: DAMAGE_TYPE_NAME, path: TEST_FOLDER }, expected: 'success', assertions: [{ path: 'structuredContent.result.existsAfter', equals: true, label: 'damage type asset exists after creation' }] },
  { scenario: 'CONFIG: configure_damage_execution', toolName: 'manage_combat', arguments: { action: 'configure_damage_execution', blueprintPath: weaponPath, damageImpulse: 700, criticalMultiplier: 2, headshotMultiplier: 3 }, expected: 'success', assertions: [{ path: 'structuredContent.result.headshotMultiplier', equals: 3, label: 'headshot multiplier applied' }] },
  { scenario: 'ACTION: setup_hitbox_component', toolName: 'manage_combat', arguments: { action: 'setup_hitbox_component', blueprintPath: weaponPath, hitboxType: 'Box', hitboxBoneName: 'spine_03', hitboxSize: { extent: { x: 12, y: 18, z: 22 } }, isDamageZoneHead: true, damageMultiplier: 2 }, expected: 'success', assertions: [{ path: 'structuredContent.result.hitboxType', equals: 'Box', label: 'hitbox type applied' }, { path: 'structuredContent.result.hitboxSize.extent.x', equals: 12, label: 'hitbox extent x applied' }, { path: 'structuredContent.result.hitboxSize.extent.y', equals: 18, label: 'hitbox extent y applied' }, { path: 'structuredContent.result.hitboxSize.extent.z', equals: 22, label: 'hitbox extent z applied' }] },

  // === WEAPON FEATURES ===
  { scenario: 'ACTION: setup_reload_system', toolName: 'manage_combat', arguments: { action: 'setup_reload_system', blueprintPath: weaponPath, magazineSize: 24, reloadTime: 2, reloadAnimationPath: '/Game/MCPTest/FakeReloadMontage' }, expected: 'success', assertions: [{ path: 'structuredContent.result.magazineSize', equals: 24, label: 'magazine size applied' }, { path: 'structuredContent.result.reloadAnimationPath', equals: '/Game/MCPTest/FakeReloadMontage', label: 'reload animation path consumed' }, { path: 'structuredContent.result.reloadAnimationLoaded', equals: false, label: 'missing reload animation handled without applying object reference' }] },
  { scenario: 'ACTION: setup_ammo_system', toolName: 'manage_combat', arguments: { action: 'setup_ammo_system', blueprintPath: weaponPath, ammoType: 'Rifle', maxAmmo: 120, startingAmmo: 48, ammoPerShot: 1, infiniteAmmo: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.maxAmmo', equals: 120, label: 'max ammo applied' }, { path: 'structuredContent.result.ammoPerShot', equals: 1, label: 'ammo per shot applied' }, { path: 'structuredContent.result.infiniteAmmo', equals: false, label: 'infinite ammo flag applied' }] },
  { scenario: 'ACTION: setup_attachment_system', toolName: 'manage_combat', arguments: { action: 'setup_attachment_system', blueprintPath: weaponPath, attachmentSlots: [{ slotName: 'Optic', socketName: 'OpticSocket', allowedTypes: ['Scope'] }, { slotName: 'Magazine', socketName: 'MagazineSocket', allowedTypes: ['Magazine'] }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.attachmentSlots', length: 2, label: 'attachment slots registered' }, { path: 'structuredContent.result.componentsCreated', length: 2, label: 'attachment scene components created' }] },
  { scenario: 'ACTION: setup_weapon_switching', toolName: 'manage_combat', arguments: { action: 'setup_weapon_switching', blueprintPath: weaponPath, switchInTime: 0.5, switchOutTime: 0.25, equipAnimationPath: '/Game/MCPTest/FakeEquipMontage', unequipAnimationPath: '/Game/MCPTest/FakeUnequipMontage' }, expected: 'success', assertions: [{ path: 'structuredContent.result.switchInTime', equals: 0.5, label: 'weapon switch-in time applied' }, { path: 'structuredContent.result.equipAnimationPath', equals: '/Game/MCPTest/FakeEquipMontage', label: 'equip animation path consumed' }, { path: 'structuredContent.result.unequipAnimationPath', equals: '/Game/MCPTest/FakeUnequipMontage', label: 'unequip animation path consumed' }, { path: 'structuredContent.result.equipAnimationLoaded', equals: false, label: 'missing equip animation handled without applying object reference' }, { path: 'structuredContent.result.unequipAnimationLoaded', equals: false, label: 'missing unequip animation handled without applying object reference' }] },

  // === EFFECTS ===
  { scenario: 'CONFIG: configure_muzzle_flash', toolName: 'manage_combat', arguments: { action: 'configure_muzzle_flash', blueprintPath: weaponPath, muzzleFlashParticlePath: '/Game/MCPTest/FakeMuzzleFlash', muzzleFlashScale: 1.5, muzzleSoundPath: '/Game/MCPTest/FakeMuzzleSound' }, expected: 'success', assertions: [{ path: 'structuredContent.result.particlePath', equals: '/Game/MCPTest/FakeMuzzleFlash', label: 'muzzle flash particle path applied' }, { path: 'structuredContent.result.soundPath', equals: '/Game/MCPTest/FakeMuzzleSound', label: 'muzzle sound path consumed' }, { path: 'structuredContent.result.scale', equals: 1.5, label: 'muzzle flash scale applied' }, { path: 'structuredContent.result.particleLoaded', equals: false, label: 'missing muzzle particle handled without object reference' }, { path: 'structuredContent.result.soundLoaded', equals: false, label: 'missing muzzle sound handled without object reference' }] },
  { scenario: 'CONFIG: configure_tracer', toolName: 'manage_combat', arguments: { action: 'configure_tracer', blueprintPath: weaponPath, tracerParticlePath: '/Game/MCPTest/FakeTracerParticle', tracerSpeed: 15000 }, expected: 'success', assertions: [{ path: 'structuredContent.result.tracerPath', equals: '/Game/MCPTest/FakeTracerParticle', label: 'tracer particle path applied' }, { path: 'structuredContent.result.tracerSpeed', equals: 15000, label: 'tracer speed applied' }] },
  { scenario: 'CONFIG: configure_impact_effects', toolName: 'manage_combat', arguments: { action: 'configure_impact_effects', blueprintPath: weaponPath, impactParticlePath: '/Game/MCPTest/FakeImpactParticle', impactSoundPath: '/Game/MCPTest/FakeImpactSound', impactDecalPath: '/Game/MCPTest/FakeImpactDecal' }, expected: 'success', assertions: [{ path: 'structuredContent.result.particlePath', equals: '/Game/MCPTest/FakeImpactParticle', label: 'impact particle path applied' }, { path: 'structuredContent.result.soundPath', equals: '/Game/MCPTest/FakeImpactSound', label: 'impact sound path applied' }, { path: 'structuredContent.result.decalPath', equals: '/Game/MCPTest/FakeImpactDecal', label: 'impact decal path applied' }] },
  { scenario: 'CONFIG: configure_shell_ejection', toolName: 'manage_combat', arguments: { action: 'configure_shell_ejection', blueprintPath: weaponPath, shellMeshPath: '/Engine/BasicShapes/Cube.Cube', shellEjectionForce: 400, shellLifespan: 3 }, expected: 'success', assertions: [{ path: 'structuredContent.result.shellMeshPath', equals: '/Engine/BasicShapes/Cube.Cube', label: 'shell mesh path applied' }, { path: 'structuredContent.result.ejectionForce', equals: 400, label: 'shell ejection force applied' }] },

  // === MELEE COMBAT ===
  { scenario: 'CREATE: create_melee_trace', toolName: 'manage_combat', arguments: { action: 'create_melee_trace', blueprintPath: weaponPath, meleeTraceStartSocket: 'BladeBase', meleeTraceEndSocket: 'BladeTip', meleeTraceRadius: 16 }, expected: 'success', assertions: [{ path: 'structuredContent.result.traceRadius', equals: 16, label: 'melee trace radius applied' }] },
  { scenario: 'CONFIG: configure_combo_system', toolName: 'manage_combat', arguments: { action: 'configure_combo_system', blueprintPath: weaponPath, comboWindowTime: 0.5, maxComboCount: 4 }, expected: 'success', assertions: [{ path: 'structuredContent.result.maxComboCount', equals: 4, label: 'max combo count applied' }] },
  { scenario: 'CREATE: create_hit_pause', toolName: 'manage_combat', arguments: { action: 'create_hit_pause', blueprintPath: weaponPath, hitPauseDuration: 0.0625, hitPauseTimeDilation: 0.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.timeDilation', equals: 0.5, label: 'hit pause dilation applied' }] },
  { scenario: 'CONFIG: configure_hit_reaction', toolName: 'manage_combat', arguments: { action: 'configure_hit_reaction', blueprintPath: weaponPath, hitReactionMontage: '/Game/MCPTest/FakeHitReactionMontage', hitReactionStunTime: 0.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.hitReactionMontage', equals: '/Game/MCPTest/FakeHitReactionMontage', label: 'hit reaction montage path applied' }, { path: 'structuredContent.result.stunTime', equals: 0.5, label: 'hit reaction stun time applied' }, { path: 'structuredContent.result.animationLoaded', equals: false, label: 'missing hit reaction montage handled without object reference' }] },
  { scenario: 'ACTION: setup_parry_block_system', toolName: 'manage_combat', arguments: { action: 'setup_parry_block_system', blueprintPath: weaponPath, parryWindowStart: 0, parryWindowEnd: 0.25, parryAnimationPath: '/Game/MCPTest/FakeParryMontage', blockDamageReduction: 0.5, blockStaminaCost: 12 }, expected: 'success', assertions: [{ path: 'structuredContent.result.parryAnimationPath', equals: '/Game/MCPTest/FakeParryMontage', label: 'parry animation path consumed' }, { path: 'structuredContent.result.blockDamageReduction', equals: 0.5, label: 'block reduction applied' }, { path: 'structuredContent.result.parryAnimationLoaded', equals: false, label: 'missing parry animation handled without object reference' }] },
  { scenario: 'CONFIG: configure_weapon_trails', toolName: 'manage_combat', arguments: { action: 'configure_weapon_trails', blueprintPath: weaponPath, weaponTrailParticlePath: '/Game/MCPTest/FakeWeaponTrail', weaponTrailStartSocket: 'TrailStart', weaponTrailEndSocket: 'TrailEnd' }, expected: 'success', assertions: [{ path: 'structuredContent.result.trailParticlePath', equals: '/Game/MCPTest/FakeWeaponTrail', label: 'weapon trail particle path applied' }, { path: 'structuredContent.result.trailStartSocket', equals: 'TrailStart', label: 'weapon trail start socket applied' }] },

  // === INFO AND ALIASES ===
  { scenario: 'INFO: get_combat_info', toolName: 'manage_combat', arguments: { action: 'get_combat_info', blueprintPath: weaponPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.combatInfo.parentClass', equals: 'Actor', label: 'weapon parent class reported' }, { path: 'structuredContent.result.combatInfo.hasWeaponMesh', equals: true, label: 'weapon info sees mesh component' }, { path: 'structuredContent.result.combatInfo.components', length: 4, label: 'weapon info reports mesh, hitbox, and attachment components' }] },
  { scenario: 'ACTION: setup_damage_type', toolName: 'manage_combat', arguments: { action: 'setup_damage_type', name: ALIAS_DAMAGE_TYPE_NAME, path: TEST_FOLDER }, expected: 'success', assertions: [{ path: 'structuredContent.result.damageTypePath', equals: `${TEST_FOLDER}/${ALIAS_DAMAGE_TYPE_NAME}.${ALIAS_DAMAGE_TYPE_NAME}`, label: 'setup_damage_type returns created path' }] },
  { scenario: 'CONFIG: configure_hit_detection', toolName: 'manage_combat', arguments: { action: 'configure_hit_detection', blueprintPath: weaponPath, hitboxType: 'Sphere', damageMultiplier: 1.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.hitboxType', equals: 'Sphere', label: 'hit detection hitbox type applied' }] },
  { scenario: 'INFO: get_combat_stats', toolName: 'manage_combat', arguments: { action: 'get_combat_stats', blueprintPath: weaponPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.combatInfo.parentClass', equals: 'Actor', label: 'combat stats reports weapon parent class' }] },

  // === DAMAGE EFFECTS AND DEFENSE ===
  { scenario: 'CREATE: create_damage_effect', toolName: 'manage_combat', arguments: { action: 'create_damage_effect', name: DAMAGE_EFFECT_NAME, path: TEST_FOLDER, duration: 8, damagePerSecond: 15, effectType: 'DamageOverTime' }, expected: 'success', assertions: [{ path: 'structuredContent.result.duration', equals: 8, label: 'damage effect duration applied' }] },
  { scenario: 'ACTION: apply_damage', toolName: 'manage_combat', arguments: { action: 'apply_damage', blueprintPath: weaponPath, damageAmount: 33, damageType: 'Fire' }, expected: 'success', assertions: [{ path: 'structuredContent.result.damageAmount', equals: 33, label: 'damage amount applied' }] },
  { scenario: 'ACTION: heal', toolName: 'manage_combat', arguments: { action: 'heal', blueprintPath: weaponPath, healAmount: 25, maxHealth: 125 }, expected: 'success', assertions: [{ path: 'structuredContent.result.maxHealth', equals: 125, label: 'max health applied' }] },
  { scenario: 'CREATE: create_shield', toolName: 'manage_combat', arguments: { action: 'create_shield', blueprintPath: weaponPath, shieldAmount: 60, maxShield: 120, shieldRegenRate: 6, shieldRegenDelay: 3 }, expected: 'success', assertions: [{ path: 'structuredContent.result.maxShield', equals: 120, label: 'max shield applied' }, { path: 'structuredContent.result.shieldRegenDelay', equals: 3, label: 'shield regen delay applied' }] },
  { scenario: 'CONFIG: modify_armor', toolName: 'manage_combat', arguments: { action: 'modify_armor', blueprintPath: weaponPath, armorValue: 75, damageReduction: 0.25 }, expected: 'success', assertions: [{ path: 'structuredContent.result.armorValue', equals: 75, label: 'armor value applied' }] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TEST_ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-combat', testCases);
