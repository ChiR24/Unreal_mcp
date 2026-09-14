/**
 * Skeleton bone authoring: create_skeleton plus bone and virtual-bone edits,
 * grounded in the animation_physics SKELETON_ACTIONS enum and native
 * HandleManageSkeleton.
 */

import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { A } from './animation-properties.js';

const T = 'animation_physics';
const F = 'skeleton';
const W = ['A skeleton, bone, socket, physics body or morph target must be authored.'];
const ESU = ['EditorScriptingUtilities'];
const SKEL_BONE_REQUIRED = ['action', 'skeletonPath', 'boneName'];

export const SKELETON_BONE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_skeleton`, action: 'create_skeleton', family: F,
    summary: 'Create a new Skeleton asset.', whenToUse: W, whenNotToUse: ['A SkeletalMesh already exists.'],
    inputProps: { action: P.action, name: P.name, path: P.path, rootBoneName: A.rootBoneName, save: P.save }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: ESU,
    exampleInput: { action: 'create_skeleton', path: '/Game/SK_Char', rootBoneName: 'Root', save: true }, exampleOutput: { success: true, message: 'Skeleton created' } }),
  buildRecord({ parentTool: T, id: `${T}.add_bone`, action: 'add_bone', family: F,
    summary: 'Add a bone to a skeleton.', whenToUse: W, whenNotToUse: ['Bone already exists.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, boneName: P.boneName, parentBoneName: P.parentBoneName, location: P.location, rotation: P.rotation, scale: A.scale, save: P.save },
    required: SKEL_BONE_REQUIRED, effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'add_bone', skeletonPath: '/Game/SK_Char', boneName: 'spine_01', parentBoneName: 'Root', location: [10, 0, 0], save: true },
    exampleOutput: { success: true, message: 'Bone added' } }),
  buildRecord({ parentTool: T, id: `${T}.remove_bone`, action: 'remove_bone', family: F,
    summary: 'Remove a bone from a skeleton.', whenToUse: ['A bone must be removed.'], whenNotToUse: ['Keep the bone.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, boneName: P.boneName, removeChildren: A.removeChildren, save: P.save }, required: SKEL_BONE_REQUIRED,
    effect: 'destructive', behavior: { safeToRetry: false, supportsUndo: false }, latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'remove_bone', skeletonPath: '/Game/SK_Char', boneName: 'tail_01', removeChildren: true, save: true },
    exampleOutput: { success: true, message: 'Bone removed' } }),
  buildRecord({ parentTool: T, id: `${T}.rename_bone`, action: 'rename_bone', family: F,
    summary: 'Rename a virtual bone on a skeleton (real skeleton bones cannot be renamed and answer OPERATION_NOT_SUPPORTED).', whenToUse: ['A virtual bone needs renaming.'], whenNotToUse: ['Name is correct.', 'The bone is a real skeleton bone.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, boneName: P.boneName, newBoneName: P.newBoneName, save: P.save },
    required: SKEL_BONE_REQUIRED, effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'rename_bone', skeletonPath: '/Game/SK_Char', boneName: 'old', newBoneName: 'new', save: true },
    exampleOutput: { success: true, message: 'Bone renamed' } }),
  buildRecord({ parentTool: T, id: `${T}.set_bone_transform`, action: 'set_bone_transform', family: F,
    summary: 'Set the rest transform of a bone.', whenToUse: ['A bone rest pose must change.'], whenNotToUse: ['Use runtime animation.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, skeletalMeshPath: P.skeletalMeshPath, boneName: P.boneName, location: P.location, rotation: P.rotation, scale: A.scale, save: P.save },
    required: ['action', 'boneName'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'set_bone_transform', skeletalMeshPath: '/Game/SM_Char', boneName: 'spine_01', location: [0, 0, 0], scale: [1, 1, 1] },
    exampleOutput: { success: true, message: 'Bone transform set' } }),
  buildRecord({ parentTool: T, id: `${T}.set_bone_parent`, action: 'set_bone_parent', family: F,
    summary: 'Reparent a bone under a new parent.', whenToUse: ['Bone hierarchy must change.'], whenNotToUse: ['Hierarchy is correct.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, boneName: P.boneName, parentBoneName: P.parentBoneName, save: P.save },
    required: SKEL_BONE_REQUIRED, effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'set_bone_parent', skeletonPath: '/Game/SK_Char', boneName: 'tail_01', parentBoneName: 'spine_03', save: true },
    exampleOutput: { success: true, message: 'Bone parent set' } }),
  buildRecord({ parentTool: T, id: `${T}.create_virtual_bone`, action: 'create_virtual_bone', family: F,
    summary: 'Create a virtual bone between two bones.', whenToUse: ['A virtual bone is needed for IK.'], whenNotToUse: ['Use a real bone.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, boneName: P.boneName, sourceBoneName: P.sourceBoneName, targetBoneName: P.targetBoneName, save: P.save },
    required: ['action', 'skeletonPath', 'sourceBoneName'], effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'create_virtual_bone', skeletonPath: '/Game/SK_Char', sourceBoneName: 'hand_l', targetBoneName: 'hand_r', boneName: 'VB_Hands', save: true },
    exampleOutput: { success: true, message: 'Virtual bone created' } }),
];
