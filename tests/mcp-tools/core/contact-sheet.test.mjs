#!/usr/bin/env node
// inspect contact_sheet: one image of an actor from several sides. A cube sunk halfway into a floor, far from
// everything else.

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPSheetTag_${ts}`;
const CUBE = `MCP_SheetCube_${ts}`;
const MARKER = `MCP_SheetMarker_${ts}`;

const testCases = [
  { scenario: 'Setup: a cube sunk halfway into a floor', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG, 'mcp.placement.ok'] }, actors: [
    { actorName: `MCP_SheetFloor_${ts}`, location: { x: 62000, y: 62000, z: 0 }, scale: [10, 10, 0.2] },
    { actorName: CUBE, location: { x: 62000, y: 62000, z: 10 } },
  ] }, expected: 'success' },
  { scenario: 'Setup: a sun, so the views are lit', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.DirectionalLight', actorName: `MCP_SheetSun_${ts}`, location: { x: 62000, y: 62000, z: 800 }, rotation: { pitch: -40, yaw: 150, roll: 0 }, tags: [TAG] }, expected: 'success' },
  { scenario: 'READ: the cube from the front and the top', toolName: 'inspect', arguments: { action: 'contact_sheet', actorName: CUBE, views: ['front', 'top'], tileSize: 256 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.mimeType', equals: 'image/png', label: 'the sheet came back as a PNG' },
    { path: 'structuredContent.result.views', length: 2, label: 'one tile per view asked' },
    { path: 'structuredContent.result.size.z', equals: 100, label: 'the cube is 100 cm tall' },
    { path: 'structuredContent.result.shown.0', equals: `MCP_SheetFloor_${ts}`, label: 'the floor it stands in is drawn too' },
  ] },
  { scenario: 'Setup: a marker that draws only an editor icon', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.TargetPoint', actorName: MARKER, location: { x: 62000, y: 62400, z: 100 }, tags: [TAG] }, expected: 'success' },
  { scenario: 'ERROR: nothing the game draws', toolName: 'inspect', arguments: { action: 'contact_sheet', actorName: MARKER }, expected: 'error|NOTHING_DRAWN' },
  { scenario: 'ERROR: an unknown actor', toolName: 'inspect', arguments: { action: 'contact_sheet', actorName: `MCP_NoSuchActor_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('contact-sheet', testCases, {});
