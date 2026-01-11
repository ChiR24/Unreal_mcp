/**
 * Game Framework Handlers (Phase 21)
 *
 * Complete game framework system including:
 * - Core Classes (GameMode, GameState, PlayerController, PlayerState, GameInstance, HUD)
 * - Game Mode Configuration (default pawn, player controller, game state classes, game rules)
 * - Match Flow (match states, round system, team system, scoring, spawn system)
 * - Player Management (player start, respawn rules, spectating)
 *
 * @module game-framework-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Gets the game mode blueprint path from args, supporting both `gameModeBlueprint` and `blueprintPath`.
 */
function getGameModePath(argsRecord: Record<string, unknown>): string | undefined {
  return (argsRecord.gameModeBlueprint ?? argsRecord.blueprintPath) as string | undefined;
}

/**
 * Handles all game framework actions for the manage_game_framework tool.
 */
export async function handleGameFrameworkTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<HandlerResult> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_game_framework',
      payload as HandlerArgs,
      `Automation bridge not available for game framework action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };

  switch (action) {
    // =========================================================================
    // 21.1 Core Classes (6 actions)
    // =========================================================================

    case 'create_game_mode': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.path, 'path', 'Missing required parameter: path');
      // Creates a new GameModeBase blueprint
      // Optional: parentClass, defaultPawnClass, playerControllerClass, gameStateClass, playerStateClass, hudClass
      return sendRequest('create_game_mode');
    }

    case 'create_game_state': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.path, 'path', 'Missing required parameter: path');
      // Creates a new GameStateBase blueprint
      // Optional: parentClass
      return sendRequest('create_game_state');
    }

    case 'create_player_controller': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.path, 'path', 'Missing required parameter: path');
      // Creates a new PlayerController blueprint
      // Optional: parentClass
      return sendRequest('create_player_controller');
    }

    case 'create_player_state': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.path, 'path', 'Missing required parameter: path');
      // Creates a new PlayerState blueprint
      // Optional: parentClass
      return sendRequest('create_player_state');
    }

    case 'create_game_instance': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.path, 'path', 'Missing required parameter: path');
      // Creates a new GameInstance blueprint for persistent game data
      // Optional: parentClass
      return sendRequest('create_game_instance');
    }

    case 'create_hud_class': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.path, 'path', 'Missing required parameter: path');
      // Creates a new HUD blueprint
      // Optional: parentClass, hudClass
      return sendRequest('create_hud_class');
    }

    // =========================================================================
    // 21.2 Game Mode Configuration (5 actions)
    // =========================================================================

    case 'set_default_pawn_class': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      requireNonEmptyString(argsRecord.pawnClass ?? argsRecord.defaultPawnClass, 'pawnClass', 'Missing required parameter: pawnClass or defaultPawnClass');
      // Sets the default pawn class for a game mode
      return sendRequest('set_default_pawn_class');
    }

    case 'set_player_controller_class': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      requireNonEmptyString(argsRecord.playerControllerClass, 'playerControllerClass', 'Missing required parameter: playerControllerClass');
      // Sets the player controller class for a game mode
      return sendRequest('set_player_controller_class');
    }

    case 'set_game_state_class': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      requireNonEmptyString(argsRecord.gameStateClass, 'gameStateClass', 'Missing required parameter: gameStateClass');
      // Sets the game state class for a game mode
      return sendRequest('set_game_state_class');
    }

    case 'set_player_state_class': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      requireNonEmptyString(argsRecord.playerStateClass, 'playerStateClass', 'Missing required parameter: playerStateClass');
      // Sets the player state class for a game mode
      return sendRequest('set_player_state_class');
    }

    case 'configure_game_rules': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      // Configures game rules like friendly fire, time limits, score limits
      // Optional: friendlyFire, timeLimit, scoreLimit, bDelayedStart, startPlayersNeeded
      return sendRequest('configure_game_rules');
    }

    // =========================================================================
    // 21.3 Match Flow (5 actions)
    // =========================================================================

    case 'setup_match_states': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      // Sets up match state machine (waiting, warmup, in_progress, post_match, custom)
      // Optional: states (array of state definitions)
      return sendRequest('setup_match_states');
    }

    case 'configure_round_system': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      // Configures round-based gameplay
      // Optional: numRounds, roundTime, intermissionTime
      return sendRequest('configure_round_system');
    }

    case 'configure_team_system': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      // Configures team-based gameplay
      // Optional: numTeams, teamSize, autoBalance, friendlyFire
      return sendRequest('configure_team_system');
    }

    case 'configure_scoring_system': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      // Configures scoring mechanics
      // Optional: scorePerKill, scorePerAssist, scorePerObjective, scoreLimit
      return sendRequest('configure_scoring_system');
    }

    case 'configure_spawn_system': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      // Configures spawn system behavior
      // Optional: spawnSelectionMethod, respawnDelay, respawnLocation, usePlayerStarts
      return sendRequest('configure_spawn_system');
    }

    // =========================================================================
    // 21.4 Player Management (3 actions)
    // =========================================================================

    case 'configure_player_start': {
      // Requires blueprintPath to identify context
      const bpPath = argsRecord.blueprintPath ?? argsRecord.gameModeBlueprint;
      requireNonEmptyString(bpPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Optional: teamIndex, location, rotation, bPlayerOnly
      return sendRequest('configure_player_start');
    }

    case 'set_respawn_rules': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      // Configures respawn behavior
      // Optional: respawnDelay, respawnLocation, respawnConditions
      return sendRequest('set_respawn_rules');
    }

    case 'configure_spectating': {
      const gmPath = getGameModePath(argsRecord);
      requireNonEmptyString(gmPath, 'gameModeBlueprint', 'Missing required parameter: gameModeBlueprint or blueprintPath');
      // Configures spectator mode settings
      // Optional: allowSpectating, spectatorClass, spectatorViewMode
      return sendRequest('configure_spectating');
    }

    // =========================================================================
    // 21.5 Utility (1 action)
    // =========================================================================

    case 'get_game_framework_info': {
      // Returns game framework info for a level or game mode
      // Optional: levelPath, gameModeBlueprint, blueprintPath
      return sendRequest('get_game_framework_info');
    }

    // =========================================================================
    // Default: Unknown action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown manage_game_framework action: ${action}`
      });
  }
}
