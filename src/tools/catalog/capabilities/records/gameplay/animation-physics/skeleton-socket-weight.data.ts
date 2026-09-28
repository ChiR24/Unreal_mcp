/**
 * Skeleton sockets and skinning: socket create/configure plus the vertex weight
 * pipeline, grounded in the animation_physics SKELETON_ACTIONS enum.
 */

import type { CapabilityRecordSource } from '../../../model.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { A } from './animation-properties.js';
import { num, str } from '../../shared/schema-props.js';

const T = 'animation_physics';
const F = 'skeleton';
const ESU = ['EditorScriptingUtilities'];
const MESH_REQUIRED = ['skeletalMeshPath'];
// What every source-data weight edit reports: per-bone influenced-vertex counts, morphs a rebuild dropped, the save.
const TRANSFER_OUT = {
  boneVertexCounts: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'before and after: bone name to the number of vertices it influences, read back from the source data.' },
  morphTargetsDropped: { type: 'array', items: { type: 'string' }, description: 'Render-only morph targets (made by set_morph_target_deltas) the rebuild removed; import_morph_targets writes morphs that survive.' },
  saved: { type: 'boolean', description: 'Whether the mesh was saved.' },
};

export const SKELETON_SOCKET_WEIGHT_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.create_socket`, action: 'create_socket', family: F,
    summary: 'Create a socket on a bone.', whenToUse: ['An attach point is needed.'], whenNotToUse: ['Use an existing socket.'],
    inputProps: { skeletonPath: P.skeletonPath, socketName: P.socketName, attachBoneName: A.attachBoneName, relativeLocation: A.relativeLocation, relativeRotation: A.relativeRotation, relativeScale: A.relativeScale, save: A.save },
    required: ['skeletonPath', 'socketName'], effect: 'write', latency: 'interactive', resources: 'low',
    plugins: ESU, outputProps: { socketName: P.socketName, boneName: P.boneName, skeletonPath: P.skeletonPath }, outputRequired: [],
    exampleInput: { action: 'create_socket', skeletonPath: '/Game/SK_Char', socketName: 'Weapon', attachBoneName: 'hand_r', relativeLocation: [1, 2, 3], save: true },
    exampleOutput: { success: true, message: 'Socket created' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_socket`, action: 'configure_socket', family: F,
    summary: 'Configure an existing socket transform.', whenToUse: ['Socket offset must change.'], whenNotToUse: ['Use create_socket.'],
    inputProps: { skeletonPath: P.skeletonPath, socketName: P.socketName, attachBoneName: A.attachBoneName, relativeLocation: A.relativeLocation, relativeRotation: A.relativeRotation, relativeScale: A.relativeScale, save: A.save },
    required: ['skeletonPath', 'socketName'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    plugins: ESU, outputProps: { socketName: P.socketName, skeletonPath: P.skeletonPath }, outputRequired: [],
    exampleInput: { action: 'configure_socket', skeletonPath: '/Game/SK_Char', socketName: 'Weapon', relativeLocation: [4, 5, 6], save: true },
    exampleOutput: { success: true, message: 'Socket configured' } }),
  buildRecord({ parentTool: T, id: `${T}.auto_skin_weights`, action: 'auto_skin_weights', family: F,
    summary: 'Recompute LOD 0 skin weights of a skeletal mesh by smooth binding to its own skeleton (UE 5.5+ with GeometryScripting).', whenToUse: ['A mesh needs skinning.'], whenNotToUse: ['Weights already exist.', 'The weights should be copied from a dressed body mesh (use skin_mesh_to_skeleton).'],
    inputProps: { skeletalMeshPath: P.skeletalMeshPath, save: A.save }, required: MESH_REQUIRED,
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high', plugins: ESU,
    exampleInput: { action: 'auto_skin_weights', skeletalMeshPath: '/Game/SM_Char', save: false } }),
  buildRecord({ parentTool: T, id: `${T}.set_vertex_weights`, action: 'set_vertex_weights', family: F,
    summary: 'Set explicit vertex skin weights.', whenToUse: ['Manual weight painting is required.'], whenNotToUse: ['Auto-skin suffices.'],
    inputProps: { skeletalMeshPath: P.skeletalMeshPath, profileName: A.profileName, lodIndex: A.lodIndex, weights: A.weights, save: A.save },
    required: MESH_REQUIRED, effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high',
    plugins: ESU, exampleInput: { action: 'set_vertex_weights', skeletalMeshPath: '/Game/SM_Char', profileName: 'Default', lodIndex: 0, weights: [{ vertexIndex: 0, influences: [{ boneIndex: 0, weight: 1 }] }] },
    exampleOutput: { success: true, message: 'Vertex weights set' } }),
  buildRecord({ parentTool: T, id: `${T}.copy_weights`, action: 'copy_weights', family: F,
    topics: ['copy skin weights', 'transfer weights', 'weight transfer'],
    summary: 'Replace the skin weights of an existing skeletal mesh with those of another skinned mesh (for example the body under a garment), vertex by nearest vertex, mapping bones by name; the mesh keeps its geometry, physics asset, sockets and LODs.',
    whenToUse: ['A new or re-modelled mesh must deform like an already skinned one.'], whenNotToUse: ['The mesh has no weights and no skinned mesh to copy from (edit auto).'],
    inputProps: {
      sourceMeshPath: str('Skinned mesh to copy the weights FROM (its LOD 0).'),
      targetMeshPath: str('Skeletal mesh whose weights are replaced in place.'),
      lodIndex: A.lodIndex, save: A.save,
    },
    required: ['sourceMeshPath', 'targetMeshPath'], effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high', plugins: ESU,
    outputProps: { ...TRANSFER_OUT, verticesWritten: num('Target vertices that took new weights.'), verticesFarFromSource: num('Target vertices farther than 2% of the source size from any source vertex; check those areas.'), unmappedBones: { type: 'array', items: { type: 'string' }, description: 'Source bones the target lacks; their weight went to the nearest parent bone it has.' } },
    exampleInput: { action: 'copy_weights', sourceMeshPath: '/Game/Characters/SK_Body', targetMeshPath: '/Game/Characters/SK_Jacket' } }),
  buildRecord({ parentTool: T, id: `${T}.mirror_weights`, action: 'mirror_weights', family: F,
    topics: ['mirror skin weights', 'symmetric weights'],
    summary: 'Mirror the skin weights of one side of a skeletal mesh onto the other, swapping left and right bones by name (_l and _r, Left and Right, .L and .R); vertices on the symmetry plane keep their weights.',
    whenToUse: ['One side of a symmetric character is weighted and the other must match.'], whenNotToUse: ['The mesh is not symmetric or has no left and right bones.'],
    inputProps: {
      skeletalMeshPath: P.skeletalMeshPath,
      axis: str('Symmetry axis in mesh space: X (default), Y or Z.'),
      direction: str('positive_to_negative (default: the positive side is copied onto the negative side) or negative_to_positive.'),
      lodIndex: A.lodIndex, save: A.save,
    },
    required: MESH_REQUIRED, effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high', plugins: ESU,
    outputProps: { ...TRANSFER_OUT, verticesMirrored: num('Vertices that took the mirrored weights.'), unmatchedVertices: num('Destination-side vertices with no mirror partner; they kept their weights.'), bonePairs: { type: 'array', items: { type: 'object', properties: { from: { type: 'string', description: 'Bone.' }, to: { type: 'string', description: 'Its opposite-side bone.' } }, additionalProperties: false }, description: 'Left and right bone pairs that were swapped.' } },
    exampleInput: { action: 'mirror_weights', skeletalMeshPath: '/Game/Characters/SK_Hero', axis: 'X' } }),
  buildRecord({ parentTool: T, id: `${T}.prune_weights`, action: 'prune_weights', family: F,
    topics: ['prune weights', 'remove small influences', 'clean skin weights'],
    summary: 'Remove skin influences below a weight threshold (each vertex keeps its strongest bone) and renormalize; a mesh with nothing below it is left untouched.',
    whenToUse: ['Auto-skinning or a transfer left tiny noisy influences.'], whenNotToUse: ['Joints should stay soft; a high threshold stiffens them.'],
    inputProps: { skeletalMeshPath: P.skeletalMeshPath, threshold: num('Influences below this weight are removed, above 0 and below 0.5 (default 0.01).'), lodIndex: A.lodIndex, save: A.save },
    required: MESH_REQUIRED, effect: 'write', behavior: { idempotency: 'idempotent', longRunning: true }, latency: 'long-running', resources: 'high', plugins: ESU,
    outputProps: { influencesRemoved: num('Influences removed.'), verticesChanged: num('Vertices that lost an influence.'), maxInfluencesBefore: num('Most influences on one vertex before.'), maxInfluencesAfter: num('Most influences on one vertex after.'), morphTargetsDropped: TRANSFER_OUT.morphTargetsDropped, saved: TRANSFER_OUT.saved },
    exampleInput: { action: 'prune_weights', skeletalMeshPath: '/Game/Characters/SK_Hero', threshold: 0.02 } }),
  buildRecord({ parentTool: T, id: `${T}.create_physics_asset`, action: 'create_physics_asset', family: F,
    summary: 'Create a PhysicsAsset for a skeletal mesh.', whenToUse: ['Ragdoll/physics needed.'], whenNotToUse: ['PhysicsAsset exists.'],
    inputProps: { skeletonPath: P.skeletonPath, skeletalMeshPath: P.skeletalMeshPath, outputPath: A.outputPath, name: { type: 'string', description: 'Asset name (alternative to outputPath).' }, path: { type: 'string', description: 'Destination folder (with name).' }, geomType: { type: 'string', description: 'Body primitive: Sphyl, Box, Sphere, TaperedCapsule, MultiConvexHull or SingleConvexHull.' }, minBoneSize: { type: 'number', description: 'Bones smaller than this get no body.' }, createConstraints: { type: 'boolean', description: 'Create joint constraints between bodies.' }, bodyForAll: { type: 'boolean', description: 'Create a body for every bone regardless of size.' }, assignToMesh: { type: 'boolean', description: 'Assign the new asset to the skeletal mesh.' }, save: A.save }, required: [],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: ESU,
    outputProps: { assetPath: P.assetPath, physicsAssetPath: P.physicsAssetPath }, outputRequired: [],
    exampleInput: { action: 'create_physics_asset', skeletalMeshPath: '/Game/SM_Char', outputPath: '/Game/PA_Char', save: true }, exampleOutput: { success: true, message: 'PhysicsAsset created', assetPath: '/Game/PA_Char' } }),
];
