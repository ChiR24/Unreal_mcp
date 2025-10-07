#!/usr/bin/env node
/**
 * Level Management Test Suite
 * Tool: manage_level
 * Actions: load, save, stream, create_light, build_lighting
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Load level',
    toolName: 'manage_level',
    arguments: {
      action: 'load',
      levelPath: '/Game/Maps/TestMap'
    },
    expected: 'success - level loaded'
  },
  {
    scenario: 'Save current level',
    toolName: 'manage_level',
    arguments: {
      action: 'save'
    },
    expected: 'success - level saved'
  },
  {
    scenario: 'Save level as new',
    toolName: 'manage_level',
    arguments: {
      action: 'save',
      levelPath: '/Game/Maps/NewMap'
    },
    expected: 'success - level saved as'
  },
  {
    scenario: 'Stream sublevel',
    toolName: 'manage_level',
    arguments: {
      action: 'stream',
      levelPath: '/Game/Maps/Sublevel1',
      shouldBeLoaded: true,
      shouldBeVisible: true
    },
    expected: 'success - sublevel streamed'
  },
  {
    scenario: 'Unload sublevel',
    toolName: 'manage_level',
    arguments: {
      action: 'stream',
      levelPath: '/Game/Maps/Sublevel1',
      shouldBeLoaded: false
    },
    expected: 'success - sublevel unloaded'
  },
  {
    scenario: 'Create directional light',
    toolName: 'manage_level',
    arguments: {
      action: 'create_light',
      lightType: 'Directional',
      location: { x: 0, y: 0, z: 500 },
      rotation: { pitch: -60, yaw: 0, roll: 0 },
      intensity: 5.0
    },
    expected: 'success - light created'
  },
  {
    scenario: 'Create point light',
    toolName: 'manage_level',
    arguments: {
      action: 'create_light',
      lightType: 'Point',
      location: { x: 200, y: 200, z: 100 },
      intensity: 5000.0,
      color: { r: 255, g: 200, b: 150 }
    },
    expected: 'success - point light created'
  },
  {
    scenario: 'Create spot light',
    toolName: 'manage_level',
    arguments: {
      action: 'create_light',
      lightType: 'Spot',
      location: { x: 0, y: 0, z: 300 },
      rotation: { pitch: -90, yaw: 0, roll: 0 },
      intensity: 10000.0
    },
    expected: 'success - spot light created'
  },
  {
    scenario: 'Build lighting - Preview quality',
    toolName: 'manage_level',
    arguments: {
      action: 'build_lighting',
      quality: 'Preview'
    },
    expected: 'success - lighting built'
  },
  {
    scenario: 'Build lighting - Production quality',
    toolName: 'manage_level',
    arguments: {
      action: 'build_lighting',
      quality: 'Production'
    },
    expected: 'success - lighting built'
  }
];

await runToolTests('Level Management', testCases);
