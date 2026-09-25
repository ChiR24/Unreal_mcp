/**
 * Spawn and lifecycle records: spawn/spawn_actor/spawn_blueprint, duplicate,
 * delete/destroy_actor/delete_by_tag.
 *
 * Grounded in actor-basic-handlers.ts (spawn, delete, duplicate,
 * spawn_blueprint, delete_by_tag) and the native ControlActor dispatch
 * (spawn/spawn_actor -> HandleControlActorSpawn, spawn_blueprint ->
 * HandleControlActorSpawnBlueprint, delete/destroy_actor ->
 * HandleControlActorDelete, duplicate -> HandleControlActorDuplicate,
 * delete_by_tag -> HandleControlActorDeleteByTag).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { num } from '../shared/schema-props.js';
import { actorAlias, CANONICAL_NR, DOMAIN, P } from './properties.js';

const FAMILY_SPAWN = 'spawn';
const FAMILY_LIFECYCLE = 'lifecycle';

export const SPAWN_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'spawn',
    domain: DOMAIN,
    family: FAMILY_SPAWN,
    topics: ['spawn actor', 'place actor', 'add actor to level', 'create actor', 'spawn cube', 'spawn static mesh', 'instantiate class'],
    summary:
      'Spawn a new actor instance from a class path into the current level.',
    whenToUse: [
      'A new actor of a known Unreal class must be created in the scene.',
    ],
    whenNotToUse: ['A Blueprint instance is needed (use spawn_blueprint).'],
    inputProps: {
      classPath: P.classPath,
      actorClass: P.actorClass,
      actorName: P.actorName,
      meshPath: P.meshPath,
      location: P.location,
      rotation: P.rotation,
      scale: P.scale,
    },
    required: [],
    requiredOneOf: ['classPath', 'actorClass'],
    outputProps: { name: P.actorName },
    outputRequired: [],
    effect: 'write',
    costLatency: 'interactive',
    costResources: 'low',
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: CANONICAL_NR,
    exampleInput: {
      action: 'spawn',
      classPath: '/Script/Engine.PointLight',
      actorName: 'MyLight',
      location: [0, 0, 100],
    },
    exampleOutput: {
      success: true,
      message: 'Spawned actor: MyLight',
      name: 'MyLight',
    },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'spawn_actor',
    domain: DOMAIN,
    family: FAMILY_SPAWN,
    summary:
      'Long-form alias for spawn; normalizeActorAction maps spawn_actor to spawn.',
    whenToUse: ['Preferred when callers use the explicit spawn_actor verb.'],
    whenNotToUse: ['Use the shorter spawn form to avoid alias normalization.'],
    inputProps: {
      classPath: P.classPath,
      actorClass: P.actorClass,
      actorName: P.actorName,
      meshPath: P.meshPath,
      location: P.location,
      rotation: P.rotation,
      scale: P.scale,
    },
    required: [],
    requiredOneOf: ['classPath', 'actorClass'],
    outputProps: { name: P.actorName },
    outputRequired: [],
    effect: 'write',
    costLatency: 'interactive',
    costResources: 'low',
    ...actorAlias('spawn'),
    exampleInput: {
      action: 'spawn_actor',
      classPath: '/Script/Engine.Cube',
      actorName: 'Cube1',
    },
    exampleOutput: {
      success: true,
      message: 'Spawned actor: Cube1',
      name: 'Cube1',
    },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'spawn_blueprint',
    domain: DOMAIN,
    family: FAMILY_SPAWN,
    topics: ['spawn blueprint actor', 'place blueprint in level', 'instantiate blueprint'],
    summary:
      'Spawn an actor instance from a Blueprint asset path into the current level.',
    whenToUse: ['A Blueprint instance must be placed in the scene.'],
    whenNotToUse: ['A native class instance is needed (use spawn).'],
    // `scale` is read and applied by the native handler
    // (McpAutomationBridge_ControlActorBlueprintSpawn.cpp: bHasScale ->
    // SetActorScale3D) and echoed in its response, but was undeclared here.
    // With additionalProperties:false the gateway rejected it as an
    // UNDECLARED_PARAMETER, so spawning a scaled Blueprint actor needed a
    // second set_transform round-trip while `spawn` accepted scale directly.
    inputProps: {
      blueprintPath: P.blueprintPath,
      actorName: P.actorName,
      location: P.location,
      rotation: P.rotation,
      scale: P.scale,
    },
    required: ['blueprintPath'],
    // `required` already demands blueprintPath; declaring it as the variant's
    // identity obligation lets the folded `spawn` family union it with
    // classPath/actorClass, so a bare spawn is refused by the schema instead
    // of by the handler.
    requiredOneOf: ['blueprintPath'],
    outputProps: { name: P.actorName },
    outputRequired: [],
    effect: 'write',
    costLatency: 'interactive',
    costResources: 'low',
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: CANONICAL_NR,
    exampleInput: {
      action: 'spawn_blueprint',
      blueprintPath: '/Game/Blueprints/BP_Lamp',
      actorName: 'Lamp1',
    },
    exampleOutput: {
      success: true,
      message: 'Spawned blueprint: Lamp1',
      name: 'Lamp1',
    },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'spawn_batch',
    domain: DOMAIN,
    family: FAMILY_SPAWN,
    topics: ['spawn many actors', 'batch spawn', 'place many actors', 'lay out level', 'build level layout'],
    summary:
      'Spawn many actors in one call; each item is a spawn payload, optionally with a material, Blueprint variables, outliner folder and tags.',
    whenToUse: ['More than a couple of actors must be placed, such as laying out a level.'],
    whenNotToUse: ['A single actor is needed (use spawn).'],
    // Each item runs through the single-spawn handler in-process (native
    // HandleControlActorSpawnBatch), so an item takes exactly spawn's fields.
    inputProps: {
      actors: {
        type: 'array',
        items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
        'x-unreal-reflection-boundary': true,
        description: 'Actors to spawn, 1-500. Each is a spawn payload: classPath, blueprintPath or meshPath, plus '
          + 'actorName, location, rotation, scale ([x, y, z] arrays). Optional per item: materialPath (applied like '
          + 'set_material; componentName/materialSlot/allComponents narrow it), variables ({name: value} Blueprint '
          + 'variables set on the new instance, like set_blueprint_variables), folder (outliner folder path), tags '
          + '(actor tags; delete_by_tag removes the batch again). Items that fail are reported; the rest still spawn.',
      },
      defaults: {
        type: 'object',
        additionalProperties: true,
        'x-unreal-reflection-boundary': true,
        description: 'Fields shared by every item (e.g. meshPath, materialPath, folder, tags); an item\'s own fields win.',
      },
    },
    required: ['actors'],
    requiredOneOf: ['actors'],
    outputProps: {
      spawned: num('Actors spawned.'),
      failed: num('Items that failed to spawn or to take their material.'),
      results: {
        type: 'array',
        items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
        'x-unreal-reflection-boundary': true,
        description: 'Per item: index, success, name, path, error, errorCode, variablesSet, materialApplied, materialError.',
      },
    },
    outputRequired: [],
    effect: 'write',
    costLatency: 'interactive',
    costResources: 'medium',
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: CANONICAL_NR,
    normalizationProvenance: 'post-migration',
    exampleInput: {
      action: 'spawn_batch',
      defaults: { meshPath: '/Engine/BasicShapes/Cube', folder: 'Level/Blocks', tags: ['LevelBlocks'] },
      actors: [
        { actorName: 'Block_1', location: [0, 0, 50] },
        { actorName: 'Block_2', location: [100, 0, 50], materialPath: '/Game/Materials/M_Brick' },
      ],
    },
    exampleOutput: { success: true, message: 'Spawned 2 actors', spawned: 2, failed: 0 },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'duplicate',
    domain: DOMAIN,
    family: FAMILY_LIFECYCLE,
    topics: ['clone actor', 'copy actor'],
    summary:
      'Duplicate an existing actor, optionally with a new name and offset.',
    whenToUse: ['An actor must be copied within the current level.'],
    whenNotToUse: ['A distinct class instance is needed (use spawn).'],
    inputProps: {
      actorName: P.actorName,
      newName: P.newName,
      offset: P.offset,
    },
    required: ['actorName'],
    effect: 'write',
    costLatency: 'interactive',
    costResources: 'low',
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: CANONICAL_NR,
    exampleInput: {
      action: 'duplicate',
      actorName: 'Cube1',
      newName: 'Cube2',
      offset: [100, 0, 0],
    },
    exampleOutput: { success: true, message: 'Duplicated Cube1 to Cube2' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'delete',
    domain: DOMAIN,
    family: FAMILY_LIFECYCLE,
    topics: ['delete actor', 'destroy actor', 'remove actor from level', 'delete selected actor', 'delete spawned actor', 'delete object'],
    aliases: ['control_actor.delete_actor'],
    summary: 'Permanently delete one actor or a batch of actors by name.',
    whenToUse: ['An actor must be permanently removed from the level.'],
    whenNotToUse: ['The actor should only be hidden (use set_visibility).'],
    inputProps: { actorName: P.actorName, actorNames: P.actorNames },
    required: [],
    requiredOneOf: ['actorName', 'actorNames'],
    effect: 'destructive',
    behavior: { safeToRetry: false, supportsUndo: false },
    costLatency: 'interactive',
    costResources: 'low',
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: CANONICAL_NR,
    exampleInput: { action: 'delete', actorName: 'Cube1' },
    exampleOutput: { success: true, message: 'Deleted Cube1' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'destroy_actor',
    domain: DOMAIN,
    family: FAMILY_LIFECYCLE,
    summary:
      'Long-form alias for delete; normalizeActorAction maps destroy_actor to delete.',
    whenToUse: ['Preferred when callers use the explicit destroy_actor verb.'],
    whenNotToUse: ['Use the shorter delete form to avoid alias normalization.'],
    inputProps: { actorName: P.actorName, actorNames: P.actorNames },
    required: [],
    requiredOneOf: ['actorName', 'actorNames'],
    effect: 'destructive',
    behavior: { safeToRetry: false, supportsUndo: false },
    costLatency: 'interactive',
    costResources: 'low',
    ...actorAlias('delete'),
    exampleInput: { action: 'destroy_actor', actorName: 'Cube1' },
    exampleOutput: { success: true, message: 'Deleted Cube1' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'delete_by_tag',
    domain: DOMAIN,
    family: FAMILY_LIFECYCLE,
    summary: 'Permanently delete every actor matching a gameplay tag, or any of several tags in one call.',
    whenToUse: ['All actors sharing a tag must be removed in one operation.', 'Actors under several tags must go at once (tags, one consent).'],
    whenNotToUse: ['A single named actor should be removed (use delete).'],
    inputProps: {
      tag: P.tag,
      tags: { type: 'array', items: { type: 'string' }, description: 'Several actor tags at once, in place of tag: every actor carrying any of them is deleted under one consent.' },
    },
    required: [],
    requiredOneOf: ['tag', 'tags'],
    effect: 'destructive',
    behavior: { safeToRetry: false, supportsUndo: false },
    costLatency: 'interactive',
    costResources: 'low',
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: CANONICAL_NR,
    exampleInput: { action: 'delete_by_tag', tag: 'Disposable' },
    exampleOutput: {
      success: true,
      message: 'Deleted actors by tag: Disposable',
    },
  }),
];
