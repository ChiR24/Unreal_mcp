#!/usr/bin/env node
/**
 * Consolidated Integration Test Suite (~40 scenarios)
 *
 * Covers all 17 top-level MCP tools with minimal overlap:
 * - Infrastructure & Discovery
 * - Asset & Material Lifecycle
 * - Actor Control & Introspection
 * - Blueprint Authoring
 * - Environment & Visuals
 * - AI & Input
 * - Cinematics & Audio
 * - Operations & Performance
 *
 * Usage:
 *   node tests/integration.mjs
 *   npm run test:all
 */

import { runToolTests } from './test-runner.mjs';

// Shared test folder for asset chaining
const TEST_FOLDER = '/Game/IntegrationTest';
const TEST_ACTOR = 'IT_Cube';
const TEST_BP = 'BP_IntegrationTest';
const TEST_MATERIAL = 'M_IntegrationTest';
const TEST_SEQUENCE = 'LS_IntegrationTest';

const testCases = [
  // ============================================================
  // Group 1: Infrastructure & Discovery (4 scenarios)
  // ============================================================
  {
    scenario: 'System: execute safe console command (log)',
    toolName: 'system_control',
    arguments: { action: 'execute_command', command: 'Log Integration test started' },
    expected: 'success|handled|blocked'
  },
  {
    scenario: 'Lighting: list available light types',
    toolName: 'manage_lighting',
    arguments: { action: 'list_light_types' },
    expected: 'success'
  },
  {
    scenario: 'Effects: list available debug shapes',
    toolName: 'manage_effect',
    arguments: { action: 'list_debug_shapes' },
    expected: 'success'
  },
  {
    scenario: 'Sequencer: list available track types',
    toolName: 'manage_sequence',
    arguments: { action: 'list_track_types' },
    expected: 'success'
  },

  // ============================================================
  // Group 2: Asset & Material Lifecycle (6 scenarios)
  // ============================================================
  {
    scenario: 'Asset: create test folder',
    toolName: 'manage_asset',
    arguments: { action: 'create_folder', path: TEST_FOLDER },
    expected: 'success|already exists'
  },
  {
    scenario: 'Asset: list assets in test folder',
    toolName: 'manage_asset',
    arguments: { action: 'list', path: TEST_FOLDER },
    expected: 'success'
  },
  {
    scenario: 'Asset: create material',
    toolName: 'manage_asset',
    arguments: { action: 'create_material', name: TEST_MATERIAL, path: TEST_FOLDER },
    expected: 'success|already exists'
  },
  {
    scenario: 'Asset: create material instance',
    toolName: 'manage_asset',
    arguments: { action: 'create_material_instance', name: 'MI_IntegrationTest', parentPath: `${TEST_FOLDER}/${TEST_MATERIAL}`, path: TEST_FOLDER },
    expected: 'success|already exists|not found'
  },
  {
    scenario: 'Asset: search for integration test assets',
    toolName: 'manage_asset',
    arguments: { action: 'search_assets', query: 'IntegrationTest' },
    expected: 'success'
  },
  {
    scenario: 'Asset: get asset info',
    toolName: 'manage_asset',
    arguments: { action: 'get_info', path: `${TEST_FOLDER}/${TEST_MATERIAL}` },
    expected: 'success|not found'
  },

  // ============================================================
  // Group 3: Actor Control & Introspection (7 scenarios)
  // ============================================================
  {
    scenario: 'Actor: spawn StaticMeshActor (cube)',
    toolName: 'control_actor',
    arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 200 } },
    expected: 'success'
  },
  {
    scenario: 'Actor: set transform',
    toolName: 'control_actor',
    arguments: { action: 'set_transform', actorName: TEST_ACTOR, location: { x: 100, y: 100, z: 300 }, rotation: { pitch: 0, yaw: 45, roll: 0 } },
    expected: 'success|not found'
  },
  {
    scenario: 'Actor: get transform',
    toolName: 'control_actor',
    arguments: { action: 'get_transform', actorName: TEST_ACTOR },
    expected: 'success|not found'
  },
  {
    scenario: 'Actor: add PointLight component',
    toolName: 'control_actor',
    arguments: { action: 'add_component', actorName: TEST_ACTOR, componentType: 'PointLightComponent', componentName: 'IT_Light', properties: { Intensity: 5000 } },
    expected: 'success|not found'
  },
  {
    scenario: 'Actor: get components',
    toolName: 'control_actor',
    arguments: { action: 'get_components', actorName: TEST_ACTOR },
    expected: 'success|not found'
  },
  {
    scenario: 'Inspect: get actor property',
    toolName: 'inspect',
    arguments: { action: 'get_property', targetName: TEST_ACTOR, propertyName: 'ActorLabel' },
    expected: 'success|not found'
  },
  {
    scenario: 'Inspect: list objects matching filter',
    toolName: 'inspect',
    arguments: { action: 'list_objects', classFilter: 'StaticMeshActor', nameFilter: 'IT_' },
    expected: 'success'
  },

  // ============================================================
  // Group 4: Blueprint Authoring (6 scenarios)
  // ============================================================
  {
    scenario: 'Blueprint: create Actor blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', name: TEST_BP, path: TEST_FOLDER, parentClass: 'Actor' },
    expected: 'success|already exists'
  },
  {
    scenario: 'Blueprint: add variable',
    toolName: 'manage_blueprint',
    arguments: { action: 'add_variable', blueprintPath: `${TEST_FOLDER}/${TEST_BP}`, variableName: 'Health', variableType: 'float', defaultValue: '100.0' },
    expected: 'success|already exists|not found'
  },
  {
    scenario: 'Blueprint: add StaticMesh component',
    toolName: 'manage_blueprint',
    arguments: { action: 'add_component', blueprintPath: `${TEST_FOLDER}/${TEST_BP}`, componentClass: 'StaticMeshComponent', componentName: 'MeshComp' },
    expected: 'success|already exists|not found'
  },
  {
    scenario: 'Blueprint: compile',
    toolName: 'manage_blueprint',
    arguments: { action: 'compile', blueprintPath: `${TEST_FOLDER}/${TEST_BP}` },
    expected: 'success|not found'
  },
  {
    scenario: 'Blueprint Graph: add event node',
    toolName: 'manage_blueprint_graph',
    arguments: { action: 'add_node', blueprintPath: `${TEST_FOLDER}/${TEST_BP}`, graphName: 'EventGraph', nodeType: 'K2Node_Event', eventName: 'ReceiveBeginPlay' },
    expected: 'success|already exists|not found'
  },
  {
    scenario: 'Blueprint Graph: add PrintString node',
    toolName: 'manage_blueprint_graph',
    arguments: { action: 'add_node', blueprintPath: `${TEST_FOLDER}/${TEST_BP}`, graphName: 'EventGraph', nodeType: 'K2Node_CallFunction', functionName: 'PrintString' },
    expected: 'success|not found'
  },

  // ============================================================
  // Group 5: Environment & Visuals (6 scenarios)
  // ============================================================
  {
    scenario: 'Lighting: spawn directional light',
    toolName: 'manage_lighting',
    arguments: { action: 'spawn_light', lightType: 'directional', actorName: 'IT_DirLight', location: { x: 0, y: 0, z: 500 } },
    expected: 'success'
  },
  {
    scenario: 'Lighting: configure global illumination',
    toolName: 'manage_lighting',
    arguments: { action: 'setup_global_illumination', method: 'Lumen' },
    expected: 'success|not supported|unsupported'
  },
  {
    scenario: 'Environment: create landscape (minimal)',
    toolName: 'build_environment',
    arguments: { action: 'create_landscape', sizeX: 1, sizeY: 1, sectionsPerComponent: 1, componentsX: 1, componentsY: 1 },
    expected: 'success|already exists|not supported'
  },
  {
    scenario: 'Effects: spawn Niagara system',
    toolName: 'manage_effect',
    arguments: { action: 'spawn_niagara', systemPath: '/Engine/Transient.DefaultParticleSystem', actorName: 'IT_Niagara', location: { x: 200, y: 0, z: 200 } },
    expected: 'success|not found'
  },
  {
    scenario: 'Effects: draw debug sphere',
    toolName: 'manage_effect',
    arguments: { action: 'debug_shape', shapeType: 'Sphere', location: { x: 0, y: 200, z: 200 }, radius: 50, color: { r: 255, g: 0, b: 0 }, duration: 5 },
    expected: 'success'
  },
  {
    scenario: 'Actor: toggle visibility',
    toolName: 'control_actor',
    arguments: { action: 'set_visibility', actorName: TEST_ACTOR, visible: false },
    expected: 'success|not found'
  },

  // ============================================================
  // Group 6: AI & Input (4 scenarios)
  // ============================================================
  {
    scenario: 'Behavior Tree: create BT asset',
    toolName: 'manage_behavior_tree',
    arguments: { action: 'create', name: 'BT_IntegrationTest', path: TEST_FOLDER },
    expected: 'success|already exists'
  },
  {
    scenario: 'Behavior Tree: add sequence node',
    toolName: 'manage_behavior_tree',
    arguments: { action: 'add_node', btPath: `${TEST_FOLDER}/BT_IntegrationTest`, nodeType: 'BTComposite_Sequence', nodeName: 'MainSequence' },
    expected: 'success|not found'
  },
  {
    scenario: 'Input: create input action',
    toolName: 'manage_input',
    arguments: { action: 'create_input_action', name: 'IA_IntegrationTest', path: TEST_FOLDER },
    expected: 'success|already exists'
  },
  {
    scenario: 'Input: create input mapping context',
    toolName: 'manage_input',
    arguments: { action: 'create_mapping_context', name: 'IMC_IntegrationTest', path: TEST_FOLDER },
    expected: 'success|already exists'
  },

  // ============================================================
  // Group 7: Cinematics & Audio (4 scenarios)
  // ============================================================
  {
    scenario: 'Sequencer: create level sequence',
    toolName: 'manage_sequence',
    arguments: { action: 'create', name: TEST_SEQUENCE, path: TEST_FOLDER },
    expected: 'success|already exists'
  },
  {
    scenario: 'Sequencer: add actor to sequence',
    toolName: 'manage_sequence',
    arguments: { action: 'add_actor', sequencePath: `${TEST_FOLDER}/${TEST_SEQUENCE}`, actorName: TEST_ACTOR },
    expected: 'success|not found'
  },
  {
    scenario: 'Audio: play 2D sound',
    toolName: 'manage_audio',
    arguments: { action: 'play_sound_2d', soundPath: '/Engine/EngineSounds/WhiteNoise' },
    expected: 'success|not found'
  },
  {
    scenario: 'Audio: play sound at location',
    toolName: 'manage_audio',
    arguments: { action: 'play_sound_at_location', soundPath: '/Engine/EngineSounds/WhiteNoise', location: { x: 0, y: 0, z: 100 } },
    expected: 'success|not found'
  },

  // ============================================================
  // Group 8: Operations & Performance (5 scenarios)
  // ============================================================
  {
    scenario: 'Level: list available levels',
    toolName: 'manage_level',
    arguments: { action: 'list_levels' },
    expected: 'success'
  },
  {
    scenario: 'Editor: take screenshot',
    toolName: 'control_editor',
    arguments: { action: 'screenshot', filename: 'integration_test_screenshot' },
    expected: 'success|not supported'
  },
  {
    scenario: 'Editor: set viewport resolution',
    toolName: 'control_editor',
    arguments: { action: 'set_viewport_resolution', width: 1920, height: 1080 },
    expected: 'success'
  },
  {
    scenario: 'Performance: set scalability preset',
    toolName: 'manage_performance',
    arguments: { action: 'set_scalability', preset: 'Epic' },
    expected: 'success'
  },
  {
    scenario: 'Performance: show FPS',
    toolName: 'manage_performance',
    arguments: { action: 'show_fps', enabled: true },
    expected: 'success'
  },

  // ============================================================
  // Cleanup (2 scenarios)
  // ============================================================
  {
    scenario: 'Cleanup: delete test actor',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorName: TEST_ACTOR },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete test folder',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: TEST_FOLDER, force: true },
    expected: 'success|not found'
  }
];

// Run the consolidated integration suite
runToolTests('integration', testCases);
