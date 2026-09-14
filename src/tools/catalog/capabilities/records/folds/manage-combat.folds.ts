// Fold specs for manage_combat. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_COMBAT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_combat_asset', selector: 'kind',
    summary: 'Create a combat asset: weapon Blueprint, projectile Blueprint, damage type, or damage effect.',
    topics: ['weapon blueprint', 'projectile blueprint', 'damage type', 'damage effect', 'create weapon'],
    members: { weapon_blueprint: 'create_weapon_blueprint', projectile_blueprint: 'create_projectile_blueprint', damage_type: 'create_damage_type', damage_type_setup: 'setup_damage_type', damage_effect: 'create_damage_effect' },
  },
  {
    primary: 'configure_weapon', selector: 'setting',
    summary: 'Configure a weapon Blueprint: stats, mesh, sockets, hitscan, spread, recoil, aim-down-sights, muzzle flash, tracer, shell ejection, trails, ammo, reload, attachments, switching.',
    topics: ['weapon stats', 'hitscan', 'recoil', 'spread', 'aim down sights', 'muzzle flash', 'ammo', 'reload'],
    members: {
      ...byTarget('configure_', ['configure_weapon_mesh', 'configure_weapon_sockets', 'configure_hitscan', 'configure_spread_pattern', 'configure_recoil_pattern',
        'configure_aim_down_sights', 'configure_muzzle_flash', 'configure_tracer', 'configure_shell_ejection', 'configure_weapon_trails']),
      stats: 'set_weapon_stats', ammo: 'setup_ammo_system', reload: 'setup_reload_system', attachments: 'setup_attachment_system', switching: 'setup_weapon_switching',
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
    summary: 'Configure damage and defense: damage execution, hit detection, hitboxes, hit reactions, impact effects, combos, hit pause, melee traces, parry/block, shields, armor, or apply damage and heal.',
    topics: ['damage', 'hit detection', 'hitbox', 'hit reaction', 'impact effects', 'combo', 'melee', 'parry', 'shield', 'armor', 'heal'],
    members: {
      execution: 'configure_damage_execution', hit_detection: 'configure_hit_detection', hitbox: 'setup_hitbox_component', hit_reaction: 'configure_hit_reaction',
      impact_effects: 'configure_impact_effects', combo: 'configure_combo_system', hit_pause: 'create_hit_pause', melee_trace: 'create_melee_trace',
      parry_block: 'setup_parry_block_system', shield: 'create_shield', armor: 'modify_armor', apply: 'apply_damage', heal: 'heal',
    },
  },
  {
    primary: 'get_combat_info', selector: 'info',
    summary: 'Read a combat Blueprint\'s configuration or its stats.',
    members: { info: 'get_combat_info', stats: 'get_combat_stats' },
  },
];
