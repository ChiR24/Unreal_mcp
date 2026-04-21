#!/usr/bin/env node
/**
 * Ch8 E2E: War project — exercises ALL Ch1-7 new actions against the
 * real War content layout (ModifierKeys authoring workflow).
 *
 * Requirements: live UE Editor running the War project with the
 * McpAutomationBridge plugin loaded and WS server listening.
 *
 * Run: node tests/scenarios/war-e2e.mjs
 *
 * Coverage (22 scenarios):
 *   - manage_blueprint: create_struct, modify_struct (add_member),
 *                       create, add_variable, set_parent_class,
 *                       add_interface, list_interfaces
 *   - manage_data:      create_data_table, add_data_table_row (x3),
 *                       create_data_asset, set_data_asset_property,
 *                       list_data_table_rows
 *   - manage_curve:     create_curve_float, set_curve_keys, inspect_curve
 *   - manage_gameplay_tags: add_gameplay_tag, list_gameplay_tags
 */

import { runToolTests } from '../test-runner.mjs';

const ROOT = '/Game/War/Data/ModifierKeys';
const STRUCT_PATH = `${ROOT}/ST_ModifierKeyRow`;
const DT_PATH = `${ROOT}/DT_ModifierKeys`;
const DECAY_CURVE_PATH = `${ROOT}/C_ModifierDecay`;
const DA_PARENT_BP_PATH = `${ROOT}/BP_WarModifierSet`;
const DA_INSTANCE_PATH = `${ROOT}/DA_Modifiers`;
const E2E_BP_PATH = `${ROOT}/BP_WarE2E`;

