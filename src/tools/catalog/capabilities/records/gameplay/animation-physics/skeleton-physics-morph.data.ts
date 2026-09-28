/**
 * Skeleton physics assets and morph targets: PhysicsAsset creation, bodies,
 * constraints, cloth binding and morph authoring.
 */

import type { CapabilityRecordSource } from '../../../model.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import { A } from './animation-properties.js';
import { num, vec3 } from '../../shared/schema-props.js';

const T = 'animation_physics';
const F = 'skeleton';
const ESU = ['EditorScriptingUtilities'];
const MESH_REQUIRED = ['skeletalMeshPath'];
const MESH_SKELETON = { type: 'string', description: 'A skeletal mesh whose skeleton to read, instead of skeletonPath.' };

export const SKELETON_PHYSICS_MORPH_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({ parentTool: T, id: `${T}.add_physics_body`, action: 'add_physics_body', family: F,
    summary: 'Add a physics body to a PhysicsAsset.', whenToUse: ['A bone needs a collision body.'], whenNotToUse: ['Body exists.'],
    inputProps: { physicsAssetPath: P.physicsAssetPath, boneName: P.boneName, bodyType: A.bodyType, radius: num('Sphere or capsule radius (default 10).'), length: num('Capsule length between the end caps (default 20).'), width: num('Box extent along X (default 10).'), depth: num('Box extent along Y (default 10).'), height: num('Box extent along Z (default 10).'), center: A.center, rotation: vec3('Box or capsule rotation relative to the bone as [pitch, yaw, roll].'), save: A.save }, required: ['physicsAssetPath', 'boneName'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_physics_body', physicsAssetPath: '/Game/PA_Char', boneName: 'spine_01', bodyType: 'Sphere', radius: 12, center: [0, 0, 0] },
    exampleOutput: { success: true, message: 'Physics body added' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_physics_body`, action: 'configure_physics_body', family: F,
    summary: 'Configure a physics body mass/damping/collision.', whenToUse: ['Body parameters must change.'], whenNotToUse: ['Use add_physics_body.'],
    inputProps: { physicsAssetPath: P.physicsAssetPath, boneName: P.boneName, mass: P.mass, linearDamping: P.linearDamping, angularDamping: P.angularDamping, collisionEnabled: P.collisionEnabled, simulatePhysics: P.simulatePhysics, save: A.save },
    required: ['physicsAssetPath', 'boneName'], effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    plugins: ESU, exampleInput: { action: 'configure_physics_body', physicsAssetPath: '/Game/PA_Char', boneName: 'spine_01', mass: 5, linearDamping: 0.25, collisionEnabled: true },
    exampleOutput: { success: true, message: 'Physics body configured' } }),
  buildRecord({ parentTool: T, id: `${T}.add_physics_constraint`, action: 'add_physics_constraint', family: F,
    summary: 'Add a physics constraint between two bodies; refuses a pair that already has one.', whenToUse: ['Bodies must be jointed.'], whenNotToUse: ['Rigid bodies suffice.'],
    inputProps: { physicsAssetPath: P.physicsAssetPath, bodyA: P.bodyA, bodyB: P.bodyB, constraintName: A.constraintName, limits: A.limits, save: A.save }, required: ['physicsAssetPath', 'bodyA', 'bodyB'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'add_physics_constraint', physicsAssetPath: '/Game/PA_Char', bodyA: 'spine_01', bodyB: 'spine_02', constraintName: 'Spine' },
    exampleOutput: { success: true, message: 'Constraint added' } }),
  buildRecord({ parentTool: T, id: `${T}.configure_constraint_limits`, action: 'configure_constraint_limits', family: F,
    summary: 'Configure constraint angular/linear limits.', whenToUse: ['Joint limits must change.'], whenNotToUse: ['Use add_physics_constraint.'],
    inputProps: { physicsAssetPath: P.physicsAssetPath, bodyA: P.bodyA, bodyB: P.bodyB, limits: A.limits, save: A.save }, required: ['physicsAssetPath', 'bodyA', 'bodyB'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: ESU,
    exampleInput: { action: 'configure_constraint_limits', physicsAssetPath: '/Game/PA_Char', bodyA: 'spine_01', bodyB: 'spine_02', limits: { swing1LimitAngle: 30 } },
    exampleOutput: { success: true, message: 'Constraint limits configured' } }),
  buildRecord({ parentTool: T, id: `${T}.bind_cloth_to_skeletal_mesh`, action: 'bind_cloth_to_skeletal_mesh', family: F,
    summary: 'Bind a cloth asset to a skeletal mesh.', whenToUse: ['Cloth simulation is needed (Chaos Cloth).'], whenNotToUse: ['No cloth.'],
    inputProps: { skeletalMeshPath: P.skeletalMeshPath, clothAssetName: { type: 'string', description: 'Name of a clothing asset already registered on the mesh.' }, clothAssetPath: { type: 'string', description: 'Clothing asset to load and register before binding.' }, meshLodIndex: { type: 'number', description: 'Mesh LOD to bind (default 0).' }, sectionIndex: { type: 'number', description: 'Mesh section to bind (default 0).' }, assetLodIndex: { type: 'number', description: 'Clothing asset LOD to use (default 0).' }, save: A.save }, required: MESH_REQUIRED,
    effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high', plugins: ['ChaosCloth', 'EditorScriptingUtilities'],
    exampleInput: { action: 'bind_cloth_to_skeletal_mesh', skeletalMeshPath: '/Game/SM_Char', save: false },
    exampleOutput: { success: true, message: 'Cloth bound' } }),
  buildRecord({ parentTool: T, id: `${T}.assign_cloth_asset_to_mesh`, action: 'assign_cloth_asset_to_mesh', family: F,
    summary: 'Assign a cloth asset to a skeletal mesh section.', whenToUse: ['A cloth asset must be assigned.'], whenNotToUse: ['Use bind_cloth_to_skeletal_mesh.'],
    inputProps: { skeletalMeshPath: P.skeletalMeshPath, clothAssetName: { type: 'string', description: 'Name of a clothing asset already registered on the mesh.' }, clothAssetPath: { type: 'string', description: 'Clothing asset to load and register before binding.' }, meshLodIndex: { type: 'number', description: 'Mesh LOD to bind (default 0).' }, sectionIndex: { type: 'number', description: 'Mesh section to bind (default 0).' }, assetLodIndex: { type: 'number', description: 'Clothing asset LOD to use (default 0).' }, save: A.save }, required: MESH_REQUIRED,
    effect: 'write', latency: 'interactive', resources: 'low', plugins: ['ChaosCloth', 'EditorScriptingUtilities'],
    exampleInput: { action: 'assign_cloth_asset_to_mesh', skeletalMeshPath: '/Game/SM_Char', save: false },
    exampleOutput: { success: true, message: 'Cloth asset assigned' } }),
  buildRecord({ parentTool: T, id: `${T}.create_morph_target`, action: 'create_morph_target', family: F,
    summary: 'Create a morph target on a skeletal mesh.', whenToUse: ['Facial/shape blending needed.'], whenNotToUse: ['No morph targets.'],
    inputProps: { skeletalMeshPath: P.skeletalMeshPath, morphTargetName: P.morphTargetName, deltas: A.deltas, lodIndex: A.lodIndex, save: A.save }, required: ['skeletalMeshPath', 'morphTargetName'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: ESU,
    exampleInput: { action: 'create_morph_target', skeletalMeshPath: '/Game/SM_Char', morphTargetName: 'smile', deltas: [{ vertexIndex: 0, positionDelta: { x: 0, y: 0, z: 1 } }] },
    exampleOutput: { success: true, message: 'Morph target created' } }),
  buildRecord({ parentTool: T, id: `${T}.set_morph_target_deltas`, action: 'set_morph_target_deltas', family: F,
    summary: 'Set vertex deltas for a morph target.', whenToUse: ['Morph shape must be authored.'], whenNotToUse: ['Use create_morph_target.'],
    inputProps: { skeletalMeshPath: P.skeletalMeshPath, morphTargetName: P.morphTargetName, deltas: A.deltas, lodIndex: A.lodIndex, save: A.save },
    required: ['skeletalMeshPath', 'morphTargetName'], effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high',
    plugins: ESU, exampleInput: { action: 'set_morph_target_deltas', skeletalMeshPath: '/Game/SM_Char', morphTargetName: 'smile', deltas: [{ vertexIndex: 0, positionDelta: { x: 0, y: 0, z: 2 } }] },
    exampleOutput: { success: true, message: 'Morph deltas set' } }),
  buildRecord({ parentTool: T, id: `${T}.import_morph_targets`, action: 'import_morph_targets', family: F,
    topics: ['import morph targets', 'transfer blend shapes', 'copy morph targets'],
    summary: 'Copy morph targets (blend shapes) from another skeletal mesh into this mesh\'s source data, each vertex taking the delta of the nearest source vertex, so they survive every rebuild.',
    whenToUse: ['A mesh must get the blend shapes of a sculpted or imported variant.'], whenNotToUse: ['The morph is authored by hand on this mesh (edit create or set_deltas).'],
    inputProps: {
      skeletalMeshPath: P.skeletalMeshPath,
      sourceMeshPath: { type: 'string', description: 'Skeletal mesh that owns the morph targets (its LOD 0 source data).' },
      morphTargets: { type: 'array', items: { type: 'string' }, description: 'Names to import (default all the source mesh has in its source data).' },
      lodIndex: A.lodIndex, dropRenderOnlyMorphs: A.dropRenderOnlyMorphs, save: A.save,
    },
    required: ['skeletalMeshPath', 'sourceMeshPath'], effect: 'write', behavior: { longRunning: true }, latency: 'long-running', resources: 'high', plugins: ESU,
    outputProps: {
      imported: { type: 'array', items: { type: 'object', properties: { name: { type: 'string', description: 'Morph target.' }, verticesAffected: { type: 'number', description: 'Vertices it moves on this mesh.' } }, additionalProperties: false }, description: 'Each imported morph target.' },
      replaced: { type: 'array', items: { type: 'string' }, description: 'Morph targets the mesh already had and were replaced.' },
      builtMorphTargets: { type: 'array', items: { type: 'string' }, description: 'Imported names that exist as morph targets after the rebuild.' },
      unmatchedVertices: { type: 'number', description: 'Vertices farther than 2% of the source size from any source vertex.' },
      morphTargetsDropped: { type: 'array', items: { type: 'string' }, description: 'Render-only morph targets the rebuild removed, only with dropRenderOnlyMorphs true.' },
      saved: { type: 'boolean', description: 'Whether the mesh was saved.' },
    },
    exampleInput: { action: 'import_morph_targets', skeletalMeshPath: '/Game/Characters/SK_Head', sourceMeshPath: '/Game/Characters/SK_Head_Sculpt', morphTargets: ['Smile'] } }),
  buildRecord({ parentTool: T, id: `${T}.get_skeleton_info`, action: 'get_skeleton_info', family: F,
    summary: 'Read skeleton bone/socket info.', whenToUse: ['Inspect skeleton structure.'], whenNotToUse: ['Mutate the skeleton.'],
    inputProps: { skeletonPath: P.skeletonPath, skeletalMeshPath: MESH_SKELETON }, required: [], requiredOneOf: ['skeletonPath', 'skeletalMeshPath'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: ESU,
    outputProps: { boneCount: P.num_, virtualBoneCount: P.num_, socketCount: P.num_ }, outputRequired: [],
    exampleInput: { action: 'get_skeleton_info', skeletonPath: '/Game/SK_Char' },
    exampleOutput: { success: true, message: 'Skeleton info', boneCount: 50, socketCount: 2 } }),
  buildRecord({ parentTool: T, id: `${T}.list_bones`, action: 'list_bones', family: F,
    summary: 'List bones of a skeleton.', whenToUse: ['Enumerate bones.'], whenNotToUse: ['Mutate bones.'],
    inputProps: { skeletonPath: P.skeletonPath, skeletalMeshPath: MESH_SKELETON }, required: [], requiredOneOf: ['skeletonPath', 'skeletalMeshPath'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: ESU,
    outputProps: {
      bones: { type: 'array', description: 'One entry per bone in reference-skeleton order.', items: { type: 'object', additionalProperties: false, properties: { name: { type: 'string' }, index: { type: 'number' }, parentIndex: { type: 'number', description: '-1 for the root bone.' }, parentName: { type: 'string' }, location: { type: 'object', additionalProperties: false, properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } } }, required: ['name', 'index', 'parentIndex'] } },
      count: { type: 'number', description: 'Number of bones listed.' },
    }, outputRequired: [],
    exampleInput: { action: 'list_bones', skeletonPath: '/Game/SK_Char' },
    exampleOutput: { success: true, message: 'Bones listed', bones: [{ name: 'root', index: 0, parentIndex: -1, location: { x: 0, y: 0, z: 0 } }, { name: 'spine_01', index: 1, parentIndex: 0, parentName: 'root', location: { x: 0, y: 0, z: 90 } }], count: 2 } }),
];
