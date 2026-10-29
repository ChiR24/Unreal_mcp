// Fold specs for manage_character. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_CHARACTER_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'configure_character', selector: 'setting',
    summary: 'Configure a Character Blueprint\'s defaults by setting: movement_speeds (walk, crouch, swim, fly, acceleration, braking, friction), jump (launch speed, hold, double jump, air control, gravity), crouch, rotation (orient to movement or face the aim), capsule_component, mesh_component (skeletal mesh, Animation Blueprint, offset), camera_component (a third-person spring arm and camera, made when missing, or with view first_person a camera at the eyes) or nav_movement (AI agent size, avoidance). Each compiles and saves the Blueprint, changes only the fields sent and reads the values back; a Blueprint that is not a Character is refused (NOT_A_CHARACTER).',
    topics: ['character movement', 'jump', 'crouch', 'capsule', 'character camera', 'third person camera', 'follow camera', 'assign anim blueprint', 'make the player run faster', 'change the player speed', 'change the camera distance', 'make the player float', 'switch to first person or third person', 'double jump', 'add a crouch', 'make the player faster or slower', 'make the character walk or run faster'],
    members: {
      ...byTarget('configure_', ['configure_movement_speeds', 'configure_jump', 'configure_crouch', 'configure_rotation', 'configure_capsule_component',
        'configure_mesh_component', 'configure_camera_component', 'configure_nav_movement']),
    },
  },
  {
    primary: 'set_movement_property', selector: 'movementProperty',
    summary: 'Set one character movement property: walk speed, jump height (set as its launch speed, JumpZVelocity), gravity scale, ground friction, braking deceleration; compiled, saved and read back.',
    topics: ['walk speed', 'jump height', 'gravity scale', 'ground friction', 'braking'],
    members: byTarget('set_', ['set_walk_speed', 'set_jump_height', 'set_gravity_scale', 'set_ground_friction', 'set_braking_deceleration']),
  },
];
