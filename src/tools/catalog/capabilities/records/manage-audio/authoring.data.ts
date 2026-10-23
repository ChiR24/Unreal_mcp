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
// innerRadius gives a box one size on all three axes, and a hall or corridor zone is no cube.
const BOX_EXTENT = {
  type: 'object',
  description: 'A Box shape\'s half-size on each axis in cm, {x, y, z}, each above 0: a zone the length of a hall without '
    + 'reaching as far sideways. It replaces the size innerRadius gives a box; another shape is refused.',
  properties: {
    x: { type: 'number', description: 'Half-size along X, cm.' },
    y: { type: 'number', description: 'Half-size along Y, cm.' },
    z: { type: 'number', description: 'Half-size along Z, cm.' },
  },
  required: ['x', 'y', 'z'],
  additionalProperties: false,
} as const;

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
  a('add_mix_modifier', 'Add a Sound Class modifier to a Sound Mix, or edit the one the mix already has for that class (replaced true), keeping what is not sent: volumeAdjuster and pitchAdjuster (multipliers; a new modifier starts at 1) and applyToChildren (default true). fadeInTime and fadeOutTime set the fades of the mix itself. Saved unless save is false.',
    ['assetPath', 'soundClassPath', 'volumeAdjuster', 'pitchAdjuster', 'applyToChildren', 'fadeInTime', 'fadeOutTime', 'save'], ['assetPath', 'soundClassPath']),
  a('add_source_effect', 'Add an effect to a Source Effect Chain: a new preset of effectType (eq, chorus, delay, filter, dynamics, bitcrusher, phaser, waveshaper, panner, stereodelay, foldback, ringmodulation, midsidespreader, motionfilter, envelopefollower or convolutionreverb), saved as an asset of its own beside the chain and listed in the receipt, or an existing preset asset by effectPresetPath. An unknown effectType or a preset that does not load is refused.',
    ['assetPath', 'effectType', 'effectPresetPath', 'bypass', 'save'], ['assetPath'], [], [], [], ['effectType', 'effectPresetPath']),
  { ...(withInputProps(a('build_metasound', 'Run many MetaSound graph edits in one call: add nodes, inputs and outputs, connect pins and set literals, with $id references between steps.', ['assetPath'], ['assetPath', 'operations'], ['nodeIds', 'results', 'saved'], [], META), {
    operations: {
      type: 'array', items: OBJ_ITEM, 'x-unreal-reflection-boundary': true,
      description: 'Steps run in order, 1-200, stopping at the first failure; every add_node class is looked up first, so a node '
        + 'class the registry does not hold fails the batch with nothing applied, and so is every pin a connect or set_default names '
        + 'on a node the batch adds (a wrong one fails with PIN_NOT_FOUND and that node\'s pins). Each is {edit, ...the params of that edit}: edit is '
        + 'add_node, connect, disconnect, remove_node, set_default, add_input or add_output (the add_metasound_node, connect_metasound_nodes, '
        + 'disconnect_metasound_nodes, remove_metasound_node, set_metasound_default, add_metasound_input, add_metasound_output params). Optional per step: id (names the node '
        + 'it creates; later steps use "$id" in nodeId/nodeIds/sourceNodeId/targetNodeId), from/to ("$id.PinName" shorthand for '
        + 'connect; interface nodes such as the On Play input are named with the explicit fields).',
    },
    save: { type: 'boolean', description: 'Save the MetaSound once, after the steps (default true); a batch that stops at a failing step saves the steps before it. false leaves every edit in memory only.' },
  })), examples: [{ title: 'Build a one-oscillator MetaSound', ...BUILD_METASOUND_EXAMPLE }] },
  withInputProps(a('configure_distance_attenuation', 'Configure distance attenuation: inner radius (or a box\'s size per axis), falloff distance and curve, and air absorption (a low-pass that muffles the sound with distance).',
    ['assetPath', 'innerRadius', 'falloffDistance', 'distanceAlgorithm', 'save'], ['assetPath']), {
    extent: BOX_EXTENT,
    attenuateWithLPF: { type: 'boolean', description: 'Air absorption on or off: a low-pass whose cutoff drops with distance. Sending any lpf value turns it on unless this is false.' },
    lpfRadiusMin: { type: 'number', description: 'Distance in cm where the low-pass starts closing; nearer than this the cutoff is lpfFrequencyAtMin.' },
    lpfRadiusMax: { type: 'number', description: 'Distance in cm where the cutoff reaches lpfFrequencyAtMax.' },
    lpfFrequencyAtMin: { type: 'number', description: 'Low-pass cutoff in Hz at lpfRadiusMin and nearer (20000 leaves the sound unfiltered).' },
    lpfFrequencyAtMax: { type: 'number', description: 'Low-pass cutoff in Hz at lpfRadiusMax and beyond (900 sounds like a distant, muffled engine).' },
  }),
  a('configure_mix_eq', 'Configure Sound Mix equalization: the four bands through eqSettings (frequencyCenterN, gainN, bandwidthN for N 0-3), or the low, mid, highMid and high fields on top of them. Sending a band switches the EQ on unless applyEQ is false; eqPriority ranks the mix against other active mixes. Values are clamped (gain 0-4, frequency 0-20000 Hz, bandwidth 0-2) and the reply lists every band after clamping; a call that sets nothing is refused.',
    ['assetPath', 'applyEQ', 'eqPriority', 'eqSettings', 'lowFrequency', 'lowGain', 'midFrequency', 'midGain', 'highMidFrequency', 'highMidGain', 'highFrequency', 'highGain', 'save'], ['assetPath']),
  a('configure_occlusion', 'Set occlusion on a Sound Attenuation asset (assetPath), each value only when sent: enable turns it on or off, occlusionFilterScale (0-1) times 20000 Hz is the low-pass cutoff when blocked, occlusionVolumeScale (0-1) the volume kept, occlusionInterpolationTime the seconds to blend. The reply reads the four values back; the asset is saved unless save is false.', ['assetPath', 'enable', 'occlusionVolumeScale', 'occlusionFilterScale', 'occlusionInterpolationTime', 'save'], ['assetPath']),
  a('configure_reverb_send', 'Configure an audio reverb send.', ['assetPath', 'enableReverbSend', 'reverbDistanceMin', 'reverbDistanceMax', 'reverbWetLevelMin', 'reverbWetLevelMax', 'save'], ['assetPath']),
  a('configure_spatialization', 'Set how a Sound Attenuation asset places its sounds in space: spatialize on or off, and spatialization Default (panning) or Binaural (HRTF, for headphones). Only what is sent changes, a call with neither is refused, and another algorithm is INVALID_ARGUMENT. Saved unless save is false.', ['assetPath', 'spatialize', 'spatialization', 'save'], ['assetPath']),
  a('connect_cue_nodes', 'Connect two Sound Cue graph nodes.', ['assetPath', 'sourceNodeId', 'targetNodeId', 'childIndex', 'save'], ['assetPath', 'sourceNodeId', 'targetNodeId']),
  withInputProps(a('remove_metasound_node', 'Remove one node, or several with nodeIds, and every link on them from a MetaSound graph; graph inputs and outputs (On Play, Out Mono) are refused. All or nothing.', ['assetPath', 'nodeId', 'nodeIds', 'save'], ['assetPath'], ['removed', 'removedCount', 'saved'], ['removed'], META, ['nodeId', 'nodeIds']), {
    nodeIds: { type: 'array', items: { type: 'string' }, minItems: 1, maxItems: 200, description: 'Several node ids to remove in one call, in place of nodeId; every one must exist or nothing is removed.' },
  }),
  a('disconnect_metasound_nodes', 'Remove a link from a MetaSound graph: the one into targetNodeId.targetInputName, or every link out of sourceNodeId.sourceOutputName; with both ends given, that exact link. Refused when there is no such link.', ['assetPath', 'sourceNodeId', 'sourceOutputName', 'targetNodeId', 'targetInputName', 'save'], ['assetPath'], ['edgesRemoved', 'saved'], ['edgesRemoved'], META, ['targetNodeId', 'sourceNodeId']),
  { ...withTopics(utilityRecord({
    tool: T, action: 'get_metasound_graph', family: 'metasound', summary: 'Read a MetaSound graph: every node with its input literals, the links by pin name, and the graph inputs and outputs.',
    params: ['assetPath'], required: ['assetPath'], effect: 'read', plugins: META,
    outputs: ['assetPath', 'nodeCount', 'edgeCount', 'nodes', 'edges', 'graphInputs', 'graphOutputs'], outputRequired: ['nodes', 'edges'],
  }), ['read metasound graph', 'metasound nodes', 'inspect metasound', 'metasound links']), examples: [{ title: 'Read a one-oscillator MetaSound', ...GET_METASOUND_GRAPH_EXAMPLE }] },
  // A node's class name was a guess until add_node refused it: nothing listed what the registry holds.
  { ...withInputProps(withTopics(utilityRecord({
    tool: T, action: 'list_metasound_nodes', family: 'metasound', summary: 'List the MetaSound node classes the engine has, by part of their class name, display name or category, with what each does and, with includePins, its input and output pins: the nodeClassName add_node takes.',
    params: [], effect: 'read', plugins: META,
    outputs: ['nodeClasses', 'matchCount', 'truncated'], outputRequired: ['nodeClasses'],
  }), ['list metasound nodes', 'metasound node classes', 'available metasound nodes', 'find metasound node', 'metasound node library']), {
    filter: { type: 'string', description: 'Part of a class name, display name or category, spaces and case ignored ("Trigger", "Envelope", "Filters"); omitted, every class up to limit.' },
    includePins: { type: 'boolean', description: 'Also list each class\'s input and output pins as "Name (Type)", the names connect and set_default take.' },
    limit: { type: 'number', minimum: 1, maximum: 200, description: 'At most this many classes (default 50), those whose own name equals, starts with or contains filter first, the engine\'s UE nodes first among those, then by name; matchCount and truncated say what was left out.' },
  }), examples: [{ title: 'Find the trigger nodes', input: { action: 'list_metasound_nodes', filter: 'Trigger Filter', includePins: true }, output: { success: true, message: '1 node classes match', nodeClasses: [{ nodeClassName: 'UE.Trigger Filter', category: 'Triggers', description: 'When triggered, Heads has the given probability chance to trigger, otherwise it\'s Tail.', inputs: ['Trigger (Trigger)', 'Reset (Trigger)', 'Seed (Int32)', 'Probability (Float)'], outputs: ['Heads (Trigger)', 'Tails (Trigger)'] }], matchCount: 1, truncated: false } }] },
  a('connect_metasound_nodes', 'Connect two MetaSound graph pins.', ['assetPath', 'sourceNodeId', 'sourceOutputName', 'targetNodeId', 'targetInputName', 'save'], ['assetPath', 'sourceNodeId', 'sourceOutputName', 'targetNodeId', 'targetInputName'], [], [], META),
  a('create_attenuation_settings', 'Create attenuation settings and return the asset path.', ['name', 'path', 'innerRadius', 'falloffDistance', 'save'], ['name'], ['assetPath'], ['assetPath']),
  a('create_dialogue_voice', 'Create a Dialogue Voice asset and return its path.', ['name', 'path', 'gender', 'plurality', 'save'], ['name'], ['assetPath'], ['assetPath']),
  a('create_dialogue_wave', 'Create a Dialogue Wave asset and return its path; wavePath or speakerPath adds its first context mapping.', ['name', 'path', 'spokenText', 'wavePath', 'speakerPath', 'save'], ['name'], ['assetPath'], ['assetPath']),
  withInputProps(a('create_metasound', 'Create a MetaSound asset, named by name and path or by its full assetPath, and return its asset path. A MetaSound already at that path is refused (ASSET_EXISTS), never replaced. A one-shot source (the default: On Play in, On Finished out) is never virtualized by the engine, so one left playing on an AudioComponent that starts out of earshot is dropped for good; make a drone, hum or ambience with oneShot false.', ['name', 'path', 'assetPath', 'oneShot', 'save'], [], ['assetPath', 'oneShot'], ['assetPath'], META, ['name', 'assetPath']), {
    oneShot: { type: 'boolean', description: 'false makes a continuous source with no On Finished output: the engine treats it as looping, so a hum or ambience whose AudioComponent starts out of earshot resumes when the listener comes near. Default true (it plays until On Finished fires).' },
  }),
  a('create_reverb_effect', 'Create a Reverb Effect asset and return its path.', ['name', 'path', 'density', 'diffusion', 'gain', 'gainHF', 'decayTime', 'decayHFRatio', 'save'], ['name'], ['assetPath'], ['assetPath']),
  a('create_source_effect_chain', 'Create a Source Effect Chain asset and return its path.', ['name', 'path', 'save'], ['name'], ['assetPath'], ['assetPath']),
  withInputProps(a('create_submix_effect', 'Create a Sound Submix whose effect chain holds a new submix effect preset of effectType, and return its path.', ['name', 'path', 'effectType', 'save'], ['name', 'effectType'], ['assetPath'], ['assetPath']), {
    effectType: { type: 'string', description: 'Submix effect: Reverb, EQ or Dynamics (a compressor).' },
  }),
  withInputProps(a('create_sound_class', 'Create a Sound Class asset and return its asset path.', ['name', 'path', 'parentClass', 'volume', 'pitch', 'save'], ['name'], ['assetPath'], ['assetPath']), { parentClass: PARENT_CLASS }),
  withTopics(a('create_sound_cue', 'Create a Sound Cue asset and return its asset path; volume and pitch add a modulator over the wave player.', ['name', 'path', 'wavePath', 'looping', 'volume', 'pitch', 'save'], ['name'], ['assetPath'], ['assetPath']), ['sound cue', 'new sound cue', 'audio cue']),
  a('create_sound_mix', 'Create a Sound Mix asset and return its asset path.', ['name', 'path', 'save'], ['name'], ['assetPath'], ['assetPath']),
  utilityRecord({
    tool: T, action: 'get_audio_info', family: 'authoring',
    summary: 'Read an audio asset: Sound Wave duration, sample rate and channel count; Sound Cue duration, node count and attenuation; Sound Class volume, pitch and parent; Sound Mix modifier count; Attenuation falloff.',
    topics: ['sound length', 'duration', 'sample rate', 'channel count', 'how long is a sound', 'wav sample rate', 'how many nodes in a sound cue'],
    params: ['assetPath'], required: ['assetPath'], effect: 'read',
    outputs: ['assetPath', 'assetClass', 'type', 'duration', 'nodeCount', 'attenuationPath', 'sampleRate',
      'numChannels', 'volume', 'pitch', 'parentClass', 'modifierCount', 'falloffDistance', 'spatialize'],
    outputRequired: ['assetPath', 'assetClass', 'type'],
  }),
  a('set_audio_occlusion', 'Configure occlusion settings on a sound asset.', ['soundPath', 'enable', 'occlusionVolumeScale', 'occlusionFilterScale', 'occlusionInterpolationTime', 'save'], ['soundPath']),
  withInputProps(a('set_class_parent', 'Set the parent of a Sound Class, or clear it with an empty parentClass, keeping both classes\' child lists in step (a mix modifier applying to children walks them). A parent that does not load (PARENT_NOT_FOUND), or that is the class itself or one of its children (CYCLIC_PARENT), is refused. Saved unless save is false.', ['assetPath', 'parentClass', 'save'], ['assetPath', 'parentClass']), {
    parentClass: { type: 'string', description: 'Canonical /Game SoundClass asset path of the new parent; an empty string clears the parent.' },
  }),
  a('set_class_properties', 'Set the properties of a Sound Class, each only when sent: volume and pitch (multipliers, 1 leaves them as authored), lowPassFilterFrequency (Hz; 20000 leaves the sound unfiltered), lfeBleed and voiceCenterChannelVolume (0-1). The reply lists all five; a call that sends none is refused. Saved unless save is false.', ['assetPath', 'volume', 'pitch', 'lowPassFilterFrequency', 'lfeBleed', 'voiceCenterChannelVolume', 'save'], ['assetPath']),
  a('set_cue_attenuation', 'Assign attenuation settings to a Sound Cue; omit attenuationPath to clear them.', ['assetPath', 'attenuationPath', 'save'], ['assetPath']),
  withTopics(a('set_doppler_effect', 'Add a Doppler node at the root of a Sound Cue (or update the one already there) so the cue shifts pitch as its source moves toward or away from the listener. Sound Cues only; a SoundWave or MetaSound is refused.',
    ['assetPath', 'dopplerIntensity', 'smoothing', 'save'], ['assetPath'], ['nodeName', 'inserted', 'dopplerIntensity', 'smoothing', 'rootNodeClass', 'drivesNode', 'saved'], ['inserted']),
    ['doppler effect', 'pitch shift moving sound']),
  a('set_cue_concurrency', 'Assign concurrency settings to a Sound Cue.', ['assetPath', 'concurrencyPath', 'save'], ['assetPath']),
  { ...withTopics(a('set_dialogue_context', 'Add a context to a Dialogue Wave: speakerPath (the DialogueVoice that speaks the line) speaking to targetVoices (the DialogueVoices listening), with soundWavePath (the SoundWave that plays) and localizationKeyFormat (default {ContextHash}). The same speaker and listeners already on the wave is CONTEXT_EXISTS unless replace is true, which swaps that context (replaced). Saved unless save is false (saved).', ['assetPath', 'speakerPath', 'targetVoices', 'soundWavePath', 'localizationKeyFormat', 'replace', 'save'], ['assetPath', 'speakerPath'], ['replaced', 'saved', 'contextCount']), ['link sound to speaker']), examples: [{ title: 'The guard says the line to the player', input: { action: 'set_dialogue_context', assetPath: '/Game/Audio/Dialogue/DW_Halt', speakerPath: '/Game/Audio/Dialogue/DV_Guard', targetVoices: ['/Game/Audio/Dialogue/DV_Player'], soundWavePath: '/Game/Audio/Dialogue/SW_Halt_Guard' }, output: { success: true, replaced: false, saved: true, contextCount: 2 } }] },
  withInputProps(a('set_sound_attenuation', 'Create a Sound Attenuation asset name in path (default /Game/Audio/Attenuation): innerRadius (default 400), falloffDistance (default 3600), attenuationShape Sphere (the default), Capsule, Box (sized per axis with extent) or Cone, and falloffMode Linear (the default), Logarithmic, Inverse or NaturalSound; another shape or mode is refused. An asset already at the path is ASSET_ALREADY_EXISTS (change one with configure_distance_attenuation). Saved unless save is false; the reply reads the values back.', ['name', 'path', 'innerRadius', 'falloffDistance', 'attenuationShape', 'falloffMode', 'save'], ['name'], ['assetPath'], ['assetPath']), {
    extent: BOX_EXTENT,
  }),
];
