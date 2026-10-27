#!/usr/bin/env node
// inspect describe_view saveAs / compareWith (what changed in the picture since a saved frame) and points tolerance.
// A cube in front of a wall on a floor, far from everything else, the camera placed to frame it.

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPViewChangeTag_${ts}`;
const FLOOR = `MCP_ViewChangeFloor_${ts}`;
const SUBJECT = `MCP_ViewChangeCube_${ts}`;
const BEFORE = `before_${ts}`;
const CAMERA = { location: { x: 54400, y: 55000, z: 160 }, rotation: { pitch: -5, yaw: 0, roll: 0 } };

const testCases = [
  { scenario: 'Setup: a cube before a wall', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: FLOOR, location: { x: 55000, y: 55000, z: 0 }, scale: [20, 20, 0.2] },
    { actorName: `MCP_ViewChangeWall_${ts}`, location: { x: 55600, y: 55000, z: 500 }, scale: [0.2, 20, 10] },
    { actorName: SUBJECT, location: { x: 55000, y: 55000, z: 60 } },
  ] }, expected: 'success' },
  { scenario: 'Setup: a sun, so the scene is lit', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.DirectionalLight', actorName: `MCP_ViewChangeSun_${ts}`, location: { x: 55000, y: 55000, z: 800 }, rotation: { pitch: -40, yaw: 150, roll: 0 }, tags: [TAG] }, expected: 'success' },
  { scenario: 'READ: the frame kept under a name', toolName: 'inspect', arguments: { action: 'describe_view', saveAs: BEFORE, ...CAMERA }, expected: 'success', assertions: [
    { path: 'structuredContent.result.savedAs', equals: BEFORE, label: 'kept under its name' },
  ] },
  { scenario: 'READ: nothing changed, so no region changed', toolName: 'inspect', arguments: { action: 'describe_view', compareWith: BEFORE }, expected: 'success', assertions: [
    { path: 'structuredContent.result.change.regionCount', equals: 0, label: 'the renderer\'s own noise is no change' },
  ] },
  { scenario: 'UPDATE: the cube moved out of the shot', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: SUBJECT, location: { x: 55000, y: 58000, z: 60 } }, expected: 'success' },
  { scenario: 'READ: the change is where the cube was', toolName: 'inspect', arguments: { action: 'describe_view', compareWith: BEFORE }, expected: 'success', assertions: [
    { path: 'structuredContent.result.change.regionCount', gte: 1, label: 'a region changed' },
    { path: 'structuredContent.result.change.regions.0.before', includesObject: { name: SUBJECT }, label: 'the cube was drawn there' },
  ] },
  { scenario: 'ERROR: a comparison from another camera', toolName: 'inspect', arguments: { action: 'describe_view', compareWith: BEFORE, location: { x: 54000, y: 55000, z: 160 } }, expected: 'error|CAMERA_MOVED' },
  { scenario: 'ERROR: a name never saved', toolName: 'inspect', arguments: { action: 'describe_view', compareWith: `never_saved_${ts}` }, expected: 'error|VIEW_BASELINE_NOT_FOUND' },
  // Straight down from 30 m, the floor's +Y edge is at 0.667 across: a spot just past it is the void beyond.
  { scenario: 'READ: a spot just past the floor\'s edge is background', toolName: 'inspect', arguments: { action: 'describe_view', location: { x: 55000, y: 55000, z: 3000 }, rotation: { pitch: -90, yaw: 0, roll: 0 }, points: [{ x: 0.669, y: 0.5 }] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.points.0.drawn', equals: 'background', label: 'nothing is drawn there' },
  ] },
  { scenario: 'READ: with tolerance it moves onto the floor', toolName: 'inspect', arguments: { action: 'describe_view', points: [{ x: 0.669, y: 0.5 }], tolerance: 16 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.points.0.actorName', equals: FLOOR, label: 'the nearest drawn pixel is the floor' },
    { path: 'structuredContent.result.points.0.snappedTo', exists: true, label: 'and the row says where it moved' },
  ] },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('describe-view-change', testCases, {});
