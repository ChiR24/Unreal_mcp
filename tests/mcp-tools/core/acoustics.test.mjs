#!/usr/bin/env node
// manage_audio analyze_acoustics: how a space sounds where a listener stands. A closed 6 x 5 x 3 m room of engine cubes
// (its reverberation time from the traced decay, Eyring's beside it) and a 30 m corridor (too long for Eyring), both
// high above the level so nothing else is in reach.
// (tests/mcp-tools/core/acoustics.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPAcousticsTag_${ts}`;
const CUBE = '/Engine/BasicShapes/Cube';
const LISTENER = `MCP_AcousticsListener_${ts}`;
// The room's inside runs x 90000-90600, y 90000-90500, z 5000-5300 (walls 20 cm thick); the corridor's x 95000-98000,
// y 90000-90200, z 5000-5300.
const ROOM = { x: 90300, y: 90250, z: 5150 }; // the middle, where the listener cube stands
const ASIDE = { x: 90150, y: 90125, z: 5150 }; // a point clear of it
const HALL = { x: 96500, y: 90100, z: 5150 };
const walls = (name, x0, y0, sx, sy) => [
  { actorName: `${name}Floor_${ts}`, location: [x0 + sx * 50, y0 + sy * 50, 4990], scale: [sx + 0.4, sy + 0.4, 0.2] },
  { actorName: `${name}Ceiling_${ts}`, location: [x0 + sx * 50, y0 + sy * 50, 5310], scale: [sx + 0.4, sy + 0.4, 0.2] },
  { actorName: `${name}WestWall_${ts}`, location: [x0 - 10, y0 + sy * 50, 5150], scale: [0.2, sy, 3] },
  { actorName: `${name}EastWall_${ts}`, location: [x0 + sx * 100 + 10, y0 + sy * 50, 5150], scale: [0.2, sy, 3] },
  { actorName: `${name}SouthWall_${ts}`, location: [x0 + sx * 50, y0 - 10, 5150], scale: [sx + 0.4, 0.2, 3] },
  { actorName: `${name}NorthWall_${ts}`, location: [x0 + sx * 50, y0 + sy * 100 + 10, 5150], scale: [sx + 0.4, 0.2, 3] },
];

const testCases = [
  { scenario: 'Setup: a closed room, a corridor, and a small cube standing for the listener', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: CUBE, tags: [TAG, 'mcp.placement.ok'] }, actors: [
    ...walls('MCP_AcousticsRoom', 90000, 90000, 6, 5),
    ...walls('MCP_AcousticsHall', 95000, 90000, 30, 2),
    { actorName: LISTENER, location: [ROOM.x, ROOM.y, ROOM.z], scale: [0.2, 0.2, 0.2] },
  ] }, expected: 'success' },
  { scenario: 'READ: the room as heard from one side of it', toolName: 'manage_audio', arguments: { action: 'analyze_acoustics', location: ASIDE }, expected: 'success', assertions: [
    { path: 'structuredContent.result.openShare', equals: 0, label: 'closed' },
    { path: 'structuredContent.result.lengthM', approximately: 6, tolerance: 0.1, label: 'six metres long' },
    { path: 'structuredContent.result.widthM', approximately: 5, tolerance: 0.1, label: 'five wide' },
    { path: 'structuredContent.result.heightM', approximately: 3, tolerance: 0.1, label: 'three high' },
    { path: 'structuredContent.result.eyringRt60', approximately: 1.09, tolerance: 0.1, label: 'Eyring for 90 m3 at absorption 0.1' },
    { path: 'structuredContent.result.rt60', approximately: 1.09, tolerance: 0.17, label: 'the traced decay within 15% of Eyring' },
    { path: 'structuredContent.result.nextCall.action', equals: 'create_reverb_effect', label: 'a reverb effect with that decay' },
    { path: 'structuredContent.result.zoneCall.params.reverbEffect', exists: true, label: 'and the zone that places it' },
  ] },
  { scenario: 'READ: from an actor, which the rays pass through, with softer walls', toolName: 'manage_audio', arguments: { action: 'analyze_acoustics', actorName: LISTENER, absorption: 0.3 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.openShare', equals: 0, label: 'the listener cube does not close the rays in' },
    { path: 'structuredContent.result.eyringRt60', approximately: 0.32, tolerance: 0.05, label: 'more absorption, a shorter reverb' },
  ] },
  { scenario: 'READ: a corridor is too long for Eyring', toolName: 'manage_audio', arguments: { action: 'analyze_acoustics', location: HALL }, expected: 'success', assertions: [
    { path: 'structuredContent.result.proportion', gte: 3, label: 'long' },
    { path: 'structuredContent.result.eyringRt60', exists: false, label: 'no Eyring' },
    { path: 'structuredContent.result.rt60', gte: 0.1, label: 'the traced decay still holds' },
  ] },
  { scenario: 'READ: open sky has no room reverb', toolName: 'manage_audio', arguments: { action: 'analyze_acoustics', location: { x: 0, y: 0, z: 100000 }, maxDistance: 2000 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.openShare', equals: 1, label: 'every ray escapes' },
    { path: 'structuredContent.result.nextCall', exists: false, label: 'nothing suggested' },
  ] },
  { scenario: 'ERROR: a listener inside a wall', toolName: 'manage_audio', arguments: { action: 'analyze_acoustics', location: { x: 89990, y: 90250, z: 5150 } }, expected: 'error|POINT_IN_SOLID' },
  { scenario: 'ERROR: an actor not in the level', toolName: 'manage_audio', arguments: { action: 'analyze_acoustics', actorName: `MCP_NoSuchListener_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: no listener given', toolName: 'manage_audio', arguments: { action: 'analyze_acoustics', absorption: 0.1 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('acoustics', testCases, {});
