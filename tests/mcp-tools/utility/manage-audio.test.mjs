#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const ts = Date.now();
const FADE_ACTOR_NAME = `MCPTestAudioActor_${ts}`;
const SOUND_CUE = `${TEST_FOLDER}/TestSoundCue`;
const SOUND_CLASS = `${TEST_FOLDER}/TestSoundClass`;
const SOUND_MIX = `${TEST_FOLDER}/TestSoundMix`;
const METASOUND = `${TEST_FOLDER}/TestMetaSound`;
const ATTENUATION = `${TEST_FOLDER}/TestAttenuation`;
const DIALOGUE_VOICE = `${TEST_FOLDER}/TestDialogueVoice`;
const DIALOGUE_WAVE = `${TEST_FOLDER}/TestDialogueWave`;
const REVERB_EFFECT = `${TEST_FOLDER}/TestReverbEffect`;
const SOURCE_EFFECT_CHAIN = `${TEST_FOLDER}/TestSourceEffectChain`;
const SOUND_WAVE = '/Engine/VREditor/Sounds/VR_click1.VR_click1';

const testCases = [
// === SETUP ===
{ scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
{ scenario: 'Setup: create test sound cue', toolName: 'manage_audio', arguments: { action: 'create_sound_cue', name: 'TestSoundCue', path: TEST_FOLDER_ALIAS }, expected: 'success|already exists' },
{ scenario: 'Setup: create test sound class', toolName: 'manage_audio', arguments: { action: 'create_sound_class', name: 'TestSoundClass', path: TEST_FOLDER }, expected: 'success|already exists' },
{ scenario: 'Setup: create test sound mix', toolName: 'manage_audio', arguments: { action: 'create_sound_mix', name: 'TestSoundMix', path: TEST_FOLDER }, expected: 'success|already exists' },
{ scenario: 'Setup: create test metasound', toolName: 'manage_audio', arguments: { action: 'create_metasound', name: 'TestMetaSound', path: TEST_FOLDER_ALIAS }, expected: 'success|already exists' },
{ scenario: 'Setup: create test attenuation settings', toolName: 'manage_audio', arguments: { action: 'create_attenuation_settings', name: 'TestAttenuation', path: TEST_FOLDER }, expected: 'success|already exists' },
{ scenario: 'Setup: create test dialogue voice', toolName: 'manage_audio', arguments: { action: 'create_dialogue_voice', name: 'TestDialogueVoice', path: TEST_FOLDER }, expected: 'success|already exists' },
{ scenario: 'Setup: create test dialogue wave', toolName: 'manage_audio', arguments: { action: 'create_dialogue_wave', name: 'TestDialogueWave', path: TEST_FOLDER }, expected: 'success|already exists' },
{ scenario: 'Setup: create test reverb effect', toolName: 'manage_audio', arguments: { action: 'create_reverb_effect', name: 'TestReverbEffect', path: TEST_FOLDER }, expected: 'success|already exists' },
{ scenario: 'Setup: create test source effect chain', toolName: 'manage_audio', arguments: { action: 'create_source_effect_chain', name: 'TestSourceEffectChain', path: TEST_FOLDER }, expected: 'success|already exists' },

// Spawn actor for fade/attached tests — requires classPath
{ scenario: 'Setup: spawn actor for fade tests', toolName: 'control_actor', arguments: { action: 'spawn', name: FADE_ACTOR_NAME, classPath: '/Script/Engine.Actor', location: [0, 0, 0] }, expected: 'success|already exists' },

// Create AudioComponent on the spawned actor
{ scenario: 'Setup: create audio component on actor', toolName: 'manage_audio', arguments: { action: 'create_audio_component', soundPath: SOUND_CUE, actorName: FADE_ACTOR_NAME }, expected: 'success', captureResult: { key: 'audioComponentName', fromField: 'result.componentName' } },

// Add nodes to SoundCue for connect test
// C++ handler reads nodeType.ToLower() and matches 'random', 'waveplayer', 'modulator', etc.
{ scenario: 'Setup: add random node to sound cue', toolName: 'manage_audio', arguments: { action: 'add_cue_node', assetPath: SOUND_CUE, nodeType: 'random' }, expected: 'success|already exists' },
{ scenario: 'Setup: add wave player node to sound cue', toolName: 'manage_audio', arguments: { action: 'add_cue_node', assetPath: SOUND_CUE, nodeType: 'wave_player' }, expected: 'success|already exists' },

// MetaSound nodes — C++ maps nodeType to 3-part class names:
// 'sine' → {UE, Sine, Audio}, 'multiply'/'gain' → {UE, Multiply, Float}
{ scenario: 'Setup: add sine node to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'sine' }, expected: 'success', captureResult: { key: 'sineNodeId', fromField: 'result.nodeId' } },
{ scenario: 'Setup: add gain node to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'gain' }, expected: 'success', captureResult: { key: 'gainNodeId', fromField: 'result.nodeId' } },
{ scenario: 'Setup: add alias add node to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'add' }, expected: 'success', captureResult: { key: 'aliasAddNodeId', fromField: 'result.nodeId' } },
{ scenario: 'Setup: add alias gain node to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'gain' }, expected: 'success', captureResult: { key: 'aliasGainNodeId', fromField: 'result.nodeId' } },
{ scenario: 'Setup: add input to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_input', assetPath: METASOUND, inputName: 'TestFrequency', inputType: 'Float' }, expected: 'success|already exists' },

// === CREATE ===
{ scenario: 'CREATE: create_sound_cue', toolName: 'manage_audio', arguments: { action: 'create_sound_cue', name: `Testsound_cue_${ts}`, path: '/Game/MCPTest', wavePath: SOUND_WAVE, looping: true }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_audio_component', toolName: 'manage_audio', arguments: { action: 'create_audio_component', soundPath: SOUND_CUE }, expected: 'success' },
{ scenario: 'CREATE: create_sound_mix', toolName: 'manage_audio', arguments: { action: 'create_sound_mix', name: `Testsound_mix_${ts}`, path: '/Game/MCPTest' }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_sound_class', toolName: 'manage_audio', arguments: { action: 'create_sound_class', name: `Testsound_class_${ts}`, path: '/Game/MCPTest', properties: { volume: 0.9, pitch: 1.05 } }, expected: 'success|already exists' },

// === PLAYBACK === (uses ResolveSoundAsset - accepts package path)
{ scenario: 'PLAYBACK: play_sound_at_location', toolName: 'manage_audio', arguments: { action: 'play_sound_at_location', soundPath: SOUND_CUE, location: { x: 0, y: 0, z: 0 }, rotation: { pitch: 0, yaw: 45, roll: 0 }, startTime: 0.0, concurrencyPath: '/Game/MCPTest/AuthoringAssets/MissingConcurrency.MissingConcurrency' }, expected: 'success' },
{ scenario: 'PLAYBACK: play_sound_2d', toolName: 'manage_audio', arguments: { action: 'play_sound_2d', soundPath: SOUND_CUE }, expected: 'success' },
{ scenario: 'PLAYBACK: play_sound_attached', toolName: 'manage_audio', arguments: { action: 'play_sound_attached', soundPath: SOUND_CUE, actorName: FADE_ACTOR_NAME, attachPointName: 'McpAudioRoot' }, expected: 'success' },
{ scenario: 'CREATE: spawn_sound_at_location', toolName: 'manage_audio', arguments: { action: 'spawn_sound_at_location', soundPath: SOUND_CUE, location: [0, 0, 100] }, expected: 'success' },
{ scenario: 'ACTION: prime_sound', toolName: 'manage_audio', arguments: { action: 'prime_sound', soundPath: SOUND_CUE }, expected: 'success' },

// === SOUND MIX CONTROL === (uses ResolveSoundMix/Class - accepts simple names)
{ scenario: 'ACTION: push_sound_mix', toolName: 'manage_audio', arguments: { action: 'push_sound_mix', mixName: 'TestSoundMix' }, expected: 'success' },
{ scenario: 'ACTION: pop_sound_mix', toolName: 'manage_audio', arguments: { action: 'pop_sound_mix', mixName: 'TestSoundMix' }, expected: 'success' },
{ scenario: 'CONFIG: set_sound_mix_class_override', toolName: 'manage_audio', arguments: { action: 'set_sound_mix_class_override', mixName: 'TestSoundMix', soundClassName: 'TestSoundClass', volume: 0.8, pitch: 1.0, fadeInTime: 0.25 }, expected: 'success' },
{ scenario: 'DELETE: clear_sound_mix_class_override', toolName: 'manage_audio', arguments: { action: 'clear_sound_mix_class_override', mixName: 'TestSoundMix', soundClassName: 'TestSoundClass', fadeOutTime: 0.25 }, expected: 'success' },
{ scenario: 'CONFIG: set_base_sound_mix', toolName: 'manage_audio', arguments: { action: 'set_base_sound_mix', mixName: 'TestSoundMix' }, expected: 'success' },

// === SOUND FADING === (C++ handler now accepts componentName for targeted search)
{ scenario: 'ACTION: fade_sound_in', toolName: 'manage_audio', arguments: { action: 'fade_sound_in', actorName: FADE_ACTOR_NAME, componentName: '${captured:audioComponentName}', fadeTime: 1.0 }, expected: 'success' },
{ scenario: 'ACTION: fade_sound_out', toolName: 'manage_audio', arguments: { action: 'fade_sound_out', actorName: FADE_ACTOR_NAME, componentName: '${captured:audioComponentName}', fadeTime: 1.0 }, expected: 'success' },
// fade_sound: C++ reads soundName (preferred) or actorName
{ scenario: 'ACTION: fade_sound', toolName: 'manage_audio', arguments: { action: 'fade_sound', soundName: FADE_ACTOR_NAME, componentName: '${captured:audioComponentName}', fadeTime: 1.0, targetVolume: 0.5, fadeType: 'FadeTo' }, expected: 'success' },

// === AMBIENT & REVERB ===
{ scenario: 'CREATE: create_ambient_sound', toolName: 'manage_audio', arguments: { action: 'create_ambient_sound', soundPath: SOUND_CUE, location: [0, 0, 0] }, expected: 'success' },
{ scenario: 'CREATE: create_reverb_zone', toolName: 'manage_audio', arguments: { action: 'create_reverb_zone', name: `Testreverb_zone_${ts}`, location: [0, 0, 0], size: [500, 500, 500], reverbEffect: REVERB_EFFECT }, expected: 'success|already exists' },

// === ATTENUATION ===
{ scenario: 'CONFIG: set_sound_attenuation', toolName: 'manage_audio', arguments: { action: 'set_sound_attenuation', name: `TestSetAttenuation_${ts}`, innerRadius: 400, falloffDistance: 3600, attenuationShape: 'Box', falloffMode: 'Inverse', path: TEST_FOLDER, save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.existsAfter', equals: true, label: 'attenuation asset configured in editor asset registry' }, { path: 'structuredContent.result.attenuationShape', equals: 'Box', label: 'attenuation shape applied' }, { path: 'structuredContent.result.falloffMode', equals: 'Inverse', label: 'falloff mode applied' }] },

// === TOGGLE ===
{ scenario: 'TOGGLE: enable_audio_analysis', toolName: 'manage_audio', arguments: { action: 'enable_audio_analysis', enable: true }, expected: 'success' },
{ scenario: 'TOGGLE: enable_audio_analysis alias', toolName: 'manage_audio', arguments: { action: 'enable_audio_analysis', enabled: false, analysisType: 'Amplitude', windowSize: 2048 }, expected: 'success' },

// === CONFIG ===
{ scenario: 'CONFIG: set_doppler_effect', toolName: 'manage_audio', arguments: { action: 'set_doppler_effect', dopplerIntensity: 1, velocityScale: 1 }, expected: 'success' },
{ scenario: 'CONFIG: set_audio_occlusion', toolName: 'manage_audio', arguments: { action: 'set_audio_occlusion', enable: true, occlusionVolumeScale: 0.5, occlusionFilterScale: 0.5, occlusionInterpolationTime: 0.1 }, expected: 'success' },

// === SOUND CUE AUTHORING === (uses StaticLoadObject - TS normalizes paths)
{ scenario: 'ADD: add_cue_node', toolName: 'manage_audio', arguments: { action: 'add_cue_node', assetPath: SOUND_CUE, nodeType: 'modulator' }, expected: 'success|already exists' },

// connect_cue_nodes: C++ uses graph pin linking + CompileSoundNodesFromGraphNodes
// UE assigns node names like SoundNodeRandom_0 for the first Random node
{ scenario: 'CONNECT: connect_cue_nodes', toolName: 'manage_audio', arguments: { action: 'connect_cue_nodes', assetPath: SOUND_CUE, sourceNodeId: 'SoundNodeRandom_0', targetNodeId: 'SoundNodeWavePlayer_0' }, expected: 'success' },

{ scenario: 'CONFIG: set_cue_attenuation', toolName: 'manage_audio', arguments: { action: 'set_cue_attenuation', assetPath: SOUND_CUE, attenuationPath: ATTENUATION }, expected: 'success' },
{ scenario: 'CONFIG: set_cue_concurrency', toolName: 'manage_audio', arguments: { action: 'set_cue_concurrency', assetPath: SOUND_CUE, concurrencyPath: '/Game/MCPTest/AuthoringAssets/MissingConcurrency.MissingConcurrency' }, expected: 'success' },

// === METASOUND AUTHORING ===
{ scenario: 'CREATE: create_metasound', toolName: 'manage_audio', arguments: { action: 'create_metasound', name: `Testmetasound_${ts}`, path: '/Game/MCPTest' }, expected: 'success|already exists' },

// add_metasound_node: C++ maps 'add' → {UE, Add, Float}
{ scenario: 'ADD: add_metasound_node', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'add' }, expected: 'success', captureResult: { key: 'addNodeId', fromField: 'result.nodeId' } },

// connect_metasound_nodes: C++ requires GUID node IDs from add_metasound_node responses
// Capture real GUIDs from setup and use them for the connection test
{ scenario: 'CONNECT: connect_metasound_nodes', toolName: 'manage_audio', arguments: { action: 'connect_metasound_nodes', assetPath: METASOUND, sourceNodeId: '${captured:addNodeId}', sourceOutputName: 'Out', targetNodeId: '${captured:gainNodeId}', targetInputName: 'AdditionalOperands' }, expected: 'success' },
{ scenario: 'CONNECT: connect_metasound_nodes aliases', toolName: 'manage_audio', arguments: { action: 'connect_metasound_nodes', assetPath: METASOUND, sourceNode: '${captured:aliasAddNodeId}', sourcePin: 'Out', targetNode: '${captured:aliasGainNodeId}', targetPin: 'AdditionalOperands' }, expected: 'success' },

{ scenario: 'ADD: add_metasound_output', toolName: 'manage_audio', arguments: { action: 'add_metasound_output', assetPath: METASOUND, outputName: `TestOutput_${ts}`, outputType: 'Audio' }, expected: 'success' },

// set_metasound_default: TS remaps defaultValue→floatValue. C++ reads floatValue.
{ scenario: 'CONFIG: set_metasound_default', toolName: 'manage_audio', arguments: { action: 'set_metasound_default', assetPath: METASOUND, inputName: 'TestFrequency', defaultValue: 440.0 }, expected: 'success' },

// === SOUND CLASS & MIX AUTHORING ===
{ scenario: 'CONFIG: set_class_properties', toolName: 'manage_audio', arguments: { action: 'set_class_properties', assetPath: SOUND_CLASS, volume: 0.8, pitch: 1.0, lowPassFilterFrequency: 18000 }, expected: 'success', assertions: [{ path: 'structuredContent.result.lowPassFilterFrequency', equals: 18000, label: 'low-pass filter frequency applied' }] },
// set_class_parent: TS remaps parentClass→parentPath. C++ reads parentPath.
{ scenario: 'CONFIG: set_class_parent', toolName: 'manage_audio', arguments: { action: 'set_class_parent', assetPath: SOUND_CLASS, parentClass: '/Script/Engine.SoundClass' }, expected: 'success' },
{ scenario: 'ADD: add_mix_modifier', toolName: 'manage_audio', arguments: { action: 'add_mix_modifier', assetPath: SOUND_MIX, soundClassPath: SOUND_CLASS, volumeAdjuster: 0.8 }, expected: 'success|already exists' },
{ scenario: 'CONFIG: configure_mix_eq', toolName: 'manage_audio', arguments: { action: 'configure_mix_eq', assetPath: SOUND_MIX }, expected: 'success' },

// === ATTENUATION AUTHORING ===
{ scenario: 'CREATE: create_attenuation_settings', toolName: 'manage_audio', arguments: { action: 'create_attenuation_settings', name: `Testattenuation_settings_${ts}`, path: '/Game/MCPTest' }, expected: 'success|already exists' },
{ scenario: 'CONFIG: configure_distance_attenuation', toolName: 'manage_audio', arguments: { action: 'configure_distance_attenuation', assetPath: ATTENUATION, innerRadius: 400, falloffDistance: 3600 }, expected: 'success' },
// configure_spatialization: TS remaps spatialization→spatialize+spatializationAlgorithm. C++ reads both.
{ scenario: 'CONFIG: configure_spatialization', toolName: 'manage_audio', arguments: { action: 'configure_spatialization', assetPath: ATTENUATION, spatialization: 'HRTF' }, expected: 'success' },
// configure_occlusion: TS remaps enable→enableOcclusion. C++ reads enableOcclusion.
{ scenario: 'CONFIG: configure_occlusion', toolName: 'manage_audio', arguments: { action: 'configure_occlusion', assetPath: ATTENUATION, enable: true }, expected: 'success' },

// configure_reverb_send: C++ reads enableReverbSend, reverbWetLevelMin, reverbWetLevelMax, etc.
{ scenario: 'CONFIG: configure_reverb_send', toolName: 'manage_audio', arguments: { action: 'configure_reverb_send', assetPath: ATTENUATION, enableReverbSend: true, reverbWetLevelMin: 0.3, reverbWetLevelMax: 0.95, reverbDistanceMin: 100, reverbDistanceMax: 5000 }, expected: 'success' },

// === DIALOGUE ===
{ scenario: 'CREATE: create_dialogue_voice', toolName: 'manage_audio', arguments: { action: 'create_dialogue_voice', name: `Testdialogue_voice_${ts}`, path: '/Game/MCPTest' }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_dialogue_wave', toolName: 'manage_audio', arguments: { action: 'create_dialogue_wave', name: `Testdialogue_wave_${ts}`, path: '/Game/MCPTest' }, expected: 'success|already exists' },
{ scenario: 'CONFIG: set_dialogue_context', toolName: 'manage_audio', arguments: { action: 'set_dialogue_context', assetPath: DIALOGUE_WAVE, speakerPath: DIALOGUE_VOICE }, expected: 'success' },

// === EFFECTS ===
{ scenario: 'CREATE: create_reverb_effect', toolName: 'manage_audio', arguments: { action: 'create_reverb_effect', name: `Testreverb_effect_${ts}`, path: '/Game/MCPTest' }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_source_effect_chain', toolName: 'manage_audio', arguments: { action: 'create_source_effect_chain', name: `Testsource_effect_chain_${ts}`, path: '/Game/MCPTest' }, expected: 'success|already exists' },

// add_source_effect: Now routes through authoring handler.
// C++ authoring handler reads assetPath + effectType. Creates preset internally.
{ scenario: 'ADD: add_source_effect', toolName: 'manage_audio', arguments: { action: 'add_source_effect', assetPath: SOURCE_EFFECT_CHAIN, effectType: 'EQ' }, expected: 'success' },

{ scenario: 'CREATE: create_submix_effect', toolName: 'manage_audio', arguments: { action: 'create_submix_effect', name: `Testsubmix_effect_${ts}`, path: '/Game/MCPTest', effectType: 'Reverb' }, expected: 'success|already exists' },

// === INFO ===
{ scenario: 'INFO: get_audio_info', toolName: 'manage_audio', arguments: { action: 'get_audio_info', assetPath: SOUND_CUE }, expected: 'success' },

// === CLEANUP ===
// In-memory assets may not unload properly — DELETE_FAILED is acceptable
{ scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'DELETE_FAILED|success|not found' },
];

runToolTests('manage-audio', testCases);
