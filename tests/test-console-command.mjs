#!/usr/bin/env node
/**
 * Condensed Console Command Test Suite (15 cases)
 * Tool: system_control (action: console_command)
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Show FPS stats', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat fps' }, expected: 'success - command executed' },
  { scenario: 'Show unit stats', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat unit' }, expected: 'success - unit stats shown' },
  { scenario: 'Set resolution 1280x720 windowed', toolName: 'system_control', arguments: { action: 'console_command', command: 'r.SetRes 1280x720w' }, expected: 'success - resolution set' },
  { scenario: 'Enable wireframe view', toolName: 'system_control', arguments: { action: 'console_command', command: 'viewmode wireframe' }, expected: 'success - wireframe enabled' },
  { scenario: 'Enable lit view', toolName: 'system_control', arguments: { action: 'console_command', command: 'viewmode lit' }, expected: 'success - lit view enabled' },
  { scenario: 'Show GPU profiler', toolName: 'system_control', arguments: { action: 'console_command', command: 'profilegpu' }, expected: 'success - GPU profiler shown' },
  { scenario: 'Toggle collision visibility', toolName: 'system_control', arguments: { action: 'console_command', command: 'show collision' }, expected: 'success - collision visibility toggled' },
  { scenario: 'Set screen percentage to 100', toolName: 'system_control', arguments: { action: 'console_command', command: 'r.ScreenPercentage 100' }, expected: 'success - screen percentage set' },
  { scenario: 'Enable temporal AA weight', toolName: 'system_control', arguments: { action: 'console_command', command: 'r.TemporalAACurrentFrameWeight 0.04' }, expected: 'success - TAA weight set' },
  { scenario: 'Set VSync on', toolName: 'system_control', arguments: { action: 'console_command', command: 'r.VSync 1' }, expected: 'success - VSync enabled' },
  { scenario: 'Set max FPS to 60', toolName: 'system_control', arguments: { action: 'console_command', command: 't.MaxFPS 60' }, expected: 'success - max FPS set' },
  { scenario: 'Show RHI stats', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat rhi' }, expected: 'success - RHI stats shown' },
  { scenario: 'Show game thread stats', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat game' }, expected: 'success - game thread stats shown' },
  { scenario: 'Show animation stats', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat anim' }, expected: 'success - animation stats shown' },
  { scenario: 'Reset viewmode to lit', toolName: 'system_control', arguments: { action: 'console_command', command: 'viewmode lit' }, expected: 'success - viewmode reset' },
  { scenario: 'Show GPU', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat gpu' }, expected: 'success or handled' },
  { scenario: 'Show memory', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat memory' }, expected: 'success or handled' },
  { scenario: 'Set Mip LOD Bias', toolName: 'system_control', arguments: { action: 'console_command', command: 'r.MipMapLODBias 0' }, expected: 'success or handled' },
  { scenario: 'Set AA Quality', toolName: 'system_control', arguments: { action: 'console_command', command: 'r.PostProcessAAQuality 4' }, expected: 'success or handled' },
  { scenario: 'Flush rendering commands', toolName: 'system_control', arguments: { action: 'console_command', command: 'r.FlushRenderingCommands' }, expected: 'success or handled' },
  {
    scenario: "Blocked: Dangerous quit",
    toolName: "system_control",
    arguments: { action: "console_command", command: "quit" },
    expected: "blocked|dangerous"
  },
  {
    scenario: "Error: Empty command",
    toolName: "system_control",
    arguments: { action: "console_command", command: "" },
    expected: "error|empty"
  },
  {
    scenario: "Edge: Very long safe command",
    toolName: "system_control",
    arguments: { action: "console_command", command: "stat fps; stat gpu; stat memory" },
    expected: "blocked|command_blocked|blocked for safety"
  },
  {
    scenario: "Warning: Unknown command",
    toolName: "system_control",
    arguments: { action: "console_command", command: "unknowncmd" },
    expected: "warning|unknown"
  }
];

await runToolTests('Console Command', testCases);
