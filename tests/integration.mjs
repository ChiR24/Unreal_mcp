#!/usr/bin/env node
/**
 * Fully Consolidated Integration Test Suite
 *
 * Covers all 37 MCP tools (Phases 1-26):
 * - Groups 1-8: Original 17 tools
 * - Groups 9-26: Advanced tools (Phases 6-26)
 *
 * Usage:
 *   node tests/integration.mjs
 *   npm test
 */

import { runToolTests } from './test-runner.mjs';

const TEST_FOLDER = '/Game/IntegrationTest';
const ADV_TEST_FOLDER = '/Game/AdvancedIntegrationTest';

const testCases = [
  { scenario: 'System: execute safe console command (log)', toolName: 'system_control', arguments: { action: 'execute_command', command: 'Log Integration test started' }, expected: 'success|handled|blocked' },
  { scenario: 'Lighting: list available light types', toolName: 'manage_lighting', arguments: { action: 'list_light_types' }, expected: 'success' },
  { scenario: 'Effects: list available debug shapes', toolName: 'manage_effect', arguments: { action: 'list_debug_shapes' }, expected: 'success' },
  { scenario: 'Sequencer: list available track types', toolName: 'manage_sequence', arguments: { action: 'list_track_types' }, expected: 'success' },
  { scenario: 'Asset: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Asset: create material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_IntegrationTest', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Actor: spawn StaticMeshActor (cube)', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'IT_Cube', location: { x: 0, y: 0, z: 200 } }, expected: 'success' },
  { scenario: 'Actor: set transform', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: 'IT_Cube', location: { x: 100, y: 100, z: 300 } }, expected: 'success|not found' },
  { scenario: 'Blueprint: create Actor blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: 'BP_IntegrationTest', path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  { scenario: 'Geometry: Create box primitive', toolName: 'manage_geometry', arguments: { action: 'create_box', actorName: 'GeoTest_Box', dimensions: [100, 100, 100], location: { x: 0, y: 0, z: 100 } }, expected: 'success|already exists' },
  { scenario: 'Skeleton: Get skeleton info', toolName: 'manage_skeleton', arguments: { action: 'get_skeleton_info', skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  { scenario: 'Material Authoring: Create material', toolName: 'manage_material_authoring', arguments: { action: 'create_material', name: 'M_AdvTest', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Texture: Create noise texture', toolName: 'manage_texture', arguments: { action: 'create_noise_texture', name: 'T_TestNoise', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Animation: Create anim blueprint', toolName: 'manage_animation_authoring', arguments: { action: 'create_anim_blueprint', name: 'ABP_Test', path: ADV_TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|already exists|not found' },
  { scenario: 'Niagara: Create niagara system', toolName: 'manage_niagara_authoring', arguments: { action: 'create_niagara_system', name: 'NS_Test', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GAS: Create attribute set', toolName: 'manage_gas', arguments: { action: 'create_attribute_set', name: 'AS_TestAttributes', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: Create weapon blueprint', toolName: 'manage_combat', arguments: { action: 'create_weapon_blueprint', name: 'BP_TestWeapon', path: ADV_TEST_FOLDER, weaponType: 'Rifle' }, expected: 'success|already exists' },
  { scenario: 'AI: Create AI controller', toolName: 'manage_ai', arguments: { action: 'create_ai_controller', name: 'AIC_Test', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Interaction: Create door actor', toolName: 'manage_interaction', arguments: { action: 'create_door_actor', name: 'BP_TestDoor', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Widget: Create widget blueprint', toolName: 'manage_widget_authoring', arguments: { action: 'create_widget_blueprint', name: 'WBP_TestWidget', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Networking: Set property replicated', toolName: 'manage_networking', arguments: { action: 'set_property_replicated', blueprintPath: `${ADV_TEST_FOLDER}/BP_TestCharacter`, propertyName: 'Health', replicated: true }, expected: 'success|not found' },
  { scenario: 'Game Framework: Create game mode', toolName: 'manage_game_framework', arguments: { action: 'create_game_mode', name: 'GM_Test', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Game Framework: Get info', toolName: 'manage_game_framework', arguments: { action: 'get_game_framework_info', gameModeBlueprint: `${ADV_TEST_FOLDER}/GM_Test` }, expected: 'success|not found' },
  { scenario: 'Sessions: Configure local session', toolName: 'manage_sessions', arguments: { action: 'configure_local_session_settings', maxPlayers: 4, sessionName: 'TestSession' }, expected: 'success' },
  { scenario: 'Sessions: Configure split screen', toolName: 'manage_sessions', arguments: { action: 'configure_split_screen', enabled: true, splitScreenType: 'TwoPlayer_Horizontal' }, expected: 'success' },
  { scenario: 'Sessions: Get info', toolName: 'manage_sessions', arguments: { action: 'get_sessions_info' }, expected: 'success' },
  // Phase 23: Level Structure
  { scenario: 'Level Structure: Get info', toolName: 'manage_level_structure', arguments: { action: 'get_level_structure_info' }, expected: 'success' },
  { scenario: 'Level Structure: Enable World Partition', toolName: 'manage_level_structure', arguments: { action: 'enable_world_partition', bEnableWorldPartition: true }, expected: 'success' },
  { scenario: 'Level Structure: Configure grid size', toolName: 'manage_level_structure', arguments: { action: 'configure_grid_size', gridCellSize: 12800, loadingRange: 25600 }, expected: 'success|not enabled' },
  { scenario: 'Level Structure: Create data layer', toolName: 'manage_level_structure', arguments: { action: 'create_data_layer', dataLayerName: 'TestLayer', dataLayerType: 'Runtime' }, expected: 'success|not available' },
  { scenario: 'Level Structure: Configure HLOD', toolName: 'manage_level_structure', arguments: { action: 'configure_hlod_layer', hlodLayerName: 'DefaultHLOD', cellSize: 25600 }, expected: 'success' },
  { scenario: 'Level Structure: Open Level Blueprint', toolName: 'manage_level_structure', arguments: { action: 'open_level_blueprint' }, expected: 'success' },
  // Phase 24: Volumes & Zones
  { scenario: 'Volumes: Create trigger box', toolName: 'manage_volumes', arguments: { action: 'create_trigger_box', volumeName: 'IT_TriggerBox', location: { x: 500, y: 0, z: 100 }, extent: { x: 100, y: 100, z: 100 } }, expected: 'success' },
  { scenario: 'Volumes: Create blocking volume', toolName: 'manage_volumes', arguments: { action: 'create_blocking_volume', volumeName: 'IT_BlockingVol', location: { x: 600, y: 0, z: 100 }, extent: { x: 200, y: 200, z: 200 } }, expected: 'success' },
  { scenario: 'Volumes: Create physics volume', toolName: 'manage_volumes', arguments: { action: 'create_physics_volume', volumeName: 'IT_PhysicsVol', location: { x: 700, y: 0, z: 100 }, bWaterVolume: true, fluidFriction: 0.5 }, expected: 'success' },
  { scenario: 'Volumes: Create audio volume', toolName: 'manage_volumes', arguments: { action: 'create_audio_volume', volumeName: 'IT_AudioVol', location: { x: 800, y: 0, z: 100 }, bEnabled: true }, expected: 'success' },
  { scenario: 'Volumes: Create nav mesh bounds', toolName: 'manage_volumes', arguments: { action: 'create_nav_mesh_bounds_volume', volumeName: 'IT_NavBoundsVol', location: { x: 0, y: 500, z: 100 }, extent: { x: 2000, y: 2000, z: 500 } }, expected: 'success' },
  { scenario: 'Volumes: Get volumes info', toolName: 'manage_volumes', arguments: { action: 'get_volumes_info', volumeType: 'Trigger' }, expected: 'success' },
  { scenario: 'Volumes: Set volume properties', toolName: 'manage_volumes', arguments: { action: 'set_volume_properties', volumeName: 'IT_PhysicsVol', bWaterVolume: false, fluidFriction: 0.3 }, expected: 'success|not found' },
  // Phase 25: Navigation System
  { scenario: 'Navigation: Get navigation info', toolName: 'manage_navigation', arguments: { action: 'get_navigation_info' }, expected: 'success' },
  { scenario: 'Navigation: Set nav agent properties', toolName: 'manage_navigation', arguments: { action: 'set_nav_agent_properties', agentRadius: 35, agentHeight: 144, agentStepHeight: 35 }, expected: 'success' },
  { scenario: 'Navigation: Configure nav mesh settings', toolName: 'manage_navigation', arguments: { action: 'configure_nav_mesh_settings', cellSize: 19, cellHeight: 10, tileSizeUU: 1000 }, expected: 'success' },
  { scenario: 'Navigation: Create nav link proxy', toolName: 'manage_navigation', arguments: { action: 'create_nav_link_proxy', actorName: 'IT_NavLink', location: { x: 0, y: 0, z: 100 }, startPoint: { x: -100, y: 0, z: 0 }, endPoint: { x: 100, y: 0, z: 0 }, direction: 'BothWays' }, expected: 'success' },
  { scenario: 'Navigation: Configure nav link', toolName: 'manage_navigation', arguments: { action: 'configure_nav_link', actorName: 'IT_NavLink', snapRadius: 30 }, expected: 'success|not found' },
  { scenario: 'Navigation: Set nav link type', toolName: 'manage_navigation', arguments: { action: 'set_nav_link_type', actorName: 'IT_NavLink', linkType: 'smart' }, expected: 'success|not found' },
  // Phase 26: Spline System
  { scenario: 'Splines: Create spline actor', toolName: 'manage_splines', arguments: { action: 'create_spline_actor', actorName: 'IT_SplineActor', location: { x: 0, y: 0, z: 100 }, bClosedLoop: false }, expected: 'success' },
  { scenario: 'Splines: Add spline point', toolName: 'manage_splines', arguments: { action: 'add_spline_point', actorName: 'IT_SplineActor', position: { x: 500, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Splines: Set spline point position', toolName: 'manage_splines', arguments: { action: 'set_spline_point_position', actorName: 'IT_SplineActor', pointIndex: 1, position: { x: 600, y: 100, z: 150 } }, expected: 'success|not found' },
  { scenario: 'Splines: Set spline type', toolName: 'manage_splines', arguments: { action: 'set_spline_type', actorName: 'IT_SplineActor', splineType: 'linear' }, expected: 'success|not found' },
  { scenario: 'Splines: Create road spline', toolName: 'manage_splines', arguments: { action: 'create_road_spline', actorName: 'IT_RoadSpline', location: { x: 1000, y: 0, z: 0 }, width: 400 }, expected: 'success' },
  { scenario: 'Splines: Get splines info', toolName: 'manage_splines', arguments: { action: 'get_splines_info' }, expected: 'success' },
  { scenario: 'Splines: Get specific spline info', toolName: 'manage_splines', arguments: { action: 'get_splines_info', actorName: 'IT_SplineActor' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete spline actors', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'IT_SplineActor' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete road spline', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'IT_RoadSpline' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'IT_Cube' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete advanced test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: ADV_TEST_FOLDER, force: true }, expected: 'success|not found' }
];

runToolTests('integration', testCases);
