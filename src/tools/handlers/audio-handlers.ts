import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import { requireNonEmptyString } from './common-handlers.js';

function toVec3Array(v: any): [number, number, number] | undefined {
  if (!v || typeof v !== 'object') return undefined;
  const x = Number(v.x);
  const y = Number(v.y);
  const z = Number(v.z);
  if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(z)) return undefined;
  return [x, y, z];
}

function toRotArray(r: any): [number, number, number] | undefined {
  if (!r || typeof r !== 'object') return undefined;
  const pitch = Number(r.pitch);
  const yaw = Number(r.yaw);
  const roll = Number(r.roll);
  if (!Number.isFinite(pitch) || !Number.isFinite(yaw) || !Number.isFinite(roll)) return undefined;
  return [pitch, yaw, roll];
}

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

export async function handleAudioTools(
  action: string,
  args: any,
  tools: ITools
) {
  switch (action) {
    case 'create_sound_cue':
      requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(args?.wavePath ?? args?.soundPath, 'soundPath', 'Missing required parameter: soundPath (or wavePath)');
      return cleanObject(await tools.audioTools.createSoundCue({
        name: args.name,
        // MCP schema uses soundPath; AudioTools uses wavePath.
        wavePath: args.wavePath ?? args.soundPath,
        savePath: args.savePath,
        settings: args.settings
      }));

    case 'play_sound_at_location':
      requireNonEmptyString(args?.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      return cleanObject(await tools.audioTools.playSoundAtLocation({
        soundPath: args.soundPath,
        location: toVec3Array(args.location) ?? [0, 0, 0],
        rotation: toRotArray(args.rotation),
        volume: args.volume,
        pitch: args.pitch,
        startTime: args.startTime,
        attenuationPath: args.attenuationPath,
        concurrencyPath: args.concurrencyPath
      }));

    case 'play_sound_2d':
      requireNonEmptyString(args?.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      return cleanObject(await tools.audioTools.playSound2D({
        soundPath: args.soundPath,
        volume: args.volume,
        pitch: args.pitch,
        startTime: args.startTime
      }));

    case 'create_audio_component':
      requireNonEmptyString(args?.actorName, 'actorName', 'Missing required parameter: actorName');
      requireNonEmptyString(args?.componentName, 'componentName', 'Missing required parameter: componentName');
      requireNonEmptyString(args?.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      return cleanObject(await tools.audioTools.createAudioComponent({
        actorName: args.actorName,
        componentName: args.componentName,
        soundPath: args.soundPath,
        autoPlay: args.autoPlay,
        is3D: args.is3D
      }));

    case 'set_sound_attenuation':
      requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');
      return cleanObject(await tools.audioTools.setSoundAttenuation({
        name: args.name,
        innerRadius: args.innerRadius,
        falloffDistance: args.falloffDistance,
        attenuationShape: args.attenuationShape,
        falloffMode: args.falloffMode
      }));

    case 'create_sound_class':
      requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');
      return cleanObject(await tools.audioTools.createSoundClass({
        name: args.name,
        parentClass: args.parentClass,
        properties: args.properties
      }));

    case 'create_sound_mix':
      requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');
      return cleanObject(await tools.audioTools.createSoundMix({
        name: args.name,
        classAdjusters: args.classAdjusters
      }));

    case 'push_sound_mix':
      requireNonEmptyString(args?.mixName ?? args?.name, 'mixName', 'Missing required parameter: mixName (or name)');
      return cleanObject(await tools.audioTools.pushSoundMix({
        mixName: args.mixName ?? args.name
      }));

    case 'pop_sound_mix':
      requireNonEmptyString(args?.mixName ?? args?.name, 'mixName', 'Missing required parameter: mixName (or name)');
      return cleanObject(await tools.audioTools.popSoundMix({
        mixName: args.mixName ?? args.name
      }));

    case 'create_ambient_sound':
      requireNonEmptyString(args?.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      return cleanObject(await tools.audioTools.createAmbientSound({
        soundPath: args.soundPath,
        location: toVec3Array(args.location) ?? [0, 0, 0],
        volume: args.volume,
        pitch: args.pitch,
        startTime: args.startTime,
        attenuationPath: args.attenuationPath,
        concurrencyPath: args.concurrencyPath
      }));

    case 'create_reverb_zone':
      requireNonEmptyString(args?.name, 'name', 'Missing required parameter: name');
      return cleanObject(await tools.audioTools.createReverbZone({
        name: args.name,
        location: toVec3Array(args.location) ?? [0, 0, 0],
        size: toVec3Array(args.size) ?? [0, 0, 0],
        reverbEffect: args.reverbEffect,
        volume: args.volume,
        fadeTime: args.fadeTime
      }));

    case 'enable_audio_analysis':
      return cleanObject(await tools.audioTools.enableAudioAnalysis({
        enabled: args.enabled,
        fftSize: args.fftSize,
        outputType: args.outputType
      }));

    case 'fade_sound':
      requireNonEmptyString(args?.soundName, 'soundName', 'Missing required parameter: soundName');
      return cleanObject(await tools.audioTools.fadeSound({
        soundName: args.soundName,
        targetVolume: args.targetVolume,
        fadeTime: args.fadeTime,
        fadeType: args.fadeType
      }));

    case 'set_doppler_effect':
      return cleanObject(await tools.audioTools.setDopplerEffect({
        enabled: args.enabled,
        scale: args.scale
      }));

    case 'set_audio_occlusion':
      return cleanObject(await tools.audioTools.setAudioOcclusion({
        enabled: args.enabled,
        lowPassFilterFrequency: args.lowPassFilterFrequency,
        volumeAttenuation: args.volumeAttenuation
      }));

    case 'spawn_sound_at_location':
      // Direct pass-through to C++ handler
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('spawn_sound_at_location', args, { timeoutMs: getTimeoutMs() }));

    case 'play_sound_attached':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('play_sound_attached', args, { timeoutMs: getTimeoutMs() }));

    case 'set_sound_mix_class_override':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('set_sound_mix_class_override', args, { timeoutMs: getTimeoutMs() }));

    case 'clear_sound_mix_class_override':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('clear_sound_mix_class_override', args, { timeoutMs: getTimeoutMs() }));

    case 'set_base_sound_mix':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('set_base_sound_mix', args, { timeoutMs: getTimeoutMs() }));

    case 'prime_sound':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('prime_sound', args, { timeoutMs: getTimeoutMs() }));

    case 'fade_sound_in':
    case 'fade_sound_out':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest(action, args, { timeoutMs: getTimeoutMs() }));

    default:
      return cleanObject({ success: false, error: 'UNKNOWN_ACTION', message: `Unknown audio action: ${action}` });
  }
}
