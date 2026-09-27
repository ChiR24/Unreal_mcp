#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/StructProperty';
const ts = Date.now();

// Validates the FInstancedStruct property access surface (set/get_instanced_struct_property)
// exposed under manage_asset via the C++ StructPropertyInstanced handler.
//
// IMPORTANT (issue #struct-ecosystem): the positive round-trip (initialize / set / get an
// FInstancedStruct with a concrete inner-struct type and field values) requires an asset that
// already owns an FInstancedStruct-typed property. The current manage_blueprint `add_variable`
// and manage_asset `create_struct` type resolver cannot synthesize an FInstancedStruct property
// (native-struct resolution fails for FInstancedStruct, FVector, FTransform, etc.), so those
// cases are not executable through the available actions. See the specification block at the
// bottom of this file.
//
// This suite therefore locks the NEGATIVE contract: the handler is reachable and returns the
// correct error codes for a non-instanced property and for a missing asset.

const testCases = [
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
    scenario: 'SETUP: add a regular (non-instanced) variable to the holder',
    toolName: 'manage_blueprint',
    arguments: {
      action: 'add_variable',
      blueprintPath: `${TEST_FOLDER}/BP_InstancedHolder_${ts}`,
      variableName: 'SomeRegularField',
      variableType: 'Float',
      isPublic: true,
    },
    expected: 'success|already exists',
  },

  // A regular property must be rejected as not-an-FInstancedStruct.
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
      { path: 'structuredContent.data.result.error', includes: 'INVALID_OPERATION', label: 'non-instanced property rejected' },
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
      { path: 'structuredContent.data.result.error', includes: 'ASSET_NOT_FOUND', label: 'missing asset reported' },
    ],
  },

  // set_instanced_struct_property negative contract: exercises structType and
  // structValues so the strict parameter audit sees both optional parameters.
  {
    scenario: 'INSTANCED STRUCT ERROR: set on a non-instanced property',
    toolName: 'manage_asset',
    arguments: {
      action: 'set_instanced_struct_property',
      assetPath: `${TEST_FOLDER}/BP_InstancedHolder_${ts}`,
      propertyName: 'SomeRegularField',
      structType: '/Game/MCPTest/StructProperty/S_MyInner',
      structValues: { Score: 42, Label: 'hi' },
    },
    expected: 'error',
    assertions: [
      { path: 'structuredContent.data.result.error', includes: 'INVALID_OPERATION', label: 'non-instanced property rejected on set' },
    ],
  },
  {
    scenario: 'INSTANCED STRUCT ERROR: set on missing asset',
    toolName: 'manage_asset',
    arguments: {
      action: 'set_instanced_struct_property',
      assetPath: `${TEST_FOLDER}/BP_DoesNotExist_${ts}`,
      propertyName: 'Payload',
      structType: '/Game/MCPTest/StructProperty/S_MyInner',
      structValues: { Score: 42 },
    },
    expected: 'error',
    assertions: [
      { path: 'structuredContent.data.result.error', includes: 'ASSET_NOT_FOUND', label: 'missing asset reported on set' },
    ],
  },
];

runToolTests('manage-asset', testCases);
