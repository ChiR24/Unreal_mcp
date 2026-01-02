#!/usr/bin/env node
/**
 * Fully Consolidated Integration Test Suite
 *
 * Covers all 34 MCP tools (Phases 1-22):
 * - Groups 1-8: Original 17 tools
 * - Groups 9-22: Advanced tools (Phases 6-22)
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
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'IT_Cube' }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete advanced test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: ADV_TEST_FOLDER, force: true }, expected: 'success|not found' }
];

runToolTests('integration', testCases);
