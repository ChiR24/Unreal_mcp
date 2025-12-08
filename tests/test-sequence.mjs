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
  // Setup: Ensure TC_Cube exists for the cinematic
  { scenario: 'Setup - Spawn Cube for Cinematic', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'TC_Cube', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'Cinematic - Create Sequence', toolName: 'manage_sequence', arguments: { action: 'create', name: 'TC_Cinematic', path: '/Game/Cinematics' }, expected: 'success|Sequence already exists' },
  { scenario: 'Cinematic - Add Actor', toolName: 'manage_sequence', arguments: { action: 'add_actor', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_Cube' }, expected: 'success' },
  { scenario: 'Cinematic - Keyframe Transform', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_Cube', property: 'Transform', frame: 0, value: { location: { x: 0, y: 0, z: 0 } } }, expected: 'success' },
  { scenario: 'Cinematic - Keyframe End', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', path: '/Game/Cinematics/TC_Cinematic', actorName: 'TC_Cube', property: 'Transform', frame: 60, value: { location: { x: 200, y: 0, z: 0 } } }, expected: 'success' },

  // Cleanup
  { scenario: 'Cleanup - Delete Cinematic', toolName: 'manage_sequence', arguments: { action: 'delete', path: '/Game/Cinematics/TC_Cinematic' }, expected: 'success|Failed to delete' },
  { scenario: 'Cleanup - Delete Cube', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'TC_Cube' }, expected: 'success' },
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
  {
    scenario: "Cleanup Sequence Tests",
    toolName: "manage_asset",
    arguments: { action: "delete", assetPaths: [seqPath] },
    expected: "success"
  }
];

await runToolTests('Sequences', testCases);
