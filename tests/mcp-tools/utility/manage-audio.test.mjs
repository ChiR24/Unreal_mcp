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
const DOPPLER_CUE_NAME = `TestDopplerCue_${ts}`;
const DOPPLER_CUE = `${TEST_FOLDER}/${DOPPLER_CUE_NAME}`;

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
{ scenario: 'Setup: add sine node to metasound via nodeClassName', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeClassName: 'UE.Sine.Audio' }, expected: 'success', captureResult: { key: 'classNameSineNodeId', fromField: 'result.nodeId' } },
{ scenario: 'Setup: add gain node to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'gain' }, expected: 'success', captureResult: { key: 'gainNodeId', fromField: 'result.nodeId' } },
{ scenario: 'Setup: add alias add node to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'add', save: true }, expected: 'success', captureResult: { key: 'aliasAddNodeId', fromField: 'result.nodeId' } },
{ scenario: 'Setup: add alias gain node to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'gain' }, expected: 'success', captureResult: { key: 'aliasGainNodeId', fromField: 'result.nodeId' } },
{ scenario: 'Setup: add input to metasound', toolName: 'manage_audio', arguments: { action: 'add_metasound_input', assetPath: METASOUND, inputName: 'TestFrequency', inputType: 'Float', defaultValue: 220, save: true }, expected: 'success|already exists' },

// === CREATE ===
{ scenario: 'CREATE: create_sound_cue', toolName: 'manage_audio', arguments: { action: 'create_sound_cue', name: `Testsound_cue_${ts}`, path: '/Game/MCPTest', wavePath: SOUND_WAVE, looping: true, volume: 0.8, pitch: 1.1, save: false }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_sound_cue with a missing wave is refused', toolName: 'manage_audio', arguments: { action: 'create_sound_cue', name: `TestMissingWaveCue_${ts}`, path: '/Game/MCPTest', wavePath: `/Game/MCPTest/NoSuchWave_${ts}` }, expected: 'error|WAVE_NOT_FOUND' },
{ scenario: 'CREATE: create_audio_component', toolName: 'manage_audio', arguments: { action: 'create_audio_component', soundPath: SOUND_CUE }, expected: 'success' },
// autoPlay is read by the TS playback handler and forwarded to the bridge payload.
{ scenario: 'CREATE: create_audio_component with autoPlay', toolName: 'manage_audio', arguments: { action: 'create_audio_component', soundPath: SOUND_CUE, actorName: FADE_ACTOR_NAME, componentName: `MCPTestAutoPlayAudio_${ts}`, autoPlay: true, volume: 0.7, pitch: 1.2 }, expected: 'success' },
{ scenario: 'CREATE: create_audio_component at a location, not playing', toolName: 'manage_audio', arguments: { action: 'create_audio_component', soundPath: SOUND_CUE, location: [100, 0, 50], rotation: { pitch: 0, yaw: 90, roll: 0 }, autoPlay: false }, expected: 'success' },
{ scenario: 'CREATE: create_audio_component on a missing actor is refused', toolName: 'manage_audio', arguments: { action: 'create_audio_component', soundPath: SOUND_CUE, actorName: `MissingAudioActor_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },
{ scenario: 'CREATE: create_sound_mix', toolName: 'manage_audio', arguments: { action: 'create_sound_mix', name: `Testsound_mix_${ts}`, path: '/Game/MCPTest' }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_sound_class', toolName: 'manage_audio', arguments: { action: 'create_sound_class', name: `Testsound_class_${ts}`, path: '/Game/MCPTest', volume: 0.9, pitch: 1.05, save: false }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_sound_class under a parent', toolName: 'manage_audio', arguments: { action: 'create_sound_class', name: `TestChildSoundClass_${ts}`, path: '/Game/MCPTest', parentClass: SOUND_CLASS }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.parentClass', includes: 'TestSoundClass', label: 'parent class applied' }] },

// === PLAYBACK === (uses ResolveSoundAsset - accepts package path)
{ scenario: 'PLAYBACK: play_sound_at_location', toolName: 'manage_audio', arguments: { action: 'play_sound_at_location', soundPath: SOUND_CUE, location: { x: 0, y: 0, z: 0 }, rotation: { pitch: 0, yaw: 45, roll: 0 }, startTime: 0.0, attenuationPath: ATTENUATION }, expected: 'success' },
{ scenario: 'PLAYBACK: play_sound_at_location takes an [x, y, z] location', toolName: 'manage_audio', arguments: { action: 'play_sound_at_location', soundPath: SOUND_CUE, location: [100, 200, 300], rotation: [0, 90, 0] }, expected: 'success', assertions: [{ path: 'structuredContent.result.location.x', equals: 100, label: 'array location reached the handler' }] },
{ scenario: 'PLAYBACK: play_sound_2d', toolName: 'manage_audio', arguments: { action: 'play_sound_2d', soundPath: SOUND_CUE }, expected: 'success' },
{ scenario: 'PLAYBACK: stop_sound stops what play_sound_2d started from this sound', toolName: 'manage_audio', arguments: { action: 'stop_sound', soundPath: SOUND_CUE }, expected: 'success' },
{ scenario: 'PLAYBACK: stop_sound all silences every audio device', toolName: 'manage_audio', arguments: { action: 'stop_sound', all: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.allStopped', equals: true, label: 'every device stopped' }] },
{ scenario: 'PLAYBACK: play_sound_attached', toolName: 'manage_audio', arguments: { action: 'play_sound_attached', soundPath: SOUND_CUE, actorName: FADE_ACTOR_NAME, attachPointName: 'McpAudioRoot', componentName: `MCPTestAttachedAudio_${ts}`, volume: 0.6, pitch: 0.9 }, expected: 'success', assertions: [{ path: 'structuredContent.result.attachedTo', equals: 'McpAudioRoot', label: 'attached to the named component' }] },
{ scenario: 'PLAYBACK: play_sound_attached to a missing attach point is refused', toolName: 'manage_audio', arguments: { action: 'play_sound_attached', soundPath: SOUND_CUE, actorName: FADE_ACTOR_NAME, attachPointName: `NoSuchSocket_${ts}` }, expected: 'error|ATTACH_POINT_NOT_FOUND' },
{ scenario: 'CREATE: spawn_sound_at_location', toolName: 'manage_audio', arguments: { action: 'spawn_sound_at_location', soundPath: SOUND_CUE, location: [0, 0, 100], rotation: { pitch: 0, yaw: 30, roll: 0 }, name: `MCPTestSpawnedSound_${ts}` }, expected: 'success' },
{ scenario: 'ACTION: prime_sound', toolName: 'manage_audio', arguments: { action: 'prime_sound', soundPath: SOUND_CUE }, expected: 'success' },

// === SOUND MIX CONTROL === (uses ResolveSoundMix/Class - accepts simple names)
{ scenario: 'ACTION: push_sound_mix', toolName: 'manage_audio', arguments: { action: 'push_sound_mix', mixName: 'TestSoundMix' }, expected: 'success' },
{ scenario: 'ACTION: pop_sound_mix', toolName: 'manage_audio', arguments: { action: 'pop_sound_mix', mixName: 'TestSoundMix' }, expected: 'success' },
{ scenario: 'CONFIG: set_sound_mix_class_override', toolName: 'manage_audio', arguments: { action: 'set_sound_mix_class_override', mixName: 'TestSoundMix', soundClassName: 'TestSoundClass', volume: 0.8, pitch: 1.0, fadeTime: 0.25 }, expected: 'success' },
{ scenario: 'DELETE: clear_sound_mix_class_override', toolName: 'manage_audio', arguments: { action: 'clear_sound_mix_class_override', mixName: 'TestSoundMix', soundClassName: 'TestSoundClass', fadeOutTime: 0.25 }, expected: 'success' },
{ scenario: 'CONFIG: set_base_sound_mix', toolName: 'manage_audio', arguments: { action: 'set_base_sound_mix', mixName: 'TestSoundMix' }, expected: 'success' },

// === SOUND FADING === (C++ handler now accepts componentName for targeted search)
{ scenario: 'ACTION: fade_sound_in', toolName: 'manage_audio', arguments: { action: 'fade_sound_in', soundName: FADE_ACTOR_NAME, componentName: '${captured:audioComponentName}', fadeInTime: 1.0, targetVolume: 0.9 }, expected: 'success', assertions: [{ path: 'structuredContent.result.fadeTime', equals: 1, label: 'fadeInTime applied' }] },
{ scenario: 'ACTION: fade_sound_out', toolName: 'manage_audio', arguments: { action: 'fade_sound_out', soundName: FADE_ACTOR_NAME, componentName: '${captured:audioComponentName}', fadeOutTime: 0.5, targetVolume: 0.2 }, expected: 'success', assertions: [{ path: 'structuredContent.result.targetVolume', equals: 0.2, label: 'fade-out target volume applied' }] },
// fade_sound: C++ reads soundName (preferred) or actorName
{ scenario: 'ACTION: fade_sound', toolName: 'manage_audio', arguments: { action: 'fade_sound', soundName: FADE_ACTOR_NAME, componentName: '${captured:audioComponentName}', fadeTime: 1.0, targetVolume: 0.5, fadeType: 'FadeTo' }, expected: 'success' },

// === AMBIENT & REVERB ===
{ scenario: 'CREATE: create_ambient_sound', toolName: 'manage_audio', arguments: { action: 'create_ambient_sound', soundPath: SOUND_CUE, location: { x: 0, y: 0, z: 0 }, name: `MCPTestAmbient_${ts}`, volume: 0.5, pitch: 1.0, attenuationPath: ATTENUATION }, expected: 'success' },
{ scenario: 'CREATE: create_reverb_zone', toolName: 'manage_audio', arguments: { action: 'create_reverb_zone', name: `Testreverb_zone_${ts}`, location: { x: 0, y: 0, z: 0 }, size: [800, 600, 400], reverbEffect: REVERB_EFFECT, volume: 0.8, fadeTime: 1.0 }, expected: 'success|already exists' },

// === ATTENUATION ===
{ scenario: 'CONFIG: set_sound_attenuation', toolName: 'manage_audio', arguments: { action: 'set_sound_attenuation', name: `TestSetAttenuation_${ts}`, innerRadius: 400, falloffDistance: 3600, attenuationShape: 'Box', falloffMode: 'Inverse', path: TEST_FOLDER, save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.existsAfter', equals: true, label: 'attenuation asset configured in editor asset registry' }, { path: 'structuredContent.result.attenuationShape', equals: 'Box', label: 'attenuation shape applied' }, { path: 'structuredContent.result.falloffMode', equals: 'Inverse', label: 'falloff mode applied' }] },

// === TOGGLE ===

// === CONFIG ===
{ scenario: 'CONFIG: set_audio_occlusion', toolName: 'manage_audio', arguments: { action: 'set_audio_occlusion', soundPath: SOUND_CUE, enable: true, occlusionVolumeScale: 0.5, occlusionFilterScale: 0.5, occlusionInterpolationTime: 0.1 }, expected: 'success' },

// === SOUND CUE AUTHORING === (uses StaticLoadObject - TS normalizes paths)
{ scenario: 'ADD: add_cue_node', toolName: 'manage_audio', arguments: { action: 'add_cue_node', assetPath: SOUND_CUE, nodeType: 'modulator', volume: 0.8, pitch: 1.1 }, expected: 'success|already exists' },
{ scenario: 'ADD: add_cue_node wave player with a wave', toolName: 'manage_audio', arguments: { action: 'add_cue_node', assetPath: SOUND_CUE, nodeType: 'wave_player', wavePath: SOUND_WAVE }, expected: 'success' },
{ scenario: 'ADD: add_cue_node looping', toolName: 'manage_audio', arguments: { action: 'add_cue_node', assetPath: SOUND_CUE, nodeType: 'looping', indefinite: false, loopCount: 2 }, expected: 'success' },
{ scenario: 'ADD: add_cue_node attenuation', toolName: 'manage_audio', arguments: { action: 'add_cue_node', assetPath: SOUND_CUE, nodeType: 'attenuation', attenuationPath: ATTENUATION }, expected: 'success' },
{ scenario: 'ADD: add_cue_node delay', toolName: 'manage_audio', arguments: { action: 'add_cue_node', assetPath: SOUND_CUE, nodeType: 'delay', delay: 0.25, save: false }, expected: 'success' },

// connect_cue_nodes: C++ uses graph pin linking + CompileSoundNodesFromGraphNodes
// UE assigns node names like SoundNodeRandom_0 for the first Random node
{ scenario: 'CONNECT: connect_cue_nodes', toolName: 'manage_audio', arguments: { action: 'connect_cue_nodes', assetPath: SOUND_CUE, sourceNodeId: 'SoundNodeRandom_0', targetNodeId: 'SoundNodeWavePlayer_0', childIndex: 0, save: false }, expected: 'success' },

{ scenario: 'CONFIG: set_cue_attenuation', toolName: 'manage_audio', arguments: { action: 'set_cue_attenuation', assetPath: SOUND_CUE, attenuationPath: ATTENUATION, save: false }, expected: 'success' },
// set_doppler_effect: a Doppler node goes above the cue's root; a second call updates it in place.
{ scenario: 'Setup: cue that plays a wave for Doppler', toolName: 'manage_audio', arguments: { action: 'create_sound_cue', name: DOPPLER_CUE_NAME, path: TEST_FOLDER, wavePath: SOUND_WAVE }, expected: 'success' },
{ scenario: 'CONFIG: set_doppler_effect inserts a Doppler root', toolName: 'manage_audio', arguments: { action: 'set_doppler_effect', assetPath: DOPPLER_CUE, dopplerIntensity: 1.5, smoothing: true, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.inserted', equals: true, label: 'Doppler node inserted' }, { path: 'structuredContent.result.rootNodeClass', equals: 'SoundNodeDoppler', label: 'Doppler is the cue root' }, { path: 'structuredContent.result.drivesNode', includes: 'SoundNodeWavePlayer', label: 'Doppler plays the old root' }, { path: 'structuredContent.result.atRoot', equals: true, label: 'reported at the root' }] },
{ scenario: 'CONFIG: set_doppler_effect again updates the same node', toolName: 'manage_audio', arguments: { action: 'set_doppler_effect', assetPath: DOPPLER_CUE, dopplerIntensity: 0 }, expected: 'success', assertions: [{ path: 'structuredContent.result.inserted', equals: false, label: 'no second Doppler node' }, { path: 'structuredContent.result.dopplerIntensity', equals: 0, label: 'intensity updated' }] },
{ scenario: 'CONFIG: set_doppler_effect on a cue that plays nothing is refused', toolName: 'manage_audio', arguments: { action: 'set_doppler_effect', assetPath: SOUND_CUE }, expected: 'error|CUE_EMPTY' },
{ scenario: 'CONFIG: set_doppler_effect on a MetaSound is refused', toolName: 'manage_audio', arguments: { action: 'set_doppler_effect', assetPath: METASOUND }, expected: 'error|DOPPLER_NEEDS_SOUND_CUE' },
{ scenario: 'CONFIG: set_cue_concurrency', toolName: 'manage_audio', arguments: { action: 'set_cue_concurrency', assetPath: SOUND_CUE, concurrencyPath: '/Game/MCPTest/AuthoringAssets/MissingConcurrency.MissingConcurrency' }, expected: 'success' },

// === METASOUND AUTHORING ===
{ scenario: 'CREATE: create_metasound', toolName: 'manage_audio', arguments: { action: 'create_metasound', name: `Testmetasound_${ts}`, path: '/Game/MCPTest', save: true }, expected: 'success|already exists' },

// add_metasound_node: C++ maps 'add' → {UE, Add, Float}
{ scenario: 'ADD: add_metasound_node', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', assetPath: METASOUND, nodeType: 'add' }, expected: 'success', captureResult: { key: 'addNodeId', fromField: 'result.nodeId' } },

// connect_metasound_nodes: C++ requires GUID node IDs from add_metasound_node responses
// Capture real GUIDs from setup and use them for the connection test
{ scenario: 'CONNECT: connect_metasound_nodes', toolName: 'manage_audio', arguments: { action: 'connect_metasound_nodes', assetPath: METASOUND, sourceNodeId: '${captured:addNodeId}', sourceOutputName: 'Out', targetNodeId: '${captured:gainNodeId}', targetInputName: 'AdditionalOperands' }, expected: 'success' },
{ scenario: 'CONNECT: connect_metasound_nodes without saving', toolName: 'manage_audio', arguments: { action: 'connect_metasound_nodes', assetPath: METASOUND, sourceNodeId: '${captured:aliasAddNodeId}', sourceOutputName: 'Out', targetNodeId: '${captured:aliasGainNodeId}', targetInputName: 'AdditionalOperands', save: false }, expected: 'success' },

{ scenario: 'ADD: add_metasound_output', toolName: 'manage_audio', arguments: { action: 'add_metasound_output', assetPath: METASOUND, outputName: `TestOutput_${ts}`, outputType: 'Audio', save: true }, expected: 'success' },

// set_metasound_default: C++ converts defaultValue to the input's own data type.
{ scenario: 'CONFIG: set_metasound_default', toolName: 'manage_audio', arguments: { action: 'set_metasound_default', assetPath: METASOUND, inputName: 'TestFrequency', defaultValue: 440.0 }, expected: 'success', assertions: [{ path: 'structuredContent.result.dataType', equals: 'Float', label: 'converted to the Float input type' }] },
// nodeId targets a node input instead of a graph input (the sine node's Frequency literal).
{ scenario: 'CONFIG: set_metasound_default on a node input', toolName: 'manage_audio', arguments: { action: 'set_metasound_default', assetPath: METASOUND, nodeId: '${captured:sineNodeId}', inputName: 'Frequency', defaultValue: 660, save: false }, expected: 'success' },
{ scenario: 'BATCH: build_metasound adds, sets and wires in one call', toolName: 'manage_audio', arguments: { action: 'build_metasound', assetPath: METASOUND, save: true, operations: [{ edit: 'add_node', id: 'osc', nodeClassName: 'UE.Sine.Audio' }, { edit: 'set_default', nodeId: '$osc', inputName: 'Frequency', defaultValue: 880 }, { edit: 'add_node', id: 'gain', nodeType: 'multiply_audio' }, { edit: 'connect', from: '$osc.Audio', to: '$gain.PrimaryOperand' }] }, expected: 'success' },
// get_metasound_graph: the graph as data (nodes with literals, links by pin name), not the document export text.
{ scenario: 'READ: get_metasound_graph lists nodes, links and interface', toolName: 'manage_audio', arguments: { action: 'get_metasound_graph', assetPath: METASOUND }, expected: 'success', assertions: [{ path: 'structuredContent.result.nodeCount', gte: 4, label: 'nodes listed' }, { path: 'structuredContent.result.edges', includesObject: { toPin: 'AdditionalOperands' }, label: 'links named by pin' }, { path: 'structuredContent.result.graphOutputs', minLength: 1, label: 'graph outputs listed' }] },
// disconnect_metasound_nodes: with both ends only that exact link goes; an unlinked input is refused.
{ scenario: 'EDIT: disconnect_metasound_nodes removes one link', toolName: 'manage_audio', arguments: { action: 'disconnect_metasound_nodes', assetPath: METASOUND, sourceNodeId: '${captured:addNodeId}', sourceOutputName: 'Out', targetNodeId: '${captured:gainNodeId}', targetInputName: 'AdditionalOperands' }, expected: 'success', assertions: [{ path: 'structuredContent.result.edgesRemoved', equals: 1, label: 'one link removed' }] },
{ scenario: 'EDIT: disconnect_metasound_nodes on an unlinked input is refused', toolName: 'manage_audio', arguments: { action: 'disconnect_metasound_nodes', assetPath: METASOUND, targetNodeId: '${captured:gainNodeId}', targetInputName: 'AdditionalOperands' }, expected: 'error|NOT_CONNECTED' },
{ scenario: 'EDIT: disconnect_metasound_nodes by source output', toolName: 'manage_audio', arguments: { action: 'disconnect_metasound_nodes', assetPath: METASOUND, sourceNodeId: '${captured:aliasAddNodeId}', sourceOutputName: 'Out', save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.saved', equals: false, label: 'left unsaved' }] },
// remove_metasound_node: nodes with every link on them; an unknown id is refused and nothing is removed.
{ scenario: 'EDIT: remove_metasound_node refuses an unknown id and removes nothing', toolName: 'manage_audio', arguments: { action: 'remove_metasound_node', assetPath: METASOUND, nodeIds: ['${captured:addNodeId}', 'NoSuchNode_Zz'] }, expected: 'error|NODE_NOT_REMOVABLE' },
{ scenario: 'EDIT: remove_metasound_node', toolName: 'manage_audio', arguments: { action: 'remove_metasound_node', assetPath: METASOUND, nodeId: '${captured:addNodeId}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.removedCount', equals: 1, label: 'one node removed' }] },
{ scenario: 'EDIT: remove_metasound_node with nodeIds', toolName: 'manage_audio', arguments: { action: 'remove_metasound_node', assetPath: METASOUND, nodeIds: ['${captured:aliasAddNodeId}', '${captured:aliasGainNodeId}'], save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.removedCount', equals: 2, label: 'both nodes removed' }] },
{ scenario: 'BATCH: build_metasound disconnect and remove_node steps', toolName: 'manage_audio', arguments: { action: 'build_metasound', assetPath: METASOUND, save: false, operations: [{ edit: 'add_node', id: 'tmpAdd', nodeType: 'add' }, { edit: 'add_node', id: 'tmpGain', nodeType: 'gain' }, { edit: 'connect', from: '$tmpAdd.Out', to: '$tmpGain.AdditionalOperands' }, { edit: 'disconnect', targetNodeId: '$tmpGain', targetInputName: 'AdditionalOperands' }, { edit: 'remove_node', nodeIds: ['$tmpAdd', '$tmpGain'] }] }, expected: 'success' },

// === SOUND CLASS & MIX AUTHORING ===
{ scenario: 'CONFIG: set_class_properties', toolName: 'manage_audio', arguments: { action: 'set_class_properties', assetPath: SOUND_CLASS, volume: 0.8, pitch: 1.0, lowPassFilterFrequency: 18000, lfeBleed: 0.4, voiceCenterChannelVolume: 0.2 }, expected: 'success', assertions: [{ path: 'structuredContent.result.lowPassFilterFrequency', equals: 18000, label: 'low-pass filter frequency applied' }] },
// set_class_parent: the C++ reads parentClass (a SoundClass asset); a class that does not load is refused.
{ scenario: 'CONFIG: set_class_parent', toolName: 'manage_audio', arguments: { action: 'set_class_parent', assetPath: SOUND_CLASS, parentClass: `/Game/MCPTest/Testsound_class_${ts}`, save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.parentPath', includes: `Testsound_class_${ts}`, label: 'parent class applied' }] },
{ scenario: 'CONFIG: set_class_parent to a missing class is refused', toolName: 'manage_audio', arguments: { action: 'set_class_parent', assetPath: SOUND_CLASS, parentClass: `/Game/MCPTest/NoSuchClass_${ts}` }, expected: 'error|PARENT_NOT_FOUND' },
{ scenario: 'ADD: add_mix_modifier', toolName: 'manage_audio', arguments: { action: 'add_mix_modifier', assetPath: SOUND_MIX, soundClassPath: SOUND_CLASS, volumeAdjuster: 0.8, pitchAdjuster: 1.1, applyToChildren: false, fadeInTime: 0.5, fadeOutTime: 0.75, save: false }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.fadeOutTime', equals: 0.75, label: 'mix fade-out applied' }] },
{ scenario: 'CONFIG: configure_mix_eq', toolName: 'manage_audio', arguments: { action: 'configure_mix_eq', assetPath: SOUND_MIX, applyEQ: true, eqPriority: 2, lowFrequency: 200, lowGain: 1.5, midFrequency: 1000, midGain: 1.0, highMidFrequency: 4000, highMidGain: 0.8, highFrequency: 10000, highGain: 1.2 }, expected: 'success', assertions: [{ path: 'structuredContent.result.eqSettings.frequencyCenter0', equals: 200, label: 'low band applied' }] },
{ scenario: 'CONFIG: configure_mix_eq through eqSettings', toolName: 'manage_audio', arguments: { action: 'configure_mix_eq', assetPath: SOUND_MIX, eqSettings: { frequencyCenter3: 12000, gain3: 1.1 }, save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.eqSettings.frequencyCenter3', equals: 12000, label: 'eqSettings band applied' }] },

// === ATTENUATION AUTHORING ===
{ scenario: 'CREATE: create_attenuation_settings', toolName: 'manage_audio', arguments: { action: 'create_attenuation_settings', name: `Testattenuation_settings_${ts}`, path: '/Game/MCPTest', innerRadius: 300, falloffDistance: 2000, save: false }, expected: 'success|already exists' },
{ scenario: 'CONFIG: configure_distance_attenuation', toolName: 'manage_audio', arguments: { action: 'configure_distance_attenuation', assetPath: ATTENUATION, innerRadius: 400, falloffDistance: 3600, distanceAlgorithm: 'Logarithmic', save: false }, expected: 'success' },
{ scenario: 'CONFIG: configure_spatialization', toolName: 'manage_audio', arguments: { action: 'configure_spatialization', assetPath: ATTENUATION, spatialize: true, spatialization: 'Binaural', save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.spatializationAlgorithm', equals: 'HRTF', label: 'binaural algorithm applied' }] },
{ scenario: 'CONFIG: configure_occlusion', toolName: 'manage_audio', arguments: { action: 'configure_occlusion', assetPath: ATTENUATION, enable: false, occlusionVolumeScale: 0.4, occlusionFilterScale: 0.5, occlusionInterpolationTime: 0.2, save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.enableOcclusion', equals: false, label: 'enable:false reached the asset' }, { path: 'structuredContent.result.occlusionLowPassFilterFrequency', equals: 10000, label: 'filter scale converted to a cutoff' }] },

// configure_reverb_send: C++ reads enableReverbSend, reverbWetLevelMin, reverbWetLevelMax, etc.
{ scenario: 'CONFIG: configure_reverb_send', toolName: 'manage_audio', arguments: { action: 'configure_reverb_send', assetPath: ATTENUATION, enableReverbSend: true, reverbWetLevelMin: 0.3, reverbWetLevelMax: 0.95, reverbDistanceMin: 100, reverbDistanceMax: 5000, save: false }, expected: 'success' },

// === DIALOGUE ===
{ scenario: 'CREATE: create_dialogue_voice', toolName: 'manage_audio', arguments: { action: 'create_dialogue_voice', name: `Testdialogue_voice_${ts}`, path: '/Game/MCPTest', gender: 'Feminine', plurality: 'Plural', save: false }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_dialogue_wave', toolName: 'manage_audio', arguments: { action: 'create_dialogue_wave', name: `Testdialogue_wave_${ts}`, path: '/Game/MCPTest', spokenText: 'Hello there', wavePath: SOUND_WAVE, speakerPath: DIALOGUE_VOICE, save: false }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.contextCount', equals: 1, label: 'first context mapping filled' }] },
{ scenario: 'CONFIG: set_dialogue_context', toolName: 'manage_audio', arguments: { action: 'set_dialogue_context', assetPath: DIALOGUE_WAVE, speakerPath: DIALOGUE_VOICE, targetVoices: [DIALOGUE_VOICE], soundWavePath: SOUND_WAVE, localizationKeyFormat: '{ContextHash}', replace: true, save: false }, expected: 'success' },

// === EFFECTS ===
{ scenario: 'CREATE: create_reverb_effect', toolName: 'manage_audio', arguments: { action: 'create_reverb_effect', name: `Testreverb_effect_${ts}`, path: '/Game/MCPTest', density: 0.8, diffusion: 0.9, gain: 0.3, gainHF: 0.8, decayTime: 2.5, decayHFRatio: 0.7, save: false }, expected: 'success|already exists' },
{ scenario: 'CREATE: create_source_effect_chain', toolName: 'manage_audio', arguments: { action: 'create_source_effect_chain', name: `Testsource_effect_chain_${ts}`, path: '/Game/MCPTest', save: false }, expected: 'success|already exists' },

// add_source_effect: Now routes through authoring handler.
// C++ authoring handler reads assetPath + effectType. Creates preset internally.
{ scenario: 'ADD: add_source_effect', toolName: 'manage_audio', arguments: { action: 'add_source_effect', assetPath: SOURCE_EFFECT_CHAIN, effectType: 'EQ', bypass: true, save: false }, expected: 'success' },
{ scenario: 'ADD: add_source_effect with a missing preset is refused', toolName: 'manage_audio', arguments: { action: 'add_source_effect', assetPath: SOURCE_EFFECT_CHAIN, effectPresetPath: `/Game/MCPTest/NoSuchPreset_${ts}` }, expected: 'error|PRESET_NOT_FOUND' },

{ scenario: 'CREATE: create_submix_effect', toolName: 'manage_audio', arguments: { action: 'create_submix_effect', name: `Testsubmix_effect_${ts}`, path: '/Game/MCPTest', effectType: 'Reverb', save: false }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.effectCount', equals: 1, label: 'reverb preset in the submix chain' }] },
{ scenario: 'CREATE: create_submix_effect with an unknown effect is refused', toolName: 'manage_audio', arguments: { action: 'create_submix_effect', name: `TestBadSubmix_${ts}`, path: '/Game/MCPTest', effectType: 'Flanger' }, expected: 'error|UNSUPPORTED_EFFECT_TYPE' },

// === INFO ===
{ scenario: 'INFO: get_audio_info', toolName: 'manage_audio', arguments: { action: 'get_audio_info', assetPath: SOUND_CUE }, expected: 'success' },
// params envelope: clients that cannot send arbitrary top-level fields nest them
// under `params`, which is merged with top-level arguments before routing.
{ scenario: 'INFO: get_audio_info via params envelope', toolName: 'manage_audio', arguments: { action: 'get_audio_info', params: { assetPath: SOUND_CUE } }, expected: 'success' },

// === CLEANUP ===
// In-memory assets may not unload properly — DELETE_FAILED is acceptable
{ scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found|DELETE_FAILED' },
];

runToolTests('manage-audio', testCases);
