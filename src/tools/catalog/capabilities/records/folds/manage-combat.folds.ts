// Fold specs for manage_combat. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_COMBAT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_combat_asset', selector: 'kind',
    summary: 'Create a combat asset: weapon Blueprint, projectile Blueprint, or damage type.',
    topics: ['weapon blueprint', 'projectile blueprint', 'damage type', 'create weapon'],
    members: { weapon_blueprint: 'create_weapon_blueprint', projectile_blueprint: 'create_projectile_blueprint', damage_type: 'create_damage_type', damage_type_setup: 'setup_damage_type' },
  },
  {
    primary: 'configure_weapon', selector: 'setting',
    summary: 'Configure a weapon Blueprint: its mesh or its attachment system.',
    topics: ['weapon mesh', 'weapon attachments'],
    members: {
      ...byTarget('configure_', ['configure_weapon_mesh']), attachments: 'setup_attachment_system',
    },
  },
  {
    primary: 'configure_projectile', selector: 'setting',
    summary: 'Configure a projectile Blueprint: class and speed, movement, collision, homing.',
    topics: ['projectile', 'projectile movement', 'projectile collision', 'homing'],
    members: { projectile: 'configure_projectile', movement: 'configure_projectile_movement', collision: 'configure_projectile_collision', homing: 'configure_projectile_homing' },
  },
  {
    primary: 'configure_damage', selector: 'setting',
    summary: 'Configure hit detection or add a hitbox component.',
    topics: ['hit detection', 'hitbox'],
    members: { hit_detection: 'configure_hit_detection', hitbox: 'setup_hitbox_component',
    },
  },
  {
    primary: 'get_combat_info', selector: 'info',
    summary: 'Read a combat Blueprint\'s configuration or its stats.',
    members: { info: 'get_combat_info', stats: 'get_combat_stats' },
  },
];
