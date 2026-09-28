import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { utilityRecord, withInputProps, withTopics } from '../utility/utility-record-builders.js';

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
const SINE_ID = '5E1F0C2A4B7D4E8F9A0B1C2D3E4F5A6B';
const OUT_ID = '0A1B2C3D4E5F60718293A4B5C6D7E8F9';
const GET_METASOUND_GRAPH_EXAMPLE: { readonly input: JsonObject; readonly output: JsonObject } = {
  input: { action: 'get_metasound_graph', assetPath: '/Game/Audio/MS_Beep' },
  output: {
    success: true, assetPath: '/Game/Audio/MS_Beep.MS_Beep', nodeCount: 2, edgeCount: 1,
    nodes: [
      { nodeId: SINE_ID, name: 'Sine', className: 'UE.Sine.Audio', kind: 'Node', inputs: [{ name: 'Frequency', type: 'Float', literal: '880.000000' }], outputs: [{ name: 'Audio', type: 'Audio' }] },
      { nodeId: OUT_ID, name: 'Out Mono', className: 'Out Mono', kind: 'GraphOutput', inputs: [{ name: 'Out Mono', type: 'Audio' }], outputs: [] },
    ],
    edges: [{ fromNodeId: SINE_ID, fromNode: 'Sine', fromPin: 'Audio', toNodeId: OUT_ID, toNode: 'Out Mono', toPin: 'Out Mono' }],
    graphInputs: [], graphOutputs: [{ name: 'Out Mono', type: 'Audio', nodeId: OUT_ID }],
  },
};
// Every audio asset edit saves unless told not to; manage_networking shares the bare `save` pin with the opposite default.
const SAVE = { type: 'boolean', description: 'Persist the asset to disk (default true); false leaves the change in memory only.' } as const;
const PARENT_CLASS = { type: 'string', description: 'Canonical /Game SoundClass asset path of the parent class.' } as const;

const a = (action: string, summary: string, params: readonly string[], required: readonly string[], outputs: readonly string[] = [], outputRequired: readonly string[] = [], plugins: readonly string[] = [], requiredOneOf?: readonly string[]): CapabilityRecordSource => {
  const record = utilityRecord({
    tool: T, action, family: plugins.length > 0 ? 'metasound' : 'authoring', summary,
    params, required, requiredOneOf, outputs, outputRequired, plugins,
  });
  return params.includes('save') ? withInputProps(record, { save: SAVE }) : record;
};

