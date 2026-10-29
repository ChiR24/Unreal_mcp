#!/usr/bin/env node
// control_actor arrange_by_relations: actors moved until stated relations hold. A floor, a wall, a table, a lamp, a chair
// and a shelf, far from everything else, set out wrong on purpose; every relation names actors this run made.

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPArrangeTag_${ts}`;
const FLOOR = `MCP_ArrangeFloor_${ts}`;
const WALL = `MCP_ArrangeWall_${ts}`;
const TABLE = `MCP_ArrangeTable_${ts}`;
const LAMP = `MCP_ArrangeLamp_${ts}`;
const CHAIR = `MCP_ArrangeChair_${ts}`;
const SHELF = `MCP_ArrangeShelf_${ts}`;
const ROOM = [`${LAMP} on ${TABLE}`, `${CHAIR} nextTo ${TABLE}`, `${CHAIR} facing ${TABLE}`, `${SHELF} against ${WALL}`];
const CRATES = ['A', 'B', 'C'].map((letter) => `MCP_ArrangeCrate${letter}_${ts}`);

const testCases = [
  { scenario: 'Setup: a room set out wrong', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: FLOOR, location: { x: 30000, y: 30000, z: 0 }, scale: [10, 10, 0.2] },
    { actorName: WALL, location: { x: 30000, y: 30300, z: 160 }, scale: [6, 0.2, 3] },
    { actorName: TABLE, location: { x: 30000, y: 30000, z: 50 }, scale: [2, 1, 0.8] },
    { actorName: LAMP, location: { x: 29700, y: 29800, z: 35 }, scale: [0.3, 0.3, 0.5] },
    { actorName: CHAIR, location: { x: 30250, y: 29850, z: 55 }, scale: [0.5, 0.5, 0.9] },
    { actorName: SHELF, location: { x: 29700, y: 30120, z: 110 }, scale: [1.5, 0.4, 2] },
    // A row of 100 cm crates spaced unevenly along x, for distribute_actors.
    { actorName: CRATES[0], location: { x: 31000, y: 30000, z: 50 } },
    { actorName: CRATES[1], location: { x: 31130, y: 30000, z: 50 } },
    { actorName: CRATES[2], location: { x: 31600, y: 30000, z: 50 } },
  ] }, expected: 'success' },
  { scenario: 'READ: dryRun plans the room and moves nothing', toolName: 'control_actor', arguments: { action: 'arrange_by_relations', relations: ROOM, dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.dryRun', equals: true, label: 'a plan only' },
    { path: 'structuredContent.result.moved', minLength: 3, label: 'lamp, chair and shelf would move' },
  ] },
  { scenario: 'READ: the lamp did not move', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LAMP }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.0', approximately: 29700, tolerance: 1, label: 'still on the floor' },
  ] },
  { scenario: 'UPDATE: the room is arranged as stated', toolName: 'control_actor', arguments: { action: 'arrange_by_relations', relations: ROOM }, expected: 'success', assertions: [
    { path: 'structuredContent.result.unmet', equals: 0, label: 'every relation holds' },
    { path: 'structuredContent.result.relations.3.verdict', equals: 'ok', label: 'the shelf has its back to the wall' },
  ] },
  { scenario: 'READ: expect agrees with the arrangement', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LAMP }, options: { expect: [`${LAMP} on ${TABLE}`, `${SHELF} against ${WALL}`, `${CHAIR} facing ${TABLE}`] }, expected: 'success', assertions: [
    { path: 'structuredContent.receipt.expectations.0.verdict', equals: 'ok', label: 'the lamp rests on the table' },
    { path: 'structuredContent.receipt.expectations.1.verdict', equals: 'ok', label: 'the shelf stands against the wall' },
    { path: 'structuredContent.receipt.expectations.2.verdict', equals: 'ok', label: 'the chair faces the table' },
  ] },
  { scenario: 'READ: describe_relations reads the turned chair beside the table', toolName: 'control_actor', arguments: { action: 'describe_relations', actorNames: [CHAIR], kinds: ['nextTo'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.relations', includesObject: { 0: TABLE, 1: 'nextTo', 2: CHAIR }, label: 'side by side although its bounds overlap the table\'s' },
  ] },
  { scenario: 'UPDATE: undo puts the room back in one step', toolName: 'control_editor', arguments: { action: 'undo' }, expected: 'success' },
  { scenario: 'READ: the lamp is back on the floor', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: LAMP }, expected: 'success', assertions: [
    { path: 'structuredContent.result.location.0', approximately: 29700, tolerance: 1, label: 'where it stood' },
  ] },
  { scenario: 'UPDATE: only the actors named move', toolName: 'control_actor', arguments: { action: 'arrange_by_relations', relations: [`${CHAIR} nextTo ${SHELF}`], actorNames: [CHAIR] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.unmet', equals: 0, label: 'the chair stands by the shelf' },
    { path: 'structuredContent.result.moved', length: 1, label: 'and only the chair moved' },
  ] },
  { scenario: 'ERROR: a comparison is no relation', toolName: 'control_actor', arguments: { action: 'arrange_by_relations', relations: [`${LAMP}.location.z > 5`] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: an unknown actor moves nothing', toolName: 'control_actor', arguments: { action: 'arrange_by_relations', relations: [`MCP_NoSuchActor_${ts} on ${TABLE}`] }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: a floor never moves', toolName: 'control_actor', arguments: { action: 'arrange_by_relations', relations: [`${FLOOR} on ${TABLE}`] }, expected: 'error|NOTHING_TO_MOVE' },
  // snap_to_grid: the shelf (set out at 29700, 30120, 110) onto a 100 cm grid on x and y, its height kept.
  { scenario: 'READ: snap_to_grid dryRun reports and moves nothing', toolName: 'control_actor', arguments: { action: 'snap_to_grid', actorNames: [SHELF], gridCm: 100, axes: 'xy', dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.dryRun', equals: true, label: 'a report only' },
  ] },
  { scenario: 'UPDATE: snap_to_grid puts the shelf on the 100 cm grid, squared to 90 degrees', toolName: 'control_actor', arguments: { action: 'snap_to_grid', actorNames: [SHELF], gridCm: 100, axes: 'xy', rotationDeg: 90 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.actors.0.to.0', equals: 29700, label: 'x on the grid' },
    { path: 'structuredContent.result.actors.0.to.1', equals: 30100, label: 'y on the grid' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'READ: snap_to_grid on the grid already moves nothing and makes no undo step', toolName: 'control_actor', arguments: { action: 'snap_to_grid', actorNames: [SHELF], gridCm: 100, axes: 'xy', rotationDeg: 90 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.moved', equals: 0, label: 'nothing moved' },
    { path: 'structuredContent.result.undo', exists: false, label: 'and no undo step' },
  ] },
  { scenario: 'ERROR: snap_to_grid with an axis that is not one', toolName: 'control_actor', arguments: { action: 'snap_to_grid', actorNames: [SHELF], axes: 'xw' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: snap_to_grid names an actor that is not there', toolName: 'control_actor', arguments: { action: 'snap_to_grid', actorNames: [`MCP_NoSuchActor_${ts}`] }, expected: 'error|ACTOR_NOT_FOUND' },
  // distribute_actors: the crates at 31000, 31130 and 31600 along x.
  { scenario: 'READ: distribute_actors dryRun reports and moves nothing', toolName: 'control_actor', arguments: { action: 'distribute_actors', actorNames: CRATES, dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.axis', equals: 'x', label: 'the axis they spread along' },
    { path: 'structuredContent.result.stepCm', equals: 300, label: 'the first and last stay, 600 apart' },
    { path: 'structuredContent.result.moved', equals: 1, label: 'only the middle one would move' },
  ] },
  { scenario: 'UPDATE: distribute_actors centres the middle crate', toolName: 'control_actor', arguments: { action: 'distribute_actors', actorNames: CRATES }, expected: 'success', assertions: [
    { path: 'structuredContent.result.actors.1.to.0', equals: 31300, label: 'the middle crate halfway' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'READ: distribute_actors on an even row moves nothing and makes no undo step', toolName: 'control_actor', arguments: { action: 'distribute_actors', actorNames: CRATES, axis: 'x' }, expected: 'success', assertions: [
    { path: 'structuredContent.result.moved', equals: 0, label: 'nothing moved' },
    { path: 'structuredContent.result.undo', exists: false, label: 'and no undo step' },
  ] },
  { scenario: 'UPDATE: distribute_actors with gapCm lays them 50 cm apart from the first', toolName: 'control_actor', arguments: { action: 'distribute_actors', actorNames: CRATES, axis: 'x', gapCm: 50 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.actors.1.to.0', equals: 31150, label: 'the second 50 cm past the first' },
    { path: 'structuredContent.result.actors.2.to.0', equals: 31300, label: 'the third 50 cm past the second' },
  ] },
  { scenario: 'ERROR: distribute_actors names an actor that is not there', toolName: 'control_actor', arguments: { action: 'distribute_actors', actorNames: [CRATES[0], CRATES[1], `MCP_NoSuchActor_${ts}`] }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ERROR: distribute_actors with the same actor named three times', toolName: 'control_actor', arguments: { action: 'distribute_actors', actorNames: [CRATES[0], CRATES[0], CRATES[0]] }, expected: 'error|INVALID_ARGUMENT' },
  // lock_actors: the shelf and the lamp locked; fix_placement then leaves the lamp where it is.
  { scenario: 'UPDATE: lock_actors locks the shelf and the lamp', toolName: 'control_actor', arguments: { action: 'lock_actors', actorNames: [SHELF, LAMP] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.changed', equals: 2, label: 'both locked' },
    { path: 'structuredContent.result.actors.1.locked', equals: true, label: 'read back locked' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'READ: locking them again changes nothing and makes no undo step', toolName: 'control_actor', arguments: { action: 'lock_actors', actorNames: [SHELF, LAMP] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.changed', equals: 0, label: 'already locked' },
    { path: 'structuredContent.result.undo', exists: false, label: 'and no undo step' },
  ] },
  { scenario: 'READ: fix_placement leaves the locked lamp where it is', toolName: 'control_actor', arguments: { action: 'fix_placement', kinds: ['sunk', 'floating'], actorNames: [LAMP], dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.skipped.0.actorName', equals: LAMP, label: 'the lamp is skipped' },
    { path: 'structuredContent.result.skipped.0.reason', includes: 'Lock Actor Movement', label: 'because it is locked' },
  ] },
  { scenario: 'UPDATE: lock_actors with locked false unlocks the lamp', toolName: 'control_actor', arguments: { action: 'lock_actors', actorNames: [LAMP], locked: false }, expected: 'success', assertions: [
    { path: 'structuredContent.result.actors.0.locked', equals: false, label: 'unlocked' },
    { path: 'structuredContent.result.changed', equals: 1, label: 'only the lamp changed' },
  ] },
  { scenario: 'ERROR: lock_actors names an actor that is not there', toolName: 'control_actor', arguments: { action: 'lock_actors', actorNames: [LAMP, `MCP_NoSuchActor_${ts}`] }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'DELETE: the room by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('arrange-relations', testCases, {});
