#!/usr/bin/env node
/**
 * Comprehensive Audio Management Test Suite
 * Tool: manage_audio
 * Coverage: All 22 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // === SOUND CUE CREATION ===
  {
    scenario: 'Create Sound Cue',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_sound_cue',
      name: 'SC_TestCue',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      volume: 0.8,
      pitch: 1.2
    },
    expected: 'success'
  },
  {
    scenario: 'Create Sound Cue with location',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_sound_cue',
      name: 'SC_TestCue3D',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 100, y: 0, z: 0 },
      volume: 1.0
    },
    expected: 'success'
  },

  // === PLAY SOUND (play_sound_at_location, play_sound_2d, play_sound_attached, spawn_sound_at_location) ===
  {
    scenario: 'Play Sound at Location',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_at_location',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 100, y: 0, z: 50 },
      volume: 1.0,
      pitch: 1.0
    },
    expected: 'success'
  },
  {
    scenario: 'Play Sound 2D',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_2d',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      volume: 0.5,
      pitch: 1.0
    },
    expected: 'success'
  },
  {
    scenario: 'Play Sound with Rotation',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_at_location',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 0, y: 0, z: 0 },
      rotation: { pitch: 45, yaw: 90, roll: 0 },
      volume: 1.0
    },
    expected: 'success'
  },
  {
    scenario: 'Play Sound Attached',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_attached',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      actorName: 'TestActor',
      attachPointName: 'RootComponent',
      volume: 0.8
    },
    expected: 'success|not_found'
  },
  {
    scenario: 'Spawn Sound at Location',
    toolName: 'manage_audio',
    arguments: {
      action: 'spawn_sound_at_location',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 200, y: 200, z: 0 },
      volume: 0.7
    },
    expected: 'success'
  },

  // === AUDIO COMPONENT (create_audio_component) ===
  {
    scenario: 'Create Audio Component',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_audio_component',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 200, y: 0, z: 0 },
      volume: 0.7
    },
    expected: 'success'
  },
  {
    scenario: 'Create Audio Component with Attenuation',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_audio_component',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 0, y: 0, z: 0 },
      volume: 1.0,
      falloffDistance: 1000
    },
    expected: 'success'
  },

  // === SOUND MIXES (create_sound_mix, push_sound_mix, pop_sound_mix) ===
  {
    scenario: 'Create Sound Mix',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_sound_mix',
      name: 'SM_TestMix'
    },
    expected: 'success'
  },
  {
    scenario: 'Push Sound Mix',
    toolName: 'manage_audio',
    arguments: {
      action: 'push_sound_mix',
      mixName: 'SM_TestMix'
    },
    expected: 'success'
  },
  {
    scenario: 'Pop Sound Mix',
    toolName: 'manage_audio',
    arguments: {
      action: 'pop_sound_mix',
      mixName: 'SM_TestMix'
    },
    expected: 'success'
  },
  {
    scenario: 'Set Base Sound Mix',
    toolName: 'manage_audio',
    arguments: {
      action: 'set_base_sound_mix',
      mixName: 'SM_TestMix'
    },
    expected: 'success'
  },

  // === SOUND CLASS (create_sound_class, set_sound_mix_class_override, clear_sound_mix_class_override) ===
  {
    scenario: 'Create Sound Class',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_sound_class',
      name: 'SoundClass_TestClass'
    },
    expected: 'success'
  },
  {
    scenario: 'Set Sound Mix Class Override',
    toolName: 'manage_audio',
    arguments: {
      action: 'set_sound_mix_class_override',
      mixName: 'SM_TestMix',
      soundClassName: 'SoundClass_TestClass',
      volume: 0.5,
      pitch: 1.0
    },
    expected: 'success'
  },
  {
    scenario: 'Clear Sound Mix Class Override',
    toolName: 'manage_audio',
    arguments: {
      action: 'clear_sound_mix_class_override',
      mixName: 'SM_TestMix',
      soundClassName: 'SoundClass_TestClass'
    },
    expected: 'success'
  },

  // === FADING (fade_sound, fade_sound_in, fade_sound_out) ===
  {
    scenario: 'Fade Sound',
    toolName: 'manage_audio',
    arguments: {
      action: 'fade_sound',
      componentName: 'AudioComponent_1',
      targetVolume: 0.2,
      fadeTime: 1.0
    },
    expected: 'success|not_found'
  },
  {
    scenario: 'Fade Sound In',
    toolName: 'manage_audio',
    arguments: {
      action: 'fade_sound_in',
      componentName: 'AudioComponent_1',
      fadeInTime: 2.0,
      targetVolume: 1.0
    },
    expected: 'success|not_found'
  },
  {
    scenario: 'Fade Sound Out',
    toolName: 'manage_audio',
    arguments: {
      action: 'fade_sound_out',
      componentName: 'AudioComponent_1',
      fadeOutTime: 1.5
    },
    expected: 'success|not_found'
  },

  // === AMBIENT SOUND (create_ambient_sound) ===
  {
    scenario: 'Create Ambient Sound',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_ambient_sound',
      name: 'AmbientSound_Test',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 0, y: 0, z: 100 }
    },
    expected: 'success'
  },

  // === REVERB (create_reverb_zone) ===
  {
    scenario: 'Create Reverb Zone',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_reverb_zone',
      name: 'ReverbZone_Test',
      location: { x: 0, y: 0, z: 0 },
      size: { x: 500, y: 500, z: 300 }
    },
    expected: 'success'
  },

  // === ATTENUATION (set_sound_attenuation) ===
  {
    scenario: 'Set Sound Attenuation',
    toolName: 'manage_audio',
    arguments: {
      action: 'set_sound_attenuation',
      name: 'Attenuation_Test',
      falloffDistance: 2000,
      attenuationShape: 'Sphere'
    },
    expected: 'success'
  },

  // === AUDIO ANALYSIS (enable_audio_analysis) ===
  {
    scenario: 'Enable Audio Analysis',
    toolName: 'manage_audio',
    arguments: {
      action: 'enable_audio_analysis',
      enabled: true,
      fftSize: 1024
    },
    expected: 'success'
  },
  {
    scenario: 'Disable Audio Analysis',
    toolName: 'manage_audio',
    arguments: {
      action: 'enable_audio_analysis',
      enabled: false
    },
    expected: 'success'
  },

  // === EFFECTS (set_doppler_effect, set_audio_occlusion) ===
  {
    scenario: 'Set Doppler Effect',
    toolName: 'manage_audio',
    arguments: {
      action: 'set_doppler_effect',
      enabled: true,
      scale: 1.5
    },
    expected: 'success'
  },
  {
    scenario: 'Set Audio Occlusion',
    toolName: 'manage_audio',
    arguments: {
      action: 'set_audio_occlusion',
      enabled: true,
      lowPassFilterFrequency: 5000
    },
    expected: 'success'
  },

  // === PRIME SOUND ===
  {
    scenario: 'Prime Sound',
    toolName: 'manage_audio',
    arguments: {
      action: 'prime_sound',
      soundPath: '/Engine/EngineSounds/WhiteNoise'
    },
    expected: 'success'
  },

  // === ERROR CASES ===
  {
    scenario: 'Error: Play Non-Existent Sound',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_2d',
      soundPath: '/Game/NonExistentSound'
    },
    expected: 'error|not_found'
  },
  {
    scenario: 'Error: Create Cue Invalid Path',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_sound_cue',
      name: 'SC_Invalid',
      soundPath: '/Game/InvalidSound'
    },
    expected: 'error|not_found'
  },
  {
    scenario: 'Error: Missing Sound Path',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_2d'
    },
    expected: 'error|missing'
  },
  {
    scenario: 'Error: Missing Name for Cue',
    toolName: 'manage_audio',
    arguments: {
      action: 'create_sound_cue',
      soundPath: '/Engine/EngineSounds/WhiteNoise'
    },
    expected: 'error|missing'
  },

  // === EDGE CASES ===
  {
    scenario: 'Edge: Volume 0 (silent)',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_2d',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      volume: 0
    },
    expected: 'success'
  },
  {
    scenario: 'Edge: Negative Pitch',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_2d',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      pitch: -1.0
    },
    expected: 'success|clamped'
  },
  {
    scenario: 'Edge: Very High Pitch',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_2d',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      pitch: 10.0
    },
    expected: 'success'
  },
  {
    scenario: 'Edge: Very Low Volume',
    toolName: 'manage_audio',
    arguments: {
      action: 'play_sound_2d',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      volume: 0.001
    },
    expected: 'success'
  },

  // === CLEANUP ===
  {
    scenario: 'Cleanup Audio Assets',
    toolName: 'manage_asset',
    arguments: {
      action: 'delete',
      assetPaths: [
        '/Game/Audio/SC_TestCue',
        '/Game/Audio/SC_TestCue3D',
        '/Game/Audio/SM_TestMix',
        '/Game/Audio/SoundClass_TestClass'
      ]
    },
    expected: 'success|handled'
  }
];

await runToolTests('Audio Management', testCases);
