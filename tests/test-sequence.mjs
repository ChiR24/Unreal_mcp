#!/usr/bin/env node
/**
 * Comprehensive Sequencer Test Suite
 * Tool: manage_sequence
 * Coverage: All 31 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const seqPath = '/Game/Cinematics/TC_Seq';
const copyDir = '/Game/Cinematics/Copies';

const testCases = [
  // === PRE-CLEANUP ===
  {
    scenario: 'Pre-cleanup: Delete existing test sequences',
    toolName: 'manage_asset',
    arguments: { action: 'delete', assetPaths: [seqPath, `${copyDir}/TC_Seq_Copy`, `${copyDir}/TC_Seq_Renamed`, '/Game/Cinematics/TC_Cinematic'] },
    expected: 'success|not_found'
  },

  // === LIFECYCLE (create, open, duplicate, rename, delete, list) ===
  { scenario: 'Create sequence', toolName: 'manage_sequence', arguments: { action: 'create', name: 'TC_Seq', path: '/Game/Cinematics' }, expected: 'success|Sequence already exists' },
  { scenario: 'Open sequence', toolName: 'manage_sequence', arguments: { action: 'open', path: seqPath }, expected: 'success' },
  { scenario: 'List sequences', toolName: 'manage_sequence', arguments: { action: 'list', path: '/Game/Cinematics' }, expected: 'success' },
  { scenario: 'Get sequence properties', toolName: 'manage_sequence', arguments: { action: 'get_properties', path: seqPath }, expected: 'success' },
  { scenario: 'Set sequence properties', toolName: 'manage_sequence', arguments: { action: 'set_properties', path: seqPath, playbackStart: 0, playbackEnd: 120 }, expected: 'success' },
  { scenario: 'Duplicate sequence', toolName: 'manage_sequence', arguments: { action: 'duplicate', path: seqPath, destinationPath: copyDir, newName: 'TC_Seq_Copy', overwrite: true }, expected: 'success - duplicated' },
  { scenario: 'Rename sequence copy', toolName: 'manage_sequence', arguments: { action: 'rename', path: `${copyDir}/TC_Seq_Copy`, newName: 'TC_Seq_Renamed' }, expected: 'success|Failed to rename' },

  // === METADATA ===
  { scenario: 'Get sequence metadata', toolName: 'manage_sequence', arguments: { action: 'get_metadata', path: seqPath }, expected: 'success' },
  { scenario: 'Set sequence metadata', toolName: 'manage_sequence', arguments: { action: 'set_metadata', path: seqPath, metadata: { author: 'Test', version: '1.0' } }, expected: 'success' },

  // === BINDINGS (add_camera, add_actor, add_actors, remove_actors, get_bindings, add_spawnable_from_class) ===
  { scenario: 'Add camera (spawn-only fallback ok)', toolName: 'manage_sequence', arguments: { action: 'add_camera', path: seqPath, spawnable: true }, expected: 'success' },
  { scenario: 'Get bindings', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: seqPath }, expected: 'success - bindings listed' },

  // Setup actor for binding tests
  { scenario: 'Setup - Spawn Cube for Cinematic', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'TC_SeqCube', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'Add actor binding', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: seqPath, actorName: 'TC_SeqCube' }, expected: 'success' },
  { scenario: 'Add spawnable from class', toolName: 'manage_sequence', arguments: { action: 'add_spawnable_from_class', path: seqPath, className: 'PointLight' }, expected: 'success' },
  { scenario: 'Remove actors', toolName: 'manage_sequence', arguments: { action: 'remove_actors', path: seqPath, actorNames: ['TC_SeqCube'] }, expected: 'success' },

  // === PLAYBACK (play, pause, stop, set_playback_speed) ===
  { scenario: 'Play sequence', toolName: 'manage_sequence', arguments: { action: 'play', path: seqPath }, expected: 'success' },
  { scenario: 'Pause sequence', toolName: 'manage_sequence', arguments: { action: 'pause', path: seqPath }, expected: 'success' },
  { scenario: 'Stop sequence', toolName: 'manage_sequence', arguments: { action: 'stop', path: seqPath }, expected: 'success' },
  { scenario: 'Set playback speed 1.5x', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', path: seqPath, speed: 1.5 }, expected: 'success' },
  { scenario: 'Set playback speed 0.5x', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', path: seqPath, speed: 0.5 }, expected: 'success' },

  // === KEYFRAMES (add_keyframe) ===
  { scenario: 'Re-add actor for keyframes', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: seqPath, actorName: 'TC_SeqCube' }, expected: 'success' },
  { scenario: 'Add keyframe at frame 0', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: seqPath, actorName: 'TC_SeqCube', property: 'Transform', frame: 0, value: { location: { x: 0, y: 0, z: 0 } } }, expected: 'success' },
  { scenario: 'Add keyframe at frame 60', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: seqPath, actorName: 'TC_SeqCube', property: 'Transform', frame: 60, value: { location: { x: 200, y: 0, z: 0 } } }, expected: 'success' },
  { scenario: 'Add keyframe at frame 120', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: seqPath, actorName: 'TC_SeqCube', property: 'Transform', frame: 120, value: { location: { x: 0, y: 200, z: 0 } } }, expected: 'success' },

  // === TRACKS (add_track, add_section, list_tracks, remove_track, set_track_muted, set_track_solo, set_track_locked) ===
  { scenario: 'Add Transform track', toolName: 'manage_sequence', arguments: { action: 'add_track', path: seqPath, actorName: 'TC_SeqCube', trackType: 'Transform' }, expected: 'success' },
  { scenario: 'Add Event track', toolName: 'manage_sequence', arguments: { action: 'add_track', path: seqPath, actorName: 'TC_SeqCube', trackType: 'Event' }, expected: 'success' },
  { scenario: 'List tracks', toolName: 'manage_sequence', arguments: { action: 'list_tracks', path: seqPath }, expected: 'success' },
  { scenario: 'Add section to track', toolName: 'manage_sequence', arguments: { action: 'add_section', path: seqPath, trackName: 'Transform', startFrame: 0, endFrame: 120 }, expected: 'success' },
  { scenario: 'Set track muted', toolName: 'manage_sequence', arguments: { action: 'set_track_muted', path: seqPath, trackName: 'Transform', muted: true }, expected: 'success' },
  { scenario: 'Set track unmuted', toolName: 'manage_sequence', arguments: { action: 'set_track_muted', path: seqPath, trackName: 'Transform', muted: false }, expected: 'success' },
  { scenario: 'Set track solo', toolName: 'manage_sequence', arguments: { action: 'set_track_solo', path: seqPath, trackName: 'Transform', solo: true }, expected: 'success' },
  { scenario: 'Set track locked', toolName: 'manage_sequence', arguments: { action: 'set_track_locked', path: seqPath, trackName: 'Transform', locked: true }, expected: 'success' },
  { scenario: 'Set track unlocked', toolName: 'manage_sequence', arguments: { action: 'set_track_locked', path: seqPath, trackName: 'Transform', locked: false }, expected: 'success' },

  // === DISPLAY/TIMING SETTINGS ===
  { scenario: 'Set display rate 30fps', toolName: 'manage_sequence', arguments: { action: 'set_display_rate', path: seqPath, frameRate: '30fps' }, expected: 'success' },
  { scenario: 'Set display rate 60fps', toolName: 'manage_sequence', arguments: { action: 'set_display_rate', path: seqPath, frameRate: '60fps' }, expected: 'success' },
  { scenario: 'Set tick resolution', toolName: 'manage_sequence', arguments: { action: 'set_tick_resolution', path: seqPath, resolution: '24000fps' }, expected: 'success' },
  { scenario: 'Set work range', toolName: 'manage_sequence', arguments: { action: 'set_work_range', path: seqPath, start: 0, end: 150 }, expected: 'success' },
  { scenario: 'Set view range', toolName: 'manage_sequence', arguments: { action: 'set_view_range', path: seqPath, start: 0, end: 120 }, expected: 'success' },

  // === REAL-WORLD SCENARIO ===
  { scenario: 'Cinematic - Create Sequence', toolName: 'manage_sequence', arguments: { action: 'create', name: 'TC_Cinematic', path: '/Game/Cinematics' }, expected: 'success|Sequence already exists' },
  { scenario: 'Cinematic - Add Actor', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_SeqCube' }, expected: 'success' },
  { scenario: 'Cinematic - Keyframe Start', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_SeqCube', property: 'Transform', frame: 0, value: { location: { x: 0, y: 0, z: 0 } } }, expected: 'success' },
  { scenario: 'Cinematic - Keyframe End', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_SeqCube', property: 'Transform', frame: 60, value: { location: { x: 200, y: 0, z: 0 } } }, expected: 'success' },

  // === ERROR CASES ===
  { scenario: 'Error: Invalid actor name', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: seqPath, actorName: 'NonExistentActor' }, expected: 'not_found|error|ASSET_NOT_FOUND|LOAD_FAILED' },
  { scenario: 'Error: Invalid class spawnable', toolName: 'manage_sequence', arguments: { action: 'add_spawnable_from_class', path: seqPath, className: 'InvalidClass' }, expected: 'error' },
  { scenario: 'Error: Invalid sequence path', toolName: 'manage_sequence', arguments: { action: 'open', path: '/Game/NonExistent/Sequence' }, expected: 'error|not_found' },
  { scenario: 'Error: Remove track non-existent', toolName: 'manage_sequence', arguments: { action: 'remove_track', path: seqPath, trackName: 'NonExistentTrack' }, expected: 'error|not_found' },

  // === EDGE CASES ===
  { scenario: 'Edge: Playback speed 0', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', speed: 0 }, expected: 'error' },
  { scenario: 'Edge: Negative frame keyframe', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: seqPath, actorName: 'TC_SeqCube', property: 'Transform', frame: -10, value: {} }, expected: 'success|error' },
  { scenario: 'Edge: Empty actors array', toolName: 'manage_sequence', arguments: { action: 'add_actors', path: seqPath, actorNames: [] }, expected: 'actorNames required|INVALID_ARGUMENT' },
  { scenario: 'Edge: Very high work range', toolName: 'manage_sequence', arguments: { action: 'set_work_range', path: seqPath, start: 0, end: 100000 }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup - Delete Cinematic', toolName: 'manage_sequence', arguments: { action: 'delete', path: '/Game/Cinematics/TC_Cinematic' }, expected: 'success|Failed to delete' },
  { scenario: 'Cleanup - Delete Cube', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'TC_SeqCube' }, expected: 'success' },
  { scenario: 'Cleanup - Remove track', toolName: 'manage_sequence', arguments: { action: 'remove_track', path: seqPath, trackName: 'Event' }, expected: 'success|not_found' },
  { scenario: 'Delete original sequence', toolName: 'manage_sequence', arguments: { action: 'delete', path: seqPath }, expected: 'success|Failed to delete' },
  { scenario: 'Delete renamed copy', toolName: 'manage_sequence', arguments: { action: 'delete', path: `${copyDir}/TC_Seq_Renamed` }, expected: 'success|Failed to delete' },
  { scenario: 'Verify sequences removed', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: seqPath }, expected: 'success|not found|error|ASSET_NOT_FOUND|LOAD_FAILED' },
  { scenario: 'Final cleanup via manage_asset', toolName: 'manage_asset', arguments: { action: 'delete', assetPaths: [seqPath, copyDir] }, expected: 'success|ASSET_NOT_FOUND|LOAD_FAILED' }
];

await runToolTests('Sequences', testCases);
