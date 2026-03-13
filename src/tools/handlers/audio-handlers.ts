/**
 * Audio tool handlers - migrated from class-based AudioTools
 * All operations route through executeAutomationRequest to the C++ bridge
 */
import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, AudioArgs } from '../../types/handler-types.js';
import { requireNonEmptyString } from './common-handlers.js';
import { executeAutomationRequest } from './common-handlers.js';
import { toNumber, toBoolean, toString, toVec3Array, toRotArray, validateAudioParams } from '../../utils/type-coercion.js';
import { TOOL_ACTIONS } from '../../utils/action-constants.js';
/**
 * Create sound cue
 */
async function createSoundCue(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');
  requireNonEmptyString(args?.wavePath ?? args?.soundPath, 'soundPath', 'Missing required parameter: soundPath (or wavePath)');

  const name = args.name ?? '';
  const wavePath = args.wavePath ?? args.soundPath ?? '';
  const savePath = toString(args.savePath) || '/Game/Audio/Cues';
  const { volume, pitch } = validateAudioParams(
    toNumber(args.settings?.volume),
    toNumber(args.settings?.pitch)
  );

  const payload = {
    name,
    packagePath: savePath,
    wavePath,
    attenuationPath: toString(args.settings?.attenuationSettings),
    volume,
    pitch,
    looping: toBoolean(args.settings?.looping)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_SOUND_CUE, payload)) as Record<string, unknown>;
}

/**
 * Play sound at location
 */
async function playSoundAtLocation(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.soundPath, 'soundPath', 'Missing required parameter: soundPath');

  const soundPath = args.soundPath ?? '';
  const location = toVec3Array(args.location) ?? [0, 0, 0];
  const rotation = toRotArray(args.rotation) ?? [0, 0, 0];
  const { volume, pitch } = validateAudioParams(toNumber(args.volume), toNumber(args.pitch));

  const payload = {
    soundPath,
    location,
    rotation,
    volume,
    pitch,
    startTime: toNumber(args.startTime) ?? 0.0,
    attenuationPath: toString(args.attenuationPath),
    concurrencyPath: toString(args.concurrencyPath)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.PLAY_SOUND_AT_LOCATION, payload)) as Record<string, unknown>;
}

/**
 * Play sound 2D
 */
async function playSound2D(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.soundPath, 'soundPath', 'Missing required parameter: soundPath');

  const payload = {
    soundPath: args.soundPath ?? '',
    volume: toNumber(args.volume) ?? 1.0,
    pitch: toNumber(args.pitch) ?? 1.0,
    startTime: toNumber(args.startTime) ?? 0.0
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.PLAY_SOUND_2D, payload)) as Record<string, unknown>;
}

/**
 * Create audio component
 */
async function createAudioComponent(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.actorName, 'actorName', 'Missing required parameter: actorName');
  requireNonEmptyString(args?.componentName, 'componentName', 'Missing required parameter: componentName');
  requireNonEmptyString(args?.soundPath, 'soundPath', 'Missing required parameter: soundPath');

  const payload = {
    actorName: args.actorName ?? '',
    componentName: args.componentName ?? '',
    soundPath: args.soundPath ?? '',
    autoPlay: toBoolean(args.autoPlay) ?? false,
    is3D: toBoolean(args.is3D) ?? true
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_AUDIO_COMPONENT, payload)) as Record<string, unknown>;
}

/**
 * Set sound attenuation
 */
async function setSoundAttenuation(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');

  const payload = {
    name: args.name ?? '',
    innerRadius: toNumber(args.innerRadius),
    falloffDistance: toNumber(args.falloffDistance),
    attenuationShape: toString(args.attenuationShape),
    falloffMode: toString(args.falloffMode)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SET_SOUND_ATTENUATION, payload)) as Record<string, unknown>;
}

/**
 * Create sound class
 */
async function createSoundClass(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');

  const payload = {
    name: args.name ?? '',
    parentClass: toString(args.parentClass),
    properties: args.properties
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_SOUND_CLASS, payload)) as Record<string, unknown>;
}

/**
 * Create sound mix
 */
async function createSoundMix(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');

  const payload = {
    name: args.name ?? '',
    classAdjusters: args.classAdjusters
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_SOUND_MIX, payload)) as Record<string, unknown>;
}

/**
 * Push sound mix
 */
async function pushSoundMix(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.mixName ?? args?.name, 'mixName', 'Missing required parameter: mixName (or name)');

  const payload = {
    mixName: args.mixName ?? args.name ?? ''
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.PUSH_SOUND_MIX, payload)) as Record<string, unknown>;
}

/**
 * Pop sound mix
 */
async function popSoundMix(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.mixName ?? args?.name, 'mixName', 'Missing required parameter: mixName (or name)');

  const payload = {
    mixName: args.mixName ?? args.name ?? ''
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.POP_SOUND_MIX, payload)) as Record<string, unknown>;
}

/**
 * Create ambient sound
 */
async function createAmbientSound(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.soundPath, 'soundPath', 'Missing required parameter: soundPath');

  const payload = {
    soundPath: args.soundPath ?? '',
    location: toVec3Array(args.location) ?? [0, 0, 0],
    volume: toNumber(args.volume) ?? 1.0,
    pitch: toNumber(args.pitch) ?? 1.0,
    startTime: toNumber(args.startTime) ?? 0.0,
    attenuationPath: toString(args.attenuationPath),
    concurrencyPath: toString(args.concurrencyPath)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_AMBIENT_SOUND, payload)) as Record<string, unknown>;
}

