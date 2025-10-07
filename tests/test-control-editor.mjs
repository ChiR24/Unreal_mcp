#!/usr/bin/env node
/**
 * Editor Control Test Suite
 * Tool: control_editor
 * Actions: play, stop, set_camera, set_view_mode
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Start PIE (Play in Editor)',
    toolName: 'control_editor',
    arguments: {
      action: 'play'
    },
    expected: 'success - PIE started'
  },
  {
    scenario: 'Stop PIE',
    toolName: 'control_editor',
    arguments: {
      action: 'stop'
    },
    expected: 'success - PIE stopped'
  },
  {
    scenario: 'Set camera location',
    toolName: 'control_editor',
    arguments: {
      action: 'set_camera',
      location: { x: 0, y: 0, z: 500 },
      rotation: { pitch: -45, yaw: 0, roll: 0 }
    },
    expected: 'success - camera set'
  },
  {
    scenario: 'Focus camera on actor',
    toolName: 'control_editor',
    arguments: {
      action: 'set_camera',
      targetActor: 'MainCube'
    },
    expected: 'success - camera focused'
  },
  {
    scenario: 'Set view mode to Lit',
    toolName: 'control_editor',
    arguments: {
      action: 'set_view_mode',
      viewMode: 'Lit'
    },
    expected: 'success - view mode set'
  },
  {
    scenario: 'Set view mode to Wireframe',
    toolName: 'control_editor',
    arguments: {
      action: 'set_view_mode',
      viewMode: 'Wireframe'
    },
    expected: 'success - view mode set'
  },
  {
    scenario: 'Set view mode to Unlit',
    toolName: 'control_editor',
    arguments: {
      action: 'set_view_mode',
      viewMode: 'Unlit'
    },
    expected: 'success - unlit view enabled'
  },
  {
    scenario: 'Set camera with rotation',
    toolName: 'control_editor',
    arguments: {
      action: 'set_camera',
      location: { x: 1000, y: 1000, z: 500 },
      rotation: { pitch: -30, yaw: 45, roll: 0 }
    },
    expected: 'success - camera positioned and rotated'
  },
  {
    scenario: 'Set view mode to Detail Lighting',
    toolName: 'control_editor',
    arguments: {
      action: 'set_view_mode',
      viewMode: 'DetailLighting'
    },
    expected: 'success - detail lighting view enabled'
  }
];

await runToolTests('Editor Control', testCases);
