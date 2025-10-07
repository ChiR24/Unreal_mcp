#!/usr/bin/env node
/**
 * Console Command Test Suite
 * Tool: console_command
 * Single command parameter - no action enum
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Show FPS stats',
    toolName: 'console_command',
    arguments: {
      command: 'stat fps'
    },
    expected: 'success - command executed'
  },
  {
    scenario: 'Show unit stats',
    toolName: 'console_command',
    arguments: {
      command: 'stat unit'
    },
    expected: 'success - unit stats shown'
  },
  {
    scenario: 'Show memory stats',
    toolName: 'console_command',
    arguments: {
      command: 'stat memory'
    },
    expected: 'success - memory stats shown'
  },
  {
    scenario: 'Set resolution to 1920x1080',
    toolName: 'console_command',
    arguments: {
      command: 'r.SetRes 1920x1080'
    },
    expected: 'success - resolution set'
  },
  {
    scenario: 'Set resolution to 1280x720 windowed',
    toolName: 'console_command',
    arguments: {
      command: 'r.SetRes 1280x720w'
    },
    expected: 'success - windowed resolution set'
  },
  {
    scenario: 'Enable wireframe view',
    toolName: 'console_command',
    arguments: {
      command: 'viewmode wireframe'
    },
    expected: 'success - wireframe enabled'
  },
  {
    scenario: 'Enable lit view',
    toolName: 'console_command',
    arguments: {
      command: 'viewmode lit'
    },
    expected: 'success - lit view enabled'
  },
  {
    scenario: 'Show GPU visualizer',
    toolName: 'console_command',
    arguments: {
      command: 'profilegpu'
    },
    expected: 'success - GPU profiler shown'
  },
  {
    scenario: 'Toggle collision visibility',
    toolName: 'console_command',
    arguments: {
      command: 'show collision'
    },
    expected: 'success - collision visibility toggled'
  },
  {
    scenario: 'Set screen percentage to 100',
    toolName: 'console_command',
    arguments: {
      command: 'r.ScreenPercentage 100'
    },
    expected: 'success - screen percentage set'
  },
  {
    scenario: 'Enable temporal AA',
    toolName: 'console_command',
    arguments: {
      command: 'r.TemporalAACurrentFrameWeight 0.04'
    },
    expected: 'success - TAA weight set'
  },
  {
    scenario: 'Show render stats',
    toolName: 'console_command',
    arguments: {
      command: 'stat rhi'
    },
    expected: 'success - RHI stats shown'
  }
];

await runToolTests('Console Command', testCases);
