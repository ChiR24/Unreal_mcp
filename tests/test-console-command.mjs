#!/usr/bin/env node
/**
 * Condensed Console Command Test Suite (15 cases)
 * Tool: console_command
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  { scenario: 'Show FPS stats', toolName: 'console_command', arguments: { command: 'stat fps' }, expected: 'success - command executed' },
  { scenario: 'Show unit stats', toolName: 'console_command', arguments: { command: 'stat unit' }, expected: 'success - unit stats shown' },
  { scenario: 'Set resolution 1280x720 windowed', toolName: 'console_command', arguments: { command: 'r.SetRes 1280x720w' }, expected: 'success - resolution set' },
  { scenario: 'Enable wireframe view', toolName: 'console_command', arguments: { command: 'viewmode wireframe' }, expected: 'success - wireframe enabled' },
  { scenario: 'Enable lit view', toolName: 'console_command', arguments: { command: 'viewmode lit' }, expected: 'success - lit view enabled' },
  { scenario: 'Show GPU profiler', toolName: 'console_command', arguments: { command: 'profilegpu' }, expected: 'success - GPU profiler shown' },
  { scenario: 'Toggle collision visibility', toolName: 'console_command', arguments: { command: 'show collision' }, expected: 'success - collision visibility toggled' },
  { scenario: 'Set screen percentage to 100', toolName: 'console_command', arguments: { command: 'r.ScreenPercentage 100' }, expected: 'success - screen percentage set' },
  { scenario: 'Enable temporal AA weight', toolName: 'console_command', arguments: { command: 'r.TemporalAACurrentFrameWeight 0.04' }, expected: 'success - TAA weight set' },
  { scenario: 'Set VSync on', toolName: 'console_command', arguments: { command: 'r.VSync 1' }, expected: 'success - VSync enabled' },
  { scenario: 'Set max FPS to 60', toolName: 'console_command', arguments: { command: 't.MaxFPS 60' }, expected: 'success - max FPS set' },
  { scenario: 'Show RHI stats', toolName: 'console_command', arguments: { command: 'stat rhi' }, expected: 'success - RHI stats shown' },
  { scenario: 'Show game thread stats', toolName: 'console_command', arguments: { command: 'stat game' }, expected: 'success - game thread stats shown' },
  { scenario: 'Show animation stats', toolName: 'console_command', arguments: { command: 'stat anim' }, expected: 'success - animation stats shown' },
{ scenario: 'Reset viewmode to lit', toolName: 'console_command', arguments: { command: 'viewmode lit' }, expected: 'success - viewmode reset' },
  // Additional
  { scenario: 'Show GPU', toolName: 'console_command', arguments: { command: 'stat gpu' }, expected: 'success or handled' },
  { scenario: 'Show memory', toolName: 'console_command', arguments: { command: 'stat memory' }, expected: 'success or handled' },
  { scenario: 'Set Mip LOD Bias', toolName: 'console_command', arguments: { command: 'r.MipMapLODBias 0' }, expected: 'success or handled' },
  { scenario: 'Set AA Quality', toolName: 'console_command', arguments: { command: 'r.PostProcessAAQuality 4' }, expected: 'success or handled' },
  { scenario: 'Flush rendering commands', toolName: 'console_command', arguments: { command: 'r.FlushRenderingCommands' }, expected: 'success or handled' }
];

await runToolTests('Console Command', testCases);
