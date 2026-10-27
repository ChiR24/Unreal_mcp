#!/usr/bin/env node
// manage_sequence fly_through: a camera flown through points and keyed into a Level Sequence, never nearer to geometry
// than its clearance. A floor and a wall standing across the line between the two points (the flight goes round or
// over it), a statue to look at, and a sequence of this run's own. Every name is one this run made.
// (tests/mcp-tools/utility/fly-through.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPFlyTag_${ts}`;
const FLOOR = `MCP_FlyFloor_${ts}`;
const WALL = `MCP_FlyWall_${ts}`;
const STATUE = `MCP_FlyStatue_${ts}`;
const CAM = `MCP_FlyCam_${ts}`;
const FOLDER = '/Game/MCPTest/AuthoringAssets';
const SEQ_NAME = `SEQ_Fly_${ts}`;
const SEQ = `${FOLDER}/${SEQ_NAME}`;
const FROM = { x: 50000, y: 50000, z: 200 };
const TO = { x: 51000, y: 50000, z: 200 };

const testCases = [
  { scenario: 'Setup: a floor, a wall across the way and a statue', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: FLOOR, location: { x: 50500, y: 50000, z: 0 }, scale: [40, 40, 0.2] },
    { actorName: WALL, location: { x: 50500, y: 50000, z: 210 }, scale: [0.4, 6, 4] },
    { actorName: STATUE, classPath: '/Engine/BasicShapes/Cylinder', location: { x: 50500, y: 50700, z: 110 }, scale: [1, 1, 2] },
  ] }, expected: 'success' },
  { scenario: 'Setup: a sequence of its own', toolName: 'manage_sequence', arguments: { action: 'create', name: SEQ_NAME, path: FOLDER }, expected: 'success' },
  { scenario: 'READ: dryRun plans the flight round or over the wall and writes nothing', toolName: 'manage_sequence', arguments: { action: 'fly_through', sequencePath: SEQ, through: [FROM, TO], camera: CAM, dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.dryRun', equals: true, label: 'nothing written' },
    { path: 'structuredContent.result.spawned', equals: true, label: 'it would spawn the camera' },
    { path: 'structuredContent.result.corners', minLength: 3, label: 'a corner beside or over the wall' },
  ] },
  { scenario: 'ERROR: the dry run left no camera behind', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: CAM }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'UPDATE: a four-second flight facing the statue', toolName: 'manage_sequence', arguments: { action: 'fly_through', sequencePath: SEQ, through: [FROM, TO], camera: CAM, lookAt: STATUE, seconds: 4, clearanceCm: 60 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.keys', equals: 41, label: 'ten keys a second' },
    { path: 'structuredContent.result.cameraCut', equals: true, label: 'cut to over the flight' },
    { path: 'structuredContent.result.spawned', equals: true, label: 'spawned under the label asked' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'READ: the camera starts where the flight does', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: CAM }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.0', approximately: FROM.x, tolerance: 1, label: 'at the first point' },
  ] },
  { scenario: 'UPDATE: flying the same camera again replaces its keys and keeps one cut', toolName: 'manage_sequence', arguments: { action: 'fly_through', sequencePath: SEQ, through: [TO, FROM], camera: CAM, seconds: 3 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.spawned', equals: false, label: 'the camera from before' },
    { path: 'structuredContent.result.replacedTrack', equals: true, label: 'its earlier flight replaced' },
    { path: 'structuredContent.result.keys', equals: 31, label: 'three seconds of keys' },
  ] },
  { scenario: 'UPDATE: undo takes the second flight back', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'UPDATE: undo takes the first flight back, camera and all', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'ERROR: the undone camera is gone', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: CAM }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: a point inside the wall', toolName: 'manage_sequence', arguments: { action: 'fly_through', sequencePath: SEQ, through: [FROM, { x: 50500, y: 50000, z: 200 }], camera: CAM }, expected: 'error|POINT_IN_SOLID' },
  { scenario: 'ERROR: an unknown actor to look at', toolName: 'manage_sequence', arguments: { action: 'fly_through', sequencePath: SEQ, through: [FROM, TO], camera: CAM, lookAt: `MCP_NoSuchStatue_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: a sequence that does not exist', toolName: 'manage_sequence', arguments: { action: 'fly_through', sequencePath: `${FOLDER}/SEQ_NoSuch_${ts}`, through: [FROM, TO], camera: CAM }, expected: 'error|INVALID_SEQUENCE' },
  { scenario: 'Cleanup: delete the sequence', toolName: 'manage_asset', arguments: { action: 'delete', path: SEQ, force: true }, expected: 'success|not found' },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('fly-through', testCases, {});
