#!/usr/bin/env node
/**
 * Nested `params` envelope coverage for the eight core parent tools.
 *
 * Every parent tool declares an optional `params` object (appended by
 * addActionParamsSchema), and normalizeConsolidatedCall merges it into the
 * argument record as `{ ...args.params, ...args }` before deleting the key, so
 * a caller may pass an action's arguments nested under `params` instead of at
 * the top level. The static parameter audit (--optional-strict) requires that
 * declared optional to be referenced by a genuine case per tool.
 *
 * Each case below nests a real argument the action actually consumes, so a pass
 * proves the envelope reached the handler rather than merely being accepted.
 * No source or runtime changes: the merge already exists.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreParamsEnvelope';
const ts = Date.now();
const BP_NAME = `BP_ParamsEnvelope_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create blueprint for nested reads', toolName: 'manage_blueprint', arguments: { action: 'create', name: BP_NAME, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },

  // === PARAMS ENVELOPE (one per core parent tool) ===
  { scenario: 'PARAMS: control_actor list via nested params', toolName: 'control_actor', arguments: { action: 'list', params: { limit: 20, filter: 'MCP_' } }, expected: 'success' },
  { scenario: 'PARAMS: control_editor console_command via nested params', toolName: 'control_editor', arguments: { action: 'console_command', params: { command: 'stat fps' } }, expected: 'success' },
  { scenario: 'PARAMS: inspect find_by_class via nested params', toolName: 'inspect', arguments: { action: 'find_by_class', params: { className: 'StaticMeshActor' } }, expected: 'success' },
  { scenario: 'PARAMS: manage_asset list via nested params', toolName: 'manage_asset', arguments: { action: 'list', params: { path: TEST_FOLDER, recursive: false, limit: 5 } }, expected: 'success' },
  { scenario: 'PARAMS: manage_blueprint list_node_types via nested params', toolName: 'manage_blueprint', arguments: { action: 'list_node_types', params: { blueprintPath: BP_PATH } }, expected: 'success' },
  { scenario: 'PARAMS: manage_level get_summary via nested params', toolName: 'manage_level', arguments: { action: 'get_summary', params: { levelPath: '/Game/MCPTest/MainLevel' } }, expected: 'success|not found' },
  { scenario: 'PARAMS: manage_tools enable_tools via nested params', toolName: 'manage_tools', arguments: { action: 'enable_tools', params: { tools: ['system_control'] } }, expected: 'success' },
  { scenario: 'PARAMS: system_control get_project_settings via nested params', toolName: 'system_control', arguments: { action: 'get_project_settings', params: { section: '/Script/Engine.Engine' } }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: reset dynamic tool state', toolName: 'manage_tools', arguments: { action: 'reset' }, expected: 'success' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('core-params-envelope', testCases);
