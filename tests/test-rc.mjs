#!/usr/bin/env node
/**
 * Remote Control Test Suite
 * Tool: manage_rc
 * Actions: create_preset, expose_actor, expose_property, list_fields, set_property, get_property
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Create RC preset',
    toolName: 'manage_rc',
    arguments: {
      action: 'create_preset',
      presetName: 'TestPreset'
    },
    expected: 'success - preset created'
  },
  {
    scenario: 'Expose actor to RC',
    toolName: 'manage_rc',
    arguments: {
      action: 'expose_actor',
      presetName: 'TestPreset',
      actorPath: '/Game/Maps/TestLevel.TestLevel:PersistentLevel.StaticMeshActor_1'
    },
    expected: 'success - actor exposed'
  },
  {
    scenario: 'Expose actor property',
    toolName: 'manage_rc',
    arguments: {
      action: 'expose_property',
      presetName: 'TestPreset',
      actorPath: '/Game/Maps/TestLevel.TestLevel:PersistentLevel.StaticMeshActor_1',
      propertyPath: 'RelativeLocation'
    },
    expected: 'success - property exposed'
  },
  {
    scenario: 'List RC fields',
    toolName: 'manage_rc',
    arguments: {
      action: 'list_fields',
      presetName: 'TestPreset'
    },
    expected: 'success - fields listed'
  },
  {
    scenario: 'Set actor location via RC',
    toolName: 'manage_rc',
    arguments: {
      action: 'set_property',
      presetName: 'TestPreset',
      propertyPath: 'StaticMeshActor_1.RelativeLocation',
      value: { x: 100, y: 200, z: 300 }
    },
    expected: 'success - property set'
  },
  {
    scenario: 'Get property value via RC',
    toolName: 'manage_rc',
    arguments: {
      action: 'get_property',
      presetName: 'TestPreset',
      propertyPath: 'StaticMeshActor_1.RelativeLocation'
    },
    expected: 'success - property retrieved'
  },
  {
    scenario: 'Set boolean property via RC',
    toolName: 'manage_rc',
    arguments: {
      action: 'set_property',
      presetName: 'TestPreset',
      propertyPath: 'Light_1.bVisible',
      value: true
    },
    expected: 'success - boolean property set'
  },
  {
    scenario: 'Set color property via RC',
    toolName: 'manage_rc',
    arguments: {
      action: 'set_property',
      presetName: 'TestPreset',
      propertyPath: 'Light_1.LightColor',
      value: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 }
    },
    expected: 'success - color property set'
  },
  {
    scenario: 'Expose multiple properties at once',
    toolName: 'manage_rc',
    arguments: {
      action: 'expose_property',
      presetName: 'TestPreset',
      actorPath: '/Game/Maps/TestLevel.TestLevel:PersistentLevel.StaticMeshActor_2',
      propertyPath: 'RelativeScale3D'
    },
    expected: 'success - scale property exposed'
  },
  {
    scenario: 'Get float property via RC',
    toolName: 'manage_rc',
    arguments: {
      action: 'get_property',
      presetName: 'TestPreset',
      propertyPath: 'Light_1.Intensity'
    },
    expected: 'success - float value retrieved'
  }
];

await runToolTests('Remote Control', testCases);
