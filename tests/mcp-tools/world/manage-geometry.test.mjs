#!/usr/bin/env node
/**
 * manage_geometry Tool Integration Tests
 * Covers all 74 actions with proper setup/teardown sequencing.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/WorldAssets';
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const ts = Date.now();
const TEST_MESH = '/Game/MCPTest/TestMesh';
const TEST_MESH_ALIAS = TEST_MESH.slice(1);
const EDIT_ACTOR = 'TestBox';
const TOOL_ACTOR = 'TestSphere';
const SPLINE_ACTOR = 'TestSpline';
const DISPLACE_TEXTURE = `T_GeometryDisplace_${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: `TestActor_${ts}`, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },

  // === CREATE ===
  { scenario: 'CREATE: create_box', toolName: 'manage_geometry', arguments: {"action": "create_box", "name": `Testbox_${ts}`, "path": TEST_FOLDER_ALIAS, "width": 120, "height": 80, "depth": 60, "widthSegments": 2, "heightSegments": 2, "depthSegments": 2, "rotation": {"pitch": 0, "yaw": 15, "roll": 0}, "scale": {"x": 1, "y": 1, "z": 1}}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_sphere', toolName: 'manage_geometry', arguments: {"action": "create_sphere", "name": `Testsphere_${ts}`, "path": "/Game/MCPTest", "radius": 45, "radialSegments": 12}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_cylinder', toolName: 'manage_geometry', arguments: {"action": "create_cylinder", "name": `Testcylinder_${ts}`, "path": "/Game/MCPTest", "radius": 40, "height": 120, "numSides": 12}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_cone', toolName: 'manage_geometry', arguments: {"action": "create_cone", "name": `Testcone_${ts}`, "path": "/Game/MCPTest", "radius": 45, "height": 110, "numSides": 12}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_capsule', toolName: 'manage_geometry', arguments: {"action": "create_capsule", "name": `Testcapsule_${ts}`, "path": "/Game/MCPTest", "radius": 35, "radialSegments": 12, "numRings": 4}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_torus', toolName: 'manage_geometry', arguments: {"action": "create_torus", "name": `Testtorus_${ts}`, "path": "/Game/MCPTest", "radius": 70, "innerRadius": 20, "numSides": 18, "radialSegments": 8}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_plane', toolName: 'manage_geometry', arguments: {"action": "create_plane", "name": "Testplane", "path": "/Game/MCPTest"}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_disc', toolName: 'manage_geometry', arguments: {"action": "create_disc", "name": `Testdisc_${ts}`, "path": "/Game/MCPTest", "radius": 55, "numSides": 24}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_stairs', toolName: 'manage_geometry', arguments: {"action": "create_stairs", "name": `Teststairs_${ts}`, "path": "/Game/MCPTest", "numSteps": 5, "stepWidth": 90, "stepHeight": 18, "stepDepth": 28}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_spiral_stairs', toolName: 'manage_geometry', arguments: {"action": "create_spiral_stairs", "name": `Testspiral_stairs_${ts}`, "path": "/Game/MCPTest", "numSteps": 6, "stepWidth": 80, "stepHeight": 16, "innerRadius": 120, "numTurns": 0.5}, expected: 'success|already exists' },
  { scenario: 'Reset: cleanup geometry actors', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'GeoTest' }, expected: 'success|not found' },
  { scenario: 'CREATE: create_ring', toolName: 'manage_geometry', arguments: {"action": "create_ring", "name": `Testring_${ts}`, "path": "/Game/MCPTest", "radius": 55, "innerRadius": 25, "numSides": 24}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_arch', toolName: 'manage_geometry', arguments: {"action": "create_arch", "name": `Testarch_${ts}`, "path": "/Game/MCPTest", "radius": 90, "innerRadius": 20, "radialSegments": 12, "numRings": 6}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_pipe', toolName: 'manage_geometry', arguments: {"action": "create_pipe", "name": `Testpipe_${ts}`, "path": "/Game/MCPTest", "radius": 55, "innerRadius": 35, "height": 110, "numSides": 18, "heightSegments": 2}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_ramp', toolName: 'manage_geometry', arguments: {"action": "create_ramp", "name": "Testramp", "path": "/Game/MCPTest"}, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: boolean_union', toolName: 'manage_geometry', arguments: {"action": "boolean_union", "targetActor": EDIT_ACTOR, "toolActor": TOOL_ACTOR, "keepTool": true}, expected: 'success' },
  { scenario: 'ACTION: boolean_subtract', toolName: 'manage_geometry', arguments: {"action": "boolean_subtract", "targetActor": EDIT_ACTOR, "toolActor": TOOL_ACTOR, "keepTool": true}, expected: 'success' },
  { scenario: 'Setup: reset boolean target', toolName: 'control_actor', arguments: { action: 'delete', actorName: EDIT_ACTOR }, expected: 'success' },
  { scenario: 'Setup: reset boolean tool', toolName: 'control_actor', arguments: { action: 'delete', actorName: TOOL_ACTOR }, expected: 'success' },
  { scenario: 'Setup: recreate boolean target', toolName: 'manage_geometry', arguments: { action: 'create_box', name: EDIT_ACTOR, width: 100, height: 100, depth: 100 }, expected: 'success' },
  { scenario: 'Setup: recreate boolean tool', toolName: 'manage_geometry', arguments: { action: 'create_sphere', name: TOOL_ACTOR, radius: 50, segments: 16 }, expected: 'success' },
  { scenario: 'ACTION: boolean_intersection', toolName: 'manage_geometry', arguments: {"action": "boolean_intersection", "targetActor": EDIT_ACTOR, "toolActor": TOOL_ACTOR, "keepTool": true}, expected: 'success' },
  { scenario: 'Setup: reset trim target', toolName: 'control_actor', arguments: { action: 'delete', actorName: EDIT_ACTOR }, expected: 'success' },
  { scenario: 'Setup: reset trim tool', toolName: 'control_actor', arguments: { action: 'delete', actorName: TOOL_ACTOR }, expected: 'success' },
  { scenario: 'Setup: recreate trim target', toolName: 'manage_geometry', arguments: { action: 'create_box', name: EDIT_ACTOR, width: 100, height: 100, depth: 100 }, expected: 'success' },
  { scenario: 'Setup: recreate trim tool', toolName: 'manage_geometry', arguments: { action: 'create_sphere', name: TOOL_ACTOR, radius: 50, segments: 16 }, expected: 'success' },
  { scenario: 'ACTION: boolean_trim', toolName: 'manage_geometry', arguments: {"action": "boolean_trim", "actorName": EDIT_ACTOR, "trimActorName": TOOL_ACTOR, "keepInside": false}, expected: 'success' },
  { scenario: 'ACTION: self_union', toolName: 'manage_geometry', arguments: {"action": "self_union", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: extrude', toolName: 'manage_geometry', arguments: {"action": "extrude", "actorName": EDIT_ACTOR, "amount": 8}, expected: 'success' },
  { scenario: 'Reset: cleanup geometry actors', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'GeoTest' }, expected: 'success|not found' },
  { scenario: 'ACTION: inset', toolName: 'manage_geometry', arguments: {"action": "inset", "actorName": EDIT_ACTOR, "distance": 4}, expected: 'success' },
  { scenario: 'ACTION: outset', toolName: 'manage_geometry', arguments: {"action": "outset", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: bevel', toolName: 'manage_geometry', arguments: {"action": "bevel", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: offset_faces', toolName: 'manage_geometry', arguments: {"action": "offset_faces", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: shell', toolName: 'manage_geometry', arguments: {"action": "shell", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: revolve', toolName: 'manage_geometry', arguments: {"action": "revolve", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: chamfer', toolName: 'manage_geometry', arguments: {"action": "chamfer", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: extrude_along_spline', toolName: 'manage_geometry', arguments: {"action": "extrude_along_spline", "actorName": EDIT_ACTOR, "splineActorName": SPLINE_ACTOR, "segments": 8, "cap": true}, expected: 'success' },
  { scenario: 'ACTION: bridge', toolName: 'manage_geometry', arguments: {"action": "bridge", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: loft', toolName: 'manage_geometry', arguments: {"action": "loft", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'Reset: cleanup geometry actors', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'GeoTest' }, expected: 'success|not found' },
  { scenario: 'ACTION: sweep', toolName: 'manage_geometry', arguments: {"action": "sweep", "actorName": EDIT_ACTOR, "splineActorName": SPLINE_ACTOR, "steps": 8}, expected: 'success' },
  { scenario: 'ACTION: duplicate_along_spline', toolName: 'manage_geometry', arguments: {"action": "duplicate_along_spline", "actorName": EDIT_ACTOR, "splineActorName": SPLINE_ACTOR, "count": 3}, expected: 'success' },
  { scenario: 'ACTION: loop_cut', toolName: 'manage_geometry', arguments: {"action": "loop_cut", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: edge_split', toolName: 'manage_geometry', arguments: {"action": "edge_split", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: quadrangulate', toolName: 'manage_geometry', arguments: {"action": "quadrangulate", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: bend', toolName: 'manage_geometry', arguments: {"action": "bend", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: twist', toolName: 'manage_geometry', arguments: {"action": "twist", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: taper', toolName: 'manage_geometry', arguments: {"action": "taper", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: noise_deform', toolName: 'manage_geometry', arguments: {"action": "noise_deform", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: smooth', toolName: 'manage_geometry', arguments: {"action": "smooth", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'Reset: cleanup geometry actors', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'GeoTest' }, expected: 'success|not found' },
  { scenario: 'ACTION: relax', toolName: 'manage_geometry', arguments: {"action": "relax", "actorName": EDIT_ACTOR, "iterations": 2, "strength": 0.35}, expected: 'success' },
  { scenario: 'ACTION: stretch', toolName: 'manage_geometry', arguments: {"action": "stretch", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: spherify', toolName: 'manage_geometry', arguments: {"action": "spherify", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: cylindrify', toolName: 'manage_geometry', arguments: {"action": "cylindrify", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: lattice_deform', toolName: 'manage_geometry', arguments: {"action": "lattice_deform", "actorName": EDIT_ACTOR, "latticeResolution": 3, "position": {"x": 0, "y": 0, "z": 0}, "weight": 0.35, "axis": "Z"}, expected: 'success' },
  { scenario: 'Setup: create texture for displacement', toolName: 'manage_asset', arguments: { action: 'create_noise_texture', name: DISPLACE_TEXTURE, path: TEST_FOLDER, width: 32, height: 32, scale: 2, octaves: 2, save: true }, expected: 'success|already exists' },
  { scenario: 'ACTION: displace_by_texture', toolName: 'manage_geometry', arguments: {"action": "displace_by_texture", "actorName": EDIT_ACTOR, "texturePath": `${TEST_FOLDER_ALIAS}/${DISPLACE_TEXTURE}`, "heightScale": 12, "midpoint": 0.5, "axis": "Z"}, expected: 'success' },
  { scenario: 'ACTION: triangulate', toolName: 'manage_geometry', arguments: {"action": "triangulate", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: poke', toolName: 'manage_geometry', arguments: {"action": "poke", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: mirror', toolName: 'manage_geometry', arguments: {"action": "mirror", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: array_linear', toolName: 'manage_geometry', arguments: {"action": "array_linear", "actorName": EDIT_ACTOR, "count": 3, "offset": {"x": 125, "y": 0, "z": 0}}, expected: 'success' },
  { scenario: 'ACTION: array_radial', toolName: 'manage_geometry', arguments: {"action": "array_radial", "actorName": EDIT_ACTOR, "count": 4, "center": {"x": 0, "y": 0, "z": 0}, "axis": {"x": 0, "y": 0, "z": 1}, "angle": 180}, expected: 'success' },
  { scenario: 'ACTION: simplify_mesh', toolName: 'manage_geometry', arguments: {"action": "simplify_mesh", "actorName": EDIT_ACTOR, "reductionPercent": 25}, expected: 'success' },
  { scenario: 'Reset: cleanup geometry actors', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'GeoTest' }, expected: 'success|not found' },
  { scenario: 'ACTION: subdivide', toolName: 'manage_geometry', arguments: {"action": "subdivide", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: remesh_uniform', toolName: 'manage_geometry', arguments: {"action": "remesh_uniform", "actorName": EDIT_ACTOR, "targetTriangleCount": 256}, expected: 'success' },
  { scenario: 'ACTION: merge_vertices', toolName: 'manage_geometry', arguments: {"action": "merge_vertices", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: remesh_voxel', toolName: 'manage_geometry', arguments: {"action": "remesh_voxel", "actorName": EDIT_ACTOR, "targetEdgeLength": 12}, expected: 'success' },
  { scenario: 'ACTION: weld_vertices', toolName: 'manage_geometry', arguments: {"action": "weld_vertices", "actorName": EDIT_ACTOR, "weldDistance": 0.001}, expected: 'success' },
  { scenario: 'ACTION: fill_holes', toolName: 'manage_geometry', arguments: {"action": "fill_holes", "actorName": EDIT_ACTOR}, expected: 'success' },
  // === DELETE ===
  { scenario: 'DELETE: remove_degenerates', toolName: 'manage_geometry', arguments: {"action": "remove_degenerates", "actorName": EDIT_ACTOR}, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: auto_uv', toolName: 'manage_geometry', arguments: {"action": "auto_uv", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: project_uv', toolName: 'manage_geometry', arguments: {"action": "project_uv", "actorName": EDIT_ACTOR, "uvChannel": 0}, expected: 'success' },
  { scenario: 'ACTION: transform_uvs', toolName: 'manage_geometry', arguments: {"action": "transform_uvs", "actorName": EDIT_ACTOR, "uvChannel": 0, "uvOffset": {"u": 0.1, "v": 0.2}, "uvScale": {"u": 1.1, "v": 0.9}, "rotation": 15}, expected: 'success' },
  { scenario: 'Reset: cleanup geometry actors', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'GeoTest' }, expected: 'success|not found' },
  { scenario: 'ACTION: unwrap_uv', toolName: 'manage_geometry', arguments: {"action": "unwrap_uv", "actorName": EDIT_ACTOR, "uvChannel": 0}, expected: 'success' },
  { scenario: 'ACTION: pack_uv_islands', toolName: 'manage_geometry', arguments: {"action": "pack_uv_islands", "actorName": EDIT_ACTOR, "uvChannel": 0}, expected: 'success' },
  { scenario: 'ACTION: recalculate_normals', toolName: 'manage_geometry', arguments: {"action": "recalculate_normals", "actorName": EDIT_ACTOR, "computeWeightedNormals": true, "hardEdgeAngle": 45}, expected: 'success' },
  { scenario: 'ACTION: flip_normals', toolName: 'manage_geometry', arguments: {"action": "flip_normals", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: recompute_tangents', toolName: 'manage_geometry', arguments: {"action": "recompute_tangents", "actorName": EDIT_ACTOR}, expected: 'success' },
  { scenario: 'ACTION: generate_collision', toolName: 'manage_geometry', arguments: {"action": "generate_collision", "actorName": EDIT_ACTOR, "collisionType": "convex"}, expected: 'success|already exists' },
  { scenario: 'ACTION: generate_complex_collision', toolName: 'manage_geometry', arguments: {"action": "generate_complex_collision", "actorName": EDIT_ACTOR, "hullCount": 8, "maxHullCount": 8, "maxVerticesPerHull": 32, "maxHullVerts": 32, "hullPrecision": 100}, expected: 'success|already exists' },
  { scenario: 'ACTION: simplify_collision', toolName: 'manage_geometry', arguments: {"action": "simplify_collision", "actorName": EDIT_ACTOR, "targetHullCount": 4, "simplificationFactor": 0.5}, expected: 'success' },
  { scenario: 'ACTION: generate_lods', toolName: 'manage_geometry', arguments: {"action": "generate_lods", "assetPath": TEST_MESH_ALIAS, "lodCount": 3}, expected: 'success|already exists' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_lod_settings', toolName: 'manage_geometry', arguments: {"action": "set_lod_settings", "assetPath": TEST_MESH_ALIAS, "lodIndex": 0, "reductionPercent": 50, "recomputeNormals": false, "recomputeTangents": false}, expected: 'success' },
  { scenario: 'CONFIG: set_lod_settings triangle percent', toolName: 'manage_geometry', arguments: {"action": "set_lod_settings", "assetPath": TEST_MESH_ALIAS, "lodIndex": 0, "trianglePercent": 75}, expected: 'success' },
  { scenario: 'Reset: cleanup geometry actors', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'GeoTest' }, expected: 'success|not found' },
  { scenario: 'CONFIG: set_lod_screen_sizes', toolName: 'manage_geometry', arguments: {"action": "set_lod_screen_sizes", "assetPath": TEST_MESH_ALIAS, "screenSizes": [1.0, 0.5, 0.25]}, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: convert_to_nanite', toolName: 'manage_geometry', arguments: {"action": "convert_to_nanite", "actorName": EDIT_ACTOR, "outputPath": `Game/GeneratedMeshes/TestBox_Nanite_${ts}`}, expected: 'success' },
  { scenario: 'ACTION: convert_to_static_mesh', toolName: 'manage_geometry', arguments: {"action": "convert_to_static_mesh", "actorName": EDIT_ACTOR, "outputPath": `Game/GeneratedMeshes/TestBox_Static_${ts}`}, expected: 'success' },
  // === INFO ===
  { scenario: 'INFO: get_mesh_info', toolName: 'manage_geometry', arguments: {"action": "get_mesh_info", "actorName": EDIT_ACTOR}, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: `TestActor_${ts}` }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-geometry', testCases);
