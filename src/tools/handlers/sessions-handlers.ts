/**
 * Sessions & Local Multiplayer Handlers (Phase 22)
 *
 * Complete session management including:
 * - Session Management (local session settings, session interface)
 * - Local Multiplayer (split-screen, local players)
 * - LAN (LAN play configuration, hosting, joining)
 * - Voice Chat (voice settings, channels, muting, attenuation, push-to-talk)
 *
 * @module sessions-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all sessions and local multiplayer actions for the manage_sessions tool.
 */
export async function handleSessionsTools(
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
      'manage_sessions',
      payload as HandlerArgs,
      `Automation bridge not available for sessions action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };

  switch (action) {
    // ========================================================================
    // Session Management (2 actions)
    // ========================================================================
    case 'configure_local_session_settings':
      return sendRequest('configure_local_session_settings');

    case 'configure_session_interface':
      return sendRequest('configure_session_interface');

    // ========================================================================
    // Local Multiplayer (4 actions)
    // ========================================================================
    case 'configure_split_screen':
      return sendRequest('configure_split_screen');

    case 'set_split_screen_type':
      return sendRequest('set_split_screen_type');

    case 'add_local_player':
      return sendRequest('add_local_player');

    case 'remove_local_player':
      return sendRequest('remove_local_player');

    // ========================================================================
    // LAN (3 actions)
    // ========================================================================
    case 'configure_lan_play':
      return sendRequest('configure_lan_play');

    case 'host_lan_server':
      return sendRequest('host_lan_server');

    case 'join_lan_server':
      return sendRequest('join_lan_server');

    // ========================================================================
    // Voice Chat (6 actions)
    // ========================================================================
    case 'enable_voice_chat':
      return sendRequest('enable_voice_chat');

    case 'configure_voice_settings':
      return sendRequest('configure_voice_settings');

    case 'set_voice_channel':
      return sendRequest('set_voice_channel');

    case 'mute_player':
      return sendRequest('mute_player');

    case 'set_voice_attenuation':
      return sendRequest('set_voice_attenuation');

    case 'configure_push_to_talk':
      return sendRequest('configure_push_to_talk');

    // ========================================================================
    // Utility (1 action)
    // ========================================================================
    case 'get_sessions_info':
      return sendRequest('get_sessions_info');

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown sessions action: ${action}`
      });
  }
}
