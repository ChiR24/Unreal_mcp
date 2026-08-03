/**
 * Level operations family records (13 actions): streaming, lighting,
 * metadata, query, io, and sublevel.
 *
 * Grounded in:
 * - src/tools/handlers/level/runtime/level-handlers.ts: stream/unload route
 *   to the stream_level bridge action (dispatchMode 'action'); unload_level
 *   is NOT in the switch and falls to the default manage_level dispatch
 *   (dispatchMode 'tool'); list_levels routes to the list_levels bridge
 *   action; get_summary/get_current_level route through manage_level;
 *   set_metadata routes to the shared set_metadata bridge action; add_sublevel
 *   routes through manage_level with action 'add_sublevel'.
 * - src/tools/handlers/level/runtime/level-light-handlers.ts: create_light
 *   and build_lighting route to the manage_lighting bridge action
 *   (dispatchMode 'action'); build_lighting uses LONG_RUNNING_OP_TIMEOUT_MS.
 * - src/tools/handlers/level/runtime/level-asset-handlers.ts: validate_level
 *   calls sendAutomationRequest('execute_editor_function', ...) directly with
 *   functionName ASSET_EXISTS_SIMPLE (dispatchMode 'action');
 *   export_level/import_level route through manage_level.
 * - native McpAutomationBridge_LevelHandlers.cpp: stream_level and
 *   build_lighting are top-level Level actions; set_metadata is not in the
 *   bIsLevelAction set, confirming it dispatches to a separate route.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const D = 'level';
const NR = 'Distinct manage_level operation verb and target; no cross-tool duplicate.';

export const OPERATIONS_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_level', action: 'stream', dispatchAction: 'stream_level', dispatchMode: 'action',
    domain: D, family: 'streaming',
    summary: 'Stream a level in or out of the world, controlling load and visibility state.',
    whenToUse: ['A streaming level must be loaded, unloaded, or toggled visible.'],
    whenNotToUse: ['The level should be permanently loaded; use load.'],
    inputProps: {
      levelPath: P.levelPath, levelName: P.levelName,
      shouldBeLoaded: P.shouldBeLoaded, shouldBeVisible: P.shouldBeVisible,
    },
    required: ['shouldBeLoaded'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'stream', levelPath: '/Game/Maps/Sub01', shouldBeLoaded: true, shouldBeVisible: true },
    exampleOutput: { success: true, message: 'Level streaming state updated' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'unload', dispatchAction: 'stream_level', dispatchMode: 'action',
    domain: D, family: 'streaming',
    summary: 'Unload a streaming level (force stream-out with load and visibility false).',
    whenToUse: ['A streaming level must be removed from the world.'],
    whenNotToUse: ['The level asset should be deleted; use delete.'],
    inputProps: { levelPath: P.levelPath, levelName: P.levelName },
    required: ['levelPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'unload', levelPath: '/Game/Maps/Sub01' },
    exampleOutput: { success: true, message: 'Level unloaded' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'unload_level', dispatchAction: 'unload_level',
    domain: D, family: 'streaming',
    summary: 'Unload a streaming level via the manage_level parent (alias of unload).',
    whenToUse: ['A streaming level must be unloaded using the unload_level verb.'],
    whenNotToUse: ['Prefer the shorter unload verb.'],
    inputProps: { levelPath: P.levelPath, levelName: P.levelName },
    required: ['levelPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'unload_level', levelPath: '/Game/Maps/Sub01' },
    exampleOutput: { success: true, message: 'Level unloaded' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'create_light', dispatchAction: 'manage_lighting', dispatchMode: 'action',
    domain: D, family: 'lighting',
    summary: 'Create a light actor in the current level.',
    whenToUse: ['A Point, Directional, Spot, Sky, or Rect light must be added.'],
    whenNotToUse: ['A generic actor spawn is needed; use control_actor.'],
    inputProps: {
      lightType: P.lightType, name: P.name, intensity: P.intensity, color: P.color,
      location: P.location, rotation: P.rotation,
    },
    required: ['lightType'],
    effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_light', lightType: 'Point', name: 'KeyLight', location: { x: 0, y: 0, z: 200 } },
    exampleOutput: { success: true, message: 'Light created' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'build_lighting', dispatchAction: 'manage_lighting', dispatchMode: 'action',
    domain: D, family: 'lighting',
    summary: 'Build baked lighting for the current level (long-running; cannot be cancelled mid-bake).',
    whenToUse: ['Baked lighting must be (re)computed for the level.'],
    whenNotToUse: ['Dynamic lighting is in use and no bake is required.'],
    inputProps: {
      quality: P.quality,
    },
    required: [],
    effect: 'write', behavior: { longRunning: true, safeToRetry: false },
    costLatency: 'long-running', costResources: 'high',
    exampleInput: { action: 'build_lighting', quality: 'Preview' },
    exampleOutput: { success: true, message: 'Lighting build complete' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'set_metadata', dispatchAction: 'set_metadata', dispatchMode: 'action',
    domain: D, family: 'metadata',
    summary: 'Write metadata key/value pairs to a level asset via the shared metadata bridge route.',
    whenToUse: ['Level asset metadata must be written.'],
    whenNotToUse: ['The target is not a /Game level asset path.'],
    inputProps: { levelPath: P.levelPath, assetPath: P.assetPath, metadata: P.metadata },
    required: ['levelPath', 'metadata'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_metadata', levelPath: '/Game/Maps/Demo', metadata: { author: 'MCP' } },
    exampleOutput: { success: true, message: 'Metadata set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'list_levels', dispatchAction: 'list_levels', dispatchMode: 'action',
    domain: D, family: 'query',
    summary: 'List all levels available in the project.',
    whenToUse: ['The set of available level assets must be enumerated.'],
    whenNotToUse: ['A single level summary is needed; use get_summary.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'list_levels' },
    exampleOutput: { success: true, message: 'Levels listed', levels: ['/Game/Maps/Demo'] },
    outputProps: { levels: { type: 'array', items: { type: 'string' }, description: 'Available level asset paths.' } },
    outputRequired: ['levels'],
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'get_current_level', dispatchAction: 'get_current_level',
    domain: D, family: 'query',
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
    required: ['levelPath'],
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
  buildCoreRecord({
    parentTool: 'manage_level', action: 'export_level', dispatchAction: 'export_level',
    domain: D, family: 'io',
    summary: 'Export a level asset to a file.',
    whenToUse: ['A level must be serialized to an external file.'],
    whenNotToUse: ['The level should be saved as a new asset; use save_as.'],
    inputProps: { levelPath: P.levelPath, exportPath: P.exportPath, destinationPath: P.destinationPath },
    required: ['levelPath'],
    effect: 'read', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'export_level', levelPath: '/Game/Maps/Demo', exportPath: '/Temp/Demo.export' },
    exampleOutput: { success: true, message: 'Level exported' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'import_level', dispatchAction: 'import_level',
    domain: D, family: 'io',
    summary: 'Import a level asset from a package or source file.',
    whenToUse: ['A level must be imported from an external file.'],
    whenNotToUse: ['A new empty level is needed; use create_level.'],
    inputProps: {
      packagePath: P.packagePath, sourcePath: P.sourcePath,
      destinationPath: P.destinationPath, targetPath: P.targetPath,
      overwrite: P.overwrite,
    },
    required: ['packagePath'],
    effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'import_level', packagePath: '/Temp/Demo.export', destinationPath: '/Game/Maps/Imported' },
    exampleOutput: { success: true, message: 'Level imported' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
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
    required: ['subLevelPath'],
    effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_sublevel', subLevelPath: '/Game/Maps/Sub01', parentLevel: '/Game/Maps/Demo' },
    exampleOutput: { success: true, message: 'Sub-level added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
