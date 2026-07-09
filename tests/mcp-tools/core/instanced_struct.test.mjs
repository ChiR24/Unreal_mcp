#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/StructProperty';
const ts = Date.now();

// These scenarios require a live Unreal Editor with the McpAutomationBridge
// plugin. They validate the FInstancedStruct property access surface that the
// C++ handler exposes under manage_asset.
const testCases = [
  // === GET on a known instanced-struct property ===
  {
    scenario: 'INSTANCED STRUCT: get_instanced_struct_property (initialized)',
    toolName: 'manage_asset',
    arguments: {
      action: 'get_instanced_struct_property',
      assetPath: `${TEST_FOLDER}/BP_InstancedHolder_${ts}`,
      propertyName: 'Payload',
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.initialized', equals: true, label: 'instance is initialized' },
      { path: 'structuredContent.result.scriptStruct', includes: 'Struct', label: 'inner struct name reported' },
    ],
  },

  // === SET on a known instanced-struct property ===
  {
    scenario: 'INSTANCED STRUCT: set_instanced_struct_property (string ExportText)',
    toolName: 'manage_asset',
    arguments: {
      action: 'set_instanced_struct_property',
      assetPath: `${TEST_FOLDER}/BP_InstancedHolder_${ts}`,
      propertyName: 'Payload',
      structType: '/Game/MCPTest/StructProperty/S_MyInner_' + ts,
      value: '(Score=42,Label="hello")',
      bSave: true,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.scriptStruct', includes: 'S_MyInner', label: 'inner struct type reported' },
      { path: 'structuredContent.result.saved', equals: true, label: 'saved flag echoed' },
    ],
  },
  {
    scenario: 'INSTANCED STRUCT: set_instanced_struct_property (JSON object)',
    toolName: 'manage_asset',
    arguments: {
      action: 'set_instanced_struct_property',
      assetPath: `${TEST_FOLDER}/BP_InstancedHolder_${ts}`,
      propertyName: 'Payload',
      structType: '/Game/MCPTest/StructProperty/S_MyInner_' + ts,
      value: { Score: 7, Label: 'world' },
      bSave: false,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.saved', equals: false, label: 'unsaved flag echoed' },
    ],
  },

  // === ERROR: property is not an instanced-struct type ===
  {
    scenario: 'INSTANCED STRUCT ERROR: get on a non-instanced property',
    toolName: 'manage_asset',
    arguments: {
      action: 'get_instanced_struct_property',
      assetPath: `${TEST_FOLDER}/BP_InstancedHolder_${ts}`,
      propertyName: 'SomeRegularField',
    },
    expected: 'error',
    assertions: [
      { path: 'structuredContent.error', includes: 'INVALID_OPERATION', label: 'non-instanced property rejected' },
    ],
  },
  {
    scenario: 'INSTANCED STRUCT ERROR: set missing structType',
    toolName: 'manage_asset',
    arguments: {
      action: 'set_instanced_struct_property',
      assetPath: `${TEST_FOLDER}/BP_InstancedHolder_${ts}`,
      propertyName: 'Payload',
      value: '(Score=1)',
    },
    expected: 'error',
    assertions: [
      { path: 'structuredContent.error', includes: 'MISSING_PARAMETER', label: 'missing structType reported' },
    ],
  },
  {
    scenario: 'INSTANCED STRUCT ERROR: get on missing asset',
    toolName: 'manage_asset',
    arguments: {
      action: 'get_instanced_struct_property',
      assetPath: `${TEST_FOLDER}/BP_DoesNotExist_${ts}`,
      propertyName: 'Payload',
    },
    expected: 'error',
    assertions: [
      { path: 'structuredContent.error', includes: 'ASSET_NOT_FOUND', label: 'missing asset reported' },
    ],
  },
];

runToolTests('manage-asset', testCases);
