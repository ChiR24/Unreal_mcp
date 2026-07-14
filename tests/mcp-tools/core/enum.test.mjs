#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

// These scenarios require a live Unreal Editor with the McpAutomationBridge
// plugin. They validate the UserDefinedEnum authoring surface (issue #510
// enum follow-up) that the C++ Lifecycle.cpp / Values.cpp shards expose under
// the manage_asset tool's enum actions.
const testCases = [
  // === ENUM LIFECYCLE ===
  {
    scenario: 'ENUM: create_enum',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_enum',
      name: `E_MCP_Enum_${ts}`,
      path: TEST_FOLDER,
      save: true,
    },
    expected: 'success',
    captureResult: { key: 'enumPath', fromField: 'result.enumPath' },
    assertions: [
      { path: 'structuredContent.result.enumName', equals: `E_MCP_Enum_${ts}`, label: 'enum name reported' },
    ],
  },
  {
    scenario: 'ENUM: get_enum',
    toolName: 'manage_asset',
    arguments: {
      action: 'get_enum',
      enumPath: '${captured:enumPath}',
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.valueCount', equals: 0, label: 'fresh enum has no values' },
    ],
  },
  // === ENUM VALUES ===
  {
    scenario: 'ENUM: add_enum_value (Red)',
    toolName: 'manage_asset',
    arguments: {
      action: 'add_enum_value',
      enumPath: '${captured:enumPath}',
      valueName: 'Red',
      save: false,
    },
    expected: 'success',
    captureResult: { key: 'redIndex', fromField: 'result.index' },
    assertions: [
      { path: 'structuredContent.result.valueName', equals: 'Red', label: 'added value name reported' },
    ],
  },
  {
    scenario: 'ENUM: add_enum_value (Green)',
    toolName: 'manage_asset',
    arguments: {
      action: 'add_enum_value',
      enumPath: '${captured:enumPath}',
      valueName: 'Green',
      save: false,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.valueCount', equals: 2, label: 'two values present' },
    ],
  },
  {
    scenario: 'ENUM: add_enum_value (Blue)',
    toolName: 'manage_asset',
    arguments: {
      action: 'add_enum_value',
      enumPath: '${captured:enumPath}',
      valueName: 'Blue',
      save: false,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.valueCount', equals: 3, label: 'three values present' },
    ],
  },
  {
    scenario: 'ENUM: rename_enum_value (Red -> Crimson)',
    toolName: 'manage_asset',
    arguments: {
      action: 'rename_enum_value',
      enumPath: '${captured:enumPath}',
      valueName: 'Red',
      newValueName: 'Crimson',
      save: false,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.valueName', equals: 'Crimson', label: 'renamed value reported' },
    ],
  },
  {
    scenario: 'ENUM: set_enum_value_metadata (Green)',
    toolName: 'manage_asset',
    arguments: {
      action: 'set_enum_value_metadata',
      enumPath: '${captured:enumPath}',
      valueName: 'Green',
      key: 'Category',
      value: 'Primary',
      save: false,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.value', equals: 'Primary', label: 'metadata value applied' },
    ],
  },
  {
    scenario: 'ENUM: reorder_enum_values (Green, Crimson, Blue)',
    toolName: 'manage_asset',
    arguments: {
      action: 'reorder_enum_values',
      enumPath: '${captured:enumPath}',
      order: ['Green', 'Crimson', 'Blue'],
      save: false,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.reordered', equals: true, label: 'reorder applied' },
    ],
  },
  {
    scenario: 'ENUM: remove_enum_value (Blue)',
    toolName: 'manage_asset',
    arguments: {
      action: 'remove_enum_value',
      enumPath: '${captured:enumPath}',
      valueName: 'Blue',
      save: true,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.removed', equals: true, label: 'value removed flag' },
    ],
  },
  {
    scenario: 'ENUM: split_enum (extract Green + Crimson)',
    toolName: 'manage_asset',
    arguments: {
      action: 'split_enum',
      enumPath: '${captured:enumPath}',
      newEnumName: `E_MCP_EnumSplit_${ts}`,
      values: ['Green', 'Crimson'],
      path: TEST_FOLDER,
      save: true,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.valueCount', equals: 2, label: 'split enum has two values' },
    ],
  },

  // === ENUM ERROR PATHS ===
  {
    scenario: 'ENUM ERROR: create_enum missing name',
    toolName: 'manage_asset',
    arguments: {
      action: 'create_enum',
      path: TEST_FOLDER,
    },
    expected: 'error',
    assertions: [
      { path: 'structuredContent.error', includes: 'MISSING_PARAMETER', label: 'missing name reported' },
    ],
  },
  {
    scenario: 'ENUM ERROR: add_enum_value missing valueName',
    toolName: 'manage_asset',
    arguments: {
      action: 'add_enum_value',
      enumPath: '${captured:enumPath}',
    },
    expected: 'error',
    assertions: [
      { path: 'structuredContent.error', includes: 'MISSING_PARAMETER', label: 'missing value name reported' },
    ],
  },
  {
    scenario: 'ENUM ERROR: get_enum on missing enum',
    toolName: 'manage_asset',
    arguments: {
      action: 'get_enum',
      enumPath: `${TEST_FOLDER}/E_MCP_NoSuchEnum_${ts}`,
    },
    expected: 'error',
    assertions: [
      { path: 'structuredContent.error', includes: 'Enum not found', label: 'missing enum reported' },
    ],
  },

  // === ENUM LIFECYCLE DELETE (after all edits and error paths complete) ===
  {
    scenario: 'ENUM: delete_enum',
    toolName: 'manage_asset',
    arguments: {
      action: 'delete_enum',
      enumPath: '${captured:enumPath}',
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.deleted', equals: true, label: 'enum deleted flag' },
    ],
  },

  // === CLEANUP ===
  {
    scenario: 'Cleanup: delete test folder',
    toolName: 'manage_asset',
    arguments: {
      action: 'delete',
      path: TEST_FOLDER,
      force: true,
    },
    expected: 'success|not found',
  },
];

runToolTests('manage-asset', testCases);
