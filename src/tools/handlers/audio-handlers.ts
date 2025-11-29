import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';

export async function handleAudioTools(
  action: string,
  args: any,
  tools: ITools
) {
  switch (action) {
    case 'create_sound_cue':
      return await tools.audio.createSoundCue({
        name: args.name,
        wavePath: args.wavePath,
        savePath: args.savePath,
        settings: args.settings
      });

    case 'play_sound_at_location':
      return await tools.audio.playSoundAtLocation({
        soundPath: args.soundPath,
        location: [args.location.x, args.location.y, args.location.z],
        rotation: args.rotation ? [args.rotation.pitch, args.rotation.yaw, args.rotation.roll] : undefined,
        volume: args.volume,
        pitch: args.pitch,
        startTime: args.startTime,
        attenuationPath: args.attenuationPath,
        concurrencyPath: args.concurrencyPath
      });

    case 'play_sound_2d':
      return await tools.audio.playSound2D({
        soundPath: args.soundPath,
        volume: args.volume,
        pitch: args.pitch,
        startTime: args.startTime
      });

    case 'create_audio_component':
      return await tools.audio.createAudioComponent({
        actorName: args.actorName,
        componentName: args.componentName,
        soundPath: args.soundPath,
        autoPlay: args.autoPlay,
        is3D: args.is3D
      });

    case 'set_sound_attenuation':
      return await tools.audio.setSoundAttenuation({
        name: args.name,
        innerRadius: args.innerRadius,
        falloffDistance: args.falloffDistance,
        attenuationShape: args.attenuationShape,
        falloffMode: args.falloffMode
      });

    case 'create_sound_class':
      return await tools.audio.createSoundClass({
        name: args.name,
        parentClass: args.parentClass,
        properties: args.properties
      });

    case 'create_sound_mix':
      return await tools.audio.createSoundMix({
        name: args.name,
        classAdjusters: args.classAdjusters
      });

    case 'push_sound_mix':
      return await tools.audio.pushSoundMix({
        mixName: args.mixName
      });

    case 'pop_sound_mix':
      return await tools.audio.popSoundMix({
        mixName: args.mixName
      });

    case 'create_ambient_sound':
      return await tools.audio.createAmbientSound({
        soundPath: args.soundPath,
        location: [args.location.x, args.location.y, args.location.z],
        volume: args.volume,
        pitch: args.pitch,
        startTime: args.startTime,
        attenuationPath: args.attenuationPath,
        concurrencyPath: args.concurrencyPath
      });

    case 'create_reverb_zone':
      return await tools.audio.createReverbZone({
        name: args.name,
        location: [args.location.x, args.location.y, args.location.z],
        size: [args.size.x, args.size.y, args.size.z],
        reverbEffect: args.reverbEffect,
        volume: args.volume,
        fadeTime: args.fadeTime
      });

    case 'enable_audio_analysis':
      return await tools.audio.enableAudioAnalysis({
        enabled: args.enabled,
        fftSize: args.fftSize,
        outputType: args.outputType
      });

    case 'fade_sound':
      return await tools.audio.fadeSound({
        soundName: args.soundName,
        targetVolume: args.targetVolume,
        fadeTime: args.fadeTime,
        fadeType: args.fadeType
      });

    case 'set_doppler_effect':
      return await tools.audio.setDopplerEffect({
        enabled: args.enabled,
        scale: args.scale
      });

    case 'set_audio_occlusion':
      return await tools.audio.setAudioOcclusion({
        enabled: args.enabled,
        lowPassFilterFrequency: args.lowPassFilterFrequency,
        volumeAttenuation: args.volumeAttenuation
      });

    default:
      return cleanObject({ success: false, error: 'UNKNOWN_ACTION', message: `Unknown audio action: ${action}` });
  }
}
