/**
 * Spawn and lifecycle records: spawn/spawn_actor/spawn_blueprint, duplicate,
 * delete/destroy_actor/delete_by_tag.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';
import { bool, num, str, vec3 } from '../shared/schema-props.js';
import { DOMAIN, P, PLACEMENT_FACTS } from './properties.js';

const FAMILY_SPAWN = 'spawn';
const FAMILY_LIFECYCLE = 'lifecycle';

// Every spawn is one undo step opened before it touches the level; a batch holds one for all its items.
const SPAWN_UNDO = {
  type: 'object',
  additionalProperties: true,
  'x-unreal-reflection-boundary': true,
  description: 'Whether editor undo takes the spawn back: {undoable: true, transactionScope: "Spawn Actors"} (control_editor undo removes every actor the call made), or {undoable: false, reasonCode, reason}.',
} as const;

// Outliner folder and actor tags, as a spawn_batch item takes them; a single spawn refused folder.
const SPAWN_ORGANISATION = {
  folder: str('Outliner folder path for the new actor (Level/Signs), made when missing.'),
  tags: {
    type: 'array',
    items: { type: 'string' },
    description: 'Actor tags for the new actor: delete_by_tag removes them again, and mcp.placement.ok marks a deliberate overlap before the placement is judged.',
  },
} as const;

export const SPAWN_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'spawn',
    editorStates: ANY_EDITOR_STATE,
    domain: DOMAIN,
    family: FAMILY_SPAWN,
    topics: ['spawn actor', 'place actor', 'add actor to level', 'create actor', 'spawn cube', 'spawn static mesh', 'instantiate class'],
    summary:
      'Spawn a new actor instance from a class path into the current level, or a mesh actor from a mesh path alone.',
    whenToUse: [
      'A new actor of a known Unreal class must be created in the scene.',
      'A static or skeletal mesh must be placed as its own actor (meshPath alone; the actor class follows the mesh).',
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
      ...SPAWN_ORGANISATION,
    },
    required: [],
    // The handler places a mesh actor for a bare meshPath; the schema refused that call before it got there.
    requiredOneOf: ['classPath', 'actorClass', 'meshPath'],
    outputProps: { name: P.actorName, undo: SPAWN_UNDO, ...PLACEMENT_FACTS },
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
    editorStates: ANY_EDITOR_STATE,
    domain: DOMAIN,
    family: FAMILY_SPAWN,
    topics: ['spawn blueprint actor', 'place blueprint in level', 'instantiate blueprint'],
    summary:
      'Spawn an actor instance from a Blueprint asset path into the current level.',
    whenToUse: ['A Blueprint instance must be placed in the scene.'],
    whenNotToUse: ['A native class instance is needed (use spawn).'],
    // `scale` is read and applied by the native handler
    // (McpActorBPSpawn.cpp: bHasScale ->
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
      ...SPAWN_ORGANISATION,
    },
    required: ['blueprintPath'],
    // `required` already demands blueprintPath; declaring it as the variant's
    // identity obligation lets the folded `spawn` family union it with
    // classPath/actorClass, so a bare spawn is refused by the schema instead
    // of by the handler.
    requiredOneOf: ['blueprintPath'],
    outputProps: { name: P.actorName, undo: SPAWN_UNDO, ...PLACEMENT_FACTS },
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
    editorStates: ANY_EDITOR_STATE,
    domain: DOMAIN,
    family: FAMILY_SPAWN,
    topics: ['spawn many actors', 'batch spawn', 'place many actors', 'lay out level', 'build level layout',
      'row of actors', 'grid of actors'],
    summary:
      'Spawn many actors in one call; each item is a spawn payload, optionally with a material, Blueprint variables, outliner folder and tags.',
    whenToUse: [
      'More than a couple of actors must be placed, such as laying out a level.',
      'A row or grid of the same actor must be placed (an item\'s repeat).',
    ],
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
          + 'variables set on the new instance, like set_blueprint_variables; in a running game they are set before its '
          + 'BeginPlay, so it starts with them), properties ({name: value} set on the root component, or on componentName\'s, '
          + 'as edit_component sets them: a light\'s Intensity, LightColor and AttenuationRadius), folder (outliner folder path), tags '
          + '(actor tags; delete_by_tag removes the batch again), repeat ({count, offset [x, y, z], rows, rowOffset '
          + '[x, y, z]}: count x rows copies of the item, each moved from its location by offset per copy and rowOffset '
          + 'per row, actorName numbered _1, _2, ...; a row of racks, a grid of lights; at most 1000 actors once '
          + 'unrolled). Items that fail are reported; the rest still spawn. Parts meant to hang in the air or sit inside another (ceiling ducts, lights, a flange on a pipe) take the tag mcp.placement.ok (defaults.tags covers the whole batch), so the reply keeps only what needs fixing.',
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
        description: 'Which items results lists: all, or failures only (items that failed to spawn or to take '
          + 'their material or variables, and items with a placementWarning); spawned and failed still count every item. '
          + 'Default all, or failures for a batch of more than 20 items once repeats are unrolled; the receipt still names every actor.',
      },
    },
    required: ['actors'],
    requiredOneOf: ['actors'],
    outputProps: {
      spawned: num('Actors spawned.'),
      failed: num('Items that failed to spawn or to take their material, variables or properties.'),
      placementWarnings: num('Items that spawned but look misplaced (sunk into the surface under them, floating, intersecting other actors, with nothing below, or with a face flickering against another actor\'s in the same plane); each carries its placementWarning in results. A warning is not a failure: it changes neither failed nor success.'),
      results: {
        type: 'array',
        items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
        'x-unreal-reflection-boundary': true,
        description: 'Per item (only the failed ones and those with a placementWarning or ignoredKeys under report: failures, the default past 20 items): '
          + 'index (its place in the batch once repeats are unrolled), success, name, path, error, errorCode, variablesSet, variablesError, materialApplied, materialError, propertiesError, '
          + 'ignoredKeys (the item\'s keys no spawn reads, such as a "label" meant as actorName), and the placement '
          + 'facts set_transform reports (groundZ, groundClearance, placementWarning, overlappingActors, coplanarFaces, suggestedLocation), '
          + 'measured after the item took its tags, variables and material, so an item tagged mcp.placement.ok is never '
          + 'reported. name is the label the item asked for, or the unique actor name when it gave no actorName.',
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
      'Duplicate a placed actor: a copy with its components, properties, materials and tags at the original\'s transform moved by offset, labelled newName or numbered after the original, in one undo step.',
    whenToUse: [
      'An actor must be copied within the current level.',
      'A configured prop, light or Blueprint instance should appear again elsewhere without setting each property again: duplicate it with the offset to where the copy goes.',
    ],
    whenNotToUse: [
      'A distinct class instance is needed (use spawn).',
      'Many copies in a row or a grid are wanted: spawn_batch places them in one call and one undo step.',
    ],
    inputProps: {
      actorName: P.actorName,
      newName: str('Label for the copy. Without it the editor numbers the copy after the highest label of its series (Desk_1 copied becomes Desk_20 when Desk_19 exists); the reply\'s actorName names the copy either way.'),
      offset: vec3('World offset [x, y, z] in cm from the original\'s location (default [0, 0, 0]). With no offset the copy sits exactly on the original: both draw every face in the same place, which flickers, and the reply\'s warnings call it a duplicate. Wherever it lands, the reply\'s warnings name a copy left sunk, floating or intersecting.'),
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
    editorStates: ANY_EDITOR_STATE,
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
    editorStates: ANY_EDITOR_STATE,
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
    editorStates: ANY_EDITOR_STATE,
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
