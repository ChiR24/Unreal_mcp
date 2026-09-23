import { normalizePathFields } from '../../foundation/dispatch/common-handlers.js';

const AUDIO_AUTHORING_PATH_FIELDS = [
  'assetPath',
  'attenuationPath',
  'soundClassPath',
  'speakerPath',
  'reverbEffect',
  'effectPresetPath',
  'parentPath',
  'path',
  'savePath',
  'packagePath',
  'soundPath',
  'wavePath'
];

const CREATE_PATH_FALLBACK_ACTIONS = new Set([
  'create_sound_cue',
  'create_sound_class',
  'create_sound_mix'
]);

const LOAD_OBJECT_PATH_FIELDS = [
  'assetPath',
  'attenuationPath',
  'soundClassPath',
  'speakerPath',
  'reverbEffect'
];

function normalizeAssetPathForLoadObject(assetPath: string): string {
  if (!assetPath || !assetPath.startsWith('/')) {
    return assetPath;
  }

  const lastSegment = assetPath.split('/').pop() ?? '';
  if (lastSegment.includes('.')) {
    return assetPath;
  }

  return `${assetPath}.${lastSegment}`;
}

function normalizeLoadObjectFields(payload: Record<string, unknown>, fields: readonly string[]): void {
  for (const field of fields) {
    const value = payload[field];
    if (typeof value === 'string') {
      payload[field] = normalizeAssetPathForLoadObject(value);
    }
  }
}

function applyCreationAliases(payload: Record<string, unknown>, subAction: string): void {
  if (CREATE_PATH_FALLBACK_ACTIONS.has(subAction) && payload.path === undefined) {
    payload.path = payload.savePath ?? payload.packagePath;
  }

  if (subAction === 'create_sound_cue' && payload.wavePath === undefined && payload.soundPath !== undefined) {
    payload.wavePath = payload.soundPath;
  }
}

function applyMetasoundConnectionAliases(payload: Record<string, unknown>, subAction: string): void {
  if (subAction !== 'connect_metasound_nodes') {
    return;
  }

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

function applyEffectAliases(payload: Record<string, unknown>, subAction: string): void {
  if (subAction !== 'add_source_effect') {
    return;
  }

  const effectPresetPath = payload.effectPresetPath;
  if (typeof effectPresetPath === 'string') {
    payload.effectPresetPath = normalizeAssetPathForLoadObject(effectPresetPath);
  }
}

function applySoundClassAliases(payload: Record<string, unknown>, subAction: string): void {
  if (subAction === 'set_class_parent' && payload.parentClass && !payload.parentPath) {
    payload.parentPath = payload.parentClass;
    delete payload.parentClass;
  }
}

function applySpatializationAliases(payload: Record<string, unknown>, subAction: string): void {
  if (subAction !== 'configure_spatialization' || payload.spatialization === undefined) {
    return;
  }

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

function applyOcclusionAliases(payload: Record<string, unknown>, subAction: string): void {
  if (subAction === 'configure_occlusion' && payload.enable !== undefined && payload.enableOcclusion === undefined) {
    payload.enableOcclusion = payload.enable;
    delete payload.enable;
  }
}

export function prepareAudioAuthoringPayload(
  args: Record<string, unknown>,
  subAction: string
): Record<string, unknown> {
  const payload = normalizePathFields({ ...args, subAction }, AUDIO_AUTHORING_PATH_FIELDS);

  applyCreationAliases(payload, subAction);
  normalizeLoadObjectFields(payload, LOAD_OBJECT_PATH_FIELDS);
  applyMetasoundConnectionAliases(payload, subAction);
  applyEffectAliases(payload, subAction);
  applySoundClassAliases(payload, subAction);
  applySpatializationAliases(payload, subAction);
  applyOcclusionAliases(payload, subAction);

  return payload;
}
