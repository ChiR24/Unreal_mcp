#!/usr/bin/env node
/**
 * Condensed System Control Test Suite (15 safe cases)
 * Tool: system_control
 * This file avoids destructive operations (engine quit/start) and focuses on
 * safe diagnostics, screenshots, widget operations, CVARs, and profiling toggles.
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Enable FPS display', toolName: 'system_control', arguments: { action: 'show_fps', enabled: true }, expected: 'success - FPS shown' },
  { scenario: 'Disable FPS display', toolName: 'system_control', arguments: { action: 'show_fps', enabled: false }, expected: 'success - FPS hidden' },
  { scenario: 'Enable CPU profiling (brief)', toolName: 'system_control', arguments: { action: 'profile', profileType: 'CPU', enabled: true }, expected: 'success - profiling enabled' },
  { scenario: 'Disable CPU profiling', toolName: 'system_control', arguments: { action: 'profile', profileType: 'CPU', enabled: false }, expected: 'success - profiling disabled' },
  { scenario: 'Set shadow quality to medium', toolName: 'system_control', arguments: { action: 'set_quality', category: 'Shadow', level: 1 }, expected: 'success - quality set' },
  { scenario: 'Set texture quality to high', toolName: 'system_control', arguments: { action: 'set_quality', category: 'Texture', level: 2 }, expected: 'success - quality set' },
  { scenario: 'Execute console command (stat fps)', toolName: 'system_control', arguments: { action: 'execute_command', command: 'stat fps' }, expected: 'success - console command executed' },
  { scenario: 'Take a screenshot', toolName: 'system_control', arguments: { action: 'screenshot', filename: './tests/reports/tc_sys_screenshot' }, expected: 'success - screenshot taken' },
  { scenario: 'Take screenshot with metadata', toolName: 'system_control', arguments: { action: 'screenshot', includeMetadata: true, metadata: { test: 'system_tc' } }, expected: 'success - metadata screenshot taken' },
  { scenario: 'Create debug widget (no-op if missing)', toolName: 'system_control', arguments: { action: 'create_widget', widgetPath: '/Engine/EditorWidgets/WBP_Debug', owningPlayer: 0 }, expected: 'success - widget created or handled' },
  { scenario: 'Show short notification', toolName: 'system_control', arguments: { action: 'show_widget', widgetId: 'Notification', visible: true, message: 'TC: Notification', duration: 1.5 }, expected: 'success - notification shown' },
  { scenario: 'Set VSync CVAR', toolName: 'system_control', arguments: { action: 'set_cvar', name: 'r.VSync', value: '0' }, expected: 'success - CVAR set' },
  { scenario: 'Set max FPS CVAR', toolName: 'system_control', arguments: { action: 'set_cvar', name: 't.MaxFPS', value: '60' }, expected: 'success - CVAR set' },
  { scenario: 'Play short UI sound (best-effort)', toolName: 'system_control', arguments: { action: 'play_sound', soundPath: '/Engine/EditorSounds/Generic', volume: 0.5 }, expected: 'success - sound played or handled' },
{ scenario: 'Create & remove lightweight console widget (cleanup)', toolName: 'system_control', arguments: { action: 'create_widget', widgetPath: '/Engine/EditorWidgets/WBP_Console', owningPlayer: 0 }, expected: 'success - widget created or handled' },
  // Additional
  { scenario: 'Toggle fullscreen', toolName: 'system_control', arguments: { action: 'set_fullscreen', enabled: false }, expected: 'success or handled' },
  { scenario: 'Set resolution 1920x1080', toolName: 'system_control', arguments: { action: 'set_resolution', width: 1920, height: 1080, windowed: true }, expected: 'success or handled' },
  { scenario: 'Set ViewDistanceScale', toolName: 'system_control', arguments: { action: 'set_cvar', name: 'r.ViewDistanceScale', value: '1' }, expected: 'success or handled' },
  { scenario: 'Show memory stat', toolName: 'system_control', arguments: { action: 'execute_command', command: 'stat memory' }, expected: 'success or handled' },
  { scenario: 'Capture GPU trace (best-effort)', toolName: 'system_control', arguments: { action: 'execute_command', command: 'profilegpu' }, expected: 'success or handled' }
];

await runToolTests('System Control', testCases);
