/**
 * Sessions & Local Multiplayer Handlers
 *
 * Complete session management including:
 * - Session Management (local session settings, session interface)
 * - Local Multiplayer (split-screen, local players)
 * - LAN (LAN play configuration, hosting, joining)
 * - Voice Chat (voice settings, channels, muting, attenuation, push-to-talk)
 *
 * @module sessions-handlers
 */

import { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { createSubActionDispatcher, createUnknownActionResponse } from '../foundation/dispatch/common-handlers.js';


const SESSIONS_ACTIONS = new Set([
  'configure_local_session_settings',
  'configure_session_interface',
  'configure_split_screen',
  'set_split_screen_type',
  'add_local_player',
  'remove_local_player',
  'configure_lan_play',
  'host_lan_server',
  'join_lan_server',
  'enable_voice_chat',
  'configure_voice_settings',
  'set_voice_channel',
  'mute_player',
  'set_voice_attenuation',
  'configure_push_to_talk',
  'get_sessions_info',
]);

/**
 * Handles all sessions and local multiplayer actions for the manage_sessions tool.
 */
export async function handleSessionsTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { sendRequest } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_sessions',
    domainName: 'sessions'
  });

  if (SESSIONS_ACTIONS.has(action)) {
    return sendRequest(action);
  }
  return createUnknownActionResponse(`Unknown sessions action: ${action}`);
}
