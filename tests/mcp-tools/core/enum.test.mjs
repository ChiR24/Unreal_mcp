#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

// These scenarios require a live Unreal Editor with the EngineWire
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
    },
    expected: 'success',
    captureResult: { key: 'enumPath', fromField: 'result.enumPath' },
    assertions: [
      { path: 'structuredContent.result.enumName', equals: `E_MCP_Enum_${ts}`, label: 'enum name reported' },
      { path: 'structuredContent.result.saved', equals: true, label: 'save defaults to true' },
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
      { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step on every UE 5.x' },
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
      { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step on every UE 5.x' },
      { path: 'structuredContent.result.valueName', equals: 'Crimson', label: 'renamed value reported' },
    ],
  },
  {
    scenario: 'ENUM: get_enum shows the new name and keeps the internal one',
    toolName: 'manage_asset',
    arguments: {
      action: 'get_enum',
      enumPath: '${captured:enumPath}',
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.values.0.displayName', equals: 'Crimson', label: 'shown name changed' },
      { path: 'structuredContent.result.values.0.name', equals: 'Red', label: 'internal name kept for stored values' },
    ],
  },
  // Every enum edit undoes and redoes, from UE 5.7 too, where restoring an enum's saved state crashes the editor: the
  // edit records the enum as a change undo writes back.
  {
    scenario: 'ENUM: undo takes the rename back',
    toolName: 'control_editor',
    arguments: { action: 'undo' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.transaction', equals: 'Rename Enum Value', label: 'the rename was one step' }],
  },
  {
    scenario: 'ENUM: get_enum shows the old name after the undo',
    toolName: 'manage_asset',
    arguments: { action: 'get_enum', enumPath: '${captured:enumPath}' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.values.0.displayName', equals: 'Red', label: 'the shown name is back' }],
  },
  {
    scenario: 'ENUM: redo renames it again',
    toolName: 'control_editor',
    arguments: { action: 'redo' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.transaction', equals: 'Rename Enum Value', label: 'the rename is redone' }],
  },
  {
    scenario: 'ENUM: set_property gives the enum a description',
    toolName: 'inspect',
    arguments: {
      action: 'set_property',
      objectPath: '${captured:enumPath}',
      propertyName: 'EnumDescription',
      value: 'Colours for the test',
    },
    expected: 'success',
  },
  {
    scenario: 'ENUM: undo takes the description back (a property set on an enum undoes too)',
    toolName: 'control_editor',
    arguments: { action: 'undo' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.transaction', equals: 'Set Property', label: 'the property set was the step undone' }],
  },
  {
    scenario: 'ENUM: get_property reads the description gone after the undo',
    toolName: 'inspect',
    arguments: { action: 'get_property', objectPath: '${captured:enumPath}', propertyName: 'EnumDescription' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.value', equals: '', label: 'the description is empty again' }],
  },
  {
    scenario: 'ENUM: get_enum keeps the renamed value through the description undo',
    toolName: 'manage_asset',
    arguments: { action: 'get_enum', enumPath: '${captured:enumPath}' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.values.0.displayName', equals: 'Crimson', label: 'only the description was undone' }],
  },
  {
    scenario: 'ENUM ERROR: add_enum_value refuses a name the enum has',
    toolName: 'manage_asset',
    arguments: {
      action: 'add_enum_value',
      enumPath: '${captured:enumPath}',
      valueName: 'Crimson',
      save: false,
    },
    expected: 'error|VALUE_EXISTS',
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
      { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step on every UE 5.x' },
      { path: 'structuredContent.result.reordered', equals: true, label: 'reorder applied' },
    ],
  },
  {
    scenario: 'ENUM: undo puts the old order back in one step',
    toolName: 'control_editor',
    arguments: { action: 'undo' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.transaction', equals: 'Reorder Enum Values', label: 'every move of the reorder was one step' }],
  },
  {
    scenario: 'ENUM: get_enum reads the old order after the undo',
    toolName: 'manage_asset',
    arguments: { action: 'get_enum', enumPath: '${captured:enumPath}' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.values.0.name', equals: 'Red', label: 'Crimson (internally Red) is first again' }],
  },
  {
    scenario: 'ENUM: redo reorders again',
    toolName: 'control_editor',
    arguments: { action: 'redo' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.transaction', equals: 'Reorder Enum Values', label: 'the reorder is redone' }],
  },
  {
    scenario: 'ENUM: get_enum reads the new order after the redo',
    toolName: 'manage_asset',
    arguments: { action: 'get_enum', enumPath: '${captured:enumPath}' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.values.0.name', equals: 'Green', label: 'Green is first again' }],
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
      { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step on every UE 5.x' },
      { path: 'structuredContent.result.removed', equals: true, label: 'value removed flag' },
    ],
  },
  {
    scenario: 'ENUM: undo brings the removed value back',
    toolName: 'control_editor',
    arguments: { action: 'undo' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.transaction', equals: 'Remove Enum Value', label: 'the remove was one step' }],
  },
  {
    scenario: 'ENUM: get_enum counts three values after the undo',
    toolName: 'manage_asset',
    arguments: { action: 'get_enum', enumPath: '${captured:enumPath}' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.valueCount', equals: 3, label: 'Blue is back' }],
  },
  {
    scenario: 'ENUM: redo removes it again',
    toolName: 'control_editor',
    arguments: { action: 'redo' },
    expected: 'success',
    assertions: [{ path: 'structuredContent.result.transaction', equals: 'Remove Enum Value', label: 'the remove is redone' }],
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
  {
    scenario: 'ENUM: split_enum with an explicit split index',
    toolName: 'manage_asset',
    arguments: {
      action: 'split_enum',
      enumPath: '${captured:enumPath}',
      newEnumName: `E_MCP_EnumSplitIdx_${ts}`,
      index: 1,
      path: TEST_FOLDER,
      save: false,
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.valueCount', equals: 1, label: 'every value from index 1 on (Crimson) moves' },
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
      { path: 'structuredContent.error', includes: 'ASSET_NOT_FOUND', label: 'missing enum reported as a failure' },
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
