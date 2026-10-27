#!/usr/bin/env node
// build_environment erode_landscape: a landscape with a raised block and three square pits erodes to ground where every
// cell drains, the pits filled and rivers listed; a dryRun writes nothing; with fillDepressions false a new pit keeps
// its basin and drains is false. (tests/mcp-tools/world/erode-landscape.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const LANDSCAPE = `ErodeLandscape_${ts}`;
const at = { x: 400000, y: 400000, z: 0 };
// 4 x 4 components of 63 quads: vertices 0 to 252 each way, a metre apart.
const pit = (x, y) => ({ action: 'modify_heightmap', landscapeName: LANDSCAPE, operation: 'lower', amount: 300, region: { minX: x, minY: y, maxX: x + 4, maxY: y + 4 } });

const testCases = [
  { scenario: 'Setup: a landscape', toolName: 'build_environment', arguments: { action: 'create_landscape', name: LANDSCAPE, location: at, componentCount: { x: 4, y: 4 }, quadsPerSection: 63, sectionsPerComponent: 1 }, expected: 'success', captureResult: { key: 'erodeLandscapePath', fromField: 'result.actorPath' } },
  { scenario: 'Setup: a raised block', toolName: 'build_environment', arguments: { action: 'modify_heightmap', landscapeName: LANDSCAPE, operation: 'raise', amount: 2000, region: { minX: 60, minY: 60, maxX: 140, maxY: 140 } }, expected: 'success' },
  { scenario: 'Setup: pit 1', toolName: 'build_environment', arguments: pit(30, 200), expected: 'success' },
  { scenario: 'Setup: pit 2', toolName: 'build_environment', arguments: pit(180, 40), expected: 'success' },
  { scenario: 'Setup: pit 3', toolName: 'build_environment', arguments: pit(200, 200), expected: 'success' },
  { scenario: 'DRY RUN: computes, writes nothing', toolName: 'build_environment', arguments: { action: 'erode_landscape', landscapeName: LANDSCAPE, dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.drains', equals: true, label: 'every cell would drain' },
    { path: 'structuredContent.result.depressionsFilled', gte: 3, label: 'the three pits' },
  ] },
  { scenario: 'ERODE: valleys, droplets, pits filled', toolName: 'build_environment', arguments: { action: 'erode_landscape', landscapeName: LANDSCAPE, valleys: 0.3, droplets: 8000, seed: 7 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.drains', equals: true, label: 'every cell drains' },
    { path: 'structuredContent.result.dropletsRun', equals: 8000, label: 'all droplets ran' },
    { path: 'structuredContent.result.rivers', minLength: 1, label: 'rivers listed' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'Setup: a new pit', toolName: 'build_environment', arguments: pit(100, 200), expected: 'success' },
  { scenario: 'ERODE: fillDepressions false keeps the basin', toolName: 'build_environment', arguments: { action: 'erode_landscape', landscapeActorPath: '${captured:erodeLandscapePath}', region: { minX: 80, minY: 180, maxX: 130, maxY: 230 }, fillDepressions: false, valleys: 0.1, droplets: 0, talusAngle: 35, riverAreaM2: 500 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.drains', equals: false, label: 'the pit holds water' },
    { path: 'structuredContent.result.undrainedCells', gte: 1, label: 'its cells counted' },
  ] },
  { scenario: 'ERROR: a region past the landscape', toolName: 'build_environment', arguments: { action: 'erode_landscape', landscapeName: LANDSCAPE, region: { minX: 0, minY: 0, maxX: 400, maxY: 10 } }, expected: 'error|INVALID_REGION' },
  { scenario: 'ERROR: no such landscape', toolName: 'build_environment', arguments: { action: 'erode_landscape', landscapePath: '/Game/MCPTest/NoSuchLandscape' }, expected: 'error|LANDSCAPE_NOT_FOUND' },
  { scenario: 'Cleanup: the landscape', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [LANDSCAPE] }, expected: 'success' },
];

runToolTests('erode-landscape', testCases, {});
