#!/usr/bin/env node
// build_environment place_decals: a decal traced down onto a floor lies flat on it, one traced at a wall lies flat on the
// wall, an area scatter lays its count spaced on the floor it is limited to (or up the wall when traced into it), a decal
// goes where a mesh draws its surface (a trough's floor inside the box that is its collision) and on through where it
// draws none (past the trough's floor), a dry run places nothing, and points that find no surface (or only another
// actor's), a material that is no decal and a missing one are refused.
// (tests/mcp-tools/world/place-decals.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = '/Game/MCPTest';
const DECAL = `${FOLDER}/M_DecalProbe_${ts}`;
const FLOOR = `DecalFloor_${ts}`;
const WALL = `DecalWall_${ts}`;
const at = { x: 300000, y: -300000, z: 0 };
const above = (dx, dy) => ({ x: at.x + dx, y: at.y + dy, z: at.z + 200 });
// A trough on the floor (whose top is at z 10): walls 120 cm tall along X, a 20 cm floor under its -X half only, baked
// with the default box collision, which covers the walls' tops and the open half alike.
const TROUGH = `DecalTrough_${ts}`;
const TROUGH_MESH = `${FOLDER}/SM_DecalTrough_${ts}`;
const trough = { x: at.x + 600, y: at.y + 600, z: at.z + 10 };
const TROUGH_BOXES = [
  { min: { x: -200, y: -150, z: 0 }, max: { x: 0, y: 150, z: 20 } },
  { min: { x: -200, y: -150, z: 0 }, max: { x: 200, y: -110, z: 120 } },
  { min: { x: -200, y: 110, z: 0 }, max: { x: 200, y: 150, z: 120 } },
];

