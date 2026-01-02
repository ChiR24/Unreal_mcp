/**
 * Audio Authoring Handlers (Phase 11)
 *
 * Complete audio system authoring including:
 * - Sound Cues (expanded graph editing)
 * - MetaSounds
 * - Sound Classes & Mixes
 * - Attenuation & Spatialization
 * - Dialogue System
 * - Audio Effects
 *
 * @module audio-authoring-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all audio authoring actions for the manage_audio_authoring tool.
 */
export async function handleAudioAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<Record<string, unknown>> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_audio_authoring',
      payload as HandlerArgs,
      `Automation bridge not available for audio authoring action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // =========================================================================
    // 11.1 Sound Cues (5 actions)
    // =========================================================================

    case 'create_sound_cue': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_sound_cue');
    }

    case 'add_cue_node': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.nodeType, 'nodeType', 'Missing required parameter: nodeType');
      return sendRequest('add_cue_node');
    }

    case 'connect_cue_nodes': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.sourceNodeId, 'sourceNodeId', 'Missing required parameter: sourceNodeId');
      requireNonEmptyString(argsRecord.targetNodeId, 'targetNodeId', 'Missing required parameter: targetNodeId');
      return sendRequest('connect_cue_nodes');
    }

    case 'set_cue_attenuation': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('set_cue_attenuation');
    }

    case 'set_cue_concurrency': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('set_cue_concurrency');
    }

    // =========================================================================
    // 11.2 MetaSounds (6 actions)
    // =========================================================================

    case 'create_metasound': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_metasound');
    }

    case 'add_metasound_node': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.nodeType, 'nodeType', 'Missing required parameter: nodeType');
      return sendRequest('add_metasound_node');
    }

    case 'connect_metasound_nodes': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.sourceNode, 'sourceNode', 'Missing required parameter: sourceNode');
      requireNonEmptyString(argsRecord.sourcePin, 'sourcePin', 'Missing required parameter: sourcePin');
      requireNonEmptyString(argsRecord.targetNode, 'targetNode', 'Missing required parameter: targetNode');
      requireNonEmptyString(argsRecord.targetPin, 'targetPin', 'Missing required parameter: targetPin');
      return sendRequest('connect_metasound_nodes');
    }

    case 'add_metasound_input': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.inputName, 'inputName', 'Missing required parameter: inputName');
      requireNonEmptyString(argsRecord.inputType, 'inputType', 'Missing required parameter: inputType');
      return sendRequest('add_metasound_input');
    }

    case 'add_metasound_output': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.outputName, 'outputName', 'Missing required parameter: outputName');
      requireNonEmptyString(argsRecord.outputType, 'outputType', 'Missing required parameter: outputType');
      return sendRequest('add_metasound_output');
    }

    case 'set_metasound_default': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.inputName, 'inputName', 'Missing required parameter: inputName');
      return sendRequest('set_metasound_default');
    }

    // =========================================================================
    // 11.3 Sound Classes & Mixes (6 actions)
    // =========================================================================

    case 'create_sound_class': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_sound_class');
    }

    case 'set_class_properties': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('set_class_properties');
    }

    case 'set_class_parent': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('set_class_parent');
    }

    case 'create_sound_mix': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_sound_mix');
    }

    case 'add_mix_modifier': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.soundClassPath, 'soundClassPath', 'Missing required parameter: soundClassPath');
      return sendRequest('add_mix_modifier');
    }

    case 'configure_mix_eq': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('configure_mix_eq');
    }

    // =========================================================================
    // 11.4 Attenuation & Spatialization (5 actions)
    // =========================================================================

    case 'create_attenuation_settings': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_attenuation_settings');
    }

    case 'configure_distance_attenuation': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('configure_distance_attenuation');
    }

    case 'configure_spatialization': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('configure_spatialization');
    }

    case 'configure_occlusion': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('configure_occlusion');
    }

    case 'configure_reverb_send': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('configure_reverb_send');
    }

    // =========================================================================
    // 11.5 Dialogue System (3 actions)
    // =========================================================================

    case 'create_dialogue_voice': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_dialogue_voice');
    }

    case 'create_dialogue_wave': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_dialogue_wave');
    }

    case 'set_dialogue_context': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('set_dialogue_context');
    }

    // =========================================================================
    // 11.6 Effects (4 actions)
    // =========================================================================

    case 'create_reverb_effect': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_reverb_effect');
    }

    case 'create_source_effect_chain': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_source_effect_chain');
    }

    case 'add_source_effect': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      requireNonEmptyString(argsRecord.effectType, 'effectType', 'Missing required parameter: effectType');
      return sendRequest('add_source_effect');
    }

    case 'create_submix_effect': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.effectType, 'effectType', 'Missing required parameter: effectType');
      return sendRequest('create_submix_effect');
    }

    // =========================================================================
    // Utility (1 action)
    // =========================================================================

    case 'get_audio_info': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('get_audio_info');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown audio authoring action: ${action}`
      });
  }
}
