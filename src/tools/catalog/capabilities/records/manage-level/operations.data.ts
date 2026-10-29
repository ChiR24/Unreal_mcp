/**
 * Level operations family records (15 actions): streaming, lighting,
 * metadata, query, io, sublevel, and settings. Grounded per shard: see the
 * `Grounded in:` header of streaming/lighting/metadata/query/io/sublevel.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';
import { D, OPERATIONS_POWER_RECORDS } from './operations-power.data.js';
import { OPERATIONS_IO_RECORDS } from './operations-io.data.js';

// Both readers answer whether the level (and anything else) is unsaved (AddUnsavedState).
const UNSAVED_OUT = {
  unsaved: { type: 'boolean', description: 'Whether this level has changes that are not saved.' },
  unsavedPackages: { type: 'array', items: { type: 'string' }, description: 'Every level and asset package with unsaved changes, levels first (the first 100).' },
  unsavedPackageCount: { type: 'number', description: 'How many packages have unsaved changes.' },
};

// add_sublevel's reply, which load with streaming:true answers with too.
export const ADD_SUBLEVEL_OUT = {
  subLevelPath: { type: 'string', description: 'The sublevel package that was added, or was already there.' },
  world: { type: 'string', description: 'Name of the open level it belongs to.' },
  streamingMethod: { type: 'string', enum: ['Blueprint', 'AlwaysLoaded'], description: 'How it streams: the method asked for, or for alreadyExists the one it already had.' },
  alreadyExists: { type: 'boolean', description: 'True when it was already a sublevel of the open level; nothing was added.' },
};

export const OPERATIONS_RECORDS: readonly CapabilityRecordSource[] = [
  ...OPERATIONS_POWER_RECORDS,
  buildCoreRecord({
    parentTool: 'manage_level', action: 'get_current_level', dispatchAction: 'get_current_level',
    domain: D, family: 'query',
    topics: ['current level', 'current map', 'which level is open', 'active level', 'level name', 'is the level saved', 'list unsaved changes'],
    summary: 'Return the path of the level currently loaded in the editor.',
    whenToUse: ['The active level path must be inspected.'],
    whenNotToUse: ['All levels must be enumerated; use list_levels.'],
    inputProps: {},
    required: [],
    effect: 'read',
    exampleInput: { action: 'get_current_level' },
    exampleOutput: { success: true, message: 'Current level', levelPath: '/Game/Maps/Demo', unsaved: true, unsavedPackages: ['/Game/Maps/Demo'], unsavedPackageCount: 1 },
    outputProps: { levelPath: P.levelPath, ...UNSAVED_OUT },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'get_summary', dispatchAction: 'get_summary',
    domain: D, family: 'query',
    summary: 'Return a summary of a level asset (native maps to get_level_info).',
    whenToUse: ['Metadata and stats for a specific level must be inspected.'],
    whenNotToUse: ['The current level path is needed; use get_current_level.'],
    inputProps: { levelPath: P.levelPath },
    required: [], // levelPath defaults to the loaded world (dogfood #14)
    effect: 'read',
    exampleInput: { action: 'get_summary', levelPath: '/Game/Maps/Demo' },
    exampleOutput: {
      success: true, message: 'Level summary', levelPath: '/Game/Maps/Demo',
      levelName: 'Demo', actorCount: 42, loaded: true,
    },
    outputProps: {
      levelPath: P.levelPath,
      levelName: { type: 'string', description: 'Level asset name.' },
      actorCount: { type: 'number', description: 'Actor count when the level is loaded.' },
      loaded: { type: 'boolean', description: 'Whether the level is loaded in the editor.' },
      packageName: { type: 'string', description: 'Package name (asset-registry lookup).' },
      assetName: { type: 'string', description: 'Asset name (asset-registry lookup).' },
      objectPath: { type: 'string', description: 'Object path (asset-registry lookup).' },
      assetClass: { type: 'string', description: 'Asset class path (asset-registry lookup).' },
      tagsAndValues: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Asset-registry tag/value pairs (asset-registry lookup).' },
      ...UNSAVED_OUT,
    },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'validate_level',
    domain: D, family: 'query',
    topics: ['does it exist', 'map file', 'check map path'],
    summary: 'Check that a level asset exists: returns exists, its normalized path, the .umap file path, and whether the package and the file are present.',
    whenToUse: ['A level asset path must be verified before load or delete.'],
    whenNotToUse: ['The level should be loaded; use load.'],
    inputProps: { levelPath: P.levelPath },
    required: ['levelPath'],
    effect: 'read',
    exampleInput: { action: 'validate_level', levelPath: '/Game/Maps/Demo' },
    exampleOutput: { success: true, exists: true, levelPath: '/Game/Maps/Demo', message: 'Level asset exists' },
    outputProps: {
      exists: { type: 'boolean', description: 'Whether the level asset exists.' },
      levelPath: P.levelPath,
    },
    outputRequired: ['exists'],
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'run_map_check',
    domain: D, family: 'query',
    // No topic holds "errors" or "problems": with "map check" they named every word of "check for errors" (read_log).
    // One "map" topic only: a second raised "what maps exist" to 98% of list_levels.
    topics: ['map check', 'level warnings', 'unbuilt lighting'],
    summary: 'Run the editor\'s own Map Check (Build > Map Check) on the level open in the editor and answer its messages '
      + 'as rows: severity (error, warning, performance or info), message, the actor it names (actor, actorPath) and '
      + 'code, the map error\'s name (the key of the engine\'s guidance for it, such as LightingBuildNeeded or '
      + 'StaticMeshNull). Counts by severity come back as errorCount, warningCount, performanceWarningCount and noteCount, with total; at '
      + 'most 200 rows (truncated says when more were found). No Message Log tab opens and nothing in the level changes. '
      + 'NO_WORLD: no level is open.',
    whenToUse: ['The engine\'s own checks must be read for the open level: unbuilt lighting, meshes with no mesh, '
      + 'overlapping stationary lights, volumes with a bad brush and the rest.', 'A level is about to be saved, '
      + 'shipped or reviewed.'],
    whenNotToUse: ['Placement problems such as sunk, floating, intersecting or z-fighting actors (use control_actor '
      + 'audit_placement).'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'interactive',
    exampleInput: { action: 'run_map_check' },
    exampleOutput: {
      success: true, message: 'Map Check of \'L_Demo\': 1 error(s), 2 warning(s), 0 performance warning(s), 0 note(s).',
      levelPath: '/Game/Maps/L_Demo', errorCount: 1, warningCount: 2, performanceWarningCount: 0, noteCount: 0, total: 3,
    },
    outputProps: {
      levelPath: { type: 'string', description: 'The level checked.' },
      messages: {
        type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
        'x-unreal-reflection-boundary': true,
        description: 'One row per Map Check message: severity, message, actor and actorPath when it names one, code (the map error name).',
      },
      errorCount: { type: 'number', description: 'Error messages.' },
      warningCount: { type: 'number', description: 'Warning messages (the reply\'s warnings list only what else the call logged).' },
      performanceWarningCount: { type: 'number', description: 'Performance warnings.' },
      noteCount: { type: 'number', description: 'Info messages, the check\'s closing summary left out.' },
      total: { type: 'number', description: 'Every message found, listed or not.' },
      truncated: { type: 'boolean', description: 'True when more than 200 messages were found.' },
      hiddenByLogFilters: { type: 'number', description: 'Messages the Map Check tab\'s severity filters hide from these rows: tick every severity in that tab to list them. Present only when some are hidden.' },
    },
  }),
  ...OPERATIONS_IO_RECORDS,
  buildCoreRecord({
    parentTool: 'manage_level', action: 'add_sublevel', topics: ['add streaming level'], dispatchAction: 'add_sublevel',
    domain: D, family: 'sublevel',
    summary: 'Add an existing level map to the level open in the editor as a streaming sublevel, loaded and shown in the editor at once. streamingMethod Blueprint (the default) is not loaded when the game starts until something streams it in (stream during Play, or a Load Stream Level node); AlwaysLoaded loads with the open level. A sublevel already there answers alreadyExists with the method it streams with. The add is not undoable and leaves the open level unsaved: save it to keep the sublevel, or load the level again with discardUnsaved to drop it (and every other unsaved change). Refusals: PARENT_LEVEL_NOT_LOADED (parentLevel is not the open level), PACKAGE_NOT_FOUND or FILE_NOT_FOUND (no such map; nothing is left behind).',
    whenToUse: ['An existing level map must become a sublevel of the open level.'],
    whenNotToUse: ['The sublevel is already added and should be streamed in or out, or shown or hidden; use stream.', 'A new empty level is needed first; use create_level.'],
    // `sublevelPath` used to sit here beside `subLevelPath` as a case-variant
    // alias. UE hashes and compares FString case-insensitively, so the native
    // surface could only ever advertise one of the pair: the alias worked over
    // stdio and answered UNDECLARED_PARAMETER over native `/mcp`, which is the
    // worst of both worlds. There is no parameter-alias stage on the execute
    // path to move it to -- declared properties are the only accepted names on
    // either transport -- so the pair is resolved by dropping the spelling that
    // only differed by case. `levelPath` remains for callers who want a second
    // spelling; it cannot collide.
    inputProps: {
      subLevelPath: { ...P.subLevelPath, description: 'The level map to add, by /Game package path (an object path or .umap name is cut to the package); it must exist.' },
      levelPath: { ...P.levelPath, description: 'Alias of subLevelPath, read when subLevelPath is absent.' },
      parentLevel: P.parentLevel,
      streamingMethod: { ...P.streamingMethod, enum: ['Blueprint', 'AlwaysLoaded'], description: 'Blueprint (default): the game streams it in and out, and it starts unloaded. AlwaysLoaded: loaded whenever the open level is.' },
    },
    required: [],
    requiredOneOf: ['subLevelPath', 'levelPath'],
    outputProps: ADD_SUBLEVEL_OUT,
    effect: 'write', costLatency: 'interactive',
    exampleInput: { action: 'add_sublevel', subLevelPath: '/Game/Maps/Sub01', parentLevel: '/Game/Maps/Demo' },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'remove_sublevel', topics: ['remove streaming level', 'take a sublevel out of the level'], dispatchAction: 'remove_sublevel',
    domain: D, family: 'sublevel',
    summary: 'Remove a sublevel from the level open in the editor, as the Levels panel\'s Remove does: its streaming entry goes and its actors leave the editor world, while its map file stays on disk (manage_level delete removes that). Not undoable, as in the editor. A sublevel with unsaved changes is refused (DIRTY_PACKAGES, listing them) unless discardUnsaved is true, a locked one is refused (LEVEL_LOCKED), a Level Instance\'s level is not a sublevel (NOT_A_SUBLEVEL: delete its actor instead), and a name that is not a sublevel of the open level is SUBLEVEL_NOT_FOUND, listing the ones it has. The open level is left unsaved: save it (manage_level save) for the removal to last.',
    whenToUse: ['A sublevel added to the open level (add_sublevel, create_sublevel) must leave it.'],
    whenNotToUse: ['The sublevel should only be hidden or streamed out; use stream.', 'The level map itself must go from the project: remove it here first, then manage_level delete.'],
    inputProps: {
      subLevelPath: { ...P.subLevelPath, description: 'The sublevel to remove: its /Game package path (an object path or .umap name is cut to the package), or a name only one sublevel\'s path ends with.' },
      levelPath: { ...P.levelPath, description: 'Alias of subLevelPath, read when subLevelPath is absent.' },
      discardUnsaved: { type: 'boolean', description: 'Remove it even though its level (or its actors\' own files) has unsaved changes, which are lost (default false: refused with DIRTY_PACKAGES).' },
    },
    required: [],
    requiredOneOf: ['subLevelPath', 'levelPath'],
    outputProps: {
      subLevelPath: { type: 'string', description: 'The sublevel package that was removed.' },
      world: { type: 'string', description: 'Name of the open level it was removed from.' },
      wasLoaded: { type: 'boolean', description: 'False when the sublevel was not loaded in the editor (only its entry was removed).' },
      discardedPackages: { type: 'array', items: { type: 'string' }, description: 'With discardUnsaved, the packages whose unsaved changes were dropped.' },
    },
    effect: 'destructive', costLatency: 'interactive',
    exampleInput: { action: 'remove_sublevel', subLevelPath: '/Game/Maps/Sub01' },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'move_actors_to_level', topics: ['move actors to another level', 'move actors into a sublevel', 'put actors in a sublevel'], dispatchAction: 'move_actors_to_level',
    domain: D, family: 'sublevel',
    summary: 'Move placed actors into another loaded level of the open world (one of its sublevels, or the open level itself with levelPath persistent), as the Levels panel\'s Move Selection to Level does: they are cut and pasted into it, the clipboard kept as it was, and soft references to them follow. One undo step; both levels then need saving. All or none: every name and the level are checked first (ACTOR_NOT_FOUND, SUBLEVEL_NOT_FOUND naming the loaded levels, LEVEL_NOT_LOADED, LEVEL_LOCKED), and an actor already in that level is left as it is and listed under alreadyThere.',
    whenToUse: ['Actors placed in the open level belong in one of its sublevels (lighting, gameplay, a streaming area), or back in the open level.'],
    whenNotToUse: ['Copies in the other level are wanted: duplicate the actors there instead.', 'A sublevel itself must be added or removed: use add_sublevel or remove_sublevel.'],
    inputProps: {
      actorNames: { type: 'array', minItems: 1, maxItems: 500, items: { type: 'string' }, description: 'The actors to move, by label or name.' },
      levelPath: { ...P.levelPath, description: 'The level to move them into: a loaded sublevel\'s /Game package path or a name only one sublevel\'s path ends with, or persistent (the open level itself, as is its own path).' },
    },
    required: ['actorNames', 'levelPath'],
    outputProps: {
      levelPath: { type: 'string', description: 'The level the actors are in now.' },
      moved: { type: 'array', items: { type: 'string' }, description: 'The actors moved, by their labels in the new level.' },
      alreadyThere: { type: 'array', items: { type: 'string' }, description: 'Actors named that were in that level already, left as they were.' },
      levels: { type: 'array', items: { type: 'string' }, description: 'With SUBLEVEL_NOT_FOUND or LEVEL_NOT_LOADED, the levels loaded in the open world.' },
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: the move, one step. Absent when nothing moved.' },
    },
    effect: 'destructive', costLatency: 'interactive',
    exampleInput: { action: 'move_actors_to_level', actorNames: ['Lamp_1', 'Lamp_2'], levelPath: '/Game/Maps/Lighting' },
  }),
];
