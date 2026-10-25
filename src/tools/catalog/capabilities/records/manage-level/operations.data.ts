/**
 * Level operations family records (13 actions): streaming, lighting,
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
];
