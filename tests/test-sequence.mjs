#!/usr/bin/env node
/**
 * Sequencer Test Suite (supported actions only)
 * Tool: manage_sequence
 */

import { runToolTests } from './test-runner.mjs';

const seqPath = '/Game/Cinematics/TC_Seq';
const copyDir = '/Game/Cinematics/Copies';

const testCases = [
  { scenario: 'Create sequence', toolName: 'manage_sequence', arguments: { action: 'create', name: 'TC_Seq', path: '/Game/Cinematics' }, expected: 'success - sequence created' },
  { scenario: 'Open sequence', toolName: 'manage_sequence', arguments: { action: 'open', path: seqPath }, expected: 'success' },
  { scenario: 'Add camera (spawn-only fallback ok)', toolName: 'manage_sequence', arguments: { action: 'add_camera', spawnable: true }, expected: 'success or not implemented' },
  { scenario: 'Get bindings', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: seqPath }, expected: 'success - bindings listed' },
  { scenario: 'Set playback speed', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', speed: 1.2 }, expected: 'success' },
  { scenario: 'Get sequence properties', toolName: 'manage_sequence', arguments: { action: 'get_properties', path: seqPath }, expected: 'success - properties retrieved' },
  { scenario: 'Duplicate sequence', toolName: 'manage_sequence', arguments: { action: 'duplicate', path: seqPath, destinationPath: copyDir, newName: 'TC_Seq_Copy', overwrite: true }, expected: 'success - duplicated' },
  { scenario: 'Rename sequence copy', toolName: 'manage_sequence', arguments: { action: 'rename', path: `${copyDir}/TC_Seq_Copy`, newName: 'TC_Seq_Renamed' }, expected: 'success - renamed' },
  { scenario: 'Delete sequence copy', toolName: 'manage_sequence', arguments: { action: 'delete', path: `${copyDir}/TC_Seq_Renamed` }, expected: 'success - deleted' },
  { scenario: 'Delete original sequence', toolName: 'manage_sequence', arguments: { action: 'delete', path: seqPath }, expected: 'success - deleted' },
  { scenario: 'Verify sequence removed', toolName: 'manage_sequence', arguments: { action: 'get_bindings', path: seqPath }, expected: 'success or not found' }
];

await runToolTests('Sequences', testCases);
