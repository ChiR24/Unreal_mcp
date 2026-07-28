/**
 * Animation/Physics authoring records — part 1 of 3 (creation, blend, state).
 *
 * Grounded in animation_physics action enum and native HandleAnimationPhysicsAction
 * (Plugins/.../Private/Domains/Animation/McpAutomationBridge_AnimationHandlers.cpp)
 * plus the TS special handlers (animation-special-handlers.ts,
 * animation-physics-actions.ts). Authoring actions mutate Animation Blueprint /
 * asset assets in the editor (editorState 'edit'); ragdoll actions are
 * editor-build-gated (see ragdoll.data.ts). Alias actions normalize to a
 * canonical verb per the TS special-handler alias map.
 *
 * Graph actions accept either `blueprintPath` or `assetPath`; both are resolved
 * by graph-handlers.ts. `skeletonPath` is primary, `targetSkeleton` its alias
 * (animation-special-handlers.ts).
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { A } from './animation-properties.js';

const T = 'animation_physics';
const F = 'animation';
const W = ['An animation asset or graph must be authored.'];
const ESU = ['EditorScriptingUtilities'];
const CR_ESU = ['ControlRig', 'EditorScriptingUtilities'];

export const ANIM_AUTHORED_1: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_animation_blueprint`, action: 'create_animation_blueprint', family: F,
    summary: 'Create an Animation Blueprint asset.', whenToUse: W, whenNotToUse: ['A SkeletalMesh suffices.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, targetSkeleton: P.skeletonPath, parentClass: P.string_ },
    required: ['action', 'name'], effect: 'write', latency: 'interactive', resources: 'medium', plugins: CR_ESU,
    outputProps: { assetPath: P.assetPath }, outputRequired: ['assetPath'],
    exampleInput: { action: 'create_animation_blueprint', name: 'ABP_Char', skeletonPath: '/Game/SK_Char', parentClass: 'Actor' }, exampleOutput: { success: true, message: 'Animation Blueprint created', assetPath: '/Game/ABP_Char' } }),
  buildRecord({ parentTool: T, id: `${T}.create_animation_bp`, action: 'create_animation_bp', family: F,
    summary: 'Alias of create_animation_blueprint (short form).', whenToUse: ['Caller uses the create_animation_bp verb.'], whenNotToUse: ['Use create_animation_blueprint.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, targetSkeleton: P.skeletonPath, parentClass: P.string_ }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: CR_ESU,
    normalizationClass: 'B_ALIAS', normalizationDisposition: 'alias', normalizationRationale: 'Alias of create_animation_blueprint (animation-special-handlers.ts).', normalizationAliasOf: `${T}.create_animation_blueprint`,
    exampleInput: { action: 'create_animation_bp', name: 'ABP_Char', skeletonPath: '/Game/SK_Char' }, exampleOutput: { success: true, message: 'Animation Blueprint created' } }),
  buildRecord({ parentTool: T, id: `${T}.create_anim_blueprint`, action: 'create_anim_blueprint', family: F,
    summary: 'Alias of create_animation_blueprint (short form).', whenToUse: ['Caller uses the create_anim_blueprint verb.'], whenNotToUse: ['Use create_animation_blueprint.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, targetSkeleton: P.skeletonPath, parentClass: P.string_ }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: CR_ESU,
    normalizationClass: 'B_ALIAS', normalizationDisposition: 'alias', normalizationRationale: 'Alias of create_animation_blueprint (animation-special-handlers.ts).', normalizationAliasOf: `${T}.create_animation_blueprint`,
    exampleInput: { action: 'create_anim_blueprint', name: 'ABP_Char', skeletonPath: '/Game/SK_Char' }, exampleOutput: { success: true, message: 'Animation Blueprint created' } }),
  buildRecord({ parentTool: T, id: `${T}.create_blend_space`, action: 'create_blend_space', family: F,
    summary: 'Create a Blend Space asset.', whenToUse: W, whenNotToUse: ['Use create_blend_space_1d for one axis.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, targetSkeleton: P.skeletonPath }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'create_blend_space', name: 'BS_Locomotion', skeletonPath: '/Game/SK_Char' }, exampleOutput: { success: true, message: 'Blend Space created' } }),
  buildRecord({ parentTool: T, id: `${T}.create_blend_space_1d`, action: 'create_blend_space_1d', family: F,
    summary: 'Create a 1D Blend Space asset.', whenToUse: ['A single-axis blend is needed.'], whenNotToUse: ['Use create_blend_space for 2D.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, targetSkeleton: P.skeletonPath }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'create_blend_space_1d', name: 'BS1D_Speed', skeletonPath: '/Game/SK_Char' }, exampleOutput: { success: true, message: 'Blend Space 1D created' } }),
  buildRecord({ parentTool: T, id: `${T}.create_blend_space_2d`, action: 'create_blend_space_2d', family: F,
    summary: 'Create a 2D Blend Space asset.', whenToUse: ['A two-axis blend is needed.'], whenNotToUse: ['Use create_blend_space for grid.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, targetSkeleton: P.skeletonPath }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'create_blend_space_2d', name: 'BS2D_DirSpeed', skeletonPath: '/Game/SK_Char' }, exampleOutput: { success: true, message: 'Blend Space 2D created' } }),
  buildRecord({ parentTool: T, id: `${T}.create_blend_tree`, action: 'create_blend_tree', family: F,
    summary: 'Create a blend tree node asset.', whenToUse: ['A reusable blend tree is needed.'], whenNotToUse: ['Inline blend suffices.'],
    inputProps: { action: P.action, name: P.name, path: P.path, blueprintPath: P.blueprintPath }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'create_blend_tree', name: 'BT_Move', blueprintPath: '/Game/ABP_Char' }, exampleOutput: { success: true, message: 'Blend tree created' } }),
  buildRecord({ parentTool: T, id: `${T}.create_procedural_anim`, action: 'create_procedural_anim', family: F,
    summary: 'Create a procedural anim asset.', whenToUse: ['A runtime-authored animation is needed.'], whenNotToUse: ['Use a static sequence.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, boneTracks: A.boneTracks, frameRate: P.frameRate }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'create_procedural_anim', name: 'PA_Wave', skeletonPath: '/Game/SK_Char', frameRate: 30 }, exampleOutput: { success: true, message: 'Procedural anim created' } }),
  buildRecord({ parentTool: T, id: `${T}.create_aim_offset`, action: 'create_aim_offset', family: F,
    summary: 'Create an Aim Offset asset.', whenToUse: ['Aim blending by look direction is needed.'], whenNotToUse: ['Use a blend space.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, targetSkeleton: P.skeletonPath }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'create_aim_offset', name: 'AO_Aim', skeletonPath: '/Game/SK_Char' }, exampleOutput: { success: true, message: 'Aim Offset created' } }),
  buildRecord({ parentTool: T, id: `${T}.add_aim_offset_sample`, action: 'add_aim_offset_sample', family: F,
    summary: 'Add a sample pose to an Aim Offset.', whenToUse: ['Aim sample must be added.'], whenNotToUse: ['Use create_aim_offset.'],
    inputProps: { action: P.action, assetPath: P.assetPath, animationPath: P.animationPath, yaw: A.yaw, pitch: A.pitch }, required: ['action', 'assetPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_aim_offset_sample', assetPath: '/Game/AO_Aim', animationPath: '/Game/A_Idle', yaw: 0, pitch: 0 },
    exampleOutput: { success: true, message: 'Aim sample added' } }),
  buildRecord({ parentTool: T, id: `${T}.create_state_machine`, action: 'create_state_machine', family: F,
    summary: 'Create a State Machine graph asset.', whenToUse: ['State-driven animation is needed.'], whenNotToUse: ['A single graph suffices.'],
    inputProps: { action: P.action, path: P.path, blueprintPath: P.blueprintPath, machineName: P.machineName }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'create_state_machine', blueprintPath: '/Game/ABP_Char', machineName: 'SM_Locomotion' }, exampleOutput: { success: true, message: 'State machine created' } }),
  buildRecord({ parentTool: T, id: `${T}.add_state_machine`, action: 'add_state_machine', family: F,
    summary: 'Add a State Machine node to an AnimGraph.', whenToUse: ['A state machine must be embedded.'], whenNotToUse: ['Use create_state_machine.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, stateMachineName: A.stateMachineName, machineName: P.machineName }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_state_machine', blueprintPath: '/Game/ABP_Char', stateMachineName: 'SM_Loco' },
    exampleOutput: { success: true, message: 'State machine node added' } }),
  buildRecord({ parentTool: T, id: `${T}.add_state`, action: 'add_state', family: F,
    summary: 'Add a state to a State Machine.', whenToUse: ['A new animation state is needed.'], whenNotToUse: ['Use add_transition.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, stateMachineName: A.stateMachineName, stateName: P.stateName }, required: ['action', 'stateName'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_state', blueprintPath: '/Game/ABP_Char', stateMachineName: 'SM_Loco', stateName: 'Idle' },
    exampleOutput: { success: true, message: 'State added' } }),
  buildRecord({ parentTool: T, id: `${T}.add_transition`, action: 'add_transition', family: F,
    summary: 'Add a transition between two states.', whenToUse: ['States must connect.'], whenNotToUse: ['Use add_state.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, stateMachineName: A.stateMachineName, fromState: P.fromState, toState: P.toState }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_transition', blueprintPath: '/Game/ABP_Char', stateMachineName: 'SM_Loco', fromState: 'Idle', toState: 'Run' },
    exampleOutput: { success: true, message: 'Transition added' } }),
  buildRecord({ parentTool: T, id: `${T}.set_transition_rules`, action: 'set_transition_rules', family: F,
    summary: 'Set transition rule logic.', whenToUse: ['Transition condition must change.'], whenNotToUse: ['Use add_transition.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, stateMachineName: A.stateMachineName, fromState: P.fromState, toState: P.toState, blendTime: P.blendTime }, required: ['action'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'set_transition_rules', blueprintPath: '/Game/ABP_Char', stateMachineName: 'SM_Loco', fromState: 'Idle', toState: 'Run', blendTime: 0.2 },
    exampleOutput: { success: true, message: 'Transition rules set' } }),
  buildRecord({ parentTool: T, id: `${T}.add_blend_node`, action: 'add_blend_node', family: F,
    summary: 'Add a blend node to an AnimGraph.', whenToUse: ['Graph needs a blend node.'], whenNotToUse: ['Use create_blend_tree.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, blendType: A.blendType, nodeName: P.nodeName }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_blend_node', blueprintPath: '/Game/ABP_Char', blendType: 'TwoWayBlend', nodeName: 'Blend_0' }, exampleOutput: { success: true, message: 'Blend node added' } }),
  buildRecord({ parentTool: T, id: `${T}.add_cached_pose`, action: 'add_cached_pose', family: F,
    summary: 'Add a cached pose node.', whenToUse: ['Pose reuse optimization needed.'], whenNotToUse: ['Inline pose suffices.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, cacheName: P.cacheName }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_cached_pose', blueprintPath: '/Game/ABP_Char', cacheName: 'LocomotionPose' }, exampleOutput: { success: true, message: 'Cached pose added' } }),
  buildRecord({ parentTool: T, id: `${T}.add_slot_node`, action: 'add_slot_node', family: F,
    summary: 'Add an animation slot node.', whenToUse: ['Montage slot must be exposed.'], whenNotToUse: ['Use create_montage.'],
    inputProps: { action: P.action, assetPath: P.assetPath, blueprintPath: P.blueprintPath, slotName: P.slotName }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_slot_node', blueprintPath: '/Game/ABP_Char', slotName: 'DefaultSlot' },
    exampleOutput: { success: true, message: 'Slot node added' } }),
  buildRecord({ parentTool: T, id: `${T}.create_control_rig`, action: 'create_control_rig', family: F,
    summary: 'Create a Control Rig asset.', whenToUse: ['Procedural rigging/IK needed (ControlRig).'], whenNotToUse: ['Use create_ik_rig.'],
    inputProps: { action: P.action, name: P.name, path: P.path, skeletonPath: P.skeletonPath, targetSkeleton: P.skeletonPath }, required: ['action', 'name'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: ['ControlRig', 'RigVM', 'EditorScriptingUtilities'],
    exampleInput: { action: 'create_control_rig', name: 'CR_Char', skeletonPath: '/Game/SK_Char' }, exampleOutput: { success: true, message: 'Control Rig created' } }),
];
