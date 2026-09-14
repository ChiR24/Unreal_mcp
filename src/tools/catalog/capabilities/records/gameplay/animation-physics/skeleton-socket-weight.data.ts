/**
 * Skeleton sockets and skinning: socket create/configure plus the vertex weight
 * pipeline, grounded in the animation_physics SKELETON_ACTIONS enum.
 */

import type { CapabilityRecordSource } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { A } from './animation-properties.js';

const T = 'animation_physics';
const F = 'skeleton';
const ESU = ['EditorScriptingUtilities'];
const MESH_REQUIRED = ['action', 'skeletalMeshPath'];

export const SKELETON_SOCKET_WEIGHT_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_socket`, action: 'create_socket', family: F,
    summary: 'Create a socket on a bone.', whenToUse: ['An attach point is needed.'], whenNotToUse: ['Use an existing socket.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, socketName: P.socketName, attachBoneName: A.attachBoneName, relativeLocation: A.relativeLocation, relativeRotation: A.relativeRotation, relativeScale: A.relativeScale, save: P.save },
    required: ['action', 'skeletonPath', 'socketName'], effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, outputProps: { socketName: P.socketName, boneName: P.boneName, skeletonPath: P.skeletonPath }, outputRequired: [],
    exampleInput: { action: 'create_socket', skeletonPath: '/Game/SK_Char', socketName: 'Weapon', attachBoneName: 'hand_r', relativeLocation: [1, 2, 3], save: true },
    exampleOutput: { success: true, message: 'Socket created' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_socket`, action: 'configure_socket', family: F,
    summary: 'Configure an existing socket transform.', whenToUse: ['Socket offset must change.'], whenNotToUse: ['Use create_socket.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, socketName: P.socketName, attachBoneName: A.attachBoneName, relativeLocation: A.relativeLocation, relativeRotation: A.relativeRotation, relativeScale: A.relativeScale, save: P.save },
    required: ['action', 'skeletonPath', 'socketName'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    plugins: ESU, outputProps: { socketName: P.socketName, skeletonPath: P.skeletonPath }, outputRequired: [],
    exampleInput: { action: 'configure_socket', skeletonPath: '/Game/SK_Char', socketName: 'Weapon', relativeLocation: [4, 5, 6], save: true },
    exampleOutput: { success: true, message: 'Socket configured' } }),
  buildRecord({ parentTool: T, id: `${T}.auto_skin_weights`, action: 'auto_skin_weights', family: F,
    summary: 'Auto-generate skin weights for a mesh against the skeleton.', whenToUse: ['A mesh needs skinning.'], whenNotToUse: ['Weights already exist.'],
    inputProps: { action: P.action, skeletalMeshPath: P.skeletalMeshPath, save: P.save }, required: MESH_REQUIRED,
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high', plugins: ESU,
    exampleInput: { action: 'auto_skin_weights', skeletalMeshPath: '/Game/SM_Char', save: false }, exampleOutput: { success: true, message: 'Skin weights generated' } }),
  buildRecord({ parentTool: T, id: `${T}.set_vertex_weights`, action: 'set_vertex_weights', family: F,
    summary: 'Set explicit vertex skin weights.', whenToUse: ['Manual weight painting is required.'], whenNotToUse: ['Auto-skin suffices.'],
    inputProps: { action: P.action, skeletalMeshPath: P.skeletalMeshPath, profileName: A.profileName, lodIndex: A.lodIndex, weights: A.weights, save: P.save },
    required: MESH_REQUIRED, effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high',
    plugins: ESU, exampleInput: { action: 'set_vertex_weights', skeletalMeshPath: '/Game/SM_Char', profileName: 'Default', lodIndex: 0, weights: [{ vertexIndex: 0, influences: [{ boneIndex: 0, weight: 1 }] }] },
    exampleOutput: { success: true, message: 'Vertex weights set' } }),
  buildRecord({ parentTool: T, id: `${T}.normalize_weights`, action: 'normalize_weights', family: F,
    summary: 'Normalize skin weights to sum to one.', whenToUse: ['Weights must be normalized.'], whenNotToUse: ['Weights already normalized.'],
    inputProps: { action: P.action, skeletalMeshPath: P.skeletalMeshPath, save: P.save }, required: MESH_REQUIRED,
    effect: 'write', behavior: { idempotency: 'idempotent', longRunning: true }, latency: 'long-running', resources: 'high',
    plugins: ESU, exampleInput: { action: 'normalize_weights', skeletalMeshPath: '/Game/SM_Char', save: false },
    exampleOutput: { success: true, message: 'Weights normalized' } }),
  buildRecord({ parentTool: T, id: `${T}.prune_weights`, action: 'prune_weights', family: F,
    summary: 'Prune low-influence bone weights.', whenToUse: ['Influences must be reduced.'], whenNotToUse: ['Keep all influences.'],
    inputProps: { action: P.action, skeletalMeshPath: P.skeletalMeshPath, threshold: P.threshold, save: P.save }, required: MESH_REQUIRED,
    effect: 'write', behavior: { idempotency: 'idempotent', longRunning: true }, latency: 'long-running', resources: 'high',
    plugins: ESU, exampleInput: { action: 'prune_weights', skeletalMeshPath: '/Game/SM_Char', threshold: 0.01, save: false },
    exampleOutput: { success: true, message: 'Weights pruned' } }),
  buildRecord({ parentTool: T, id: `${T}.copy_weights`, action: 'copy_weights', family: F,
    summary: 'Copy skin weights from a source mesh.', whenToUse: ['Reuse weights from another mesh.'], whenNotToUse: ['Generate fresh weights.'],
    inputProps: { action: P.action, sourceMeshPath: P.meshPath, targetMeshPath: A.targetMeshPath, profileName: A.profileName, lodIndex: A.lodIndex }, required: ['action', 'sourceMeshPath'],
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high', plugins: ESU,
    exampleInput: { action: 'copy_weights', sourceMeshPath: '/Game/SM_Source', targetMeshPath: '/Game/SM_Char', profileName: 'Default', lodIndex: 0 },
    exampleOutput: { success: true, message: 'Weights copied' } }),
  buildRecord({ parentTool: T, id: `${T}.mirror_weights`, action: 'mirror_weights', family: F,
    summary: 'Mirror skin weights across a symmetry axis.', whenToUse: ['Symmetric mesh needs mirrored weights.'], whenNotToUse: ['Asymmetric rig.'],
    inputProps: { action: P.action, skeletalMeshPath: P.skeletalMeshPath, axis: A.axis, profileName: A.profileName, lodIndex: A.lodIndex, save: P.save }, required: MESH_REQUIRED,
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high', plugins: ESU,
    exampleInput: { action: 'mirror_weights', skeletalMeshPath: '/Game/SM_Char', axis: 'X', profileName: 'Mirror', lodIndex: 0 },
    exampleOutput: { success: true, message: 'Weights mirrored' } }),
  buildRecord({ parentTool: T, id: `${T}.create_physics_asset`, action: 'create_physics_asset', family: F,
    summary: 'Create a PhysicsAsset for a skeletal mesh.', whenToUse: ['Ragdoll/physics needed.'], whenNotToUse: ['PhysicsAsset exists.'],
    inputProps: { action: P.action, skeletonPath: P.skeletonPath, skeletalMeshPath: P.skeletalMeshPath, outputPath: A.outputPath, name: { type: 'string', description: 'Asset name (alternative to outputPath).' }, path: { type: 'string', description: 'Destination folder (with name).' }, geomType: { type: 'string', description: 'Body primitive: Sphyl, Box, Sphere, TaperedCapsule, MultiConvexHull or SingleConvexHull.' }, minBoneSize: { type: 'number', description: 'Bones smaller than this get no body.' }, createConstraints: { type: 'boolean', description: 'Create joint constraints between bodies.' }, bodyForAll: { type: 'boolean', description: 'Create a body for every bone regardless of size.' }, assignToMesh: { type: 'boolean', description: 'Assign the new asset to the skeletal mesh.' }, save: P.save }, required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: ESU,
    outputProps: { assetPath: P.assetPath, physicsAssetPath: P.physicsAssetPath }, outputRequired: [],
    exampleInput: { action: 'create_physics_asset', skeletalMeshPath: '/Game/SM_Char', outputPath: '/Game/PA_Char', save: true }, exampleOutput: { success: true, message: 'PhysicsAsset created', assetPath: '/Game/PA_Char' } }),
];