/**
 * Create reverb zone
 */
async function createReverbZone(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');

  const payload = {
    name: args.name ?? '',
    location: toVec3Array(args.location) ?? [0, 0, 0],
    size: toVec3Array(args.size) ?? [0, 0, 0],
    reverbEffect: toString(args.reverbEffect),
    volume: toNumber(args.volume),
    fadeTime: toNumber(args.fadeTime)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_REVERB_ZONE, payload)) as Record<string, unknown>;
}

/**
 * Enable audio analysis
 */
async function enableAudioAnalysis(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  const payload = {
    enabled: toBoolean(args.enabled),
    fftSize: toNumber(args.fftSize),
    outputType: toString(args.outputType)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.ENABLE_AUDIO_ANALYSIS, payload)) as Record<string, unknown>;
}

/**
 * Fade sound
 */
async function fadeSound(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  requireNonEmptyString(args?.soundName, 'soundName', 'Missing required parameter: soundName');

  const payload = {
    soundName: args.soundName ?? '',
    targetVolume: toNumber(args.targetVolume),
    fadeTime: toNumber(args.fadeTime),
    fadeType: toString(args.fadeType) || 'FadeTo'
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.FADE_SOUND, payload)) as Record<string, unknown>;
}

/**
 * Set doppler effect
 */
async function setDopplerEffect(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  const payload = {
    enabled: toBoolean(args.enabled),
    scale: toNumber(args.scale) ?? 1.0
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SET_DOPPLER_EFFECT, payload)) as Record<string, unknown>;
}

/**
 * Set audio occlusion
 */
async function setAudioOcclusion(tools: ITools, args: AudioArgs): Promise<Record<string, unknown>> {
  const payload = {
    enabled: toBoolean(args.enabled),
    lowPassFilterFrequency: toNumber(args.lowPassFilterFrequency),
    volumeAttenuation: toNumber(args.volumeAttenuation)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SET_AUDIO_OCCLUSION, payload)) as Record<string, unknown>;
}

/**
 * Main handler for audio tools
 */
export async function handleAudioTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsTyped = args as AudioArgs;
  const argsRecord = args as Record<string, unknown>;

  switch (action) {
    case 'create_sound_cue':
      return cleanObject(await createSoundCue(tools, argsTyped)) as Record<string, unknown>;

    case 'play_sound_at_location':
      return cleanObject(await playSoundAtLocation(tools, argsTyped)) as Record<string, unknown>;

    case 'play_sound_2d':
      return cleanObject(await playSound2D(tools, argsTyped)) as Record<string, unknown>;

    case 'create_audio_component':
      return cleanObject(await createAudioComponent(tools, argsTyped)) as Record<string, unknown>;

    case 'set_sound_attenuation':
      return cleanObject(await setSoundAttenuation(tools, argsTyped)) as Record<string, unknown>;

    case 'create_sound_class':
      return cleanObject(await createSoundClass(tools, argsTyped)) as Record<string, unknown>;

    case 'create_sound_mix':
      return cleanObject(await createSoundMix(tools, argsTyped)) as Record<string, unknown>;

    case 'push_sound_mix':
      return cleanObject(await pushSoundMix(tools, argsTyped)) as Record<string, unknown>;

    case 'pop_sound_mix':
      return cleanObject(await popSoundMix(tools, argsTyped)) as Record<string, unknown>;

    case 'create_ambient_sound':
      return cleanObject(await createAmbientSound(tools, argsTyped)) as Record<string, unknown>;

    case 'create_reverb_zone':
      return cleanObject(await createReverbZone(tools, argsTyped)) as Record<string, unknown>;

    case 'enable_audio_analysis':
      return cleanObject(await enableAudioAnalysis(tools, argsTyped)) as Record<string, unknown>;

    case 'fade_sound':
      return cleanObject(await fadeSound(tools, argsTyped)) as Record<string, unknown>;

    case 'set_doppler_effect':
      return cleanObject(await setDopplerEffect(tools, argsTyped)) as Record<string, unknown>;

    case 'set_audio_occlusion':
      return cleanObject(await setAudioOcclusion(tools, argsTyped)) as Record<string, unknown>;

    // Direct pass-through actions (already using bridge directly)
    case 'spawn_sound_at_location':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.SPAWN_SOUND_AT_LOCATION, argsRecord)) as Record<string, unknown>;

    case 'play_sound_attached':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.PLAY_SOUND_ATTACHED, argsRecord)) as Record<string, unknown>;

    case 'set_sound_mix_class_override':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.SET_SOUND_MIX_CLASS_OVERRIDE, argsRecord)) as Record<string, unknown>;

    case 'clear_sound_mix_class_override':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.CLEAR_SOUND_MIX_CLASS_OVERRIDE, argsRecord)) as Record<string, unknown>;

    case 'set_base_sound_mix':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.SET_BASE_SOUND_MIX, argsRecord)) as Record<string, unknown>;

    case 'prime_sound':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.PRIME_SOUND, argsRecord)) as Record<string, unknown>;

    case 'fade_sound_in':
    case 'fade_sound_out':
      return cleanObject(await executeAutomationRequest(tools, action, argsRecord)) as Record<string, unknown>;

    default:
      return cleanObject({ success: false, isError: true, error: 'UNKNOWN_ACTION', message: `Unknown audio action: ${action}` });
  }
}
