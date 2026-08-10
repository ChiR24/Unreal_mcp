import type { CapabilityRecordSource } from '../../index.js';
import { utilityRecord } from '../utility/helpers.js';

const T = 'manage_networking' as const;
const f = (action: string, summary: string, params: readonly string[], required: readonly string[], outputs: readonly string[] = [], outputRequired: readonly string[] = [], read = false): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: 'gameFramework', summary, params, required, outputs, outputRequired,
  effect: read ? 'read' : 'write', safeToRetry: read, dispatchAction: 'manage_game_framework',
});
const create = (action: string, label: string, extra: readonly string[] = []): CapabilityRecordSource => f(
  action, `Create a ${label} Blueprint asset and return its path.`,
  ['name', 'path', 'parentClass', 'save', ...extra], ['name', 'path'], ['assetPath'], ['assetPath'],
);

export const NETWORKING_FRAMEWORK_RECORDS: readonly CapabilityRecordSource[] = [
  create('create_game_mode', 'GameMode', ['defaultPawnClass', 'playerControllerClass', 'gameStateClass', 'playerStateClass', 'hudClass']),
  create('create_game_state', 'GameState'),
  create('create_player_controller', 'PlayerController'),
  create('create_player_state', 'PlayerState'),
  create('create_game_instance', 'GameInstance'),
  create('create_hud_class', 'HUD'),
  f('set_default_pawn_class', 'Set a GameMode default pawn class.', ['gameModeBlueprint', 'blueprintPath', 'pawnClass', 'defaultPawnClass'], ['gameModeBlueprint', 'pawnClass']),
  f('set_player_controller_class', 'Set a GameMode PlayerController class.', ['gameModeBlueprint', 'blueprintPath', 'playerControllerClass'], ['gameModeBlueprint', 'playerControllerClass']),
  f('set_game_state_class', 'Set a GameMode GameState class.', ['gameModeBlueprint', 'blueprintPath', 'gameStateClass'], ['gameModeBlueprint', 'gameStateClass']),
  f('set_player_state_class', 'Set a GameMode PlayerState class.', ['gameModeBlueprint', 'blueprintPath', 'playerStateClass'], ['gameModeBlueprint', 'playerStateClass']),
  f('configure_game_rules', 'Configure GameMode rule flags.', ['gameModeBlueprint', 'blueprintPath', 'bDelayedStart'], ['gameModeBlueprint']),
  f('setup_match_states', 'Configure GameMode match states.', ['gameModeBlueprint', 'blueprintPath', 'states'], ['gameModeBlueprint']),
  f('configure_round_system', 'Configure round-based gameplay.', ['gameModeBlueprint', 'blueprintPath', 'numRounds', 'roundTime', 'intermissionTime'], ['gameModeBlueprint']),
  f('configure_team_system', 'Configure team-based gameplay.', ['gameModeBlueprint', 'blueprintPath', 'numTeams', 'teamSize', 'autoBalance', 'friendlyFire'], ['gameModeBlueprint']),
  f('configure_scoring_system', 'Configure gameplay scoring.', ['gameModeBlueprint', 'blueprintPath', 'scorePerKill', 'scorePerAssist', 'scorePerObjective', 'scorePerDeath', 'winScore'], ['gameModeBlueprint']),
  f('configure_spawn_system', 'Configure player spawning.', ['gameModeBlueprint', 'blueprintPath', 'spawnSelectionMethod', 'respawnDelay', 'respawnLocation', 'usePlayerStarts', 'canRespawn', 'maxRespawns'], ['gameModeBlueprint']),
  f('configure_player_start', 'Configure PlayerStart behavior.', ['blueprintPath', 'gameModeBlueprint', 'teamIndex'], ['blueprintPath']),
  f('set_respawn_rules', 'Configure player respawn rules.', ['gameModeBlueprint', 'blueprintPath', 'respawnDelay', 'respawnLocation', 'forceRespawn', 'respawnLives'], ['gameModeBlueprint']),
  f('configure_spectating', 'Configure spectator behavior.', ['gameModeBlueprint', 'blueprintPath', 'allowSpectating', 'spectatorClass', 'spectatorViewMode'], ['gameModeBlueprint']),
  f('get_game_framework_info', 'Read Game Framework class and rule state.', ['gameModeBlueprint', 'blueprintPath'], [], ['gameFrameworkInfo'], ['gameFrameworkInfo'], true),
];
