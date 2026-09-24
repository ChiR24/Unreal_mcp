import type { CapabilityRecordSource, JsonObject } from '../../index.js';
import { asPostMigration, utilityRecord, withInputProps, withTopics } from '../utility/helpers.js';

const T = 'manage_audio' as const;
const META = ['MetaSound'] as const;
// A MetaSound literal takes the target input's own data type, so it cannot be pinned by field name.
const META_LITERAL = {
  description: 'Value in the data type of the input: a number (Float, Int32, Time, enums), a boolean, a string, or a JSON '
    + 'array of them for an array input such as Float:Array. Converted to the declared type; a mismatch is refused.',
};
const OBJ_ITEM = { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true } as const;
// The generated example cannot express a step list, so the batch carries its own.
const BUILD_METASOUND_EXAMPLE: { readonly input: JsonObject; readonly output: JsonObject } = {
  input: {
    action: 'build_metasound', assetPath: '/Game/Audio/MS_Beep', operations: [
      { edit: 'add_node', id: 'osc', nodeClassName: 'UE.Sine.Audio' },
      { edit: 'set_default', nodeId: '$osc', inputName: 'Frequency', defaultValue: 880 },
    ],
  },
  output: { success: true, message: 'Ran 2 MetaSound operations', nodeIds: { osc: '5E1F0C2A4B7D4E8F9A0B1C2D3E4F5A6B' } },
};

const a = (action: string, summary: string, params: readonly string[], required: readonly string[], outputs: readonly string[] = [], outputRequired: readonly string[] = [], plugins: readonly string[] = [], requiredOneOf?: readonly string[]): CapabilityRecordSource => utilityRecord({
  tool: T, action, family: plugins.length > 0 ? 'metasound' : 'authoring', summary,
  params, required, requiredOneOf, outputs, outputRequired, plugins,
});

export const AUDIO_AUTHORING_RECORDS: readonly CapabilityRecordSource[] = [
  a('add_cue_node', 'Add a node to a Sound Cue graph.', ['assetPath', 'nodeType', 'properties'], ['assetPath', 'nodeType'], ['nodeId'], ['nodeId']),
  withInputProps(a('add_metasound_input', 'Add an input to a MetaSound graph.', ['assetPath', 'inputName', 'inputType', 'defaultValue'], ['assetPath', 'inputName', 'inputType'], [], [], META), { defaultValue: META_LITERAL }),
  a('add_metasound_node', 'Add a node to a MetaSound graph.', ['assetPath', 'nodeClassName', 'nodeType'], ['assetPath'], ['nodeId'], ['nodeId'], META, ['nodeClassName', 'nodeType']),
  a('add_metasound_output', 'Add an output to a MetaSound graph.', ['assetPath', 'outputName', 'outputType'], ['assetPath', 'outputName', 'outputType'], [], [], META),
  withInputProps(a('set_metasound_default', 'Set the default of a MetaSound graph input, or with nodeId the literal on a node input (an oscillator frequency, an envelope time, a note array).', ['assetPath', 'inputName', 'defaultValue'], ['assetPath', 'inputName', 'defaultValue'], [], [], META), {
    defaultValue: META_LITERAL,
    nodeId: { type: 'string', description: 'Set an input on this node (the nodeId add_metasound_node returned) instead of a graph input; inputName then names the node input.' },
  }),
  a('add_mix_modifier', 'Add a Sound Class modifier to a Sound Mix.', ['assetPath', 'soundClassPath', 'volumeAdjuster'], ['assetPath', 'soundClassPath']),
  a('add_source_effect', 'Add an effect to a Source Effect Chain.', ['assetPath', 'effectType', 'properties'], ['assetPath', 'effectType']),
  { ...(asPostMigration(withInputProps(a('build_metasound', 'Run many MetaSound graph edits in one call: add nodes, inputs and outputs, connect pins and set literals, with $id references between steps.', ['assetPath'], ['assetPath', 'operations'], ['nodeIds', 'results'], [], META), {
    operations: {
      type: 'array', items: OBJ_ITEM, 'x-unreal-reflection-boundary': true,
      description: 'Steps run in order, 1-200, stopping at the first failure. Each is {edit, ...the params of that edit}: edit is '
        + 'add_node, connect, set_default, add_input or add_output (the add_metasound_node, connect_metasound_nodes, '
        + 'set_metasound_default, add_metasound_input, add_metasound_output params). Optional per step: id (names the node '
        + 'it creates; later steps use "$id" in nodeId/sourceNodeId/targetNodeId), from/to ("$id.PinName" shorthand for '
        + 'connect; interface nodes such as the On Play input are named with the explicit fields).',
    },
  }), 'Batch front end over the existing MetaSound edits; added after the gateway migration.')), examples: [{ title: 'Build a one-oscillator MetaSound', ...BUILD_METASOUND_EXAMPLE }] },
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
  withTopics(a('create_sound_cue', 'Create a Sound Cue asset and return its asset path.', ['name', 'path', 'wavePath', 'looping'], ['name'], ['assetPath'], ['assetPath']), ['sound cue', 'new sound cue', 'audio cue']),
  a('create_sound_mix', 'Create a Sound Mix asset and return its asset path.', ['name', 'path', 'properties'], ['name'], ['assetPath'], ['assetPath']),
  utilityRecord({
    tool: T, action: 'get_audio_info', family: 'authoring', summary: 'Read metadata for an audio asset.',
    params: ['assetPath'], required: ['assetPath'], effect: 'read',
    outputs: ['assetPath', 'assetClass', 'type', 'duration', 'nodeCount', 'attenuationPath', 'sampleRate',
      'numChannels', 'volume', 'pitch', 'parentClass', 'modifierCount', 'falloffDistance', 'spatialize'],
    outputRequired: ['assetPath', 'assetClass', 'type'],
  }),
  a('set_audio_occlusion', 'Configure occlusion settings on a sound asset.', ['soundPath', 'enable', 'occlusionVolumeScale', 'occlusionFilterScale', 'occlusionInterpolationTime', 'save'], ['soundPath']),
  a('set_class_parent', 'Set a Sound Class parent.', ['assetPath', 'parentClass'], ['assetPath']),
  a('set_class_properties', 'Set Sound Class properties.', ['assetPath', 'properties', 'volume', 'pitch', 'lowPassFilterFrequency', 'save'], ['assetPath']),
  a('set_cue_attenuation', 'Assign attenuation settings to a Sound Cue.', ['assetPath', 'attenuationPath'], ['assetPath']),
  a('set_cue_concurrency', 'Assign concurrency settings to a Sound Cue.', ['assetPath', 'concurrencyPath'], ['assetPath']),
  a('set_dialogue_context', 'Set Dialogue Wave context.', ['assetPath', 'speakerPath'], ['assetPath']),
  a('set_doppler_effect', 'Configure Doppler settings on a sound asset.', ['soundPath', 'dopplerIntensity', 'velocityScale', 'save'], ['soundPath']),
  a('set_sound_attenuation', 'Create or update sound attenuation settings.', ['name', 'path', 'innerRadius', 'falloffDistance', 'attenuationShape', 'falloffMode', 'save'], ['name'], ['assetPath'], ['assetPath']),
];
