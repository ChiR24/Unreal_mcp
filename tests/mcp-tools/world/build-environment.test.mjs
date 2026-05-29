#!/usr/bin/env node
/**
 * build_environment Tool Integration Tests
 * Covers all 23 actions with proper setup/teardown sequencing.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/WorldAssets_${ts}`;
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const LANDSCAPE_NAME = `TestLandscape_${ts}`;
const FOLIAGE_TYPE_NAME = `TestFoliage_${ts}`;
const FOLIAGE_TYPE_PATH = `/Game/Foliage/${FOLIAGE_TYPE_NAME}`;
const FOLIAGE_TYPE_PATH_ALIAS = FOLIAGE_TYPE_PATH.slice(1);
const TEST_MESH = '/Engine/BasicShapes/Sphere';
const TEST_MESH_ALIAS = TEST_MESH.slice(1);
const TEST_MATERIAL = '/Engine/BasicShapes/BasicShapeMaterial';
const TEST_MATERIAL_ALIAS = TEST_MATERIAL.slice(1);
const SNAPSHOT_DIR = './tmp/unreal-mcp/build-environment';
const SNAPSHOT_FILE = `snapshot_${ts}.json`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: `TestActor_${ts}`, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
  { scenario: 'Setup: spawn environment delete actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: `EnvDeleteActor_${ts}`, location: { x: 150, y: 0, z: 100 } }, expected: 'success' },

  // === CREATE ===
  { scenario: 'CREATE: create_landscape', toolName: 'build_environment', arguments: {"action": "create_landscape", "name": LANDSCAPE_NAME, "path": TEST_FOLDER_ALIAS, "materialPath": TEST_MATERIAL_ALIAS, "location": {"x": 0, "y": 0, "z": 0}, "sizeX": 1000, "sizeY": 1000, "sectionSize": 7, "quadsPerSection": 7, "sectionsPerComponent": 1, "componentCount": {"x": 1, "y": 1}}, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: sculpt', toolName: 'build_environment', arguments: {"action": "sculpt", "landscapeName": LANDSCAPE_NAME, "tool": "Raise", "location": {"x": 0, "y": 0, "z": 0}, "radius": 128, "falloff": 0.25, "strength": 0.1, "skipFlush": true}, expected: 'success' },
  { scenario: 'ACTION: sculpt_landscape', toolName: 'build_environment', arguments: {"action": "sculpt_landscape", "landscapeName": LANDSCAPE_NAME, "location": {"x": 64, "y": 64, "z": 0}, "radius": 128, "strength": 0.1, "skipFlush": true}, expected: 'success' },
  // === ADD ===
  { scenario: 'ADD: add_foliage', toolName: 'build_environment', arguments: {"action": "add_foliage", "name": FOLIAGE_TYPE_NAME, "meshPath": TEST_MESH_ALIAS, "density": 10, "alignToNormal": false, "randomYaw": false, "cullDistance": 2500}, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: paint_foliage', toolName: 'build_environment', arguments: {"action": "paint_foliage", "foliageType": FOLIAGE_TYPE_PATH_ALIAS, "locations": [{"x": 0, "y": 0, "z": 100}]}, expected: 'success' },
  { scenario: 'ADD: add_foliage generated positions', toolName: 'build_environment', arguments: {"action": "add_foliage", "foliageType": FOLIAGE_TYPE_PATH_ALIAS, "location": {"x": 25, "y": 25, "z": 100}, "radius": 50, "count": 2}, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_procedural_terrain', toolName: 'build_environment', arguments: {"action": "create_procedural_terrain", "name": `TestProceduralTerrain_${ts}`, "path": TEST_FOLDER_ALIAS, "sizeX": 8, "sizeY": 8, "heightScale": 40, "subdivisions": 4, "rotation": {"pitch": 0, "yaw": 15, "roll": 0}, "material": TEST_MATERIAL_ALIAS}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_procedural_foliage', toolName: 'build_environment', arguments: {"action": "create_procedural_foliage", "volumeName": `TestProceduralFoliage_${ts}`, "path": TEST_FOLDER_ALIAS, "bounds": {"location": {"x": 0, "y": 0, "z": 0}, "size": {"x": 500, "y": 500, "z": 300}}, "seed": 123, "tileSize": 500, "foliageTypes": [{"meshPath": TEST_MESH_ALIAS, "density": 1}]}, expected: 'success|already exists' },
  // === ADD ===
  { scenario: 'ADD: add_foliage_instances', toolName: 'build_environment', arguments: {"action": "add_foliage_instances", "foliageTypePath": FOLIAGE_TYPE_PATH_ALIAS, "locations": [{"x": 100, "y": 0, "z": 100}], "transforms": [{"location": {"x": 100, "y": 0, "z": 100}, "rotation": {"pitch": 0, "yaw": 45, "roll": 0}, "scale": {"x": 1.1, "y": 1.1, "z": 1.1}}]}, expected: 'success|already exists' },
  // === INFO ===
  { scenario: 'INFO: get_foliage_instances', toolName: 'build_environment', arguments: {"action": "get_foliage_instances"}, expected: 'success' },
  // === DELETE ===
  { scenario: 'DELETE: remove_foliage', toolName: 'build_environment', arguments: {"action": "remove_foliage"}, expected: 'success|not found' },
  // === ACTION ===
  { scenario: 'ACTION: paint_landscape', toolName: 'build_environment', arguments: {"action": "paint_landscape", "landscapeName": LANDSCAPE_NAME, "layerName": "TestLayer", "region": {"minX": 0, "minY": 0, "maxX": 1, "maxY": 1}, "skipFlush": true}, expected: 'success' },
  { scenario: 'ACTION: paint_landscape_layer', toolName: 'build_environment', arguments: {"action": "paint_landscape_layer", "landscapeName": LANDSCAPE_NAME, "layerName": "TestLayer", "region": {"minX": 0, "minY": 0, "maxX": 1, "maxY": 1}, "skipFlush": true}, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: modify_heightmap', toolName: 'build_environment', arguments: {"action": "modify_heightmap", "landscapeName": LANDSCAPE_NAME, "operation": "add", "heightData": [0], "minX": 0, "minY": 0, "maxX": 0, "maxY": 0, "updateNormals": true, "skipFlush": true}, expected: 'success' },
  { scenario: 'CONFIG: set_landscape_material', toolName: 'build_environment', arguments: {"action": "set_landscape_material", "landscapeName": LANDSCAPE_NAME, "materialPath": TEST_MATERIAL_ALIAS}, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_landscape_grass_type', toolName: 'build_environment', arguments: {"action": "create_landscape_grass_type", "name": `TestLandscapeGrassType_${ts}`, "path": TEST_FOLDER_ALIAS, "staticMesh": TEST_MESH_ALIAS}, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: generate_lods', toolName: 'build_environment', arguments: {"action": "generate_lods", "assetPaths": [TEST_MESH_ALIAS], "assets": [TEST_MESH_ALIAS], "numLODs": 2}, expected: 'success|already exists' },
  { scenario: 'ACTION: bake_lightmap', toolName: 'build_environment', arguments: {"action": "bake_lightmap"}, expected: 'success' },
  { scenario: 'ACTION: export_snapshot', toolName: 'build_environment', arguments: {"action": "export_snapshot", "path": SNAPSHOT_DIR, "filename": SNAPSHOT_FILE}, expected: 'success' },
  { scenario: 'ACTION: import_snapshot', toolName: 'build_environment', arguments: {"action": "import_snapshot", "path": SNAPSHOT_DIR, "filename": SNAPSHOT_FILE}, expected: 'success' },
  // === DELETE ===
  { scenario: 'DELETE: delete', toolName: 'build_environment', arguments: {"action": "delete", "names": [`EnvDeleteActor_${ts}`]}, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_sky_sphere', toolName: 'build_environment', arguments: {"action": "create_sky_sphere", "name": `TestSkySphere_${ts}`, "path": TEST_FOLDER}, expected: 'success|already exists' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_time_of_day', toolName: 'build_environment', arguments: {"action": "set_time_of_day", "time": 9, "hour": 9, "propertyName": "time_of_day", "propertyValue": 1}, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_fog_volume', toolName: 'build_environment', arguments: {"action": "create_fog_volume", "name": `TestFogVolume_${ts}`, "path": TEST_FOLDER}, expected: 'success|already exists' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: `TestActor_${ts}` }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

// === LIGHTING ACTIONS ===
{
  /**
   * build_environment lighting action integration tests
   * Covers all 15 actions with proper setup/teardown sequencing.
   */

  const TEST_FOLDER = '/Game/MCPTest/WorldAssets';
  const ts = Date.now();
  const TEST_ACTOR = `TestActor_${ts}`;

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },

    // === CREATE ===
    { scenario: 'CREATE: spawn_light', toolName: 'build_environment', arguments: {"action": "spawn_light", "lightType": "Point", "location": {"x": 0, "y": 0, "z": 100}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_light', toolName: 'build_environment', arguments: {"action": "create_light", "name": "Testlight", "path": "/Game/MCPTest"}, expected: 'success|already exists' },
    { scenario: 'CREATE: spawn_sky_light', toolName: 'build_environment', arguments: {"action": "spawn_sky_light", "location": {"x": 0, "y": 0, "z": 100}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_sky_light', toolName: 'build_environment', arguments: {"action": "create_sky_light", "name": "Testsky_light", "path": "/Game/MCPTest"}, expected: 'success|already exists' },
    // === ACTION ===
    { scenario: 'ACTION: ensure_single_sky_light', toolName: 'build_environment', arguments: {"action": "ensure_single_sky_light"}, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_lightmass_volume', toolName: 'build_environment', arguments: {"action": "create_lightmass_volume", "name": "Testlightmass_volume", "path": "/Game/MCPTest"}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_lighting_enabled_level', toolName: 'build_environment', arguments: {"action": "create_lighting_enabled_level", "name": "Testlighting_enabled_level", "path": "/Game/MCPTest/Testlighting_enabled_level"}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_dynamic_light', toolName: 'build_environment', arguments: {"action": "create_dynamic_light", "name": "Testdynamic_light", "path": "/Game/MCPTest"}, expected: 'success|already exists' },
    // === ACTION ===
    { scenario: 'ACTION: setup_global_illumination', toolName: 'build_environment', arguments: {"action": "setup_global_illumination", "method": "LumenGI"}, expected: 'success|already exists' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_shadows', toolName: 'build_environment', arguments: {"action": "configure_shadows"}, expected: 'success' },
    { scenario: 'CONFIG: set_exposure', toolName: 'build_environment', arguments: {"action": "set_exposure", "method": "Manual", "minBrightness": 1, "maxBrightness": 1, "compensationValue": 0}, expected: 'success' },
    { scenario: 'CONFIG: set_ambient_occlusion', toolName: 'build_environment', arguments: {"action": "set_ambient_occlusion", "enabled": true, "intensity": 0.5, "radius": 2000, "quality": "High"}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: setup_volumetric_fog', toolName: 'build_environment', arguments: {"action": "setup_volumetric_fog"}, expected: 'success|already exists' },
    // === CREATE ===
    { scenario: 'CREATE: build_lighting', toolName: 'build_environment', arguments: {"action": "build_lighting"}, expected: 'success|already exists' },
    // === INFO ===
    { scenario: 'INFO: list_light_types', toolName: 'build_environment', arguments: {"action": "list_light_types"}, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup setup: restore test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success|already exists' },
    { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TEST_ACTOR }, expected: 'success' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

// === SPLINE ACTIONS ===
{
  /**
   * build_environment spline action integration tests
   * Covers all 22 actions with proper setup/teardown sequencing.
   */

  const ts = Date.now();
  const TEST_FOLDER = `/Game/MCPTest/WorldAssets_${ts}`;
  const SPLINE_ACTOR = `SplineActor_${ts}`;
  const SPLINE_MESH_BP = `BP_SplineMesh_${ts}`;
  const SPLINE_MESH_BP_PATH = `${TEST_FOLDER}/${SPLINE_MESH_BP}`;
  const SPLINE_MESH_ACTOR = `SplineMeshActor_${ts}`;
  const SPLINE_MESH_COMPONENT = 'SplineMeshComp';
  const TEST_MESH = `${TEST_FOLDER}/SM_SplineTest_${ts}`;
  const TEST_MESH_OBJECT = `${TEST_MESH}.SM_SplineTest_${ts}`;
  const TEST_MATERIAL = `${TEST_FOLDER}/M_SplineTest_${ts}`;
  const TEST_MATERIAL_OBJECT = `${TEST_MATERIAL}.M_SplineTest_${ts}`;
  const ROAD_SPLINE = `RoadSpline_${ts}`;
  const RIVER_SPLINE = `RiverSpline_${ts}`;
  const FENCE_SPLINE = `FenceSpline_${ts}`;
  const WALL_SPLINE = `WallSpline_${ts}`;
  const CABLE_SPLINE = `CableSpline_${ts}`;
  const PIPE_SPLINE = `PipeSpline_${ts}`;

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: `TestActor_${ts}`, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
    { scenario: 'Setup: duplicate spline test mesh', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/EngineMeshes/Cube', destinationPath: TEST_MESH }, expected: 'success|already exists' },
    { scenario: 'Setup: create spline test material', toolName: 'manage_asset', arguments: { action: 'create_material', name: `M_SplineTest_${ts}`, path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: create spline mesh blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_blueprint', name: SPLINE_MESH_BP, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },

    // === CREATE ===
    { scenario: 'CREATE: create_spline_actor', toolName: 'build_environment', arguments: { action: 'create_spline_actor', actorName: SPLINE_ACTOR, location: { x: 0, y: 0, z: 0 }, initialPoints: [{ location: { x: 0, y: 0, z: 0 } }, { location: { x: 300, y: 0, z: 0 } }], splineType: 'Curve', timeoutMs: 120000 }, expected: 'success|already exists' },
    // === ADD ===
    { scenario: 'ADD: add_spline_point', toolName: 'build_environment', arguments: { action: 'add_spline_point', actorName: SPLINE_ACTOR, position: { x: 600, y: 120, z: 0 }, pointType: 'Curve' }, expected: 'success|already exists' },
    // === DELETE ===
    { scenario: 'DELETE: remove_spline_point', toolName: 'build_environment', arguments: { action: 'remove_spline_point', actorName: SPLINE_ACTOR, pointIndex: 2 }, expected: 'success|not found' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_spline_point_position', toolName: 'build_environment', arguments: { action: 'set_spline_point_position', actorName: SPLINE_ACTOR, pointIndex: 1, position: { x: 350, y: 80, z: 0 } }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_point_tangents', toolName: 'build_environment', arguments: { action: 'set_spline_point_tangents', actorName: SPLINE_ACTOR, pointIndex: 1, arriveTangent: { x: 100, y: 25, z: 0 }, leaveTangent: { x: 100, y: 25, z: 0 } }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_point_rotation', toolName: 'build_environment', arguments: { action: 'set_spline_point_rotation', actorName: SPLINE_ACTOR, pointIndex: 1, pointRotation: { pitch: 0, yaw: 20, roll: 0 } }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_point_scale', toolName: 'build_environment', arguments: { action: 'set_spline_point_scale', actorName: SPLINE_ACTOR, pointIndex: 1, pointScale: { x: 1.25, y: 1.25, z: 1 } }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_type', toolName: 'build_environment', arguments: { action: 'set_spline_type', actorName: SPLINE_ACTOR, splineType: 'Linear' }, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_spline_mesh_component', toolName: 'build_environment', arguments: { action: 'create_spline_mesh_component', blueprintPath: SPLINE_MESH_BP_PATH, componentName: SPLINE_MESH_COMPONENT, meshPath: TEST_MESH_OBJECT, forwardAxis: 'X', save: true }, expected: 'success|already exists' },
    { scenario: 'Setup: spawn spline mesh blueprint actor', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: SPLINE_MESH_BP_PATH, actorName: SPLINE_MESH_ACTOR, location: { x: 300, y: 300, z: 0 } }, expected: 'success|already exists' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_spline_mesh_asset', toolName: 'build_environment', arguments: { action: 'set_spline_mesh_asset', actorName: SPLINE_MESH_ACTOR, componentName: SPLINE_MESH_COMPONENT, meshPath: TEST_MESH_OBJECT }, expected: 'success' },
    { scenario: 'CONFIG: configure_spline_mesh_axis', toolName: 'build_environment', arguments: { action: 'configure_spline_mesh_axis', actorName: SPLINE_MESH_ACTOR, componentName: SPLINE_MESH_COMPONENT, forwardAxis: 'Y' }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_mesh_material', toolName: 'build_environment', arguments: { action: 'set_spline_mesh_material', actorName: SPLINE_MESH_ACTOR, componentName: SPLINE_MESH_COMPONENT, materialPath: TEST_MATERIAL_OBJECT, materialIndex: 0 }, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: scatter_meshes_along_spline', toolName: 'build_environment', arguments: { action: 'scatter_meshes_along_spline', actorName: SPLINE_ACTOR, meshPath: TEST_MESH_OBJECT, spacing: 100, alignToSpline: true }, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_mesh_spacing', toolName: 'build_environment', arguments: { action: 'configure_mesh_spacing', spacing: 125, useRandomOffset: true, randomOffsetRange: 10 }, expected: 'success' },
    { scenario: 'CONFIG: configure_mesh_randomization', toolName: 'build_environment', arguments: { action: 'configure_mesh_randomization', randomizeScale: true, minScale: 0.9, maxScale: 1.1, randomizeRotation: true, rotationRange: 45 }, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_road_spline', toolName: 'build_environment', arguments: { action: 'create_road_spline', actorName: ROAD_SPLINE, location: { x: 0, y: 500, z: 0 } }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_river_spline', toolName: 'build_environment', arguments: { action: 'create_river_spline', actorName: RIVER_SPLINE, location: { x: 0, y: 650, z: 0 } }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_fence_spline', toolName: 'build_environment', arguments: { action: 'create_fence_spline', actorName: FENCE_SPLINE, location: { x: 0, y: 800, z: 0 } }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_wall_spline', toolName: 'build_environment', arguments: { action: 'create_wall_spline', actorName: WALL_SPLINE, location: { x: 0, y: 950, z: 0 } }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_cable_spline', toolName: 'build_environment', arguments: { action: 'create_cable_spline', actorName: CABLE_SPLINE, location: { x: 0, y: 1100, z: 0 } }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_pipe_spline', toolName: 'build_environment', arguments: { action: 'create_pipe_spline', actorName: PIPE_SPLINE, location: { x: 0, y: 1250, z: 0 } }, expected: 'success|already exists' },
    // === INFO ===
    { scenario: 'INFO: get_splines_info', toolName: 'build_environment', arguments: { action: 'get_splines_info', actorName: SPLINE_ACTOR }, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: `TestActor_${ts}` }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: SPLINE_ACTOR }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete spline mesh actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: SPLINE_MESH_ACTOR }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete road spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: ROAD_SPLINE }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete river spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: RIVER_SPLINE }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete fence spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: FENCE_SPLINE }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete wall spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: WALL_SPLINE }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete cable spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: CABLE_SPLINE }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete pipe spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: PIPE_SPLINE }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete spline mesh blueprint', toolName: 'manage_asset', arguments: { action: 'delete', path: SPLINE_MESH_BP_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete spline test mesh', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_MESH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete spline test material', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_MATERIAL, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('build-environment', testCases);
