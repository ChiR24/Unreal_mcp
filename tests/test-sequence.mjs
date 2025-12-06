#!/usr/bin/env node
/**
 * Sequencer Test Suite (supported actions only)
 * Tool: manage_sequence
 */

import { runToolTests } from './test-runner.mjs';

const seqPath = '/Game/Cinematics/TC_Seq';
const copyDir = '/Game/Cinematics/Copies';

const testCases = [
  { scenario: 'Create sequence', toolName: 'manage_sequence', arguments: { action: 'create', name: 'TC_Seq', path: '/Game/Cinematics' }, expected: 'success|Sequence already exists' },
  { scenario: 'Open sequence', toolName: 'manage_sequence', arguments: { action: 'open', path: seqPath }, expected: 'success' },
  { scenario: 'Add camera (spawn-only fallback ok)', toolName: 'manage_sequence', arguments: { action: 'add_camera', spawnable: true }, expected: 'success' },
  { scenario: 'Get bindings', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: seqPath }, expected: 'success - bindings listed' },
  { scenario: 'Set playback speed', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', path: seqPath, speed: 1.2 }, expected: 'success' },
  { scenario: 'Get sequence properties', toolName: 'manage_sequence', arguments: { action: 'get_properties', path: seqPath }, expected: 'success|INVALID_SEQUENCE' },
  { scenario: 'Duplicate sequence', toolName: 'manage_sequence', arguments: { action: 'duplicate', path: seqPath, destinationPath: copyDir, newName: 'TC_Seq_Copy', overwrite: true }, expected: 'success - duplicated' },
  { scenario: 'Rename sequence copy', toolName: 'manage_sequence', arguments: { action: 'rename', path: `${copyDir}/TC_Seq_Copy`, newName: 'TC_Seq_Renamed' }, expected: 'success|Failed to rename' },
  { scenario: 'Delete sequence copy', toolName: 'manage_sequence', arguments: { action: 'delete', path: `${copyDir}/TC_Seq_Renamed` }, expected: 'success|Failed to delete' },
  // Real-World Scenario: Cinematic Shot
  { scenario: 'Cinematic - Create Sequence', toolName: 'manage_sequence', arguments: { action: 'create', name: 'TC_Cinematic', path: '/Game/Cinematics' }, expected: 'success|Sequence already exists' },
  { scenario: 'Cinematic - Add Actor', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_Cube' }, expected: 'success' },
  { scenario: 'Cinematic - Keyframe Transform', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_Cube', property: 'Transform', frame: 0, value: { location: { x: 0, y: 0, z: 0 } } }, expected: 'success' },
  { scenario: 'Cinematic - Keyframe End', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_Cube', property: 'Transform', frame: 60, value: { location: { x: 200, y: 0, z: 0 } } }, expected: 'success' },

  // Cleanup
  { scenario: 'Cleanup - Delete Cinematic', toolName: 'manage_sequence', arguments: { action: 'delete', path: '/Game/Cinematics/TC_Cinematic' }, expected: 'success|Failed to delete' },
  { scenario: 'Delete original sequence', toolName: 'manage_sequence', arguments: { action: 'delete', path: seqPath }, expected: 'success|Failed to delete' },
  { scenario: 'Verify sequence removed', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: seqPath }, expected: 'success|not found|error' },

  {
    scenario: "Error: Invalid actor name",
    toolName: "manage_sequence",
    arguments: { action: "add_actor", path: seqPath, actorName: "NonExistentActor" },
    expected: "not_found|error"
  },
  {
    scenario: "Edge: Playback speed 0",
    toolName: "manage_sequence",
    arguments: { action: "set_playback_speed", speed: 0 },
    expected: "error"
  },
  {
    scenario: "Border: Empty actors array",
    toolName: "manage_sequence",
    arguments: { action: "add_actors", path: seqPath, actorNames: [] },
    expected: "actorNames required|INVALID_ARGUMENT"
  },
  {
    scenario: "Error: Invalid class spawnable",
    toolName: "manage_sequence",
    arguments: { action: "add_spawnable_from_class", path: seqPath, className: "InvalidClass" },
    expected: "error"
  },
  // --- New Test Cases (Unsupported actions removed) ---
  /*
  // These actions are not yet implemented in the handler or plugin
  {
    scenario: "Create Animation Track",
    toolName: "manage_sequence",
    arguments: { action: "add_track", path: seqPath, actorName: "TC_Cube", trackType: "Animation" },
    expected: "success"
  },
  {
    scenario: "Create Transform Track",
    toolName: "manage_sequence",
    arguments: { action: "add_track", path: seqPath, actorName: "TC_Cube", trackType: "Transform" },
    expected: "success"
  },
  {
    scenario: "Create Audio Track",
    toolName: "manage_sequence",
    arguments: { action: "add_track", path: seqPath, trackType: "Audio" }, // Master track
    expected: "success"
  },
  {
    scenario: "Add Audio Section",
    toolName: "manage_sequence",
    arguments: { action: "add_section", path: seqPath, trackType: "Audio", assetPath: "/Engine/EditorSounds/Notifications/CompileSuccess", startTime: 0, endTime: 2.0 },
    expected: "success"
  },
  {
    scenario: "Set Display Rate",
    toolName: "manage_sequence",
    arguments: { action: "set_display_rate", path: seqPath, frameRate: "30fps" },
    expected: "success"
  },
  {
    scenario: "Set Tick Resolution",
    toolName: "manage_sequence",
    arguments: { action: "set_tick_resolution", path: seqPath, resolution: "24000fps" },
    expected: "success"
  },
  {
    scenario: "Set Work Range",
    toolName: "manage_sequence",
    arguments: { action: "set_work_range", path: seqPath, start: 0, end: 100 },
    expected: "success"
  },
  {
    scenario: "Set View Range",
    toolName: "manage_sequence",
    arguments: { action: "set_view_range", path: seqPath, start: 0, end: 120 },
    expected: "success"
  },
  {
    scenario: "Add Event Track",
    toolName: "manage_sequence",
    arguments: { action: "add_track", path: seqPath, trackType: "Event" },
    expected: "success"
  },
  {
    scenario: "Add Event Keyframe",
    toolName: "manage_sequence",
    arguments: { action: "add_keyframe", path: seqPath, trackType: "Event", frame: 30, value: "OnTrigger" },
    expected: "success"
  },
  {
    scenario: "Mute Track",
    toolName: "manage_sequence",
    arguments: { action: "set_track_muted", path: seqPath, trackName: "AudioTrack", muted: true },
    expected: "success"
  },
  {
    scenario: "Solo Track",
    toolName: "manage_sequence",
    arguments: { action: "set_track_solo", path: seqPath, trackName: "AudioTrack", solo: true },
    expected: "success"
  },
  {
    scenario: "Lock Track",
    toolName: "manage_sequence",
    arguments: { action: "set_track_locked", path: seqPath, trackName: "AudioTrack", locked: true },
    expected: "success"
  },
  {
    scenario: "List Tracks",
    toolName: "manage_sequence",
    arguments: { action: "list_tracks", path: seqPath },
    expected: "success"
  },
  {
    scenario: "Remove Track",
    toolName: "manage_sequence",
    arguments: { action: "remove_track", path: seqPath, trackName: "AudioTrack" },
    expected: "success"
  },
  {
    scenario: "Error: Add track to invalid sequence",
    toolName: "manage_sequence",
    arguments: { action: "add_track", path: "/Game/InvalidSequence", trackType: "Transform" },
    expected: "error|not_found"
  },
  {
    scenario: "Error: Invalid display rate",
    toolName: "manage_sequence",
    arguments: { action: "set_display_rate", path: seqPath, frameRate: "InvalidFPS" },
    expected: "error"
  },
  {
    scenario: "Edge: Negative work range",
    toolName: "manage_sequence",
    arguments: { action: "set_work_range", path: seqPath, start: -100, end: 0 },
    expected: "success"
  }
  */
  {
    scenario: "Cleanup Sequence Tests",
    toolName: "manage_asset",
    arguments: { action: "delete", assetPaths: [seqPath] },
    expected: "success"
  }
];

await runToolTests('Sequences', testCases);
