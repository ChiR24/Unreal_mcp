#!/usr/bin/env node
/**
 * Condensed Editor Control Test Suite (15 cases) — safe for real Editor runs.
 * Tool: control_editor
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Start PIE (Play in Editor)', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success - PIE started' },
  { scenario: 'Stop PIE', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success - PIE stopped' },
  { scenario: 'Set camera location', toolName: 'control_editor', arguments: { action: 'set_camera', location: { x: 0, y: 0, z: 500 }, rotation: { pitch: -45, yaw: 0, roll: 0 } }, expected: 'success - camera set' },
  { scenario: 'Focus camera on actor', toolName: 'control_editor', arguments: { action: 'set_camera', targetActor: 'TC_Cube' }, expected: 'success - camera focused' },
  { scenario: 'Set view mode to Lit', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Lit' }, expected: 'success - view mode set' },
  { scenario: 'Set view mode to Wireframe', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Wireframe' }, expected: 'success - view mode set' },
  { scenario: 'Take screenshot', toolName: 'control_editor', arguments: { action: 'screenshot', filename: 'TC_Screenshot' }, expected: 'success - screenshot taken' },
  { scenario: 'Start recording', toolName: 'control_editor', arguments: { action: 'start_recording', filename: 'TC_Record', frameRate: 30 }, expected: 'success - recording started' },
  { scenario: 'Stop recording', toolName: 'control_editor', arguments: { action: 'stop_recording' }, expected: 'success - recording stopped' },
  { scenario: 'Pause PIE', toolName: 'control_editor', arguments: { action: 'pause' }, expected: 'success - PIE paused' },
  { scenario: 'Resume PIE', toolName: 'control_editor', arguments: { action: 'resume' }, expected: 'success - PIE resumed' },
  { scenario: 'Step PIE frame', toolName: 'control_editor', arguments: { action: 'step_frame' }, expected: 'success - frame stepped' },
  { scenario: 'Create camera bookmark', toolName: 'control_editor', arguments: { action: 'create_bookmark', bookmarkName: 'TC_Bookmark' }, expected: 'success - bookmark created' },
  { scenario: 'Jump to camera bookmark', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', bookmarkName: 'TC_Bookmark' }, expected: 'success - jumped to bookmark' },
  { scenario: 'Set editor preferences', toolName: 'control_editor', arguments: { action: 'set_preferences', category: 'Editor', preferences: { UseGrid: true, SnapToGrid: true } }, expected: 'success - editor preferences set' },
  // Additional
  { scenario: 'Toggle viewport realtime', toolName: 'control_editor', arguments: { action: 'set_viewport_realtime', enabled: true }, expected: 'success or handled' },
  { scenario: 'Set time dilation', toolName: 'control_editor', arguments: { action: 'set_time_dilation', value: 0.5 }, expected: 'success or handled' },
  { scenario: 'Toggle game view', toolName: 'control_editor', arguments: { action: 'toggle_game_view', enabled: true }, expected: 'success or handled' },
  { scenario: 'Set auto exposure bias', toolName: 'control_editor', arguments: { action: 'set_exposure', bias: 1.0 }, expected: 'success or handled' },
  { scenario: 'Set viewport resolution (windowed)', toolName: 'control_editor', arguments: { action: 'set_resolution', width: 1280, height: 720, windowed: true }, expected: 'success or handled' },
  { scenario: 'Open Output Log', toolName: 'control_editor', arguments: { action: 'open_output_log' }, expected: 'success or handled' }
];

await runToolTests('Editor Control', testCases);
