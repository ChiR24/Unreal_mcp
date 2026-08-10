#!/usr/bin/env node
/**
 * manage_sequence Tool Integration Tests
 * Exercises real LevelSequence creation, binding, playback, tracks, keyframes, and metadata.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
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
const SKEL_PATH = '/Game/Characters/SequenceSkeleton.Default';

// === RECORD REPLAY / TAKE RECORDER (L4/L5) CASE STATE ===
const TAKE_SEQ_NAME = `SEQ_Take_${ts}`;
const TAKE_SEQ_PATH = `${TEST_FOLDER}/${TAKE_SEQ_NAME}`;
const TAKE_ACTOR = `TakeActor_${ts}`;
const DEMO_NAME = `McpReplay_${ts}`;
const TAKE_PRESET_PATH = '/Game/TakePresets/DefaultTakePreset.DefaultTakePreset';

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn sequence actor A', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: ACTOR_A, location: { x: 0, y: 0, z: 100 } }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn sequence actor B', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: ACTOR_B, location: { x: 200, y: 0, z: 100 } }, expected: 'success|already exists' },

  // === CREATE / OPEN ===
  { scenario: 'ACTION: create', toolName: 'manage_sequence', arguments: { action: 'create', name: SEQUENCE_NAME, path: TEST_FOLDER_ALIAS }, expected: 'success|already exists' },
  { scenario: 'ACTION: open', toolName: 'manage_sequence', arguments: { action: 'open', path: SEQUENCE_PATH }, expected: 'success' },

  // === BINDINGS ===
  { scenario: 'ADD: add_camera', toolName: 'manage_sequence', arguments: { action: 'add_camera', path: SEQUENCE_PATH, spawnable: true }, expected: 'success|already exists' },
  { scenario: 'ADD: add_actor', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: SEQUENCE_PATH, actorName: ACTOR_A }, expected: 'success|already exists', captureResult: { key: 'actorBindingId', fromField: 'result.bindingGuid' } },
  { scenario: 'ADD: add_actors', toolName: 'manage_sequence', arguments: { action: 'add_actors', path: SEQUENCE_PATH, actorNames: [ACTOR_A, ACTOR_B] }, expected: 'success|already exists' },
  { scenario: 'INFO: get_bindings', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: SEQUENCE_PATH }, expected: 'success' },

  // === PLAYBACK ===
  { scenario: 'PLAYBACK: play', toolName: 'manage_sequence', arguments: { action: 'play', path: SEQUENCE_PATH, startTime: 0, loopMode: 'once' }, expected: 'success' },
  { scenario: 'PLAYBACK: pause', toolName: 'manage_sequence', arguments: { action: 'pause', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'PLAYBACK: stop', toolName: 'manage_sequence', arguments: { action: 'stop', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'CONFIG: set_playback_speed', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', path: SEQUENCE_PATH, speed: 1.25 }, expected: 'success' },

  // === PROPERTIES / KEYFRAMES ===
  { scenario: 'ADD: add_keyframe', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: SEQUENCE_PATH, actorName: ACTOR_A, property: 'Location', frame: 12, value: { x: 100, y: 50, z: 150 } }, expected: 'success' },
  // bindingId is parsed by ReadBindingGuid (Cinematics.cpp:113) as the binding to key against.
  { scenario: 'ADD: add_keyframe via bindingId', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: SEQUENCE_PATH, actorName: ACTOR_A, bindingId: '${captured:actorBindingId}', property: 'Location', frame: 24, value: { x: 10, y: 20, z: 30 } }, expected: 'success' },
  // normalizeConsolidatedCall merges args.params into the argument record before routing.
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

  // === TRACKS ===
  { scenario: 'ADD: add_spawnable_from_class', toolName: 'manage_sequence', arguments: { action: 'add_spawnable_from_class', path: SEQUENCE_PATH, className: 'CameraActor' }, expected: 'success|already exists' },
  { scenario: 'ADD: add_track', toolName: 'manage_sequence', arguments: { action: 'add_track', path: SEQUENCE_PATH, trackType: TRACK_TYPE, trackName: TRACK_NAME }, expected: 'success|already exists' },
  { scenario: 'ADD: add_section', toolName: 'manage_sequence', arguments: { action: 'add_section', path: SEQUENCE_PATH, trackName: TRACK_NAME, startFrame: 0, endFrame: 48 }, expected: 'success|already exists' },
  { scenario: 'CONFIG: set_track_muted', toolName: 'manage_sequence', arguments: { action: 'set_track_muted', path: SEQUENCE_PATH, trackName: TRACK_NAME, muted: true }, expected: 'success' },
  { scenario: 'CONFIG: set_track_solo', toolName: 'manage_sequence', arguments: { action: 'set_track_solo', path: SEQUENCE_PATH, trackName: TRACK_NAME, solo: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.solo', equals: true, label: 'set_track_solo reports enabled state' }] },
  { scenario: 'CONFIG: set_track_locked', toolName: 'manage_sequence', arguments: { action: 'set_track_locked', path: SEQUENCE_PATH, trackName: TRACK_NAME, locked: true }, expected: 'success' },
  { scenario: 'INFO: list_tracks', toolName: 'manage_sequence', arguments: { action: 'list_tracks', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'DELETE: remove_track', toolName: 'manage_sequence', arguments: { action: 'remove_track', path: SEQUENCE_PATH, trackName: TRACK_NAME }, expected: 'success|not found' },
  { scenario: 'INFO: list_track_types', toolName: 'manage_sequence', arguments: { action: 'list_track_types' }, expected: 'success' },

  // === CINEMATICS TRACKS / RIG (L1) — close parameter-combination coverage gaps ===
  // create_master_sequence
  { scenario: 'CINEMATICS: create_master_sequence', toolName: 'manage_sequence', arguments: { action: 'create_master_sequence', name: MASTER_NAME, path: TEST_FOLDER_ALIAS }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: create_master_sequence optional', toolName: 'manage_sequence', arguments: { action: 'create_master_sequence', name: MASTER_NAME_2, path: TEST_FOLDER_ALIAS, assetPath: MASTER_PATH_2, save: true }, expected: 'success|already exists' },
  // add_subsequence
  { scenario: 'CINEMATICS: add_subsequence', toolName: 'manage_sequence', arguments: { action: 'add_subsequence', sequencePath: SEQUENCE_PATH, subsequencePath: SUB_PATH }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_subsequence optional', toolName: 'manage_sequence', arguments: { action: 'add_subsequence', sequencePath: SEQUENCE_PATH, subsequencePath: SUB_PATH_2, masterSequencePath: MASTER_PATH, rowIndex: 0, durationFrames: 60, save: true }, expected: 'success|already exists' },
  // add_shot_track
  { scenario: 'CINEMATICS: add_shot_track', toolName: 'manage_sequence', arguments: { action: 'add_shot_track', sequencePath: SEQUENCE_PATH, shotName: 'Shot_01' }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_shot_track optional', toolName: 'manage_sequence', arguments: { action: 'add_shot_track', sequencePath: SEQUENCE_PATH, shotSequencePath: `${SEQUENCE_PATH}_Shot`, displayName: 'ShotOne', durationFrames: 100, rowIndex: 0, save: true }, expected: 'success|already exists' },
  // configure_shot_settings
  { scenario: 'CINEMATICS: configure_shot_settings', toolName: 'manage_sequence', arguments: { action: 'configure_shot_settings', sequencePath: SEQUENCE_PATH, shotName: 'Shot_01', displayName: 'ShotOne', sectionIndex: 0, durationFrames: 120, save: true }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_shot_settings optional', toolName: 'manage_sequence', arguments: { action: 'configure_shot_settings', sequencePath: SEQUENCE_PATH, shotName: 'Shot_01', displayName: 'ShotOne', sectionIndex: 0, durationFrames: 120, save: true }, expected: 'success' },
  // create_cine_camera_actor
  { scenario: 'CINEMATICS: create_cine_camera_actor', toolName: 'manage_sequence', arguments: { action: 'create_cine_camera_actor', sequencePath: SEQUENCE_PATH, cameraName: CINE_CAM }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: create_cine_camera_actor optional', toolName: 'manage_sequence', arguments: { action: 'create_cine_camera_actor', sequencePath: SEQUENCE_PATH, cameraActorName: CINE_CAM_2, label: 'CineCameraB', save: true, location: { x: 0, y: 0, z: 200 }, rotation: { pitch: 0, yaw: 0, roll: 0 } }, expected: 'success|already exists' },
  // configure_camera_settings
  { scenario: 'CINEMATICS: configure_camera_settings', toolName: 'manage_sequence', arguments: { action: 'configure_camera_settings', sequencePath: SEQUENCE_PATH, cameraName: CINE_CAM, aperture: 2.8 }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_camera_settings optional', toolName: 'manage_sequence', arguments: { action: 'configure_camera_settings', sequencePath: SEQUENCE_PATH, cameraName: CINE_CAM, actorName: CINE_CAM, aperture: 4.0, focalLength: 50, focusDistance: 1000, sensorHeight: 24, sensorWidth: 36, currentAperture: 2.8, currentFocalLength: 35, manualFocusDistance: 750, lens: { focalLength: 50 }, filmback: { sensorWidth: 36, sensorHeight: 24 }, focus: { focusDistance: 1000 } }, expected: 'success' },
  // add_camera_cut_track
  { scenario: 'CINEMATICS: add_camera_cut_track', toolName: 'manage_sequence', arguments: { action: 'add_camera_cut_track', sequencePath: SEQUENCE_PATH, cameraName: CINE_CAM }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_camera_cut_track optional', toolName: 'manage_sequence', arguments: { action: 'add_camera_cut_track', sequencePath: SEQUENCE_PATH, cameraActorName: CINE_CAM, actorPath: CINE_CAM, rowIndex: 0, durationFrames: 45, save: true }, expected: 'success|already exists' },
  // add_camera_shake_track
  { scenario: 'CINEMATICS: add_camera_shake_track', toolName: 'manage_sequence', arguments: { action: 'add_camera_shake_track', sequencePath: SEQUENCE_PATH, cameraShakePath: SHAKE_PATH }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_camera_shake_track optional', toolName: 'manage_sequence', arguments: { action: 'add_camera_shake_track', sequencePath: SEQUENCE_PATH, cameraShakeClass: '/Script/Engine.CameraShakeBase', cameraName: CINE_CAM, save: true }, expected: 'success|already exists' },
  // configure_camera_rig_rail
  { scenario: 'CINEMATICS: configure_camera_rig_rail', toolName: 'manage_sequence', arguments: { action: 'configure_camera_rig_rail', sequencePath: SEQUENCE_PATH, positionOnRail: 50 }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_camera_rig_rail optional', toolName: 'manage_sequence', arguments: { action: 'configure_camera_rig_rail', sequencePath: SEQUENCE_PATH, actorName: RIG_NAME, label: 'RailRig', save: true }, expected: 'success' },
  // configure_camera_rig_crane
  { scenario: 'CINEMATICS: configure_camera_rig_crane', toolName: 'manage_sequence', arguments: { action: 'configure_camera_rig_crane', sequencePath: SEQUENCE_PATH, craneArmLength: 300 }, expected: 'success' },
  { scenario: 'CINEMATICS: configure_camera_rig_crane optional', toolName: 'manage_sequence', arguments: { action: 'configure_camera_rig_crane', sequencePath: SEQUENCE_PATH, actorName: CRANE_NAME, cranePitch: 10, craneYaw: 45, label: 'CraneRig', save: true }, expected: 'success' },
  // add_fade_track
  { scenario: 'CINEMATICS: add_fade_track', toolName: 'manage_sequence', arguments: { action: 'add_fade_track', sequencePath: SEQUENCE_PATH }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_fade_track optional', toolName: 'manage_sequence', arguments: { action: 'add_fade_track', sequencePath: SEQUENCE_PATH, from: 0, to: 1, durationFrames: 60, rowIndex: 1, save: true }, expected: 'success|already exists' },
  // add_level_visibility_track
  { scenario: 'CINEMATICS: add_level_visibility_track', toolName: 'manage_sequence', arguments: { action: 'add_level_visibility_track', sequencePath: SEQUENCE_PATH, levelNames: ['/Game/Levels/Level01'] }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_level_visibility_track optional', toolName: 'manage_sequence', arguments: { action: 'add_level_visibility_track', sequencePath: SEQUENCE_PATH, levelNames: ['/Game/Levels/Level01'], visibility: 'Visible', durationFrames: 90, rowIndex: 1, save: true }, expected: 'success|already exists' },
  // add_material_parameter_track
  { scenario: 'CINEMATICS: add_material_parameter_track', toolName: 'manage_sequence', arguments: { action: 'add_material_parameter_track', sequencePath: SEQUENCE_PATH, materialPath: MAT_PATH, parameterName: 'Color' }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_material_parameter_track optional', toolName: 'manage_sequence', arguments: { action: 'add_material_parameter_track', sequencePath: SEQUENCE_PATH, componentName: 'MeshComp', materialPath: MAT_PATH, materialIndex: 0, parameterName: 'Color', save: true }, expected: 'success|already exists' },
  // add_particle_track
  { scenario: 'CINEMATICS: add_particle_track', toolName: 'manage_sequence', arguments: { action: 'add_particle_track', sequencePath: SEQUENCE_PATH }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_particle_track optional', toolName: 'manage_sequence', arguments: { action: 'add_particle_track', sequencePath: SEQUENCE_PATH, activate: true, durationFrames: 120, rowIndex: 2, bindingGuid: '${captured:actorBindingId}', save: true }, expected: 'success|already exists' },
  // add_skeletal_animation_track
  { scenario: 'CINEMATICS: add_skeletal_animation_track', toolName: 'manage_sequence', arguments: { action: 'add_skeletal_animation_track', sequencePath: SEQUENCE_PATH, animationSequencePath: ANIM_PATH }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_skeletal_animation_track optional', toolName: 'manage_sequence', arguments: { action: 'add_skeletal_animation_track', sequencePath: SEQUENCE_PATH, animationPath: ANIM_PATH, skeletalMeshPath: SKEL_PATH, actorName: ACTOR_A, sourceActors: [ACTOR_A], sourceClasses: ['SkeletalMeshActor'], prioritizeActors: true, save: true }, expected: 'success|already exists' },
  // add_transform_track
  { scenario: 'CINEMATICS: add_transform_track', toolName: 'manage_sequence', arguments: { action: 'add_transform_track', sequencePath: SEQUENCE_PATH, actorName: ACTOR_A }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_transform_track optional', toolName: 'manage_sequence', arguments: { action: 'add_transform_track', sequencePath: SEQUENCE_PATH, actorName: ACTOR_A, location: { x: 10, y: 20, z: 30 }, rotation: { pitch: 0, yaw: 90, roll: 0 }, tracks: ['Transform'], save: true }, expected: 'success|already exists' },
  // add_event_track
  { scenario: 'CINEMATICS: add_event_track', toolName: 'manage_sequence', arguments: { action: 'add_event_track', sequencePath: SEQUENCE_PATH, actorName: ACTOR_A }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_event_track optional', toolName: 'manage_sequence', arguments: { action: 'add_event_track', sequencePath: SEQUENCE_PATH, actorName: ACTOR_A, property: 'bHidden', save: true }, expected: 'success|already exists' },
  // add_property_track
  { scenario: 'CINEMATICS: add_property_track', toolName: 'manage_sequence', arguments: { action: 'add_property_track', sequencePath: SEQUENCE_PATH, actorName: ACTOR_A, property: 'bHidden' }, expected: 'success|already exists' },
  { scenario: 'CINEMATICS: add_property_track optional', toolName: 'manage_sequence', arguments: { action: 'add_property_track', sequencePath: SEQUENCE_PATH, actorName: ACTOR_A, propertyName: 'RelativeLocation', propertyPath: 'RelativeLocation', propertyType: 'vector', save: true }, expected: 'success|already exists' },

  // === CINEMATICS (L1) CLEANUP ===
  { scenario: 'Cleanup: delete master sequence B', toolName: 'manage_asset', arguments: { action: 'delete', path: MASTER_PATH_2, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete master sequence', toolName: 'manage_asset', arguments: { action: 'delete', path: MASTER_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete cine camera actors', toolName: 'control_actor', arguments: { action: 'delete', actorName: CINE_CAM }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete cine camera actor B', toolName: 'control_actor', arguments: { action: 'delete', actorName: CINE_CAM_2 }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete camera rig actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: RIG_NAME }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete camera crane actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: CRANE_NAME }, expected: 'success|not found' },

  // === DUPLICATE / RENAME / DELETE ===
  { scenario: 'ACTION: duplicate', toolName: 'manage_sequence', arguments: { action: 'duplicate', path: SEQUENCE_PATH, destinationPath: TEST_FOLDER_ALIAS, newName: DUPLICATE_NAME }, expected: 'success' },
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
  { scenario: 'Setup: create Take Recorder sequence', toolName: 'manage_sequence', arguments: { action: 'create', name: TAKE_SEQ_NAME, path: TEST_FOLDER_ALIAS }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn Take Recorder actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TAKE_ACTOR, location: { x: 0, y: 0, z: 50 } }, expected: 'success|already exists' },

  // create_take_recorder_panel (recordingSequencePath / takeSequencePath / takePresetPath / recordInto / frameRate)
  { scenario: 'RECORDREPLAY: create_take_recorder_panel', toolName: 'manage_sequence', arguments: { action: 'create_take_recorder_panel', sequencePath: TAKE_SEQ_PATH, recordingSequencePath: TAKE_SEQ_PATH, takeSequencePath: TAKE_SEQ_PATH, takePresetPath: TAKE_PRESET_PATH, frameRate: 30, recordInto: false }, expected: 'success' },

  // configure_take_sources (actorName / clearSources / reduceKeys / recordParentHierarchy / recordType)
  { scenario: 'RECORDREPLAY: configure_take_sources', toolName: 'manage_sequence', arguments: { action: 'configure_take_sources', sequencePath: TAKE_SEQ_PATH, actorName: TAKE_ACTOR, clearSources: false, actors: [TAKE_ACTOR], recordParentHierarchy: true, reduceKeys: true, recordType: '0', recordingSequencePath: TAKE_SEQ_PATH, takeSequencePath: TAKE_SEQ_PATH }, expected: 'success' },

  // configure_recorded_tracks (properties / trackNames / tracks / enabled / disableOthers / reduceKeys / recordParentHierarchy / recordType)
  { scenario: 'RECORDREPLAY: configure_recorded_tracks', toolName: 'manage_sequence', arguments: { action: 'configure_recorded_tracks', sequencePath: TAKE_SEQ_PATH, actorName: TAKE_ACTOR, properties: ['RelativeLocation'], trackNames: ['Transform'], tracks: ['Transform'], enabled: true, disableOthers: false, recordParentHierarchy: true, reduceKeys: true, recordType: '0' }, expected: 'success' },

  // start_recording (recordingSequencePath / takeSequencePath / takePresetPath)
  { scenario: 'RECORDREPLAY: start_recording', toolName: 'manage_sequence', arguments: { action: 'start_recording', sequencePath: TAKE_SEQ_PATH, recordingSequencePath: TAKE_SEQ_PATH, takeSequencePath: TAKE_SEQ_PATH, takePresetPath: TAKE_PRESET_PATH }, expected: 'success' },

  // stop_recording (no payload params)
  { scenario: 'RECORDREPLAY: stop_recording', toolName: 'manage_sequence', arguments: { action: 'stop_recording' }, expected: 'success' },

  // configure_demo_settings (demoName / friendlyName / additionalOptions / checkpointSaveMaxMSPerFrame / maxRecordTimeSeconds / playbackSpeed / loadDefaultMapOnStop) — no PIE required
  { scenario: 'RECORDREPLAY: configure_demo_settings', toolName: 'manage_sequence', arguments: { action: 'configure_demo_settings', demoName: DEMO_NAME, friendlyName: 'McpReplayDemo', additionalOptions: ['-windowed'], checkpointSaveMaxMSPerFrame: 10, maxRecordTimeSeconds: 30, playbackSpeed: 1.0, loadDefaultMapOnStop: false }, expected: 'success' },

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
  { scenario: 'RECORDREPLAY: play_demo', toolName: 'manage_sequence', arguments: { action: 'play_demo', demoName: DEMO_NAME, replayName: DEMO_NAME, timeSeconds: 0 }, expected: 'error' },
  // pause_demo (demoName / paused)
  { scenario: 'RECORDREPLAY: pause_demo', toolName: 'manage_sequence', arguments: { action: 'pause_demo', demoName: DEMO_NAME, paused: true }, expected: 'error' },
  // seek_demo (demoName / timeSeconds / seconds / seekTime)
  { scenario: 'RECORDREPLAY: seek_demo', toolName: 'manage_sequence', arguments: { action: 'seek_demo', demoName: DEMO_NAME, timeSeconds: 1.0, seconds: 1.0, seekTime: 1.0 }, expected: 'error' },
  // set_demo_playback_speed (demoName / speed / playbackSpeed)
  { scenario: 'RECORDREPLAY: set_demo_playback_speed', toolName: 'manage_sequence', arguments: { action: 'set_demo_playback_speed', demoName: DEMO_NAME, speed: 2.0, playbackSpeed: 2.0 }, expected: 'error' },
  // start_killcam (demoName / replayName / durationSeconds / endTime)
  { scenario: 'RECORDREPLAY: start_killcam', toolName: 'manage_sequence', arguments: { action: 'start_killcam', demoName: DEMO_NAME, replayName: DEMO_NAME, durationSeconds: 4.0 }, expected: 'error' },

  // === RECORD REPLAY (L4/L5) CLEANUP ===
  { scenario: 'Cleanup: delete Take Recorder sequence', toolName: 'manage_asset', arguments: { action: 'delete', path: TAKE_SEQ_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete Take Recorder actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TAKE_ACTOR }, expected: 'success|not found' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete sequence asset', toolName: 'manage_asset', arguments: { action: 'delete', path: SEQUENCE_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete actor A', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR_A }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete actor B', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR_B }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete sequence camera', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'SequenceCamera' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-sequence', testCases);
