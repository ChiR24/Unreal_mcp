#!/usr/bin/env node
/**
 * manage_asset `list` optional-parameter coverage.
 *
 * Static parameter audit (--optional-strict) requires the optional schema
 * parameters `cursor`, `includeTags`, and `pagination` to be referenced by a
 * genuine `list` case. Each is exercised in its own case so the audit sees the
 * parameter key independently:
 *   - `cursor`      depends on a first-page `list` that yields a real cursor,
 *                    then resumes on the SAME path (matching path dependency).
 *   - `includeTags` lists with asset tags requested.
 *   - `pagination`  passes the nested { limit, offset } control.
 *
 * No source/runtime changes: the handlers already forward these flags. Setup
 * creates a folder plus two materials so a limit:1 first page returns a
 * non-null cursor for the dependent second call.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/AssetListOptional';
const ts = Date.now();

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create list-source material A', toolName: 'manage_asset', arguments: { action: 'create_material', name: `M_ListOptA_${ts}`, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create list-source material B', toolName: 'manage_asset', arguments: { action: 'create_material', name: `M_ListOptB_${ts}`, path: TEST_FOLDER }, expected: 'success|already exists' },

  // === OPTIONAL: cursor (depends on a real first-page cursor on the SAME path) ===
  {
    scenario: 'OPTIONAL: list first page (limit 1) captures cursor',
    toolName: 'manage_asset',
    arguments: { action: 'list', path: TEST_FOLDER, recursive: false, limit: 1 },
    expected: 'success',
    captureResult: { key: 'listNextCursor', fromField: 'nextCursor' }
  },
  {
    scenario: 'OPTIONAL: list resume via cursor (matching path)',
    toolName: 'manage_asset',
    arguments: { action: 'list', path: TEST_FOLDER, recursive: false, cursor: '${captured:listNextCursor}' },
    expected: 'success'
  },

  // === OPTIONAL: includeTags (separately parsed) ===
  {
    scenario: 'OPTIONAL: list with includeTags',
    toolName: 'manage_asset',
    arguments: { action: 'list', path: TEST_FOLDER, recursive: true, includeTags: true },
    expected: 'success'
  },

  // === OPTIONAL: nested pagination (separately parsed) ===
  {
    scenario: 'OPTIONAL: list with nested pagination',
    toolName: 'manage_asset',
    arguments: { action: 'list', path: TEST_FOLDER, recursive: true, pagination: { limit: 2, offset: 0 } },
    expected: 'success'
  },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-asset-list-optional', testCases);
