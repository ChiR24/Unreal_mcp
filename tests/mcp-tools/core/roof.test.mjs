#!/usr/bin/env node
// manage_geometry create_primitive roof: a hip roof over a footprint from its straight skeleton. A 10 x 6 m rectangle
// at 45 degrees rises 3 m; an L plan with eaves closes watertight; a self-crossing outline is refused.
// (tests/mcp-tools/core/roof.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const RECT = `MCP_RoofRect_${ts}`;
const L = `MCP_RoofL_${ts}`;
const OPEN = `MCP_RoofOpen_${ts}`;
const RECTANGLE = [{ x: 0, y: 0 }, { x: 1000, y: 0 }, { x: 1000, y: 600 }, { x: 0, y: 600 }];
const L_PLAN = [{ x: 0, y: 0 }, { x: 1000, y: 0 }, { x: 1000, y: 500 }, { x: 500, y: 500 }, { x: 500, y: 1000 }, { x: 0, y: 1000 }];

const testCases = [
  { scenario: 'CREATE: a rectangle at 45 degrees rises half its width', toolName: 'manage_geometry', arguments: { action: 'create_roof', name: RECT, footprint: RECTANGLE, pitch: 45, location: { x: 60000, y: 60000, z: 300 } }, expected: 'success', assertions: [
    { path: 'structuredContent.result.faces', equals: 4, label: 'one face per edge' },
    { path: 'structuredContent.result.ridgeHeight', approximately: 300, tolerance: 0.5, label: 'the ridge 3 m up' },
    { path: 'structuredContent.result.watertight', equals: true, label: 'closed underneath' },
  ] },
  { scenario: 'CREATE: an L plan with eaves, drawn clockwise', toolName: 'manage_geometry', arguments: { action: 'create_roof', name: L, footprint: [...L_PLAN].reverse(), pitch: 30, overhang: 40, location: { x: 62000, y: 60000, z: 300 }, rotation: { pitch: 0, yaw: 30, roll: 0 }, scale: { x: 1, y: 1, z: 1 } }, expected: 'success', assertions: [
    { path: 'structuredContent.result.faces', equals: 6, label: 'six faces' },
    { path: 'structuredContent.result.eaveHeight', approximately: -23.1, tolerance: 0.2, label: 'eaves 40 cm out drop 40 tan 30' },
    { path: 'structuredContent.result.watertight', equals: true, label: 'watertight' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'CREATE: without a base it is open underneath', toolName: 'manage_geometry', arguments: { action: 'create_roof', name: OPEN, footprint: RECTANGLE, base: false, location: { x: 64000, y: 60000, z: 300 } }, expected: 'success', assertions: [
    { path: 'structuredContent.result.watertight', equals: false, label: 'open' },
  ] },
  { scenario: 'ERROR: an outline that crosses itself', toolName: 'manage_geometry', arguments: { action: 'create_roof', footprint: [{ x: 0, y: 0 }, { x: 100, y: 100 }, { x: 100, y: 0 }, { x: 0, y: 100 }] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: three corners in a line', toolName: 'manage_geometry', arguments: { action: 'create_roof', footprint: [{ x: 0, y: 0 }, { x: 100, y: 0 }, { x: 200, y: 0 }] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'Cleanup: the roofs', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [RECT, L, OPEN] }, expected: 'success' },
];

runToolTests('roof', testCases, {});
