#!/usr/bin/env node
// control_editor explore_level: a pawn of the running game explores the level from an archive of cells. A small
// platform it can fall off (the level's kill height set close below) and, beside it, a sealed room the nav mesh covers
// but no path reaches (an island). Every name is this run's.
// (tests/mcp-tools/core/explore-level.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPExploreTag_${ts}`;
const BOUNDS = `MCP_ExploreBounds_${ts}`;
// A placed Character: an AIController possesses it at Play, and with a plain game mode the player's own pawn flies.
const WALKER = `MCP_ExploreWalker_${ts}`;
const X = 38000;
const Y = 38000;
const wall = (side, x, y, sx, sy) => ({ actorName: `MCP_ExploreWall${side}_${ts}`, location: { x: X + x, y: Y + y, z: 150 }, scale: [sx, sy, 3] });

const testCases = [
  { scenario: 'ERROR: no game running', toolName: 'control_editor', arguments: { action: 'explore_level', op: 'start' }, expected: 'error|EDITOR_STATE_MISMATCH' },
  { scenario: 'Setup: a platform and a sealed room', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: `MCP_ExplorePlatform_${ts}`, location: { x: X, y: Y, z: 0 }, scale: [4, 4, 0.2] },
    { actorName: `MCP_ExploreRoomFloor_${ts}`, location: { x: X + 1500, y: Y, z: 0 }, scale: [4, 4, 0.2] },
    wall('N', 1500, 210, 4.4, 0.2), wall('S', 1500, -210, 4.4, 0.2), wall('E', 1710, 0, 0.2, 4.4), wall('W', 1290, 0, 0.2, 4.4),
    { actorName: WALKER, classPath: '/Script/Engine.Character', location: { x: X, y: Y, z: 100 } },
  ] }, expected: 'success' },
  { scenario: 'Setup: nav mesh bounds over both', toolName: 'manage_level_structure', arguments: { action: 'create_nav_mesh_bounds_volume', volumeName: BOUNDS, location: { x: X + 750, y: Y, z: 0 }, extent: { x: 1200, y: 500, z: 300 } }, expected: 'success' },
  { scenario: 'Setup: build the nav mesh', toolName: 'manage_ai', arguments: { action: 'rebuild_navigation' }, expected: 'success' },
  // A plain game mode (no title screen to pause the game) and a kill height just below the platform.
  { scenario: 'Setup: a plain game mode and a near kill height', toolName: 'manage_level', arguments: { action: 'set_world_settings', gameMode: '/Script/Engine.GameModeBase', killZ: -500 }, expected: 'success' },
  { scenario: 'Setup: start Play', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'ERROR: continue before any session', toolName: 'control_editor', arguments: { action: 'explore_level', op: 'continue' }, expected: 'error|NO_SESSION' },
  { scenario: 'ERROR: a track text that does not parse', toolName: 'control_editor', arguments: { action: 'explore_level', op: 'start', actorName: WALKER, track: [`${WALKER} on`] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ACTION: explore from the platform', toolName: 'control_editor', arguments: { action: 'explore_level', op: 'start', actorName: WALKER, cellCm: 100, heightCm: 100, seed: 7, track: [`${WALKER} exists`], seconds: 15 }, expected: 'success', timeoutMs: 30000, assertions: [
    { path: 'structuredContent.result.findings', includesObject: { kind: 'escape_killz' }, label: 'it fell off the platform below the kill height' },
    { path: 'structuredContent.result.islands', minLength: 1, label: 'the sealed room is an island' },
  ] },
  { scenario: 'ACTION: explore more from the first cell', toolName: 'control_editor', arguments: { action: 'explore_level', op: 'continue', cell: 0, seconds: 4 }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'READ: the archive with its cells', toolName: 'control_editor', arguments: { action: 'explore_level', op: 'read', detail: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.archive.0.cell', equals: 0, label: 'the start cell is first' },
  ] },
  { scenario: 'ACTION: stop the session', toolName: 'control_editor', arguments: { action: 'explore_level', op: 'stop' }, expected: 'success' },
  { scenario: 'ERROR: read after stop', toolName: 'control_editor', arguments: { action: 'explore_level', op: 'read' }, expected: 'error|NO_SESSION' },
  { scenario: 'Cleanup: stop Play', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success', timeoutMs: 30000 },
  { scenario: 'Cleanup: the scene', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success|not found' },
  { scenario: 'Cleanup: the nav bounds', toolName: 'control_actor', arguments: { action: 'delete', actorName: BOUNDS }, expected: 'success|not found' },
];

runToolTests('explore-level', testCases, {});
