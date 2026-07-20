import type { CapabilityRecordSource } from '../../index.js';
import { utilityRecord } from '../utility/helpers.js';

const T = 'manage_audio' as const;
const META = ['MetaSound'] as const;
const a = (action: string, summary: string, params: readonly string[], required: readonly string[], outputs: readonly string[] = [], outputRequired: readonly string[] = [], plugins: readonly string[] = []): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: plugins.length > 0 ? 'metasound' : 'authoring', summary,
  params, required, outputs, outputRequired, plugins,
});

export const AUDIO_AUTHORING_RECORDS: readonly CapabilityRecordSource[] = [
  a('add_cue_node', 'Add a node to a Sound Cue graph.', ['assetPath', 'nodeType', 'properties'], ['assetPath', 'nodeType'], ['nodeId'], ['nodeId']),
  a('add_metasound_input', 'Add an input to a MetaSound graph.', ['assetPath', 'inputName', 'inputType', 'defaultValue'], ['assetPath', 'inputName', 'inputType'], [], [], META),
  a('add_metasound_node', 'Add a node to a MetaSound graph.', ['assetPath', 'nodeType'], ['assetPath', 'nodeType'], ['nodeId'], ['nodeId'], META),
  a('add_metasound_output', 'Add an output to a MetaSound graph.', ['assetPath', 'outputName', 'outputType'], ['assetPath', 'outputName', 'outputType'], [], [], META),
  a('set_metasound_default', 'Set a MetaSound input default value.', ['assetPath', 'inputName', 'defaultValue'], ['assetPath', 'inputName'], [], [], META),
  a('add_mix_modifier', 'Add a Sound Class modifier to a Sound Mix.', ['assetPath', 'soundClassPath', 'volumeAdjuster'], ['assetPath', 'soundClassPath']),
  a('add_source_effect', 'Add an effect to a Source Effect Chain.', ['assetPath', 'effectType', 'properties'], ['assetPath', 'effectType']),
  a('configure_distance_attenuation', 'Configure distance attenuation.', ['assetPath', 'innerRadius', 'falloffDistance'], ['assetPath']),
  a('configure_mix_eq', 'Configure Sound Mix equalization.', ['assetPath', 'properties'], ['assetPath']),
  a('configure_occlusion', 'Configure audio occlusion.', ['assetPath', 'enable', 'occlusionVolumeScale', 'occlusionFilterScale'], ['assetPath']),
  a('configure_reverb_send', 'Configure an audio reverb send.', ['assetPath', 'enableReverbSend', 'reverbDistanceMin', 'reverbDistanceMax', 'reverbWetLevelMin', 'reverbWetLevelMax'], ['assetPath']),
  a('configure_spatialization', 'Configure audio spatialization.', ['assetPath', 'spatialization'], ['assetPath']),
  a('connect_cue_nodes', 'Connect two Sound Cue graph nodes.', ['assetPath', 'sourceNodeId', 'targetNodeId'], ['assetPath', 'sourceNodeId', 'targetNodeId']),
  a('connect_metasound_nodes', 'Connect two MetaSound graph pins.', ['assetPath', 'sourceNodeId', 'sourceOutputName', 'targetNodeId', 'targetInputName', 'sourceNode', 'sourcePin', 'targetNode', 'targetPin'], ['assetPath', 'sourceNodeId', 'sourceOutputName', 'targetNodeId', 'targetInputName'], [], [], META),
  a('create_attenuation_settings', 'Create attenuation settings and return the asset path.', ['name', 'path'], ['name'], ['assetPath'], ['assetPath']),
  a('create_dialogue_voice', 'Create a Dialogue Voice asset and return its path.', ['name', 'path'], ['name'], ['assetPath'], ['assetPath']),
  a('create_dialogue_wave', 'Create a Dialogue Wave asset and return its path.', ['name', 'path', 'wavePath', 'speakerPath'], ['name'], ['assetPath'], ['assetPath']),
  a('create_metasound', 'Create a MetaSound asset and return its asset path.', ['name', 'path'], ['name'], ['assetPath'], ['assetPath'], META),
  a('create_reverb_effect', 'Create a Reverb Effect asset and return its path.', ['name', 'path', 'properties'], ['name'], ['assetPath'], ['assetPath']),
  a('create_source_effect_chain', 'Create a Source Effect Chain asset and return its path.', ['name', 'path'], ['name'], ['assetPath'], ['assetPath']),
  a('create_submix_effect', 'Create a Submix Effect asset and return its path.', ['name', 'path', 'effectType'], ['name', 'effectType'], ['assetPath'], ['assetPath']),
  a('create_sound_class', 'Create a Sound Class asset and return its asset path.', ['name', 'path', 'parentClass', 'properties'], ['name'], ['assetPath'], ['assetPath']),
  a('create_sound_cue', 'Create a Sound Cue asset and return its asset path.', ['name', 'path', 'wavePath', 'looping'], ['name'], ['assetPath'], ['assetPath']),
  a('create_sound_mix', 'Create a Sound Mix asset and return its asset path.', ['name', 'path', 'properties'], ['name'], ['assetPath'], ['assetPath']),
  a('get_audio_info', 'Read metadata for an audio asset.', ['assetPath'], ['assetPath'], ['audioInfo'], ['audioInfo']),
  a('set_audio_occlusion', 'Configure occlusion settings on a sound asset.', ['soundPath', 'enable', 'occlusionVolumeScale', 'occlusionFilterScale', 'occlusionInterpolationTime', 'save'], ['soundPath']),
  a('set_class_parent', 'Set a Sound Class parent.', ['assetPath', 'parentClass'], ['assetPath']),
  a('set_class_properties', 'Set Sound Class properties.', ['assetPath', 'properties', 'volume', 'pitch', 'lowPassFilterFrequency', 'save'], ['assetPath']),
  a('set_cue_attenuation', 'Assign attenuation settings to a Sound Cue.', ['assetPath', 'attenuationPath'], ['assetPath']),
  a('set_cue_concurrency', 'Assign concurrency settings to a Sound Cue.', ['assetPath', 'concurrencyPath'], ['assetPath']),
  a('set_dialogue_context', 'Set Dialogue Wave context.', ['assetPath', 'speakerPath'], ['assetPath']),
  a('set_doppler_effect', 'Configure Doppler settings on a sound asset.', ['soundPath', 'dopplerIntensity', 'velocityScale', 'save'], ['soundPath']),
  a('set_sound_attenuation', 'Create or update sound attenuation settings.', ['name', 'path', 'innerRadius', 'falloffDistance', 'attenuationShape', 'falloffMode', 'save'], ['name'], ['assetPath'], ['assetPath']),
];
