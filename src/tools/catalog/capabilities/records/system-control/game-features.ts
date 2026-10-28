/**
 * Game feature plugin records (2): list_game_features, set_game_feature_active.
 *
 * Game feature plugins (under Plugins/GameFeatures, loaded on demand) are driven by the Game Features subsystem, whose
 * C++ has no reflected functions: the native handler reaches it through IMcpGameFeaturesProvider, which the module
 * EngineWireGameFeatures registers only where the project enables the GameFeatures plugin. The state enum is public
 * from 5.1, so both records need it. create_plugin template game_feature makes one.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { ANY_EDITOR_STATE, buildCoreRecord } from '../core/builder.js';

const PT = 'system_control';

export const GAME_FEATURE_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: PT,
    action: 'list_game_features',
    domain: 'project',
    family: 'game_feature',
    topics: ['game feature plugins', 'game features', 'modular gameplay', 'which game features are active', 'game feature state'],
    summary: 'List the game feature plugins the Game Features subsystem knows (plugins under Plugins/GameFeatures that load on demand), each with its state (installed, registered, loaded or active, else the engine\'s own name for a step on the way, such as Activating, or an error, such as ErrorRegistering) and the state its descriptor asks for at startup (initialState). Needs the GameFeatures plugin: GAME_FEATURES_DISABLED otherwise (GAME_FEATURES_UNAVAILABLE in a build of EngineWire without its game features module, such as the Fab one).',
    whenToUse: [
      'Which game features exist, or which are active now, must be known before activating one or adding content to it.',
    ],
    whenNotToUse: ['Plugins in general are listed (use list_plugins).'],
    inputProps: {},
    outputProps: {
      gameFeatures: {
        type: 'array',
        description: 'Name-sorted.',
        items: {
          type: 'object',
          additionalProperties: false,
          properties: {
            pluginName: { type: 'string', description: 'The plugin.' },
            state: { type: 'string', description: 'installed, registered, loaded or active; else the engine\'s name for a step on the way or an error.' },
            initialState: { type: 'string', description: 'What its descriptor asks for at startup (BuiltInInitialFeatureState), absent when it says nothing.' },
          },
        },
      },
    },
    effect: 'read',
    plugins: ['GameFeatures'],
    editorStates: ANY_EDITOR_STATE,
    minEngineVersion: '5.1',
    dispatchAction: 'system_control',
    exampleInput: { action: 'list_game_features' },
    exampleOutput: { success: true, gameFeatures: [{ pluginName: 'ShooterCore', state: 'active', initialState: 'Active' }] },
  }),
  buildCoreRecord({
    parentTool: PT,
    action: 'set_game_feature_active',
    domain: 'project',
    family: 'game_feature',
    topics: ['activate game feature', 'deactivate game feature', 'turn on a game feature', 'load game feature plugin'],
    summary: 'Activate a game feature plugin (its content loads and its actions run in the game: components added, abilities granted, input mapped) or deactivate it (active false). The reply comes when the subsystem has finished, with the state reached, or after 20 seconds with GAME_FEATURE_PENDING while it is still on its way; one already in the asked state answers changed false. A name the subsystem does not know is GAME_FEATURE_NOT_FOUND; GAME_FEATURES_DISABLED when the project leaves the GameFeatures plugin off (GAME_FEATURES_UNAVAILABLE in a build of EngineWire without its game features module).',
    whenToUse: ['A game feature\'s content or actions must take effect in the editor or Play (or stop).'],
    whenNotToUse: ['A plugin that is no game feature is turned on or off (use enable_plugin, which needs a restart).'],
    inputProps: {
      pluginName: { type: 'string', description: 'The game feature plugin, as list_game_features names it.' },
      active: { type: 'boolean', description: 'true activates (default), false deactivates.' },
    },
    required: ['pluginName'],
    outputProps: {
      pluginName: { type: 'string', description: 'The plugin.' },
      state: { type: 'string', description: 'Its state now.' },
      changed: { type: 'boolean', description: 'False when it was in the asked state already.' },
    },
    effect: 'write',
    costLatency: 'interactive',
    plugins: ['GameFeatures'],
    editorStates: ANY_EDITOR_STATE,
    minEngineVersion: '5.1',
    dispatchAction: 'system_control',
    exampleInput: { action: 'set_game_feature_active', pluginName: 'ShooterCore', active: true },
    exampleOutput: { success: true, pluginName: 'ShooterCore', state: 'active', changed: true },
  }),
];
