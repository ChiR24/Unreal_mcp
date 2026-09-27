// Fold specs for manage_character. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_CHARACTER_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'configure_character', selector: 'setting',
    summary: 'Configure a Character Blueprint: movement speeds, jump, crouch, rotation, capsule, mesh, camera, nav movement.',
    topics: ['character movement', 'jump', 'crouch', 'capsule', 'character camera'],
    members: {
      ...byTarget('configure_', ['configure_movement_speeds', 'configure_jump', 'configure_crouch', 'configure_rotation', 'configure_capsule_component',
        'configure_mesh_component', 'configure_camera_component', 'configure_nav_movement']),
    },
  },
  {
    primary: 'set_movement_property', selector: 'movementProperty',
    summary: 'Set one character movement property: walk speed, jump height, gravity scale, ground friction, braking deceleration.',
    topics: ['walk speed', 'jump height', 'gravity scale', 'ground friction', 'braking'],
    members: byTarget('set_', ['set_walk_speed', 'set_jump_height', 'set_gravity_scale', 'set_ground_friction', 'set_braking_deceleration']),
  },
];
