#!/usr/bin/env node
/**
 * Condensed Editor Control Test Suite (15 cases) — safe for real Editor runs.
 * Tool: control_editor
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Pre-Fix: Remove Invalid Data Layers (Native)',
    toolName: 'manage_level',
    arguments: {
      action: 'cleanup_invalid_datalayers'
    },
    // We expect success if supported, or error if not. 
    // Since we just added it, it should succeed if plugin is rebuilt.
    // If plugin is NOT rebuilt yet, this will fail with valid error or "unknown action" depending on how it falls through.
    // But since the user is expected to rebuild, we expect success.
    expected: 'success'
  },
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
  // Real-World Scenario: Editor Automation
  { scenario: 'Editor Auto - Open Asset', toolName: 'control_editor', arguments: { action: 'open_asset', assetPath: '/Engine/BasicShapes/Cube' }, expected: 'success' },
  { scenario: 'Editor Auto - Focus Viewport', toolName: 'control_editor', arguments: { action: 'set_camera', location: { x: 500, y: 500, z: 500 }, rotation: { pitch: -30, yaw: 225, roll: 0 } }, expected: 'success' },
  { scenario: 'Editor Auto - Run Command', toolName: 'control_editor', arguments: { action: 'execute_command', command: 'stat fps' }, expected: 'success' },
  {
    scenario: "Error: Invalid view mode",
    toolName: "control_editor",
    arguments: { action: "set_view_mode", viewMode: "InvalidMode" },
    expected: "error|unknown_viewmode"
  },
  {
    scenario: "Edge: Extreme FOV (1 degree)",
    toolName: "control_editor",
    arguments: { action: "set_camera_fov", fov: 1 },
    expected: "success"
  },
  {
    scenario: "Border: Negative speed",
    toolName: "control_editor",
    arguments: { action: "set_game_speed", speed: -1 },
    expected: "success|handled"
  },
  {
    scenario: "Error: Screenshot invalid res",
    toolName: "control_editor",
    arguments: { action: "screenshot", resolution: "invalidxres" },
    expected: "error"
  },
  {
    scenario: "Edge: Empty command",
    toolName: "control_editor",
    arguments: { action: "console_command", command: "" },
    expected: "error|empty"
  },
  {
    scenario: "Timeout short fail",
    toolName: "control_editor",
    arguments: { action: "play", timeoutMs: 1 },
    expected: "timeout|error"
  }
];

await runToolTests('Editor Control', testCases);