export const AUDIO_AUTHORING_RECORDS: readonly CapabilityRecordSource[] = [
  a('add_cue_node', 'Add a node to a Sound Cue graph: wave_player (wavePath), mixer, random, modulator (volume, pitch), looping (indefinite, loopCount), attenuation (attenuationPath), concatenator, delay (delay), switch or branch.',
    ['assetPath', 'nodeType', 'wavePath', 'volume', 'pitch', 'indefinite', 'loopCount', 'attenuationPath', 'delay', 'save'], ['assetPath', 'nodeType'], ['nodeId'], ['nodeId']),
  withInputProps(a('add_metasound_input', 'Add an input to a MetaSound graph.', ['assetPath', 'inputName', 'inputType', 'defaultValue', 'save'], ['assetPath', 'inputName', 'inputType'], [], [], META), { defaultValue: META_LITERAL }),
  a('add_metasound_node', 'Add a node to a MetaSound graph.', ['assetPath', 'nodeClassName', 'nodeType', 'save'], ['assetPath'], ['nodeId'], ['nodeId'], META, ['nodeClassName', 'nodeType']),
  a('add_metasound_output', 'Add an output to a MetaSound graph.', ['assetPath', 'outputName', 'outputType', 'save'], ['assetPath', 'outputName', 'outputType'], [], [], META),
  withInputProps(a('set_metasound_default', 'Set the default of a MetaSound graph input, or with nodeId the literal on a node input (an oscillator frequency, an envelope time, a note array).', ['assetPath', 'inputName', 'defaultValue', 'save'], ['assetPath', 'inputName', 'defaultValue'], [], [], META), {
    defaultValue: META_LITERAL,
    nodeId: { type: 'string', description: 'Set an input on this node (the nodeId add_metasound_node returned) instead of a graph input; inputName then names the node input.' },
  }),
  a('add_mix_modifier', 'Add a Sound Class modifier to a Sound Mix; fadeInTime and fadeOutTime set the fades of the mix itself.',
    ['assetPath', 'soundClassPath', 'volumeAdjuster', 'pitchAdjuster', 'applyToChildren', 'fadeInTime', 'fadeOutTime', 'save'], ['assetPath', 'soundClassPath']),
  a('add_source_effect', 'Add an effect to a Source Effect Chain: a new preset of effectType, or an existing preset asset by effectPresetPath.',
    ['assetPath', 'effectType', 'effectPresetPath', 'bypass', 'save'], ['assetPath'], [], [], [], ['effectType', 'effectPresetPath']),
  { ...(withInputProps(a('build_metasound', 'Run many MetaSound graph edits in one call: add nodes, inputs and outputs, connect pins and set literals, with $id references between steps.', ['assetPath'], ['assetPath', 'operations'], ['nodeIds', 'results'], [], META), {
    operations: {
      type: 'array', items: OBJ_ITEM, 'x-unreal-reflection-boundary': true,
      description: 'Steps run in order, 1-200, stopping at the first failure. Each is {edit, ...the params of that edit}: edit is '
        + 'add_node, connect, disconnect, remove_node, set_default, add_input or add_output (the add_metasound_node, connect_metasound_nodes, '
        + 'disconnect_metasound_nodes, remove_metasound_node, set_metasound_default, add_metasound_input, add_metasound_output params). Optional per step: id (names the node '
        + 'it creates; later steps use "$id" in nodeId/nodeIds/sourceNodeId/targetNodeId), from/to ("$id.PinName" shorthand for '
        + 'connect; interface nodes such as the On Play input are named with the explicit fields).',
    },
    save: { type: 'boolean', description: 'Save the MetaSound after each step (default true); false leaves every edit in memory only.' },
  })), examples: [{ title: 'Build a one-oscillator MetaSound', ...BUILD_METASOUND_EXAMPLE }] },
  a('configure_distance_attenuation', 'Configure distance attenuation.', ['assetPath', 'innerRadius', 'falloffDistance', 'distanceAlgorithm', 'save'], ['assetPath']),
  a('configure_mix_eq', 'Configure Sound Mix equalization: the four bands through eqSettings, or the low, mid, highMid and high fields.',
    ['assetPath', 'applyEQ', 'eqPriority', 'eqSettings', 'lowFrequency', 'lowGain', 'midFrequency', 'midGain', 'highMidFrequency', 'highMidGain', 'highFrequency', 'highGain', 'save'], ['assetPath']),
  a('configure_occlusion', 'Configure audio occlusion.', ['assetPath', 'enable', 'occlusionVolumeScale', 'occlusionFilterScale', 'occlusionInterpolationTime', 'save'], ['assetPath']),
  a('configure_reverb_send', 'Configure an audio reverb send.', ['assetPath', 'enableReverbSend', 'reverbDistanceMin', 'reverbDistanceMax', 'reverbWetLevelMin', 'reverbWetLevelMax', 'save'], ['assetPath']),
  a('configure_spatialization', 'Configure audio spatialization.', ['assetPath', 'spatialize', 'spatialization', 'save'], ['assetPath']),
  a('connect_cue_nodes', 'Connect two Sound Cue graph nodes.', ['assetPath', 'sourceNodeId', 'targetNodeId', 'childIndex', 'save'], ['assetPath', 'sourceNodeId', 'targetNodeId']),
  withInputProps(a('remove_metasound_node', 'Remove one node, or several with nodeIds, and every link on them from a MetaSound graph; graph inputs and outputs (On Play, Out Mono) are refused. All or nothing.', ['assetPath', 'nodeId', 'nodeIds', 'save'], ['assetPath'], ['removed', 'removedCount', 'saved'], ['removed'], META, ['nodeId', 'nodeIds']), {
    nodeIds: { type: 'array', items: { type: 'string' }, minItems: 1, maxItems: 200, description: 'Several node ids to remove in one call, in place of nodeId; every one must exist or nothing is removed.' },
  }),
  a('disconnect_metasound_nodes', 'Remove a link from a MetaSound graph: the one into targetNodeId.targetInputName, or every link out of sourceNodeId.sourceOutputName; with both ends given, that exact link. Refused when there is no such link.', ['assetPath', 'sourceNodeId', 'sourceOutputName', 'targetNodeId', 'targetInputName', 'save'], ['assetPath'], ['edgesRemoved', 'saved'], ['edgesRemoved'], META, ['targetNodeId', 'sourceNodeId']),
  { ...withTopics(utilityRecord({
    tool: T, action: 'get_metasound_graph', family: 'metasound', summary: 'Read a MetaSound graph: every node with its input literals, the links by pin name, and the graph inputs and outputs.',
    params: ['assetPath'], required: ['assetPath'], effect: 'read', plugins: META,
    outputs: ['assetPath', 'nodeCount', 'edgeCount', 'nodes', 'edges', 'graphInputs', 'graphOutputs'], outputRequired: ['nodes', 'edges'],
  }), ['read metasound graph', 'metasound nodes', 'list metasound nodes', 'inspect metasound', 'metasound links']), examples: [{ title: 'Read a one-oscillator MetaSound', ...GET_METASOUND_GRAPH_EXAMPLE }] },
  a('connect_metasound_nodes', 'Connect two MetaSound graph pins.', ['assetPath', 'sourceNodeId', 'sourceOutputName', 'targetNodeId', 'targetInputName', 'save'], ['assetPath', 'sourceNodeId', 'sourceOutputName', 'targetNodeId', 'targetInputName'], [], [], META),
  a('create_attenuation_settings', 'Create attenuation settings and return the asset path.', ['name', 'path', 'innerRadius', 'falloffDistance', 'save'], ['name'], ['assetPath'], ['assetPath']),
  a('create_dialogue_voice', 'Create a Dialogue Voice asset and return its path.', ['name', 'path', 'gender', 'plurality', 'save'], ['name'], ['assetPath'], ['assetPath']),
  a('create_dialogue_wave', 'Create a Dialogue Wave asset and return its path; wavePath or speakerPath adds its first context mapping.', ['name', 'path', 'spokenText', 'wavePath', 'speakerPath', 'save'], ['name'], ['assetPath'], ['assetPath']),
  a('create_metasound', 'Create a MetaSound asset and return its asset path.', ['name', 'path', 'save'], ['name'], ['assetPath'], ['assetPath'], META),
  a('create_reverb_effect', 'Create a Reverb Effect asset and return its path.', ['name', 'path', 'density', 'diffusion', 'gain', 'gainHF', 'decayTime', 'decayHFRatio', 'save'], ['name'], ['assetPath'], ['assetPath']),
  a('create_source_effect_chain', 'Create a Source Effect Chain asset and return its path.', ['name', 'path', 'save'], ['name'], ['assetPath'], ['assetPath']),
  withInputProps(a('create_submix_effect', 'Create a Sound Submix whose effect chain holds a new submix effect preset of effectType, and return its path.', ['name', 'path', 'effectType', 'save'], ['name', 'effectType'], ['assetPath'], ['assetPath']), {
    effectType: { type: 'string', description: 'Submix effect: Reverb, EQ or Dynamics (a compressor).' },
  }),
  withInputProps(a('create_sound_class', 'Create a Sound Class asset and return its asset path.', ['name', 'path', 'parentClass', 'volume', 'pitch', 'save'], ['name'], ['assetPath'], ['assetPath']), { parentClass: PARENT_CLASS }),
  withTopics(a('create_sound_cue', 'Create a Sound Cue asset and return its asset path; volume and pitch add a modulator over the wave player.', ['name', 'path', 'wavePath', 'looping', 'volume', 'pitch', 'save'], ['name'], ['assetPath'], ['assetPath']), ['sound cue', 'new sound cue', 'audio cue']),
  a('create_sound_mix', 'Create a Sound Mix asset and return its asset path.', ['name', 'path', 'save'], ['name'], ['assetPath'], ['assetPath']),
  utilityRecord({
    tool: T, action: 'get_audio_info', family: 'authoring', summary: 'Read metadata for an audio asset.',
    params: ['assetPath'], required: ['assetPath'], effect: 'read',
    outputs: ['assetPath', 'assetClass', 'type', 'duration', 'nodeCount', 'attenuationPath', 'sampleRate',
      'numChannels', 'volume', 'pitch', 'parentClass', 'modifierCount', 'falloffDistance', 'spatialize'],
    outputRequired: ['assetPath', 'assetClass', 'type'],
  }),
  a('set_audio_occlusion', 'Configure occlusion settings on a sound asset.', ['soundPath', 'enable', 'occlusionVolumeScale', 'occlusionFilterScale', 'occlusionInterpolationTime', 'save'], ['soundPath']),
  withInputProps(a('set_class_parent', 'Set a Sound Class parent.', ['assetPath', 'parentClass', 'save'], ['assetPath']), {
    parentClass: { type: 'string', description: 'Canonical /Game SoundClass asset path of the new parent; omit it to clear the parent.' },
  }),
  a('set_class_properties', 'Set Sound Class properties.', ['assetPath', 'volume', 'pitch', 'lowPassFilterFrequency', 'lfeBleed', 'voiceCenterChannelVolume', 'save'], ['assetPath']),
  a('set_cue_attenuation', 'Assign attenuation settings to a Sound Cue; omit attenuationPath to clear them.', ['assetPath', 'attenuationPath', 'save'], ['assetPath']),
  withTopics(a('set_doppler_effect', 'Add a Doppler node at the root of a Sound Cue (or update the one already there) so the cue shifts pitch as its source moves toward or away from the listener. Sound Cues only; a SoundWave or MetaSound is refused.',
    ['assetPath', 'dopplerIntensity', 'smoothing', 'save'], ['assetPath'], ['nodeName', 'inserted', 'dopplerIntensity', 'smoothing', 'rootNodeClass', 'drivesNode', 'saved'], ['inserted']),
    ['doppler effect', 'pitch shift moving sound']),
  a('set_cue_concurrency', 'Assign concurrency settings to a Sound Cue.', ['assetPath', 'concurrencyPath', 'save'], ['assetPath']),
  a('set_dialogue_context', 'Add a context mapping to a Dialogue Wave: who speaks it, to whom, and which SoundWave plays.', ['assetPath', 'speakerPath', 'targetVoices', 'soundWavePath', 'localizationKeyFormat', 'replace', 'save'], ['assetPath']),
  a('set_sound_attenuation', 'Create or update sound attenuation settings.', ['name', 'path', 'innerRadius', 'falloffDistance', 'attenuationShape', 'falloffMode', 'save'], ['name'], ['assetPath'], ['assetPath']),
];
