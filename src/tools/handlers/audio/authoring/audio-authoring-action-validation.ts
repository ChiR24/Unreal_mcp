import { requireNonEmptyString } from '../../foundation/dispatch/common-handlers.js';

interface AudioAuthoringRequirement {
  readonly field: string;
  readonly message: string;
  readonly value?: (args: Record<string, unknown>) => unknown;
}

const nameRequired: readonly AudioAuthoringRequirement[] = [
  { field: 'name', message: 'Missing required parameter: name' }
];

const assetPathRequired: readonly AudioAuthoringRequirement[] = [
  { field: 'assetPath', message: 'Missing required parameter: assetPath' }
];

const AUDIO_AUTHORING_REQUIRED_FIELDS: Readonly<Record<string, readonly AudioAuthoringRequirement[]>> = {
  create_sound_cue: nameRequired,
  add_cue_node: [
    ...assetPathRequired,
    { field: 'nodeType', message: 'Missing required parameter: nodeType' }
  ],
  connect_cue_nodes: [
    ...assetPathRequired,
    { field: 'sourceNodeId', message: 'Missing required parameter: sourceNodeId' },
    { field: 'targetNodeId', message: 'Missing required parameter: targetNodeId' }
  ],
  set_cue_attenuation: assetPathRequired,
  set_cue_concurrency: assetPathRequired,
  create_metasound: nameRequired,
  add_metasound_node: [
    ...assetPathRequired,
    { field: 'nodeType', message: 'Missing required parameter: nodeType or nodeClassName', value: args => args.nodeType ?? args.nodeClassName }
  ],
  connect_metasound_nodes: [
    ...assetPathRequired,
    { field: 'sourceNodeId', message: 'Missing required parameter: sourceNodeId', value: args => args.sourceNodeId ?? args.sourceNode },
    { field: 'sourceOutputName', message: 'Missing required parameter: sourceOutputName', value: args => args.sourceOutputName ?? args.sourcePin },
    { field: 'targetNodeId', message: 'Missing required parameter: targetNodeId', value: args => args.targetNodeId ?? args.targetNode },
    { field: 'targetInputName', message: 'Missing required parameter: targetInputName', value: args => args.targetInputName ?? args.targetPin }
  ],
  add_metasound_input: [
    ...assetPathRequired,
    { field: 'inputName', message: 'Missing required parameter: inputName' },
    { field: 'inputType', message: 'Missing required parameter: inputType' }
  ],
  add_metasound_output: [
    ...assetPathRequired,
    { field: 'outputName', message: 'Missing required parameter: outputName' },
    { field: 'outputType', message: 'Missing required parameter: outputType' }
  ],
  set_metasound_default: [
    ...assetPathRequired,
    { field: 'inputName', message: 'Missing required parameter: inputName' }
  ],
  create_sound_class: nameRequired,
  set_class_properties: assetPathRequired,
  set_class_parent: assetPathRequired,
  create_sound_mix: nameRequired,
  add_mix_modifier: [
    ...assetPathRequired,
    { field: 'soundClassPath', message: 'Missing required parameter: soundClassPath' }
  ],
  configure_mix_eq: assetPathRequired,
  create_attenuation_settings: nameRequired,
  configure_distance_attenuation: assetPathRequired,
  configure_spatialization: assetPathRequired,
  configure_occlusion: assetPathRequired,
  configure_reverb_send: assetPathRequired,
  create_dialogue_voice: nameRequired,
  create_dialogue_wave: nameRequired,
  set_dialogue_context: assetPathRequired,
  create_reverb_effect: nameRequired,
  create_source_effect_chain: nameRequired,
  add_source_effect: [
    ...assetPathRequired,
    { field: 'effectType', message: 'Missing required parameter: effectType' }
  ],
  create_submix_effect: [
    ...nameRequired,
    { field: 'effectType', message: 'Missing required parameter: effectType' }
  ],
  get_audio_info: assetPathRequired
};

export function validateAudioAuthoringAction(action: string, args: Record<string, unknown>): boolean {
  const requirements = AUDIO_AUTHORING_REQUIRED_FIELDS[action];
  if (requirements === undefined) {
    return false;
  }

  for (const requirement of requirements) {
    requireNonEmptyString(
      requirement.value ? requirement.value(args) : args[requirement.field],
      requirement.field,
      requirement.message
    );
  }

  return true;
}