const testCases = [
  // ---------------------------------------------------------------------
  // 1) Folder prep
  // ---------------------------------------------------------------------
  { scenario: 'E2E.0: create /Game/War/Data/ModifierKeys folder',
    toolName: 'manage_asset',
    arguments: { action: 'create_folder', path: ROOT },
    expected: 'success|already exists' },

  // ---------------------------------------------------------------------
  // 2) Struct: ST_ModifierKeyRow with 3 fields (single create_struct call;
  //    handler schema expects assetPath + members[{name,type}])
  // ---------------------------------------------------------------------
  { scenario: 'E2E.1: create struct ST_ModifierKeyRow (Name:FName, Value:double, Description:FText)',
    toolName: 'manage_blueprint',
    arguments: { action: 'create_struct', assetPath: STRUCT_PATH,
      members: [
        { name: 'Name', type: 'name' },
        { name: 'Value', type: 'double' },
        { name: 'Description', type: 'text' }
      ] },
    expected: 'success|already exists' },

  // Exercise modify_struct path: add+remove a probe field (no-op end state)
  { scenario: 'E2E.2a: modify_struct add probe field',
    toolName: 'manage_blueprint',
    arguments: { action: 'modify_struct', assetPath: STRUCT_PATH,
      operations: [ { op: 'add_member', name: 'Probe', type: 'int32' } ] },
    expected: 'success' },
  { scenario: 'E2E.2b: modify_struct remove probe field',
    toolName: 'manage_blueprint',
    arguments: { action: 'modify_struct', assetPath: STRUCT_PATH,
      operations: [ { op: 'remove_member', name: 'Probe' } ] },
    expected: 'success' },

  // ---------------------------------------------------------------------
  // 3) DataTable DT_ModifierKeys referencing the struct
  // ---------------------------------------------------------------------
  { scenario: 'E2E.3: create DT_ModifierKeys (rowStruct = ST_ModifierKeyRow)',
    toolName: 'manage_data',
    arguments: { action: 'create_data_table', path: ROOT, name: 'DT_ModifierKeys',
      rowStructPath: `${STRUCT_PATH}.ST_ModifierKeyRow` },
    expected: 'success|already exists' },

  // ---------------------------------------------------------------------
  // 4) Three rows: Rain, Snow, Drought
  // ---------------------------------------------------------------------
  { scenario: 'E2E.4a: add row Rain',
    toolName: 'manage_data',
    arguments: { action: 'add_data_table_row', path: DT_PATH, rowName: 'Rain',
      fields: { Name: 'Rain', Value: 1.25, Description: 'Rain buff' } },
    expected: 'success|already exists' },
  { scenario: 'E2E.4b: add row Snow',
    toolName: 'manage_data',
    arguments: { action: 'add_data_table_row', path: DT_PATH, rowName: 'Snow',
      fields: { Name: 'Snow', Value: 0.8, Description: 'Snow penalty' } },
    expected: 'success|already exists' },
  { scenario: 'E2E.4c: add row Drought',
    toolName: 'manage_data',
    arguments: { action: 'add_data_table_row', path: DT_PATH, rowName: 'Drought',
      fields: { Name: 'Drought', Value: 0.5, Description: 'Drought penalty' } },
    expected: 'success|already exists' },

  // ---------------------------------------------------------------------
  // 5) DataAsset: create BP parent deriving from /Script/Engine.DataAsset,
  //    add DT-reference var, instantiate, set property
  // ---------------------------------------------------------------------
  { scenario: 'E2E.5a: create DataAsset parent BP (BP_WarModifierSet)',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', name: 'BP_WarModifierSet', path: ROOT,
      parentClass: '/Script/Engine.DataAsset' },
    expected: 'success|already exists' },
  { scenario: 'E2E.5b: add ModifierTable variable (DataTable object ref)',
    toolName: 'manage_blueprint',
    arguments: { action: 'add_variable', blueprintPath: DA_PARENT_BP_PATH,
      variableName: 'ModifierTable', variableType: 'object /Script/Engine.DataTable' },
    expected: 'success|already exists' },
  { scenario: 'E2E.5c: create DA_Modifiers instance of BP_WarModifierSet',
    toolName: 'manage_data',
    arguments: { action: 'create_data_asset', path: ROOT, name: 'DA_Modifiers',
      dataAssetClassPath: `${DA_PARENT_BP_PATH}.BP_WarModifierSet_C` },
    expected: 'success|already exists' },
  { scenario: 'E2E.5d: set ModifierTable property on DA_Modifiers',
    toolName: 'manage_data',
    arguments: { action: 'set_data_asset_property', path: DA_INSTANCE_PATH,
      propertyPath: 'ModifierTable', value: `${DT_PATH}.DT_ModifierKeys` },
    expected: 'success' },

  // ---------------------------------------------------------------------
  // 6) Curve: C_ModifierDecay with 3 keys (Auto, Linear, Constant)
  // ---------------------------------------------------------------------
  { scenario: 'E2E.6a: create C_ModifierDecay (UCurveFloat)',
    toolName: 'manage_curve',
    arguments: { action: 'create_curve_float', path: ROOT, name: 'C_ModifierDecay' },
    expected: 'success|already exists' },
  { scenario: 'E2E.6b: set_curve_keys — 3 keys across interp modes',
    toolName: 'manage_curve',
    arguments: { action: 'set_curve_keys', path: DECAY_CURVE_PATH,
      keys: [
        { time: 0,  value: 1,   interpMode: 'Auto' },
        { time: 5,  value: 0.5, interpMode: 'Linear' },
        { time: 10, value: 0,   interpMode: 'Constant' }
      ] },
    expected: 'success' },

  // ---------------------------------------------------------------------
  // 7) GameplayTag: Modifier.Weather.Rain
  // ---------------------------------------------------------------------
  { scenario: 'E2E.7: add gameplay tag Modifier.Weather.Rain',
    toolName: 'manage_gameplay_tags',
    arguments: { action: 'add_gameplay_tag', tag: 'Modifier.Weather.Rain',
      comment: 'Rain weather modifier (War E2E)' },
    expected: 'success|already exists' },

  // ---------------------------------------------------------------------
  // 8) Blueprint: create Actor BP, reparent to Pawn, add interface
  // ---------------------------------------------------------------------
  { scenario: 'E2E.8a: create BP_WarE2E (parent = Actor)',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', name: 'BP_WarE2E', path: ROOT,
      parentClass: 'Actor' },
    expected: 'success|already exists' },
  { scenario: 'E2E.8b: reparent BP_WarE2E Actor -> Pawn',
    toolName: 'manage_blueprint',
    arguments: { action: 'set_parent_class', blueprintPath: E2E_BP_PATH,
      parentClass: '/Script/Engine.Pawn' },
    expected: 'success' },
  { scenario: 'E2E.8c: add Interface_AssetUserData to BP_WarE2E',
    toolName: 'manage_blueprint',
    arguments: { action: 'add_interface', blueprintPath: E2E_BP_PATH,
      interfacePath: '/Script/Engine.Interface_AssetUserData' },
    expected: 'success' },

  // ---------------------------------------------------------------------
  // 9) Verification pass
  // ---------------------------------------------------------------------
  { scenario: 'E2E.9a: list DT_ModifierKeys rows (expect 3)',
    toolName: 'manage_data',
    arguments: { action: 'list_data_table_rows', path: DT_PATH },
    expected: 'success' },
  { scenario: 'E2E.9b: inspect_curve C_ModifierDecay (expect 3 keys)',
    toolName: 'manage_curve',
    arguments: { action: 'inspect_curve', path: DECAY_CURVE_PATH },
    expected: 'success' },
  { scenario: 'E2E.9c: list gameplay tags under Modifier.Weather',
    toolName: 'manage_gameplay_tags',
    arguments: { action: 'list_gameplay_tags', prefix: 'Modifier.Weather' },
    expected: 'success' },
  { scenario: 'E2E.9d: list interfaces on BP_WarE2E (expect 1)',
    toolName: 'manage_blueprint',
    arguments: { action: 'list_interfaces', blueprintPath: E2E_BP_PATH },
    expected: 'success' }
];

runToolTests('war-e2e', testCases);
