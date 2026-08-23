#!/usr/bin/env node
/**
 * Canonical Integration Test Suite
 *
 * Covers the 23 exposed MCP tools, with advanced capabilities routed as
 * parent-tool actions instead of separate child tools.
 *
 * Usage:
 *   node tests/integration.mjs
 *   npm test
 */

import { runToolTests } from './test-runner.mjs';

const TEST_FOLDER = '/Game/IntegrationTest';
const ADV_TEST_FOLDER = '/Game/AdvancedIntegrationTest';
const PCG_TEST_GRAPH = `${ADV_TEST_FOLDER}/PCG_IT_Main`;

const testCases = [
  { scenario: 'System: execute safe console command (log)', toolName: 'system_control', arguments: { action: 'execute_command', command: 'Log Integration test started' }, expected: 'success|handled|blocked' },
  { scenario: 'Lighting: list available light types', toolName: 'build_environment', arguments: { action: 'list_light_types' }, expected: 'success' },
  { scenario: 'Effects: list available debug shapes', toolName: 'manage_effect', arguments: { action: 'list_debug_shapes' }, expected: 'success' },
  { scenario: 'Sequencer: list available track types', toolName: 'manage_sequence', arguments: { action: 'list_track_types' }, expected: 'success' },
  { scenario: 'Asset: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Asset: create material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_IntegrationTest', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Actor: spawn StaticMeshActor (cube)', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'IT_Cube', location: { x: 0, y: 0, z: 200 } }, expected: 'success' },
  { scenario: 'Actor: set transform', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: 'IT_Cube', location: { x: 100, y: 100, z: 300 } }, expected: 'success|not found' },
  { scenario: 'Blueprint: create Actor blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: 'BP_IntegrationTest', path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  // Struct
  { scenario: 'Struct: Create test struct', toolName: 'manage_asset', arguments: { action: 'create_struct', name: 'S_IntegrationTest', path: TEST_FOLDER, save: true }, expected: 'success|already exists' },
  { scenario: 'Struct: Add struct member', toolName: 'manage_asset', arguments: { action: 'add_struct_member', structPath: `${TEST_FOLDER}/S_IntegrationTest`, memberName: 'Weight', memberType: 'Float', defaultValue: '1.0' }, expected: 'success' },
  { scenario: 'Struct: List struct members', toolName: 'manage_asset', arguments: { action: 'list_struct_members', structPath: `${TEST_FOLDER}/S_IntegrationTest` }, expected: 'success' },
  { scenario: 'Struct: Read struct', toolName: 'manage_asset', arguments: { action: 'read_struct', structPath: `${TEST_FOLDER}/S_IntegrationTest` }, expected: 'success' },
  { scenario: 'Struct: Recompile struct', toolName: 'manage_asset', arguments: { action: 'recompile_struct', structPath: `${TEST_FOLDER}/S_IntegrationTest`, save: true }, expected: 'success' },
  { scenario: 'Struct: Delete test struct', toolName: 'manage_asset', arguments: { action: 'delete_struct', structPath: `${TEST_FOLDER}/S_IntegrationTest` }, expected: 'success|not found' },
  { scenario: 'Geometry: Create box primitive', toolName: 'manage_geometry', arguments: { action: 'create_box', actorName: 'GeoTest_Box', dimensions: [100, 100, 100], location: { x: 0, y: 0, z: 100 } }, expected: 'success|already exists' },
  { scenario: 'Skeleton: Get skeleton info', toolName: 'animation_physics', arguments: { action: 'get_skeleton_info', skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|not found' },
  // Content ingestion. Every case is read-only or dryRun: a real migration
  // writes hundreds of packages into /Game, which is not something an
  // integration run should leave behind.
  { scenario: 'Content: list every content source root', toolName: 'manage_asset', arguments: { action: 'list_content_sources' }, expected: 'success' },
  { scenario: 'Content: list engine templates with package counts', toolName: 'manage_asset', arguments: { action: 'list_content_sources', sourceRoot: 'engineTemplates', filter: 'TP_', includePackageCounts: true, limit: 25, offset: 0 }, expected: 'success' },
  { scenario: 'Content: preview migrating a template subtree', toolName: 'manage_asset', arguments: { action: 'migrate_assets', sourceRoot: 'engineTemplates', sourceId: 'TP_ThirdPersonBP', subPath: 'ThirdPerson', destinationPath: '/Game', overwrite: false, dryRun: true, maxPackages: 4000 }, expected: 'success|not found' },
  { scenario: 'Content: reject a sourceRoot escape', toolName: 'manage_asset', arguments: { action: 'migrate_assets', sourceRoot: 'engineTemplates', sourceId: '../../../Windows' }, expected: 'error' },
  { scenario: 'Content: list downloaded Fab packs', toolName: 'manage_asset', arguments: { action: 'list_fab_downloads' }, expected: 'success' },
  { scenario: 'Content: Fab download rejects a non-https url', toolName: 'manage_asset', arguments: { action: 'download_fab_asset', assetId: 'it-1', downloadUrl: 'ftp://bad', destinationDirectory: '', downloadType: 'http' }, expected: 'error' },
  { scenario: 'Content: Fab details rejects a listing id with path characters', toolName: 'manage_asset', arguments: { action: 'get_fab_listing_details', listingId: '../../i/auth' }, expected: 'error' },
  { scenario: 'Content: Fab search rejects a query with quotes', toolName: 'manage_asset', arguments: { action: 'search_fab_listings', query: 'rock" OR 1=1', freeOnly: true, limit: 5 }, expected: 'error' },
  { scenario: 'Content: Fab add rejects a listing id with path characters', toolName: 'manage_asset', arguments: { action: 'add_fab_asset_to_project', listingId: '../../i/auth' }, expected: 'error' },
  { scenario: 'Content: list synced Fab library', toolName: 'manage_asset', arguments: { action: 'list_fab_library', limit: 50, columnTypes: ['/Script/Fab.FabObjectNameColumn'] }, expected: 'success' },
  { scenario: 'Content: list Megascans library', toolName: 'manage_asset', arguments: { action: 'list_megascans_library', filter: 'surface' }, expected: 'success' },
  { scenario: 'Content: import Megascans without required fields', toolName: 'manage_asset', arguments: { action: 'import_megascans_asset', assetPaths: [], folderName: 'IT_Missing', assetType: '3d', exportMode: 'normal', assetId: 'it-1', name: 'IT Missing' }, expected: 'error' },
  { scenario: 'Content: import Megascans with a raw payload', toolName: 'manage_asset', arguments: { action: 'import_megascans_asset', payload: { exportPayload: [] } }, expected: 'success' },
  { scenario: 'Editor: invoke a reflected function on a missing class', toolName: 'control_editor', arguments: { action: 'invoke_reflected_function', className: 'NoSuchReflectedClassForIT', functionName: 'Nope', arguments: {} }, expected: 'error' },
  { scenario: 'Editor: describe a reflected plugin API', toolName: 'control_editor', arguments: { action: 'describe_reflected_api', className: 'FabBrowserApi', filter: 'Add' }, expected: 'success|not found' },
  { scenario: 'Editor: open an unregistered tab id', toolName: 'control_editor', arguments: { action: 'open_editor_tab', tabId: 'NoSuchTabForIntegrationTest' }, expected: 'error' },
  { scenario: 'Plugins: list plugins matching Chaos', toolName: 'system_control', arguments: { action: 'list_plugins', filter: 'Chaos', enabledOnly: false }, expected: 'success' },
  { scenario: 'Plugins: enable a plugin that is not installed', toolName: 'system_control', arguments: { action: 'enable_plugin', pluginName: 'NoSuchPluginForIntegrationTest' }, expected: 'error' },
  { scenario: 'Plugins: disable a plugin that is not installed', toolName: 'system_control', arguments: { action: 'disable_plugin', pluginName: 'NoSuchPluginForIntegrationTest' }, expected: 'error' },
  { scenario: 'Asset: create advanced test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Material Authoring: Create material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_AdvTest', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Texture: Create noise texture', toolName: 'manage_asset', arguments: { action: 'create_noise_texture', name: 'T_TestNoise', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Animation: Create anim blueprint', toolName: 'animation_physics', arguments: { action: 'create_anim_blueprint', name: 'ABP_Test', path: ADV_TEST_FOLDER, skeletonPath: '/Engine/EngineMeshes/SkeletalCube_Skeleton' }, expected: 'success|already exists|not found' },
  { scenario: 'Niagara: Create niagara system', toolName: 'manage_effect', arguments: { action: 'create_niagara_system', name: 'NS_Test', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GAS: Create attribute set', toolName: 'manage_gas', arguments: { action: 'create_attribute_set', name: 'AS_TestAttributes', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Combat: Create weapon blueprint', toolName: 'manage_combat', arguments: { action: 'create_weapon_blueprint', name: 'BP_TestWeapon', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'AI: Create AI controller', toolName: 'manage_ai', arguments: { action: 'create_ai_controller', name: 'AIC_Test', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Interaction: Create door actor', toolName: 'manage_interaction', arguments: { action: 'create_door_actor', name: 'BP_TestDoor', folder: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Widget: Create widget blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', name: 'WBP_TestWidget', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Networking setup: create character blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: 'BP_TestCharacter', path: ADV_TEST_FOLDER, parentClass: 'Character' }, expected: 'success|already exists' },
  { scenario: 'Networking setup: add Health variable', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: `${ADV_TEST_FOLDER}/BP_TestCharacter`, variableName: 'Health', variableType: 'float' }, expected: 'success|already exists' },
  { scenario: 'Networking: Set property replicated', toolName: 'manage_networking', arguments: { action: 'set_property_replicated', blueprintPath: `${ADV_TEST_FOLDER}/BP_TestCharacter`, propertyName: 'Health', replicated: true }, expected: 'success' },
  { scenario: 'Game Framework: Create game mode', toolName: 'manage_networking', arguments: { action: 'create_game_mode', name: 'GM_Test', path: ADV_TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Game Framework: Get info', toolName: 'manage_networking', arguments: { action: 'get_game_framework_info', gameModeBlueprint: `${ADV_TEST_FOLDER}/GM_Test` }, expected: 'success|not found' },
  { scenario: 'Sessions: Configure local session', toolName: 'manage_networking', arguments: { action: 'configure_local_session_settings', maxPlayers: 4, sessionName: 'TestSession' }, expected: 'success' },
  { scenario: 'Sessions: Configure split screen', toolName: 'manage_networking', arguments: { action: 'configure_split_screen', enabled: true, splitScreenType: 'TwoPlayer_Horizontal' }, expected: 'success' },
  { scenario: 'Sessions: Get info', toolName: 'manage_networking', arguments: { action: 'get_sessions_info' }, expected: 'success' },
  // Level Structure
  { scenario: 'Level Structure: Get info', toolName: 'manage_level_structure', arguments: { action: 'get_level_structure_info' }, expected: 'success' },
  { scenario: 'Level Structure: Enable World Partition', toolName: 'manage_level_structure', arguments: { action: 'enable_world_partition', bEnableWorldPartition: true }, expected: 'success|cannot enable' },
  { scenario: 'Level Structure: Configure grid size', toolName: 'manage_level_structure', arguments: { action: 'configure_grid_size', gridCellSize: 12800, loadingRange: 25600 }, expected: 'success|not enabled' },
  { scenario: 'Level Structure: Create data layer', toolName: 'manage_level_structure', arguments: { action: 'create_data_layer', dataLayerName: 'TestLayer', dataLayerType: 'Runtime' }, expected: 'success|world partition|not available' },
  { scenario: 'Level Structure: Configure HLOD', toolName: 'manage_level_structure', arguments: { action: 'configure_hlod_layer', hlodLayerName: 'DefaultHLOD', cellSize: 25600 }, expected: 'success' },
  { scenario: 'Level Structure: Open Level Blueprint', toolName: 'manage_level_structure', arguments: { action: 'open_level_blueprint' }, expected: 'success' },
  // Volumes & Zones
  { scenario: 'Volumes: Create trigger box', toolName: 'manage_level_structure', arguments: { action: 'create_trigger_box', volumeName: 'IT_TriggerBox', location: { x: 500, y: 0, z: 100 }, extent: { x: 100, y: 100, z: 100 } }, expected: 'success' },
  { scenario: 'Volumes: Create blocking volume', toolName: 'manage_level_structure', arguments: { action: 'create_blocking_volume', volumeName: 'IT_BlockingVol', location: { x: 600, y: 0, z: 100 }, extent: { x: 200, y: 200, z: 200 } }, expected: 'success' },
  { scenario: 'Volumes: Create physics volume', toolName: 'manage_level_structure', arguments: { action: 'create_physics_volume', volumeName: 'IT_PhysicsVol', location: { x: 700, y: 0, z: 100 }, bWaterVolume: true, fluidFriction: 0.5 }, expected: 'success' },
  { scenario: 'Volumes: Create audio volume', toolName: 'manage_level_structure', arguments: { action: 'create_audio_volume', volumeName: 'IT_AudioVol', location: { x: 800, y: 0, z: 100 }, bEnabled: true }, expected: 'success' },
  { scenario: 'Volumes: Create nav mesh bounds', toolName: 'manage_level_structure', arguments: { action: 'create_nav_mesh_bounds_volume', volumeName: 'IT_NavBoundsVol', location: { x: 0, y: 500, z: 100 }, extent: { x: 2000, y: 2000, z: 500 } }, expected: 'success' },
  { scenario: 'Volumes: Get volumes info', toolName: 'manage_level_structure', arguments: { action: 'get_volumes_info', volumeType: 'Trigger' }, expected: 'success' },
  { scenario: 'Volumes: Set volume properties', toolName: 'manage_level_structure', arguments: { action: 'set_volume_properties', volumeName: 'IT_PhysicsVol', bWaterVolume: false, fluidFriction: 0.3 }, expected: 'success|not found' },
  // Navigation System
  { scenario: 'Navigation: Get navigation info', toolName: 'manage_ai', arguments: { action: 'get_navigation_info' }, expected: 'success' },
  { scenario: 'Navigation: Set nav agent properties', toolName: 'manage_ai', arguments: { action: 'set_nav_agent_properties', agentRadius: 35, agentHeight: 144, agentStepHeight: 35 }, expected: 'success' },
  { scenario: 'Navigation: Configure nav mesh settings', toolName: 'manage_ai', arguments: { action: 'configure_nav_mesh_settings', cellSize: 19, cellHeight: 10, tileSizeUU: 1000 }, expected: 'success' },
  { scenario: 'Navigation: Create nav link proxy', toolName: 'manage_ai', arguments: { action: 'create_nav_link_proxy', actorName: 'IT_NavLink', location: { x: 0, y: 0, z: 100 }, startPoint: { x: -100, y: 0, z: 0 }, endPoint: { x: 100, y: 0, z: 0 }, direction: 'BothWays' }, expected: 'success' },
  { scenario: 'Navigation: Configure nav link', toolName: 'manage_ai', arguments: { action: 'configure_nav_link', actorName: 'IT_NavLink', snapRadius: 30 }, expected: 'success|not found' },
  { scenario: 'Navigation: Set nav link type', toolName: 'manage_ai', arguments: { action: 'set_nav_link_type', actorName: 'IT_NavLink', linkType: 'smart' }, expected: 'success|not found' },
  // Spline System
  { scenario: 'Splines: Create spline actor', toolName: 'build_environment', arguments: { action: 'create_spline_actor', actorName: 'IT_SplineActor', location: { x: 0, y: 0, z: 100 }, bClosedLoop: false }, expected: 'success' },
  { scenario: 'Splines: Add spline point', toolName: 'build_environment', arguments: { action: 'add_spline_point', actorName: 'IT_SplineActor', position: { x: 500, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Splines: Set spline point position', toolName: 'build_environment', arguments: { action: 'set_spline_point_position', actorName: 'IT_SplineActor', pointIndex: 1, position: { x: 600, y: 100, z: 150 } }, expected: 'success|not found' },
  { scenario: 'Splines: Set spline type', toolName: 'build_environment', arguments: { action: 'set_spline_type', actorName: 'IT_SplineActor', splineType: 'linear' }, expected: 'success|not found' },
  { scenario: 'Splines: Create road spline', toolName: 'build_environment', arguments: { action: 'create_road_spline', actorName: 'IT_RoadSpline', location: { x: 1000, y: 0, z: 0 }, width: 400 }, expected: 'success' },
  { scenario: 'Splines: Get splines info', toolName: 'build_environment', arguments: { action: 'get_splines_info' }, expected: 'success' },
  { scenario: 'Splines: Get specific spline info', toolName: 'build_environment', arguments: { action: 'get_splines_info', actorName: 'IT_SplineActor' }, expected: 'success|not found' },
  { scenario: 'PCG: Create graph', toolName: 'manage_pcg', arguments: { action: 'create_pcg_graph', graphPath: PCG_TEST_GRAPH, overwrite: true, save: false }, expected: { successPattern: 'PCG graph', errorPattern: 'PCG_PLUGIN' } },
  { scenario: 'Cleanup: delete spline actors', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'IT_SplineActor' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete road spline', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'IT_RoadSpline' }, expected: 'success|not found' },
  // search_assets: searchText filtering (fix for Issue #233)
  { scenario: 'Asset: search by text (exact name)', toolName: 'manage_asset', arguments: { action: 'search_assets', searchText: 'BP_IntegrationTest' }, expected: 'success' },
  { scenario: 'Asset: search by text (partial, case-insensitive)', toolName: 'manage_asset', arguments: { action: 'search_assets', searchText: 'integrationtest' }, expected: 'success' },
  { scenario: 'Asset: search by text + class filter', toolName: 'manage_asset', arguments: { action: 'search_assets', searchText: 'IntegrationTest', classNames: ['Blueprint'] }, expected: 'success' },
  { scenario: 'Asset: search by text + path filter', toolName: 'manage_asset', arguments: { action: 'search_assets', searchText: 'IntegrationTest', packagePaths: ['/Game/IntegrationTest'], recursivePaths: true }, expected: 'success' },
  { scenario: 'Asset: search with no matches', toolName: 'manage_asset', arguments: { action: 'search_assets', searchText: 'ZZZZZ_NonExistent_Asset_12345' }, expected: 'success' },
  { scenario: 'Asset: search without searchText (structured query)', toolName: 'manage_asset', arguments: { action: 'search_assets', classNames: ['Blueprint'], packagePaths: ['/Game/IntegrationTest'] }, expected: 'success' },
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'IT_Cube' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found', timeoutMs: 30000 },
  { scenario: 'Cleanup: delete advanced test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: ADV_TEST_FOLDER, force: true }, expected: 'success|not found', timeoutMs: 30000 }
];

runToolTests('integration', testCases);
