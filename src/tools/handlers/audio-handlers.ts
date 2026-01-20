import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, HandlerResult, AudioArgs, Vector3, Rotator } from '../../types/handler-types.js';
import { requireNonEmptyString } from './common-handlers.js';

function toVec3Array(v: Vector3 | undefined): [number, number, number] | undefined {
  if (!v || typeof v !== 'object') return undefined;
  const x = Number(v.x);
  const y = Number(v.y);
  const z = Number(v.z);
  if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(z)) return undefined;
  return [x, y, z];
}

function toRotArray(r: Rotator | undefined): [number, number, number] | undefined {
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
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  const argsTyped = args as AudioArgs;
  const argsRecord = args as Record<string, unknown>;
  
  switch (action) {
    case 'create_sound_cue':
      requireNonEmptyString(argsTyped?.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsTyped?.wavePath ?? argsTyped?.soundPath, 'soundPath', 'Missing required parameter: soundPath (or wavePath)');
      return cleanObject(await tools.audioTools.createSoundCue({
        name: argsTyped.name ?? '',
        // MCP schema uses soundPath; AudioTools uses wavePath.
        wavePath: argsTyped.wavePath ?? argsTyped.soundPath ?? '',
        savePath: argsTyped.savePath,
        settings: argsTyped.settings
      })) as HandlerResult;

    case 'play_sound_at_location':
      requireNonEmptyString(argsTyped?.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      return cleanObject(await tools.audioTools.playSoundAtLocation({
        soundPath: argsTyped.soundPath ?? '',
        location: toVec3Array(argsTyped.location) ?? [0, 0, 0],
        rotation: toRotArray(argsTyped.rotation),
        volume: argsTyped.volume,
        pitch: argsTyped.pitch,
        startTime: argsTyped.startTime,
        attenuationPath: argsTyped.attenuationPath,
        concurrencyPath: argsTyped.concurrencyPath
      })) as HandlerResult;

    case 'play_sound_2d':
      requireNonEmptyString(argsTyped?.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      return cleanObject(await tools.audioTools.playSound2D({
        soundPath: argsTyped.soundPath ?? '',
        volume: argsTyped.volume,
        pitch: argsTyped.pitch,
        startTime: argsTyped.startTime
      })) as HandlerResult;

    case 'create_audio_component':
      requireNonEmptyString(argsTyped?.actorName, 'actorName', 'Missing required parameter: actorName');
      requireNonEmptyString(argsTyped?.componentName, 'componentName', 'Missing required parameter: componentName');
      requireNonEmptyString(argsTyped?.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      return cleanObject(await tools.audioTools.createAudioComponent({
        actorName: argsTyped.actorName ?? '',
        componentName: argsTyped.componentName ?? '',
        soundPath: argsTyped.soundPath ?? '',
        autoPlay: argsTyped.autoPlay,
        is3D: argsTyped.is3D
      })) as HandlerResult;

    case 'set_sound_attenuation':
      requireNonEmptyString(argsTyped?.name, 'name', 'Missing required parameter: name');
      return cleanObject(await tools.audioTools.setSoundAttenuation({
        name: argsTyped.name ?? '',
        innerRadius: argsTyped.innerRadius,
        falloffDistance: argsTyped.falloffDistance,
        attenuationShape: argsTyped.attenuationShape,
        falloffMode: argsTyped.falloffMode
      })) as HandlerResult;

    case 'create_sound_class':
      requireNonEmptyString(argsTyped?.name, 'name', 'Missing required parameter: name');
      return cleanObject(await tools.audioTools.createSoundClass({
        name: argsTyped.name ?? '',
        parentClass: argsTyped.parentClass,
        properties: argsTyped.properties
      })) as HandlerResult;

    case 'create_sound_mix':
      requireNonEmptyString(argsTyped?.name, 'name', 'Missing required parameter: name');
      return cleanObject(await tools.audioTools.createSoundMix({
        name: argsTyped.name ?? '',
        classAdjusters: argsTyped.classAdjusters as { soundClass: string; volumeAdjust: number }[] | undefined
      })) as HandlerResult;

    case 'push_sound_mix':
      requireNonEmptyString(argsTyped?.mixName ?? argsTyped?.name, 'mixName', 'Missing required parameter: mixName (or name)');
      return cleanObject(await tools.audioTools.pushSoundMix({
        mixName: argsTyped.mixName ?? argsTyped.name ?? ''
      })) as HandlerResult;

    case 'pop_sound_mix':
      requireNonEmptyString(argsTyped?.mixName ?? argsTyped?.name, 'mixName', 'Missing required parameter: mixName (or name)');
      return cleanObject(await tools.audioTools.popSoundMix({
        mixName: argsTyped.mixName ?? argsTyped.name ?? ''
      })) as HandlerResult;

    case 'create_ambient_sound':
      requireNonEmptyString(argsTyped?.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      return cleanObject(await tools.audioTools.createAmbientSound({
        soundPath: argsTyped.soundPath ?? '',
        location: toVec3Array(argsTyped.location) ?? [0, 0, 0],
        volume: argsTyped.volume,
        pitch: argsTyped.pitch,
        startTime: argsTyped.startTime,
        attenuationPath: argsTyped.attenuationPath,
        concurrencyPath: argsTyped.concurrencyPath
      })) as HandlerResult;

    case 'create_reverb_zone':
      requireNonEmptyString(argsTyped?.name, 'name', 'Missing required parameter: name');
      return cleanObject(await tools.audioTools.createReverbZone({
        name: argsTyped.name ?? '',
        location: toVec3Array(argsTyped.location) ?? [0, 0, 0],
        size: toVec3Array(argsTyped.size) ?? [0, 0, 0],
        reverbEffect: argsTyped.reverbEffect,
        volume: argsTyped.volume,
        fadeTime: argsTyped.fadeTime
      })) as HandlerResult;

    case 'enable_audio_analysis':
      return cleanObject(await tools.audioTools.enableAudioAnalysis({
        enabled: argsTyped.enabled,
        fftSize: argsTyped.fftSize,
        outputType: argsTyped.outputType
      })) as HandlerResult;

    case 'fade_sound':
      requireNonEmptyString(argsTyped?.soundName, 'soundName', 'Missing required parameter: soundName');
      return cleanObject(await tools.audioTools.fadeSound({
        soundName: argsTyped.soundName ?? '',
        targetVolume: argsTyped.targetVolume,
        fadeTime: argsTyped.fadeTime,
        fadeType: argsTyped.fadeType
      })) as HandlerResult;

    case 'set_doppler_effect':
      return cleanObject(await tools.audioTools.setDopplerEffect({
        enabled: argsTyped.enabled,
        scale: argsTyped.scale
      })) as HandlerResult;

    case 'set_audio_occlusion':
      return cleanObject(await tools.audioTools.setAudioOcclusion({
        enabled: argsTyped.enabled,
        lowPassFilterFrequency: argsTyped.lowPassFilterFrequency,
        volumeAttenuation: argsTyped.volumeAttenuation
      })) as HandlerResult;

    case 'spawn_sound_at_location':
      // Direct pass-through to C++ handler
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('spawn_sound_at_location', argsRecord, { timeoutMs: getTimeoutMs() })) as HandlerResult;

    case 'play_sound_attached':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('play_sound_attached', argsRecord, { timeoutMs: getTimeoutMs() })) as HandlerResult;

    case 'set_sound_mix_class_override':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('set_sound_mix_class_override', argsRecord, { timeoutMs: getTimeoutMs() })) as HandlerResult;

    case 'clear_sound_mix_class_override':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('clear_sound_mix_class_override', argsRecord, { timeoutMs: getTimeoutMs() })) as HandlerResult;

    case 'set_base_sound_mix':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('set_base_sound_mix', argsRecord, { timeoutMs: getTimeoutMs() })) as HandlerResult;

    case 'prime_sound':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('prime_sound', argsRecord, { timeoutMs: getTimeoutMs() })) as HandlerResult;

    case 'fade_sound_in':
    case 'fade_sound_out':
      return cleanObject(await tools.automationBridge?.sendAutomationRequest(action, argsRecord, { timeoutMs: getTimeoutMs() })) as HandlerResult;

    // MetaSounds (Phase 3C.1)
    case 'create_metasound':
    case 'add_metasound_node':
    case 'connect_metasound_nodes':
    case 'remove_metasound_node':
    case 'set_metasound_variable':
    case 'create_oscillator':
    case 'create_envelope':
    case 'create_filter':
    case 'create_sequencer_node':
    case 'create_procedural_music':
    case 'import_audio_to_metasound':
    case 'export_metasound_preset':
    case 'configure_audio_modulation': {
      // Map test payload field names to C++ expected field names
      // Tests use metaSoundName/metaSoundPath but C++ expects name/packagePath
      const mappedArgs: Record<string, unknown> = { ...argsRecord };
      
      // Map metaSoundName -> name (for create_metasound)
      if ('metaSoundName' in mappedArgs && !('name' in mappedArgs)) {
        mappedArgs.name = mappedArgs.metaSoundName;
        delete mappedArgs.metaSoundName;
      }
      
      // Map metaSoundPath -> packagePath or assetPath (for node operations)
      if ('metaSoundPath' in mappedArgs) {
        if (!('packagePath' in mappedArgs)) {
          mappedArgs.packagePath = mappedArgs.metaSoundPath;
        }
        if (!('assetPath' in mappedArgs)) {
          mappedArgs.assetPath = mappedArgs.metaSoundPath;
        }
        delete mappedArgs.metaSoundPath;
      }
      
      // Route to manage_audio tool, preserving the action name in the payload
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('manage_audio', {
        ...mappedArgs,
        action: action 
      }, { timeoutMs: getTimeoutMs() })) as HandlerResult;
    }

    // MetaSounds Queries (A6)
    case 'list_metasound_assets': {
      // assetPath filter is optional
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('manage_audio', {
        ...argsRecord,
        action: 'list_metasound_assets'
      }, { timeoutMs: getTimeoutMs() })) as HandlerResult;
    }

    case 'get_metasound_inputs': {
      if (!argsRecord.assetPath) {
        return cleanObject({
          success: false,
          error: 'MISSING_PARAMETER',
          message: 'Missing required parameter: assetPath'
        });
      }
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('manage_audio', {
        ...argsRecord,
        action: 'get_metasound_inputs'
      }, { timeoutMs: getTimeoutMs() })) as HandlerResult;
    }

    case 'trigger_metasound': {
      if (!argsRecord.actorName) {
        return cleanObject({
          success: false,
          error: 'MISSING_PARAMETER',
          message: 'Missing required parameter: actorName'
        });
      }
      if (!argsRecord.inputName) {
        return cleanObject({
          success: false,
          error: 'MISSING_PARAMETER',
          message: 'Missing required parameter: inputName'
        });
      }
      // value can be number, boolean, or string
      return cleanObject(await tools.automationBridge?.sendAutomationRequest('manage_audio', {
        ...argsRecord,
        action: 'trigger_metasound'
      }, { timeoutMs: getTimeoutMs() })) as HandlerResult;
    }

    default:
      return cleanObject({ success: false, error: 'UNKNOWN_ACTION', message: `Unknown audio action: ${action}` });
  }
}
