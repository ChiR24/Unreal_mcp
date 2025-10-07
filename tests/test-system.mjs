#!/usr/bin/env node
/**
 * System Control Test Suite
 * Tool: system_control
 * Actions: profile, show_fps, set_quality, play_sound, create_widget, show_widget, screenshot, engine_start, engine_quit
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Enable CPU profiling',
    toolName: 'system_control',
    arguments: {
      action: 'profile',
      profileType: 'CPU',
      enabled: true
    },
    expected: 'success - CPU profiling enabled'
  },
  {
    scenario: 'Enable GPU profiling',
    toolName: 'system_control',
    arguments: {
      action: 'profile',
      profileType: 'GPU',
      enabled: true
    },
    expected: 'success - GPU profiling enabled'
  },
  {
    scenario: 'Show FPS display',
    toolName: 'system_control',
    arguments: {
      action: 'show_fps',
      enabled: true
    },
    expected: 'success - FPS shown'
  },
  {
    scenario: 'Hide FPS display',
    toolName: 'system_control',
    arguments: {
      action: 'show_fps',
      enabled: false
    },
    expected: 'success - FPS hidden'
  },
  {
    scenario: 'Set shadow quality to high',
    toolName: 'system_control',
    arguments: {
      action: 'set_quality',
      category: 'Shadow',
      level: 2
    },
    expected: 'success - quality set'
  },
  {
    scenario: 'Set texture quality to epic',
    toolName: 'system_control',
    arguments: {
      action: 'set_quality',
      category: 'Texture',
      level: 3
    },
    expected: 'success - texture quality set'
  },
  {
    scenario: 'Play 2D sound',
    toolName: 'system_control',
    arguments: {
      action: 'play_sound',
      soundPath: '/Game/Audio/SFX_Click',
      volume: 1.0
    },
    expected: 'success - sound played'
  },
  {
    scenario: 'Play 3D sound at location',
    toolName: 'system_control',
    arguments: {
      action: 'play_sound',
      soundPath: '/Game/Audio/SFX_Explosion',
      location: { x: 0, y: 0, z: 100 },
      volume: 0.8
    },
    expected: 'success - 3D sound played'
  },
  {
    scenario: 'Take screenshot',
    toolName: 'system_control',
    arguments: {
      action: 'screenshot'
    },
    expected: 'success - screenshot taken'
  },
  {
    scenario: 'Create UI widget',
    toolName: 'system_control',
    arguments: {
      action: 'create_widget',
      widgetPath: '/Game/UI/WBP_HUD',
      owningPlayer: 0
    },
    expected: 'success - widget created'
  }
];

await runToolTests('System Control', testCases);
