import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { utilityRecord, withInputProps } from '../utility/utility-record-builders.js';

const T = 'manage_networking' as const;
// Every Game Framework action authors a Blueprint asset, and an unsaved edit is lost on the next
// editor restart, so the handler saves unless told not to.
const SAVE: Readonly<Record<string, JsonObject>> = {
  save: { type: 'boolean', description: 'Save the Blueprint asset to disk after the change (default true).' },
};
const f = (action: string, summary: string, params: readonly string[], required: readonly string[], outputs: readonly string[] = [], outputRequired: readonly string[] = [], read = false, props: Readonly<Record<string, JsonObject>> = {}): CapabilityRecordSource => {
  const record = utilityRecord({
    tool: T, action, family: 'gameFramework', summary, params, required, outputs, outputRequired,
    effect: read ? 'read' : 'write', safeToRetry: read, dispatchAction: 'manage_game_framework',
  });
  return withInputProps(record, read ? props : { ...SAVE, ...props });
};
const create = (action: string, label: string, extra: readonly string[] = []): CapabilityRecordSource => f(
  action, `Create a ${label} Blueprint asset and return its path; an unloadable parentClass, or one that is not a ${label}, is refused.`,
  ['name', 'path', 'parentClass', 'save', ...extra], ['name', 'path'], ['assetPath'], ['assetPath'],
);
// Setting a class changes only this game mode. makeDefault opts in to making it the project default
// (DefaultEngine.ini), and the reply always says which game mode the open level runs in play.
const EFFECTIVE = ' Pass makeDefault: true to also make this game mode the project default; the reply says whether the open level runs it (effectiveInOpenLevel).';
const setClass = (action: string, label: string, field: string, alias: readonly string[] = []): CapabilityRecordSource => f(
  action, `Set a GameMode ${label} class.${EFFECTIVE}`, ['gameModeBlueprint', 'blueprintPath', field, ...alias, 'makeDefault', 'save'], ['gameModeBlueprint', field],
  ['madeDefault', 'effectiveInOpenLevel', 'openLevelGameMode'],
);

export const NETWORKING_FRAMEWORK_RECORDS: readonly CapabilityRecordSource[] = [
  create('create_game_mode', 'GameMode', ['defaultPawnClass', 'playerControllerClass', 'gameStateClass', 'playerStateClass', 'hudClass']),
  create('create_game_state', 'GameState'),
  create('create_player_controller', 'PlayerController'),
  create('create_player_state', 'PlayerState'),
  create('create_game_instance', 'GameInstance'),
  create('create_hud_class', 'HUD'),
  setClass('set_default_pawn_class', 'default pawn', 'pawnClass', ['defaultPawnClass']),
  setClass('set_player_controller_class', 'PlayerController', 'playerControllerClass'),
  setClass('set_game_state_class', 'GameState', 'gameStateClass'),
  setClass('set_player_state_class', 'PlayerState', 'playerStateClass'),
  setClass('set_hud_class', 'HUD', 'hudClass'),
  f('configure_game_rules', 'Set a GameMode\'s bDelayedStart match rule; only GameMode (AGameMode) children have it, a GameModeBase child is refused with NOT_SUPPORTED.', ['gameModeBlueprint', 'blueprintPath', 'bDelayedStart', 'save'], ['gameModeBlueprint', 'bDelayedStart'], [], [], false, {
    bDelayedStart: { type: 'boolean', description: 'Hold the match in WaitingToStart until StartMatch is called (AGameMode::bDelayedStart).' },
  }),
  f('set_respawn_rules', 'Set the game mode\'s minimum respawn delay (AGameMode::MinRespawnDelay); a GameModeBase child is refused with NOT_SUPPORTED.', ['gameModeBlueprint', 'blueprintPath', 'respawnDelay', 'save'], ['gameModeBlueprint', 'respawnDelay']),
  f('configure_spectating', 'Set the GameMode spectator pawn class (SpectatorClass).', ['gameModeBlueprint', 'blueprintPath', 'spectatorClass', 'save'], ['gameModeBlueprint', 'spectatorClass'], [], [], false, {
    spectatorClass: { type: 'string', description: 'SpectatorPawn class path, e.g. /Script/Engine.SpectatorPawn or a Blueprint path.' },
  }),
  f('get_game_framework_info', 'Read Game Framework class and rule state.', ['gameModeBlueprint', 'blueprintPath'], [], ['gameFrameworkInfo'], ['gameFrameworkInfo'], true),
];
