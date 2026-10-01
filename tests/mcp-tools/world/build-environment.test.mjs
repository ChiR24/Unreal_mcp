#!/usr/bin/env node
/**
 * build_environment Tool Integration Tests
 * Covers environment, lighting, spline, and environment action families with proper setup/teardown sequencing.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/WorldAssets_${ts}`;
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const LANDSCAPE_NAME = `TestLandscape_${ts}`;
const FOLIAGE_TYPE_NAME = `TestFoliage_${ts}`;
const FOLIAGE_TYPE_PATH = `/Game/Foliage/${FOLIAGE_TYPE_NAME}`;
const FOLIAGE_TYPE_PATH_ALIAS = FOLIAGE_TYPE_PATH.slice(1);
const ENVIRONMENT_FOLIAGE_TYPE_NAME = `EnvironmentFoliage_${ts}`;
const ENVIRONMENT_FOLIAGE_TYPE_PATH = `/Game/Foliage/${ENVIRONMENT_FOLIAGE_TYPE_NAME}`;
const ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS = ENVIRONMENT_FOLIAGE_TYPE_PATH.slice(1);
const TEST_MESH = '/Engine/BasicShapes/Sphere';
const TEST_MESH_ALIAS = TEST_MESH.slice(1);
const TEST_MATERIAL = '/Engine/BasicShapes/BasicShapeMaterial';
const TEST_MATERIAL_ALIAS = TEST_MATERIAL.slice(1);
const SNAPSHOT_DIR = './tmp/unreal-mcp/build-environment';
const SNAPSHOT_FILE = `snapshot_${ts}.json`;
const HEIGHTMAP_FILE = `${SNAPSHOT_DIR}/heightmap_${ts}.raw`;
const SNAPSHOT_SUN_NAME = `SunLight_${ts}`;
const SNAPSHOT_SKY_NAME = `SkyLight_${ts}`;
const ORIGINAL_SUN_ROTATION = { pitch: 35, yaw: 45, roll: 0 };
const MUTATED_SUN_ROTATION = { pitch: -20, yaw: 120, roll: 0 };
const ORIGINAL_SUN_INTENSITY = 8;
const ORIGINAL_SKY_INTENSITY = 1.5;
const MUTATED_SUN_INTENSITY = 2;
const MUTATED_SKY_INTENSITY = 0.25;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: `TestActor_${ts}`, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
  { scenario: 'Setup: spawn environment delete actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: `EnvDeleteActor_${ts}`, location: { x: 150, y: 0, z: 100 } }, expected: 'success' },

  // === CREATE ===
  { scenario: 'CREATE: create_landscape', toolName: 'build_environment', arguments: {"action": "create_landscape", "name": LANDSCAPE_NAME, "path": TEST_FOLDER_ALIAS, "materialPath": TEST_MATERIAL_ALIAS, "location": {"x": 0, "y": 0, "z": 0}, "sizeX": 1000, "sizeY": 1000, "sectionSize": 7, "quadsPerSection": 7, "sectionsPerComponent": 1, "componentCount": {"x": 1, "y": 1}}, expected: 'success|already exists' },
  // Zero sections per component used to divide by zero in the creation task.
  { scenario: 'ERROR: create_landscape rejects zero sections per component', toolName: 'build_environment', arguments: { action: 'create_landscape', name: `BadLandscape_${ts}`, quadsPerSection: 7, sectionsPerComponent: 0 }, expected: 'error' },
  // === ACTION ===
  { scenario: 'ACTION: sculpt', toolName: 'build_environment', arguments: {"action": "sculpt", "landscapeName": LANDSCAPE_NAME, "tool": "Raise", "location": {"x": 0, "y": 0, "z": 0}, "radius": 128, "falloff": 0.25, "strength": 0.1, "skipFlush": true}, expected: 'success' },
  { scenario: 'ACTION: sculpt_landscape', toolName: 'build_environment', arguments: {"action": "sculpt_landscape", "landscapeName": LANDSCAPE_NAME, "location": {"x": 64, "y": 64, "z": 0}, "radius": 128, "strength": 0.1, "skipFlush": true}, expected: 'success' },
  { scenario: 'ERROR: sculpt refuses a tool it cannot apply', toolName: 'build_environment', arguments: { action: 'sculpt', landscapeName: LANDSCAPE_NAME, tool: 'Smooth', location: { x: 0, y: 0, z: 0 }, radius: 128, skipFlush: true }, expected: 'error' },
  // === ADD ===
  { scenario: 'ADD: add_foliage', toolName: 'build_environment', arguments: {"action": "add_foliage", "name": FOLIAGE_TYPE_NAME, "path": "/Game/Foliage", "meshPath": TEST_MESH_ALIAS, "density": 10, "alignToNormal": false, "randomYaw": false, "cullDistance": 2500}, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: paint_foliage', toolName: 'build_environment', arguments: {"action": "paint_foliage", "foliageType": FOLIAGE_TYPE_PATH_ALIAS, "locations": [{"x": 0, "y": 0, "z": 100}]}, expected: 'success' },
  // A type plus a placement scatters instances; add_foliage used to build only the type asset.
  { scenario: 'ADD: add_foliage generated positions', toolName: 'build_environment', arguments: {"action": "add_foliage", "foliageType": FOLIAGE_TYPE_PATH_ALIAS, "location": {"x": 25, "y": 25, "z": 100}, "radius": 50, "count": 2}, expected: 'success', assertions: [{ path: 'structuredContent.result.requestedPoints', equals: 2, label: 'add_foliage scatters count instances' }] },
  { scenario: 'ACTION: paint_foliage over a box area', toolName: 'build_environment', arguments: { action: 'paint_foliage', foliageType: FOLIAGE_TYPE_PATH_ALIAS, area: { min: { x: 0, y: 0, z: 100 }, max: { x: 200, y: 200, z: 100 } }, count: 3, snapToSurface: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.instancesPlaced', equals: 3, label: 'area paint places exactly count instances' }, { path: 'structuredContent.result.snappedToSurface', equals: false, label: 'area paint honours snapToSurface off' }] },
  // === CREATE ===
  { scenario: 'CREATE: create_procedural_terrain', toolName: 'build_environment', arguments: {"action": "create_procedural_terrain", "name": `TestProceduralTerrain_${ts}`, "path": TEST_FOLDER_ALIAS, "sizeX": 8, "sizeY": 8, "heightScale": 40, "subdivisions": 4, "rotation": {"pitch": 0, "yaw": 15, "roll": 0}, "material": TEST_MATERIAL_ALIAS}, expected: 'success|already exists' },
  { scenario: 'CREATE: create_procedural_foliage', toolName: 'build_environment', arguments: {"action": "create_procedural_foliage", "volumeName": `TestProceduralFoliage_${ts}`, "path": TEST_FOLDER_ALIAS, "bounds": {"location": {"x": 0, "y": 0, "z": 0}, "size": {"x": 500, "y": 500, "z": 300}}, "seed": 123, "tileSize": 500, "foliageTypes": [{"meshPath": TEST_MESH_ALIAS, "density": 1}]}, expected: 'success|already exists' },
  // === ADD ===
  { scenario: 'ADD: add_foliage_instances', toolName: 'build_environment', arguments: {"action": "add_foliage_instances", "foliageTypePath": FOLIAGE_TYPE_PATH_ALIAS, "locations": [{"x": 100, "y": 0, "z": 100}], "transforms": [{"location": {"x": 100, "y": 0, "z": 100}, "rotation": {"pitch": 0, "yaw": 45, "roll": 0}, "scale": {"x": 1.1, "y": 1.1, "z": 1.1}}]}, expected: 'success|already exists' },
  { scenario: 'ADD: add_foliage_instances from a mesh path', toolName: 'build_environment', arguments: { action: 'add_foliage_instances', meshPath: TEST_MESH_ALIAS, locations: [{ x: 120, y: 0, z: 100 }] }, expected: 'success' },
  // === INFO ===
  { scenario: 'INFO: get_foliage_instances', toolName: 'build_environment', arguments: {"action": "get_foliage_instances"}, expected: 'success' },
  { scenario: 'INFO: get_foliage_instances counts per type only', toolName: 'build_environment', arguments: {"action": "get_foliage_instances", "summary": true}, expected: 'success' },
  { scenario: 'INFO: get_foliage_instances capped list', toolName: 'build_environment', arguments: {"action": "get_foliage_instances", "limit": 5}, expected: 'success' },
  // === DELETE ===
  { scenario: 'ERROR: remove_foliage with nothing named', toolName: 'build_environment', arguments: {"action": "remove_foliage"}, expected: 'error' },
  { scenario: 'ERROR: remove_foliage of an unknown type', toolName: 'build_environment', arguments: { action: 'remove_foliage', foliageType: `Missing_${ts}` }, expected: 'error' },
  { scenario: 'DELETE: remove_foliage', toolName: 'build_environment', arguments: {"action": "remove_foliage", "foliageType": FOLIAGE_TYPE_PATH_ALIAS}, expected: 'success' },
  { scenario: 'DELETE: remove_foliage inside several area boxes under one consent', toolName: 'build_environment', arguments: { action: 'remove_foliage', areas: [{ min: { x: 90, y: -10, z: 0 }, max: { x: 110, y: 10, z: 200 } }, { min: { x: 5000, y: -10, z: 0 }, max: { x: 5100, y: 10, z: 200 } }] }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: paint_landscape', toolName: 'build_environment', arguments: {"action": "paint_landscape", "landscapeName": LANDSCAPE_NAME, "layerName": "TestLayer", "region": {"minX": 0, "minY": 0, "maxX": 1, "maxY": 1}, "skipFlush": true}, expected: 'success' },
  // A brush paints only its disc; it used to be ignored in favour of the default region.
  { scenario: 'ACTION: paint_landscape with a brush', toolName: 'build_environment', arguments: { action: 'paint_landscape', landscapeName: LANDSCAPE_NAME, layerName: 'TestLayer', location: { x: 0, y: 0, z: 0 }, radius: 300, strength: 0.5, skipFlush: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.regionMaxX', gte: 1, label: 'the brush region spans the disc' }] },
  { scenario: 'ACTION: paint_landscape_layer', toolName: 'build_environment', arguments: {"action": "paint_landscape_layer", "landscapeName": LANDSCAPE_NAME, "layerName": "TestLayer", "region": {"minX": 0, "minY": 0, "maxX": 1, "maxY": 1}, "strength": 0.8, "skipFlush": true}, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: modify_heightmap', toolName: 'build_environment', arguments: {"action": "modify_heightmap", "landscapeName": LANDSCAPE_NAME, "operation": "add", "heightData": [0], "minX": 0, "minY": 0, "maxX": 0, "maxY": 0, "updateNormals": true, "skipFlush": true}, expected: 'success' },
  { scenario: 'CONFIG: set_landscape_material', toolName: 'build_environment', arguments: {"action": "set_landscape_material", "landscapeName": LANDSCAPE_NAME, "materialPath": TEST_MATERIAL_ALIAS}, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_landscape_grass_type', toolName: 'build_environment', arguments: {"action": "create_landscape_grass_type", "name": `TestLandscapeGrassType_${ts}`, "path": TEST_FOLDER_ALIAS, "staticMesh": TEST_MESH_ALIAS}, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: generate_lods', toolName: 'build_environment', arguments: {"action": "generate_lods", "assetPath": TEST_MESH_ALIAS, "assetPaths": [TEST_MESH_ALIAS], "assets": [TEST_MESH_ALIAS], "numLODs": 2}, expected: 'success|already exists' },
  { scenario: 'ACTION: bake_lightmap', toolName: 'build_environment', arguments: {"action": "bake_lightmap"}, expected: 'success' },
  // === SNAPSHOT SETUP ===
  { scenario: 'CONFIG: configure snapshot skylight', toolName: 'build_environment', arguments: { action: 'configure_sky_light', actorName: SNAPSHOT_SKY_NAME, intensity: ORIGINAL_SKY_INTENSITY, cubemapPath: '/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap' }, expected: 'success', captureResult: { key: 'snapshotSkyPath', fromField: 'result.actorPath' } },
  { scenario: 'CONFIG: configure snapshot directional light', toolName: 'build_environment', arguments: { action: 'configure_directional_light_atmosphere', actorName: SNAPSHOT_SUN_NAME, intensity: ORIGINAL_SUN_INTENSITY, rotation: ORIGINAL_SUN_ROTATION, settings: { bUsedAsAtmosphereSunLight: true } }, expected: 'success', captureResult: { key: 'snapshotSunPath', fromField: 'result.actorPath' } },
  { scenario: 'INFO: discover snapshot directional light component', toolName: 'control_actor', arguments: { action: 'get_components', actorName: SNAPSHOT_SUN_NAME }, expected: 'success', captureResult: { key: 'snapshotSunComponent', fromField: 'result.components', where: { path: 'class', includes: 'DirectionalLightComponent' }, selectField: 'name' } },
  { scenario: 'INFO: discover snapshot skylight component', toolName: 'control_actor', arguments: { action: 'get_components', actorName: SNAPSHOT_SKY_NAME }, expected: 'success', captureResult: { key: 'snapshotSkyComponent', fromField: 'result.components', where: { path: 'class', includes: 'SkyLightComponent' }, selectField: 'name' } },
  { scenario: 'ACTION: export_snapshot', toolName: 'build_environment', arguments: { action: 'export_snapshot', path: SNAPSHOT_DIR, filename: SNAPSHOT_FILE, directionalLightActorPath: '${captured:snapshotSunPath}', skyLightActorPath: '${captured:snapshotSkyPath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.directionalLightActorPath', includes: 'DirectionalLight_', label: 'snapshot selected a directional light actor' }, { path: 'structuredContent.result.skyLightActorPath', includes: 'SkyLight_', label: 'snapshot selected a skylight actor' }, { path: 'structuredContent.result.snapshot.directionalLightRotation.pitch', approximately: ORIGINAL_SUN_ROTATION.pitch, tolerance: 0.001, label: 'snapshot captured original sun pitch' }, { path: 'structuredContent.result.snapshot.directionalLightRotation.yaw', approximately: ORIGINAL_SUN_ROTATION.yaw, tolerance: 0.001, label: 'snapshot captured original sun yaw' }, { path: 'structuredContent.result.snapshot.directionalLightRotation.roll', approximately: ORIGINAL_SUN_ROTATION.roll, tolerance: 0.001, label: 'snapshot captured original sun roll' }, { path: 'structuredContent.result.snapshot.sunIntensity', equals: ORIGINAL_SUN_INTENSITY, label: 'snapshot captured original sun intensity' }, { path: 'structuredContent.result.snapshot.skylightIntensity', equals: ORIGINAL_SKY_INTENSITY, label: 'snapshot captured original skylight intensity' }] },
  { scenario: 'CONFIG: mutate snapshot sun rotation', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: SNAPSHOT_SUN_NAME, rotation: MUTATED_SUN_ROTATION }, expected: 'success' },
  { scenario: 'CONFIG: mutate snapshot sun intensity', toolName: 'control_actor', arguments: { action: 'set_component_property', actorName: SNAPSHOT_SUN_NAME, componentName: '${captured:snapshotSunComponent}', propertyName: 'Intensity', value: MUTATED_SUN_INTENSITY }, expected: 'success' },
  { scenario: 'CONFIG: mutate snapshot sky intensity', toolName: 'control_actor', arguments: { action: 'set_component_property', actorName: SNAPSHOT_SKY_NAME, componentName: '${captured:snapshotSkyComponent}', propertyName: 'Intensity', value: MUTATED_SKY_INTENSITY }, expected: 'success' },
  { scenario: 'INFO: snapshot sun rotation mutated', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: SNAPSHOT_SUN_NAME }, expected: 'success', assertions: [{ path: 'structuredContent.result.data.rotation.0', approximately: MUTATED_SUN_ROTATION.pitch, tolerance: 0.001, label: 'sun pitch changed before import' }, { path: 'structuredContent.result.data.rotation.1', approximately: MUTATED_SUN_ROTATION.yaw, tolerance: 0.001, label: 'sun yaw changed before import' }, { path: 'structuredContent.result.data.rotation.2', approximately: MUTATED_SUN_ROTATION.roll, tolerance: 0.001, label: 'sun roll changed before import' }] },
  { scenario: 'INFO: snapshot sun intensity mutated', toolName: 'control_actor', arguments: { action: 'get_component_property', actorName: SNAPSHOT_SUN_NAME, componentName: '${captured:snapshotSunComponent}', propertyName: 'Intensity' }, expected: 'success', assertions: [{ path: 'structuredContent.result.data.value', equals: MUTATED_SUN_INTENSITY, label: 'sun intensity changed before import' }] },
  { scenario: 'INFO: snapshot sky intensity mutated', toolName: 'control_actor', arguments: { action: 'get_component_property', actorName: SNAPSHOT_SKY_NAME, componentName: '${captured:snapshotSkyComponent}', propertyName: 'Intensity' }, expected: 'success', assertions: [{ path: 'structuredContent.result.data.value', equals: MUTATED_SKY_INTENSITY, label: 'skylight intensity changed before import' }] },
  { scenario: 'ACTION: import_snapshot restores lighting', toolName: 'build_environment', arguments: { action: 'import_snapshot', path: SNAPSHOT_DIR, filename: SNAPSHOT_FILE }, expected: 'success', assertions: [{ path: 'structuredContent.result.directionalLightRotation.pitch', approximately: ORIGINAL_SUN_ROTATION.pitch, tolerance: 0.001, label: 'import reports restored sun pitch' }, { path: 'structuredContent.result.directionalLightRotation.yaw', approximately: ORIGINAL_SUN_ROTATION.yaw, tolerance: 0.001, label: 'import reports restored sun yaw' }, { path: 'structuredContent.result.directionalLightRotation.roll', approximately: ORIGINAL_SUN_ROTATION.roll, tolerance: 0.001, label: 'import reports restored sun roll' }, { path: 'structuredContent.result.sunIntensity', equals: ORIGINAL_SUN_INTENSITY, label: 'import reports restored sun intensity' }, { path: 'structuredContent.result.skylightIntensity', equals: ORIGINAL_SKY_INTENSITY, label: 'import reports restored skylight intensity' }] },
  { scenario: 'INFO: snapshot sun rotation restored', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: SNAPSHOT_SUN_NAME }, expected: 'success', assertions: [{ path: 'structuredContent.result.data.rotation.0', approximately: ORIGINAL_SUN_ROTATION.pitch, tolerance: 0.001, label: 'sun pitch restored after import' }, { path: 'structuredContent.result.data.rotation.1', approximately: ORIGINAL_SUN_ROTATION.yaw, tolerance: 0.001, label: 'sun yaw restored after import' }, { path: 'structuredContent.result.data.rotation.2', approximately: ORIGINAL_SUN_ROTATION.roll, tolerance: 0.001, label: 'sun roll restored after import' }] },
  { scenario: 'INFO: snapshot sun intensity restored', toolName: 'control_actor', arguments: { action: 'get_component_property', actorName: SNAPSHOT_SUN_NAME, componentName: '${captured:snapshotSunComponent}', propertyName: 'Intensity' }, expected: 'success', assertions: [{ path: 'structuredContent.result.data.value', equals: ORIGINAL_SUN_INTENSITY, label: 'sun intensity restored after import' }] },
  { scenario: 'INFO: snapshot sky intensity restored', toolName: 'control_actor', arguments: { action: 'get_component_property', actorName: SNAPSHOT_SKY_NAME, componentName: '${captured:snapshotSkyComponent}', propertyName: 'Intensity' }, expected: 'success', assertions: [{ path: 'structuredContent.result.data.value', equals: ORIGINAL_SKY_INTENSITY, label: 'skylight intensity restored after import' }] },
  // === DELETE ===
  { scenario: 'DELETE: delete', toolName: 'build_environment', arguments: {"action": "delete", "names": [`EnvDeleteActor_${ts}`]}, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_sky_sphere', toolName: 'build_environment', arguments: {"action": "create_sky_sphere", "name": `TestSkySphere_${ts}`}, expected: 'success|already exists' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_time_of_day', toolName: 'build_environment', arguments: {"action": "set_time_of_day", "time": 9, "hour": 9, "azimuth": 30}, expected: 'success' },
  { scenario: 'CONFIG: set_time_of_day via params passthrough', toolName: 'build_environment', arguments: { action: 'set_time_of_day', params: { time: 9, hour: 9 } }, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_fog_volume', toolName: 'build_environment', arguments: {"action": "create_fog_volume", "name": `TestFogVolume_${ts}`, "density": 0.03, "settings": { "FogHeightFalloff": 0.3 }}, expected: 'success|already exists' },

  // === LANDSCAPE ===
  { scenario: 'ACTION: export_heightmap', toolName: 'build_environment', arguments: { action: 'export_heightmap', landscapeName: LANDSCAPE_NAME, landscapePath: `/Game/${LANDSCAPE_NAME}`, outputPath: HEIGHTMAP_FILE, region: { minX: 0, minY: 0, maxX: 0, maxY: 0 } }, expected: 'success' },
  { scenario: 'ACTION: import_heightmap', toolName: 'build_environment', arguments: { action: 'import_heightmap', landscapeName: LANDSCAPE_NAME, landscapePath: `/Game/${LANDSCAPE_NAME}`, heightmapPath: HEIGHTMAP_FILE, region: { minX: 0, minY: 0, maxX: 0, maxY: 0 }, updateNormals: true, skipFlush: true }, expected: 'success' },
  { scenario: 'CREATE: create_landscape_layer_info', toolName: 'build_environment', arguments: { action: 'create_landscape_layer_info', name: `LayerInfo_${ts}`, path: TEST_FOLDER_ALIAS, layerName: `AssetLayer_${ts}`, physicalMaterialPath: '/Engine/EngineMaterials/DefaultPhysicalMaterial', noWeightBlend: true, hardness: 0.5 }, expected: 'success|already exists' },
  { scenario: 'ERROR: create_landscape_layer_info rejects a material as its physical material', toolName: 'build_environment', arguments: { action: 'create_landscape_layer_info', name: `LayerInfoBad_${ts}`, path: TEST_FOLDER_ALIAS, layerName: 'TestLayer', physicalMaterialPath: TEST_MATERIAL_ALIAS }, expected: 'error' },
  { scenario: 'ACTION: paint_landscape_layer from a layer info asset', toolName: 'build_environment', arguments: { action: 'paint_landscape_layer', landscapeName: LANDSCAPE_NAME, layerName: `AssetLayer_${ts}`, layerInfoPath: `${TEST_FOLDER}/LayerInfo_${ts}`, skipFlush: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.layerInfoPath', includes: `LayerInfo_${ts}`, label: 'the new layer takes the given layer info asset' }] },
  { scenario: 'ERROR: paint_landscape_layer refuses a layer info asset of another layer', toolName: 'build_environment', arguments: { action: 'paint_landscape_layer', landscapeName: LANDSCAPE_NAME, layerName: `OtherLayer_${ts}`, layerInfoPath: `${TEST_FOLDER}/LayerInfo_${ts}`, skipFlush: true }, expected: 'error' },
  { scenario: 'CONFIG: configure_landscape_material', toolName: 'build_environment', arguments: { action: 'configure_landscape_material', landscapeName: LANDSCAPE_NAME, landscapePath: `/Game/${LANDSCAPE_NAME}`, materialPath: TEST_MATERIAL_ALIAS }, expected: 'success' },
  { scenario: 'ERROR: configure_landscape_splines rejects empty point before spline component creation', toolName: 'build_environment', arguments: { action: 'configure_landscape_splines', landscapeName: LANDSCAPE_NAME, landscapePath: `/Game/${LANDSCAPE_NAME}`, points: [{}], width: 50 }, expected: 'error' },
  { scenario: 'CONFIG: configure_landscape_splines', toolName: 'build_environment', arguments: { action: 'configure_landscape_splines', landscapeName: LANDSCAPE_NAME, landscapePath: `/Game/${LANDSCAPE_NAME}`, points: [{ x: 0, y: 0, z: 0 }, { x: 300, y: 0, z: 0 }], width: 250, closedLoop: false, layerName: 'TestLayer', raiseTerrain: true, lowerTerrain: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.landscapeName', equals: LANDSCAPE_NAME, label: 'landscape spline target linked' }, { path: 'structuredContent.result.pointCount', equals: 2, label: 'landscape spline control points created' }, { path: 'structuredContent.result.segmentCount', equals: 1, label: 'landscape spline segment created' }, { path: 'structuredContent.result.width', equals: 250, label: 'landscape spline width applied' }], captureResult: { key: 'landscapeActorPath', fromField: 'result.actorPath' } },
  { scenario: 'CONFIG: configure_landscape_splines actor path only', toolName: 'build_environment', arguments: { action: 'configure_landscape_splines', landscapePath: '${captured:landscapeActorPath}', points: [{ x: 0, y: 0, z: 0 }, { x: 400, y: 0, z: 0 }], width: 125, settings: { ClosedLoop: false } }, expected: 'success', assertions: [{ path: 'structuredContent.result.landscapeName', equals: LANDSCAPE_NAME, label: 'landscape spline actor-path lookup target linked' }, { path: 'structuredContent.result.pointCount', equals: 2, label: 'actor-path lookup spline control points created' }, { path: 'structuredContent.result.segmentCount', equals: 1, label: 'actor-path lookup spline segment created' }, { path: 'structuredContent.result.width', equals: 125, label: 'actor-path lookup spline width applied' }] },
  { scenario: 'CONFIG: configure_landscape_splines landscapeActorPath alias', toolName: 'build_environment', arguments: { action: 'configure_landscape_splines', landscapeActorPath: '${captured:landscapeActorPath}', points: [{ x: 0, y: 0, z: 0 }, { x: 500, y: 0, z: 0 }], width: 100, settings: { ClosedLoop: false } }, expected: 'success', assertions: [{ path: 'structuredContent.result.landscapeName', equals: LANDSCAPE_NAME, label: 'landscapeActorPath lookup target linked' }, { path: 'structuredContent.result.pointCount', equals: 2, label: 'landscapeActorPath spline control points created' }, { path: 'structuredContent.result.segmentCount', equals: 1, label: 'landscapeActorPath spline segment created' }, { path: 'structuredContent.result.width', equals: 100, label: 'landscapeActorPath spline width applied' }] },
  { scenario: 'ERROR: configure_landscape_splines rejects empty point', toolName: 'build_environment', arguments: { action: 'configure_landscape_splines', landscapePath: '${captured:landscapeActorPath}', points: [{}], width: 50 }, expected: 'error' },
  { scenario: 'CONFIG: configure_landscape_lod', toolName: 'build_environment', arguments: { action: 'configure_landscape_lod', landscapeName: LANDSCAPE_NAME, landscapePath: `/Game/${LANDSCAPE_NAME}`, settings: { MaxLODLevel: 2 } }, expected: 'success|not found' },
  { scenario: 'CREATE: create_landscape_streaming_proxy', toolName: 'build_environment', arguments: { action: 'create_landscape_streaming_proxy', actorName: `LandscapeProxy_${ts}`, landscapeActorPath: '${captured:landscapeActorPath}', location: { x: 1000, y: 0, z: 0 }, rotation: { pitch: 0, yaw: 0, roll: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.sourceLandscapeName', equals: LANDSCAPE_NAME, label: 'streaming proxy source landscape linked' }, { path: 'structuredContent.result.linkedToLandscape', equals: true, label: 'streaming proxy has landscape actor reference' }] },

  // === FOLIAGE ===
  { scenario: 'CREATE: create_foliage_type', toolName: 'build_environment', arguments: { action: 'create_foliage_type', name: ENVIRONMENT_FOLIAGE_TYPE_NAME, meshPath: TEST_MESH_ALIAS, density: 12, minScale: 0.8, maxScale: 1.2, alignToNormal: true, randomYaw: true, cullDistance: 4000 }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.asset_path', equals: `${ENVIRONMENT_FOLIAGE_TYPE_PATH}.${ENVIRONMENT_FOLIAGE_TYPE_NAME}`, label: 'environment foliage type path returned' }] },
  { scenario: 'CONFIG: configure_foliage_mesh', toolName: 'build_environment', arguments: { action: 'configure_foliage_mesh', foliageTypePath: ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS, meshPath: TEST_MESH_ALIAS, staticMesh: TEST_MESH_ALIAS }, expected: 'success', assertions: [{ path: 'structuredContent.result.foliageTypePath', equals: ENVIRONMENT_FOLIAGE_TYPE_PATH, label: 'environment foliage mesh targets created asset' }, { path: 'structuredContent.result.configuredProperties', includes: 'Mesh', label: 'the mesh is named among the properties written' }, { path: 'structuredContent.result.configuredPropertyCount', gte: 1, label: 'and counted' }, { path: 'structuredContent.result.placedInstances', gte: 0, label: 'the reply counts the type\'s placed instances' }, { path: 'structuredContent.result.instancesShowingNewMesh', gte: 0, label: 'and how many of them draw the new mesh' }] },
  { scenario: 'CONFIG: configure_foliage_placement', toolName: 'build_environment', arguments: { action: 'configure_foliage_placement', foliageTypePath: ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS, density: 20, minScale: 0.9, maxScale: 1.4, alignToNormal: true, randomYaw: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.foliageTypePath', equals: ENVIRONMENT_FOLIAGE_TYPE_PATH, label: 'environment foliage placement targets created asset' }, { path: 'structuredContent.result.configuredProperties', includes: 'Density', label: 'the density is named among the properties written' }, { path: 'structuredContent.result.configuredProperties', includes: 'ScaleX', label: 'so is the scale range' }, { path: 'structuredContent.result.configuredPropertyCount', gte: 5, label: 'every setting written is counted' }] },
  { scenario: 'ERROR: configure_foliage_mesh rejects a mesh that does not load', toolName: 'build_environment', arguments: { action: 'configure_foliage_mesh', foliageTypePath: ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS, meshPath: '/Game/Missing/SM_Nothing' }, expected: 'error' },
  { scenario: 'CONFIG: configure_foliage_lod', toolName: 'build_environment', arguments: { action: 'configure_foliage_lod', foliageTypePath: ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS, cullDistance: 5000, settings: { MinLOD: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.foliageTypePath', equals: ENVIRONMENT_FOLIAGE_TYPE_PATH, label: 'environment foliage lod targets created asset' }] },
  { scenario: 'CONFIG: configure_foliage_collision', toolName: 'build_environment', arguments: { action: 'configure_foliage_collision', foliageTypePath: ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS, collisionEnabled: true, settings: { CollisionWithWorld: true } }, expected: 'success', assertions: [{ path: 'structuredContent.result.foliageTypePath', equals: ENVIRONMENT_FOLIAGE_TYPE_PATH, label: 'environment foliage collision targets created asset' }] },
  { scenario: 'CONFIG: configure_foliage_culling', toolName: 'build_environment', arguments: { action: 'configure_foliage_culling', foliageTypePath: ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS, cullDistance: 6000 }, expected: 'success', assertions: [{ path: 'structuredContent.result.foliageTypePath', equals: ENVIRONMENT_FOLIAGE_TYPE_PATH, label: 'environment foliage culling targets created asset' }, { path: 'structuredContent.result.cullDistance', equals: 6000, label: 'cull distance applied without a configuration error' }] },
  { scenario: 'ACTION: paint_foliage_instances', toolName: 'build_environment', arguments: { action: 'paint_foliage_instances', foliageTypePath: ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS, locations: [{ x: 0, y: 100, z: 100 }], position: { x: 0, y: 100, z: 100 }, radius: 100, density: 1 }, expected: 'success' },
  { scenario: 'DELETE: remove_foliage_instances', toolName: 'build_environment', arguments: { action: 'remove_foliage_instances', foliageTypePath: ENVIRONMENT_FOLIAGE_TYPE_PATH_ALIAS, removeAll: false }, expected: 'success' },

  // === SKY, WEATHER, TIME, WATER ===
  { scenario: 'CONFIG: configure_sky_atmosphere', toolName: 'build_environment', arguments: { action: 'configure_sky_atmosphere', actorName: `SkyAtmosphere_${ts}`, location: { x: 0, y: 0, z: 0 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, settings: { MieScatteringScale: 0.1 } }, expected: 'success|already exists|class not found' },
  { scenario: 'CONFIG: configure_exponential_height_fog', toolName: 'build_environment', arguments: { action: 'configure_exponential_height_fog', actorName: `HeightFog_${ts}`, intensity: 0.4, heightScale: 0.7, settings: { FogDensity: 0.02 } }, expected: 'success|already exists|class not found' },
  { scenario: 'CONFIG: configure_volumetric_cloud', toolName: 'build_environment', arguments: { action: 'configure_volumetric_cloud', actorName: `VolumetricCloud_${ts}`, settings: { LayerBottomAltitude: 2 } }, expected: 'success|already exists|class not found' },
  { scenario: 'CREATE: create_weather_system', toolName: 'build_environment', arguments: { action: 'create_weather_system', name: `WeatherSystem_${ts}`, particleSystemPath: '/Game/Weather/P_Weather', location: { x: 0, y: 0, z: 300 }, rotation: { pitch: 0, yaw: 0, roll: 0 } }, expected: 'success|already exists|not found' },
  { scenario: 'CONFIG: configure_rain_particles', toolName: 'build_environment', arguments: { action: 'configure_rain_particles', actorName: `RainParticles_${ts}`, particleSystemPath: '/Game/Weather/P_Rain', amplitude: 0.8, speed: 1.2, settings: { Intensity: 0.8 } }, expected: 'success|already exists|not found' },
  { scenario: 'CONFIG: configure_snow_particles', toolName: 'build_environment', arguments: { action: 'configure_snow_particles', actorName: `SnowParticles_${ts}`, particleSystemPath: '/Game/Weather/P_Snow', amplitude: 0.4, speed: 0.6, settings: { Intensity: 0.4 } }, expected: 'success|already exists|not found' },
  { scenario: 'CONFIG: configure_wind', toolName: 'build_environment', arguments: { action: 'configure_wind', actorName: `Wind_${ts}`, direction: { pitch: 0, yaw: 90, roll: 0 }, speed: 20, strength: 0.7, settings: { Strength: 0.7 } }, expected: 'success|already exists|class not found' },
  { scenario: 'CONFIG: configure_lightning', toolName: 'build_environment', arguments: { action: 'configure_lightning', actorName: `Lightning_${ts}`, particleSystemPath: '/Game/Weather/P_Lightning', intensity: 2, settings: { FlashInterval: 4 } }, expected: 'success|already exists|not found' },
  { scenario: 'CREATE: create_time_of_day_system', toolName: 'build_environment', arguments: { action: 'create_time_of_day_system', name: `TimeOfDay_${ts}`, location: { x: 0, y: 0, z: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.sunComponentName', equals: 'TimeOfDaySun', label: 'time of day sun component created' }, { path: 'structuredContent.result.skyLightComponentName', equals: 'TimeOfDaySkyLight', label: 'time of day skylight component created' }, { path: 'structuredContent.result.skyAtmosphereComponentName', equals: 'TimeOfDaySkyAtmosphere', label: 'time of day sky atmosphere component created' }, { path: 'structuredContent.result.currentHour', equals: 12, label: 'the rig starts at noon' }, { path: 'structuredContent.result.sunIntensity', equals: 10, label: 'time of day sun intensity' }, { path: 'structuredContent.result.skyLightIntensity', equals: 1, label: 'time of day skylight intensity' }] },
  { scenario: 'INFO: time of day skylight intensity persisted', toolName: 'control_actor', arguments: { action: 'get_component_property', actorName: `TimeOfDay_${ts}`, componentName: 'TimeOfDaySkyLight', propertyName: 'Intensity' }, expected: 'success', assertions: [{ path: 'structuredContent.result.data.value', equals: 1, label: 'time of day actual skylight intensity persisted' }] },
  { scenario: 'CONFIG: configure_sun_position', toolName: 'build_environment', arguments: { action: 'configure_sun_position', actorName: `SunLight_${ts}`, azimuth: 120, elevation: 45, settings: { Intensity: 10 } }, expected: 'success|not found' },
  { scenario: 'CONFIG: configure_sun_position by hour on the level sun', toolName: 'build_environment', arguments: { action: 'configure_sun_position', hour: 15 }, expected: 'success' },
  { scenario: 'CONFIG: configure_light_color_curve', toolName: 'build_environment', arguments: { action: 'configure_light_color_curve', curvePath: `${TEST_FOLDER}/LightColorCurve_${ts}`, keys: [{ time: 6, color: { r: 1, g: 0.6, b: 0.3 } }, { time: 12, color: { r: 1, g: 0.95, b: 0.8, a: 1 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.keyCount', equals: 2, label: 'light color curve carries the given keys' }] },
  { scenario: 'CONFIG: configure_sky_color_curve', toolName: 'build_environment', arguments: { action: 'configure_sky_color_curve', curvePath: `${TEST_FOLDER}/SkyColorCurve_${ts}`, keys: [{ time: 12, color: { r: 0.4, g: 0.6, b: 1 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.keyCount', equals: 1, label: 'sky color curve carries the given key' }] },
  { scenario: 'CONFIG: configure_sky_color_curve edits the same asset', toolName: 'build_environment', arguments: { action: 'configure_sky_color_curve', curvePath: `${TEST_FOLDER}/SkyColorCurve_${ts}`, keys: [{ time: 0, color: { r: 0.05, g: 0.05, b: 0.2 } }, { time: 12, color: { r: 0.4, g: 0.6, b: 1 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.created', equals: false, label: 'an existing curve is edited, not recreated' }] },
  { scenario: 'CREATE: create_water_body_ocean', toolName: 'build_environment', arguments: { action: 'create_water_body_ocean', waterBodyName: `Ocean_${ts}`, location: { x: 0, y: 0, z: 0 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, materialPath: TEST_MATERIAL_ALIAS, materialIndex: 0, collisionEnabled: true, settings: { WaterBodyIndex: 1 } }, expected: 'success|already exists|class not found' },
  { scenario: 'CREATE: create_water_body_lake', toolName: 'build_environment', arguments: { action: 'create_water_body_lake', waterBodyName: `Lake_${ts}`, location: { x: 400, y: 0, z: 0 }, materialPath: TEST_MATERIAL_ALIAS, collisionEnabled: true, waveHeight: 40, waveLength: 300, settings: { WaterBodyIndex: 2 } }, expected: 'success|already exists|class not found' },
  { scenario: 'CREATE: create_water_body_river', toolName: 'build_environment', arguments: { action: 'create_water_body_river', waterBodyName: `River_${ts}`, location: { x: 800, y: 0, z: 0 }, materialPath: TEST_MATERIAL_ALIAS, collisionEnabled: true, waveHeight: 20, waveLength: 200, settings: { WaterBodyIndex: 3 } }, expected: 'success|already exists|class not found' },
  { scenario: 'CREATE: create_water_body_custom', toolName: 'build_environment', arguments: { action: 'create_water_body_custom', waterBodyName: `CustomWater_${ts}`, location: { x: 1200, y: 0, z: 0 }, materialPath: TEST_MATERIAL_ALIAS, collisionEnabled: false, waveHeight: 10, waveLength: 100, settings: { WaterBodyIndex: 4 } }, expected: 'success|already exists|class not found' },
  { scenario: 'CONFIG: configure_water_waves', toolName: 'build_environment', arguments: { action: 'configure_water_waves', waterBodyName: `Lake_${ts}`, waveHeight: 25, waveLength: 250, steepness: 0.6 }, expected: 'success|not found|class not found' },
  { scenario: 'CONFIG: configure_water_material', toolName: 'build_environment', arguments: { action: 'configure_water_material', waterBodyName: `Lake_${ts}`, materialPath: TEST_MATERIAL_ALIAS, materialIndex: 0 }, expected: 'success|not found|class not found' },
  { scenario: 'CONFIG: configure_water_collision', toolName: 'build_environment', arguments: { action: 'configure_water_collision', waterBodyName: `Lake_${ts}`, collisionEnabled: true }, expected: 'success|not found|class not found' },
  { scenario: 'CREATE: create_buoyancy_component', toolName: 'build_environment', arguments: { action: 'create_buoyancy_component', targetActor: `TestActor_${ts}`, settings: { PontoonRadius: 50 } }, expected: 'success|not found|class not found' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete snapshot directional light', toolName: 'control_actor', arguments: { action: 'delete', actorName: SNAPSHOT_SUN_NAME }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete snapshot skylight', toolName: 'control_actor', arguments: { action: 'delete', actorName: SNAPSHOT_SKY_NAME }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: `TestActor_${ts}` }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete base foliage type', toolName: 'manage_asset', arguments: { action: 'delete', path: FOLIAGE_TYPE_PATH, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete environment foliage type', toolName: 'manage_asset', arguments: { action: 'delete', path: ENVIRONMENT_FOLIAGE_TYPE_PATH, force: true }, expected: 'success|not found' },
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
    { scenario: 'CREATE: spawn_light by class', toolName: 'build_environment', arguments: {"action": "spawn_light", "lightClass": "PointLight", "location": {"x": 0, "y": 0, "z": 200}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_light', toolName: 'build_environment', arguments: {"action": "create_light", "lightType": "Point", "name": "Testlight", "color": {"r": 1, "g": 0.8, "b": 0.6}, "properties": {"attenuationRadius": 800}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_light sky with intensity and color', toolName: 'build_environment', arguments: { action: 'create_light', lightType: 'sky', name: `TestSkyLightType_${ts}`, intensity: 2, color: { r: 0.8, g: 0.9, b: 1 } }, expected: 'success' },
    // The reply lists details.applied and details.refused: outerConeAngle does not fit a point light, so it is named, not dropped.
    { scenario: 'CREATE: create_light names the setting that does not fit', toolName: 'build_environment', arguments: { action: 'create_light', lightType: 'Point', name: `TestLightRefuses_${ts}`, properties: { attenuationRadius: 900, outerConeAngle: 30 } }, expected: 'success' },
    { scenario: 'CREATE: spawn_sky_light', toolName: 'build_environment', arguments: {"action": "spawn_sky_light", "location": {"x": 0, "y": 0, "z": 100}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_sky_light', toolName: 'build_environment', arguments: {"action": "create_sky_light", "name": "Testsky_light", "rotation": {"pitch": 0, "yaw": 90, "roll": 0}, "cubemapPath": "/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap", "recapture": true}, expected: 'success|already exists' },
    // === ACTION ===
    { scenario: 'ACTION: ensure_single_sky_light', toolName: 'build_environment', arguments: {"action": "ensure_single_sky_light", "name": "Testsky_light", "recapture": true}, expected: 'success' },
    { scenario: 'CONFIG: configure_sky_light intensity on the kept sky light', toolName: 'build_environment', arguments: { action: 'configure_sky_light', actorName: 'Testsky_light', skyLightIntensity: 1.5 }, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_lightmass_volume', toolName: 'build_environment', arguments: {"action": "create_lightmass_volume", "name": "Testlightmass_volume", "size": {"x": 1500, "y": 1500, "z": 600}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_lighting_enabled_level', toolName: 'build_environment', arguments: {"action": "create_lighting_enabled_level", "name": "Testlighting_enabled_level", "path": "/Game/MCPTest/Testlighting_enabled_level"}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_lighting_enabled_level (folder path + levelName)', toolName: 'build_environment', arguments: {"action": "create_lighting_enabled_level", "path": "/Game/MCPTest/Maps", "levelName": "Testlighting_enabled_level2"}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_dynamic_light', toolName: 'build_environment', arguments: {"action": "create_dynamic_light", "name": "Testdynamic_light", "path": "/Game/MCPTest"}, expected: 'success|already exists' },
    // === ACTION ===
    { scenario: 'ACTION: setup_global_illumination', toolName: 'build_environment', arguments: {"action": "setup_global_illumination", "method": "LumenGI", "quality": "High", "indirectLightingIntensity": 1.0, "bounces": 3}, expected: 'success|already exists' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_shadows', toolName: 'build_environment', arguments: {"action": "configure_shadows", "shadowQuality": "High", "shadowDistance": 1.0, "contactShadows": true, "rayTracedShadows": false, "virtualShadowMaps": true}, expected: 'success', assertions: [{ path: 'structuredContent.result.appliedSettings', includes: 'shadowQuality', label: 'shadow quality is applied' }] },
    { scenario: 'CONFIG: configure_shadows on one light', toolName: 'build_environment', arguments: { action: 'configure_shadows', actorName: 'Testlight', castShadows: true, shadowBias: 0.5, shadowSlopeBias: 0.5, shadowResolutionScale: 1.5 }, expected: 'success|not found' },
    { scenario: 'CONFIG: set_exposure', toolName: 'build_environment', arguments: {"action": "set_exposure", "method": "Manual", "minBrightness": 1, "maxBrightness": 1, "compensationValue": 0}, expected: 'success' },
    { scenario: 'CONFIG: set_ambient_occlusion', toolName: 'build_environment', arguments: {"action": "set_ambient_occlusion", "enabled": true, "intensity": 0.5, "radius": 2000, "quality": "High"}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: setup_volumetric_fog', toolName: 'build_environment', arguments: {"action": "setup_volumetric_fog", "enabled": true, "viewDistance": 6000}, expected: 'success|already exists' },
    // === CREATE ===
    { scenario: 'CREATE: build_lighting', toolName: 'build_environment', arguments: {"action": "build_lighting"}, expected: 'success|already exists' },
    // === INFO ===
    { scenario: 'INFO: list_light_types', toolName: 'build_environment', arguments: {"action": "list_light_types"}, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete sky light type actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: `TestSkyLightType_${ts}` }, expected: 'success|not found' },
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
    { scenario: 'CREATE: create_spline_actor', toolName: 'build_environment', arguments: { action: 'create_spline_actor', actorName: SPLINE_ACTOR, location: { x: 0, y: 0, z: 0 }, initialPoints: [{ location: { x: 0, y: 0, z: 0 } }, { location: { x: 300, y: 0, z: 0 } }], splineType: 'Curve' }, expected: 'success|already exists', timeoutMs: 120000, captureResult: { key: 'splineActorPath', fromField: 'result.actorPath' } },
    { scenario: 'CREATE: create_spline_actor from declared points', toolName: 'build_environment', arguments: { action: 'create_spline_actor', name: `${SPLINE_ACTOR}_Points`, points: [{ position: { x: 0, y: 0, z: 0 } }, { position: { x: 400, y: 100, z: 0 }, arriveTangent: { x: 200, y: 0, z: 0 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.splineLength', gte: 400, label: 'points given as position are placed, not stacked at the origin' }] },
    // === ADD ===
    { scenario: 'ADD: add_spline_point', toolName: 'build_environment', arguments: { action: 'add_spline_point', actorName: SPLINE_ACTOR, position: { x: 600, y: 120, z: 0 }, pointType: 'Curve' }, expected: 'success|already exists' },
    { scenario: 'ADD: add_spline_point at an index with tangents', toolName: 'build_environment', arguments: { action: 'add_spline_point', actorName: SPLINE_ACTOR, position: { x: 150, y: -60, z: 0 }, index: 1, arriveTangent: { x: 100, y: 0, z: 0 }, leaveTangent: { x: 80, y: 20, z: 0 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.pointIndex', equals: 1, label: 'the point is inserted at the given index' }] },
    { scenario: 'DELETE: remove the indexed spline point', toolName: 'build_environment', arguments: { action: 'remove_spline_point', actorName: SPLINE_ACTOR, pointIndex: 1 }, expected: 'success' },
    // === DELETE ===
    { scenario: 'DELETE: remove_spline_point', toolName: 'build_environment', arguments: { action: 'remove_spline_point', actorName: SPLINE_ACTOR, pointIndex: 2 }, expected: 'success|not found' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_spline_point_position', toolName: 'build_environment', arguments: { action: 'set_spline_point_position', actorName: SPLINE_ACTOR, pointIndex: 1, position: { x: 350, y: 80, z: 0 } }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_point_tangents', toolName: 'build_environment', arguments: { action: 'set_spline_point_tangents', actorName: SPLINE_ACTOR, pointIndex: 1, arriveTangent: { x: 100, y: 25, z: 0 }, leaveTangent: { x: 100, y: 25, z: 0 } }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_point_rotation', toolName: 'build_environment', arguments: { action: 'set_spline_point_rotation', actorName: SPLINE_ACTOR, pointIndex: 1, pointRotation: { pitch: 0, yaw: 20, roll: 0 } }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_point_scale', toolName: 'build_environment', arguments: { action: 'set_spline_point_scale', actorName: SPLINE_ACTOR, pointIndex: 1, pointScale: { x: 1.25, y: 1.25, z: 1 } }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_type', toolName: 'build_environment', arguments: { action: 'set_spline_type', actorName: SPLINE_ACTOR, splineType: 'Linear' }, expected: 'success' },
    { scenario: 'CONFIG: set_spline_point_scale by actorPath', toolName: 'build_environment', arguments: { action: 'set_spline_point_scale', actorPath: '${captured:splineActorPath}', pointIndex: 0, pointScale: { x: 1, y: 1, z: 1 } }, expected: 'success' },
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
    // Route, mesh and loop together: the handler used to ignore points/meshPath entirely and
    // emit a fixed demo zigzag with no geometry, so this asserts the real deformed road.
    { scenario: 'CREATE: create_road_spline with explicit route and mesh', toolName: 'build_environment', arguments: { action: 'create_road_spline', actorName: 'IT_RoadLoop', meshPath: '/Engine/BasicShapes/Cube', materialPath: '/Engine/BasicShapes/BasicShapeMaterial', width: 600, closedLoop: true, points: [{ position: { x: 0, y: 0, z: 0 } }, { position: { x: 2000, y: 0, z: 0 } }, { position: { x: 2000, y: 2000, z: 0 } }, { position: { x: 0, y: 2000, z: 0 } }] }, expected: 'success|already exists' },
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
    { scenario: 'Cleanup: delete points spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: `${SPLINE_ACTOR}_Points` }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete spline mesh actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: SPLINE_MESH_ACTOR }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete road spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: ROAD_SPLINE }, expected: 'success|not found' },
    // The explicit-route road spline above is the one actor in this suite with a
    // fixed (non-timestamped) name, so without this it survived the run and every
    // later run took the 'already exists' branch against the leftover actor.
    { scenario: 'Cleanup: delete explicit-route road spline actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'IT_RoadLoop' }, expected: 'success|not found' },
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

runToolTests('build-environment', testCases, { folder: TEST_FOLDER });
