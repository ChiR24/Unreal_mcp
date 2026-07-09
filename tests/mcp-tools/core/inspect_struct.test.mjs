#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

// These scenarios require a live Unreal Editor with the bridge plugin loaded.
// inspect_struct is read-only reflection: it never saves or mutates the target.
const testCases = [
  // === STRUCT INSPECTION (read-only reflection, issue #struct-ecosystem) ===
  { scenario: 'STRUCT: inspect_struct on a known engine struct', toolName: 'inspect', arguments: { action: 'inspect_struct', structPath: '/Script/Engine.Vector' }, expected: 'success', assertions: [{ path: 'structuredContent.result.structName', equals: 'Vector', label: 'engine struct name reported' }, { path: 'structuredContent.result.members', gte: 1, label: 'at least one member enumerated' }] },
  { scenario: 'STRUCT: inspect_struct on a known gameplay struct', toolName: 'inspect', arguments: { action: 'inspect_struct', structPath: '/Script/Engine.Transform' }, expected: 'success', assertions: [{ path: 'structuredContent.result.structName', equals: 'Transform', label: 'gameplay struct name reported' }] },
  { scenario: 'STRUCT ERROR: inspect_struct missing structPath', toolName: 'inspect', arguments: { action: 'inspect_struct' }, expected: 'error', assertions: [{ path: 'structuredContent.error', includes: 'MISSING_PARAMETER', label: 'missing structPath reported' }] },
  { scenario: 'STRUCT ERROR: inspect_struct unresolved struct path', toolName: 'inspect', arguments: { action: 'inspect_struct', structPath: '/Script/Engine.MCP_NonExistent_Struct_XYZ_' + ts }, expected: 'error', assertions: [{ path: 'structuredContent.error', includes: 'ASSET_NOT_FOUND', label: 'unresolved struct reported' }] },
];

runToolTests('inspect', testCases);
