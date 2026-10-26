#!/usr/bin/env node
// manage_sequence frame_shot: a camera placed so one or two actors sit on their screen points, seen clear of what stands
// around them, moved or spawned with its lens and focus as one undo step. Two posts on a floor, a third shut inside a
// box (no clear shot of it), and a plain camera actor. Every name is one this run made.
// (tests/mcp-tools/utility/frame-shot.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPFrameShotTag_${ts}`;
const FLOOR = `MCP_ShotFloor_${ts}`;
const LEFT = `MCP_ShotPostLeft_${ts}`;
const RIGHT = `MCP_ShotPostRight_${ts}`;
const SHUT = `MCP_ShotShut_${ts}`;
const BOX = `MCP_ShotBox_${ts}`;
const CINE = `MCP_ShotCine_${ts}`;
const PLAIN = `MCP_ShotPlain_${ts}`;

const testCases = [
  { scenario: 'Setup: a floor, two posts and a post shut in a box', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cylinder', tags: [TAG] }, actors: [
    { actorName: FLOOR, classPath: '/Engine/BasicShapes/Cube', location: { x: 40000, y: 40000, z: 0 }, scale: [40, 40, 0.2] },
    { actorName: LEFT, location: { x: 40000, y: 39850, z: 100 }, scale: [0.6, 0.6, 1.8] },
    { actorName: RIGHT, location: { x: 40000, y: 40150, z: 100 }, scale: [0.6, 0.6, 1.8] },
    { actorName: SHUT, location: { x: 41000, y: 41000, z: 60 }, scale: [0.4, 0.4, 0.8] },
    { actorName: BOX, classPath: '/Engine/BasicShapes/Cube', location: { x: 41000, y: 41000, z: 160 }, scale: [3, 3, 3] },
  ] }, expected: 'success' },
  { scenario: 'Setup: a plain camera actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.CameraActor', actorName: PLAIN, location: { x: 40600, y: 40000, z: 200 }, tags: [TAG] }, expected: 'success' },
  { scenario: 'READ: dryRun finds the shot and spawns nothing', toolName: 'manage_sequence', arguments: { action: 'frame_shot', subjects: [LEFT], camera: CINE, dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.dryRun', equals: true, label: 'nothing written' },
    { path: 'structuredContent.result.spawned', equals: true, label: 'it would spawn the camera' },
    { path: 'structuredContent.result.subjects.0.error', approximately: 0, tolerance: 0.01, label: 'the post on the left third' },
    { path: 'structuredContent.result.fill', approximately: 0.55, tolerance: 0.03, label: 'a medium shot' },
  ] },
  { scenario: 'ERROR: the dry run left no camera behind', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: CINE }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'UPDATE: a close shot spawns a cine camera with an 85 mm lens', toolName: 'manage_sequence', arguments: { action: 'frame_shot', subjects: [LEFT], camera: CINE, shotSize: 'close' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.spawned', equals: true, label: 'spawned under the label asked' },
    { path: 'structuredContent.result.focalLength', equals: 85, label: 'the close-shot lens' },
    { path: 'structuredContent.result.fill', approximately: 0.9, tolerance: 0.03, label: 'it fills the frame' },
    { path: 'structuredContent.result.subjects.0.hidden', equals: 0, label: 'nothing in the way' },
    { path: 'structuredContent.result.rotation.2', equals: 0, label: 'no roll' },
  ] },
  { scenario: 'UPDATE: a two-shot moves the same camera', toolName: 'manage_sequence', arguments: { action: 'frame_shot', subjects: [LEFT, RIGHT], camera: CINE, framing: [{ x: -0.4, y: 0.1 }, { x: 0.4, y: 0.1 }], focalLength: 35 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.spawned', equals: false, label: 'the camera from before' },
    { path: 'structuredContent.result.focalLength', equals: 35, label: 'the lens asked' },
    { path: 'structuredContent.result.subjects.0.error', approximately: 0, tolerance: 0.03, label: 'the left post on its point' },
    { path: 'structuredContent.result.subjects.1.error', approximately: 0, tolerance: 0.03, label: 'the right post on its point' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'UPDATE: undo takes the two-shot back', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'UPDATE: a plain camera keeps its own field of view', toolName: 'manage_sequence', arguments: { action: 'frame_shot', subjects: [RIGHT], camera: PLAIN, shotSize: 'wide' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.spawned', equals: false, label: 'moved, not spawned' },
    { path: 'structuredContent.result.fieldOfView', approximately: 90, tolerance: 0.5, label: 'a camera actor\'s default 90 degrees' },
    { path: 'structuredContent.result.fill', approximately: 0.25, tolerance: 0.03, label: 'a wide shot' },
  ] },
  { scenario: 'ERROR: a plain camera has no lens to set', toolName: 'manage_sequence', arguments: { action: 'frame_shot', subjects: [RIGHT], camera: PLAIN, focalLength: 50 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: a post shut in a box has no clear shot', toolName: 'manage_sequence', arguments: { action: 'frame_shot', subjects: [SHUT], camera: CINE }, expected: 'error|NO_CLEAR_SHOT' },
  { scenario: 'ERROR: an unknown subject frames nothing', toolName: 'manage_sequence', arguments: { action: 'frame_shot', subjects: [`MCP_NoSuchPost_${ts}`], camera: CINE }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: one framing point for two subjects', toolName: 'manage_sequence', arguments: { action: 'frame_shot', subjects: [LEFT, RIGHT], camera: CINE, framing: [{ x: 0, y: 0 }] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'DELETE: the spawned camera', toolName: 'control_actor', arguments: { action: 'delete', actorName: CINE }, expected: 'success' },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('frame-shot', testCases, {});
