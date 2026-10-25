/**
 * Per-action parameter fragments for the manage_combat capability records.
 */
import type { JsonObject } from '../../../model.js';
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

  projectileClass: str('The projectile the weapon shoots: its Blueprint path (/Game/Weapons/BP_Bullet) or class path.'),
  projectileSpeed: num('Projectile launch/travel speed.'),


  // Projectiles — Projectiles.cpp
  projectileGravityScale: num('Gravity scale applied to the projectile.'),
  projectileLifespan: num('Projectile lifetime in seconds.'),
  projectileMeshPath: str('Canonical /Game projectile mesh path.'),
  collisionRadius: num('Projectile collision sphere radius.'),
  bounceEnabled: bool('Enable projectile bouncing on impact.'),
  bounceVelocityRatio: num('Fraction of velocity retained on bounce (0-1).'),
  homingEnabled: bool('Enable homing behaviour.'),
  homingAcceleration: num('Homing turn acceleration.'),

  hitboxBoneName: str('Bone the hitbox is attached to.'),
  hitboxType: choice(['Capsule', 'Box', 'Sphere'], 'Hitbox shape and component: Capsule (default, HitboxCapsule), Box (HitboxBox) or Sphere (HitboxSphere). The component of that name is kept when the Blueprint has it, else added; any other value is refused.'),
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
  isDamageZoneHead: bool('Default of the Blueprint variable bIsHeadshotZone (added when missing; default false), for its damage logic to read.'),
  damageMultiplier: num('Default of the Blueprint variable HitboxDamageMultiplier (added when missing; default 1). Nothing reads it until the Blueprint damage logic does.'),


  // Attachments and switching — WeaponEquipment.cpp
  attachmentSlots: {
    type: 'array',
    description: 'Attachment slots: each adds an AttachPoint_<slotName> scene component under WeaponMesh (kept when there). An empty list or a nameless slot is refused before anything changes.',
    items: {
      type: 'object',
      additionalProperties: false,
      properties: {
        slotName: { type: 'string', description: 'Slot name, giving the component AttachPoint_<slotName>.' },
        socketName: { type: 'string', description: 'Socket of the WeaponMesh static mesh the attach point sits on; omitted, it sits at the mesh origin.' },
        allowedTypes: {
          type: 'array',
          items: { type: 'string' },
          description: 'Attachment types the slot takes, added to the attach point component tags for the game to read (ComponentHasTag, GetComponentsByTag).',
        },
      },
    },
  },
  attachmentSlotsOut: {
    type: 'array',
    description: 'The slots made or kept, as each attach point now stands.',
    items: {
      type: 'object',
      additionalProperties: false,
      properties: {
        slotName: { type: 'string', description: 'Slot name.' },
        socketName: { type: 'string', description: 'Socket the attach point sits on; empty when none was given.' },
        socketFound: { type: 'boolean', description: 'Given a socketName: whether the WeaponMesh static mesh has that socket. Attached to a missing socket, the point sits at the mesh origin.' },
        allowedTypes: { type: 'array', items: { type: 'string' }, description: 'Types added to its component tags.' },
      },
    },
  },







  // Native write-back fields, exposed as record outputs only
  combatInfo: {
    type: 'object',
    description: 'Combat configuration read back from the Blueprint.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
} satisfies PropertyMap;
