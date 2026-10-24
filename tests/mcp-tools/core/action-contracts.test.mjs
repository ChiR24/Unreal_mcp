#!/usr/bin/env node
// execute's options.expect and options.require, judged by the plugin after and before a call. A floor with a table on
// it, far from everything else; every assertion names actors this run made.

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPContractsTag_${ts}`;
const FLOOR = `MCP_ContractFloor_${ts}`;
const TABLE = `MCP_ContractTable_${ts}`;
const LAMP = `MCP_ContractLamp_${ts}`;
const HOVER = `MCP_ContractHover_${ts}`;

const testCases = [
  { scenario: 'Setup: a floor and a table on it', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [{ actorName: FLOOR, location: { x: 26000, y: 26000, z: 0 }, scale: [10, 10, 0.2] }, { actorName: TABLE, location: { x: 26000, y: 26000, z: 50 }, scale: [2, 1, 0.8] }] }, expected: 'success' },
  { scenario: 'CREATE: a spawn whose expectation holds says so', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: LAMP, location: { x: 26000, y: 26000, z: 115 }, scale: [0.3, 0.3, 0.5] }, options: { expect: [`${LAMP} on ${TABLE}`] }, expected: 'success', assertions: [
    { path: 'structuredContent.receipt.expectations.0.verdict', equals: 'ok', label: 'the lamp rests on the table' },
  ] },
  { scenario: 'CREATE: a spawn whose expectation fails still stands, and the reply names the fix', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: HOVER, location: { x: 26300, y: 26300, z: 300 } }, options: { expect: [`${HOVER} on ${FLOOR}`] }, expected: 'success', assertions: [
    { path: 'structuredContent.receipt.expectations.0.verdict', equals: 'failed', label: 'it floats instead' },
    { path: 'structuredContent.receipt.expectations.0.nextCall.action', equals: 'fix_placement', label: 'with the call that rests it' },
  ] },
  { scenario: 'UPDATE: a comparison on the moved lamp holds', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: LAMP, location: { x: 26050, y: 26000, z: 115 } }, options: { expect: [`${LAMP}.location.x in 26000..26100`, `${LAMP}.location.z > 1000`] }, expected: 'success', assertions: [
    { path: 'structuredContent.receipt.expectations.0.verdict', equals: 'ok', label: 'x is in the range' },
    { path: 'structuredContent.receipt.expectations.1.verdict', equals: 'failed', label: 'z is not above 1000' },
  ] },
  { scenario: 'ERROR: a requirement that does not hold runs nothing', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: LAMP, location: { x: 26000, y: 26000, z: 900 } }, options: { require: [`MCP_NoSuchActor_${ts} exists`] }, expected: 'error|PRECONDITION_FAILED' },
  { scenario: 'READ: the lamp did not move', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LAMP }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.z', approximately: 115, tolerance: 1, label: 'still on the table' },
  ] },
  { scenario: 'ERROR: an assertion that does not parse is refused before anything runs', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LAMP }, options: { expect: [`${LAMP} on`] }, expected: 'error|INVALID_OPTIONS' },
  { scenario: 'ERROR: noWarnings is no requirement', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LAMP }, options: { require: ['noWarnings'] }, expected: 'error|INVALID_OPTIONS' },
  { scenario: 'DELETE: the contract scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('action-contracts', testCases, {});
