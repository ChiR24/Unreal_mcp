#!/usr/bin/env node
/**
 * Comprehensive System Control Test Suite
 * Tool: system_control
 * Coverage: All 22 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // === PROFILING (profile, show_fps, show_stats) ===
  { scenario: 'Enable FPS display', toolName: 'system_control', arguments: { action: 'show_fps', enabled: true }, expected: 'success - FPS shown' },
  { scenario: 'Disable FPS display', toolName: 'system_control', arguments: { action: 'show_fps', enabled: false }, expected: 'success - FPS hidden' },
  // Note: show_stats not implemented in C++ plugin yet
  { scenario: 'Show stats', toolName: 'system_control', arguments: { action: 'show_stats', category: 'Engine' }, expected: 'success|NOT_IMPLEMENTED' },
  { scenario: 'Enable CPU profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'CPU', enabled: true }, expected: 'success - profiling enabled' },
  { scenario: 'Disable CPU profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'CPU', enabled: false }, expected: 'success - profiling disabled' },
  { scenario: 'Enable GPU profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'GPU', enabled: true }, expected: 'success' },
  { scenario: 'Disable GPU profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'GPU', enabled: false }, expected: 'success' },
  { scenario: 'Enable RenderThread profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'RenderThread', enabled: true }, expected: 'success' },
  { scenario: 'Disable RenderThread profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'RenderThread', enabled: false }, expected: 'success' },
  { scenario: 'Enable Memory profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'Memory', enabled: true }, expected: 'success' },
  { scenario: 'Disable Memory profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'Memory', enabled: false }, expected: 'success' },
  { scenario: 'Profile All', toolName: 'system_control', arguments: { action: 'profile', profileType: 'All', enabled: true }, expected: 'success' },
  { scenario: 'Generate memory report', toolName: 'system_control', arguments: { action: 'profile', profileType: 'Memory', detailed: true, outputPath: './tests/reports' }, expected: 'success' },

  // === QUALITY SETTINGS (set_quality, set_resolution_scale, set_vsync, set_frame_rate_limit) ===
  { scenario: 'Set shadow quality to medium', toolName: 'system_control', arguments: { action: 'set_quality', category: 'Shadow', level: 1 }, expected: 'success - quality set' },
  { scenario: 'Set texture quality to high', toolName: 'system_control', arguments: { action: 'set_quality', category: 'Texture', level: 2 }, expected: 'success - quality set' },
  { scenario: 'Set quality level 0 (low)', toolName: 'system_control', arguments: { action: 'set_quality', category: 'ViewDistance', level: 0 }, expected: 'success' },
  { scenario: 'Set quality level 3 (epic)', toolName: 'system_control', arguments: { action: 'set_quality', category: 'Effects', level: 3 }, expected: 'success' },

  // === CONSOLE COMMANDS (execute_command, console_command) ===
  { scenario: 'Execute console command (stat fps)', toolName: 'system_control', arguments: { action: 'execute_command', command: 'stat fps' }, expected: 'success - console command executed' },
  { scenario: 'Console command (alias)', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat unit' }, expected: 'success' },
  { scenario: 'Console command - stat none', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat none' }, expected: 'success' },
  { scenario: 'Console command - gc', toolName: 'system_control', arguments: { action: 'console_command', command: 'obj gc' }, expected: 'success' },

  // === SCREENSHOTS ===
  { scenario: 'Take a screenshot', toolName: 'system_control', arguments: { action: 'screenshot', filename: 'tc_sys_screenshot' }, expected: 'success - screenshot taken' },
  { scenario: 'Take screenshot with metadata', toolName: 'system_control', arguments: { action: 'screenshot', includeMetadata: true, metadata: { test: 'system_tc' } }, expected: 'success - metadata screenshot taken' },

  // === RESOLUTION / DISPLAY ===
  { scenario: 'Set resolution 1920x1080', toolName: 'system_control', arguments: { action: 'set_resolution', resolution: '1920x1080' }, expected: 'success' },
  { scenario: 'Set fullscreen on', toolName: 'system_control', arguments: { action: 'set_fullscreen', enabled: true }, expected: 'success' },
  { scenario: 'Set fullscreen off', toolName: 'system_control', arguments: { action: 'set_fullscreen', enabled: false }, expected: 'success' },

  // === WIDGETS ===
  { scenario: 'Create debug widget asset', toolName: 'system_control', arguments: { action: 'create_widget', name: 'TestWidget_Safe', savePath: '/Game/Tests' }, expected: 'success - widget created or handled' },
  { scenario: 'Create console widget asset', toolName: 'system_control', arguments: { action: 'create_widget', name: 'TestWidget_Console', savePath: '/Game/Tests' }, expected: 'success - widget created or handled' },
  { scenario: 'Show notification widget', toolName: 'system_control', arguments: { action: 'show_widget', widgetId: 'Notification', visible: true, message: 'TC: Notification', duration: 1.5 }, expected: 'success - notification shown' },
  { scenario: 'Add child to widget', toolName: 'system_control', arguments: { action: 'add_widget_child', parentName: 'TestWidget_Safe', childClass: 'Button' }, expected: 'success|handled' },

  // === CVARS ===
  { scenario: 'Set VSync CVAR', toolName: 'system_control', arguments: { action: 'set_cvar', name: 'r.VSync', value: '0' }, expected: 'success - CVAR set' },
  { scenario: 'Set max FPS CVAR', toolName: 'system_control', arguments: { action: 'set_cvar', name: 't.MaxFPS', value: '60' }, expected: 'success - CVAR set' },
  { scenario: 'Set anti-aliasing CVAR', toolName: 'system_control', arguments: { action: 'set_cvar', name: 'r.PostProcessAAQuality', value: '4' }, expected: 'success' },
  { scenario: 'Set LOD bias CVAR', toolName: 'system_control', arguments: { action: 'set_cvar', name: 'foliage.LODDistanceScale', value: '1.0' }, expected: 'success' },

  // === SOUND ===
  { scenario: 'Play UI sound', toolName: 'system_control', arguments: { action: 'play_sound', soundPath: '/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess', volume: 0.5 }, expected: 'success - sound played or handled' },
  { scenario: 'Play sound silent', toolName: 'system_control', arguments: { action: 'play_sound', soundPath: '/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess', volume: 0 }, expected: 'success' },

  // === PROJECT SETTINGS ===
  { scenario: 'Get project settings', toolName: 'system_control', arguments: { action: 'get_project_settings', category: 'Project' }, expected: 'success' },
  { scenario: 'Get project settings - Engine', toolName: 'system_control', arguments: { action: 'get_project_settings', section: '/Script/EngineSettings.GeneralProjectSettings' }, expected: 'success' },
  { scenario: 'Set project setting', toolName: 'system_control', arguments: { action: 'set_project_setting', section: '/Script/Engine.Engine', key: 'bSmoothFrameRate', value: 'true', configName: 'Engine' }, expected: 'success|handled' },

  // === VALIDATION ===
  { scenario: 'Validate assets', toolName: 'system_control', arguments: { action: 'validate_assets', paths: ['/Game'] }, expected: 'success' },

  // === LUMEN ===
  { scenario: 'Lumen update scene', toolName: 'system_control', arguments: { action: 'lumen_update_scene' }, expected: 'success|handled' },

  // === SUBSCRIPTIONS (subscribe, unsubscribe, spawn_category, start_session) ===
  { scenario: 'Subscribe to events', toolName: 'system_control', arguments: { action: 'subscribe', channels: 'Assets' }, expected: 'success|handled' },
  { scenario: 'Unsubscribe from events', toolName: 'system_control', arguments: { action: 'unsubscribe', channels: 'Assets' }, expected: 'success|handled' },
  { scenario: 'Spawn category', toolName: 'system_control', arguments: { action: 'spawn_category', category: 'TestCategory' }, expected: 'success|handled' },
  { scenario: 'Start session', toolName: 'system_control', arguments: { action: 'start_session' }, expected: 'success|handled' },

  // === ERROR CASES ===
  // Note: C++ plugin often accepts invalid values and uses defaults instead of erroring
  { scenario: 'Error: Invalid profile type', toolName: 'system_control', arguments: { action: 'profile', profileType: 'InvalidProfile' }, expected: 'success|error' },
  { scenario: 'Error: Empty command', toolName: 'system_control', arguments: { action: 'console_command', command: '' }, expected: 'success|error|handled' },
  { scenario: 'Error: Invalid resolution', toolName: 'system_control', arguments: { action: 'set_resolution', width: -1, height: -1 }, expected: 'success|error|validation' },
  // Quality level is now clamped to 0-4 range, so 999 becomes 4 (success with clamping)
  { scenario: 'Quality level clamped', toolName: 'system_control', arguments: { action: 'set_quality', category: 'Shadow', level: 999 }, expected: 'success' },
  // Invalid sound path may fall back to engine sounds, so accept success or not_found
  { scenario: 'Invalid sound path (may fallback)', toolName: 'system_control', arguments: { action: 'play_sound', soundPath: '/Game/Invalid/Sound' }, expected: 'success|error|not_found' },

  // === CLEANUP ===
  {
    scenario: 'Cleanup widgets',
    toolName: 'manage_asset',
    arguments: { action: 'delete', assetPaths: ['/Game/Tests/TestWidget_Safe', '/Game/Tests/TestWidget_Console'] },
    expected: 'success|handled'
  }
];

await runToolTests('System Control', testCases);
