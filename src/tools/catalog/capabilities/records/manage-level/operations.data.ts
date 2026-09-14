/**
 * Level operations family records (13 actions): streaming, lighting,
 * metadata, query, io, sublevel, and settings. Grounded per shard: see the
 * `Grounded in:` header of streaming/lighting/metadata/query/io/sublevel.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';
import { D, NR, OPERATIONS_POWER_RECORDS } from './operations-power.data.js';
import { OPERATIONS_IO_RECORDS } from './operations-io.data.js';

export const OPERATIONS_RECORDS: readonly CapabilityRecordSource[] = [
  ...OPERATIONS_POWER_RECORDS,
  buildCoreRecord({
    parentTool: 'manage_level', action: 'get_current_level', dispatchAction: 'get_current_level',
    domain: D, family: 'query',
    topics: ['current level', 'current map', 'which level is open', 'active level', 'level name'],
    summary: 'Return the path of the level currently loaded in the editor.',
    whenToUse: ['The active level path must be inspected.'],
    whenNotToUse: ['All levels must be enumerated; use list_levels.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_current_level' },
    exampleOutput: { success: true, message: 'Current level', levelPath: '/Game/Maps/Demo' },
    outputProps: { levelPath: P.levelPath },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'get_summary', dispatchAction: 'get_summary',
    domain: D, family: 'query',
    summary: 'Return a summary of a level asset (native maps to get_level_info).',
    whenToUse: ['Metadata and stats for a specific level must be inspected.'],
    whenNotToUse: ['The current level path is needed; use get_current_level.'],
    inputProps: { levelPath: P.levelPath },
    required: [], // levelPath defaults to the loaded world (dogfood #14)
    effect: 'read', costLatency: 'instant', costResources: 'low',
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
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'validate_level', dispatchAction: 'execute_editor_function', dispatchMode: 'action',
    domain: D, family: 'query',
    summary: 'Validate that a level asset exists on disk via the execute_editor_function bridge route.',
    whenToUse: ['A level asset path must be verified before load or delete.'],
    whenNotToUse: ['The level should be loaded; use load.'],
    inputProps: { levelPath: P.levelPath, assetPath: P.assetPath },
    required: ['levelPath'],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'validate_level', levelPath: '/Game/Maps/Demo' },
    exampleOutput: { success: true, exists: true, levelPath: '/Game/Maps/Demo', message: 'Level asset exists' },
    outputProps: {
      exists: { type: 'boolean', description: 'Whether the level asset exists.' },
      levelPath: P.levelPath,
    },
    outputRequired: ['exists'],
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  ...OPERATIONS_IO_RECORDS,
  buildCoreRecord({
    parentTool: 'manage_level', action: 'add_sublevel', dispatchAction: 'add_sublevel',
    domain: D, family: 'sublevel',
    summary: 'Add a sub-level as a streaming child of a parent level.',
    whenToUse: ['A streaming child level must be associated with a parent.'],
    whenNotToUse: ['The sub-level should be streamed independently; use stream.'],
    inputProps: {
      subLevelPath: P.subLevelPath, sublevelPath: P.sublevelPath, levelPath: P.levelPath,
      parentLevel: P.parentLevel, parentPath: P.parentPath, streamingMethod: P.streamingMethod,
    },
    required: [],
    requiredOneOf: ['subLevelPath', 'sublevelPath', 'levelPath'],
    effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_sublevel', subLevelPath: '/Game/Maps/Sub01', parentLevel: '/Game/Maps/Demo' },
    exampleOutput: { success: true, message: 'Sub-level added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
