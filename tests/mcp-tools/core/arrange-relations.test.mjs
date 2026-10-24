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

const testCases = [
  { scenario: 'Setup: a room set out wrong', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: FLOOR, location: { x: 30000, y: 30000, z: 0 }, scale: [10, 10, 0.2] },
    { actorName: WALL, location: { x: 30000, y: 30300, z: 160 }, scale: [6, 0.2, 3] },
    { actorName: TABLE, location: { x: 30000, y: 30000, z: 50 }, scale: [2, 1, 0.8] },
    { actorName: LAMP, location: { x: 29700, y: 29800, z: 35 }, scale: [0.3, 0.3, 0.5] },
    { actorName: CHAIR, location: { x: 30250, y: 29850, z: 55 }, scale: [0.5, 0.5, 0.9] },
    { actorName: SHELF, location: { x: 29700, y: 30120, z: 110 }, scale: [1.5, 0.4, 2] },
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
  { scenario: 'DELETE: the room by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('arrange-relations', testCases, {});
