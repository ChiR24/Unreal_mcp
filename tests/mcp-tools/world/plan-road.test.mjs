#!/usr/bin/env node
// build_environment plan_road: a road up a 30 m hill whose sides are too steep to climb straight winds up under the grade
// limit with no corner under the radius, and is laid with the points it hands back; a box across a straight route is
// passed round; an end on the box, a limit no road can keep and an unknown ground actor are refused.
// (tests/mcp-tools/world/plan-road.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const LANDSCAPE = `RoadLandscape_${ts}`;
const BOX = `RoadBox_${ts}`;
const ROAD = `PlannedRoad_${ts}`;
const at = { x: 600000, y: 600000, z: 0 };
// 8 x 8 components of 63 quads: 505 vertices a side, a metre apart; 250 m in, a 30 m hill with a 45 m flat top and
// 28.6% sides (at 38% a 12% road could take no 18.43 degree turn uphill from level, so some lattices found no way up).
const top = { x: at.x + 25000, y: at.y + 25000 };
const across = (y) => ({ from: { x: at.x + 5000, y: at.y + y }, to: { x: at.x + 45000, y: at.y + y } });

const testCases = [
  { scenario: 'Setup: a landscape', toolName: 'build_environment', arguments: { action: 'create_landscape', name: LANDSCAPE, location: at, componentCount: { x: 8, y: 8 }, quadsPerSection: 63, sectionsPerComponent: 1 }, expected: 'success' },
  { scenario: 'Setup: a flat-topped hill', toolName: 'build_environment', arguments: { action: 'sculpt', landscapeName: LANDSCAPE, tool: 'Raise', location: { ...top, z: 0 }, radius: 15000, falloff: 0.7, strength: 30 }, expected: 'success' },
  { scenario: 'PLAN: up the hill under 12%', toolName: 'build_environment', arguments: { action: 'plan_road', from: { x: at.x + 2000, y: at.y + 25000 }, to: top, maxGradePercent: 12, turnRadius: 1500, width: 600 }, expected: 'success', captureResult: { key: 'roadPoints', fromField: 'result.nextCall.params.points' }, assertions: [
    { path: 'structuredContent.result.maxGradePercent', approximately: 6, tolerance: 6, label: 'no grade over 12%' },
    { path: 'structuredContent.result.minTurnRadiusM', gte: 15, label: 'no corner under 15 m' },
    { path: 'structuredContent.result.switchbacks', gte: 1, label: 'it winds' },
    { path: 'structuredContent.result.groundGapCm', approximately: 12.5, tolerance: 12.5, label: 'the straight road between points stays within 25 cm of the ground' },
    { path: 'structuredContent.result.maxLiftCm', gte: 10, label: 'points lifted at least 10 cm, more over a crest between two' },
    { path: 'structuredContent.result.points.0.x', approximately: at.x + 2000, tolerance: 1, label: 'it starts at from' },
    { path: 'structuredContent.result.points.0.y', approximately: at.y + 25000, tolerance: 1, label: 'it starts at from (y)' },
    { path: 'structuredContent.result.nextCall.action', equals: 'create_road_spline', label: 'ready to lay' },
    { path: 'structuredContent.result.landscapeSplineCall.params.landscapeName', equals: LANDSCAPE, label: 'or to shape the landscape' },
    { path: 'structuredContent.result.landscapeSplineCall.params.width', equals: 300, label: 'a landscape spline takes half the road\'s width' },
  ] },
  { scenario: 'PLAN: straight across the flat at a slant, end to end', toolName: 'build_environment', arguments: { action: 'plan_road', from: { x: at.x + 5000, y: at.y + 5000 }, to: { x: at.x + 20000, y: at.y + 9000 } }, expected: 'success', assertions: [
    { path: 'structuredContent.result.points.1.x', approximately: at.x + 20000, tolerance: 1, label: 'it ends at to' },
    { path: 'structuredContent.result.points.1.y', approximately: at.y + 9000, tolerance: 1, label: 'it ends at to (y), in one straight run' },
    { path: 'structuredContent.result.lengthM', approximately: 155.2, tolerance: 0.1, label: 'as long as the line between them' },
  ] },
  { scenario: 'LAY: the planned road, as handed back', toolName: 'build_environment', arguments: { action: 'create_road_spline', name: ROAD, points: '${captured:roadPoints}', width: 600 }, expected: 'success' },
  { scenario: 'SHAPE: the same route as a landscape spline, as handed back', toolName: 'build_environment', arguments: { action: 'configure_landscape_splines', landscapeName: LANDSCAPE, points: '${captured:roadPoints}', width: 300 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.terrainShaped', equals: true, label: 'the terrain is shaped to the road' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'Setup: a box across a straight route', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: BOX, location: { x: at.x + 25000, y: at.y + 45000, z: 150 }, scale: { x: 4, y: 40, z: 4 } }, expected: 'success' },
  { scenario: 'PLAN: round the box', toolName: 'build_environment', arguments: { action: 'plan_road', ...across(45000), maxGradePercent: 10 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.lengthM', gte: 401, label: 'longer than the straight 400 m' },
  ] },
  { scenario: 'PLAN: on named ground', toolName: 'build_environment', arguments: { action: 'plan_road', ...across(48000), groundActors: [LANDSCAPE] }, expected: 'success' },
  { scenario: 'ERROR: an end on the box', toolName: 'build_environment', arguments: { action: 'plan_road', from: across(45000).from, to: { x: at.x + 25000, y: at.y + 45000 } }, expected: 'error|POINT_BLOCKED' },
  { scenario: 'ERROR: 1% cannot climb the hill', toolName: 'build_environment', arguments: { action: 'plan_road', from: { x: at.x + 2000, y: at.y + 25000 }, to: top, maxGradePercent: 1 }, expected: 'error|NO_ROUTE' },
  { scenario: 'ERROR: unknown ground', toolName: 'build_environment', arguments: { action: 'plan_road', ...across(48000), groundActors: [`NoSuchFloor_${ts}`] }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'Cleanup: road, box and landscape', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [ROAD, BOX, LANDSCAPE] }, expected: 'success' },
];

runToolTests('plan-road', testCases, {});
