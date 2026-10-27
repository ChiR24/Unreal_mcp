#!/usr/bin/env node
// control_editor playtest: drive a pawn of the running game to a goal in a Play session this server started, and judge
// the leg. An open floor with a walled room, a nav mesh over both, far from everything else; every name is this run's.

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPPlaytestTag_${ts}`;
const BOUNDS = `MCP_PlaytestBounds_${ts}`;
const GOAL = `MCP_PlaytestGoal_${ts}`;
// A placed Character: an AIController possesses it at Play, so the leg never depends on where the player spawns.
const WALKER = `MCP_PlaytestWalker_${ts}`;
const X = 36000;
const Y = 36000;
const wall = (side, x, y, sx, sy) => ({ actorName: `MCP_PlaytestWall${side}_${ts}`, location: { x: X + x, y: Y + y, z: 150 }, scale: [sx, sy, 3] });

const testCases = [
  { scenario: 'ERROR: no game running', toolName: 'control_editor', arguments: { action: 'playtest', goal: { x: X, y: Y, z: 0 } }, expected: 'error|EDITOR_STATE_MISMATCH' },
  { scenario: 'Setup: floor, walled room and a goal marker', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: `MCP_PlaytestFloor_${ts}`, location: { x: X, y: Y, z: -10 }, scale: [30, 30, 0.2] },
    wall('N', 900, 1060, 3.4, 0.2), wall('S', 900, 740, 3.4, 0.2), wall('E', 1060, 900, 0.2, 3.4), wall('W', 740, 900, 0.2, 3.4),
    { actorName: GOAL, classPath: '/Script/Engine.TargetPoint', location: { x: X + 600, y: Y - 600, z: 50 } },
    { actorName: WALKER, classPath: '/Script/Engine.Character', location: { x: X - 600, y: Y + 600, z: 100 } },
  ] }, expected: 'success' },
  { scenario: 'Setup: nav mesh bounds', toolName: 'manage_level_structure', arguments: { action: 'create_nav_mesh_bounds_volume', volumeName: BOUNDS, location: { x: X, y: Y, z: 0 }, extent: { x: 1600, y: 1600, z: 400 } }, expected: 'success' },
  { scenario: 'Setup: build the nav mesh', toolName: 'manage_ai', arguments: { action: 'rebuild_navigation' }, expected: 'success' },
  // A plain game mode: a project's own (a title screen that pauses the game or reloads the level) never steers the legs.
  { scenario: 'Setup: a plain game mode for the level', toolName: 'manage_level', arguments: { action: 'set_world_settings', gameMode: '/Script/Engine.GameModeBase' }, expected: 'success' },
  { scenario: 'Setup: start Play', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'READ: the walker reaches the goal and every check holds', toolName: 'control_editor', arguments: { action: 'playtest', actorName: WALKER, goalActor: GOAL, acceptRadius: 120, maxSeconds: 12, checks: [`${GOAL} exists`] }, expected: 'success', timeoutMs: 30000, assertions: [
    { path: 'structuredContent.result.verdict', equals: 'pass', label: 'reached with evidence' },
    { path: 'structuredContent.result.ended', equals: 'reached', label: 'ended at the goal' },
  ] },
  { scenario: 'READ: a goal inside the closed room is not tried', toolName: 'control_editor', arguments: { action: 'playtest', actorName: WALKER, goal: { x: X + 900, y: Y + 900, z: 50 } }, expected: 'success', assertions: [
    { path: 'structuredContent.result.verdict', equals: 'inconclusive', label: 'no full path, no verdict on the game' },
  ] },
  { scenario: 'READ: a check that does not hold fails the leg', toolName: 'control_editor', arguments: { action: 'playtest', actorName: WALKER, goal: { x: X - 600, y: Y - 600, z: 50 }, acceptRadius: 120, maxSeconds: 12, stuckSeconds: 3, fallCm: 600, checks: [`MCP_NoSuchActor_${ts} exists`] }, expected: 'success', timeoutMs: 30000, assertions: [
    { path: 'structuredContent.result.verdict', equals: 'fail', label: 'the check failed' },
  ] },
  { scenario: 'ERROR: a check that does not parse', toolName: 'control_editor', arguments: { action: 'playtest', goal: { x: X, y: Y, z: 50 }, checks: [`${GOAL} on`] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'Cleanup: stop Play', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'Cleanup: the scene', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success|not found' },
  { scenario: 'Cleanup: the nav bounds', toolName: 'control_actor', arguments: { action: 'delete', actorName: BOUNDS }, expected: 'success|not found' },
];

runToolTests('playtest', testCases, {});
