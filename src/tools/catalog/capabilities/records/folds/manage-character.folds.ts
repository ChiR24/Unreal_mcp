// Fold specs for manage_character. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_CHARACTER_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'configure_character', selector: 'setting',
    summary: 'Configure a Character Blueprint: movement speeds, jump, crouch, sprint, rotation, capsule, mesh, camera, nav movement, footstep FX, custom movement modes, surface sounds.',
    topics: ['character movement', 'jump', 'crouch', 'sprint', 'capsule', 'character camera', 'footstep', 'movement mode'],
    members: {
      ...byTarget('configure_', ['configure_movement_speeds', 'configure_jump', 'configure_crouch', 'configure_sprint', 'configure_rotation', 'configure_capsule_component',
        'configure_mesh_component', 'configure_camera_component', 'configure_nav_movement', 'configure_footstep_fx']),
      custom_movement_mode: 'add_custom_movement_mode', surface_sound: 'map_surface_to_sound',
    },
  },
  {
    primary: 'set_movement_property', selector: 'movementProperty',
    summary: 'Set one character movement property: walk speed, jump height, gravity scale, ground friction, braking deceleration.',
    topics: ['walk speed', 'jump height', 'gravity scale', 'ground friction', 'braking'],
    members: byTarget('set_', ['set_walk_speed', 'set_jump_height', 'set_gravity_scale', 'set_ground_friction', 'set_braking_deceleration']),
  },
  {
    primary: 'setup_character_ability', selector: 'ability',
    summary: 'Set up a character movement ability: movement basics, climbing, mantling, vaulting, sliding, wall running, grappling, footstep system.',
    topics: ['climbing', 'mantling', 'vaulting', 'sliding', 'wall running', 'grappling', 'footstep system', 'movement ability'],
    members: byTarget('setup_', ['setup_movement', 'setup_climbing', 'setup_mantling', 'setup_vaulting', 'setup_sliding', 'setup_wall_running', 'setup_grappling', 'setup_footstep_system']),
  },
];
