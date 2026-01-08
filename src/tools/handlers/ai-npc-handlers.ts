/**
 * Phase 42: AI & NPC Plugins Handlers
 * Handles Convai, Inworld AI, NVIDIA ACE (Audio2Face).
 * ~30 actions across 3 AI NPC subsystems for conversational AI characters.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for AI NPC Plugins tools
 */
export async function handleAINPCTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown> {
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
    // CONVAI - Conversational AI (10 actions)
    // =========================================
    case 'create_convai_character':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for create_convai_character'
      ));

    case 'configure_character_backstory':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_character_backstory'
      ));

    case 'configure_character_voice':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_character_voice'
      ));

    case 'configure_convai_lipsync':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_convai_lipsync'
      ));

    case 'start_convai_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for start_convai_session'
      ));

    case 'stop_convai_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for stop_convai_session'
      ));

    case 'send_text_to_character':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for send_text_to_character'
      ));

    case 'get_character_response':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for get_character_response'
      ));

    case 'configure_convai_actions':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_convai_actions'
      ));

    case 'get_convai_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for get_convai_info'
      ));

    // =========================================
    // INWORLD AI - AI Character Experiences (10 actions)
    // =========================================
    case 'create_inworld_character':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for create_inworld_character'
      ));

    case 'configure_inworld_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_inworld_settings'
      ));

    case 'configure_inworld_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_inworld_scene'
      ));

    case 'start_inworld_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for start_inworld_session'
      ));

    case 'stop_inworld_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for stop_inworld_session'
      ));

    case 'send_message_to_character':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for send_message_to_character'
      ));

    case 'get_character_emotion':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for get_character_emotion'
      ));

    case 'get_character_goals':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for get_character_goals'
      ));

    case 'trigger_inworld_event':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for trigger_inworld_event'
      ));

    case 'get_inworld_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for get_inworld_info'
      ));

    // =========================================
    // NVIDIA ACE / Audio2Face (8 actions)
    // =========================================
    case 'configure_audio2face':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_audio2face'
      ));

    case 'process_audio_to_blendshapes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for process_audio_to_blendshapes'
      ));

    case 'configure_blendshape_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_blendshape_mapping'
      ));

    case 'start_audio2face_stream':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for start_audio2face_stream'
      ));

    case 'stop_audio2face_stream':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for stop_audio2face_stream'
      ));

    case 'get_audio2face_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for get_audio2face_status'
      ));

    case 'configure_ace_emotions':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for configure_ace_emotions'
      ));

    case 'get_ace_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for get_ace_info'
      ));

    // =========================================
    // UTILITIES (2 actions)
    // =========================================
    case 'get_ai_npc_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for get_ai_npc_info'
      ));

    case 'list_available_ai_backends':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_ai_npc',
        payload,
        'Automation bridge not available for list_available_ai_backends'
      ));

    default:
      // Consistent error handling pattern: return error object instead of throwing
      return {
        success: false,
        error: `Unknown manage_ai_npc action: ${action}`,
        availableActions: [
          'create_convai_character', 'configure_character_backstory', 'configure_character_voice',
          'configure_convai_lipsync', 'start_convai_session', 'stop_convai_session',
          'send_convai_message', 'get_convai_response', 'configure_convai_emotions', 'get_convai_status',
          'create_inworld_character', 'configure_inworld_personality', 'configure_inworld_voice',
          'start_inworld_session', 'stop_inworld_session', 'send_inworld_message', 'get_inworld_response',
          'configure_inworld_emotions', 'configure_inworld_goals', 'get_inworld_status',
          'configure_audio2face', 'process_audio_to_blendshapes', 'configure_blendshape_mapping',
          'start_audio2face_stream', 'stop_audio2face_stream', 'get_audio2face_status',
          'configure_ace_emotions', 'get_ai_npc_info', 'list_available_ai_backends'
        ]
      };
  }
}
