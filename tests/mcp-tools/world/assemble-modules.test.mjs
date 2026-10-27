#!/usr/bin/env node
// build_environment assemble_modules: a corridor kit made here (a slab with arms out to the sides it opens on: end,
// straight, corner, tee and cross, 4 m cells, pivot at the floor's middle) lays out a grid with no open face, as one
// actor; a pillar reaching the top of a one-layer grid is never placed and named; unturned pieces lay out once +X is
// open; a pillar alone, a pivot the pieces were not made for and a missing mesh are refused.
// (tests/mcp-tools/world/assemble-modules.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = `/Game/_McpScratch/Kit_${ts}`;
const ASSEMBLY = `Dungeon_${ts}`;
// Arms out of a 160 x 160 cm slab 90 cm tall, to the cell's sides at 200 cm.
const SLAB = { min: { x: -80, y: -80, z: 0 }, max: { x: 80, y: 80, z: 90 } };
const ARM = {
  px: { min: { x: 80, y: -80, z: 0 }, max: { x: 200, y: 80, z: 90 } },
  nx: { min: { x: -200, y: -80, z: 0 }, max: { x: -80, y: 80, z: 90 } },
  py: { min: { x: -80, y: 80, z: 0 }, max: { x: 80, y: 200, z: 90 } },
  ny: { min: { x: -80, y: -200, z: 0 }, max: { x: 80, y: -80, z: 90 } },
};
const PIECES = { End: ['px'], Straight: ['px', 'nx'], Corner: ['px', 'py'], Tee: ['px', 'nx', 'py'], Cross: ['px', 'nx', 'py', 'ny'] };
const CORRIDORS = Object.keys(PIECES);
// A pillar the full height of the cell: its top meets the top of a one-layer grid, which takes only empty faces.
const PILLAR = { min: { x: -40, y: -40, z: 0 }, max: { x: 40, y: 40, z: 300 } };
const meshOf = (piece) => `${FOLDER}/SM_${piece}`;
const kit = (pieces) => pieces.map((piece) => ({ meshPath: meshOf(piece) }));
const CELL = { x: 400, y: 400, z: 300 };

// Each piece: a 1 cm cube at the floor's middle (the actor's origin, which becomes the mesh's pivot), the slab and its
// arms appended, baked into a static mesh.
const authoring = [...Object.entries(PIECES), ['Pillar', null]].flatMap(([piece, arms]) => [
  { scenario: `Setup: ${piece} actor`, toolName: 'manage_geometry', arguments: { action: 'create_box', name: `Kit${piece}_${ts}`, dimensions: { x: 1, y: 1, z: 1 }, location: { x: 900000, y: 900000, z: 0 } }, expected: 'success' },
  { scenario: `Setup: ${piece} slab and arms`, toolName: 'manage_geometry', arguments: { action: 'append_cuboids', actorName: `Kit${piece}_${ts}`, boxes: arms ? [SLAB, ...arms.map((arm) => ARM[arm])] : [PILLAR] }, expected: 'success' },
  { scenario: `Setup: ${piece} mesh`, toolName: 'manage_geometry', arguments: { action: 'convert_to_static_mesh', actorName: `Kit${piece}_${ts}`, outputPath: meshOf(piece) }, expected: 'success' },
]);

const testCases = [
  ...authoring,
  { scenario: 'DRYRUN: the corridor kit lays out 8 x 6 cells', toolName: 'build_environment', arguments: { action: 'assemble_modules', modules: kit(CORRIDORS), cellSize: CELL, pivot: 'bottom_center', size: { x: 8, y: 6, z: 1 }, seed: 3, dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.openFaces', equals: 0, label: 'every face where two pieces meet matches' },
    { path: 'structuredContent.result.cells', equals: 48, label: 'all 48 cells decided' },
    { path: 'structuredContent.result.variants', equals: 21, label: 'five pieces at four turns, and the empty cell' },
  ] },
  { scenario: 'ASSEMBLE: spawned as one actor, the pillar never placed', toolName: 'build_environment', arguments: { action: 'assemble_modules', modules: kit([...CORRIDORS, 'Pillar']), cellSize: CELL, pivot: 'bottom_center', size: { x: 8, y: 6, z: 1 }, seed: 3, location: { x: 900000, y: 920000, z: 0 }, name: ASSEMBLY }, expected: 'success', assertions: [
    { path: 'structuredContent.result.actorName', equals: ASSEMBLY, label: 'named as asked' },
    { path: 'structuredContent.result.openFaces', equals: 0, label: 'no open face' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
    { path: 'structuredContent.result.unmatchedFaces.0', includes: 'SM_Pillar', label: 'the pillar is named as never placed' },
  ] },
  { scenario: 'DRYRUN: unturned pieces, with +X open for the arms that face it', toolName: 'build_environment', arguments: { action: 'assemble_modules', modules: kit(CORRIDORS), cellSize: CELL, pivot: 'bottom_center', size: { x: 6, y: 4, z: 1 }, rotations: false, openSides: ['-Z', '+X'], dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.variants', equals: 6, label: 'five pieces unturned, and the empty cell' },
    { path: 'structuredContent.result.openFaces', equals: 0, label: 'no open face' },
  ] },
  { scenario: 'ERROR: a pillar alone fits no one-layer grid, and is named', toolName: 'build_environment', arguments: { action: 'assemble_modules', modules: kit(['Pillar']), emptyWeight: 0, cellSize: CELL, pivot: 'bottom_center', size: { x: 2, y: 2, z: 1 } }, expected: 'error|NO_SOLUTION' },
  { scenario: 'ERROR: a pivot the pieces were not made for', toolName: 'build_environment', arguments: { action: 'assemble_modules', modules: kit(['Cross']), cellSize: CELL, pivot: 'corner', size: { x: 2, y: 2, z: 1 } }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: a mesh that is not there', toolName: 'build_environment', arguments: { action: 'assemble_modules', modules: [{ meshPath: `${FOLDER}/SM_Missing` }], cellSize: CELL, pivot: 'bottom_center', size: { x: 2, y: 2, z: 1 } }, expected: 'error|MESH_NOT_FOUND' },
  { scenario: 'Cleanup: the assembly and the piece actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [ASSEMBLY, ...[...CORRIDORS, 'Pillar'].map((piece) => `Kit${piece}_${ts}`)] }, expected: 'success' },
  { scenario: 'Cleanup: the kit folder', toolName: 'manage_asset', arguments: { action: 'delete', paths: [FOLDER] }, expected: 'success' },
];

runToolTests('assemble-modules', testCases, {});
