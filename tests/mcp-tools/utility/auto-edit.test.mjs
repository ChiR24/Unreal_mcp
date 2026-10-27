#!/usr/bin/env node
// manage_sequence auto_edit: a sequence cut between its cameras as an editor would, never across the line between the
// two subjects when another cut fits. Two posts facing each other, two cameras on one side of the line between them
// and a third across it, a post shut in a box no camera sees, and a sequence of this run's own.
// (tests/mcp-tools/utility/auto-edit.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPEditTag_${ts}`;
const LEFT = `MCP_EditLeft_${ts}`;
const RIGHT = `MCP_EditRight_${ts}`;
const SHUT = `MCP_EditShut_${ts}`;
const BOX = `MCP_EditBox_${ts}`;
const CAM_A = `MCP_EditCamA_${ts}`;
const CAM_B = `MCP_EditCamB_${ts}`;
const CAM_ACROSS = `MCP_EditCamAcross_${ts}`;
const FOLDER = '/Game/MCPTest/AuthoringAssets';
const SEQ_NAME = `SEQ_Edit_${ts}`;
const SEQ = `${FOLDER}/${SEQ_NAME}`;

const testCases = [
  { scenario: 'Setup: two posts, and a post shut in a box', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cylinder', tags: [TAG] }, actors: [
    { actorName: LEFT, location: { x: 60000, y: 59850, z: 100 }, scale: [0.6, 0.6, 1.8] },
    { actorName: RIGHT, location: { x: 60000, y: 60150, z: 100 }, scale: [0.6, 0.6, 1.8] },
    { actorName: SHUT, location: { x: 61000, y: 61000, z: 60 }, scale: [0.4, 0.4, 0.8] },
    { actorName: BOX, classPath: '/Engine/BasicShapes/Cube', location: { x: 61000, y: 61000, z: 160 }, scale: [3, 3, 3] },
  ] }, expected: 'success' },
  { scenario: 'Setup: two cameras on one side of the posts, one across the line', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Script/CinematicCamera.CineCameraActor', tags: [TAG] }, actors: [
    { actorName: CAM_A, location: { x: 59200, y: 60000, z: 180 }, rotation: { pitch: -5, yaw: 0, roll: 0 } },
    { actorName: CAM_B, location: { x: 59500, y: 59500, z: 180 }, rotation: { pitch: -5, yaw: 45, roll: 0 } },
    { actorName: CAM_ACROSS, location: { x: 60800, y: 60000, z: 180 }, rotation: { pitch: -5, yaw: 180, roll: 0 } },
  ] }, expected: 'success' },
  { scenario: 'Setup: a sequence of its own', toolName: 'manage_sequence', arguments: { action: 'create', name: SEQ_NAME, path: FOLDER }, expected: 'success' },
  { scenario: 'READ: dryRun cuts every two seconds at most without crossing the line', toolName: 'manage_sequence', arguments: { action: 'auto_edit', sequencePath: SEQ, cameras: [CAM_A, CAM_B, CAM_ACROSS], subjects: [LEFT, RIGHT], segmentSeconds: 0.5, minShotSeconds: 1, maxShotSeconds: 2, dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.dryRun', equals: true, label: 'nothing written' },
    { path: 'structuredContent.result.crossings', equals: 0, label: 'no cut across the line' },
    { path: 'structuredContent.result.camerasUsed', notIncludes: CAM_ACROSS, label: 'the camera across the line stays out' },
    { path: 'structuredContent.result.cuts', gte: 2, label: 'shots of two seconds at most' },
  ] },
  { scenario: 'UPDATE: the edit is written as the camera cut track', toolName: 'manage_sequence', arguments: { action: 'auto_edit', sequencePath: SEQ, cameras: [CAM_A, CAM_B, CAM_ACROSS], subjects: [LEFT, RIGHT] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.crossings', equals: 0, label: 'no cut across the line' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'UPDATE: undo takes the edit back', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'ERROR: a subject no camera sees', toolName: 'manage_sequence', arguments: { action: 'auto_edit', sequencePath: SEQ, cameras: [CAM_A, CAM_B], subjects: [SHUT] }, expected: 'error|NO_CUT' },
  { scenario: 'ERROR: an unknown camera', toolName: 'manage_sequence', arguments: { action: 'auto_edit', sequencePath: SEQ, cameras: [CAM_A, `MCP_NoSuchCam_${ts}`], subjects: [LEFT] }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: one camera is no edit', toolName: 'manage_sequence', arguments: { action: 'auto_edit', sequencePath: SEQ, cameras: [CAM_A], subjects: [LEFT] }, expected: 'error' },
  { scenario: 'Cleanup: delete the sequence', toolName: 'manage_asset', arguments: { action: 'delete', path: SEQ, force: true }, expected: 'success|not found' },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('auto-edit', testCases, {});
