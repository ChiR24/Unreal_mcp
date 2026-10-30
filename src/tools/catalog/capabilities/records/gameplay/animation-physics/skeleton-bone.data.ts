/**
 * Skeleton bone authoring: create_skeleton plus bone and virtual-bone edits,
 * grounded in the animation_physics SKELETON_ACTIONS enum and native
 * HandleManageSkeleton.
 */

import type { CapabilityRecordSource } from '../../../model.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { A } from './animation-properties.js';

const T = 'animation_physics';
const F = 'skeleton';
const W = ['A skeleton, bone, socket, physics body or morph target must be authored.'];
const ESU = ['EditorScriptingUtilities'];
const SKEL_BONE_REQUIRED = ['skeletonPath', 'boneName'];

export const SKELETON_BONE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_skeleton`, action: 'create_skeleton', topics: ['new skeleton asset'], family: F,
    summary: 'Create a Skeleton asset holding one root bone (rootBoneName, default Root) at path, or path/name when name is given (without name the last part of path names it), under /Game, /Engine or /Temp. Saved unless save is false; the reply\'s saved says whether it reached disk (a /Temp skeleton never does).',
    whenToUse: W, whenNotToUse: ['A skeletal mesh already has its skeleton: build on that one (add_bone).'],
    inputProps: { name: P.name, path: P.path, rootBoneName: A.rootBoneName, save: A.save }, required: [],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: ESU,
    exampleInput: { action: 'create_skeleton', path: '/Game/SK_Char', rootBoneName: 'Root', save: true } }),
  buildRecord({ parentTool: T, id: `${T}.add_bone`, action: 'add_bone', family: F,
    summary: 'Add boneName to a skeleton under parentBoneName at location, rotation and scale relative to its parent (scale a number for uniform, or {x, y, z}); a skeleton with bones refuses a new root (PARENT_REQUIRED), and a taken name is BONE_EXISTS. One undo step (the reply\'s undo block says so), saved unless save is false; the reply gives boneCount.', whenToUse: W, whenNotToUse: ['Bone already exists.'],
    inputProps: { skeletonPath: P.skeletonPath, boneName: P.boneName, parentBoneName: P.parentBoneName, location: P.location, rotation: P.rotation, scale: A.scale, save: A.save },
    required: SKEL_BONE_REQUIRED, effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'add_bone', skeletonPath: '/Game/SK_Char', boneName: 'spine_01', parentBoneName: 'Root', location: [10, 0, 0], save: true },
    exampleOutput: { success: true, message: 'Bone added' } }),
  buildRecord({ parentTool: T, id: `${T}.remove_bone`, action: 'remove_bone', family: F,
    summary: 'Remove boneName from a skeleton, with its children when removeChildren is true; otherwise its children move up to its parent and keep their place in the reference pose. A virtual bone is BONE_NOT_FOUND (delete_virtual_bone removes it). The only root a skeleton has is refused (CANNOT_REMOVE_ROOT); a second root is removed like any bone. One undo step (the reply\'s undo block says so), saved unless save is false; the reply gives boneCount.', whenToUse: ['A bone must be removed.'], whenNotToUse: ['Keep the bone.'],
    inputProps: { skeletonPath: P.skeletonPath, boneName: P.boneName, removeChildren: A.removeChildren, save: A.save }, required: SKEL_BONE_REQUIRED,
    effect: 'destructive', behavior: { safeToRetry: false }, latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'remove_bone', skeletonPath: '/Game/SK_Char', boneName: 'tail_01', removeChildren: true, save: true },
    exampleOutput: { success: true, message: 'Bone removed' } }),
  buildRecord({ parentTool: T, id: `${T}.rename_bone`, action: 'rename_bone', family: F,
    summary: 'Rename a virtual bone on a skeleton, in one undo step (the VB prefix is added when newBoneName lacks it; real skeleton bones cannot be renamed and answer OPERATION_NOT_SUPPORTED).', whenToUse: ['A virtual bone needs renaming.'], whenNotToUse: ['Name is correct.', 'The bone is a real skeleton bone.'],
    inputProps: { skeletonPath: P.skeletonPath, boneName: P.boneName, newBoneName: P.newBoneName, save: A.save },
    required: SKEL_BONE_REQUIRED, effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'rename_bone', skeletonPath: '/Game/SK_Char', boneName: 'old', newBoneName: 'new', save: true },
    exampleOutput: { success: true, message: 'Bone renamed' } }),
  buildRecord({ parentTool: T, id: `${T}.set_bone_transform`, action: 'set_bone_transform', family: F,
    summary: 'Set the rest transform of a bone.', whenToUse: ['A bone rest pose must change.'], whenNotToUse: ['Use runtime animation.'],
    inputProps: { skeletonPath: P.skeletonPath, skeletalMeshPath: P.skeletalMeshPath, boneName: P.boneName, location: P.location, rotation: P.rotation, scale: A.scale, save: A.save },
    required: ['boneName'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'set_bone_transform', skeletalMeshPath: '/Game/SM_Char', boneName: 'spine_01', location: [0, 0, 0], scale: [1, 1, 1] },
    exampleOutput: { success: true, message: 'Bone transform set' } }),
  buildRecord({ parentTool: T, id: `${T}.set_bone_parent`, action: 'set_bone_parent', family: F,
    summary: 'Move boneName, with its children, under the existing bone parentBoneName, where it keeps its place in the reference pose; the reply gives newBoneIndex (bones can be reordered) and changed. The parent it already has answers changed false and writes nothing; the bone itself or one of its descendants as parent is INVALID_ARGUMENT, a bone not in the skeleton BONE_NOT_FOUND or PARENT_NOT_FOUND. One undo step (the reply\'s undo block says so), saved unless save is false.', whenToUse: ['Bone hierarchy must change.'], whenNotToUse: ['Hierarchy is correct.'],
    inputProps: { skeletonPath: P.skeletonPath, boneName: P.boneName, parentBoneName: { type: 'string', description: 'Existing bone to become the new parent.' }, save: A.save },
    required: [...SKEL_BONE_REQUIRED, 'parentBoneName'], effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'set_bone_parent', skeletonPath: '/Game/SK_Char', boneName: 'tail_01', parentBoneName: 'spine_03', save: true },
    exampleOutput: { success: true, message: 'Bone parent set' } }),
  buildRecord({ parentTool: T, id: `${T}.create_virtual_bone`, action: 'create_virtual_bone', family: F,
    summary: 'Add a virtual bone to a skeleton: a bone that follows sourceBoneName and points at targetBoneName (an IK or aim target that moves with the animation), named VB sourceBoneName_targetBoneName or boneName, in one undo step, saved unless save is false. A bone the skeleton lacks is BONE_NOT_FOUND, and a virtual bone from the same source to the same target already there VIRTUAL_BONE_FAILED.', whenToUse: ['An IK or aim target should follow one bone toward another (a hand IK target, a look-at point).'], whenNotToUse: ['A real bone in the hierarchy: add_bone.'],
    inputProps: { skeletonPath: P.skeletonPath, boneName: P.boneName, sourceBoneName: P.sourceBoneName, targetBoneName: P.targetBoneName, save: A.save },
    required: ['skeletonPath', 'sourceBoneName', 'targetBoneName'], effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'create_virtual_bone', skeletonPath: '/Game/SK_Char', sourceBoneName: 'hand_l', targetBoneName: 'hand_r', boneName: 'Hands', save: true },
    exampleOutput: { success: true, message: 'Virtual bone created' } }),
];
