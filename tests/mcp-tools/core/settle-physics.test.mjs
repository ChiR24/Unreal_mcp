#!/usr/bin/env node
// control_actor settle_physics: actors dropped under physics in a Simulate session of their own, their resting poses kept
// as one undo step. A floor and three crates in the air, apart from each other and from everything else, so each lands on
// the floor alone; one crate is Static, as a placed prop is. Every name is one this run made.
// (tests/mcp-tools/core/settle-physics.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPSettleTag_${ts}`;
const FLOOR = `MCP_SettleFloor_${ts}`;
const LOW = `MCP_SettleCrateLow_${ts}`;
const MID = `MCP_SettleCrateMid_${ts}`;
const FIXED = `MCP_SettleCrateStatic_${ts}`;
const DROP = `MCP_SettleCrateDrop_${ts}`;
const MARKER = `MCP_SettleMarker_${ts}`;
const CRATES = [LOW, MID, FIXED];
// Every drop stays well under the default fallCm (500): a crate at z 300 rests at z 35 on the floor top (z 10).
const SETTLE = { action: 'settle_physics', actorNames: CRATES, seconds: 8 };

const testCases = [
  { scenario: 'Setup: a floor and crates in the air', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: FLOOR, location: { x: 32000, y: 32000, z: 0 }, scale: [10, 10, 0.2] },
    { actorName: LOW, location: { x: 31800, y: 32000, z: 300 }, scale: [0.5, 0.5, 0.5] },
    { actorName: MID, location: { x: 32000, y: 32000, z: 360 }, scale: [0.5, 0.5, 0.5] },
    { actorName: FIXED, location: { x: 32200, y: 32000, z: 420 }, scale: [0.5, 0.5, 0.5] },
    { actorName: DROP, location: { x: 32300, y: 32300, z: 250 }, scale: [0.5, 0.5, 0.5] },
  ] }, expected: 'success' },
  // A plain game mode: a project's own (a title screen that pauses the game) never stops the simulation.
  { scenario: 'Setup: a plain game mode for the level', toolName: 'manage_level', arguments: { action: 'set_world_settings', gameMode: '/Script/Engine.GameModeBase' }, expected: 'success' },
  { scenario: 'Setup: one crate Static, as a placed prop is', toolName: 'control_actor', arguments: { action: 'set_component_properties', actorName: FIXED, componentName: 'StaticMeshComponent0', properties: { Mobility: 'Static' } }, expected: 'success' },
  { scenario: 'Setup: a marker with no body', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.TargetPoint', actorName: MARKER, location: { x: 32300, y: 31700, z: 50 }, tags: [TAG] }, expected: 'success' },
  { scenario: 'READ: dryRun drops the crates and keeps nothing', toolName: 'control_actor', arguments: { ...SETTLE, dryRun: true }, expected: 'success', timeoutMs: 30000, assertions: [
    { path: 'structuredContent.result.dryRun', equals: true, label: 'a stability check only' },
    { path: 'structuredContent.result.kept', equals: 0, label: 'nothing kept' },
    { path: 'structuredContent.result.actors', length: 3, label: 'all three simulated' },
  ] },
  { scenario: 'READ: the low crate still hangs in the air', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LOW }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.2', approximately: 300, tolerance: 1, label: 'where it was spawned' },
  ] },
  { scenario: 'UPDATE: the crates fall and rest, judged by expect', toolName: 'control_actor', arguments: SETTLE, options: { expect: [`${LOW} on ${FLOOR}`] }, expected: 'success', timeoutMs: 30000, assertions: [
    { path: 'structuredContent.result.kept', equals: 3, label: 'three resting poses kept' },
    { path: 'structuredContent.result.unstable', length: 0, label: 'every crate came to rest' },
    { path: 'structuredContent.result.endedBecause', equals: 'rested', label: 'stopped once all slept' },
    { path: 'structuredContent.result.actors.2.moved', gte: 300, label: 'the Static crate fell too' },
    { path: 'structuredContent.receipt.expectations.0.verdict', equals: 'ok', label: 'the low crate rests on the floor' },
  ] },
  { scenario: 'READ: the low crate lies on the floor', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LOW }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.2', approximately: 35, tolerance: 3, label: 'its 50 cm on the floor top at z 10' },
  ] },
  { scenario: 'UPDATE: undo puts every crate back in one step', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'READ: the low crate is back in the air', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LOW }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.2', approximately: 300, tolerance: 1, label: 'where it was before the settle' },
  ] },
  { scenario: 'UPDATE: a drop past fallCm counts as fallen and keeps its old pose', toolName: 'control_actor', arguments: { action: 'settle_physics', actorNames: [DROP], seconds: 2, fallCm: 50 }, expected: 'success', timeoutMs: 30000, assertions: [
    { path: 'structuredContent.result.actors.0.fell', equals: true, label: 'more than 50 cm below its start' },
    { path: 'structuredContent.result.kept', equals: 0, label: 'a fallen body is not kept' },
    { path: 'structuredContent.result.unstable.0', equals: DROP, label: 'and is listed as unstable' },
  ] },
  { scenario: 'ERROR: an unknown actor simulates nothing', toolName: 'control_actor', arguments: { action: 'settle_physics', actorNames: [LOW, `MCP_NoSuchCrate_${ts}`] }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: a floor never falls', toolName: 'control_actor', arguments: { action: 'settle_physics', actorNames: [FLOOR] }, expected: 'error|NOTHING_TO_SETTLE' },
  { scenario: 'ERROR: a marker has no body to fall', toolName: 'control_actor', arguments: { action: 'settle_physics', actorNames: [MARKER] }, expected: 'error|NOTHING_TO_SETTLE' },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('settle-physics', testCases, {});
