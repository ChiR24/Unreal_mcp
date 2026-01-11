/**
 * Phase 30: Cinematics & Media - Media Framework Handlers
 * Handles media players, sources, textures, playlists,
 * playback control, and texture binding.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for media framework tools
 */
export async function handleMediaTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<HandlerResult> {
  // Build the payload for automation request
  const payload: Record<string, unknown> = {
    action: action,
    ...args
  };

  // Remove undefined values
  Object.keys(payload).forEach(key => {
    if (payload[key] === undefined) {
      delete payload[key];
    }
  });

  switch (action) {
    // =========================================
    // ASSET CREATION
    // =========================================
    case 'create_media_player':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for create_media_player'
      )) as HandlerResult;

    case 'create_file_media_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for create_file_media_source'
      )) as HandlerResult;

    case 'create_stream_media_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for create_stream_media_source'
      )) as HandlerResult;

    case 'create_media_texture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for create_media_texture'
      )) as HandlerResult;

    case 'create_media_playlist':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for create_media_playlist'
      )) as HandlerResult;

    case 'create_media_sound_wave':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for create_media_sound_wave'
      )) as HandlerResult;

    // =========================================
    // ASSET MANAGEMENT
    // =========================================
    case 'delete_media_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for delete_media_asset'
      )) as HandlerResult;

    case 'get_media_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for get_media_info'
      )) as HandlerResult;

    // =========================================
    // PLAYLIST MANAGEMENT
    // =========================================
    case 'add_to_playlist':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for add_to_playlist'
      )) as HandlerResult;

    case 'remove_from_playlist':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for remove_from_playlist'
      )) as HandlerResult;

    case 'get_playlist':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for get_playlist'
      )) as HandlerResult;

    // =========================================
    // PLAYBACK CONTROL
    // =========================================
    case 'open_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for open_source'
      )) as HandlerResult;

    case 'open_url':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for open_url'
      )) as HandlerResult;

    case 'close':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for close'
      )) as HandlerResult;

    case 'play':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for play'
      )) as HandlerResult;

    case 'pause':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for pause'
      )) as HandlerResult;

    case 'stop':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for stop'
      )) as HandlerResult;

    case 'seek':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for seek'
      )) as HandlerResult;

    case 'set_rate':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for set_rate'
      )) as HandlerResult;

    // =========================================
    // PROPERTIES
    // =========================================
    case 'set_looping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for set_looping'
      )) as HandlerResult;

    case 'get_duration':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for get_duration'
      )) as HandlerResult;

    case 'get_time':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for get_time'
      )) as HandlerResult;

    case 'get_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for get_state'
      )) as HandlerResult;

    // =========================================
    // TEXTURE BINDING
    // =========================================
    case 'bind_to_texture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for bind_to_texture'
      )) as HandlerResult;

    case 'unbind_from_texture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        'Automation bridge not available for unbind_from_texture'
      )) as HandlerResult;

    // =========================================
    // DEFAULT - PASS THROUGH
    // =========================================
    default:
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_media',
        payload,
        `Automation bridge not available for media action: ${action}`
      )) as HandlerResult;
  }
}
