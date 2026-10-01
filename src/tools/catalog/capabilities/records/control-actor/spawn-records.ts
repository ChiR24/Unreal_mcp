/**
 * Spawn and lifecycle records: spawn/spawn_actor/spawn_blueprint, duplicate,
 * delete/destroy_actor/delete_by_tag.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { bool, num, str } from '../shared/schema-props.js';
import { DOMAIN, P } from './properties.js';

const FAMILY_SPAWN = 'spawn';
const FAMILY_LIFECYCLE = 'lifecycle';

// Every spawn is one undo step opened before it touches the level; a batch holds one for all its items.
const SPAWN_UNDO = {
  type: 'object',
  additionalProperties: true,
  'x-unreal-reflection-boundary': true,
  description: 'Whether editor undo takes the spawn back: {undoable: true, transactionScope: "Spawn Actors"} (control_editor undo removes every actor the call made), or {undoable: false, reasonCode, reason}.',
} as const;

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
    outputProps: { name: P.actorName, undo: SPAWN_UNDO },
    outputRequired: [],
    effect: 'write',
    costLatency: 'interactive',
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
    outputProps: { name: P.actorName, undo: SPAWN_UNDO },
    outputRequired: [],
    effect: 'write',
    costLatency: 'interactive',
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
      report: {
        type: 'string',
        enum: ['all', 'failures'],
        description: 'Which items results lists: all (default), or failures only (items that failed to spawn or to take '
          + 'their material or variables); spawned and failed still count every item. Use failures for big layouts.',
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
        description: 'Per item (only the failed ones under report: failures): index, success, name, path, error, errorCode, '
          + 'variablesSet, variablesError, materialApplied, materialError. name is the label the item asked for, or the '
          + 'unique actor name when it gave no actorName.',
      },
      unnamedActors: {
        type: 'array',
        items: { type: 'string' },
        description: 'The unique name of every item that gave no actorName, in batch order ("" where it failed), under '
          + 'either report mode. Their labels repeat (every cube is "Cube"), so these are the names later calls must use.',
      },
      affectedActors: {
        type: 'array',
        items: { type: 'string' },
        description: 'The actors that spawned, by the name each result carries, in batch order and under either report mode; '
          + 'the receipt lists them as changes, with an actor handle each (the first 20).',
      },
      report: { type: 'string', description: 'Echoes report when it narrowed results.' },
      undo: SPAWN_UNDO,
    },
    outputRequired: [],
    effect: 'write',
    costLatency: 'interactive',
    costResources: 'medium',
    exampleInput: {
      action: 'spawn_batch',
      defaults: { meshPath: '/Engine/BasicShapes/Cube', folder: 'Level/Blocks', tags: ['LevelBlocks'] },
      actors: [
        { actorName: 'Block_1', location: [0, 0, 50] },
        { actorName: 'Block_2', location: [100, 0, 50], materialPath: '/Game/Materials/M_Brick' },
      ],
    },
    exampleOutput: { success: true, message: 'Spawned 2 actors', spawned: 2, failed: 0, affectedActors: ['Block_1', 'Block_2'] },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'duplicate',
    domain: DOMAIN,
    family: FAMILY_LIFECYCLE,
    topics: ['clone actor', 'copy actor', 'make actor copy'],
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
    exampleInput: {
      action: 'duplicate',
      actorName: 'Cube1',
      newName: 'Cube2',
      offset: [100, 0, 0],
    },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'rename',
    domain: DOMAIN,
    family: FAMILY_LIFECYCLE,
    topics: ['rename actor', 'actor label', 'relabel actor', 'outliner name', 'actor object name', 'give actor a new name', 'change actor name'],
    summary: 'Rename an actor: its label (the name the Outliner and every other call use), and with renameObject its object name too, which is the name a cooked level ships.',
    whenToUse: [
      'An actor should go by a different name.',
      'A Blueprint class was renamed and its placed actors still carry the old class in their object names (BP_OldName_C_3): rename each with renameObject, then save the level.',
    ],
    whenNotToUse: ['A copy under a new name is wanted (use duplicate with newName).'],
    inputProps: {
      actorName: P.actorName,
      newName: str('The new label; letters, digits, spaces and underscores are safe.'),
      renameObject: bool('Also rename the object itself to newName, made valid and unique (default false: the label only). The label is editor-only; the object name ships in the cooked level. References from inside the same level follow the rename, a Level Sequence binding or a soft reference by the old path does not. An actor saved in its own package (World Partition) keeps its object name, with a note.'),
    },
    required: ['actorName', 'newName'],
    outputProps: {
      actorName: str('The actor as later calls should name it.'),
      label: str('The label now.'),
      oldLabel: str('The label before.'),
      objectName: str('The object name now.'),
      oldObjectName: str('The object name before.'),
      actorPath: str('Full object path of the actor.'),
      note: str('Why the object name was kept, when renameObject could not rename it.'),
    },
    outputRequired: [],
    effect: 'write',
    costLatency: 'instant',
    exampleInput: { action: 'rename', actorName: 'BP_OldEnemy_C_6', newName: 'Enemy_01', renameObject: true },
    exampleOutput: { success: true, message: 'Actor renamed', actorName: 'Enemy_01', label: 'Enemy_01', oldLabel: 'Enemy_01', objectName: 'Enemy_01', oldObjectName: 'BP_OldEnemy_C_6' },
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
    behavior: { safeToRetry: false },
    costLatency: 'interactive',
    exampleInput: { action: 'delete', actorName: 'Cube1' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'destroy_actor',
    domain: DOMAIN,
    family: FAMILY_LIFECYCLE,
    summary:
      'Long-form alias for delete. The bridge dispatches both names to the same handler.',
    whenToUse: ['Preferred when callers use the explicit destroy_actor verb.'],
    whenNotToUse: ['Use the shorter delete form to avoid alias normalization.'],
    inputProps: { actorName: P.actorName, actorNames: P.actorNames },
    required: [],
    requiredOneOf: ['actorName', 'actorNames'],
    effect: 'destructive',
    behavior: { safeToRetry: false },
    costLatency: 'interactive',
    exampleInput: { action: 'destroy_actor', actorName: 'Cube1' },
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
    behavior: { safeToRetry: false },
    costLatency: 'interactive',
    exampleInput: { action: 'delete_by_tag', tag: 'Disposable' },
    exampleOutput: {
      success: true,
      message: 'Deleted actors by tag: Disposable',
    },
  }),
];
