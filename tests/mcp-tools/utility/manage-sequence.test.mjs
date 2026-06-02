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
  { scenario: 'ADD: add_actor', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: SEQUENCE_PATH, actorName: ACTOR_A }, expected: 'success|already exists' },
  { scenario: 'ADD: add_actors', toolName: 'manage_sequence', arguments: { action: 'add_actors', path: SEQUENCE_PATH, actorNames: [ACTOR_A, ACTOR_B] }, expected: 'success|already exists' },
  { scenario: 'INFO: get_bindings', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: SEQUENCE_PATH }, expected: 'success' },

  // === PLAYBACK ===
  { scenario: 'PLAYBACK: play', toolName: 'manage_sequence', arguments: { action: 'play', path: SEQUENCE_PATH, startTime: 0, loopMode: 'once' }, expected: 'success' },
  { scenario: 'PLAYBACK: pause', toolName: 'manage_sequence', arguments: { action: 'pause', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'PLAYBACK: stop', toolName: 'manage_sequence', arguments: { action: 'stop', path: SEQUENCE_PATH }, expected: 'success' },
  { scenario: 'CONFIG: set_playback_speed', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', path: SEQUENCE_PATH, speed: 1.25 }, expected: 'success' },

  // === PROPERTIES / KEYFRAMES ===
  { scenario: 'ADD: add_keyframe', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: SEQUENCE_PATH, actorName: ACTOR_A, property: 'Location', frame: 12, value: { x: 100, y: 50, z: 150 } }, expected: 'success' },
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

  // === CLEANUP ===
  { scenario: 'Cleanup: delete sequence asset', toolName: 'manage_asset', arguments: { action: 'delete', path: SEQUENCE_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete actor A', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR_A }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete actor B', toolName: 'control_actor', arguments: { action: 'delete', actorName: ACTOR_B }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete sequence camera', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'SequenceCamera' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-sequence', testCases);
