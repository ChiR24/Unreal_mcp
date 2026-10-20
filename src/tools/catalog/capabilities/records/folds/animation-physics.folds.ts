// Fold specs for animation_physics. Data only; see ../shared/fold.ts.
import { byName } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const ANIMATION_PHYSICS_FOLDS: readonly FoldSpec[] = [
  { primary: 'play_montage', summary: 'Play an animation montage on a placed skeletal actor (the playing one while Play In Editor runs): Montage_Play through its Anim Instance at playRate, or the montage as a single-node animation when the mesh has none, which switches that component\'s animation mode. The reply gives playMode and montageLength; outside play the actor does not tick, so nothing moves, playing is false and a warning says so.', members: ['play_anim_montage'] },
  {
    primary: 'setup_ragdoll', topics: ['go limp', 'make body go limp', 'make character go limp'], selector: 'ragdoll',
    summary: 'Make a skeletal actor go limp in the running game (Play or Simulate), chosen by ragdoll. setup switches every physics body of its skeletal mesh to simulate, assigning physicsAssetPath first when given; activate turns that on or off (activate, default true). A mesh with no physics asset is refused (NO_PHYSICS_ASSET), and ragdollActive reads back whether it now simulates. A Character keeps its capsule and movement running, so stop those as well for a body that falls.',
    members: { setup: 'setup_ragdoll', activate: 'activate_ragdoll' },
  },
  {
    primary: 'create_animation_asset', selector: 'kind',
    summary: 'Create an animation asset: a sequence, montage, blend space (1D/2D), aim offset, pose asset from a sequence, or procedural animation.',
    topics: ['create animation', 'create montage', 'create blend space', 'create aim offset', 'pose asset', 'procedural animation', 'new animation sequence'],
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
    topics: ['montage section', 'montage slot', 'montage notify', 'blend in', 'blend out', 'link sections', 'blend in time'],
    members: byName(['add_montage_notify', 'add_montage_section', 'add_montage_slot', 'link_sections', 'set_blend_in', 'set_blend_out', 'set_section_timing']),
  },
  {
    primary: 'edit_anim_graph', selector: 'edit',
    summary: 'Edit an Animation Blueprint graph: blend, cached-pose and slot nodes, state machines, states, transitions and transition rules, blend trees. Any other AnimGraph node (Two Bone IK, Modify Bone, Blend Space Player) is created by class with manage_blueprint edit_graph create_node.',
    topics: ['anim graph', 'state machine', 'add state', 'add transition', 'blend node', 'blend tree', 'cached pose', 'slot node', 'add walk state', 'locomotion states'],
    members: byName(['add_blend_node', 'add_cached_pose', 'add_slot_node', 'create_state_machine', 'add_state_machine',
      'add_state', 'add_transition', 'set_transition_rules', 'delete_transition', 'create_blend_tree']),
  },
  {
    primary: 'edit_blend_space', selector: 'edit',
    summary: 'Edit a Blend Space or Aim Offset (assetPath), chosen by edit. add_sample places animationPath at sampleValue (a number on a 1D blend space, {x, y} on a 2D one; the axis range grows to fit) and returns sampleIndex; a position another sample holds, or an animation of another skeleton or additive type, is refused (SAMPLE_REJECTED). set_axis_settings changes one axis (Horizontal, the default, or Vertical; a 1D blend space has Horizontal only): only the axisName, minValue, maxValue and gridDivisions sent change, minValue stays below maxValue, and the reply gives the axis as it now is. set_interpolation_settings sets input smoothing on every axis and the sample weight speed, only the fields sent. rebuild drops invalid samples, rebuilds the grid and recompiles the Animation Blueprints that use it (compileReferencers, default true), listing any that fail. Saved unless save is false (saved).',
    topics: ['blend space sample', 'blend space axis', 'interpolation settings', 'rebuild blend space'],
    members: { add_sample: 'add_blend_sample', set_axis_settings: 'set_axis_settings', set_interpolation_settings: 'set_interpolation_settings', rebuild: 'force_rebuild_blend_space' },
  },
  {
    primary: 'configure_anim_graph_node', selector: 'edit',
    summary: 'Configure an AnimGraph node: add a layered-blend-per-bone node or set a node property value.',
    topics: ['layered blend per bone', 'anim graph node value', 'blend upper body only'],
    members: { add_layered_blend_per_bone: 'add_layered_blend_per_bone', set_value: 'set_anim_graph_node_value' },
  },
  {
    primary: 'edit_skeleton', selector: 'edit',
    summary: 'Edit a skeleton (skeletonPath), chosen by edit. add_bone adds boneName under parentBoneName (needed once the skeleton has bones; a taken name is BONE_EXISTS) at location, rotation and scale relative to the parent. set_bone_parent moves boneName under parentBoneName (UE 5.3 and later): its own descendants are refused, and the parent it already has changes nothing (changed false). Both change the skeleton asset only; the skeletal meshes on it keep their own bones. set_bone_transform rewrites boneName in the reference pose of the skeletal mesh (skeletalMeshPath, or the skeleton\'s preview mesh), only the location, rotation or scale sent. create_virtual_bone adds a virtual bone from sourceBoneName to targetBoneName named boneName, or VB <source>_<target>; the engine\'s VB prefix is added when missing. rename_bone renames virtual bones only; a real bone takes a reimport of the mesh (OPERATION_NOT_SUPPORTED). Saved unless save is false (saved).',
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
    summary: 'Create a physics asset or edit it, chosen by edit; each change is saved unless save is false (saved). create builds bodies and joints from skeletalMeshPath as the editor\'s Create Physics Asset does, at outputPath (or name in path; default <mesh>_PhysicsAsset beside the mesh): geomType Capsule (default), Box, Sphere, TaperedCapsule (cloth only), SingleConvexHull or MultiConvexHull, any other name refused; bones shorter than minBoneSize get no body unless bodyForAll; createConstraints joins the bodies; assignToMesh (default false) makes the mesh use it; a build that makes no body is refused (BODY_GENERATION_FAILED). add_body gives boneName a Capsule (default), Sphere or Box shape sized by radius and length or width, depth and height, placed by center and rotation; a bone that already has a body gets the shape added to it (created false), and a bone the skeleton lacks is BONE_NOT_FOUND. configure_body (modify_body) sets mass (kg, above 0, as the body\'s mass override), linearDamping, angularDamping, collisionEnabled and simulatePhysics (true simulates with the ragdoll, false is Kinematic and follows the animation), and refuses a call that sets none of them. add_constraint joins bodyA and bodyB with a joint Limited to 45 degrees on every axis (both bodies must exist, BODY_NOT_FOUND; a second joint on the pair is CONSTRAINT_EXISTS); set_constraint changes the pair\'s joint or makes one; configure_constraint_limits sets swing1, swing2 and twist angles and motions from limits. assign makes skeletalMeshPath (or meshPath) use physicsAssetPath and saves the mesh.',
    topics: ['physics asset', 'physics body', 'physics constraint', 'constraint limits', 'assign physics asset', 'skeleton physics', 'preview physics', 'add collision to bones', 'assign physics asset to mesh'],
    members: {
      create: 'create_physics_asset', add_body: 'add_physics_body', configure_body: 'configure_physics_body', modify_body: 'modify_physics_body',
      add_constraint: 'add_physics_constraint', set_constraint: 'set_physics_constraint', configure_constraint_limits: 'configure_constraint_limits',
      assign: 'set_physics_asset',
    },
  },
  {
    primary: 'edit_skin_weights', selector: 'edit',
    summary: 'Edit skeletal-mesh skin weights: auto-skin, set vertex weights, copy them from another skinned mesh, mirror one side onto the other, or prune tiny influences. Weights are always normalized on write.',
    topics: ['skin weights', 'vertex weights', 'auto skin', 'copy weights', 'mirror weights', 'prune weights'],
    members: { auto: 'auto_skin_weights', set: 'set_vertex_weights', copy: 'copy_weights', mirror: 'mirror_weights', prune: 'prune_weights' },
  },
  {
    primary: 'edit_morph_target', selector: 'edit',
    summary: 'Create a morph target, set its deltas, import morph targets from another mesh, or set a morph target value on an actor.',
    topics: ['morph target', 'morph target deltas', 'import morph targets', 'morph target value'],
    members: { create: 'create_morph_target', set_deltas: 'set_morph_target_deltas', import: 'import_morph_targets', set_value: 'set_morph_target_value' },
  },
  {
    primary: 'bind_cloth_to_skeletal_mesh', topics: ['add cloth simulation'], selector: 'clothOp',
    summary: 'Bind a cloth asset to a skeletal mesh section so that section simulates as cloth, chosen by clothOp. bind takes a cloth asset already on the mesh (clothAssetName, as availableClothAssets lists them); assign also takes clothAssetPath and adds that asset to the mesh first. meshLodIndex, sectionIndex and assetLodIndex (default 0 each) pick where; a LOD or section that does not exist is BIND_FAILED, and naming no asset lists the candidates (CLOTH_ASSET_REQUIRED). Saved unless save is false (saved).',
    members: { bind: 'bind_cloth_to_skeletal_mesh', assign: 'assign_cloth_asset_to_mesh' },
  },
  { primary: 'create_animation_blueprint', summary: 'Create an Animation Blueprint named name in path (default /Game/Blueprints) for the skeleton at skeletonPath, or the one skeletalMeshPath is built on (SKELETON_NOT_FOUND when that does not load; without either it has no skeleton), from parentClass (default AnimInstance; a class that is no AnimInstance is INVALID_PARENT_CLASS). An Animation Blueprint already there is kept and answered with existingAsset; any other asset there is ASSET_TYPE_MISMATCH and is left alone. It is compiled and saved unless save is false (saved).', members: ['create_anim_blueprint', 'create_animation_bp'] },
  {
    primary: 'setup_ik', selector: 'kind',
    summary: 'Set up IK: create an IK rig or IK retargeter, a Control Rig for IK solvers, or set which source chain drives each target chain of a retargeter.',
    topics: ['ik rig', 'ik retargeter', 'inverse kinematics', 'retarget chain mapping', 'full body ik'],
    members: { setup: 'setup_ik', rig: 'create_ik_rig', retargeter: 'create_ik_retargeter', chain_mapping: 'set_retarget_chain_mapping' },
  },
  {
    primary: 'get_skeleton_info', selector: 'info',
    summary: 'Read skeleton data: summary, bones, sockets, virtual bones, bone transforms, morph targets, physics asset and bodies.',
    topics: ['skeleton info', 'list bones', 'list sockets', 'bone transform', 'morph targets', 'physics bodies', 'virtual bones', 'check skeleton bones'],
    members: {
      skeleton: 'get_skeleton_info', bones: 'list_bones', sockets: 'list_sockets', virtual_bones: 'list_virtual_bones',
      bone_transform: 'get_bone_transform', morph_targets: 'list_morph_targets', physics_asset: 'get_physics_asset_info', physics_bodies: 'list_physics_bodies',
    },
  },
  {
    primary: 'remove_skeleton_element', selector: 'element',
    summary: 'Remove part of a skeleton or physics asset, chosen by element. bone removes boneName from the skeleton (UE 5.3 and later): with removeChildren its whole branch goes, otherwise its children move up to its parent, and the root bone is refused. socket removes socketName from the skeleton (a socket defined on a mesh is not found here). physics_body removes the body of boneName from physicsAssetPath together with every constraint that uses it. A missing bone, socket or body is refused. Saved unless save is false (saved).',
    topics: ['remove bone', 'remove socket', 'remove physics body'],
    members: { bone: 'remove_bone', socket: 'remove_socket', physics_body: 'remove_physics_body' },
  },
];
