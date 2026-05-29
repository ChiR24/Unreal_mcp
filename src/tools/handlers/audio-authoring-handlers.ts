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
import { requireNonEmptyString, executeAutomationRequest, getTimeoutMs, normalizePathFields } from './common-handlers.js';

/**
 * Normalize an asset path for StaticLoadObject which requires the full
 * UE object path format: /Game/Path/AssetName.AssetName
 * If the path already contains a dot (e.g., /Game/X/Y.Y), return as-is.
 * Otherwise, append .AssetName where AssetName is the last path segment.
 */
function normalizeAssetPathForLoadObject(assetPath: string): string {
  if (!assetPath || !assetPath.startsWith('/')) return assetPath;
  // If already has the .ObjectName suffix, return as-is
  const lastSegment = assetPath.split('/').pop() ?? '';
  if (lastSegment.includes('.')) return assetPath;
  // Append .AssetName
  return `${assetPath}.${lastSegment}`;
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
    const payload: Record<string, unknown> = normalizePathFields({ ...argsRecord, subAction }, [
      'assetPath',
      'attenuationPath',
      'soundClassPath',
      'speakerPath',
      'reverbEffect',
      'effectPresetPath',
      'parentPath',
      'path'
    ]);

    // Normalize asset paths for StaticLoadObject compatibility (needs /Game/Path/Asset.AssetName)
    if (payload.assetPath && typeof payload.assetPath === 'string') {
      payload.assetPath = normalizeAssetPathForLoadObject(payload.assetPath);
    }
    if (payload.attenuationPath && typeof payload.attenuationPath === 'string') {
      payload.attenuationPath = normalizeAssetPathForLoadObject(payload.attenuationPath);
    }
    if (payload.soundClassPath && typeof payload.soundClassPath === 'string') {
      payload.soundClassPath = normalizeAssetPathForLoadObject(payload.soundClassPath);
    }
  if (payload.speakerPath && typeof payload.speakerPath === 'string') {
    payload.speakerPath = normalizeAssetPathForLoadObject(payload.speakerPath);
  }
  if (payload.reverbEffect && typeof payload.reverbEffect === 'string') {
    payload.reverbEffect = normalizeAssetPathForLoadObject(payload.reverbEffect);
  }

  // Parameter remapping: TS API names → C++ field names
  // connect_metasound_nodes: C++ reads sourceNodeId/sourceOutputName/targetNodeId/targetInputName
  if (subAction === 'connect_metasound_nodes') {
    if (payload.sourceNode && !payload.sourceNodeId) {
      payload.sourceNodeId = payload.sourceNode;
      delete payload.sourceNode;
    }
    if (payload.sourcePin && !payload.sourceOutputName) {
      payload.sourceOutputName = payload.sourcePin;
      delete payload.sourcePin;
    }
    if (payload.targetNode && !payload.targetNodeId) {
      payload.targetNodeId = payload.targetNode;
      delete payload.targetNode;
    }
    if (payload.targetPin && !payload.targetInputName) {
      payload.targetInputName = payload.targetPin;
      delete payload.targetPin;
    }
  }

	// set_metasound_default: C++ reads floatValue/intValue/boolValue/stringValue
	if (subAction === 'set_metasound_default') {
		if (payload.defaultValue !== undefined && payload.floatValue === undefined) {
			payload.floatValue = payload.defaultValue;
			delete payload.defaultValue;
		}
	}

	// add_source_effect: C++ authoring handler reads assetPath + effectPresetPath + effectType
	// (Note: main C++ handler reads chainPath — but authoring route reads assetPath)
	if (subAction === 'add_source_effect') {
		if (payload.effectPresetPath && typeof payload.effectPresetPath === 'string') {
			payload.effectPresetPath = normalizeAssetPathForLoadObject(payload.effectPresetPath as string);
		}
	}

	// set_class_parent: C++ reads parentPath (not parentClass)
	if (subAction === 'set_class_parent') {
		if (payload.parentClass && !payload.parentPath) {
			payload.parentPath = payload.parentClass;
			delete payload.parentClass;
		}
	}

	// configure_spatialization: C++ reads spatialize (bool) + spatializationAlgorithm
	if (subAction === 'configure_spatialization') {
		if (payload.spatialization !== undefined) {
			if (typeof payload.spatialization === 'string') {
				payload.spatializationAlgorithm = payload.spatialization;
				if (payload.spatialize === undefined) {
					payload.spatialize = true;
				}
			} else if (payload.spatialize === undefined) {
				payload.spatialize = payload.spatialization;
			}
			delete payload.spatialization;
		}
	}

	// configure_occlusion: C++ reads enableOcclusion (not enable)
	if (subAction === 'configure_occlusion') {
		if (payload.enable !== undefined && payload.enableOcclusion === undefined) {
			payload.enableOcclusion = payload.enable;
			delete payload.enable;
		}
	}

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
      requireNonEmptyString(argsRecord.sourceNodeId ?? argsRecord.sourceNode, 'sourceNodeId', 'Missing required parameter: sourceNodeId');
      requireNonEmptyString(argsRecord.sourceOutputName ?? argsRecord.sourcePin, 'sourceOutputName', 'Missing required parameter: sourceOutputName');
      requireNonEmptyString(argsRecord.targetNodeId ?? argsRecord.targetNode, 'targetNodeId', 'Missing required parameter: targetNodeId');
      requireNonEmptyString(argsRecord.targetInputName ?? argsRecord.targetPin, 'targetInputName', 'Missing required parameter: targetInputName');
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
