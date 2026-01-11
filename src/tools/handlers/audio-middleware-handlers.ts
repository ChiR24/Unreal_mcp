/**
 * Phase 38: Audio Middleware Plugins Handlers
 * Handles Wwise (Audiokinetic), FMOD (Firelight Technologies), and Bink Video (built-in).
 * ~80 actions across 3 middleware categories.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for audio middleware tools
 */
export async function handleAudioMiddlewareTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<HandlerResult> {
  // Build the payload for automation request
  const payload: Record<string, unknown> = {
    action_type: action,
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
    // WWISE (30 actions)
    // =========================================
    case 'connect_wwise_project':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for connect_wwise_project'
      )) as HandlerResult;

    case 'post_wwise_event':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for post_wwise_event'
      )) as HandlerResult;

    case 'post_wwise_event_at_location':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for post_wwise_event_at_location'
      )) as HandlerResult;

    case 'stop_wwise_event':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for stop_wwise_event'
      )) as HandlerResult;

    case 'set_rtpc_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_rtpc_value'
      )) as HandlerResult;

    case 'set_rtpc_value_on_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_rtpc_value_on_actor'
      )) as HandlerResult;

    case 'get_rtpc_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_rtpc_value'
      )) as HandlerResult;

    case 'set_wwise_switch':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_wwise_switch'
      )) as HandlerResult;

    case 'set_wwise_switch_on_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_wwise_switch_on_actor'
      )) as HandlerResult;

    case 'set_wwise_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_wwise_state'
      )) as HandlerResult;

    case 'load_wwise_bank':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for load_wwise_bank'
      )) as HandlerResult;

    case 'unload_wwise_bank':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for unload_wwise_bank'
      )) as HandlerResult;

    case 'get_loaded_banks':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_loaded_banks'
      )) as HandlerResult;

    case 'create_wwise_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for create_wwise_component'
      )) as HandlerResult;

    case 'configure_wwise_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_wwise_component'
      )) as HandlerResult;

    case 'configure_spatial_audio':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_spatial_audio'
      )) as HandlerResult;

    case 'configure_room':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_room'
      )) as HandlerResult;

    case 'configure_portal':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_portal'
      )) as HandlerResult;

    case 'set_listener_position':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_listener_position'
      )) as HandlerResult;

    case 'get_wwise_event_duration':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_wwise_event_duration'
      )) as HandlerResult;

    case 'create_wwise_trigger':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for create_wwise_trigger'
      )) as HandlerResult;

    case 'set_wwise_game_object':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_wwise_game_object'
      )) as HandlerResult;

    case 'unset_wwise_game_object':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for unset_wwise_game_object'
      )) as HandlerResult;

    case 'post_wwise_trigger':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for post_wwise_trigger'
      )) as HandlerResult;

    case 'set_aux_send':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_aux_send'
      )) as HandlerResult;

    case 'configure_occlusion':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_occlusion'
      )) as HandlerResult;

    case 'set_wwise_project_path':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_wwise_project_path'
      )) as HandlerResult;

    case 'get_wwise_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_wwise_status'
      )) as HandlerResult;

    case 'configure_wwise_init':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_wwise_init'
      )) as HandlerResult;

    case 'restart_wwise_engine':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for restart_wwise_engine'
      )) as HandlerResult;

    // =========================================
    // FMOD (30 actions)
    // =========================================
    case 'connect_fmod_project':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for connect_fmod_project'
      )) as HandlerResult;

    case 'play_fmod_event':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for play_fmod_event'
      )) as HandlerResult;

    case 'play_fmod_event_at_location':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for play_fmod_event_at_location'
      )) as HandlerResult;

    case 'stop_fmod_event':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for stop_fmod_event'
      )) as HandlerResult;

    case 'set_fmod_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_parameter'
      )) as HandlerResult;

    case 'set_fmod_global_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_global_parameter'
      )) as HandlerResult;

    case 'get_fmod_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_fmod_parameter'
      )) as HandlerResult;

    case 'load_fmod_bank':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for load_fmod_bank'
      )) as HandlerResult;

    case 'unload_fmod_bank':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for unload_fmod_bank'
      )) as HandlerResult;

    case 'get_fmod_loaded_banks':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_fmod_loaded_banks'
      )) as HandlerResult;

    case 'create_fmod_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for create_fmod_component'
      )) as HandlerResult;

    case 'configure_fmod_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_fmod_component'
      )) as HandlerResult;

    case 'set_fmod_bus_volume':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_bus_volume'
      )) as HandlerResult;

    case 'set_fmod_bus_paused':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_bus_paused'
      )) as HandlerResult;

    case 'set_fmod_bus_mute':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_bus_mute'
      )) as HandlerResult;

    case 'set_fmod_vca_volume':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_vca_volume'
      )) as HandlerResult;

    case 'apply_fmod_snapshot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for apply_fmod_snapshot'
      )) as HandlerResult;

    case 'release_fmod_snapshot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for release_fmod_snapshot'
      )) as HandlerResult;

    case 'set_fmod_listener_attributes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_listener_attributes'
      )) as HandlerResult;

    case 'get_fmod_event_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_fmod_event_info'
      )) as HandlerResult;

    case 'configure_fmod_occlusion':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_fmod_occlusion'
      )) as HandlerResult;

    case 'configure_fmod_attenuation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_fmod_attenuation'
      )) as HandlerResult;

    case 'set_fmod_studio_path':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_studio_path'
      )) as HandlerResult;

    case 'get_fmod_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_fmod_status'
      )) as HandlerResult;

    case 'configure_fmod_init':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_fmod_init'
      )) as HandlerResult;

    case 'restart_fmod_engine':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for restart_fmod_engine'
      )) as HandlerResult;

    case 'set_fmod_3d_attributes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_fmod_3d_attributes'
      )) as HandlerResult;

    case 'get_fmod_memory_usage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_fmod_memory_usage'
      )) as HandlerResult;

    case 'pause_all_fmod_events':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for pause_all_fmod_events'
      )) as HandlerResult;

    case 'resume_all_fmod_events':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for resume_all_fmod_events'
      )) as HandlerResult;

    // =========================================
    // BINK VIDEO (20 actions)
    // =========================================
    case 'create_bink_media_player':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for create_bink_media_player'
      )) as HandlerResult;

    case 'open_bink_video':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for open_bink_video'
      )) as HandlerResult;

    case 'play_bink':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for play_bink'
      )) as HandlerResult;

    case 'pause_bink':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for pause_bink'
      )) as HandlerResult;

    case 'stop_bink':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for stop_bink'
      )) as HandlerResult;

    case 'seek_bink':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for seek_bink'
      )) as HandlerResult;

    case 'set_bink_looping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_bink_looping'
      )) as HandlerResult;

    case 'set_bink_rate':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_bink_rate'
      )) as HandlerResult;

    case 'set_bink_volume':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_bink_volume'
      )) as HandlerResult;

    case 'get_bink_duration':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_bink_duration'
      )) as HandlerResult;

    case 'get_bink_time':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_bink_time'
      )) as HandlerResult;

    case 'get_bink_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_bink_status'
      )) as HandlerResult;

    case 'create_bink_texture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for create_bink_texture'
      )) as HandlerResult;

    case 'configure_bink_texture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_bink_texture'
      )) as HandlerResult;

    case 'set_bink_texture_player':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for set_bink_texture_player'
      )) as HandlerResult;

    case 'draw_bink_to_texture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for draw_bink_to_texture'
      )) as HandlerResult;

    case 'configure_bink_buffer_mode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_bink_buffer_mode'
      )) as HandlerResult;

    case 'configure_bink_sound_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_bink_sound_track'
      )) as HandlerResult;

    case 'configure_bink_draw_style':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for configure_bink_draw_style'
      )) as HandlerResult;

    case 'get_bink_dimensions':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_bink_dimensions'
      )) as HandlerResult;

    // =========================================
    // UTILITY
    // =========================================
    case 'get_audio_middleware_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_audio_middleware',
        payload,
        'Automation bridge not available for get_audio_middleware_info'
      )) as HandlerResult;

    default:
      return {
        success: false,
        error: `Unknown audio middleware action: ${action}`
      };
  }
}
