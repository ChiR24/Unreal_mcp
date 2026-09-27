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

  projectileClass: str('Class path of the projectile the weapon spawns.'),
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







  // Native write-back fields, exposed as record outputs only
  combatInfo: {
    type: 'object',
    description: 'Combat configuration read back from the Blueprint.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
} satisfies PropertyMap;
