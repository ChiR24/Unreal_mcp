// Fold specs for manage_combat. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_COMBAT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_combat_asset', selector: 'kind',
    summary: 'Create a combat Blueprint named name in path (default /Game), chosen by kind; it is saved, and a path an asset already holds is refused. weapon_blueprint is an Actor with a WeaponMesh component (weaponMeshPath) and BaseDamage, FireRate, Range and Spread variables (defaults 25, 600, 10000, 2), and no firing logic of its own. projectile_blueprint flies (projectileSpeed, default 5000; projectileGravityScale, default 0), stops on what it meets with a collisionRadius sphere (default 5), applies damage (default 20) to it and is destroyed, never hits the actor that fired it, and is gone 5 s after firing when it meets nothing; projectileMeshPath is its mesh. A mesh path that does not load is refused before anything is made. damage_type (damage_type_setup) is a DamageType Blueprint for ApplyDamage to name.',
    topics: ['weapon blueprint', 'projectile blueprint', 'damage type', 'create weapon', 'gun'],
    members: { weapon_blueprint: 'create_weapon_blueprint', projectile_blueprint: 'create_projectile_blueprint', damage_type: 'create_damage_type', damage_type_setup: 'setup_damage_type' },
  },
  {
    primary: 'configure_weapon', selector: 'setting',
    summary: 'A weapon Blueprint, chosen by setting. weapon_mesh assigns weaponMeshPath (a static mesh; NOT_FOUND otherwise) to its WeaponMesh component, added when missing. attachments adds an AttachPoint_<slotName> scene component under WeaponMesh for each attachmentSlots entry: on socketName when given (socketFound says whether the mesh has that socket), with allowedTypes added to its component tags. These are components only; equipping an attachment is Blueprint logic. Compiles and saves (SAVE_FAILED otherwise).',
    topics: ['weapon mesh', 'weapon attachments'],
    members: {
      ...byTarget('configure_', ['configure_weapon_mesh']), attachments: 'setup_attachment_system',
    },
  },
  {
    primary: 'configure_projectile', selector: 'setting',
    summary: 'Configure projectiles: give a weapon a Fire function that shoots a projectile class, or set a projectile movement, collision or homing.',
    topics: ['projectile', 'projectile movement', 'projectile collision', 'homing', 'make a homing missile', 'weapon fires projectiles', 'gun that shoots'],
    members: { projectile: 'configure_projectile', movement: 'configure_projectile_movement', collision: 'configure_projectile_collision', homing: 'configure_projectile_homing' },
  },
  {
    primary: 'configure_damage', selector: 'setting',
    summary: 'Hitboxes on a Blueprint, chosen by setting. hitbox adds or updates the HitboxCapsule, HitboxBox or HitboxSphere component (hitboxType, default Capsule) sized by hitboxSize (only the sizes sent change), on hitboxBoneName of its skeletal mesh when given (NO_SKELETAL_MESH without one), and sets the variables bIsHeadshotZone and HitboxDamageMultiplier. hit_detection makes sure a hitbox of hitboxType exists and sets HitboxDamageMultiplier, without sizing it. An unknown hitboxType is refused. These are a component and variables only: no damage happens until the Blueprint applies it on the hitbox overlap or hit event. Compiles and saves (SAVE_FAILED otherwise).',
    topics: ['hit detection', 'hitbox'],
    members: { hit_detection: 'configure_hit_detection', hitbox: 'setup_hitbox_component',
    },
  },
  {
    primary: 'get_combat_info', selector: 'info',
    summary: 'Read a weapon or projectile Blueprint: parent class, components, mesh, projectile movement and collision flags, and its variables with defaults; info=stats returns the variables plus a stats object of the numeric ones.',
    topics: ['weapon stats', 'projectile settings', 'variable defaults'],
    members: { info: 'get_combat_info', stats: 'get_combat_stats' },
  },
];
