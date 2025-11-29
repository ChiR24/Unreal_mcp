import { TestRunner } from './test-runner.mjs';

const runner = new TestRunner();

runner.runTests([
  {
    name: 'Audio: Create Sound Cue',
    tool: 'manage_audio',
    params: {
      action: 'create_sound_cue',
      name: 'TestSoundCue',
      savePath: '/Game/Audio/Tests',
      settings: {
        volume: 0.8,
        pitch: 1.2,
        looping: true
      }
    },
    expected: (result) => result.success === true && result.message.includes('SoundCue created')
  },
  {
    name: 'Audio: Create Sound Class',
    tool: 'manage_audio',
    params: {
      action: 'create_sound_class',
      name: 'TestSoundClass',
      properties: {
        volume: 0.5,
        pitch: 1.0
      }
    },
    expected: (result) => result.success === true
  },
  {
    name: 'Audio: Create Sound Mix',
    tool: 'manage_audio',
    params: {
      action: 'create_sound_mix',
      name: 'TestSoundMix',
      classAdjusters: [
        {
          soundClass: '/Game/Audio/Classes/TestSoundClass',
          volumeAdjuster: 0.5,
          pitchAdjuster: 1.0
        }
      ]
    },
    expected: (result) => result.success === true
  },
  {
    name: 'Audio: Push Sound Mix',
    tool: 'manage_audio',
    params: {
      action: 'push_sound_mix',
      mixName: '/Game/Audio/Mixes/TestSoundMix'
    },
    expected: (result) => result.success === true
  },
  {
    name: 'Audio: Pop Sound Mix',
    tool: 'manage_audio',
    params: {
      action: 'pop_sound_mix',
      mixName: '/Game/Audio/Mixes/TestSoundMix'
    },
    expected: (result) => result.success === true
  },
  {
    name: 'Audio: Create Ambient Sound',
    tool: 'manage_audio',
    params: {
      action: 'create_ambient_sound',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 100, y: 100, z: 100 },
      volume: 0.5,
      pitch: 1.0
    },
    expected: (result) => result.success === true && result.componentName !== undefined
  },
  {
    name: 'Audio: Play Sound At Location',
    tool: 'manage_audio',
    params: {
      action: 'play_sound_at_location',
      soundPath: '/Engine/EngineSounds/WhiteNoise',
      location: { x: 0, y: 0, z: 0 },
      rotation: { pitch: 0, yaw: 45, roll: 0 },
      volume: 1.0,
      pitch: 1.0
    },
    expected: (result) => result.success === true
  }
]);
