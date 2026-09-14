// Fold specs for animation_physics. Data only; see ../shared/fold.ts.
import { byName } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const ANIMATION_PHYSICS_FOLDS: readonly FoldSpec[] = [
  { primary: 'play_montage', summary: 'Play an animation montage on an actor.', members: ['play_anim_montage'] },
  {
    primary: 'setup_ragdoll', selector: 'ragdoll',
    summary: 'Set up ragdoll physics on an actor, or activate/deactivate an existing ragdoll.',
    members: { setup: 'setup_ragdoll', activate: 'activate_ragdoll' },
  },
  {
    primary: 'create_animation_asset', selector: 'kind',
    summary: 'Create an animation asset: a sequence, montage, blend space (1D/2D), aim offset, pose library or procedural animation.',
    topics: ['create animation', 'create montage', 'create blend space', 'create aim offset', 'create pose library', 'procedural animation'],
    members: {
      asset: 'create_animation_asset', sequence: 'create_animation_sequence', montage: 'create_montage',
      blend_space: 'create_blend_space', blend_space_1d: 'create_blend_space_1d', blend_space_2d: 'create_blend_space_2d',
      aim_offset: 'create_aim_offset', pose_library: 'create_pose_library', procedural: 'create_procedural_anim',
    },
  },
  {
    primary: 'edit_animation', selector: 'edit',
    summary: 'Edit an animation sequence: bone tracks and keys, curve keys, notifies, sync markers, additive and root-motion settings, length, aim-offset samples.',
    topics: ['add notify', 'add sync marker', 'set bone key', 'root motion', 'sequence length', 'edit animation sequence'],
    members: byName(['add_bone_track', 'set_bone_key', 'set_curve_key', 'add_notify', 'add_notify_state', 'add_sync_marker',
      'set_additive_settings', 'set_root_motion_settings', 'set_sequence_length', 'add_aim_offset_sample']),
  },
  {
    primary: 'edit_montage', selector: 'edit',
    summary: 'Edit an animation montage: notifies, sections, slots, section links, blend-in/out times and section timing.',
    topics: ['montage section', 'montage slot', 'montage notify', 'blend in', 'blend out', 'link sections'],
    members: byName(['add_montage_notify', 'add_montage_section', 'add_montage_slot', 'link_sections', 'set_blend_in', 'set_blend_out', 'set_section_timing']),
  },
  {
    primary: 'edit_anim_graph', selector: 'edit',
    summary: 'Edit an Animation Blueprint graph: blend, cached-pose and slot nodes, state machines, states, transitions and transition rules, blend trees.',
    topics: ['anim graph', 'state machine', 'add state', 'add transition', 'blend node', 'blend tree', 'cached pose', 'slot node'],
    members: byName(['add_blend_node', 'add_cached_pose', 'add_slot_node', 'create_state_machine', 'add_state_machine',
      'add_state', 'add_transition', 'set_transition_rules', 'create_blend_tree']),
  },
  {
    primary: 'edit_blend_space', selector: 'edit',
    summary: 'Edit a blend space: add samples, set axis and interpolation settings, force a rebuild.',
    topics: ['blend space sample', 'blend space axis', 'interpolation settings', 'rebuild blend space'],
    members: { add_sample: 'add_blend_sample', set_axis_settings: 'set_axis_settings', set_interpolation_settings: 'set_interpolation_settings', rebuild: 'force_rebuild_blend_space' },
  },
  {
    primary: 'configure_anim_graph_node', selector: 'edit',
    summary: 'Configure an AnimGraph node: add a layered-blend-per-bone node or set a node property value.',
    topics: ['layered blend per bone', 'anim graph node value'],
    members: { add_layered_blend_per_bone: 'add_layered_blend_per_bone', set_value: 'set_anim_graph_node_value' },
  },
  {
    primary: 'edit_skeleton', selector: 'edit',
    summary: 'Edit a skeleton: add, rename or reparent bones, set bone transforms, add virtual bones.',
    topics: ['add bone', 'rename bone', 'bone parent', 'bone transform', 'virtual bone'],
    members: byName(['add_bone', 'rename_bone', 'set_bone_parent', 'set_bone_transform', 'create_virtual_bone']),
  },
  {
    primary: 'configure_socket', selector: 'socketOp',
    summary: 'Configure a skeleton socket: upsert its attachment and offsets, or strictly add, create or modify one.',
    topics: ['skeleton socket', 'add socket', 'modify socket', 'create socket'],
    members: { configure: 'configure_socket', add: 'add_socket', create: 'create_socket', modify: 'modify_socket' },
  },
  {
    primary: 'edit_physics_asset', selector: 'edit',
    summary: 'Create a physics asset or edit its bodies and constraints, and assign it to a skeletal mesh.',
    topics: ['physics asset', 'physics body', 'physics constraint', 'constraint limits', 'assign physics asset', 'skeleton physics', 'preview physics'],
    members: {
      create: 'create_physics_asset', add_body: 'add_physics_body', configure_body: 'configure_physics_body', modify_body: 'modify_physics_body',
      add_constraint: 'add_physics_constraint', set_constraint: 'set_physics_constraint', configure_constraint_limits: 'configure_constraint_limits',
      assign: 'set_physics_asset',
    },
  },
  {
    primary: 'edit_skin_weights', selector: 'edit',
    summary: 'Edit skeletal-mesh skin weights: auto-skin, copy, mirror, normalize, prune or set vertex weights.',
    topics: ['skin weights', 'vertex weights', 'auto skin', 'mirror weights', 'copy weights'],
    members: { auto: 'auto_skin_weights', copy: 'copy_weights', mirror: 'mirror_weights', normalize: 'normalize_weights', prune: 'prune_weights', set: 'set_vertex_weights' },
  },
  {
    primary: 'edit_morph_target', selector: 'edit',
    summary: 'Create a morph target, set its deltas, or set a morph target value on an actor.',
    topics: ['morph target', 'morph target deltas', 'morph target value'],
    members: { create: 'create_morph_target', set_deltas: 'set_morph_target_deltas', set_value: 'set_morph_target_value' },
  },
  {
    primary: 'bind_cloth_to_skeletal_mesh', selector: 'clothOp',
    summary: 'Bind or assign a cloth asset to a skeletal mesh section.',
    members: { bind: 'bind_cloth_to_skeletal_mesh', assign: 'assign_cloth_asset_to_mesh' },
  },
  { primary: 'create_animation_blueprint', summary: 'Create an Animation Blueprint for a skeleton.', members: ['create_anim_blueprint', 'create_animation_bp'] },
  {
    primary: 'setup_ik', selector: 'kind',
    summary: 'Set up IK: create an IK rig or IK retargeter, or run the generic IK setup.',
    topics: ['ik rig', 'ik retargeter', 'inverse kinematics'],
    members: { setup: 'setup_ik', rig: 'create_ik_rig', retargeter: 'create_ik_retargeter' },
  },
  {
    primary: 'get_skeleton_info', selector: 'info',
    summary: 'Read skeleton data: summary, bones, sockets, virtual bones, bone transforms, morph targets, physics asset and bodies.',
    topics: ['skeleton info', 'list bones', 'list sockets', 'bone transform', 'morph targets', 'physics bodies', 'virtual bones'],
    members: {
      skeleton: 'get_skeleton_info', bones: 'list_bones', sockets: 'list_sockets', virtual_bones: 'list_virtual_bones',
      bone_transform: 'get_bone_transform', morph_targets: 'list_morph_targets', physics_asset: 'get_physics_asset_info', physics_bodies: 'list_physics_bodies',
    },
  },
  {
    primary: 'remove_skeleton_element', selector: 'element',
    summary: 'Remove a bone, socket or physics body from a skeleton or physics asset.',
    topics: ['remove bone', 'remove socket', 'remove physics body'],
    members: { bone: 'remove_bone', socket: 'remove_socket', physics_body: 'remove_physics_body' },
  },
];
