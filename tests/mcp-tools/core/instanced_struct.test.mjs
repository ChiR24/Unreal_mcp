#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/StructProperty';
const ts = Date.now();

// These scenarios require a live Unreal Editor with the McpAutomationBridge
// plugin. They validate the FInstancedStruct property access surface that the
// C++ handler exposes under manage_asset.
const testCases = [
  // === SETUP: required assets (issue #struct-ecosystem) ===
  {
    scenario: 'SETUP: create inner struct S_MyInner',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_struct',
      name: `S_MyInner_${ts}`,
      path: TEST_FOLDER,
      save: true,
    },
    expected: 'success',
    captureResult: { key: 'innerStructPath', fromField: 'result.assetPath' },
  },
  {
    scenario: 'SETUP: add Score member to S_MyInner',
    toolName: 'manage_asset',
    arguments: {
      action: 'add_struct_member',
      structPath: '${captured:innerStructPath}',
      memberName: 'Score',
      memberType: 'Int',
      save: false,
    },
    expected: 'success',
  },
  {
    scenario: 'SETUP: add Label member to S_MyInner',
    toolName: 'manage_asset',
    arguments: {
      action: 'add_struct_member',
      structPath: '${captured:innerStructPath}',
      memberName: 'Label',
      memberType: 'String',
      save: true,
    },
    expected: 'success',
  },
  {
    scenario: 'SETUP: create holder blueprint BP_InstancedHolder',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'create_blueprint',
      name: `BP_InstancedHolder_${ts}`,
      path: TEST_FOLDER,
      parentClass: 'Actor',
    },
    expected: 'success|already exists',
  },
  {
    scenario: 'SETUP: add instanced-struct property Payload to holder',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_variable',
      blueprintPath: `${TEST_FOLDER}/BP_InstancedHolder_${ts}`,
      variableName: 'Payload',
      variableType: `Struct:/Game/MCPTest/StructProperty/S_MyInner_${ts}`,
      category: 'MCP',
      isPublic: true,
    },
    expected: 'success|already exists',
  },

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
