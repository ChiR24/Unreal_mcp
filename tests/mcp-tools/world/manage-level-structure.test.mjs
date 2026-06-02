#!/usr/bin/env node
/**
 * manage_level_structure Tool Integration Tests
 * Covers all 17 actions with concrete native payload contracts.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/WorldAssets';
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const ts = Date.now();
const LEVEL_NAME = `StructLevel_${ts}`;
const SUBLEVEL_NAME = `StructSublevel_${ts}`;
const LEVEL_PATH = `${TEST_FOLDER}/${LEVEL_NAME}`;
const SUBLEVEL_PATH = `${TEST_FOLDER}/${SUBLEVEL_NAME}`;
const LEVEL_PATH_ALIAS = LEVEL_PATH.slice(1);
const SUBLEVEL_PATH_ALIAS = SUBLEVEL_PATH.slice(1);
const TEST_ACTOR = `StructActor_${ts}`;
const DATA_LAYER = `StructDataLayer_${ts}`;
const HLOD_LAYER = `StructHLOD_${ts}`;
const HLOD_FOLDER = `${TEST_FOLDER}/HLOD`;
const MINIMAP_VOLUME = `StructMiniMap_${ts}`;
const LEVEL_INSTANCE = `StructLevelInstance_${ts}`;
const PACKED_LEVEL = `StructPackedLevel_${ts}`;
const SEQUENCE_NODE = `StructSequence_${ts}`;
const SEQUENCE_TARGET_NODE = `StructSequenceTarget_${ts}`;
const STREAMING_VOLUME = `StreamingVolume_${SUBLEVEL_PATH}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn actor for data layer assignment', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },

  // === CREATE ===
  { scenario: 'CREATE: create_level', toolName: 'manage_level_structure', arguments: { action: 'create_level', levelName: LEVEL_NAME, levelPath: TEST_FOLDER_ALIAS, bCreateWorldPartition: false, bUseExternalActors: false, save: true }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_sublevel', toolName: 'manage_level_structure', arguments: { action: 'create_sublevel', sublevelName: SUBLEVEL_NAME, sublevelPath: SUBLEVEL_PATH_ALIAS, parentLevel: LEVEL_PATH_ALIAS, save: true }, expected: 'success|already exists' },

  // === LEVEL CONFIG ===
  { scenario: 'CONFIG: configure_level_streaming', toolName: 'manage_level_structure', arguments: { action: 'configure_level_streaming', levelName: SUBLEVEL_PATH_ALIAS, streamingMethod: 'Blueprint', bShouldBeVisible: true, bShouldBlockOnLoad: false, bDisableDistanceStreaming: false }, expected: 'success|not found' },
  { scenario: 'CONFIG: set_streaming_distance', toolName: 'manage_level_structure', arguments: { action: 'set_streaming_distance', levelName: SUBLEVEL_PATH_ALIAS, streamingDistance: 5000, streamingUsage: 'Blueprint', volumeLocation: { x: 0, y: 0, z: 0 }, createVolume: true }, expected: 'success|not found' },
  { scenario: 'CONFIG: configure_level_bounds', toolName: 'manage_level_structure', arguments: { action: 'configure_level_bounds', bAutoCalculateBounds: false, boundsOrigin: { x: 0, y: 0, z: 0 }, boundsExtent: { x: 1000, y: 1000, z: 1000 } }, expected: 'success' },

  // === WORLD PARTITION GUARDED ACTIONS ===
  { scenario: 'TOGGLE: enable_world_partition', toolName: 'manage_level_structure', arguments: { action: 'enable_world_partition', bEnableWorldPartition: true }, expected: 'error|cannot enable|success' },
  { scenario: 'CONFIG: configure_grid_size', toolName: 'manage_level_structure', arguments: { action: 'configure_grid_size', gridName: 'MainGrid', gridCellSize: 12800, loadingRange: 25600, bBlockOnSlowStreaming: false, priority: 0, createIfMissing: true }, expected: 'error|not enabled|success' },
  { scenario: 'CREATE: create_data_layer', toolName: 'manage_level_structure', arguments: { action: 'create_data_layer', dataLayerName: DATA_LAYER, dataLayerType: 'Runtime', bIsInitiallyVisible: true, bIsInitiallyLoaded: true }, expected: 'error|world partition|success|already exists' },
  { scenario: 'CONNECT: assign_actor_to_data_layer', toolName: 'manage_level_structure', arguments: { action: 'assign_actor_to_data_layer', actorName: TEST_ACTOR, dataLayerName: DATA_LAYER }, expected: 'error|world partition|not found|success' },
  { scenario: 'CONFIG: configure_hlod_layer', toolName: 'manage_level_structure', arguments: { action: 'configure_hlod_layer', hlodLayerName: HLOD_LAYER, hlodLayerPath: HLOD_FOLDER, cellSize: 12800, loadingDistance: 25600, layerType: 'Instancing', bIsSpatiallyLoaded: true }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_minimap_volume', toolName: 'manage_level_structure', arguments: { action: 'create_minimap_volume', volumeName: MINIMAP_VOLUME, volumeLocation: { x: 0, y: 0, z: 0 }, volumeExtent: { x: 1000, y: 1000, z: 1000 } }, expected: 'error|world partition|success|already exists' },

  // === LEVEL BLUEPRINT ===
  { scenario: 'ACTION: open_level_blueprint', toolName: 'manage_level_structure', arguments: { action: 'open_level_blueprint', levelName: LEVEL_NAME }, expected: 'success' },
  { scenario: 'ADD: add_level_blueprint_node', toolName: 'manage_level_structure', arguments: { action: 'add_level_blueprint_node', nodeClass: 'K2Node_ExecutionSequence', nodeName: SEQUENCE_NODE, nodePosition: { x: 200, y: 100 } }, expected: 'success|already exists|not found' },
  { scenario: 'ADD: add target level blueprint node', toolName: 'manage_level_structure', arguments: { action: 'add_level_blueprint_node', nodeClass: 'K2Node_ExecutionSequence', nodeName: SEQUENCE_TARGET_NODE, nodePosition: { x: 500, y: 100 } }, expected: 'success|already exists|not found' },
  { scenario: 'CONNECT: connect_level_blueprint_nodes', toolName: 'manage_level_structure', arguments: { action: 'connect_level_blueprint_nodes', sourceNodeName: SEQUENCE_NODE, targetNodeName: SEQUENCE_TARGET_NODE, sourcePinName: 'then_0', targetPinName: 'execute' }, expected: 'success' },

  // === LEVEL INSTANCE / PACKED LEVEL ===
  { scenario: 'CREATE: create_level_instance', toolName: 'manage_level_structure', arguments: { action: 'create_level_instance', levelAssetPath: SUBLEVEL_PATH, levelInstanceName: LEVEL_INSTANCE, instanceLocation: { x: 500, y: 0, z: 0 }, instanceRotation: { pitch: 0, yaw: 0, roll: 0 }, instanceScale: { x: 1, y: 1, z: 1 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_packed_level_actor', toolName: 'manage_level_structure', arguments: { action: 'create_packed_level_actor', levelAssetPath: SUBLEVEL_PATH, packedLevelName: PACKED_LEVEL, instanceLocation: { x: 700, y: 0, z: 0 }, instanceRotation: { pitch: 0, yaw: 0, roll: 0 }, bPackBlueprints: true, bPackStaticMeshes: true }, expected: 'success|already exists' },

  // === INFO ===
  { scenario: 'INFO: get_level_structure_info', toolName: 'manage_level_structure', arguments: { action: 'get_level_structure_info' }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete level instance', toolName: 'control_actor', arguments: { action: 'delete', actorName: LEVEL_INSTANCE }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete packed level actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: PACKED_LEVEL }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete minimap volume', toolName: 'control_actor', arguments: { action: 'delete', actorName: MINIMAP_VOLUME }, expected: 'not found|success' },
  { scenario: 'Cleanup: delete streaming volume', toolName: 'control_actor', arguments: { action: 'delete', actorName: STREAMING_VOLUME }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TEST_ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: unload sublevel', toolName: 'manage_level', arguments: { action: 'unload', levelPath: SUBLEVEL_PATH }, expected: 'success|not loaded|not found' },
  { scenario: 'Cleanup: delete created levels', toolName: 'manage_level', arguments: { action: 'delete', levelPaths: [SUBLEVEL_PATH, LEVEL_PATH] }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete data layer asset', toolName: 'manage_asset', arguments: { action: 'delete', path: `/Game/DataLayers/${DATA_LAYER}`, force: true }, expected: 'not found|success' },
  { scenario: 'Cleanup: delete HLOD asset', toolName: 'manage_asset', arguments: { action: 'delete', path: `${HLOD_FOLDER}/${HLOD_LAYER}`, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

// === VOLUME ACTIONS ===
{
  /**
   * manage_level_structure volume action integration tests
   * Covers all 28 actions with proper setup/teardown sequencing.
   */

  const TEST_FOLDER = '/Game/MCPTest/WorldAssets';
  const ts = Date.now();
  const TEST_ACTOR = `TestActor_${ts}`;
  const TRIGGER_VOLUME = 'Testtrigger_volume';
  const BLOCKING_VOLUME = 'Testblocking_volume';
  const KILL_Z_VOLUME = 'Testkill_z_volume';
  const PHYSICS_VOLUME = 'Testphysics_volume';
  const CULL_DISTANCE_VOLUME = 'Testcull_distance_volume';
  const POST_PROCESS_VOLUME = 'Testpost_process_volume';

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },

    // === CREATE ===
    { scenario: 'CREATE: create_trigger_volume', toolName: 'manage_level_structure', arguments: {"action": "create_trigger_volume", "volumeName": TRIGGER_VOLUME, "location": {"x": 0, "y": 0, "z": 0}, "extent": {"x": 100, "y": 100, "z": 100}}, expected: 'success|already exists' },
    // === ADD ===
    { scenario: 'ADD: add_trigger_volume', toolName: 'manage_level_structure', arguments: {"action": "add_trigger_volume", "actorPath": TEST_ACTOR, "volumeName": `${TRIGGER_VOLUME}_Actor`}, expected: 'success|already exists' },
    // === CREATE ===
    { scenario: 'CREATE: create_trigger_box', toolName: 'manage_level_structure', arguments: {"action": "create_trigger_box", "volumeName": "Testtrigger_box", "boxExtent": {"x": 100, "y": 100, "z": 100}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_trigger_sphere', toolName: 'manage_level_structure', arguments: {"action": "create_trigger_sphere", "volumeName": "Testtrigger_sphere", "sphereRadius": 100, "rotation": {"pitch": 0, "yaw": 45, "roll": 0}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_trigger_capsule', toolName: 'manage_level_structure', arguments: {"action": "create_trigger_capsule", "volumeName": "Testtrigger_capsule", "capsuleRadius": 50, "capsuleHalfHeight": 100}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_blocking_volume', toolName: 'manage_level_structure', arguments: {"action": "create_blocking_volume", "volumeName": BLOCKING_VOLUME, "extent": {"x": 100, "y": 100, "z": 100}}, expected: 'success|already exists' },
    // === ADD ===
    { scenario: 'ADD: add_blocking_volume', toolName: 'manage_level_structure', arguments: {"action": "add_blocking_volume", "actorPath": TEST_ACTOR, "volumeName": `${BLOCKING_VOLUME}_Actor`}, expected: 'success|already exists' },
    // === CREATE ===
    { scenario: 'CREATE: create_kill_z_volume', toolName: 'manage_level_structure', arguments: {"action": "create_kill_z_volume", "volumeName": KILL_Z_VOLUME, "extent": {"x": 100, "y": 100, "z": 100}}, expected: 'success|already exists' },
    // === ADD ===
    { scenario: 'ADD: add_kill_z_volume', toolName: 'manage_level_structure', arguments: {"action": "add_kill_z_volume", "actorPath": TEST_ACTOR, "volumeName": `${KILL_Z_VOLUME}_Actor`}, expected: 'success|already exists' },
    // === CREATE ===
    { scenario: 'CREATE: create_pain_causing_volume', toolName: 'manage_level_structure', arguments: {"action": "create_pain_causing_volume", "volumeName": "Testpain_causing_volume", "bPainCausing": false, "damagePerSec": 10}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_physics_volume', toolName: 'manage_level_structure', arguments: {"action": "create_physics_volume", "volumeName": PHYSICS_VOLUME, "bWaterVolume": false, "fluidFriction": 0.3}, expected: 'success|already exists' },
    // === ADD ===
    { scenario: 'ADD: add_physics_volume', toolName: 'manage_level_structure', arguments: {"action": "add_physics_volume", "actorPath": TEST_ACTOR, "volumeName": `${PHYSICS_VOLUME}_Actor`}, expected: 'success|already exists' },
    // === CREATE ===
    { scenario: 'CREATE: create_audio_volume', toolName: 'manage_level_structure', arguments: {"action": "create_audio_volume", "volumeName": "Testaudio_volume", "bEnabled": true}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_reverb_volume', toolName: 'manage_level_structure', arguments: {"action": "create_reverb_volume", "volumeName": "Testreverb_volume", "reverbVolume": 0.5, "fadeTime": 0.5}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_cull_distance_volume', toolName: 'manage_level_structure', arguments: {"action": "create_cull_distance_volume", "volumeName": CULL_DISTANCE_VOLUME, "cullDistances": [{"size": 100, "cullDistance": 1000}, {"size": 500, "cullDistance": 5000}]}, expected: 'success|already exists' },
    // === ADD ===
    { scenario: 'ADD: add_cull_distance_volume', toolName: 'manage_level_structure', arguments: {"action": "add_cull_distance_volume", "actorPath": TEST_ACTOR, "volumeName": `${CULL_DISTANCE_VOLUME}_Actor`}, expected: 'success|already exists' },
    // === CREATE ===
    { scenario: 'CREATE: create_precomputed_visibility_volume', toolName: 'manage_level_structure', arguments: {"action": "create_precomputed_visibility_volume", "volumeName": "Testprecomputed_visibility_volume"}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_lightmass_importance_volume', toolName: 'manage_level_structure', arguments: {"action": "create_lightmass_importance_volume", "volumeName": "Testlightmass_importance_volume"}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_nav_mesh_bounds_volume', toolName: 'manage_level_structure', arguments: {"action": "create_nav_mesh_bounds_volume", "volumeName": "Testnav_mesh_bounds_volume", "extent": {"x": 500, "y": 500, "z": 200}}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_nav_modifier_volume', toolName: 'manage_level_structure', arguments: {"action": "create_nav_modifier_volume", "volumeName": "Testnav_modifier_volume"}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_camera_blocking_volume', toolName: 'manage_level_structure', arguments: {"action": "create_camera_blocking_volume", "volumeName": "Testcamera_blocking_volume"}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_post_process_volume', toolName: 'manage_level_structure', arguments: {"action": "create_post_process_volume", "volumeName": POST_PROCESS_VOLUME, "priority": 0, "blendRadius": 100, "blendWeight": 1, "bUnbound": true}, expected: 'success|already exists' },
    // === ADD ===
    { scenario: 'ADD: add_post_process_volume', toolName: 'manage_level_structure', arguments: {"action": "add_post_process_volume", "actorPath": TEST_ACTOR, "volumeName": `${POST_PROCESS_VOLUME}_Actor`}, expected: 'success|already exists' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_volume_extent', toolName: 'manage_level_structure', arguments: {"action": "set_volume_extent", "volumeName": TRIGGER_VOLUME, "extent": {"x": 150, "y": 150, "z": 150}}, expected: 'success' },
    { scenario: 'CONFIG: set_volume_bounds', toolName: 'manage_level_structure', arguments: {"action": "set_volume_bounds", "volumeName": BLOCKING_VOLUME, "bounds": [-100, -100, -100, 100, 100, 100]}, expected: 'success' },
    { scenario: 'CONFIG: set_volume_properties', toolName: 'manage_level_structure', arguments: {"action": "set_volume_properties", "volumeName": PHYSICS_VOLUME, "bWaterVolume": false, "fluidFriction": 0.3, "terminalVelocity": 4000}, expected: 'success' },
    // === DELETE ===
    { scenario: 'DELETE: remove_volume', toolName: 'manage_level_structure', arguments: {"action": "remove_volume", "volumeName": TRIGGER_VOLUME}, expected: 'success|not found' },
    // === INFO ===
    { scenario: 'INFO: get_volumes_info', toolName: 'manage_level_structure', arguments: {"action": "get_volumes_info", "filter": "Test"}, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TEST_ACTOR }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('manage-level-structure', testCases);
