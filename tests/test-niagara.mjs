#!/usr/bin/env node
/**
 * Effects & Visual Test Suite - Expanded
 * Tool: create_effect
 * Actions: particle, niagara, debug_shape
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Create fire particle effect',
    toolName: 'create_effect',
    arguments: {
      action: 'particle',
      preset: 'Fire',
      location: { x: 0, y: 0, z: 100 }
    },
    expected: 'success - fire effect created'
  },
  {
    scenario: 'Create explosion particle',
    toolName: 'create_effect',
    arguments: {
      action: 'particle',
      preset: 'Explosion',
      location: { x: 1000, y: 0, z: 100 }
    },
    expected: 'success - explosion created'
  },
  {
    scenario: 'Create smoke particle',
    toolName: 'create_effect',
    arguments: {
      action: 'particle',
      preset: 'Smoke',
      location: { x: 2000, y: 0, z: 100 }
    },
    expected: 'success - smoke created'
  },
  {
    scenario: 'Create waterfall effect',
    toolName: 'create_effect',
    arguments: {
      action: 'particle',
      preset: 'Waterfall',
      location: { x: 3000, y: 0, z: 500 }
    },
    expected: 'success - waterfall created'
  },
  {
    scenario: 'Create custom Niagara system',
    toolName: 'create_effect',
    arguments: {
      action: 'niagara',
      systemPath: '/Game/VFX/NS_CustomEffect',
      location: { x: 0, y: 1000, z: 0 },
      autoActivate: true
    },
    expected: 'success - custom system spawned'
  },
  {
    scenario: 'Create Niagara with parameters',
    toolName: 'create_effect',
    arguments: {
      action: 'niagara',
      systemPath: '/Game/VFX/NS_MagicSpell',
      location: { x: 1000, y: 1000, z: 0 },
      autoActivate: true,
      parameters: {
        Color: { r: 0.5, g: 0.2, b: 1.0, a: 1.0 },
        Size: 2.0
      }
    },
    expected: 'success - magic spell created'
  },
  {
    scenario: 'Create debug line',
    toolName: 'create_effect',
    arguments: {
      action: 'debug_shape',
      shapeType: 'Line',
      start: { x: 0, y: 0, z: 0 },
      end: { x: 1000, y: 0, z: 0 },
      color: { r: 1, g: 0, b: 0, a: 1 },
      thickness: 3
    },
    expected: 'success - debug line drawn'
  },
  {
    scenario: 'Create debug box',
    toolName: 'create_effect',
    arguments: {
      action: 'debug_shape',
      shapeType: 'Box',
      location: { x: 0, y: 0, z: 100 },
      extent: { x: 100, y: 100, z: 100 },
      color: { r: 0, g: 1, b: 0, a: 0.5 }
    },
    expected: 'success - debug box created'
  },
  {
    scenario: 'Create debug sphere',
    toolName: 'create_effect',
    arguments: {
      action: 'debug_shape',
      shapeType: 'Sphere',
      location: { x: 500, y: 500, z: 100 },
      radius: 150,
      color: { r: 0, g: 0, b: 1, a: 1 }
    },
    expected: 'success - debug sphere created'
  },
  {
    scenario: 'Create debug capsule',
    toolName: 'create_effect',
    arguments: {
      action: 'debug_shape',
      shapeType: 'Capsule',
      location: { x: 1000, y: 500, z: 100 },
      radius: 50,
      halfHeight: 100,
      color: { r: 1, g: 1, b: 0, a: 1 }
    },
    expected: 'success - debug capsule created'
  },
  {
    scenario: 'Create debug arrow',
    toolName: 'create_effect',
    arguments: {
      action: 'debug_shape',
      shapeType: 'Arrow',
      start: { x: 0, y: 0, z: 200 },
      end: { x: 500, y: 0, z: 200 },
      color: { r: 1, g: 0.5, b: 0, a: 1 },
      thickness: 5
    },
    expected: 'success - debug arrow drawn'
  },
  {
    scenario: 'Create debug cone',
    toolName: 'create_effect',
    arguments: {
      action: 'debug_shape',
      shapeType: 'Cone',
      location: { x: 1500, y: 1000, z: 0 },
      radius: 200,
      height: 500,
      color: { r: 0.5, g: 0, b: 0.5, a: 1 }
    },
    expected: 'success - debug cone created'
  }
];

await runToolTests('Effects & Visual', testCases);
