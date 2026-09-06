/**
 * Per-action parameter fragments for the manage_combat capability records.
 *
 * Private to the manage_combat family; the shared gameplay `P` map is not
 * touched. Every fragment below names a field the combat implementation
 * actually reads: the names are the JSON field literals consumed by the native
 * Combat domain (plugins/.../Private/Domains/Combat/McpAutomationBridge_Combat
 * Handlers*.cpp) and forwarded verbatim by the TypeScript passthrough handler
 * (src/tools/handlers/combat/combat-handlers.ts), which validates only `name`
 * (creation actions) or `blueprintPath` (every other action).
 *
 * Fields the native side only writes back — meshPath, damageTypePath,
 * parentClass, muzzleSocket, patternType, tracerPath, particlePath — are
 * modelled as record outputs, never as inputs.
 */
import type { JsonObject } from '../../../index.js';
import type { PropertyMap } from '../properties.js';
import { str, num, bool } from '../../shared/schema-props.js';

const choice = (values: readonly string[], description: string): JsonObject => ({
  type: 'string',
  enum: [...values],
  description,
});

export const C = {
  // Weapon base — WeaponCore.cpp, WeaponStats.cpp
  baseDamage: num('Base damage per shot.'),
  fireRate: num('Rate of fire in rounds per minute.'),
  range: num('Effective weapon range in world units.'),
  spread: num('Base spread cone in degrees.'),
  weaponMeshPath: str('Canonical /Game weapon static or skeletal mesh path.'),
  muzzleSocketName: str('Socket name used as the muzzle attach point.'),
  ejectionSocketName: str('Socket name used as the shell ejection point.'),

  // Firing modes — WeaponFiring.cpp
  hitscanEnabled: bool('Enable instant-hit (hitscan) firing.'),
  traceChannel: choice(['Visibility', 'Camera', 'Weapon', 'Custom'], 'Trace channel used for hitscan.'),
  projectileClass: str('Class path of the projectile the weapon spawns.'),
  projectileSpeed: num('Projectile launch/travel speed.'),
  spreadPattern: choice(['Random', 'Fixed', 'FixedWithRandom', 'Shotgun'], 'Spread pattern type.'),
  spreadIncrease: num('Spread added per shot.'),
  spreadRecovery: num('Spread recovery rate per second.'),

  // Recoil and ADS — WeaponHandling.cpp
  recoilPitch: num('Vertical recoil per shot in degrees.'),
  recoilYaw: num('Horizontal recoil per shot in degrees.'),
  recoilRecovery: num('Recoil recovery speed.'),
  adsEnabled: bool('Enable aim-down-sights.'),
  adsFov: num('Field of view while aiming.'),
  adsSpeed: num('Time in seconds to reach full aim.'),
  adsSpreadMultiplier: num('Spread multiplier applied while aiming.'),

  // Projectiles — Projectiles.cpp
  projectileGravityScale: num('Gravity scale applied to the projectile.'),
  projectileLifespan: num('Projectile lifetime in seconds.'),
  projectileMeshPath: str('Canonical /Game projectile mesh path.'),
  collisionRadius: num('Projectile collision sphere radius.'),
  bounceEnabled: bool('Enable projectile bouncing on impact.'),
  bounceVelocityRatio: num('Fraction of velocity retained on bounce (0-1).'),
  homingEnabled: bool('Enable homing behaviour.'),
  homingAcceleration: num('Homing turn acceleration.'),

  // Damage execution and hitboxes — DamageExecution.cpp
  damageImpulse: num('Physics impulse applied on hit.'),
  criticalMultiplier: num('Critical hit damage multiplier.'),
  headshotMultiplier: num('Headshot damage multiplier.'),
  hitboxBoneName: str('Bone the hitbox is attached to.'),
  hitboxType: choice(['Capsule', 'Box', 'Sphere'], 'Hitbox collision shape.'),
  hitboxSize: {
    type: 'object',
    description: 'Hitbox dimensions: extent for Box, radius/halfHeight for Sphere and Capsule.',
    additionalProperties: false,
    properties: {
      radius: { type: 'number', description: 'Sphere or capsule radius.' },
      halfHeight: { type: 'number', description: 'Capsule half height.' },
      extent: {
        type: 'object',
        description: 'Box half extent.',
        additionalProperties: false,
        properties: {
          x: { type: 'number', description: 'Half extent along X.' },
          y: { type: 'number', description: 'Half extent along Y.' },
          z: { type: 'number', description: 'Half extent along Z.' },
        },
      },
    },
  },
  isDamageZoneHead: bool('Mark this hitbox as a headshot zone.'),
  damageMultiplier: num('Damage multiplier applied for this hitbox.'),

  // Reload and ammo — WeaponAmmo.cpp
  magazineSize: num('Rounds per magazine.'),
  reloadTime: num('Reload duration in seconds.'),
  reloadAnimationPath: str('Canonical /Game reload animation montage path.'),
  ammoType: str('Ammo type identifier.'),
  maxAmmo: num('Maximum carried ammo.'),
  startingAmmo: num('Ammo carried at spawn.'),
  ammoPerShot: num('Ammo consumed per shot.'),
  infiniteAmmo: bool('Skip ammo consumption entirely.'),

  // Attachments and switching — WeaponEquipment.cpp
  attachmentSlots: {
    type: 'array',
    description: 'Attachment slot definitions created on the weapon.',
    items: {
      type: 'object',
      additionalProperties: false,
      properties: {
        slotName: { type: 'string', description: 'Slot identifier.' },
        socketName: { type: 'string', description: 'Socket the attachment binds to.' },
        allowedTypes: {
          type: 'array',
          items: { type: 'string' },
          description: 'Attachment types accepted by this slot.',
        },
      },
    },
  },
  switchInTime: num('Time in seconds to equip the weapon.'),
  switchOutTime: num('Time in seconds to unequip the weapon.'),
  equipAnimationPath: str('Canonical /Game equip montage path.'),
  unequipAnimationPath: str('Canonical /Game unequip montage path.'),

  // Muzzle, tracer and impact FX — WeaponEffects.cpp
  muzzleFlashParticlePath: str('Canonical /Game muzzle flash particle path.'),
  muzzleFlashScale: num('Muzzle flash scale multiplier.'),
  muzzleSoundPath: str('Canonical /Game firing sound path.'),
  tracerParticlePath: str('Canonical /Game tracer particle path.'),
  tracerSpeed: num('Tracer travel speed.'),
  impactParticlePath: str('Canonical /Game impact particle path.'),
  impactSoundPath: str('Canonical /Game impact sound path.'),
  impactDecalPath: str('Canonical /Game impact decal path.'),

  // Shells and trails — WeaponShellTrails.cpp
  shellMeshPath: str('Canonical /Game shell casing mesh path.'),
  shellEjectionForce: num('Impulse applied to ejected shells.'),
  shellLifespan: num('Shell casing lifetime in seconds.'),
  weaponTrailParticlePath: str('Canonical /Game weapon trail particle path.'),
  weaponTrailStartSocket: str('Socket where the weapon trail starts.'),
  weaponTrailEndSocket: str('Socket where the weapon trail ends.'),

  // Melee core — MeleeCore.cpp
  meleeTraceStartSocket: str('Socket where the melee sweep starts.'),
  meleeTraceEndSocket: str('Socket where the melee sweep ends.'),
  meleeTraceRadius: num('Melee sphere sweep radius.'),
  comboWindowTime: num('Seconds the combo input window stays open.'),
  maxComboCount: num('Maximum number of chained combo hits.'),
  hitPauseDuration: num('Hitstop duration in seconds.'),
  hitPauseTimeDilation: num('Time dilation applied during hitstop.'),

  // Melee defense — MeleeDefense.cpp
  hitReactionMontage: str('Canonical /Game hit reaction montage path.'),
  hitReactionStunTime: num('Stun duration in seconds after a hit.'),
  parryWindowStart: num('Normalized parry window start (0-1).'),
  parryWindowEnd: num('Normalized parry window end (0-1).'),
  parryAnimationPath: str('Canonical /Game parry animation path.'),
  blockDamageReduction: num('Damage reduction while blocking (0-1).'),
  blockStaminaCost: num('Stamina consumed per blocked hit.'),

  // Damage effect, damage and healing authoring — HealthRuntime.cpp
  damagePerSecond: num('Damage applied per second by the effect.'),
  effectType: str('Damage effect classifier, for example DamageOverTime.'),
  damageAmount: num('Damage amount written to the Blueprint.'),
  damageType: str('Damage type name written to the Blueprint.'),
  healAmount: num('Heal amount written to the Blueprint.'),
  maxHealth: num('Maximum health written to the Blueprint.'),

  // Shield and armor authoring — DefenseRuntime.cpp
  shieldAmount: num('Starting shield value.'),
  maxShield: num('Maximum shield value.'),
  shieldRegenRate: num('Shield regenerated per second.'),
  shieldRegenDelay: num('Seconds before shield regeneration starts.'),
  armorValue: num('Armor value written to the Blueprint.'),
  damageReduction: num('Fraction of incoming damage mitigated by armor (0-1).'),

  // Native write-back fields, exposed as record outputs only
  combatInfo: {
    type: 'object',
    description: 'Combat configuration read back from the Blueprint.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  parentClass: str('Parent class name of the inspected Blueprint.'),
  muzzleSocket: str('Muzzle socket name applied to the weapon.'),
  patternType: str('Spread pattern type applied to the weapon.'),
  particlePath: str('Particle path applied to the weapon.'),
  soundPath: str('Sound path applied to the weapon.'),
  decalPath: str('Decal path applied to the weapon.'),
  tracerPath: str('Tracer particle path applied to the weapon.'),
  traceRadius: num('Melee sweep radius applied to the weapon.'),
  timeDilation: num('Time dilation applied for hitstop.'),
  stunTime: num('Stun duration applied on hit.'),
  ejectionForce: num('Shell ejection impulse applied to the weapon.'),
  trailParticlePath: str('Weapon trail particle path applied to the weapon.'),
  trailStartSocket: str('Weapon trail start socket applied to the weapon.'),
} satisfies PropertyMap;
