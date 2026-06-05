#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

const FOCUS_ACTOR = `MCP_EditorFocus_${ts}`;
const PIE_PAWN = `MCP_EditorPawn_${ts}`;
const BP_NAME = `BP_ControlEditor_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;
const SCREENSHOT_NAME = `MCP_ControlEditor_${ts}`;

const cameraLocation = { x: 250, y: -350, z: 260 };
const cameraRotation = { pitch: -20, yaw: 35, roll: 0 };

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create focus actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: FOCUS_ACTOR, location: { x: 0, y: 0, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'Setup: create PIE pawn', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.DefaultPawn', actorName: PIE_PAWN, location: { x: 180, y: 0, z: 140 } }, expected: 'success|already exists' },
  { scenario: 'Setup: create asset for editor open/close', toolName: 'manage_blueprint', arguments: { action: 'create', name: BP_NAME, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },

  // === PLAYBACK / PIE STATE ===
  { scenario: 'PLAYBACK: play', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success' },
  { scenario: 'ACTION: set_view_target', toolName: 'control_editor', arguments: { action: 'set_view_target', actorName: FOCUS_ACTOR, blendTime: 0 }, expected: 'success' },
  { scenario: 'ACTION: set_game_view_target objectPath', toolName: 'control_editor', arguments: { action: 'set_game_view_target', objectPath: FOCUS_ACTOR, blendTime: 0.1 }, expected: 'success' },
  { scenario: 'ACTION: possess', toolName: 'control_editor', arguments: { action: 'possess', actorName: PIE_PAWN }, expected: 'success|NOT_IN_PIE' },
  { scenario: 'ACTION: eject', toolName: 'control_editor', arguments: { action: 'eject' }, expected: 'success|NO_ACTIVE_SESSION|not active' },
  { scenario: 'PLAYBACK: pause', toolName: 'control_editor', arguments: { action: 'pause' }, expected: 'success' },
  { scenario: 'PLAYBACK: resume', toolName: 'control_editor', arguments: { action: 'resume' }, expected: 'success' },
  { scenario: 'ACTION: step_frame', toolName: 'control_editor', arguments: { action: 'step_frame', steps: 1 }, expected: 'success' },
  { scenario: 'ACTION: single_frame_step', toolName: 'control_editor', arguments: { action: 'single_frame_step', steps: 1 }, expected: 'success' },
  { scenario: 'PLAYBACK: stop', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success' },
  { scenario: 'PLAYBACK: stop_pie', toolName: 'control_editor', arguments: { action: 'stop_pie' }, expected: 'success' },

  // === CAMERA / VIEWPORT ===
  { scenario: 'CONFIG: set_camera', toolName: 'control_editor', arguments: { action: 'set_camera', location: cameraLocation, rotation: cameraRotation }, expected: 'success' },
  { scenario: 'CONFIG: set_camera_position', toolName: 'control_editor', arguments: { action: 'set_camera_position', location: { x: 300, y: -320, z: 240 }, rotation: cameraRotation }, expected: 'success' },
  { scenario: 'CONFIG: set_viewport_camera', toolName: 'control_editor', arguments: { action: 'set_viewport_camera', location: { x: 340, y: -280, z: 220 }, rotation: cameraRotation }, expected: 'success' },
  { scenario: 'CONFIG: set_camera_fov', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 85 }, expected: 'success' },
  { scenario: 'CONFIG: set_view_mode', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Lit' }, expected: 'success' },
  { scenario: 'CONFIG: set_viewport_resolution', toolName: 'control_editor', arguments: { action: 'set_viewport_resolution', width: 1280, height: 720 }, expected: 'success' },
  { scenario: 'CONFIG: set_viewport_realtime', toolName: 'control_editor', arguments: { action: 'set_viewport_realtime', realtime: false }, expected: 'success' },

  // === COMMANDS / CAPTURE / RECORDING ===
  { scenario: 'ACTION: console_command', toolName: 'control_editor', arguments: { action: 'console_command', command: 'stat fps' }, expected: 'success' },
  { scenario: 'ACTION: execute_command', toolName: 'control_editor', arguments: { action: 'execute_command', command: 'stat unit' }, expected: 'success' },
  { scenario: 'ACTION: screenshot', toolName: 'control_editor', arguments: { action: 'screenshot', filename: SCREENSHOT_NAME, resolution: '640x360', mode: 'editor_viewport', returnBase64: false, includeMetadata: true, metadata: { source: 'control-editor-suite' } }, expected: 'success' },
  { scenario: 'ACTION: take_screenshot', toolName: 'control_editor', arguments: { action: 'take_screenshot', filename: `${SCREENSHOT_NAME}_Alias`, resolution: '640x360' }, expected: 'success' },
  { scenario: 'ACTION: start_recording', toolName: 'control_editor', arguments: { action: 'start_recording', name: `Recording_${ts}` }, expected: 'success' },
  { scenario: 'PLAYBACK: stop_recording', toolName: 'control_editor', arguments: { action: 'stop_recording' }, expected: 'success' },

  // === BOOKMARKS / PREFERENCES / ASSETS ===
  { scenario: 'CREATE: create_bookmark', toolName: 'control_editor', arguments: { action: 'create_bookmark', id: 0, bookmarkName: '0' }, expected: 'success|already exists' },
  { scenario: 'ACTION: jump_to_bookmark', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', id: 0, bookmarkName: '0' }, expected: 'success' },
  { scenario: 'CONFIG: set_preferences', toolName: 'control_editor', arguments: { action: 'set_preferences', category: 'LevelEditor', preferences: { RealtimeAudio: false } }, expected: 'success' },
  { scenario: 'ACTION: open_asset', toolName: 'control_editor', arguments: { action: 'open_asset', assetPath: BP_PATH }, expected: 'success' },
  { scenario: 'ACTION: close_asset', toolName: 'control_editor', arguments: { action: 'close_asset', assetPath: BP_PATH }, expected: 'success' },

  // === INPUT / LEVEL / ACTOR FOCUS ===
  { scenario: 'ACTION: simulate_input', toolName: 'control_editor', arguments: { action: 'simulate_input', inputAction: 'pressed', key: 'K' }, expected: 'success' },
  { scenario: 'ACTION: simulate_input mouse move', toolName: 'control_editor', arguments: { action: 'simulate_input', inputType: 'move', x: 320, y: 180 }, expected: 'success' },
  { scenario: 'ACTION: simulate_input mouse click', toolName: 'control_editor', arguments: { action: 'simulate_input', type: 'mouse_click', x: 320, y: 180, button: 'left' }, expected: 'success' },
  { scenario: 'ACTION: focus_actor', toolName: 'control_editor', arguments: { action: 'focus_actor', actorName: FOCUS_ACTOR }, expected: 'success' },

  // === EDITOR DISPLAY / MODE / HISTORY ===
  { scenario: 'CONFIG: set_game_speed', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 1 }, expected: 'success' },
  { scenario: 'CONFIG: set_fixed_delta_time', toolName: 'control_editor', arguments: { action: 'set_fixed_delta_time', deltaTime: 0.01667 }, expected: 'success' },
  { scenario: 'ACTION: show_stats', toolName: 'control_editor', arguments: { action: 'show_stats', stat: 'fps' }, expected: 'success' },
  { scenario: 'ACTION: hide_stats', toolName: 'control_editor', arguments: { action: 'hide_stats', stat: 'fps' }, expected: 'success' },
  { scenario: 'CONFIG: set_editor_mode', toolName: 'control_editor', arguments: { action: 'set_editor_mode', mode: 'EM_Placement' }, expected: 'success' },
  { scenario: 'CONFIG: set_immersive_mode', toolName: 'control_editor', arguments: { action: 'set_immersive_mode', enabled: false }, expected: 'success' },
  { scenario: 'CONFIG: set_game_view', toolName: 'control_editor', arguments: { action: 'set_game_view', enabled: false }, expected: 'success' },
  { scenario: 'ACTION: undo', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'ACTION: redo', toolName: 'control_editor', arguments: { action: 'redo' }, expected: 'success' },
  { scenario: 'ACTION: save_all', toolName: 'control_editor', arguments: { action: 'save_all' }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete spawned actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [FOCUS_ACTOR, PIE_PAWN] }, expected: 'success|not found' },
  { scenario: 'ACTION: open_level via path alias', toolName: 'control_editor', arguments: { action: 'open_level', path: '/Game/MCPTest/MainLevel' }, expected: 'success' },
  { scenario: 'ACTION: open_level', toolName: 'control_editor', arguments: { action: 'open_level', levelPath: '/Game/MCPTest/MainLevel' }, expected: 'success' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('control-editor', testCases);
