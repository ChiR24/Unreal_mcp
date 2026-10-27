#!/usr/bin/env node
// inspect analyze_view: the level viewport's picture judged in numbers. A cube in front of a wall on a floor, far from
// everything else, the camera placed to frame it.

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPAnalyzeTag_${ts}`;
const SUBJECT = `MCP_AnalyzeCube_${ts}`;
const CAMERA = { location: { x: 39400, y: 40000, z: 160 }, rotation: { pitch: -5, yaw: 0, roll: 0 } };

const testCases = [
  { scenario: 'Setup: a cube before a wall', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: `MCP_AnalyzeFloor_${ts}`, location: { x: 40000, y: 40000, z: 0 }, scale: [20, 20, 0.2] },
    { actorName: `MCP_AnalyzeWall_${ts}`, location: { x: 40600, y: 40000, z: 500 }, scale: [0.2, 20, 10] },
    { actorName: SUBJECT, location: { x: 40000, y: 40000, z: 60 } },
  ] }, expected: 'success' },
  { scenario: 'Setup: a sun, so the scene is lit', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.DirectionalLight', actorName: `MCP_AnalyzeSun_${ts}`, location: { x: 40000, y: 40000, z: 800 }, rotation: { pitch: -40, yaw: 150, roll: 0 }, tags: [TAG] }, expected: 'success' },
  { scenario: 'READ: the eye goes to the cube', toolName: 'inspect', arguments: { action: 'analyze_view', subject: SUBJECT, aspects: ['saliency'], ...CAMERA }, expected: 'success', assertions: [
    { path: 'structuredContent.result.saliency.subject.rank', equals: 1, label: 'the cube draws the most attention' },
    { path: 'structuredContent.result.saliency.top', minLength: 1, label: 'actors ranked by attention' },
  ] },
  { scenario: 'READ: palette and light of the same view', toolName: 'inspect', arguments: { action: 'analyze_view', subject: SUBJECT, aspects: ['palette', 'light'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.palette.colors', minLength: 1, label: 'a palette' },
    { path: 'structuredContent.result.light.subject.keyFill', gte: 0, label: 'its sunlit side brighter than its shadowed side' },
    { path: 'structuredContent.result.light.histogram', length: 8, label: 'eight lightness bins' },
  ] },
  { scenario: 'READ: shader cost of the view, the view mode put back', toolName: 'inspect', arguments: { action: 'analyze_view', aspects: ['cost'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.cost.high', gte: 0, label: 'a share at high cost' },
  ] },
  { scenario: 'Setup: the cube drawn with an unlit material', toolName: 'control_actor', arguments: { action: 'set_material', actorName: SUBJECT, materialPath: '/Engine/EngineDebugMaterials/LevelColorationUnlitMaterial' }, expected: 'success' },
  { scenario: 'READ: no light shades an unlit subject, so it has no key or fill side', toolName: 'inspect', arguments: { action: 'analyze_view', subject: SUBJECT, aspects: ['light'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.light.subject.keyFillNote', exists: true, label: 'a note instead of a ratio' },
    { path: 'structuredContent.result.light.subject.keyFill', exists: false, label: 'no ratio read off its meaningless normals' },
  ] },
  { scenario: 'ERROR: an unknown aspect', toolName: 'inspect', arguments: { action: 'analyze_view', aspects: ['beauty'] }, expected: 'error' },
  { scenario: 'ERROR: an unknown subject', toolName: 'inspect', arguments: { action: 'analyze_view', subject: `MCP_NoSuchActor_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
];

runToolTests('analyze-view', testCases, {});
