/**
 * Per-action contract test for the manage_character capability records.
 *
 * Same rationale as the manage_gas counterpart: a parent-union assertion cannot
 * detect a per-action contract collapsing onto placeholder properties, because
 * the union still reports "some property". Only the exact per-action sets do.
 *
 * The manage_character TS handler validates blueprintPath (plus name /
 * modeName / surfaceType) and forwards everything else verbatim, so the optional
 * sets below are the fields the native Character handlers actually read.
 */
import { describe, expect, it } from 'vitest';
import { createCapabilityRecord } from '../../../parser.js';
import { MANAGE_CHARACTER_SOURCES } from './index.js';

type Contract = { readonly required: readonly string[]; readonly optional: readonly string[] };

const CONTRACTS: Readonly<Record<string, Contract>> = {
  create_character_blueprint: { required: ['name'], optional: ['path', 'parentClass', 'skeletalMeshPath'] },
  configure_capsule_component: { required: ['blueprintPath'], optional: ['capsuleRadius', 'capsuleHalfHeight'] },
  configure_mesh_component: { required: ['blueprintPath'], optional: ['skeletalMeshPath', 'animBlueprintPath', 'meshOffset', 'meshRotation'] },
  configure_camera_component: { required: ['blueprintPath'], optional: ['springArmLength', 'springArmLagEnabled', 'springArmLagSpeed', 'cameraUsePawnControlRotation'] },
  configure_movement_speeds: { required: ['blueprintPath'], optional: ['walkSpeed', 'runSpeed', 'crouchSpeed', 'swimSpeed', 'flySpeed', 'acceleration', 'deceleration', 'groundFriction'] },
  configure_jump: { required: ['blueprintPath'], optional: ['jumpHeight', 'jumpHoldTime', 'maxJumpCount', 'airControl', 'gravityScale', 'fallingLateralFriction'] },
  configure_rotation: { required: ['blueprintPath'], optional: ['orientToMovement', 'rotationRate', 'useControllerRotationYaw', 'useControllerRotationPitch', 'useControllerRotationRoll'] },
  add_custom_movement_mode: { required: ['blueprintPath', 'modeName'], optional: ['modeId', 'customSpeed'] },
  configure_nav_movement: { required: ['blueprintPath'], optional: ['navAgentRadius', 'navAgentHeight', 'avoidanceEnabled'] },
  setup_movement: { required: ['blueprintPath'], optional: ['walkSpeed', 'runSpeed', 'acceleration'] },
  set_walk_speed: { required: ['blueprintPath'], optional: ['walkSpeed'] },
  set_jump_height: { required: ['blueprintPath'], optional: ['jumpHeight'] },
  set_gravity_scale: { required: ['blueprintPath'], optional: ['gravityScale'] },
  set_ground_friction: { required: ['blueprintPath'], optional: ['groundFriction'] },
  set_braking_deceleration: { required: ['blueprintPath'], optional: ['brakingDeceleration'] },
  setup_mantling: { required: ['blueprintPath'], optional: ['mantleHeight', 'mantleReachDistance'] },
  setup_vaulting: { required: ['blueprintPath'], optional: ['vaultHeight', 'vaultDepth'] },
  setup_climbing: { required: ['blueprintPath'], optional: ['climbSpeed', 'climbableTag'] },
  setup_sliding: { required: ['blueprintPath'], optional: ['slideSpeed', 'slideDuration', 'slideCooldown'] },
  setup_wall_running: { required: ['blueprintPath'], optional: ['wallRunSpeed', 'wallRunDuration', 'wallRunGravityScale'] },
  setup_grappling: { required: ['blueprintPath'], optional: ['grappleRange', 'grappleSpeed', 'grappleTargetTag'] },
  setup_footstep_system: { required: ['blueprintPath'], optional: ['footstepEnabled', 'footstepSocketLeft', 'footstepSocketRight', 'footstepTraceDistance'] },
  map_surface_to_sound: { required: ['blueprintPath', 'surfaceType'], optional: [] },
  configure_footstep_fx: { required: ['blueprintPath'], optional: ['volumeMultiplier', 'particleScale'] },
  get_character_info: { required: ['blueprintPath'], optional: [] },
  configure_crouch: { required: ['blueprintPath'], optional: ['canCrouch', 'crouchSpeed', 'crouchedHalfHeight'] },
  configure_sprint: { required: ['blueprintPath'], optional: ['sprintSpeed'] },
};

const RECORDS = MANAGE_CHARACTER_SOURCES.map((source) => createCapabilityRecord(source));
const byAction = new Map(RECORDS.map((record) => [String(record.legacyIds[0].action), record]));

describe('manage_character capability records', () => {
  it('declares exactly 27 actions, each routed to manage_character', () => {
    expect(RECORDS).toHaveLength(27);
    expect(new Set(byAction.keys()).size).toBe(27);
    for (const record of RECORDS) {
      expect(record.routing.parentTool).toBe('manage_character');
      expect(record.routing.dispatchAction).toBe(record.legacyIds[0].action);
    }
  });

  it('covers every action named in the handler contract table', () => {
    expect([...byAction.keys()].sort()).toEqual(Object.keys(CONTRACTS).sort());
  });

  it.each(Object.keys(CONTRACTS))('declares the exact input contract for %s', (action) => {
    const record = byAction.get(action);
    const contract = CONTRACTS[action];
    if (!record || !contract) {
      throw new Error(`No record or contract declared for manage_character action: ${action}`);
    }
    const schema = record.schemas.input;

    expect([...(schema.required ?? [])].sort()).toEqual([...contract.required].sort());
    expect(Object.keys(schema.properties ?? {}).sort())
      .toEqual([...contract.required, ...contract.optional].sort());
  });

  it('never reintroduces a generic placeholder in place of a real parameter', () => {
    // No native character handler reads any of these. `num_`, `radius` and
    // `speed` are what the union regression collapsed the named scalars onto.
    const placeholders = ['num_', 'radius', 'speed', 'properties', 'params', 'value'];
    for (const record of RECORDS) {
      const declared = Object.keys(record.schemas.input.properties ?? {});
      expect(declared.filter((name) => placeholders.includes(name))).toEqual([]);
    }
  });
});
