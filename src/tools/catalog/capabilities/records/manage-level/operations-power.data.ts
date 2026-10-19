/**
 * Level operations: streaming, lighting, metadata, settings, and list (8
 * actions). CONTRACT: these records are the first 8 entries of
 * OPERATIONS_RECORDS in definition order, so this shard must stay first in
 * the concatenation.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

export const D = 'level';
export const NR = 'Distinct manage_level operation verb and target; no cross-tool duplicate.';

// stream, unload and unload_level share one handler (McpLevelStream.cpp) and its reply.
const STREAM_LEVEL_PATH = { ...P.levelPath, description: 'The sublevel: its full package path (/Game/Maps/Sub01; an object path or .umap name is cut to the package) or the part after its last slash (Sub01). A name two sublevels share is refused (AMBIGUOUS_LEVEL, listing them in matches).' };
const STREAM_LEVEL_NAME = { ...P.levelName, description: 'Alias of levelPath, read first when both are given.' };
const STREAM_OUT = {
  levelName: { type: 'string', description: 'The name asked for, read as a package.' },
  levelPath: { type: 'string', description: 'The package path of the sublevel that matched.' },
  world: { type: 'string', enum: ['play', 'editor'], description: 'play: a Play session ran and the level streamed in the running game. editor: it was shown or hidden in the editor.' },
  loaded: { type: 'boolean', description: 'Whether the level is loaded now; always true in the editor, which keeps every sublevel loaded.' },
  visible: { type: 'boolean', description: 'Whether the level is drawn now.' },
  streamingState: { type: 'string', enum: ['Loaded', 'Unloaded', 'Pending'], description: 'Loaded or Unloaded, or Pending while the game is still streaming it.' },
};

export const OPERATIONS_POWER_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'manage_level', action: 'stream', dispatchAction: 'stream_level',
    domain: D, family: 'streaming',
    summary: 'Stream a sublevel of the open level in or out: during Play in the running game (settled before the reply), otherwise shown or hidden in the editor, which keeps every sublevel loaded.',
    whenToUse: ['A sublevel must be streamed in or out while the game plays, or shown or hidden in the editor.'],
    whenNotToUse: ['The level is not a sublevel of the open level yet (STREAMING_LEVEL_NOT_FOUND): add it with add_sublevel first.', 'The level should replace the open one; use load.'],
    inputProps: {
      levelPath: STREAM_LEVEL_PATH, levelName: STREAM_LEVEL_NAME,
      shouldBeLoaded: { ...P.shouldBeLoaded, description: 'Load it (true, the default) or stream it out (false). In the editor false hides it instead, with a warning; a sublevel added AlwaysLoaded stays loaded in the game and is hidden.' },
      shouldBeVisible: { ...P.shouldBeVisible, description: 'Draw it (true, the default) or keep it loaded but hidden (false); ignored when shouldBeLoaded is false.' },
    },
    required: [],
    requiredOneOf: ['levelPath', 'levelName'],
    outputProps: STREAM_OUT,
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive',
    exampleInput: { action: 'stream', levelPath: '/Game/Maps/Sub01', shouldBeLoaded: true, shouldBeVisible: true },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'unload', dispatchAction: 'stream_level',
    domain: D, family: 'streaming',
    summary: 'Stream a sublevel of the open level out: during Play it unloads in the running game; in the editor, which keeps every sublevel loaded, it is hidden instead and the reply warns.',
    whenToUse: ['A sublevel must stop being drawn: unloaded while the game plays, hidden in the editor.'],
    whenNotToUse: ['The level asset should be deleted; use delete.', 'The open level itself: open another one with load.'],
    inputProps: { levelPath: STREAM_LEVEL_PATH, levelName: STREAM_LEVEL_NAME },
    required: [],
    requiredOneOf: ['levelPath', 'levelName'],
    outputProps: STREAM_OUT,
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive',
    exampleInput: { action: 'unload', levelPath: '/Game/Maps/Sub01' },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'unload_level', dispatchAction: 'unload_level',
    domain: D, family: 'streaming',
    summary: 'Stream a sublevel out (alias of unload): unloaded during Play, hidden with a warning in the editor.',
    whenToUse: ['A sublevel must be streamed out using the unload_level verb.'],
    whenNotToUse: ['Prefer the shorter unload verb.'],
    inputProps: { levelPath: STREAM_LEVEL_PATH, levelName: STREAM_LEVEL_NAME },
    required: [],
    requiredOneOf: ['levelPath', 'levelName'],
    outputProps: STREAM_OUT,
    effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive',
    exampleInput: { action: 'unload_level', levelPath: '/Game/Maps/Sub01' },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'create_light', dispatchAction: 'manage_lighting',
    domain: D, family: 'lighting',
    summary: 'Spawn a light actor in the level being edited: lightType point, directional, spot, rect or sky (or any light class as lightClass), at location (default 0, 0, 300), labelled name, made Movable; intensity, color and properties are applied, and the reply lists what was applied, refused or adjusted. One undo step; the level is left unsaved. During Play it still spawns into the edited level, not the running game.',
    whenToUse: ['A Point, Directional, Spot, Sky, or Rect light must be added.'],
    whenNotToUse: [
      'A generic actor spawn is needed; use control_actor.',
      'build_environment.create_light runs this same handler; prefer it, which carries the full light documentation.',
      'The light must exist in the running PIE world; manage_effect.create_dynamic_light spawns into the active world.',
    ],
    inputProps: {
      lightType: P.lightType, lightClass: P.lightClass, name: P.name, intensity: P.intensity, color: P.color,
      location: P.location, rotation: P.rotation, properties: P.lightProperties,
    },
    required: [],
    requiredOneOf: ['lightType', 'lightClass'],
    effect: 'write', costLatency: 'interactive',
    exampleInput: {
      action: 'create_light', lightType: 'Spot', name: 'KeyLight', location: { x: 0, y: 0, z: 200 },
      color: [1, 0.7, 0.42], properties: { castShadows: true, outerConeAngle: 40 },
    },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'build_lighting', dispatchAction: 'manage_lighting',
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
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'set_metadata', dispatchAction: 'set_metadata',
    domain: D, family: 'metadata',
    summary: 'Write metadata key/value pairs to a level asset via the shared metadata bridge route.',
    whenToUse: ['Level asset metadata must be written.'],
    whenNotToUse: ['The target is not a /Game level asset path.'],
    inputProps: { levelPath: P.levelPath, metadata: P.metadata },
    required: ['levelPath', 'metadata'],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_metadata', levelPath: '/Game/Maps/Demo', metadata: { author: 'MCP' } },
  }),
  // The native Level dispatch has always routed `set_level_world_settings`
  // (McpLevelHandlers.cpp), but no record published it, so the
  // only way to set a level's GameMode override through the gateway was to fall
  // back to system_control.execute_python. Published here against the same
  // native action; the handler re-validates and reports what it applied.
  buildCoreRecord({
    parentTool: 'manage_level', action: 'set_world_settings', topics: ['kill z height', 'world gravity', 'change level gravity', 'override game mode'], dispatchAction: 'set_level_world_settings',
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
    effect: 'write', behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_world_settings', gameMode: '/Game/Maps/BP_RaceGameMode' },
    exampleOutput: {
      success: true, message: 'World settings updated (1 applied)',
      levelPath: '/Game/Maps/Demo', settingsApplied: true, appliedSettings: ['gameMode'],
      gameMode: '/Game/Maps/BP_RaceGameMode.BP_RaceGameMode_C', killZ: -100000, gravityZ: -980, timeDilation: 1,
    },
  }),
  buildCoreRecord({
    parentTool: 'manage_level', action: 'list_levels', dispatchAction: 'list_levels',
    domain: D, family: 'query',
    topics: ['all levels', 'maps in project', 'list maps', 'available levels', 'what maps exist', 'what levels exist'],
    summary: 'List the open level and its streaming sublevels (each with loaded, visible and streamingState) and every map the asset registry knows (allMaps, which can be long and comes in pages past 100k characters). Read from the editor world even while Play In Editor runs; the persistent level\'s name is short, a sublevel\'s is its package path.',
    whenToUse: ['The set of available level assets must be enumerated.'],
    whenNotToUse: ['A single level summary is needed; use get_summary.'],
    inputProps: {},
    required: [],
    effect: 'read',
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
  }),
];
