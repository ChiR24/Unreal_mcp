#!/usr/bin/env node
/**
 * Comprehensive Editor Control Test Suite
 * Tool: control_editor
 * Coverage: All 24 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // === PRE-FIX ===
  {
    scenario: 'Pre-Fix: Remove Invalid Data Layers (Native)',
    toolName: 'manage_level',
    arguments: {
      action: 'cleanup_invalid_datalayers'
    },
    expected: 'success'
  },

  // === PIE CONTROL (play, stop, stop_pie, pause, resume) ===
  { scenario: 'Start PIE (Play in Editor)', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success - PIE started' },
  { scenario: 'Pause PIE', toolName: 'control_editor', arguments: { action: 'pause' }, expected: 'success - PIE paused' },
  { scenario: 'Resume PIE', toolName: 'control_editor', arguments: { action: 'resume' }, expected: 'success - PIE resumed' },
  { scenario: 'Stop PIE', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success - PIE stopped' },
  { scenario: 'Stop PIE (alias)', toolName: 'control_editor', arguments: { action: 'stop_pie' }, expected: 'success - PIE stopped' },

  // === GAME SPEED ===
  { scenario: 'Set game speed 0.5x', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 0.5 }, expected: 'success' },
  { scenario: 'Set game speed 2x', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 2.0 }, expected: 'success' },
  { scenario: 'Set game speed 1x (reset)', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 1.0 }, expected: 'success' },

  // === EJECT / POSSESS ===
  { scenario: 'Eject from possessed pawn', toolName: 'control_editor', arguments: { action: 'eject' }, expected: 'success|handled' },
  { scenario: 'Possess pawn', toolName: 'control_editor', arguments: { action: 'possess' }, expected: 'success|handled' },

  // === CAMERA CONTROL ===
  { scenario: 'Set camera location and rotation', toolName: 'control_editor', arguments: { action: 'set_camera', location: { x: 0, y: 0, z: 500 }, rotation: { pitch: -45, yaw: 0, roll: 0 } }, expected: 'success - camera set' },
  { scenario: 'Set camera position only', toolName: 'control_editor', arguments: { action: 'set_camera_position', location: { x: 100, y: 100, z: 300 } }, expected: 'success' },
  { scenario: 'Set camera rotation only', toolName: 'control_editor', arguments: { action: 'set_camera', rotation: { pitch: -30, yaw: 45, roll: 0 } }, expected: 'success' },
  { scenario: 'Focus camera on actor', toolName: 'control_editor', arguments: { action: 'set_camera', targetActor: 'TC_Cube' }, expected: 'success - camera focused' },
  { scenario: 'Set camera FOV 90', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 90 }, expected: 'success' },
  { scenario: 'Set camera FOV 60', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 60 }, expected: 'success' },
  { scenario: 'Set camera FOV extreme (120)', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 120 }, expected: 'success' },

  // === VIEW MODE ===
  { scenario: 'Set view mode to Lit', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Lit' }, expected: 'success - view mode set' },
  { scenario: 'Set view mode to Wireframe', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Wireframe' }, expected: 'success - view mode set' },
  { scenario: 'Set view mode to Unlit', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Unlit' }, expected: 'success' },
  { scenario: 'Set view mode to PathTracing', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'PathTracing' }, expected: 'success|not_supported' },

  // === VIEWPORT SETTINGS ===
  { scenario: 'Set viewport resolution 1920x1080', toolName: 'control_editor', arguments: { action: 'set_viewport_resolution', width: 1920, height: 1080 }, expected: 'success' },
  { scenario: 'Set viewport resolution 1280x720', toolName: 'control_editor', arguments: { action: 'set_viewport_resolution', width: 1280, height: 720 }, expected: 'success' },
  { scenario: 'Set viewport realtime on', toolName: 'control_editor', arguments: { action: 'set_viewport_realtime', enabled: true }, expected: 'success' },
  { scenario: 'Set viewport realtime off', toolName: 'control_editor', arguments: { action: 'set_viewport_realtime', enabled: false }, expected: 'success' },

  // === SCREENSHOT ===
  { scenario: 'Take screenshot', toolName: 'control_editor', arguments: { action: 'screenshot', filename: 'TC_Screenshot' }, expected: 'success - screenshot taken' },
  { scenario: 'Take screenshot with resolution', toolName: 'control_editor', arguments: { action: 'screenshot', filename: 'TC_Screenshot_HD', width: 1920, height: 1080 }, expected: 'success' },

  // === RECORDING ===
  { scenario: 'Start recording', toolName: 'control_editor', arguments: { action: 'start_recording', filename: 'TC_Record', frameRate: 30 }, expected: 'success - recording started' },
  { scenario: 'Stop recording', toolName: 'control_editor', arguments: { action: 'stop_recording' }, expected: 'success - recording stopped' },

  // === FRAME STEPPING ===
  { scenario: 'Step single frame', toolName: 'control_editor', arguments: { action: 'step_frame' }, expected: 'success - frame stepped' },
  { scenario: 'Step multiple frames', toolName: 'control_editor', arguments: { action: 'step_frame', steps: 5 }, expected: 'success' },

  // === BOOKMARKS ===
  { scenario: 'Create camera bookmark', toolName: 'control_editor', arguments: { action: 'create_bookmark', bookmarkName: 'TC_Bookmark_1' }, expected: 'success - bookmark created' },
  { scenario: 'Create second bookmark', toolName: 'control_editor', arguments: { action: 'create_bookmark', bookmarkName: 'TC_Bookmark_2' }, expected: 'success' },
  { scenario: 'Jump to bookmark 1', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', bookmarkName: 'TC_Bookmark_1' }, expected: 'success - jumped to bookmark' },
  { scenario: 'Jump to bookmark 2', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', bookmarkName: 'TC_Bookmark_2' }, expected: 'success' },

  // === PREFERENCES ===
  { scenario: 'Set editor preferences - grid', toolName: 'control_editor', arguments: { action: 'set_preferences', category: 'Editor', preferences: { UseGrid: true, SnapToGrid: true } }, expected: 'success - editor preferences set' },
  { scenario: 'Set editor preferences - realtime', toolName: 'control_editor', arguments: { action: 'set_preferences', category: 'Viewport', preferences: { bRealtimeUpdate: true } }, expected: 'success' },

  // === CONSOLE COMMANDS ===
  { scenario: 'Execute console command (stat fps)', toolName: 'control_editor', arguments: { action: 'execute_command', command: 'stat fps' }, expected: 'success' },
  { scenario: 'Console command (alias)', toolName: 'control_editor', arguments: { action: 'console_command', command: 'stat unit' }, expected: 'success' },
  { scenario: 'Console command - toggle stat', toolName: 'control_editor', arguments: { action: 'console_command', command: 'stat none' }, expected: 'success' },

  // === OPEN ASSET ===
  { scenario: 'Open asset in editor', toolName: 'control_editor', arguments: { action: 'open_asset', assetPath: '/Engine/BasicShapes/Cube' }, expected: 'success' },
  { scenario: 'Open material asset', toolName: 'control_editor', arguments: { action: 'open_asset', assetPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success' },

  // === SIMULATE INPUT ===
  { scenario: 'Simulate key press W', toolName: 'control_editor', arguments: { action: 'simulate_input', keyName: 'W', eventType: 'KeyDown' }, expected: 'success' },
  { scenario: 'Simulate key release W', toolName: 'control_editor', arguments: { action: 'simulate_input', keyName: 'W', eventType: 'KeyUp' }, expected: 'success' },
  { scenario: 'Simulate key press and release Space', toolName: 'control_editor', arguments: { action: 'simulate_input', keyName: 'SpaceBar', eventType: 'Both' }, expected: 'success' },

  // === ERROR CASES ===
  { scenario: 'Error: Invalid view mode', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'InvalidMode' }, expected: 'error|unknown_viewmode' },
  { scenario: 'Error: Invalid bookmark', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', bookmarkName: 'NonExistent_Bookmark_XYZ' }, expected: 'error|not_found' },
  { scenario: 'Error: Empty command', toolName: 'control_editor', arguments: { action: 'console_command', command: '' }, expected: 'error|empty' },
  { scenario: 'Error: Invalid resolution', toolName: 'control_editor', arguments: { action: 'set_viewport_resolution', width: -100, height: -100 }, expected: 'error|validation' },
  { scenario: 'Error: Open non-existent asset', toolName: 'control_editor', arguments: { action: 'open_asset', assetPath: '/Game/NonExistent/Asset' }, expected: 'error|not_found' },
  { scenario: 'Error: Screenshot invalid resolution', toolName: 'control_editor', arguments: { action: 'screenshot', resolution: 'invalidxres' }, expected: 'error' },

  // === EDGE CASES ===
  { scenario: 'Edge: Extreme FOV (1 degree)', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 1 }, expected: 'success' },
  { scenario: 'Edge: Negative speed', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: -1 }, expected: 'success|handled' },
  { scenario: 'Edge: Zero speed (frozen)', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 0 }, expected: 'success' },
  { scenario: 'Edge: Very high speed', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 10.0 }, expected: 'success' },

  // === TIMEOUT TEST ===
  { scenario: 'Timeout short fail', toolName: 'control_editor', arguments: { action: 'play', timeoutMs: 1 }, expected: 'timeout|error' }
];

await runToolTests('Editor Control', testCases);
