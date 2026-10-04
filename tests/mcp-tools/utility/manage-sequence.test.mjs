#!/usr/bin/env node
/**
 * manage_sequence Tool Integration Tests
 * Exercises real LevelSequence creation, binding, playback, tracks, keyframes, and metadata.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
const ts = Date.now();

const SEQUENCE_NAME = `SEQ_Test_${ts}`;
const SEQUENCE_PATH = `${TEST_FOLDER}/${SEQUENCE_NAME}`;
const DUPLICATE_NAME = `SEQ_Test_Duplicate_${ts}`;
const DUPLICATE_PATH = `${TEST_FOLDER}/${DUPLICATE_NAME}`;
const RENAMED_NAME = `SEQ_Test_Renamed_${ts}`;
const RENAMED_PATH = `${TEST_FOLDER}/${RENAMED_NAME}`;
const ACTOR_A = `SeqActorA_${ts}`;
const ACTOR_B = `SeqActorB_${ts}`;
const TRACK_TYPE = '/Script/MovieSceneTracks.MovieSceneEventTrack';
const TRACK_NAME = 'MovieSceneEventTrack';
const AUDIO_TRACK_NAME = 'SeqMusic';
const SOUND_WAVE = '/Engine/VREditor/Sounds/VR_click1.VR_click1';
const FOLDER_DELETE_TEST_FOLDER = `${TEST_FOLDER}/LevelSequenceFolderDelete_${ts}`;
const FOLDER_DELETE_SEQUENCE_NAME = `SEQ_FolderDelete_${ts}`;
const FOLDER_DELETE_SEQUENCE_PATH = `${FOLDER_DELETE_TEST_FOLDER}/${FOLDER_DELETE_SEQUENCE_NAME}`;

// === CINEMATICS (L1) CASE STATE ===
const MASTER_NAME = `SEQ_Master_${ts}`;
const MASTER_NAME_2 = `SEQ_MasterB_${ts}`;
const MASTER_PATH = `${TEST_FOLDER}/${MASTER_NAME}`;
const MASTER_PATH_2 = `${TEST_FOLDER}/${MASTER_NAME_2}`;
const SUB_PATH = `${SEQUENCE_PATH}_Sub_${ts}`;
const SUB_PATH_2 = `${SEQUENCE_PATH}_SubB_${ts}`;
const CINE_CAM = `CineCam_${ts}`;
const CINE_CAM_2 = `CineCamB_${ts}`;
const RIG_NAME = `CameraRig_${ts}`;
const CRANE_NAME = `CameraCrane_${ts}`;
const SHAKE_PATH = '/Engine/Sequencer/DefaultCameraShake.DefaultCameraShake';
const MAT_PATH = '/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial';
const ANIM_PATH = '/Game/Animations/SequenceAnim.Default';

// === RECORD REPLAY / TAKE RECORDER (L4/L5) CASE STATE ===
const TAKE_SEQ_NAME = `SEQ_Take_${ts}`;
const TAKE_SEQ_PATH = `${TEST_FOLDER}/${TAKE_SEQ_NAME}`;
const TAKE_ACTOR = `TakeActor_${ts}`;
const DEMO_NAME = `McpReplay_${ts}`;
const TAKE_PRESET_PATH = '/Game/TakePresets/DefaultTakePreset.DefaultTakePreset';

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: spawn sequence actor A', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: ACTOR_A, location: { x: 0, y: 0, z: 100 } }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn sequence actor B', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: ACTOR_B, location: { x: 200, y: 0, z: 100 } }, expected: 'success|already exists' },

  // === CREATE / OPEN ===
  { scenario: 'ACTION: create', toolName: 'manage_sequence', arguments: { action: 'create', name: SEQUENCE_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'ACTION: open', toolName: 'manage_sequence', arguments: { action: 'open', path: SEQUENCE_PATH }, expected: 'success' },

  // === BINDINGS ===
  { scenario: 'ADD: add_camera', toolName: 'manage_sequence', arguments: { action: 'add_camera', path: SEQUENCE_PATH, spawnable: true }, expected: 'success|already exists' },
  { scenario: 'ADD: add_actor', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: SEQUENCE_PATH, actorName: ACTOR_A }, expected: 'success|already exists', captureResult: { key: 'actorBindingId', fromField: 'result.bindingGuid' } },
  { scenario: 'ADD: add_actors', toolName: 'manage_sequence', arguments: { action: 'add_actors', path: SEQUENCE_PATH, actorNames: [ACTOR_A, ACTOR_B] }, expected: 'success|already exists' },
  { scenario: 'INFO: get_bindings', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: SEQUENCE_PATH }, expected: 'success' },

  // === PLAYBACK ===
  { scenario: 'PLAYBACK: play', toolName: 'manage_sequence', arguments: { action: 'play', path: SEQUENCE_PATH, startTime: 0, loopMode: 'once' }, expected: 'success' },
  { scenario: 'PLAYBACK: pause', toolName: 'manage_sequence', arguments: { action: 'pause', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'PLAYBACK: pause holds a frame', toolName: 'manage_sequence', arguments: { action: 'pause', path: SEQUENCE_PATH, startTime: 0.5 }, expected: 'success' },
  { scenario: 'PLAYBACK: stop', toolName: 'manage_sequence', arguments: { action: 'stop', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'CONFIG: set_playback_speed', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', path: SEQUENCE_PATH, speed: 1.25 }, expected: 'success' },

  // === PROPERTIES / KEYFRAMES ===
  { scenario: 'ADD: add_keyframe', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: SEQUENCE_PATH, actorName: ACTOR_A, property: 'Location', frame: 12, value: { x: 100, y: 50, z: 150 } }, expected: 'success' },
  // bindingId is parsed by ReadBindingGuid (Cinematics.cpp:113) as the binding to key against.
  { scenario: 'ADD: add_keyframe via bindingId', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: SEQUENCE_PATH, actorName: ACTOR_A, bindingId: '${captured:actorBindingId}', property: 'Location', frame: 24, value: { x: 10, y: 20, z: 30 } }, expected: 'success' },
  // lookAt aims the key from its location; linear keys run at a steady rate.
  { scenario: 'ADD: add_keyframe aimed with lookAt, linear', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: SEQUENCE_PATH, actorName: ACTOR_A, property: 'Transform', frame: 36, interpolation: 'linear', value: { location: { x: 300, y: 0, z: 200 }, lookAt: { x: 0, y: 0, z: 100 } } }, expected: 'success' },
  { scenario: 'ERROR: add_keyframe with an unknown interpolation', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: SEQUENCE_PATH, actorName: ACTOR_A, property: 'Location', frame: 40, interpolation: 'bezier', value: { x: 0, y: 0, z: 0 } }, expected: 'error' },
  // Visibility keys a Visibility track, which really hides the actor in renders.
  { scenario: 'ADD: add_keyframe Visibility hides the actor', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: SEQUENCE_PATH, actorName: ACTOR_A, property: 'Visibility', frame: 30, value: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.message', equals: 'Visibility Keyframe added' }] },
  // The harness merges args.params into the call arguments before routing.
  { scenario: 'PARAMS: get_properties via nested params', toolName: 'manage_sequence', arguments: { action: 'get_properties', params: { path: SEQUENCE_PATH } }, expected: 'success' },
  { scenario: 'INFO: get_properties', toolName: 'manage_sequence', arguments: { action: 'get_properties', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'CONFIG: set_properties', toolName: 'manage_sequence', arguments: { action: 'set_properties', path: SEQUENCE_PATH, frameRate: 24, playbackStart: 0, playbackEnd: 120 }, expected: 'success' },
  { scenario: 'CONFIG: set_properties lengthInFrames', toolName: 'manage_sequence', arguments: { action: 'set_properties', path: SEQUENCE_PATH, playbackStart: 12, lengthInFrames: 36 }, expected: 'success', assertions: [{ path: 'structuredContent.result.playbackStart', equals: 12 }, { path: 'structuredContent.result.playbackEnd', equals: 48 }, { path: 'structuredContent.result.duration', equals: 36 }] },
  { scenario: 'CONFIG: set_display_rate', toolName: 'manage_sequence', arguments: { action: 'set_display_rate', path: SEQUENCE_PATH, frameRate: '24fps' }, expected: 'success' },
  { scenario: 'CONFIG: set_tick_resolution', toolName: 'manage_sequence', arguments: { action: 'set_tick_resolution', path: SEQUENCE_PATH, resolution: '24000/1' }, expected: 'success' },
  { scenario: 'CONFIG: set_work_range', toolName: 'manage_sequence', arguments: { action: 'set_work_range', path: SEQUENCE_PATH, start: 0, end: 5 }, expected: 'success' },
  { scenario: 'CONFIG: set_view_range', toolName: 'manage_sequence', arguments: { action: 'set_view_range', path: SEQUENCE_PATH, start: 0, end: 5 }, expected: 'success' },

  // === METADATA / LISTING ===
  { scenario: 'ACTION: list', toolName: 'manage_sequence', arguments: { action: 'list', path: TEST_FOLDER }, expected: 'success' },
  { scenario: 'INFO: get_metadata', toolName: 'manage_sequence', arguments: { action: 'get_metadata', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'CONFIG: set_metadata', toolName: 'manage_sequence', arguments: { action: 'set_metadata', path: SEQUENCE_PATH, metadata: { owner: 'mcp', suite: 'manage_sequence', run: ts } }, expected: 'success' },
  { scenario: 'CONFIG: set_metadata single key', toolName: 'manage_sequence', arguments: { action: 'set_metadata', path: SEQUENCE_PATH, key: 'reviewed', value: true }, expected: 'success' },
  { scenario: 'INFO: get_metadata by key', toolName: 'manage_sequence', arguments: { action: 'get_metadata', path: SEQUENCE_PATH, key: 'reviewed' }, expected: 'success', assertions: [{ path: 'structuredContent.result.found', equals: true }, { path: 'structuredContent.result.value', equals: 'true' }] },

  // === TRACKS ===
  { scenario: 'ADD: add_spawnable_from_class', toolName: 'manage_sequence', arguments: { action: 'add_spawnable_from_class', path: SEQUENCE_PATH, className: 'CameraActor' }, expected: 'success|already exists' },
  { scenario: 'ADD: add_track', toolName: 'manage_sequence', arguments: { action: 'add_track', path: SEQUENCE_PATH, trackType: TRACK_TYPE, trackName: TRACK_NAME }, expected: 'success|already exists' },
  { scenario: 'ADD: add_section', toolName: 'manage_sequence', arguments: { action: 'add_section', path: SEQUENCE_PATH, trackName: TRACK_NAME, start: 0, end: 48 }, expected: 'success|already exists' },
  { scenario: 'ADD: add_section refuses an empty range', toolName: 'manage_sequence', arguments: { action: 'add_section', path: SEQUENCE_PATH, trackName: TRACK_NAME, actorName: ACTOR_A, start: 48, end: 48 }, expected: 'error' },
  { scenario: 'ADD: add_track Audio (music or sound)', toolName: 'manage_sequence', arguments: { action: 'add_track', path: SEQUENCE_PATH, trackType: 'Audio', trackName: AUDIO_TRACK_NAME }, expected: 'success', assertions: [{ path: 'structuredContent.result.trackClass', equals: 'MovieSceneAudioTrack', label: 'Audio resolves to the audio track' }] },
  { scenario: 'ADD: add_section soundPath puts the sound in the audio track, as long as the sound', toolName: 'manage_sequence', arguments: { action: 'add_section', path: SEQUENCE_PATH, trackName: AUDIO_TRACK_NAME, start: 24, soundPath: SOUND_WAVE }, expected: 'success', assertions: [{ path: 'structuredContent.result.soundName', equals: 'VR_click1', label: 'reply names the sound' }] },
  { scenario: 'ADD: add_section soundPath with an explicit end', toolName: 'manage_sequence', arguments: { action: 'add_section', path: SEQUENCE_PATH, trackName: AUDIO_TRACK_NAME, start: 0, end: 48, soundPath: SOUND_WAVE }, expected: 'success', assertions: [{ path: 'structuredContent.result.endFrame', equals: 48, label: 'the given end is kept' }] },
  { scenario: 'ADD: add_section with a missing sound is refused', toolName: 'manage_sequence', arguments: { action: 'add_section', path: SEQUENCE_PATH, trackName: AUDIO_TRACK_NAME, start: 0, soundPath: `/Game/MCPTest/NoSuchSound_${ts}` }, expected: 'error|ASSET_NOT_FOUND' },
  { scenario: 'ADD: add_section soundPath on a non-audio track is refused', toolName: 'manage_sequence', arguments: { action: 'add_section', path: SEQUENCE_PATH, trackName: TRACK_NAME, start: 0, end: 48, soundPath: SOUND_WAVE }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CONFIG: set_track_muted', toolName: 'manage_sequence', arguments: { action: 'set_track_muted', path: SEQUENCE_PATH, trackName: TRACK_NAME, muted: true }, expected: 'success' },
  { scenario: 'CONFIG: set_track_solo', toolName: 'manage_sequence', arguments: { action: 'set_track_solo', path: SEQUENCE_PATH, trackName: TRACK_NAME, solo: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.solo', equals: true, label: 'set_track_solo reports enabled state' }] },
  { scenario: 'CONFIG: set_track_locked', toolName: 'manage_sequence', arguments: { action: 'set_track_locked', path: SEQUENCE_PATH, trackName: TRACK_NAME, locked: true }, expected: 'success' },
  { scenario: 'INFO: list_tracks', toolName: 'manage_sequence', arguments: { action: 'list_tracks', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'DELETE: remove_track', toolName: 'manage_sequence', arguments: { action: 'remove_track', path: SEQUENCE_PATH, trackName: TRACK_NAME }, expected: 'success|not found' },
  { scenario: 'INFO: list_track_types', toolName: 'manage_sequence', arguments: { action: 'list_track_types' }, expected: 'success' },

  // === CINEMATICS TRACKS / RIG (L1) — close parameter-combination coverage gaps ===
  // create_master_sequence
  { scenario: 'CINEMATICS: create_master_sequence', toolName: 'manage_sequence', arguments: { action: 'create_master_sequence', name: MASTER_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: create_master_sequence optional', toolName: 'manage_sequence', arguments: { action: 'create_master_sequence', name: MASTER_NAME_2, path: TEST_FOLDER, assetPath: MASTER_PATH_2, save: true }, expected: 'success|already exists' },
  // add_subsequence
  { scenario: 'CINEMATICS: add_subsequence', toolName: 'manage_sequence', arguments: { action: 'add_subsequence', path: SEQUENCE_PATH, subsequencePath: SUB_PATH }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_subsequence optional', toolName: 'manage_sequence', arguments: { action: 'add_subsequence', path: MASTER_PATH, subsequencePath: SUB_PATH_2, rowIndex: 0, startFrame: 10, endFrame: 70, save: true }, expected: 'success|already exists' },
  // add_shot_track
  { scenario: 'CINEMATICS: add_shot_track', toolName: 'manage_sequence', arguments: { action: 'add_shot_track', path: SEQUENCE_PATH, shotSequencePath: MASTER_PATH, displayName: 'Shot_01' }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_shot_track optional', toolName: 'manage_sequence', arguments: { action: 'add_shot_track', path: SEQUENCE_PATH, shotSequencePath: MASTER_PATH_2, displayName: 'ShotTwo', startFrame: 100, durationFrames: 100, endFrame: 200, rowIndex: 1, save: true }, expected: 'success|already exists' },
  // configure_shot_settings
  { scenario: 'CINEMATICS: configure_shot_settings', toolName: 'manage_sequence', arguments: { action: 'configure_shot_settings', path: SEQUENCE_PATH, shotName: 'Shot_01', displayName: 'ShotOne', sectionIndex: 0, durationFrames: 120, save: true }, expected: 'success' },
  // Adds the frame RANGE the record's summary names and its own exampleInput uses
  // (start/end). This case previously repeated the base call argument-for-argument,
  // so it covered nothing the line above did not.
  { scenario: 'CINEMATICS: configure_shot_settings with an explicit frame range', toolName: 'manage_sequence', arguments: { action: 'configure_shot_settings', path: SEQUENCE_PATH, sectionIndex: 0, startFrame: 0, endFrame: 120, rowIndex: 0, save: true }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_shot_settings by section name', toolName: 'manage_sequence', arguments: { action: 'configure_shot_settings', path: SEQUENCE_PATH, sectionName: 'ShotOne', displayName: 'Shot 1' }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_shot_settings by shot sequence', toolName: 'manage_sequence', arguments: { action: 'configure_shot_settings', path: SEQUENCE_PATH, shotSequencePath: MASTER_PATH_2, displayName: 'Shot 2' }, expected: 'success' },
  // create_cine_camera_actor
  { scenario: 'CINEMATICS: create_cine_camera_actor', toolName: 'manage_sequence', arguments: { action: 'create_cine_camera_actor', path: SEQUENCE_PATH, actorName: CINE_CAM }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: create_cine_camera_actor optional', toolName: 'manage_sequence', arguments: { action: 'create_cine_camera_actor', path: SEQUENCE_PATH, label: CINE_CAM_2, location: { x: 0, y: 0, z: 200 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, currentFocalLength: 35, currentAperture: 2.8, sensorWidth: 36, sensorHeight: 24, manualFocusDistance: 500, lens: { focalLength: 35 }, filmback: { sensorWidth: 36 }, focus: { manualFocusDistance: 500 } }, expected: 'success|already exists' },
  // configure_camera_settings
  { scenario: 'CINEMATICS: configure_camera_settings', toolName: 'manage_sequence', arguments: { action: 'configure_camera_settings', cameraName: CINE_CAM, aperture: 2.8 }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_camera_settings optional', toolName: 'manage_sequence', arguments: { action: 'configure_camera_settings', cameraName: CINE_CAM, actorName: CINE_CAM, aperture: 4.0, focalLength: 50, focusDistance: 1000, sensorHeight: 24, sensorWidth: 36, currentAperture: 2.8, currentFocalLength: 35, manualFocusDistance: 750, lens: { focalLength: 50 }, filmback: { sensorWidth: 36, sensorHeight: 24 }, focus: { focusDistance: 1000 } }, expected: 'success' },
  // add_camera_cut_track
  { scenario: 'CINEMATICS: add_camera_cut_track', toolName: 'manage_sequence', arguments: { action: 'add_camera_cut_track', path: SEQUENCE_PATH, cameraName: CINE_CAM }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_camera_cut_track optional', toolName: 'manage_sequence', arguments: { action: 'add_camera_cut_track', path: SEQUENCE_PATH, actorPath: CINE_CAM, rowIndex: 0, startFrame: 45, endFrame: 90, save: true }, expected: 'success|already exists' },
  // add_camera_shake_track
  { scenario: 'CINEMATICS: add_camera_shake_track', toolName: 'manage_sequence', arguments: { action: 'add_camera_shake_track', path: SEQUENCE_PATH, cameraShakePath: SHAKE_PATH }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_camera_shake_track bound to a camera', toolName: 'manage_sequence', arguments: { action: 'add_camera_shake_track', path: SEQUENCE_PATH, cameraShakeClass: SHAKE_PATH, cameraName: CINE_CAM, startFrame: 0, durationFrames: 30, endFrame: 30, rowIndex: 0, save: true }, expected: 'success|already exists' },
  // configure_camera_rig_rail
  { scenario: 'CINEMATICS: configure_camera_rig_rail', toolName: 'manage_sequence', arguments: { action: 'configure_camera_rig_rail', positionOnRail: 50 }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_camera_rig_rail optional', toolName: 'manage_sequence', arguments: { action: 'configure_camera_rig_rail', actorName: RIG_NAME, label: 'RailRig', positionOnRail: 0.5 }, expected: 'success' },
  // configure_camera_rig_crane
  { scenario: 'CINEMATICS: configure_camera_rig_crane', toolName: 'manage_sequence', arguments: { action: 'configure_camera_rig_crane', craneArmLength: 300 }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_camera_rig_crane optional', toolName: 'manage_sequence', arguments: { action: 'configure_camera_rig_crane', actorName: CRANE_NAME, cranePitch: 10, craneYaw: 45, label: 'CraneRig' }, expected: 'success' },
  // add_fade_track
  { scenario: 'CINEMATICS: add_fade_track', toolName: 'manage_sequence', arguments: { action: 'add_fade_track', path: SEQUENCE_PATH }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_fade_track optional', toolName: 'manage_sequence', arguments: { action: 'add_fade_track', path: SEQUENCE_PATH, from: 0, to: 1, startFrame: 12, endFrame: 72, rowIndex: 1, save: true }, expected: 'success|already exists' },
  // add_level_visibility_track
  { scenario: 'CINEMATICS: add_level_visibility_track', toolName: 'manage_sequence', arguments: { action: 'add_level_visibility_track', path: SEQUENCE_PATH, levelNames: ['/Game/Levels/Level01'] }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_level_visibility_track optional', toolName: 'manage_sequence', arguments: { action: 'add_level_visibility_track', path: SEQUENCE_PATH, levelNames: ['/Game/Levels/Level01'], visibility: 'Visible', startFrame: 0, endFrame: 90, rowIndex: 1, save: true }, expected: 'success|already exists' },
  // add_material_parameter_track
  { scenario: 'CINEMATICS: add_material_parameter_track', toolName: 'manage_sequence', arguments: { action: 'add_material_parameter_track', path: SEQUENCE_PATH, actorName: ACTOR_A, materialPath: MAT_PATH, parameterName: 'Color', value: { r: 1, g: 0, b: 0, a: 1 } }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_material_parameter_track optional', toolName: 'manage_sequence', arguments: { action: 'add_material_parameter_track', path: SEQUENCE_PATH, bindingGuid: '${captured:actorBindingId}', componentName: 'StaticMeshComponent0', materialPath: MAT_PATH, materialIndex: 0, parameterName: 'Roughness', value: 0.5, startFrame: 24, save: true }, expected: 'success|already exists' },
  // add_particle_track
  // A cube is bound by actorName (no bindingGuid needed) and then refused because it carries no FX component.
  { scenario: 'CINEMATICS: add_particle_track by actorName', toolName: 'manage_sequence', arguments: { action: 'add_particle_track', path: SEQUENCE_PATH, actorName: ACTOR_A }, expected: 'error|PARTICLE_BINDING_REQUIRED' },
  { scenario: 'CINEMATICS: add_particle_track optional', toolName: 'manage_sequence', arguments: { action: 'add_particle_track', path: SEQUENCE_PATH, activate: true, startFrame: 0, durationFrames: 120, endFrame: 120, rowIndex: 2, bindingGuid: '${captured:actorBindingId}', save: true }, expected: 'error|PARTICLE_BINDING_REQUIRED' },
  // add_skeletal_animation_track
  { scenario: 'CINEMATICS: add_skeletal_animation_track refuses an actor with no skeletal mesh', toolName: 'manage_sequence', arguments: { action: 'add_skeletal_animation_track', path: SEQUENCE_PATH, actorName: ACTOR_A, animationSequencePath: ANIM_PATH }, expected: 'error' },
  { scenario: 'CINEMATICS: add_skeletal_animation_track refuses a component that is not a skeletal mesh', toolName: 'manage_sequence', arguments: { action: 'add_skeletal_animation_track', path: SEQUENCE_PATH, actorName: ACTOR_A, componentName: 'StaticMeshComponent0', animationSequencePath: ANIM_PATH }, expected: 'error' },
  { scenario: 'CINEMATICS: add_skeletal_animation_track optional', toolName: 'manage_sequence', arguments: { action: 'add_skeletal_animation_track', path: SEQUENCE_PATH, animationPath: ANIM_PATH, bindingGuid: '${captured:actorBindingId}', startFrame: 0, durationFrames: 48, endFrame: 48, rowIndex: 0, save: true }, expected: 'success|already exists' },
  // add_transform_track
  { scenario: 'CINEMATICS: add_transform_track', toolName: 'manage_sequence', arguments: { action: 'add_transform_track', path: SEQUENCE_PATH, actorName: ACTOR_A }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_transform_track optional', toolName: 'manage_sequence', arguments: { action: 'add_transform_track', path: SEQUENCE_PATH, bindingGuid: '${captured:actorBindingId}', startFrame: 24, durationFrames: 24, endFrame: 48, rowIndex: 1, save: true }, expected: 'success|already exists' },
  // add_event_track
  { scenario: 'CINEMATICS: add_event_track', toolName: 'manage_sequence', arguments: { action: 'add_event_track', path: SEQUENCE_PATH, actorName: ACTOR_A }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_event_track optional', toolName: 'manage_sequence', arguments: { action: 'add_event_track', path: SEQUENCE_PATH, bindingGuid: '${captured:actorBindingId}', startFrame: 0, durationFrames: 60, endFrame: 60, rowIndex: 1, save: true }, expected: 'success|already exists' },
  // add_property_track
  { scenario: 'CINEMATICS: add_property_track', toolName: 'manage_sequence', arguments: { action: 'add_property_track', path: SEQUENCE_PATH, actorName: ACTOR_A, property: 'bHidden', propertyType: 'bool' }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_property_track optional', toolName: 'manage_sequence', arguments: { action: 'add_property_track', path: SEQUENCE_PATH, actorName: ACTOR_A, property: 'Location', propertyName: 'Location', propertyPath: 'Location', startFrame: 0, durationFrames: 48, endFrame: 48, rowIndex: 2, save: true }, expected: 'success|already exists' },

  // === CINEMATICS (L1) CLEANUP ===
  { scenario: 'Cleanup: delete master sequence B', toolName: 'manage_asset', arguments: { action: 'delete', path: MASTER_PATH_2, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete master sequence', toolName: 'manage_asset', arguments: { action: 'delete', path: MASTER_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete cine camera actors', toolName: 'control_actor', arguments: { action: 'delete', actorName: CINE_CAM }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete cine camera actor B', toolName: 'control_actor', arguments: { action: 'delete', actorName: CINE_CAM_2 }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete camera rig actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: RIG_NAME }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete camera crane actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: CRANE_NAME }, expected: 'success|not found' },

  // === DUPLICATE / RENAME / DELETE ===
  { scenario: 'ACTION: duplicate', toolName: 'manage_sequence', arguments: { action: 'duplicate', path: SEQUENCE_PATH, destinationPath: TEST_FOLDER, newName: DUPLICATE_NAME }, expected: 'success' },
  { scenario: 'ACTION: rename', toolName: 'manage_sequence', arguments: { action: 'rename', path: DUPLICATE_PATH, newName: RENAMED_NAME }, expected: 'success' },
  { scenario: 'DELETE: delete', toolName: 'manage_sequence', arguments: { action: 'delete', path: RENAMED_PATH }, expected: 'success|not found' },
  { scenario: 'DELETE: remove_actors', toolName: 'manage_sequence', arguments: { action: 'remove_actors', path: SEQUENCE_PATH, actorNames: [ACTOR_A, ACTOR_B] }, expected: 'success|not found' },

  // === FOLDER DELETE REGRESSION ===
  { scenario: 'Setup: create LevelSequence folder-delete folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: FOLDER_DELETE_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create LevelSequence folder-delete asset', toolName: 'manage_sequence', arguments: { action: 'create', name: FOLDER_DELETE_SEQUENCE_NAME, path: FOLDER_DELETE_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Regression: delete folder containing LevelSequence asset', toolName: 'manage_asset', arguments: { action: 'delete', path: FOLDER_DELETE_TEST_FOLDER, force: true }, expected: 'success', assertions: [{ path: 'structuredContent.data.result.success', equals: true }, { path: 'structuredContent.data.result.existsAfter', equals: false }] },
  { scenario: 'Regression: LevelSequence asset removed by folder delete', toolName: 'manage_asset', arguments: { action: 'exists', assetPath: FOLDER_DELETE_SEQUENCE_PATH }, expected: 'success', assertions: [{ path: 'structuredContent.data.result.exists', equals: false }] },

  // === RECORD REPLAY / TAKE RECORDER (L4/L5) ===
  // Dependencies: a dedicated Level Sequence and a spawned actor so the Take
  // Recorder panel can bind a real source before recording.
  { scenario: 'Setup: create Take Recorder sequence', toolName: 'manage_sequence', arguments: { action: 'create', name: TAKE_SEQ_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn Take Recorder actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TAKE_ACTOR, location: { x: 0, y: 0, z: 50 } }, expected: 'success|already exists' },

  // create_take_recorder_panel (no payload params)
  { scenario: 'RECORDREPLAY: create_take_recorder_panel', toolName: 'manage_sequence', arguments: { action: 'create_take_recorder_panel' }, expected: 'success' },

  // configure_take_sources (actorName / clearSources / reduceKeys / recordParentHierarchy / recordType)
  { scenario: 'RECORDREPLAY: configure_take_sources', toolName: 'manage_sequence', arguments: { action: 'configure_take_sources', sequencePath: TAKE_SEQ_PATH, actorName: TAKE_ACTOR, clearSources: false, actors: [TAKE_ACTOR], recordParentHierarchy: true, reduceKeys: true, recordType: '0', recordingSequencePath: TAKE_SEQ_PATH, takeSequencePath: TAKE_SEQ_PATH, frameRate: 30, recordInto: false }, expected: 'success' },
  // takePresetPath seeds a new take from the preset (no sequence path alongside it).
  { scenario: 'RECORDREPLAY: configure_take_sources from a take preset', toolName: 'manage_sequence', arguments: { action: 'configure_take_sources', takePresetPath: TAKE_PRESET_PATH, actorNames: [TAKE_ACTOR] }, expected: 'success|not found' },

  // configure_recorded_tracks (properties / trackNames / tracks / enabled / disableOthers / reduceKeys / recordParentHierarchy / recordType)
  { scenario: 'RECORDREPLAY: configure_recorded_tracks', toolName: 'manage_sequence', arguments: { action: 'configure_recorded_tracks', sequencePath: TAKE_SEQ_PATH, actorName: TAKE_ACTOR, actorNames: [TAKE_ACTOR], frameRate: 30, recordInto: false, properties: ['RelativeLocation'], trackNames: ['Transform'], tracks: ['Transform'], enabled: true, disableOthers: false, recordParentHierarchy: true, reduceKeys: true, recordType: '0' }, expected: 'success' },

  // start_recording (sequence paths, inline sources, duration auto-stop)
  { scenario: 'RECORDREPLAY: start_recording', toolName: 'manage_sequence', arguments: { action: 'start_recording', sequencePath: TAKE_SEQ_PATH, recordingSequencePath: TAKE_SEQ_PATH, takeSequencePath: TAKE_SEQ_PATH, frameRate: 30, recordInto: false, duration: 30, actorName: TAKE_ACTOR, actorNames: [TAKE_ACTOR], sourceActors: [TAKE_ACTOR], sourceClasses: [], clearSources: true, reduceKeys: true, recordParentHierarchy: false, recordType: '0' }, expected: 'success' },

  // stop_recording (no payload params)
  { scenario: 'RECORDREPLAY: stop_recording', toolName: 'manage_sequence', arguments: { action: 'stop_recording' }, expected: 'success' },

  // configure_demo_settings (demoName / friendlyName / additionalOptions / checkpointSaveMaxMSPerFrame / maxRecordTimeSeconds / playbackSpeed / loadDefaultMapOnStop) — no PIE required
  { scenario: 'RECORDREPLAY: configure_demo_settings', toolName: 'manage_sequence', arguments: { action: 'configure_demo_settings', replayName: DEMO_NAME, prioritizeActors: true, friendlyName: 'McpReplayDemo', additionalOptions: ['-windowed'], checkpointSaveMaxMSPerFrame: 10, maxRecordTimeSeconds: 30, playbackSpeed: 1.0, loadDefaultMapOnStop: false }, expected: 'success' },

  // configure_killcam_duration (durationSeconds / duration / endTime) — no PIE required
  { scenario: 'RECORDREPLAY: configure_killcam_duration', toolName: 'manage_sequence', arguments: { action: 'configure_killcam_duration', durationSeconds: 5.0, duration: 5.0 }, expected: 'success' },

  // Demo replay actions below require an active PIE / game world; in an
  // editor-only run they return controlled errors (NOT_IN_PIE / NOT_PLAYING /
  // NOT_RECORDING). Expectation primary is `error`.
  // start_demo_recording (demoName / friendlyName / additionalOptions / maxRecordTimeSeconds / loadDefaultMapOnStop)
  { scenario: 'RECORDREPLAY: start_demo_recording', toolName: 'manage_sequence', arguments: { action: 'start_demo_recording', demoName: DEMO_NAME, friendlyName: 'McpReplayDemo', additionalOptions: ['-windowed'], maxRecordTimeSeconds: 30, loadDefaultMapOnStop: false }, expected: 'error' },
  // stop_demo_recording (no payload params)
  { scenario: 'RECORDREPLAY: stop_demo_recording', toolName: 'manage_sequence', arguments: { action: 'stop_demo_recording' }, expected: 'error' },
  // play_demo (demoName / replayName / timeSeconds)
  { scenario: 'RECORDREPLAY: play_demo', toolName: 'manage_sequence', arguments: { action: 'play_demo', demoName: DEMO_NAME, replayName: DEMO_NAME, additionalOptions: ['-windowed'] }, expected: 'error' },
  // pause_demo (demoName / paused)
  { scenario: 'RECORDREPLAY: pause_demo', toolName: 'manage_sequence', arguments: { action: 'pause_demo', demoName: DEMO_NAME, paused: true }, expected: 'error' },
  // seek_demo (demoName / timeSeconds / seconds / seekTime)
  { scenario: 'RECORDREPLAY: seek_demo', toolName: 'manage_sequence', arguments: { action: 'seek_demo', demoName: DEMO_NAME, timeSeconds: 1.0, seconds: 1.0, seekTime: 1.0 }, expected: 'error' },
  // set_demo_playback_speed (demoName / speed / playbackSpeed)
  { scenario: 'RECORDREPLAY: set_demo_playback_speed', toolName: 'manage_sequence', arguments: { action: 'set_demo_playback_speed', demoName: DEMO_NAME, speed: 2.0, playbackSpeed: 2.0 }, expected: 'error' },
  // start_killcam (demoName / replayName / durationSeconds / endTime)
  { scenario: 'RECORDREPLAY: start_killcam', toolName: 'manage_sequence', arguments: { action: 'start_killcam', demoName: DEMO_NAME, replayName: DEMO_NAME, durationSeconds: 4.0, additionalOptions: ['-windowed'] }, expected: 'error' },

  // === RECORD REPLAY (L4/L5) CLEANUP ===
  { scenario: 'Cleanup: delete Take Recorder sequence', toolName: 'manage_asset', arguments: { action: 'delete', path: TAKE_SEQ_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete Take Recorder actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TAKE_ACTOR }, expected: 'success|not found' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete sequence asset', toolName: 'manage_asset', arguments: { action: 'delete', path: SEQUENCE_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete actor A', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR_A }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete actor B', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR_B }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete sequence camera', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'SequenceCamera' }, expected: 'success|not found' },
];

runToolTests('manage-sequence', testCases, { folder: TEST_FOLDER });
