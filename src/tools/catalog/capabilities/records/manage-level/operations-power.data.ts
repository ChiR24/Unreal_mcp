/**
 * Level operations: streaming, lighting, metadata, settings, and list (8
 * actions). CONTRACT: these records are the first 8 entries of
 * OPERATIONS_RECORDS in definition order, so this shard must stay first in
 * the concatenation.
 *
 * Grounded in:
 * - src/tools/handlers/level/runtime/level-handlers.ts: stream/unload route
 *   to the stream_level bridge action (dispatchMode 'action'); unload_level
 *   is NOT in the switch and falls to the default manage_level dispatch
 *   (dispatchMode 'tool'); list_levels routes to the list_levels bridge
 *   action; set_metadata routes to the shared set_metadata bridge action.
 * - src/tools/handlers/level/runtime/level-light-handlers.ts: create_light
 *   and build_lighting route to the manage_lighting bridge action
 *   (dispatchMode 'action'); build_lighting uses LONG_RUNNING_OP_TIMEOUT_MS.
 * - native McpAutomationBridge_LevelHandlers.cpp: stream_level and
 *   build_lighting are top-level Level actions; set_metadata is not in the
 *   bIsLevelAction set, confirming it dispatches to a separate route.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

export const D = 'level';
export const NR = 'Distinct manage_level operation verb and target; no cross-tool duplicate.';

export const OPERATIONS_POWER_RECORDS: readonly CapabilityRecordSource[] = [
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
  // The native Level dispatch has always routed `set_level_world_settings`
  // (McpAutomationBridge_LevelHandlers.cpp), but no record published it, so the
  // only way to set a level's GameMode override through the gateway was to fall
  // back to system_control.execute_python. Published here against the same
  // native action; the handler re-validates and reports what it applied.
  buildCoreRecord({
    parentTool: 'manage_level', action: 'set_world_settings', dispatchAction: 'set_level_world_settings', dispatchMode: 'action',
    domain: D, family: 'settings',
    summary: "Set the loaded level's WorldSettings: GameMode override, kill Z, gravity, time dilation, and world bounds checks.",
    whenToUse: ['A level needs its GameMode override or physics/world defaults set.'],
    whenNotToUse: ['The GameMode class itself must be configured; use manage_networking.'],
    inputProps: {
      levelPath: P.levelPath,
      gameMode: P.gameMode,
      killZ: P.killZ,
      gravityZ: P.gravityZ,
      timeDilation: P.timeDilation,
      enableWorldBoundsChecks: P.enableWorldBoundsChecks,
    },
    required: [],
    // The handler rejects a call that supplies no setting, so the group is the
    // contract rather than a convenience.
    requiredOneOf: ['gameMode', 'killZ', 'gravityZ', 'timeDilation', 'enableWorldBoundsChecks'],
    outputProps: {
      levelPath: P.levelPath,
      settingsApplied: P.settingsApplied,
      appliedSettings: P.appliedSettings,
      gameMode: P.gameMode,
      killZ: P.killZ,
      gravityZ: P.gravityZ,
      timeDilation: P.timeDilation,
    },
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'set_world_settings', gameMode: '/Game/Maps/BP_RaceGameMode' },
    exampleOutput: {
      success: true, message: 'World settings updated (1 applied)',
      levelPath: '/Game/Maps/Demo', settingsApplied: true, appliedSettings: ['gameMode'],
      gameMode: '/Game/Maps/BP_RaceGameMode.BP_RaceGameMode_C', killZ: -100000, gravityZ: -980, timeDilation: 1,
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'list_levels', dispatchAction: 'list_levels', dispatchMode: 'action',
    domain: D, family: 'query',
    topics: ['all levels', 'maps in project', 'list maps', 'available levels'],
    summary: 'List all levels available in the project.',
    whenToUse: ['The set of available level assets must be enumerated.'],
    whenNotToUse: ['A single level summary is needed; use get_summary.'],
    inputProps: {},
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'list_levels' },
    exampleOutput: {
      success: true, message: 'Levels listed',
      currentWorldLevels: [{ name: 'Demo', path: '/Game/Maps/Demo', isPersistent: true, isLoaded: true, isVisible: true }],
      currentWorldLevelCount: 1,
      allMaps: [{ name: 'Demo', path: '/Game/Maps/Demo', objectPath: '/Game/Maps/Demo.Demo' }],
      allMapsCount: 1,
      currentMap: 'Demo', currentMapPath: '/Game/Maps/Demo',
    },
    outputProps: {
      currentWorldLevels: {
        type: 'array',
        items: {
          type: 'object',
          additionalProperties: false,
          properties: {
            name: { type: 'string', description: 'Level name.' },
            path: { type: 'string', description: 'Level package path.' },
            isPersistent: { type: 'boolean', description: 'Whether this is the persistent level.' },
            isLoaded: { type: 'boolean', description: 'Whether the level is loaded.' },
            isVisible: { type: 'boolean', description: 'Whether the level is visible.' },
            streamingState: { type: 'string', description: 'Streaming state for streaming levels.' },
          },
        },
        description: 'Levels in the current world (persistent level plus streaming levels).',
      },
      currentWorldLevelCount: { type: 'number', description: 'Number of current-world level entries.' },
      allMaps: {
        type: 'array',
        items: {
          type: 'object',
          additionalProperties: false,
          properties: {
            name: { type: 'string', description: 'Map asset name.' },
            path: { type: 'string', description: 'Map package path.' },
            objectPath: { type: 'string', description: 'Full object path of the map asset.' },
          },
        },
        description: 'All World assets discovered in the asset registry.',
      },
      allMapsCount: { type: 'number', description: 'Number of map assets in the asset registry.' },
      currentMap: { type: 'string', description: 'Name of the current persistent map.' },
      currentMapPath: { type: 'string', description: 'Package path of the current persistent map.' },
    },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