const testCases = [
  { scenario: 'Setup: a decal material', toolName: 'manage_asset', arguments: { action: 'create_material', kind: 'decal', name: `M_DecalProbe_${ts}`, path: FOLDER }, expected: 'success' },
  { scenario: 'Setup: a 20 m floor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: FLOOR, location: at, scale: { x: 20, y: 20, z: 0.2 } }, expected: 'success' },
  { scenario: 'Setup: a wall facing -X', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: WALL, location: { x: at.x + 800, y: at.y, z: at.z + 160 }, scale: { x: 0.2, y: 6, z: 3 } }, expected: 'success' },
  { scenario: 'PLACE: a decal traced down lies flat on the floor', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, points: [above(-300, 0)], width: 150, height: 100, label: `Floor_${ts}` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.placedCount', equals: 1, label: 'one decal' },
    { path: 'structuredContent.result.placed.0.hitActor', equals: FLOOR, label: 'on the floor' },
    { path: 'structuredContent.result.placed.0.normal.2', equals: 1, label: 'the floor faces up' },
    { path: 'structuredContent.result.placed.0.rotation.0', equals: -90, label: 'projecting straight down' },
  ] },
  { scenario: 'PLACE: a decal traced along +X lies flat on the wall', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, points: [{ x: at.x + 500, y: at.y, z: at.z + 150 }], direction: { x: 1, y: 0, z: 0 }, depth: 20, angle: 15, label: `Wall_${ts}` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.placed.0.hitActor', equals: WALL, label: 'on the wall' },
    { path: 'structuredContent.result.placed.0.normal.0', equals: -1, label: 'the face toward the point' },
  ] },
  { scenario: 'PLACE: an area scatter limited to the floor', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, area: { center: above(-300, 0), radius: 500, count: 6, seed: 3 }, width: 120, height: 120, scaleJitter: 0.3, onActors: [FLOOR], traceCm: 500, sortOrder: 2, fadeScreenSize: 0.005, label: `Scatter_${ts}`, folder: 'Decals/Test' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.placedCount', equals: 6, label: 'all six on the floor' },
    { path: 'structuredContent.result.placed.5.hitActor', equals: FLOOR, label: 'the last one too' },
  ] },
  { scenario: 'PLACE: an area scatter traced into the wall spreads up the wall', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, area: { center: { x: at.x + 500, y: at.y, z: at.z + 160 }, radius: 120, count: 4, seed: 5 }, direction: { x: 1, y: 0, z: 0 }, width: 80, height: 80, onActors: [WALL], label: `WallScatter_${ts}` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.placedCount', equals: 4, label: 'all four on the wall' },
    { path: 'structuredContent.result.placed.3.normal.0', equals: -1, label: 'flat on its face' },
  ] },
  { scenario: 'Setup: a trough actor', toolName: 'manage_geometry', arguments: { action: 'create_box', name: `${TROUGH}_Shape`, dimensions: { x: 1, y: 1, z: 1 }, location: { x: at.x, y: at.y - 5000, z: 0 } }, expected: 'success' },
  { scenario: 'Setup: its walls and half floor', toolName: 'manage_geometry', arguments: { action: 'append_cuboids', actorName: `${TROUGH}_Shape`, boxes: TROUGH_BOXES }, expected: 'success' },
  { scenario: 'Setup: baked with box collision', toolName: 'manage_geometry', arguments: { action: 'convert_to_static_mesh', actorName: `${TROUGH}_Shape`, outputPath: TROUGH_MESH }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'Setup: the trough on the floor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: TROUGH_MESH, actorName: TROUGH, location: trough }, expected: 'success' },
  { scenario: 'PLACE: into the trough, on the floor it draws, not on top of its collision box', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, points: [{ x: trough.x - 100, y: trough.y, z: trough.z + 300 }], width: 80, height: 80, label: `Trough_${ts}` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.placed.0.hitActor', equals: TROUGH, label: 'on the trough' },
    { path: 'structuredContent.result.placed.0.location.2', equals: 30, label: 'at the top of its floor (box top 130)' },
  ] },
  { scenario: 'PLACE: past the trough floor, on through to the ground', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, points: [{ x: trough.x + 100, y: trough.y, z: trough.z + 300 }], width: 80, height: 80, label: `Through_${ts}` }, expected: 'success', assertions: [
    { path: 'structuredContent.result.placed.0.hitActor', equals: FLOOR, label: 'on the floor below' },
    { path: 'structuredContent.result.placed.0.location.2', equals: 10, label: 'at the floor top' },
  ] },
  { scenario: 'PLACE: dryRun places nothing', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, points: [above(0, 300)], dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.dryRun', equals: true, label: 'nothing placed' },
  ] },
  { scenario: 'ERROR: a point over nothing', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, points: [{ x: at.x + 90000, y: at.y, z: at.z + 200 }] }, expected: 'error|NOTHING_PLACED' },
  { scenario: 'ERROR: the floor is not among onActors', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: DECAL, points: [above(-300, 0)], onActors: [WALL] }, expected: 'error|NOTHING_PLACED' },
  { scenario: 'ERROR: a surface material is no decal', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: '/Engine/BasicShapes/BasicShapeMaterial', points: [above(-300, 0)] }, expected: 'error|DECAL_MATERIAL_REQUIRED' },
  { scenario: 'ERROR: a material that does not exist', toolName: 'build_environment', arguments: { action: 'place_decals', materialPath: `${FOLDER}/M_NoDecal_${ts}`, points: [above(-300, 0)] }, expected: 'error|MATERIAL_NOT_FOUND' },
  { scenario: 'Cleanup: the decals', toolName: 'control_actor', arguments: { action: 'delete', deleteScope: 'by_tag', tag: 'mcp.decal' }, expected: 'success' },
  { scenario: 'Cleanup: floor, wall and trough', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [FLOOR, WALL, TROUGH, `${TROUGH}_Shape`] }, expected: 'success' },
  { scenario: 'Cleanup: the decal material and the trough mesh', toolName: 'manage_asset', arguments: { action: 'delete', paths: [DECAL, TROUGH_MESH] }, expected: 'success' },
];

runToolTests('place-decals', testCases, {});
