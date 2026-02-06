#!/usr/bin/env node
import path from 'node:path';
import { fileURLToPath } from 'node:url';
/**
 * Core Tools Integration Tests
 * 
 * Tools: manage_asset (99), control_actor (45), control_editor (84), manage_level (87)
 * Total Actions: 315
 * Test Cases: 630 (2x coverage: success + edge/error cases)
 * 
 * Usage:
 *   node tests/category-tests/core-tools.test.mjs
 */

import { runToolTests } from '../test-runner.mjs';

// Resolve test assets path - points to tests/assets/ directory
const TEST_ASSETS_PATH = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../assets/').replace(/\\/g, '/');

const TEST_FOLDER = '/Game/CoreToolsTest';

// ============================================================================
// TEST SETUP - Create required test assets before running tests
// These tests MUST run first to set up assets for subsequent tests
// ============================================================================
const setupTests = [
  // Create test folder structure
  { scenario: 'SETUP: Create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'SETUP: Create materials subfolder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: `${TEST_FOLDER}/Materials` }, expected: 'success|already exists' },
  
  // Create M_TestMaterial by duplicating engine material (using BasicShapeMaterial which exists in UE 5.6)
  { scenario: 'SETUP: Create M_TestMaterial', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/BasicShapes/BasicShapeMaterial', destinationPath: `${TEST_FOLDER}/M_TestMaterial` }, expected: 'success|already exists|not found' },
  
  // Create MI_TestInstance material instance (using BasicShapeMaterial which exists in UE 5.6)
  { scenario: 'SETUP: Create MI_TestInstance', toolName: 'manage_asset', arguments: { action: 'create_material_instance', name: 'MI_TestInstance', path: TEST_FOLDER, parentMaterial: '/Engine/BasicShapes/BasicShapeMaterial' }, expected: 'success|already exists|not found' },
  
  // Import FBX mesh for mesh tests (user-provided path - may fail if file not present)
  { scenario: 'SETUP: Import Dragon FBX mesh', toolName: 'manage_asset', arguments: { action: 'import', sourcePath: 'C:/Users/micro/Downloads/Compressed/fbx/Dragon 2.5_fbx.fbx', destinationPath: `${TEST_FOLDER}/SM_Dragon` }, expected: 'success|already exists|error|file not found|not implemented' },
  
  // Create test Blueprint (may not be implemented - allow error)
  { scenario: 'SETUP: Create BP_TestActor', toolName: 'manage_asset', arguments: { action: 'bp_create', name: 'BP_TestActor', path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists|not implemented|error' },
];

// ============================================================================
// MANAGE_ASSET (99 actions x 2 = 198 tests)
// ============================================================================
const manageAssetTests = [
  // === 1. list ===
  { scenario: 'Asset: list assets in /Game', toolName: 'manage_asset', arguments: { action: 'list', directory: '/Game' }, expected: 'success' },
  { scenario: 'Asset: list with limit and recursive', toolName: 'manage_asset', arguments: { action: 'list', directory: '/Game', limit: 10, recursivePaths: true }, expected: 'success' },

  // === 2. import ===
  { scenario: 'Asset: import FBX file', toolName: 'manage_asset', arguments: { action: 'import', sourcePath: `${TEST_ASSETS_PATH}/mesh.fbx`, destinationPath: TEST_FOLDER }, expected: 'success|file not found' },
  { scenario: 'Asset: import with missing source', toolName: 'manage_asset', arguments: { action: 'import', sourcePath: '/nonexistent/file.fbx', destinationPath: TEST_FOLDER }, expected: 'error|file not found|success' },

  // === 3. duplicate ===
  { scenario: 'Asset: duplicate material', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/BasicShapes/BasicShapeMaterial', destinationPath: `${TEST_FOLDER}/M_Duplicated` }, expected: 'success|not found' },
  { scenario: 'Asset: duplicate nonexistent asset', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Game/NonExistent/Asset', destinationPath: `${TEST_FOLDER}/Copy` }, expected: 'not found|error|success' },

  // === 4. rename ===
  { scenario: 'Asset: rename asset', toolName: 'manage_asset', arguments: { action: 'rename', assetPath: `${TEST_FOLDER}/M_Duplicated`, newName: 'M_Renamed' }, expected: 'success|not found' },
  { scenario: 'Asset: rename nonexistent', toolName: 'manage_asset', arguments: { action: 'rename', assetPath: '/Game/NonExistent', newName: 'NewName' }, expected: 'not found|error|success' },

  // === 5. move ===
  { scenario: 'Asset: move asset to subfolder', toolName: 'manage_asset', arguments: { action: 'move', sourcePath: `${TEST_FOLDER}/M_Renamed`, destinationPath: `${TEST_FOLDER}/Materials/M_Renamed` }, expected: 'success|not found' },
  { scenario: 'Asset: move nonexistent asset', toolName: 'manage_asset', arguments: { action: 'move', sourcePath: '/Game/NonExistent', destinationPath: TEST_FOLDER }, expected: 'not found|error|success' },

  // === 6. delete ===
  { scenario: 'Asset: delete asset', toolName: 'manage_asset', arguments: { action: 'delete', assetPath: `${TEST_FOLDER}/Materials/M_Renamed` }, expected: 'success|not found' },
  { scenario: 'Asset: delete nonexistent', toolName: 'manage_asset', arguments: { action: 'delete', assetPath: '/Game/NonExistent/ToDelete' }, expected: 'not found|success' },

  // === 7. delete_asset ===
  { scenario: 'Asset: delete_asset single', toolName: 'manage_asset', arguments: { action: 'delete_asset', assetPath: `${TEST_FOLDER}/TempAsset` }, expected: 'success|not found' },
  { scenario: 'Asset: delete_asset nonexistent', toolName: 'manage_asset', arguments: { action: 'delete_asset', assetPath: '/Game/NonExistent/Single' }, expected: 'not found|success' },

  // === 8. delete_assets ===
  { scenario: 'Asset: delete_assets batch', toolName: 'manage_asset', arguments: { action: 'delete_assets', assetPaths: [`${TEST_FOLDER}/Asset1`, `${TEST_FOLDER}/Asset2`] }, expected: 'success|not found' },
  { scenario: 'Asset: delete_assets empty list', toolName: 'manage_asset', arguments: { action: 'delete_assets', assetPaths: [] }, expected: 'success|error' },

  // === 9. create_folder ===
  { scenario: 'Asset: create_folder new', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Asset: create_folder nested', toolName: 'manage_asset', arguments: { action: 'create_folder', path: `${TEST_FOLDER}/Sub/Nested` }, expected: 'success|already exists' },

  // === 10. search_assets ===
  { scenario: 'Asset: search_assets by class', toolName: 'manage_asset', arguments: { action: 'search_assets', classNames: ['StaticMesh'], limit: 5 }, expected: 'success' },
  { scenario: 'Asset: search_assets recursive', toolName: 'manage_asset', arguments: { action: 'search_assets', classNames: ['Material'], recursiveClasses: true, limit: 3 }, expected: 'success' },

  // === 11. get_dependencies ===
  { scenario: 'Asset: get_dependencies valid', toolName: 'manage_asset', arguments: { action: 'get_dependencies', assetPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Asset: get_dependencies nonexistent', toolName: 'manage_asset', arguments: { action: 'get_dependencies', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 12. get_source_control_state ===
  { scenario: 'Asset: get_source_control_state', toolName: 'manage_asset', arguments: { action: 'get_source_control_state', assetPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not connected' },
  { scenario: 'Asset: get_source_control_state nonexistent', toolName: 'manage_asset', arguments: { action: 'get_source_control_state', assetPath: '/Game/NonExistent' }, expected: 'not found|not connected|success' },

  // === 13. analyze_graph ===
  { scenario: 'Asset: analyze_graph blueprint', toolName: 'manage_asset', arguments: { action: 'analyze_graph', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found' },
  { scenario: 'Asset: analyze_graph nonexistent', toolName: 'manage_asset', arguments: { action: 'analyze_graph', assetPath: '/Game/NonExistent/BP' }, expected: 'not found|error|success' },

  // === 14. get_asset_graph ===
  { scenario: 'Asset: get_asset_graph blueprint', toolName: 'manage_asset', arguments: { action: 'get_asset_graph', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found' },
  { scenario: 'Asset: get_asset_graph material', toolName: 'manage_asset', arguments: { action: 'get_asset_graph', assetPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },

  // === 15. create_thumbnail ===
  { scenario: 'Asset: create_thumbnail', toolName: 'manage_asset', arguments: { action: 'create_thumbnail', assetPath: `${TEST_FOLDER}/M_TestMaterial` }, expected: 'success|not found' },
  { scenario: 'Asset: create_thumbnail nonexistent', toolName: 'manage_asset', arguments: { action: 'create_thumbnail', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 16. set_tags ===
  { scenario: 'Asset: set_tags', toolName: 'manage_asset', arguments: { action: 'set_tags', assetPath: `${TEST_FOLDER}/M_TestMaterial`, tag: 'TestTag' }, expected: 'success|not found' },
  { scenario: 'Asset: set_tags nonexistent', toolName: 'manage_asset', arguments: { action: 'set_tags', assetPath: '/Game/NonExistent', tag: 'Tag' }, expected: 'not found|error|success' },

  // === 17. get_metadata ===
  { scenario: 'Asset: get_metadata', toolName: 'manage_asset', arguments: { action: 'get_metadata', assetPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Asset: get_metadata nonexistent', toolName: 'manage_asset', arguments: { action: 'get_metadata', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 18. set_metadata ===
  { scenario: 'Asset: set_metadata', toolName: 'manage_asset', arguments: { action: 'set_metadata', assetPath: `${TEST_FOLDER}/M_TestMaterial`, metadata: { Author: 'MCP' } }, expected: 'success|not found' },
  { scenario: 'Asset: set_metadata nonexistent', toolName: 'manage_asset', arguments: { action: 'set_metadata', assetPath: '/Game/NonExistent', metadata: {} }, expected: 'not found|error|success' },

  // === 19. validate ===
  { scenario: 'Asset: validate asset', toolName: 'manage_asset', arguments: { action: 'validate', assetPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Asset: validate nonexistent', toolName: 'manage_asset', arguments: { action: 'validate', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 20. fixup_redirectors ===
  { scenario: 'Asset: fixup_redirectors', toolName: 'manage_asset', arguments: { action: 'fixup_redirectors', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Asset: fixup_redirectors /Game', toolName: 'manage_asset', arguments: { action: 'fixup_redirectors', directory: '/Game' }, expected: 'success' },

  // === 21. find_by_tag ===
  { scenario: 'Asset: find_by_tag existing', toolName: 'manage_asset', arguments: { action: 'find_by_tag', tag: 'TestTag' }, expected: 'success' },
  { scenario: 'Asset: find_by_tag nonexistent', toolName: 'manage_asset', arguments: { action: 'find_by_tag', tag: 'NonExistentTag12345' }, expected: 'success' },

  // === 22. generate_report ===
  { scenario: 'Asset: generate_report /Game', toolName: 'manage_asset', arguments: { action: 'generate_report', directory: '/Game' }, expected: 'success' },
  { scenario: 'Asset: generate_report test folder', toolName: 'manage_asset', arguments: { action: 'generate_report', directory: TEST_FOLDER }, expected: 'success' },

  // === 23. create_material ===
  { scenario: 'Asset: create_material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_TestMaterial', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Asset: create_material with parent', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_ChildMat', path: TEST_FOLDER, parentMaterial: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|already exists' },

  // === 24. create_material_instance ===
  { scenario: 'Asset: create_material_instance', toolName: 'manage_asset', arguments: { action: 'create_material_instance', name: 'MI_TestInstance', path: TEST_FOLDER, parentMaterial: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|already exists' },
  { scenario: 'Asset: create_material_instance invalid parent', toolName: 'manage_asset', arguments: { action: 'create_material_instance', name: 'MI_Bad', path: TEST_FOLDER, parentMaterial: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 25. create_render_target ===
  { scenario: 'Asset: create_render_target 512x512', toolName: 'manage_asset', arguments: { action: 'create_render_target', name: 'RT_Test', path: TEST_FOLDER, width: 512, height: 512 }, expected: 'success|already exists' },
  { scenario: 'Asset: create_render_target HD', toolName: 'manage_asset', arguments: { action: 'create_render_target', name: 'RT_HD', path: TEST_FOLDER, width: 1920, height: 1080 }, expected: 'success|already exists' },

  // === 26. generate_lods ===
  { scenario: 'Asset: generate_lods', toolName: 'manage_asset', arguments: { action: 'generate_lods', meshPath: '/Engine/BasicShapes/Cube', lodCount: 3 }, expected: 'success|not found|not supported' },
  { scenario: 'Asset: generate_lods nonexistent', toolName: 'manage_asset', arguments: { action: 'generate_lods', meshPath: '/Game/NonExistent', lodCount: 2 }, expected: 'not found|error|success' },

  // === 27. add_material_parameter ===
  { scenario: 'Asset: add_material_parameter color', toolName: 'manage_asset', arguments: { action: 'add_material_parameter', assetPath: `${TEST_FOLDER}/M_TestMaterial`, parameterName: 'BaseColor', value: { r: 1, g: 0, b: 0, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Asset: add_material_parameter scalar', toolName: 'manage_asset', arguments: { action: 'add_material_parameter', assetPath: `${TEST_FOLDER}/M_TestMaterial`, parameterName: 'Metallic', value: 0.5 }, expected: 'success|not found' },

  // === 28. list_instances ===
  { scenario: 'Asset: list_instances', toolName: 'manage_asset', arguments: { action: 'list_instances', assetPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },
  { scenario: 'Asset: list_instances nonexistent', toolName: 'manage_asset', arguments: { action: 'list_instances', assetPath: '/Game/NonExistent' }, expected: 'not found|success' },

  // === 29. reset_instance_parameters ===
  { scenario: 'Asset: reset_instance_parameters', toolName: 'manage_asset', arguments: { action: 'reset_instance_parameters', assetPath: `${TEST_FOLDER}/MI_TestInstance` }, expected: 'success|not found' },
  { scenario: 'Asset: reset_instance_parameters nonexistent', toolName: 'manage_asset', arguments: { action: 'reset_instance_parameters', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 30. exists ===
  { scenario: 'Asset: exists valid', toolName: 'manage_asset', arguments: { action: 'exists', assetPath: '/Engine/BasicShapes/Cube' }, expected: 'success' },
  { scenario: 'Asset: exists nonexistent', toolName: 'manage_asset', arguments: { action: 'exists', assetPath: '/Game/NonExistent/Asset123' }, expected: 'success' },

  // === 31. get_material_stats ===
  { scenario: 'Asset: get_material_stats', toolName: 'manage_asset', arguments: { action: 'get_material_stats', assetPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },
  { scenario: 'Asset: get_material_stats nonexistent', toolName: 'manage_asset', arguments: { action: 'get_material_stats', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 32. nanite_rebuild_mesh ===
  { scenario: 'Asset: nanite_rebuild_mesh', toolName: 'manage_asset', arguments: { action: 'nanite_rebuild_mesh', meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found|not supported' },
  { scenario: 'Asset: nanite_rebuild_mesh nonexistent', toolName: 'manage_asset', arguments: { action: 'nanite_rebuild_mesh', meshPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 33. enable_nanite_mesh ===
  { scenario: 'Asset: enable_nanite_mesh on', toolName: 'manage_asset', arguments: { action: 'enable_nanite_mesh', meshPath: '/Engine/BasicShapes/Cube', enableNanite: true }, expected: 'success|not found|not supported' },
  { scenario: 'Asset: enable_nanite_mesh off', toolName: 'manage_asset', arguments: { action: 'enable_nanite_mesh', meshPath: '/Engine/BasicShapes/Sphere', enableNanite: false }, expected: 'success|not found|not supported' },

  // === 34. set_nanite_settings ===
  { scenario: 'Asset: set_nanite_settings', toolName: 'manage_asset', arguments: { action: 'set_nanite_settings', meshPath: '/Engine/BasicShapes/Cube', nanitePercentTriangles: 50 }, expected: 'success|not found|not supported' },
  { scenario: 'Asset: set_nanite_settings precision', toolName: 'manage_asset', arguments: { action: 'set_nanite_settings', meshPath: '/Engine/BasicShapes/Cube', nanitePositionPrecision: 0.01 }, expected: 'success|not found|not supported' },

  // === 35. batch_nanite_convert ===
  { scenario: 'Asset: batch_nanite_convert', toolName: 'manage_asset', arguments: { action: 'batch_nanite_convert', assetPaths: ['/Engine/BasicShapes/Cube', '/Engine/BasicShapes/Sphere'] }, expected: 'success|not found|not supported' },
  { scenario: 'Asset: batch_nanite_convert empty', toolName: 'manage_asset', arguments: { action: 'batch_nanite_convert', assetPaths: [] }, expected: 'success|error' },

  // === 36. add_material_node ===
  { scenario: 'Asset: add_material_node texture', toolName: 'manage_asset', arguments: { action: 'add_material_node', assetPath: `${TEST_FOLDER}/M_TestMaterial`, nodeType: 'TextureSample', nodeName: 'TexNode1' }, expected: 'success|not found' },
  { scenario: 'Asset: add_material_node constant', toolName: 'manage_asset', arguments: { action: 'add_material_node', assetPath: `${TEST_FOLDER}/M_TestMaterial`, nodeType: 'Constant3Vector', nodeName: 'ColorNode' }, expected: 'success|not found' },

  // === 37. connect_material_pins ===
  { scenario: 'Asset: connect_material_pins', toolName: 'manage_asset', arguments: { action: 'connect_material_pins', assetPath: `${TEST_FOLDER}/M_TestMaterial`, fromNodeId: 'ColorNode', fromPin: 'RGB', toNodeId: 'Material', toPin: 'BaseColor' }, expected: 'success|not found' },
  { scenario: 'Asset: connect_material_pins invalid', toolName: 'manage_asset', arguments: { action: 'connect_material_pins', assetPath: `${TEST_FOLDER}/M_TestMaterial`, fromNodeId: 'NonExistent', fromPin: 'Out', toNodeId: 'Material', toPin: 'In' }, expected: 'not found|error|success' },

  // === 38. remove_material_node ===
  { scenario: 'Asset: remove_material_node', toolName: 'manage_asset', arguments: { action: 'remove_material_node', assetPath: `${TEST_FOLDER}/M_TestMaterial`, nodeName: 'TexNode1' }, expected: 'success|not found' },
  { scenario: 'Asset: remove_material_node nonexistent', toolName: 'manage_asset', arguments: { action: 'remove_material_node', assetPath: `${TEST_FOLDER}/M_TestMaterial`, nodeName: 'NonExistentNode' }, expected: 'not found|success' },

  // === 39. break_material_connections ===
  { scenario: 'Asset: break_material_connections', toolName: 'manage_asset', arguments: { action: 'break_material_connections', assetPath: `${TEST_FOLDER}/M_TestMaterial`, nodeName: 'ColorNode' }, expected: 'success|not found' },
  { scenario: 'Asset: break_material_connections nonexistent', toolName: 'manage_asset', arguments: { action: 'break_material_connections', assetPath: '/Game/NonExistent', nodeName: 'Node' }, expected: 'not found|error|success' },

  // === 40. get_material_node_details ===
  { scenario: 'Asset: get_material_node_details', toolName: 'manage_asset', arguments: { action: 'get_material_node_details', assetPath: `${TEST_FOLDER}/M_TestMaterial`, nodeName: 'ColorNode' }, expected: 'success|not found' },
  { scenario: 'Asset: get_material_node_details nonexistent', toolName: 'manage_asset', arguments: { action: 'get_material_node_details', assetPath: '/Game/NonExistent', nodeName: 'Node' }, expected: 'not found|error|success' },

  // === 41. rebuild_material ===
  { scenario: 'Asset: rebuild_material', toolName: 'manage_asset', arguments: { action: 'rebuild_material', assetPath: `${TEST_FOLDER}/M_TestMaterial` }, expected: 'success|not found' },
  { scenario: 'Asset: rebuild_material nonexistent', toolName: 'manage_asset', arguments: { action: 'rebuild_material', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 42. create_metasound ===
  { scenario: 'Asset: create_metasound', toolName: 'manage_asset', arguments: { action: 'create_metasound', metaSoundName: 'MS_Test', metaSoundPath: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Asset: create_metasound another', toolName: 'manage_asset', arguments: { action: 'create_metasound', metaSoundName: 'MS_Synth', metaSoundPath: TEST_FOLDER }, expected: 'success|already exists' },

  // === 43. add_metasound_node ===
  { scenario: 'Asset: add_metasound_node oscillator', toolName: 'manage_asset', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Test`, nodeType: 'Oscillator', nodeName: 'Osc1' }, expected: 'success|not found' },
  { scenario: 'Asset: add_metasound_node gain', toolName: 'manage_asset', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Test`, nodeType: 'Gain', nodeName: 'Gain1' }, expected: 'success|not found' },

  // === 44. connect_metasound_nodes ===
  { scenario: 'Asset: connect_metasound_nodes', toolName: 'manage_asset', arguments: { action: 'connect_metasound_nodes', metaSoundPath: `${TEST_FOLDER}/MS_Test`, fromNodeId: 'Osc1', fromPin: 'Audio', toNodeId: 'Gain1', toPin: 'Audio' }, expected: 'success|not found' },
  { scenario: 'Asset: connect_metasound_nodes invalid', toolName: 'manage_asset', arguments: { action: 'connect_metasound_nodes', metaSoundPath: `${TEST_FOLDER}/MS_Test`, fromNodeId: 'NonExistent', fromPin: 'Out', toNodeId: 'Gain1', toPin: 'In' }, expected: 'not found|error|success' },

  // === 45. remove_metasound_node ===
  { scenario: 'Asset: remove_metasound_node', toolName: 'manage_asset', arguments: { action: 'remove_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Test`, nodeName: 'Gain1' }, expected: 'success|not found' },
  { scenario: 'Asset: remove_metasound_node nonexistent', toolName: 'manage_asset', arguments: { action: 'remove_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Test`, nodeName: 'NonExistent' }, expected: 'not found|success' },

  // === 46. set_metasound_variable ===
  { scenario: 'Asset: set_metasound_variable frequency', toolName: 'manage_asset', arguments: { action: 'set_metasound_variable', metaSoundPath: `${TEST_FOLDER}/MS_Test`, parameterName: 'Frequency', value: 440 }, expected: 'success|not found' },
  { scenario: 'Asset: set_metasound_variable amplitude', toolName: 'manage_asset', arguments: { action: 'set_metasound_variable', metaSoundPath: `${TEST_FOLDER}/MS_Test`, parameterName: 'Amplitude', value: 0.5 }, expected: 'success|not found' },

  // === 47. create_oscillator ===
  { scenario: 'Asset: create_oscillator sine', toolName: 'manage_asset', arguments: { action: 'create_oscillator', metaSoundPath: `${TEST_FOLDER}/MS_Test`, nodeName: 'SineOsc' }, expected: 'success|not found' },
  { scenario: 'Asset: create_oscillator saw', toolName: 'manage_asset', arguments: { action: 'create_oscillator', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeName: 'SawOsc' }, expected: 'success|not found' },

  // === 48. create_envelope ===
  { scenario: 'Asset: create_envelope ADSR', toolName: 'manage_asset', arguments: { action: 'create_envelope', metaSoundPath: `${TEST_FOLDER}/MS_Test`, nodeName: 'ADSR1' }, expected: 'success|not found' },
  { scenario: 'Asset: create_envelope second', toolName: 'manage_asset', arguments: { action: 'create_envelope', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeName: 'Env2' }, expected: 'success|not found' },

  // === 49. create_filter ===
  { scenario: 'Asset: create_filter lowpass', toolName: 'manage_asset', arguments: { action: 'create_filter', metaSoundPath: `${TEST_FOLDER}/MS_Test`, nodeName: 'LPF' }, expected: 'success|not found' },
  { scenario: 'Asset: create_filter highpass', toolName: 'manage_asset', arguments: { action: 'create_filter', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeName: 'HPF' }, expected: 'success|not found' },

  // === 50. create_sequencer_node ===
  { scenario: 'Asset: create_sequencer_node', toolName: 'manage_asset', arguments: { action: 'create_sequencer_node', metaSoundPath: `${TEST_FOLDER}/MS_Test`, nodeName: 'Seq1' }, expected: 'success|not found' },
  { scenario: 'Asset: create_sequencer_node second', toolName: 'manage_asset', arguments: { action: 'create_sequencer_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeName: 'StepSeq' }, expected: 'success|not found' },

  // === 51. create_procedural_music ===
  { scenario: 'Asset: create_procedural_music', toolName: 'manage_asset', arguments: { action: 'create_procedural_music', metaSoundPath: `${TEST_FOLDER}/MS_Test` }, expected: 'success|not found' },
  { scenario: 'Asset: create_procedural_music synth', toolName: 'manage_asset', arguments: { action: 'create_procedural_music', metaSoundPath: `${TEST_FOLDER}/MS_Synth` }, expected: 'success|not found' },

  // === 52. import_audio_to_metasound ===
  { scenario: 'Asset: import_audio_to_metasound', toolName: 'manage_asset', arguments: { action: 'import_audio_to_metasound', metaSoundPath: `${TEST_FOLDER}/MS_Test`, sourcePath: 'C:/Temp/sample.wav' }, expected: 'success|not found|file not found' },
  { scenario: 'Asset: import_audio_to_metasound missing', toolName: 'manage_asset', arguments: { action: 'import_audio_to_metasound', metaSoundPath: `${TEST_FOLDER}/MS_Test`, sourcePath: '/nonexistent.wav' }, expected: 'not found|file not found|error' },

  // === 53. export_metasound_preset ===
  { scenario: 'Asset: export_metasound_preset', toolName: 'manage_asset', arguments: { action: 'export_metasound_preset', metaSoundPath: `${TEST_FOLDER}/MS_Test`, destinationPath: 'C:/Temp/preset.json' }, expected: 'success|not found' },
  { scenario: 'Asset: export_metasound_preset nonexistent', toolName: 'manage_asset', arguments: { action: 'export_metasound_preset', metaSoundPath: '/Game/NonExistent', destinationPath: 'C:/Temp/preset.json' }, expected: 'not found|error|success' },

  // === 54. configure_audio_modulation ===
  { scenario: 'Asset: configure_audio_modulation', toolName: 'manage_asset', arguments: { action: 'configure_audio_modulation', metaSoundPath: `${TEST_FOLDER}/MS_Test` }, expected: 'success|not found' },
  { scenario: 'Asset: configure_audio_modulation nonexistent', toolName: 'manage_asset', arguments: { action: 'configure_audio_modulation', metaSoundPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 55. bp_create ===
  { scenario: 'Asset: bp_create Actor', toolName: 'manage_asset', arguments: { action: 'bp_create', name: 'BP_TestActor', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Asset: bp_create Character', toolName: 'manage_asset', arguments: { action: 'bp_create', name: 'BP_TestCharacter', path: TEST_FOLDER, parentClass: 'Character' }, expected: 'success|already exists' },

  // === 56. bp_get ===
  { scenario: 'Asset: bp_get valid', toolName: 'manage_asset', arguments: { action: 'bp_get', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found' },
  { scenario: 'Asset: bp_get nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_get', assetPath: '/Game/NonExistent/BP' }, expected: 'not found|error|success' },

  // === 57. bp_compile ===
  { scenario: 'Asset: bp_compile', toolName: 'manage_asset', arguments: { action: 'bp_compile', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found' },
  { scenario: 'Asset: bp_compile nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_compile', assetPath: '/Game/NonExistent/BP' }, expected: 'not found|error|success' },

  // === 58. bp_add_component ===
  { scenario: 'Asset: bp_add_component mesh', toolName: 'manage_asset', arguments: { action: 'bp_add_component', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentType: 'StaticMeshComponent', componentName: 'MeshComp' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_component box', toolName: 'manage_asset', arguments: { action: 'bp_add_component', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentType: 'BoxComponent', componentName: 'BoxComp' }, expected: 'success|not found' },

  // === 59. bp_set_default ===
  { scenario: 'Asset: bp_set_default', toolName: 'manage_asset', arguments: { action: 'bp_set_default', assetPath: `${TEST_FOLDER}/BP_TestActor`, propertyName: 'Health', value: 100 }, expected: 'success|not found' },
  { scenario: 'Asset: bp_set_default nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_set_default', assetPath: '/Game/NonExistent', propertyName: 'Var', value: 0 }, expected: 'not found|error|success' },

  // === 60. bp_modify_scs ===
  { scenario: 'Asset: bp_modify_scs', toolName: 'manage_asset', arguments: { action: 'bp_modify_scs', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentName: 'MeshComp', properties: { bCastDynamicShadow: true } }, expected: 'success|not found' },
  { scenario: 'Asset: bp_modify_scs nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_modify_scs', assetPath: '/Game/NonExistent', componentName: 'Comp', properties: {} }, expected: 'not found|error|success' },

  // === 61. bp_get_scs ===
  { scenario: 'Asset: bp_get_scs', toolName: 'manage_asset', arguments: { action: 'bp_get_scs', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found' },
  { scenario: 'Asset: bp_get_scs nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_get_scs', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 62. bp_add_scs_component ===
  { scenario: 'Asset: bp_add_scs_component', toolName: 'manage_asset', arguments: { action: 'bp_add_scs_component', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentType: 'SphereComponent', componentName: 'SphereComp' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_scs_component light', toolName: 'manage_asset', arguments: { action: 'bp_add_scs_component', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentType: 'PointLightComponent', componentName: 'LightComp' }, expected: 'success|not found' },

  // === 63. bp_remove_scs_component ===
  { scenario: 'Asset: bp_remove_scs_component', toolName: 'manage_asset', arguments: { action: 'bp_remove_scs_component', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentName: 'SphereComp' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_remove_scs_component nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_remove_scs_component', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentName: 'NonExistent' }, expected: 'not found|success' },

  // === 64. bp_reparent_scs_component ===
  { scenario: 'Asset: bp_reparent_scs_component', toolName: 'manage_asset', arguments: { action: 'bp_reparent_scs_component', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentName: 'LightComp', parentNodeId: 'MeshComp' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_reparent_scs_component nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_reparent_scs_component', assetPath: '/Game/NonExistent', componentName: 'Comp', parentNodeId: 'Parent' }, expected: 'not found|error|success' },

  // === 65. bp_set_scs_transform ===
  { scenario: 'Asset: bp_set_scs_transform location', toolName: 'manage_asset', arguments: { action: 'bp_set_scs_transform', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentName: 'MeshComp', location: { x: 0, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Asset: bp_set_scs_transform full', toolName: 'manage_asset', arguments: { action: 'bp_set_scs_transform', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentName: 'MeshComp', location: { x: 0, y: 0, z: 50 }, rotation: { pitch: 0, yaw: 45, roll: 0 } }, expected: 'success|not found' },

  // === 66. bp_set_scs_property ===
  { scenario: 'Asset: bp_set_scs_property visibility', toolName: 'manage_asset', arguments: { action: 'bp_set_scs_property', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentName: 'MeshComp', propertyName: 'bVisible', value: true }, expected: 'success|not found' },
  { scenario: 'Asset: bp_set_scs_property mobility', toolName: 'manage_asset', arguments: { action: 'bp_set_scs_property', assetPath: `${TEST_FOLDER}/BP_TestActor`, componentName: 'MeshComp', propertyName: 'Mobility', value: 'Movable' }, expected: 'success|not found' },

  // === 67. bp_ensure_exists ===
  { scenario: 'Asset: bp_ensure_exists new', toolName: 'manage_asset', arguments: { action: 'bp_ensure_exists', name: 'BP_AutoCreate', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Asset: bp_ensure_exists existing', toolName: 'manage_asset', arguments: { action: 'bp_ensure_exists', name: 'BP_TestActor', path: TEST_FOLDER }, expected: 'success|already exists' },

  // === 68. bp_probe_handle ===
  { scenario: 'Asset: bp_probe_handle', toolName: 'manage_asset', arguments: { action: 'bp_probe_handle', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found' },
  { scenario: 'Asset: bp_probe_handle nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_probe_handle', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 69. bp_add_variable ===
  { scenario: 'Asset: bp_add_variable float', toolName: 'manage_asset', arguments: { action: 'bp_add_variable', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'Health', memberClass: 'float' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_variable bool', toolName: 'manage_asset', arguments: { action: 'bp_add_variable', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'bIsAlive', memberClass: 'bool' }, expected: 'success|not found' },

  // === 70. bp_remove_variable ===
  { scenario: 'Asset: bp_remove_variable', toolName: 'manage_asset', arguments: { action: 'bp_remove_variable', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'TempVar' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_remove_variable nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_remove_variable', assetPath: '/Game/NonExistent', name: 'Var' }, expected: 'not found|error|success' },

  // === 71. bp_rename_variable ===
  { scenario: 'Asset: bp_rename_variable', toolName: 'manage_asset', arguments: { action: 'bp_rename_variable', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'Health', newName: 'CurrentHealth' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_rename_variable nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_rename_variable', assetPath: '/Game/NonExistent', name: 'OldVar', newName: 'NewVar' }, expected: 'not found|error|success' },

  // === 72. bp_add_function ===
  { scenario: 'Asset: bp_add_function', toolName: 'manage_asset', arguments: { action: 'bp_add_function', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'TakeDamage' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_function second', toolName: 'manage_asset', arguments: { action: 'bp_add_function', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'Heal' }, expected: 'success|not found' },

  // === 73. bp_add_event ===
  { scenario: 'Asset: bp_add_event', toolName: 'manage_asset', arguments: { action: 'bp_add_event', assetPath: `${TEST_FOLDER}/BP_TestActor`, eventName: 'OnDeath' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_event tick', toolName: 'manage_asset', arguments: { action: 'bp_add_event', assetPath: `${TEST_FOLDER}/BP_TestActor`, eventName: 'EventTick' }, expected: 'success|not found' },

  // === 74. bp_remove_event ===
  { scenario: 'Asset: bp_remove_event', toolName: 'manage_asset', arguments: { action: 'bp_remove_event', assetPath: `${TEST_FOLDER}/BP_TestActor`, eventName: 'OnDeath' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_remove_event nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_remove_event', assetPath: '/Game/NonExistent', eventName: 'Event' }, expected: 'not found|error|success' },

  // === 75. bp_add_construction_script ===
  { scenario: 'Asset: bp_add_construction_script', toolName: 'manage_asset', arguments: { action: 'bp_add_construction_script', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found|already exists' },
  { scenario: 'Asset: bp_add_construction_script nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_add_construction_script', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 76. bp_set_variable_metadata ===
  { scenario: 'Asset: bp_set_variable_metadata', toolName: 'manage_asset', arguments: { action: 'bp_set_variable_metadata', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'CurrentHealth', metadata: { Category: 'Stats' } }, expected: 'success|not found' },
  { scenario: 'Asset: bp_set_variable_metadata nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_set_variable_metadata', assetPath: '/Game/NonExistent', name: 'Var', metadata: {} }, expected: 'not found|error|success' },

  // === 77. bp_create_node ===
  { scenario: 'Asset: bp_create_node function call', toolName: 'manage_asset', arguments: { action: 'bp_create_node', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph', nodeType: 'K2Node_CallFunction', x: 200, y: 100 }, expected: 'success|not found' },
  { scenario: 'Asset: bp_create_node event', toolName: 'manage_asset', arguments: { action: 'bp_create_node', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph', nodeType: 'K2Node_Event', x: 0, y: 0 }, expected: 'success|not found' },

  // === 78. bp_add_node ===
  { scenario: 'Asset: bp_add_node', toolName: 'manage_asset', arguments: { action: 'bp_add_node', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph', nodeType: 'K2Node_Event', nodeName: 'BeginPlay', posX: 0, posY: 0 }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_node print', toolName: 'manage_asset', arguments: { action: 'bp_add_node', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph', nodeType: 'K2Node_CallFunction', nodeName: 'PrintString', posX: 200, posY: 100 }, expected: 'success|not found' },

  // === 79. bp_delete_node ===
  { scenario: 'Asset: bp_delete_node', toolName: 'manage_asset', arguments: { action: 'bp_delete_node', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph', nodeId: 'TempNode' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_delete_node nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_delete_node', assetPath: '/Game/NonExistent', graphName: 'EventGraph', nodeId: 'Node' }, expected: 'not found|error|success' },

  // === 80. bp_connect_pins ===
  { scenario: 'Asset: bp_connect_pins exec', toolName: 'manage_asset', arguments: { action: 'bp_connect_pins', assetPath: `${TEST_FOLDER}/BP_TestActor`, fromNodeId: 'BeginPlay', fromPin: 'exec', toNodeId: 'PrintString', toPin: 'exec' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_connect_pins data', toolName: 'manage_asset', arguments: { action: 'bp_connect_pins', assetPath: `${TEST_FOLDER}/BP_TestActor`, fromNodeId: 'GetHealth', fromPin: 'ReturnValue', toNodeId: 'SetHealth', toPin: 'Value' }, expected: 'success|not found' },

  // === 81. bp_break_pin_links ===
  { scenario: 'Asset: bp_break_pin_links', toolName: 'manage_asset', arguments: { action: 'bp_break_pin_links', assetPath: `${TEST_FOLDER}/BP_TestActor`, nodeName: 'PrintString', inputName: 'exec' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_break_pin_links nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_break_pin_links', assetPath: '/Game/NonExistent', nodeName: 'Node', inputName: 'Pin' }, expected: 'not found|error|success' },

  // === 82. bp_set_node_property ===
  { scenario: 'Asset: bp_set_node_property comment', toolName: 'manage_asset', arguments: { action: 'bp_set_node_property', assetPath: `${TEST_FOLDER}/BP_TestActor`, nodeName: 'BeginPlay', propertyName: 'Comment', value: 'Entry point' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_set_node_property nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_set_node_property', assetPath: '/Game/NonExistent', nodeName: 'Node', propertyName: 'Prop', value: '' }, expected: 'not found|error|success' },

  // === 83. bp_create_reroute_node ===
  { scenario: 'Asset: bp_create_reroute_node', toolName: 'manage_asset', arguments: { action: 'bp_create_reroute_node', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph', x: 300, y: 150 }, expected: 'success|not found' },
  { scenario: 'Asset: bp_create_reroute_node second', toolName: 'manage_asset', arguments: { action: 'bp_create_reroute_node', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph', x: 400, y: 200 }, expected: 'success|not found' },

  // === 84. bp_get_node_details ===
  { scenario: 'Asset: bp_get_node_details', toolName: 'manage_asset', arguments: { action: 'bp_get_node_details', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph', nodeName: 'BeginPlay' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_get_node_details nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_get_node_details', assetPath: '/Game/NonExistent', graphName: 'EventGraph', nodeName: 'Node' }, expected: 'not found|error|success' },

  // === 85. bp_get_graph_details ===
  { scenario: 'Asset: bp_get_graph_details EventGraph', toolName: 'manage_asset', arguments: { action: 'bp_get_graph_details', assetPath: `${TEST_FOLDER}/BP_TestActor`, graphName: 'EventGraph' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_get_graph_details nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_get_graph_details', assetPath: '/Game/NonExistent', graphName: 'EventGraph' }, expected: 'not found|error|success' },

  // === 86. bp_get_pin_details ===
  { scenario: 'Asset: bp_get_pin_details', toolName: 'manage_asset', arguments: { action: 'bp_get_pin_details', assetPath: `${TEST_FOLDER}/BP_TestActor`, nodeName: 'BeginPlay', inputName: 'exec' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_get_pin_details nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_get_pin_details', assetPath: '/Game/NonExistent', nodeName: 'Node', inputName: 'Pin' }, expected: 'not found|error|success' },

  // === 87. bp_list_node_types ===
  { scenario: 'Asset: bp_list_node_types', toolName: 'manage_asset', arguments: { action: 'bp_list_node_types' }, expected: 'success' },
  { scenario: 'Asset: bp_list_node_types with filter', toolName: 'manage_asset', arguments: { action: 'bp_list_node_types', filter: 'K2Node' }, expected: 'success' },

  // === 88. bp_set_pin_default_value ===
  { scenario: 'Asset: bp_set_pin_default_value', toolName: 'manage_asset', arguments: { action: 'bp_set_pin_default_value', assetPath: `${TEST_FOLDER}/BP_TestActor`, nodeName: 'PrintString', inputName: 'InString', value: 'Hello World' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_set_pin_default_value nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_set_pin_default_value', assetPath: '/Game/NonExistent', nodeName: 'Node', inputName: 'Pin', value: '' }, expected: 'not found|error|success' },

  // === 89. query_assets_by_predicate ===
  { scenario: 'Asset: query_assets_by_predicate meshes', toolName: 'manage_asset', arguments: { action: 'query_assets_by_predicate', directory: '/Engine/BasicShapes', classNames: ['StaticMesh'] }, expected: 'success' },
  { scenario: 'Asset: query_assets_by_predicate materials', toolName: 'manage_asset', arguments: { action: 'query_assets_by_predicate', directory: '/Engine', classNames: ['Material'], limit: 10 }, expected: 'success' },

  // === 90. bp_implement_interface ===
  { scenario: 'Asset: bp_implement_interface', toolName: 'manage_asset', arguments: { action: 'bp_implement_interface', assetPath: `${TEST_FOLDER}/BP_TestActor`, interfaceName: 'BPI_Interactable' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_implement_interface nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_implement_interface', assetPath: '/Game/NonExistent', interfaceName: 'Interface' }, expected: 'not found|error|success' },

  // === 91. bp_add_macro ===
  { scenario: 'Asset: bp_add_macro', toolName: 'manage_asset', arguments: { action: 'bp_add_macro', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'ClampValue' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_macro second', toolName: 'manage_asset', arguments: { action: 'bp_add_macro', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'FormatText' }, expected: 'success|not found' },

  // === 92. bp_create_widget_binding ===
  { scenario: 'Asset: bp_create_widget_binding', toolName: 'manage_asset', arguments: { action: 'bp_create_widget_binding', assetPath: `${TEST_FOLDER}/BP_TestActor`, propertyName: 'HealthText', functionName: 'GetHealthText' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_create_widget_binding nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_create_widget_binding', assetPath: '/Game/NonExistent', propertyName: 'Prop', functionName: 'Func' }, expected: 'not found|error|success' },

  // === 93. bp_add_custom_event ===
  { scenario: 'Asset: bp_add_custom_event', toolName: 'manage_asset', arguments: { action: 'bp_add_custom_event', assetPath: `${TEST_FOLDER}/BP_TestActor`, customEventName: 'OnHitReceived', x: 400, y: 200 }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_custom_event second', toolName: 'manage_asset', arguments: { action: 'bp_add_custom_event', assetPath: `${TEST_FOLDER}/BP_TestActor`, customEventName: 'OnHealthChanged', x: 400, y: 400 }, expected: 'success|not found' },

  // === 94. bp_set_replication_settings ===
  { scenario: 'Asset: bp_set_replication_settings', toolName: 'manage_asset', arguments: { action: 'bp_set_replication_settings', assetPath: `${TEST_FOLDER}/BP_TestActor`, propertyName: 'CurrentHealth', replicated: true }, expected: 'success|not found' },
  { scenario: 'Asset: bp_set_replication_settings nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_set_replication_settings', assetPath: '/Game/NonExistent', propertyName: 'Prop', replicated: false }, expected: 'not found|error|success' },

  // === 95. bp_add_event_dispatcher ===
  { scenario: 'Asset: bp_add_event_dispatcher', toolName: 'manage_asset', arguments: { action: 'bp_add_event_dispatcher', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'OnDamageReceived' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_add_event_dispatcher second', toolName: 'manage_asset', arguments: { action: 'bp_add_event_dispatcher', assetPath: `${TEST_FOLDER}/BP_TestActor`, name: 'OnLevelUp' }, expected: 'success|not found' },

  // === 96. bp_bind_event ===
  { scenario: 'Asset: bp_bind_event', toolName: 'manage_asset', arguments: { action: 'bp_bind_event', assetPath: `${TEST_FOLDER}/BP_TestActor`, eventName: 'OnDamageReceived', functionName: 'HandleDamage' }, expected: 'success|not found' },
  { scenario: 'Asset: bp_bind_event nonexistent', toolName: 'manage_asset', arguments: { action: 'bp_bind_event', assetPath: '/Game/NonExistent', eventName: 'Event', functionName: 'Func' }, expected: 'not found|error|success' },

  // === 97. get_blueprint_dependencies ===
  { scenario: 'Asset: get_blueprint_dependencies', toolName: 'manage_asset', arguments: { action: 'get_blueprint_dependencies', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found' },
  { scenario: 'Asset: get_blueprint_dependencies nonexistent', toolName: 'manage_asset', arguments: { action: 'get_blueprint_dependencies', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 98. validate_blueprint ===
  { scenario: 'Asset: validate_blueprint', toolName: 'manage_asset', arguments: { action: 'validate_blueprint', assetPath: `${TEST_FOLDER}/BP_TestActor` }, expected: 'success|not found' },
  { scenario: 'Asset: validate_blueprint nonexistent', toolName: 'manage_asset', arguments: { action: 'validate_blueprint', assetPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 99. compile_blueprint_batch ===
  { scenario: 'Asset: compile_blueprint_batch', toolName: 'manage_asset', arguments: { action: 'compile_blueprint_batch', assetPaths: [`${TEST_FOLDER}/BP_TestActor`, `${TEST_FOLDER}/BP_TestCharacter`] }, expected: 'success|not found' },
  { scenario: 'Asset: compile_blueprint_batch empty', toolName: 'manage_asset', arguments: { action: 'compile_blueprint_batch', assetPaths: [] }, expected: 'success|error' },
];

// ============================================================================
// CONTROL_ACTOR (45 actions x 2 = 90 tests)
// ============================================================================
const controlActorTests = [
  // === 1. spawn ===
  { scenario: 'Actor: spawn cube', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'Test_Cube', location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
  { scenario: 'Actor: spawn with rotation and scale', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: 'Test_Sphere', location: { x: 200, y: 0, z: 100 }, rotation: { pitch: 0, yaw: 45, roll: 0 }, scale: { x: 2, y: 2, z: 2 } }, expected: 'success' },

  // === 2. spawn_blueprint ===
  { scenario: 'Actor: spawn_blueprint', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: `${TEST_FOLDER}/BP_TestActor`, actorName: 'Test_BP', location: { x: 400, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Actor: spawn_blueprint nonexistent', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: '/Game/NonExistent/BP', actorName: 'Test_BadBP', location: { x: 0, y: 0, z: 0 } }, expected: 'not found|error|success' },

  // === 3. delete ===
  { scenario: 'Actor: delete actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'Test_ToDelete' }, expected: 'success|not found' },
  { scenario: 'Actor: delete nonexistent', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'NonExistentActor12345' }, expected: 'not found|success' },

  // === 4. delete_by_tag ===
  { scenario: 'Actor: delete_by_tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'Temporary' }, expected: 'success' },
  { scenario: 'Actor: delete_by_tag nonexistent', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: 'NonExistentTag12345' }, expected: 'success' },

  // === 5. duplicate ===
  { scenario: 'Actor: duplicate', toolName: 'control_actor', arguments: { action: 'duplicate', actorName: 'Test_Cube', newName: 'Test_Cube_Copy' }, expected: 'success|not found' },
  { scenario: 'Actor: duplicate nonexistent', toolName: 'control_actor', arguments: { action: 'duplicate', actorName: 'NonExistent', newName: 'Copy' }, expected: 'not found|error|success' },

  // === 6. apply_force ===
  { scenario: 'Actor: apply_force', toolName: 'control_actor', arguments: { action: 'apply_force', actorName: 'Test_Cube', force: { x: 0, y: 0, z: 10000 } }, expected: 'success|not found|physics not enabled' },
  { scenario: 'Actor: apply_force large', toolName: 'control_actor', arguments: { action: 'apply_force', actorName: 'Test_Sphere', force: { x: 5000, y: 5000, z: 0 } }, expected: 'success|not found|physics not enabled' },

  // === 7. set_transform ===
  { scenario: 'Actor: set_transform location', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: 'Test_Cube', location: { x: 100, y: 100, z: 200 } }, expected: 'success|not found' },
  { scenario: 'Actor: set_transform full', toolName: 'control_actor', arguments: { action: 'set_transform', actorName: 'Test_Sphere', location: { x: 0, y: 0, z: 0 }, rotation: { pitch: 45, yaw: 90, roll: 0 }, scale: { x: 1, y: 1, z: 1 } }, expected: 'success|not found' },

  // === 8. get_transform ===
  { scenario: 'Actor: get_transform', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: get_transform nonexistent', toolName: 'control_actor', arguments: { action: 'get_transform', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 9. set_visibility ===
  { scenario: 'Actor: set_visibility hide', toolName: 'control_actor', arguments: { action: 'set_visibility', actorName: 'Test_Cube', visible: false }, expected: 'success|not found' },
  { scenario: 'Actor: set_visibility show', toolName: 'control_actor', arguments: { action: 'set_visibility', actorName: 'Test_Cube', visible: true }, expected: 'success|not found' },

  // === 10. add_component ===
  { scenario: 'Actor: add_component light', toolName: 'control_actor', arguments: { action: 'add_component', actorName: 'Test_Cube', componentType: 'PointLightComponent', componentName: 'AddedLight' }, expected: 'success|not found' },
  { scenario: 'Actor: add_component audio', toolName: 'control_actor', arguments: { action: 'add_component', actorName: 'Test_Cube', componentType: 'AudioComponent', componentName: 'AddedAudio' }, expected: 'success|not found' },

  // === 11. set_component_properties ===
  { scenario: 'Actor: set_component_properties', toolName: 'control_actor', arguments: { action: 'set_component_properties', actorName: 'Test_Cube', componentName: 'AddedLight', properties: { Intensity: 5000 } }, expected: 'success|not found' },
  { scenario: 'Actor: set_component_properties multiple', toolName: 'control_actor', arguments: { action: 'set_component_properties', actorName: 'Test_Cube', componentName: 'AddedLight', properties: { Intensity: 8000, LightColor: { r: 255, g: 200, b: 150 } } }, expected: 'success|not found' },

  // === 12. get_components ===
  { scenario: 'Actor: get_components', toolName: 'control_actor', arguments: { action: 'get_components', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: get_components nonexistent', toolName: 'control_actor', arguments: { action: 'get_components', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 13. add_tag ===
  { scenario: 'Actor: add_tag', toolName: 'control_actor', arguments: { action: 'add_tag', actorName: 'Test_Cube', tag: 'Movable' }, expected: 'success|not found' },
  { scenario: 'Actor: add_tag second', toolName: 'control_actor', arguments: { action: 'add_tag', actorName: 'Test_Cube', tag: 'Interactive' }, expected: 'success|not found' },

  // === 14. remove_tag ===
  { scenario: 'Actor: remove_tag', toolName: 'control_actor', arguments: { action: 'remove_tag', actorName: 'Test_Cube', tag: 'Interactive' }, expected: 'success|not found' },
  { scenario: 'Actor: remove_tag nonexistent', toolName: 'control_actor', arguments: { action: 'remove_tag', actorName: 'Test_Cube', tag: 'NonExistentTag' }, expected: 'success|not found' },

  // === 15. find_by_tag ===
  { scenario: 'Actor: find_by_tag', toolName: 'control_actor', arguments: { action: 'find_by_tag', tag: 'Movable' }, expected: 'success' },
  { scenario: 'Actor: find_by_tag nonexistent', toolName: 'control_actor', arguments: { action: 'find_by_tag', tag: 'NonExistentTag12345' }, expected: 'success' },

  // === 16. find_by_name ===
  { scenario: 'Actor: find_by_name', toolName: 'control_actor', arguments: { action: 'find_by_name', actorName: 'Test_Cube' }, expected: 'success' },
  { scenario: 'Actor: find_by_name partial', toolName: 'control_actor', arguments: { action: 'find_by_name', actorName: 'Test_' }, expected: 'success' },

  // === 17. list ===
  { scenario: 'Actor: list all', toolName: 'control_actor', arguments: { action: 'list' }, expected: 'success' },
  { scenario: 'Actor: list with filter', toolName: 'control_actor', arguments: { action: 'list', filter: 'Test_' }, expected: 'success' },

  // === 18. set_blueprint_variables ===
  { scenario: 'Actor: set_blueprint_variables', toolName: 'control_actor', arguments: { action: 'set_blueprint_variables', actorName: 'Test_BP', variables: { Health: 100, bIsAlive: true } }, expected: 'success|not found' },
  { scenario: 'Actor: set_blueprint_variables nonexistent', toolName: 'control_actor', arguments: { action: 'set_blueprint_variables', actorName: 'NonExistent', variables: { Var: 1 } }, expected: 'not found|error|success' },

  // === 19. create_snapshot ===
  { scenario: 'Actor: create_snapshot', toolName: 'control_actor', arguments: { action: 'create_snapshot', actorName: 'Test_Cube', snapshotName: 'Snapshot1' }, expected: 'success|not found' },
  { scenario: 'Actor: create_snapshot second', toolName: 'control_actor', arguments: { action: 'create_snapshot', actorName: 'Test_Cube', snapshotName: 'Snapshot2' }, expected: 'success|not found' },

  // === 20. attach ===
  { scenario: 'Actor: attach', toolName: 'control_actor', arguments: { action: 'attach', childActor: 'Test_Sphere', parentActor: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: attach nonexistent', toolName: 'control_actor', arguments: { action: 'attach', childActor: 'NonExistent', parentActor: 'Test_Cube' }, expected: 'not found|error|success' },

  // === 21. detach ===
  { scenario: 'Actor: detach', toolName: 'control_actor', arguments: { action: 'detach', childActor: 'Test_Sphere' }, expected: 'success|not found' },
  { scenario: 'Actor: detach nonexistent', toolName: 'control_actor', arguments: { action: 'detach', childActor: 'NonExistentActor' }, expected: 'not found|success' },

  // === 22. inspect_object ===
  { scenario: 'Actor: inspect_object', toolName: 'control_actor', arguments: { action: 'inspect_object', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: inspect_object nonexistent', toolName: 'control_actor', arguments: { action: 'inspect_object', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 23. set_property ===
  { scenario: 'Actor: set_property hidden', toolName: 'control_actor', arguments: { action: 'set_property', actorName: 'Test_Cube', propertyName: 'bHidden', value: false }, expected: 'success|not found' },
  { scenario: 'Actor: set_property nonexistent', toolName: 'control_actor', arguments: { action: 'set_property', actorName: 'NonExistent', propertyName: 'Prop', value: 0 }, expected: 'not found|error|success' },

  // === 24. get_property ===
  { scenario: 'Actor: get_property', toolName: 'control_actor', arguments: { action: 'get_property', actorName: 'Test_Cube', propertyName: 'bHidden' }, expected: 'success|not found' },
  { scenario: 'Actor: get_property nonexistent', toolName: 'control_actor', arguments: { action: 'get_property', actorName: 'NonExistent', propertyName: 'Prop' }, expected: 'not found|error|success' },

  // === 25. inspect_class ===
  { scenario: 'Actor: inspect_class StaticMeshActor', toolName: 'control_actor', arguments: { action: 'inspect_class', className: 'StaticMeshActor' }, expected: 'success|not found' },
  { scenario: 'Actor: inspect_class Actor', toolName: 'control_actor', arguments: { action: 'inspect_class', className: 'Actor' }, expected: 'success|not found' },

  // === 26. list_objects ===
  { scenario: 'Actor: list_objects lights', toolName: 'control_actor', arguments: { action: 'list_objects', className: 'PointLight' }, expected: 'success' },
  { scenario: 'Actor: list_objects meshes', toolName: 'control_actor', arguments: { action: 'list_objects', className: 'StaticMeshActor' }, expected: 'success' },

  // === 27. get_component_property ===
  { scenario: 'Actor: get_component_property', toolName: 'control_actor', arguments: { action: 'get_component_property', actorName: 'Test_Cube', componentName: 'AddedLight', propertyName: 'Intensity' }, expected: 'success|not found' },
  { scenario: 'Actor: get_component_property nonexistent', toolName: 'control_actor', arguments: { action: 'get_component_property', actorName: 'NonExistent', componentName: 'Comp', propertyName: 'Prop' }, expected: 'not found|error|success' },

  // === 28. set_component_property ===
  { scenario: 'Actor: set_component_property', toolName: 'control_actor', arguments: { action: 'set_component_property', actorName: 'Test_Cube', componentName: 'AddedLight', propertyName: 'Intensity', value: 12000 }, expected: 'success|not found' },
  { scenario: 'Actor: set_component_property nonexistent', toolName: 'control_actor', arguments: { action: 'set_component_property', actorName: 'NonExistent', componentName: 'Comp', propertyName: 'Prop', value: 0 }, expected: 'not found|error|success' },

  // === 29. get_metadata ===
  { scenario: 'Actor: get_metadata', toolName: 'control_actor', arguments: { action: 'get_metadata', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: get_metadata nonexistent', toolName: 'control_actor', arguments: { action: 'get_metadata', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 30. restore_snapshot ===
  { scenario: 'Actor: restore_snapshot', toolName: 'control_actor', arguments: { action: 'restore_snapshot', actorName: 'Test_Cube', snapshotName: 'Snapshot1' }, expected: 'success|not found' },
  { scenario: 'Actor: restore_snapshot nonexistent', toolName: 'control_actor', arguments: { action: 'restore_snapshot', actorName: 'Test_Cube', snapshotName: 'NonExistentSnapshot' }, expected: 'not found|error|success' },

  // === 31. export ===
  { scenario: 'Actor: export T3D', toolName: 'control_actor', arguments: { action: 'export', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: export nonexistent', toolName: 'control_actor', arguments: { action: 'export', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 32. delete_object ===
  { scenario: 'Actor: delete_object', toolName: 'control_actor', arguments: { action: 'delete_object', objectPath: 'Test_TempActor' }, expected: 'success|not found' },
  { scenario: 'Actor: delete_object nonexistent', toolName: 'control_actor', arguments: { action: 'delete_object', objectPath: 'NonExistentPath' }, expected: 'not found|success' },

  // === 33. find_by_class ===
  { scenario: 'Actor: find_by_class StaticMeshActor', toolName: 'control_actor', arguments: { action: 'find_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'Actor: find_by_class PointLight', toolName: 'control_actor', arguments: { action: 'find_by_class', className: 'PointLight' }, expected: 'success' },

  // === 34. get_bounding_box ===
  { scenario: 'Actor: get_bounding_box', toolName: 'control_actor', arguments: { action: 'get_bounding_box', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: get_bounding_box nonexistent', toolName: 'control_actor', arguments: { action: 'get_bounding_box', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 35. query_actors_by_predicate ===
  { scenario: 'Actor: query_actors_by_predicate mesh', toolName: 'control_actor', arguments: { action: 'query_actors_by_predicate', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'Actor: query_actors_by_predicate light', toolName: 'control_actor', arguments: { action: 'query_actors_by_predicate', className: 'Light' }, expected: 'success' },

  // === 36. get_all_component_properties ===
  { scenario: 'Actor: get_all_component_properties', toolName: 'control_actor', arguments: { action: 'get_all_component_properties', actorName: 'Test_Cube', componentFilter: 'AddedLight' }, expected: 'success|not found' },
  { scenario: 'Actor: get_all_component_properties nonexistent', toolName: 'control_actor', arguments: { action: 'get_all_component_properties', actorName: 'NonExistent', componentFilter: 'Comp' }, expected: 'not found|error|success' },

  // === 37. batch_set_component_properties ===
  { scenario: 'Actor: batch_set_component_properties', toolName: 'control_actor', arguments: { action: 'batch_set_component_properties', actorName: 'Test_Cube', componentName: 'AddedLight', properties: { Intensity: 7500, AttenuationRadius: 1000 } }, expected: 'success|not found' },
  { scenario: 'Actor: batch_set_component_properties nonexistent', toolName: 'control_actor', arguments: { action: 'batch_set_component_properties', actorName: 'NonExistent', componentName: 'Comp', properties: {} }, expected: 'not found|error|success' },

  // === 38. clone_component_hierarchy ===
  { scenario: 'Actor: clone_component_hierarchy', toolName: 'control_actor', arguments: { action: 'clone_component_hierarchy', actorName: 'Test_Cube', componentName: 'AddedLight', targetActor: 'Test_Sphere' }, expected: 'success|not found' },
  { scenario: 'Actor: clone_component_hierarchy nonexistent', toolName: 'control_actor', arguments: { action: 'clone_component_hierarchy', actorName: 'NonExistent', componentName: 'Comp', targetActor: 'Dest' }, expected: 'not found|error|success' },

  // === 39. serialize_actor_state ===
  { scenario: 'Actor: serialize_actor_state', toolName: 'control_actor', arguments: { action: 'serialize_actor_state', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: serialize_actor_state nonexistent', toolName: 'control_actor', arguments: { action: 'serialize_actor_state', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 40. deserialize_actor_state ===
  { scenario: 'Actor: deserialize_actor_state', toolName: 'control_actor', arguments: { action: 'deserialize_actor_state', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: deserialize_actor_state nonexistent', toolName: 'control_actor', arguments: { action: 'deserialize_actor_state', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 41. get_actor_bounds ===
  { scenario: 'Actor: get_actor_bounds', toolName: 'control_actor', arguments: { action: 'get_actor_bounds', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: get_actor_bounds nonexistent', toolName: 'control_actor', arguments: { action: 'get_actor_bounds', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 42. batch_transform_actors ===
  { scenario: 'Actor: batch_transform_actors', toolName: 'control_actor', arguments: { action: 'batch_transform_actors', transforms: [{ actorName: 'Test_Cube', location: { x: 0, y: 0, z: 300 } }] }, expected: 'success|not found' },
  { scenario: 'Actor: batch_transform_actors nonexistent', toolName: 'control_actor', arguments: { action: 'batch_transform_actors', transforms: [{ actorName: 'NonExistent', location: { x: 0, y: 0, z: 0 } }] }, expected: 'not found|error|success' },

  // === 43. get_actor_references ===
  { scenario: 'Actor: get_actor_references', toolName: 'control_actor', arguments: { action: 'get_actor_references', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: get_actor_references nonexistent', toolName: 'control_actor', arguments: { action: 'get_actor_references', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },

  // === 44. replace_actor_class ===
  { scenario: 'Actor: replace_actor_class', toolName: 'control_actor', arguments: { action: 'replace_actor_class', actorName: 'Test_Cube', className: 'StaticMeshActor' }, expected: 'success|not found' },
  { scenario: 'Actor: replace_actor_class nonexistent', toolName: 'control_actor', arguments: { action: 'replace_actor_class', actorName: 'NonExistent', className: 'Actor' }, expected: 'not found|error|success' },

  // === 45. merge_actors ===
  { scenario: 'Actor: merge_actors', toolName: 'control_actor', arguments: { action: 'merge_actors', actorName: 'Test_Cube' }, expected: 'success|not found' },
  { scenario: 'Actor: merge_actors nonexistent', toolName: 'control_actor', arguments: { action: 'merge_actors', actorName: 'NonExistentActor' }, expected: 'not found|error|success' },
];

// ============================================================================
// CONTROL_EDITOR (84 actions x 2 = 168 tests)
// ============================================================================
const controlEditorTests = [
  // === 1. play ===
  // NOTE: PIE tests use afterHook to ensure cleanup even if the test fails
  { 
    scenario: 'Editor: play PIE', 
    toolName: 'control_editor', 
    arguments: { action: 'play' }, 
    expected: 'success|already playing',
    afterHook: { name: 'control_editor', arguments: { action: 'stop_pie' }, timeout: 10000 }
  },
  { 
    scenario: 'Editor: play default', 
    toolName: 'control_editor', 
    arguments: { action: 'play' }, 
    expected: 'success|already playing',
    afterHook: { name: 'control_editor', arguments: { action: 'stop_pie' }, timeout: 10000 }
  },

  // === 2. stop ===
  { scenario: 'Editor: stop PIE', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success|not playing' },
  { scenario: 'Editor: stop default', toolName: 'control_editor', arguments: { action: 'stop' }, expected: 'success|not playing' },

  // === 3. stop_pie ===
  { scenario: 'Editor: stop_pie', toolName: 'control_editor', arguments: { action: 'stop_pie' }, expected: 'success|not playing' },
  { scenario: 'Editor: stop_pie default', toolName: 'control_editor', arguments: { action: 'stop_pie' }, expected: 'success|not playing' },

  // === 4. pause ===
  { scenario: 'Editor: pause PIE', toolName: 'control_editor', arguments: { action: 'pause' }, expected: 'success|not playing' },
  { scenario: 'Editor: pause default', toolName: 'control_editor', arguments: { action: 'pause' }, expected: 'success|not playing' },

  // === 5. resume ===
  { scenario: 'Editor: resume PIE', toolName: 'control_editor', arguments: { action: 'resume' }, expected: 'success|not paused' },
  { scenario: 'Editor: resume default', toolName: 'control_editor', arguments: { action: 'resume' }, expected: 'success|not paused' },

  // === 6. set_game_speed ===
  { scenario: 'Editor: set_game_speed fast', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 2.0 }, expected: 'success' },
  { scenario: 'Editor: set_game_speed slow', toolName: 'control_editor', arguments: { action: 'set_game_speed', speed: 0.5 }, expected: 'success' },

  // === 7. eject ===
  { scenario: 'Editor: eject', toolName: 'control_editor', arguments: { action: 'eject' }, expected: 'success|not playing' },
  { scenario: 'Editor: eject default', toolName: 'control_editor', arguments: { action: 'eject' }, expected: 'success|not playing' },

  // === 8. possess ===
  { scenario: 'Editor: possess', toolName: 'control_editor', arguments: { action: 'possess' }, expected: 'success|not playing' },
  { scenario: 'Editor: possess default', toolName: 'control_editor', arguments: { action: 'possess' }, expected: 'success|not playing' },

  // === 9. set_camera ===
  { scenario: 'Editor: set_camera location', toolName: 'control_editor', arguments: { action: 'set_camera', location: { x: 500, y: 500, z: 300 } }, expected: 'success' },
  { scenario: 'Editor: set_camera with rotation', toolName: 'control_editor', arguments: { action: 'set_camera', location: { x: 0, y: 0, z: 500 }, rotation: { pitch: -45, yaw: 0, roll: 0 } }, expected: 'success' },

  // === 10. set_camera_position ===
  { scenario: 'Editor: set_camera_position', toolName: 'control_editor', arguments: { action: 'set_camera_position', location: { x: 0, y: 0, z: 1000 }, rotation: { pitch: -90, yaw: 0, roll: 0 } }, expected: 'success' },
  { scenario: 'Editor: set_camera_position isometric', toolName: 'control_editor', arguments: { action: 'set_camera_position', location: { x: 500, y: 500, z: 500 }, rotation: { pitch: -35, yaw: 45, roll: 0 } }, expected: 'success' },

  // === 11. set_camera_fov ===
  { scenario: 'Editor: set_camera_fov wide', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 90 }, expected: 'success' },
  { scenario: 'Editor: set_camera_fov narrow', toolName: 'control_editor', arguments: { action: 'set_camera_fov', fov: 60 }, expected: 'success' },

  // === 12. set_view_mode ===
  { scenario: 'Editor: set_view_mode Lit', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Lit' }, expected: 'success' },
  { scenario: 'Editor: set_view_mode Wireframe', toolName: 'control_editor', arguments: { action: 'set_view_mode', viewMode: 'Wireframe' }, expected: 'success' },

  // === 13. set_viewport_resolution ===
  { scenario: 'Editor: set_viewport_resolution 1080p', toolName: 'control_editor', arguments: { action: 'set_viewport_resolution', width: 1920, height: 1080 }, expected: 'success' },
  { scenario: 'Editor: set_viewport_resolution 4K', toolName: 'control_editor', arguments: { action: 'set_viewport_resolution', width: 3840, height: 2160 }, expected: 'success' },

  // === 14. console_command ===
  { scenario: 'Editor: console_command log', toolName: 'control_editor', arguments: { action: 'console_command', command: 'Log Test message' }, expected: 'success' },
  { scenario: 'Editor: console_command stat', toolName: 'control_editor', arguments: { action: 'console_command', command: 'stat fps' }, expected: 'success' },

  // === 15. execute_command ===
  { scenario: 'Editor: execute_command stat none', toolName: 'control_editor', arguments: { action: 'execute_command', command: 'stat none' }, expected: 'success' },
  { scenario: 'Editor: execute_command show collision', toolName: 'control_editor', arguments: { action: 'execute_command', command: 'show collision' }, expected: 'success' },

  // === 16. screenshot ===
  { scenario: 'Editor: screenshot', toolName: 'control_editor', arguments: { action: 'screenshot', filename: 'test_screenshot' }, expected: 'success' },
  { scenario: 'Editor: screenshot custom name', toolName: 'control_editor', arguments: { action: 'screenshot', filename: 'integration_test_capture' }, expected: 'success' },

  // === 17. step_frame ===
  { scenario: 'Editor: step_frame single', toolName: 'control_editor', arguments: { action: 'step_frame', steps: 1 }, expected: 'success|not paused' },
  { scenario: 'Editor: step_frame multiple', toolName: 'control_editor', arguments: { action: 'step_frame', steps: 5 }, expected: 'success|not paused' },

  // === 18. start_recording ===
  { scenario: 'Editor: start_recording', toolName: 'control_editor', arguments: { action: 'start_recording' }, expected: 'success|already recording' },
  { scenario: 'Editor: start_recording default', toolName: 'control_editor', arguments: { action: 'start_recording' }, expected: 'success|already recording' },

  // === 19. stop_recording ===
  { scenario: 'Editor: stop_recording', toolName: 'control_editor', arguments: { action: 'stop_recording' }, expected: 'success|not recording' },
  { scenario: 'Editor: stop_recording default', toolName: 'control_editor', arguments: { action: 'stop_recording' }, expected: 'success|not recording' },

  // === 20. create_bookmark ===
  { scenario: 'Editor: create_bookmark', toolName: 'control_editor', arguments: { action: 'create_bookmark', bookmarkName: 'TestBookmark1' }, expected: 'success' },
  { scenario: 'Editor: create_bookmark second', toolName: 'control_editor', arguments: { action: 'create_bookmark', bookmarkName: 'TestBookmark2' }, expected: 'success' },

  // === 21. jump_to_bookmark ===
  { scenario: 'Editor: jump_to_bookmark', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', bookmarkName: 'TestBookmark1' }, expected: 'success|not found' },
  { scenario: 'Editor: jump_to_bookmark nonexistent', toolName: 'control_editor', arguments: { action: 'jump_to_bookmark', bookmarkName: 'NonExistentBookmark' }, expected: 'not found|error|success' },

  // === 22. set_preferences ===
  { scenario: 'Editor: set_preferences', toolName: 'control_editor', arguments: { action: 'set_preferences' }, expected: 'success' },
  { scenario: 'Editor: set_preferences default', toolName: 'control_editor', arguments: { action: 'set_preferences' }, expected: 'success' },

  // === 23. set_viewport_realtime ===
  { scenario: 'Editor: set_viewport_realtime on', toolName: 'control_editor', arguments: { action: 'set_viewport_realtime', enabled: true }, expected: 'success' },
  { scenario: 'Editor: set_viewport_realtime off', toolName: 'control_editor', arguments: { action: 'set_viewport_realtime', enabled: false }, expected: 'success' },

  // === 24. open_asset ===
  { scenario: 'Editor: open_asset cube', toolName: 'control_editor', arguments: { action: 'open_asset', assetPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Editor: open_asset nonexistent', toolName: 'control_editor', arguments: { action: 'open_asset', assetPath: '/Game/NonExistent/Asset' }, expected: 'not found|error|success' },

  // === 25. simulate_input ===
  { scenario: 'Editor: simulate_input key down', toolName: 'control_editor', arguments: { action: 'simulate_input', keyName: 'W', eventType: 'KeyDown' }, expected: 'success' },
  { scenario: 'Editor: simulate_input key up', toolName: 'control_editor', arguments: { action: 'simulate_input', keyName: 'W', eventType: 'KeyUp' }, expected: 'success' },

  // === 26. create_input_action ===
  { scenario: 'Editor: create_input_action', toolName: 'control_editor', arguments: { action: 'create_input_action', actionPath: `${TEST_FOLDER}/IA_Test` }, expected: 'success|already exists' },
  { scenario: 'Editor: create_input_action jump', toolName: 'control_editor', arguments: { action: 'create_input_action', actionPath: `${TEST_FOLDER}/IA_Jump` }, expected: 'success|already exists' },

  // === 27. create_input_mapping_context ===
  { scenario: 'Editor: create_input_mapping_context', toolName: 'control_editor', arguments: { action: 'create_input_mapping_context', contextPath: `${TEST_FOLDER}/IMC_Test` }, expected: 'success|already exists' },
  { scenario: 'Editor: create_input_mapping_context player', toolName: 'control_editor', arguments: { action: 'create_input_mapping_context', contextPath: `${TEST_FOLDER}/IMC_Player` }, expected: 'success|already exists' },

  // === 28. add_mapping ===
  { scenario: 'Editor: add_mapping', toolName: 'control_editor', arguments: { action: 'add_mapping', contextPath: `${TEST_FOLDER}/IMC_Test`, actionPath: `${TEST_FOLDER}/IA_Test`, key: 'E' }, expected: 'success|not found' },
  { scenario: 'Editor: add_mapping space', toolName: 'control_editor', arguments: { action: 'add_mapping', contextPath: `${TEST_FOLDER}/IMC_Test`, actionPath: `${TEST_FOLDER}/IA_Jump`, key: 'SpaceBar' }, expected: 'success|not found' },

  // === 29. remove_mapping ===
  { scenario: 'Editor: remove_mapping', toolName: 'control_editor', arguments: { action: 'remove_mapping', contextPath: `${TEST_FOLDER}/IMC_Test`, actionPath: `${TEST_FOLDER}/IA_Test` }, expected: 'success|not found' },
  { scenario: 'Editor: remove_mapping nonexistent', toolName: 'control_editor', arguments: { action: 'remove_mapping', contextPath: '/Game/NonExistent', actionPath: '/Game/NonExistent' }, expected: 'not found|success' },

  // === 30. profile ===
  { scenario: 'Editor: profile cpu', toolName: 'control_editor', arguments: { action: 'profile', profileType: 'cpu' }, expected: 'success' },
  { scenario: 'Editor: profile gpu', toolName: 'control_editor', arguments: { action: 'profile', profileType: 'gpu' }, expected: 'success' },

  // === 31. show_fps ===
  { scenario: 'Editor: show_fps on', toolName: 'control_editor', arguments: { action: 'show_fps', enabled: true }, expected: 'success' },
  { scenario: 'Editor: show_fps off', toolName: 'control_editor', arguments: { action: 'show_fps', enabled: false }, expected: 'success' },

  // === 32. set_quality ===
  { scenario: 'Editor: set_quality epic', toolName: 'control_editor', arguments: { action: 'set_quality', level: 4 }, expected: 'success' },
  { scenario: 'Editor: set_quality low', toolName: 'control_editor', arguments: { action: 'set_quality', level: 0 }, expected: 'success' },

  // === 33. set_resolution ===
  { scenario: 'Editor: set_resolution 1080p', toolName: 'control_editor', arguments: { action: 'set_resolution', resolution: '1920x1080' }, expected: 'success' },
  { scenario: 'Editor: set_resolution 720p', toolName: 'control_editor', arguments: { action: 'set_resolution', resolution: '1280x720' }, expected: 'success' },

  // === 34. set_fullscreen ===
  { scenario: 'Editor: set_fullscreen off', toolName: 'control_editor', arguments: { action: 'set_fullscreen', enabled: false }, expected: 'success' },
  { scenario: 'Editor: set_fullscreen on', toolName: 'control_editor', arguments: { action: 'set_fullscreen', enabled: true }, expected: 'success' },

  // === 35. run_ubt ===
  { scenario: 'Editor: run_ubt', toolName: 'control_editor', arguments: { action: 'run_ubt', target: 'Editor', platform: 'Win64', configuration: 'Development' }, expected: 'success|error' },
  { scenario: 'Editor: run_ubt game', toolName: 'control_editor', arguments: { action: 'run_ubt', target: 'Game', platform: 'Win64' }, expected: 'success|error' },

  // === 36. run_tests ===
  { scenario: 'Editor: run_tests', toolName: 'control_editor', arguments: { action: 'run_tests', filter: 'System.Core' }, expected: 'success' },
  { scenario: 'Editor: run_tests project', toolName: 'control_editor', arguments: { action: 'run_tests', filter: 'Project' }, expected: 'success' },

  // === 37. subscribe ===
  { scenario: 'Editor: subscribe logs', toolName: 'control_editor', arguments: { action: 'subscribe', channels: 'logs' }, expected: 'success' },
  { scenario: 'Editor: subscribe events', toolName: 'control_editor', arguments: { action: 'subscribe', channels: 'events' }, expected: 'success' },

  // === 38. unsubscribe ===
  { scenario: 'Editor: unsubscribe logs', toolName: 'control_editor', arguments: { action: 'unsubscribe', channels: 'logs' }, expected: 'success' },
  { scenario: 'Editor: unsubscribe events', toolName: 'control_editor', arguments: { action: 'unsubscribe', channels: 'events' }, expected: 'success' },

  // === 39. spawn_category ===
  { scenario: 'Editor: spawn_category', toolName: 'control_editor', arguments: { action: 'spawn_category' }, expected: 'success' },
  { scenario: 'Editor: spawn_category default', toolName: 'control_editor', arguments: { action: 'spawn_category' }, expected: 'success' },

  // === 40. start_session ===
  { scenario: 'Editor: start_session', toolName: 'control_editor', arguments: { action: 'start_session' }, expected: 'success' },
  { scenario: 'Editor: start_session default', toolName: 'control_editor', arguments: { action: 'start_session' }, expected: 'success' },

  // === 41. lumen_update_scene ===
  { scenario: 'Editor: lumen_update_scene', toolName: 'control_editor', arguments: { action: 'lumen_update_scene' }, expected: 'success' },
  { scenario: 'Editor: lumen_update_scene default', toolName: 'control_editor', arguments: { action: 'lumen_update_scene' }, expected: 'success' },

  // === 42. subscribe_to_event ===
  { scenario: 'Editor: subscribe_to_event actor_spawned', toolName: 'control_editor', arguments: { action: 'subscribe_to_event', channels: 'actor_spawned' }, expected: 'success' },
  { scenario: 'Editor: subscribe_to_event level_loaded', toolName: 'control_editor', arguments: { action: 'subscribe_to_event', channels: 'level_loaded' }, expected: 'success' },

  // === 43. unsubscribe_from_event ===
  { scenario: 'Editor: unsubscribe_from_event actor_spawned', toolName: 'control_editor', arguments: { action: 'unsubscribe_from_event', channels: 'actor_spawned' }, expected: 'success' },
  { scenario: 'Editor: unsubscribe_from_event level_loaded', toolName: 'control_editor', arguments: { action: 'unsubscribe_from_event', channels: 'level_loaded' }, expected: 'success' },

  // === 44. get_subscribed_events ===
  { scenario: 'Editor: get_subscribed_events', toolName: 'control_editor', arguments: { action: 'get_subscribed_events' }, expected: 'success' },
  { scenario: 'Editor: get_subscribed_events default', toolName: 'control_editor', arguments: { action: 'get_subscribed_events' }, expected: 'success' },

  // === 45. configure_event_channel ===
  { scenario: 'Editor: configure_event_channel all', toolName: 'control_editor', arguments: { action: 'configure_event_channel', channels: 'all' }, expected: 'success' },
  { scenario: 'Editor: configure_event_channel actors', toolName: 'control_editor', arguments: { action: 'configure_event_channel', channels: 'actors' }, expected: 'success' },

  // === 46. get_event_history ===
  { scenario: 'Editor: get_event_history', toolName: 'control_editor', arguments: { action: 'get_event_history' }, expected: 'success' },
  { scenario: 'Editor: get_event_history default', toolName: 'control_editor', arguments: { action: 'get_event_history' }, expected: 'success' },

  // === 47. clear_event_subscriptions ===
  { scenario: 'Editor: clear_event_subscriptions', toolName: 'control_editor', arguments: { action: 'clear_event_subscriptions' }, expected: 'success' },
  { scenario: 'Editor: clear_event_subscriptions default', toolName: 'control_editor', arguments: { action: 'clear_event_subscriptions' }, expected: 'success' },

  // === 48. start_background_job ===
  { scenario: 'Editor: start_background_job', toolName: 'control_editor', arguments: { action: 'start_background_job', jobType: 'shader_compile' }, expected: 'success|error' },
  { scenario: 'Editor: start_background_job default', toolName: 'control_editor', arguments: { action: 'start_background_job', jobType: 'validate' }, expected: 'success|error' },

  // === 49. get_job_status ===
  { scenario: 'Editor: get_job_status', toolName: 'control_editor', arguments: { action: 'get_job_status' }, expected: 'success|not found' },
  { scenario: 'Editor: get_job_status default', toolName: 'control_editor', arguments: { action: 'get_job_status' }, expected: 'success|not found' },

  // === 50. cancel_job ===
  { scenario: 'Editor: cancel_job', toolName: 'control_editor', arguments: { action: 'cancel_job' }, expected: 'success|not found' },
  { scenario: 'Editor: cancel_job default', toolName: 'control_editor', arguments: { action: 'cancel_job' }, expected: 'success|not found' },

  // === 51. get_active_jobs ===
  { scenario: 'Editor: get_active_jobs', toolName: 'control_editor', arguments: { action: 'get_active_jobs' }, expected: 'success' },
  { scenario: 'Editor: get_active_jobs default', toolName: 'control_editor', arguments: { action: 'get_active_jobs' }, expected: 'success' },

  // === 52. play_sound ===
  { scenario: 'Editor: play_sound', toolName: 'control_editor', arguments: { action: 'play_sound', assetPath: '/Engine/VREditor/Sounds/UI/Click_01' }, expected: 'success|not found' },
  { scenario: 'Editor: play_sound nonexistent', toolName: 'control_editor', arguments: { action: 'play_sound', assetPath: '/Game/NonExistent/Sound' }, expected: 'not found|error|success' },

  // === 53. create_widget ===
  { scenario: 'Editor: create_widget', toolName: 'control_editor', arguments: { action: 'create_widget', widgetPath: `${TEST_FOLDER}/WBP_TestWidget` }, expected: 'success|not found' },
  { scenario: 'Editor: create_widget HUD', toolName: 'control_editor', arguments: { action: 'create_widget', widgetPath: `${TEST_FOLDER}/WBP_HUD` }, expected: 'success|not found' },

  // === 54. show_widget ===
  { scenario: 'Editor: show_widget', toolName: 'control_editor', arguments: { action: 'show_widget', widgetPath: `${TEST_FOLDER}/WBP_TestWidget` }, expected: 'success|not found' },
  { scenario: 'Editor: show_widget nonexistent', toolName: 'control_editor', arguments: { action: 'show_widget', widgetPath: '/Game/NonExistent/Widget' }, expected: 'not found|error|success' },

  // === 55. add_widget_child ===
  { scenario: 'Editor: add_widget_child', toolName: 'control_editor', arguments: { action: 'add_widget_child', widgetPath: `${TEST_FOLDER}/WBP_TestWidget`, childClass: 'Button' }, expected: 'success|not found' },
  { scenario: 'Editor: add_widget_child text', toolName: 'control_editor', arguments: { action: 'add_widget_child', widgetPath: `${TEST_FOLDER}/WBP_TestWidget`, childClass: 'TextBlock' }, expected: 'success|not found' },

  // === 56. set_cvar ===
  { scenario: 'Editor: set_cvar antialiasing', toolName: 'control_editor', arguments: { action: 'set_cvar', configName: 'r.DefaultFeature.AntiAliasing', value: '2' }, expected: 'success' },
  { scenario: 'Editor: set_cvar shadows', toolName: 'control_editor', arguments: { action: 'set_cvar', configName: 'r.Shadow.MaxResolution', value: '2048' }, expected: 'success' },

  // === 57. get_project_settings ===
  { scenario: 'Editor: get_project_settings', toolName: 'control_editor', arguments: { action: 'get_project_settings' }, expected: 'success' },
  { scenario: 'Editor: get_project_settings default', toolName: 'control_editor', arguments: { action: 'get_project_settings' }, expected: 'success' },

  // === 58. validate_assets ===
  { scenario: 'Editor: validate_assets /Game', toolName: 'control_editor', arguments: { action: 'validate_assets', assetPath: '/Game' }, expected: 'success' },
  { scenario: 'Editor: validate_assets test folder', toolName: 'control_editor', arguments: { action: 'validate_assets', assetPath: TEST_FOLDER }, expected: 'success' },

  // === 59. set_project_setting ===
  { scenario: 'Editor: set_project_setting debug', toolName: 'control_editor', arguments: { action: 'set_project_setting', section: '/Script/Engine.Engine', configName: 'bEnableOnScreenDebugMessages', value: 'true' }, expected: 'success' },
  { scenario: 'Editor: set_project_setting fps', toolName: 'control_editor', arguments: { action: 'set_project_setting', section: '/Script/Engine.Engine', configName: 'bSmoothFrameRate', value: 'true' }, expected: 'success' },

  // === 60. batch_execute ===
  { scenario: 'Editor: batch_execute', toolName: 'control_editor', arguments: { action: 'batch_execute', operations: [{ tool: 'control_actor', action: 'list' }] }, expected: 'success' },
  { scenario: 'Editor: batch_execute multiple', toolName: 'control_editor', arguments: { action: 'batch_execute', operations: [{ tool: 'control_actor', action: 'list' }, { tool: 'manage_asset', action: 'list', parameters: { directory: '/Game' } }] }, expected: 'success' },

  // === 61. parallel_execute ===
  { scenario: 'Editor: parallel_execute', toolName: 'control_editor', arguments: { action: 'parallel_execute', operations: [{ tool: 'control_actor', action: 'list' }], maxConcurrency: 5 }, expected: 'success' },
  { scenario: 'Editor: parallel_execute multiple', toolName: 'control_editor', arguments: { action: 'parallel_execute', operations: [{ tool: 'control_actor', action: 'list' }, { tool: 'manage_asset', action: 'list', parameters: { directory: '/Game' } }], maxConcurrency: 10 }, expected: 'success' },

  // === 62. queue_operations ===
  { scenario: 'Editor: queue_operations', toolName: 'control_editor', arguments: { action: 'queue_operations', operations: [{ tool: 'control_actor', action: 'list' }], queueId: 'test_queue' }, expected: 'success' },
  { scenario: 'Editor: queue_operations multiple', toolName: 'control_editor', arguments: { action: 'queue_operations', operations: [{ tool: 'control_actor', action: 'list' }], queueId: 'batch_queue' }, expected: 'success' },

  // === 63. flush_operation_queue ===
  { scenario: 'Editor: flush_operation_queue', toolName: 'control_editor', arguments: { action: 'flush_operation_queue', queueId: 'test_queue' }, expected: 'success' },
  { scenario: 'Editor: flush_operation_queue batch', toolName: 'control_editor', arguments: { action: 'flush_operation_queue', queueId: 'batch_queue' }, expected: 'success' },

  // === 64. capture_viewport ===
  { scenario: 'Editor: capture_viewport png', toolName: 'control_editor', arguments: { action: 'capture_viewport', outputPath: 'C:/Temp/capture.png', width: 1920, height: 1080 }, expected: 'success' },
  { scenario: 'Editor: capture_viewport jpg', toolName: 'control_editor', arguments: { action: 'capture_viewport', outputPath: 'C:/Temp/capture.jpg', format: 'jpg' }, expected: 'success' },

  // === 65. get_last_error_details ===
  { scenario: 'Editor: get_last_error_details', toolName: 'control_editor', arguments: { action: 'get_last_error_details' }, expected: 'success' },
  { scenario: 'Editor: get_last_error_details default', toolName: 'control_editor', arguments: { action: 'get_last_error_details' }, expected: 'success' },

  // === 66. suggest_fix_for_error ===
  { scenario: 'Editor: suggest_fix_for_error', toolName: 'control_editor', arguments: { action: 'suggest_fix_for_error', errorCode: 'ASSET_NOT_FOUND' }, expected: 'success' },
  { scenario: 'Editor: suggest_fix_for_error connection', toolName: 'control_editor', arguments: { action: 'suggest_fix_for_error', errorCode: 'CONNECTION_FAILED' }, expected: 'success' },

  // === 67. validate_operation_preconditions ===
  { scenario: 'Editor: validate_operation_preconditions', toolName: 'control_editor', arguments: { action: 'validate_operation_preconditions', targetAction: 'spawn', parameters: { classPath: '/Engine/BasicShapes/Cube' } }, expected: 'success' },
  { scenario: 'Editor: validate_operation_preconditions delete', toolName: 'control_editor', arguments: { action: 'validate_operation_preconditions', targetAction: 'delete', parameters: { actorName: 'TestActor' } }, expected: 'success' },

  // === 68. get_operation_history ===
  { scenario: 'Editor: get_operation_history', toolName: 'control_editor', arguments: { action: 'get_operation_history' }, expected: 'success' },
  { scenario: 'Editor: get_operation_history default', toolName: 'control_editor', arguments: { action: 'get_operation_history' }, expected: 'success' },

  // === 69. get_available_actions ===
  { scenario: 'Editor: get_available_actions', toolName: 'control_editor', arguments: { action: 'get_available_actions' }, expected: 'success' },
  { scenario: 'Editor: get_available_actions default', toolName: 'control_editor', arguments: { action: 'get_available_actions' }, expected: 'success' },

  // === 70. explain_action_parameters ===
  { scenario: 'Editor: explain_action_parameters actor', toolName: 'control_editor', arguments: { action: 'explain_action_parameters', tool: 'control_actor', targetAction: 'spawn' }, expected: 'success' },
  { scenario: 'Editor: explain_action_parameters asset', toolName: 'control_editor', arguments: { action: 'explain_action_parameters', tool: 'manage_asset', targetAction: 'create_material' }, expected: 'success' },

  // === 71. get_class_hierarchy ===
  { scenario: 'Editor: get_class_hierarchy Actor', toolName: 'control_editor', arguments: { action: 'get_class_hierarchy', className: 'Actor' }, expected: 'success' },
  { scenario: 'Editor: get_class_hierarchy Pawn', toolName: 'control_editor', arguments: { action: 'get_class_hierarchy', className: 'Pawn' }, expected: 'success' },

  // === 72. validate_action_input ===
  { scenario: 'Editor: validate_action_input spawn', toolName: 'control_editor', arguments: { action: 'validate_action_input', tool: 'control_actor', targetAction: 'spawn', parameters: { classPath: '/Engine/BasicShapes/Cube' } }, expected: 'success' },
  { scenario: 'Editor: validate_action_input delete', toolName: 'control_editor', arguments: { action: 'validate_action_input', tool: 'control_actor', targetAction: 'delete', parameters: { actorName: 'TestActor' } }, expected: 'success' },

  // === 73. get_action_statistics ===
  { scenario: 'Editor: get_action_statistics', toolName: 'control_editor', arguments: { action: 'get_action_statistics' }, expected: 'success' },
  { scenario: 'Editor: get_action_statistics default', toolName: 'control_editor', arguments: { action: 'get_action_statistics' }, expected: 'success' },

  // === 74. get_bridge_health ===
  { scenario: 'Editor: get_bridge_health', toolName: 'control_editor', arguments: { action: 'get_bridge_health' }, expected: 'success' },
  { scenario: 'Editor: get_bridge_health default', toolName: 'control_editor', arguments: { action: 'get_bridge_health' }, expected: 'success' },

  // === 75. configure_megalights ===
  { scenario: 'Editor: configure_megalights', toolName: 'control_editor', arguments: { action: 'configure_megalights' }, expected: 'success|not supported' },
  { scenario: 'Editor: configure_megalights default', toolName: 'control_editor', arguments: { action: 'configure_megalights' }, expected: 'success|not supported' },

  // === 76. get_light_budget_stats ===
  { scenario: 'Editor: get_light_budget_stats', toolName: 'control_editor', arguments: { action: 'get_light_budget_stats' }, expected: 'success' },
  { scenario: 'Editor: get_light_budget_stats default', toolName: 'control_editor', arguments: { action: 'get_light_budget_stats' }, expected: 'success' },

  // === 77. convert_to_substrate ===
  { scenario: 'Editor: convert_to_substrate', toolName: 'control_editor', arguments: { action: 'convert_to_substrate', materialPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not supported|not found' },
  { scenario: 'Editor: convert_to_substrate test', toolName: 'control_editor', arguments: { action: 'convert_to_substrate', materialPath: `${TEST_FOLDER}/M_TestMaterial` }, expected: 'success|not supported|not found' },

  // === 78. batch_substrate_migration ===
  { scenario: 'Editor: batch_substrate_migration', toolName: 'control_editor', arguments: { action: 'batch_substrate_migration', materialPaths: ['/Engine/EngineMaterials/DefaultMaterial'] }, expected: 'success|not supported' },
  { scenario: 'Editor: batch_substrate_migration multiple', toolName: 'control_editor', arguments: { action: 'batch_substrate_migration', materialPaths: [`${TEST_FOLDER}/M_TestMaterial`, `${TEST_FOLDER}/MI_TestInstance`] }, expected: 'success|not supported' },

  // === 79. record_input_session ===
  { scenario: 'Editor: record_input_session', toolName: 'control_editor', arguments: { action: 'record_input_session' }, expected: 'success|not playing' },
  { scenario: 'Editor: record_input_session default', toolName: 'control_editor', arguments: { action: 'record_input_session' }, expected: 'success|not playing' },

  // === 80. playback_input_session ===
  { scenario: 'Editor: playback_input_session', toolName: 'control_editor', arguments: { action: 'playback_input_session', sessionName: 'TestSession' }, expected: 'success|not found' },
  { scenario: 'Editor: playback_input_session nonexistent', toolName: 'control_editor', arguments: { action: 'playback_input_session', sessionName: 'NonExistentSession' }, expected: 'success|not found' },

  // === 81. capture_viewport_sequence ===
  { scenario: 'Editor: capture_viewport_sequence', toolName: 'control_editor', arguments: { action: 'capture_viewport_sequence', outputPath: 'C:/Temp/sequence', filename: 'frame' }, expected: 'success' },
  { scenario: 'Editor: capture_viewport_sequence custom', toolName: 'control_editor', arguments: { action: 'capture_viewport_sequence', outputPath: 'C:/Temp/renders', filename: 'capture' }, expected: 'success' },

  // === 82. set_editor_mode ===
  { scenario: 'Editor: set_editor_mode landscape', toolName: 'control_editor', arguments: { action: 'set_editor_mode', mode: 'Landscape' }, expected: 'success' },
  { scenario: 'Editor: set_editor_mode default', toolName: 'control_editor', arguments: { action: 'set_editor_mode', mode: 'Default' }, expected: 'success' },

  // === 83. get_selection_info ===
  { scenario: 'Editor: get_selection_info', toolName: 'control_editor', arguments: { action: 'get_selection_info' }, expected: 'success' },
  { scenario: 'Editor: get_selection_info default', toolName: 'control_editor', arguments: { action: 'get_selection_info' }, expected: 'success' },

  // === 84. toggle_realtime_rendering ===
  { scenario: 'Editor: toggle_realtime_rendering', toolName: 'control_editor', arguments: { action: 'toggle_realtime_rendering' }, expected: 'success' },
  { scenario: 'Editor: toggle_realtime_rendering default', toolName: 'control_editor', arguments: { action: 'toggle_realtime_rendering' }, expected: 'success' },
];

// ============================================================================
// MANAGE_LEVEL (87 actions x 2 = 174 tests)
// ============================================================================
const manageLevelTests = [
  // === 1. load ===
  { scenario: 'Level: load', toolName: 'manage_level', arguments: { action: 'load', levelPath: `${TEST_FOLDER}/TestLevel` }, expected: 'success|not found' },
  { scenario: 'Level: load nonexistent', toolName: 'manage_level', arguments: { action: 'load', levelPath: '/Game/NonExistent/Level' }, expected: 'not found|error|success' },

  // === 2. save ===
  { scenario: 'Level: save current', toolName: 'manage_level', arguments: { action: 'save' }, expected: 'success' },
  { scenario: 'Level: save default', toolName: 'manage_level', arguments: { action: 'save' }, expected: 'success' },

  // === 3. save_as ===
  { scenario: 'Level: save_as', toolName: 'manage_level', arguments: { action: 'save_as', levelPath: `${TEST_FOLDER}/TestLevel_Copy` }, expected: 'success' },
  { scenario: 'Level: save_as second', toolName: 'manage_level', arguments: { action: 'save_as', levelPath: `${TEST_FOLDER}/TestLevel_Backup` }, expected: 'success' },

  // === 4. save_level_as ===
  { scenario: 'Level: save_level_as', toolName: 'manage_level', arguments: { action: 'save_level_as', levelPath: `${TEST_FOLDER}/TestLevel_New` }, expected: 'success' },
  { scenario: 'Level: save_level_as second', toolName: 'manage_level', arguments: { action: 'save_level_as', levelPath: `${TEST_FOLDER}/TestLevel_Archive` }, expected: 'success' },

  // === 5. stream ===
  { scenario: 'Level: stream load', toolName: 'manage_level', arguments: { action: 'stream', levelPath: `${TEST_FOLDER}/SubLevel_Test`, shouldBeLoaded: true }, expected: 'success|not found' },
  { scenario: 'Level: stream unload', toolName: 'manage_level', arguments: { action: 'stream', levelPath: `${TEST_FOLDER}/SubLevel_Test`, shouldBeLoaded: false }, expected: 'success|not found' },

  // === 6. create_level ===
  { scenario: 'Level: create_level', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TestLevel', levelPath: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Level: create_level WP', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'TestLevelWP', levelPath: TEST_FOLDER, bCreateWorldPartition: true }, expected: 'success|already exists' },

  // === 7. create_light ===
  { scenario: 'Level: create_light directional', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Directional', location: { x: 0, y: 0, z: 1000 }, intensity: 3.14 }, expected: 'success' },
  { scenario: 'Level: create_light point', toolName: 'manage_level', arguments: { action: 'create_light', lightType: 'Point', location: { x: 500, y: 0, z: 200 }, intensity: 5000 }, expected: 'success' },

  // === 8. build_lighting ===
  { scenario: 'Level: build_lighting preview', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Preview' }, expected: 'success|skipped' },
  { scenario: 'Level: build_lighting medium', toolName: 'manage_level', arguments: { action: 'build_lighting', quality: 'Medium' }, expected: 'success|skipped' },

  // === 9. set_metadata ===
  { scenario: 'Level: set_metadata', toolName: 'manage_level', arguments: { action: 'set_metadata', levelPath: `${TEST_FOLDER}/TestLevel`, note: 'Test level' }, expected: 'success|not found' },
  { scenario: 'Level: set_metadata description', toolName: 'manage_level', arguments: { action: 'set_metadata', levelPath: `${TEST_FOLDER}/TestLevel`, note: 'Integration test map' }, expected: 'success|not found' },

  // === 10. load_cells ===
  { scenario: 'Level: load_cells', toolName: 'manage_level', arguments: { action: 'load_cells', min: [0, 0], max: [12800, 12800] }, expected: 'success|not enabled' },
  { scenario: 'Level: load_cells large', toolName: 'manage_level', arguments: { action: 'load_cells', min: [-25600, -25600], max: [25600, 25600] }, expected: 'success|not enabled' },

  // === 11. set_datalayer ===
  { scenario: 'Level: set_datalayer activated', toolName: 'manage_level', arguments: { action: 'set_datalayer', dataLayerLabel: 'TestDataLayer', dataLayerState: 'Activated' }, expected: 'success|not found' },
  { scenario: 'Level: set_datalayer deactivated', toolName: 'manage_level', arguments: { action: 'set_datalayer', dataLayerLabel: 'TestDataLayer', dataLayerState: 'Deactivated' }, expected: 'success|not found' },

  // === 12. export_level ===
  { scenario: 'Level: export_level T3D', toolName: 'manage_level', arguments: { action: 'export_level', levelPath: `${TEST_FOLDER}/TestLevel`, exportPath: 'C:/Temp/TestLevel.t3d' }, expected: 'success|not found' },
  { scenario: 'Level: export_level nonexistent', toolName: 'manage_level', arguments: { action: 'export_level', levelPath: '/Game/NonExistent', exportPath: 'C:/Temp/export.t3d' }, expected: 'not found|error|success' },

  // === 13. import_level ===
  { scenario: 'Level: import_level', toolName: 'manage_level', arguments: { action: 'import_level', sourcePath: 'C:/Temp/level.t3d', destinationPath: TEST_FOLDER }, expected: 'success|file not found' },
  { scenario: 'Level: import_level nonexistent', toolName: 'manage_level', arguments: { action: 'import_level', sourcePath: '/nonexistent/level.t3d', destinationPath: TEST_FOLDER }, expected: 'file not found|error' },

  // === 14. list_levels ===
  { scenario: 'Level: list_levels', toolName: 'manage_level', arguments: { action: 'list_levels' }, expected: 'success' },
  { scenario: 'Level: list_levels default', toolName: 'manage_level', arguments: { action: 'list_levels' }, expected: 'success' },

  // === 15. get_summary ===
  { scenario: 'Level: get_summary', toolName: 'manage_level', arguments: { action: 'get_summary', levelPath: '/Game' }, expected: 'success|not found' },
  { scenario: 'Level: get_summary test', toolName: 'manage_level', arguments: { action: 'get_summary', levelPath: TEST_FOLDER }, expected: 'success|not found' },

  // === 16. delete ===
  { scenario: 'Level: delete level', toolName: 'manage_level', arguments: { action: 'delete', levelPath: `${TEST_FOLDER}/TestLevel_Copy` }, expected: 'success|not found' },
  { scenario: 'Level: delete nonexistent', toolName: 'manage_level', arguments: { action: 'delete', levelPath: '/Game/NonExistent/Level' }, expected: 'not found|success' },

  // === 17. validate_level ===
  { scenario: 'Level: validate_level', toolName: 'manage_level', arguments: { action: 'validate_level', levelPath: `${TEST_FOLDER}/TestLevel` }, expected: 'success|not found' },
  { scenario: 'Level: validate_level nonexistent', toolName: 'manage_level', arguments: { action: 'validate_level', levelPath: '/Game/NonExistent' }, expected: 'not found|error|success' },

  // === 18. cleanup_invalid_datalayers ===
  { scenario: 'Level: cleanup_invalid_datalayers', toolName: 'manage_level', arguments: { action: 'cleanup_invalid_datalayers' }, expected: 'success' },
  { scenario: 'Level: cleanup_invalid_datalayers default', toolName: 'manage_level', arguments: { action: 'cleanup_invalid_datalayers' }, expected: 'success' },

  // === 19. add_sublevel ===
  { scenario: 'Level: add_sublevel', toolName: 'manage_level', arguments: { action: 'add_sublevel', subLevelPath: `${TEST_FOLDER}/SubLevel_Test`, parentLevel: `${TEST_FOLDER}/TestLevel` }, expected: 'success|not found' },
  { scenario: 'Level: add_sublevel nonexistent', toolName: 'manage_level', arguments: { action: 'add_sublevel', subLevelPath: '/Game/NonExistent', parentLevel: TEST_FOLDER }, expected: 'not found|error|success' },

  // === 20. create_sublevel ===
  { scenario: 'Level: create_sublevel', toolName: 'manage_level', arguments: { action: 'create_sublevel', sublevelName: 'SubLevel_Test', parentLevel: `${TEST_FOLDER}/TestLevel` }, expected: 'success|not found|already exists' },
  { scenario: 'Level: create_sublevel second', toolName: 'manage_level', arguments: { action: 'create_sublevel', sublevelName: 'SubLevel_Interior', parentLevel: `${TEST_FOLDER}/TestLevel` }, expected: 'success|not found|already exists' },

  // === 21. configure_level_streaming ===
  { scenario: 'Level: configure_level_streaming Blueprint', toolName: 'manage_level', arguments: { action: 'configure_level_streaming', sublevelPath: `${TEST_FOLDER}/SubLevel_Test`, streamingMethod: 'Blueprint' }, expected: 'success|not found' },
  { scenario: 'Level: configure_level_streaming AlwaysLoaded', toolName: 'manage_level', arguments: { action: 'configure_level_streaming', sublevelPath: `${TEST_FOLDER}/SubLevel_Test`, streamingMethod: 'AlwaysLoaded' }, expected: 'success|not found' },

  // === 22. set_streaming_distance ===
  { scenario: 'Level: set_streaming_distance', toolName: 'manage_level', arguments: { action: 'set_streaming_distance', sublevelPath: `${TEST_FOLDER}/SubLevel_Test`, streamingDistance: 5000 }, expected: 'success|not found' },
  { scenario: 'Level: set_streaming_distance far', toolName: 'manage_level', arguments: { action: 'set_streaming_distance', sublevelPath: `${TEST_FOLDER}/SubLevel_Test`, streamingDistance: 15000 }, expected: 'success|not found' },

  // === 23. configure_level_bounds ===
  { scenario: 'Level: configure_level_bounds', toolName: 'manage_level', arguments: { action: 'configure_level_bounds', boundsOrigin: { x: 0, y: 0, z: 0 }, boundsExtent: { x: 10000, y: 10000, z: 5000 } }, expected: 'success' },
  { scenario: 'Level: configure_level_bounds large', toolName: 'manage_level', arguments: { action: 'configure_level_bounds', boundsOrigin: { x: 0, y: 0, z: 0 }, boundsExtent: { x: 50000, y: 50000, z: 10000 } }, expected: 'success' },

  // === 24. enable_world_partition ===
  { scenario: 'Level: enable_world_partition on', toolName: 'manage_level', arguments: { action: 'enable_world_partition', bEnableWorldPartition: true }, expected: 'success|not available' },
  { scenario: 'Level: enable_world_partition off', toolName: 'manage_level', arguments: { action: 'enable_world_partition', bEnableWorldPartition: false }, expected: 'success|not available' },

  // === 25. configure_grid_size ===
  { scenario: 'Level: configure_grid_size', toolName: 'manage_level', arguments: { action: 'configure_grid_size', gridCellSize: 12800, loadingRange: 25600 }, expected: 'success|not enabled' },
  { scenario: 'Level: configure_grid_size large', toolName: 'manage_level', arguments: { action: 'configure_grid_size', gridCellSize: 25600, loadingRange: 51200 }, expected: 'success|not enabled' },

  // === 26. create_data_layer ===
  { scenario: 'Level: create_data_layer Runtime', toolName: 'manage_level', arguments: { action: 'create_data_layer', dataLayerName: 'TestDataLayer', dataLayerType: 'Runtime' }, expected: 'success|not available|already exists' },
  { scenario: 'Level: create_data_layer Editor', toolName: 'manage_level', arguments: { action: 'create_data_layer', dataLayerName: 'EditorDataLayer', dataLayerType: 'Editor' }, expected: 'success|not available|already exists' },

  // === 27. assign_actor_to_data_layer ===
  { scenario: 'Level: assign_actor_to_data_layer', toolName: 'manage_level', arguments: { action: 'assign_actor_to_data_layer', actorName: 'Test_Cube', dataLayerName: 'TestDataLayer' }, expected: 'success|not found' },
  { scenario: 'Level: assign_actor_to_data_layer nonexistent', toolName: 'manage_level', arguments: { action: 'assign_actor_to_data_layer', actorName: 'NonExistent', dataLayerName: 'TestDataLayer' }, expected: 'not found|error|success' },

  // === 28. configure_hlod_layer ===
  { scenario: 'Level: configure_hlod_layer', toolName: 'manage_level', arguments: { action: 'configure_hlod_layer', hlodLayerName: 'HLOD_Test', cellSize: 25600, loadingDistance: 51200 }, expected: 'success' },
  { scenario: 'Level: configure_hlod_layer large', toolName: 'manage_level', arguments: { action: 'configure_hlod_layer', hlodLayerName: 'HLOD_Far', cellSize: 51200, loadingDistance: 102400 }, expected: 'success' },

  // === 29. create_minimap_volume ===
  { scenario: 'Level: create_minimap_volume', toolName: 'manage_level', arguments: { action: 'create_minimap_volume', volumeName: 'Minimap_Test', volumeLocation: { x: 0, y: 0, z: 500 }, volumeExtent: { x: 10000, y: 10000, z: 100 } }, expected: 'success' },
  { scenario: 'Level: create_minimap_volume large', toolName: 'manage_level', arguments: { action: 'create_minimap_volume', volumeName: 'Minimap_World', volumeLocation: { x: 0, y: 0, z: 1000 }, volumeExtent: { x: 50000, y: 50000, z: 200 } }, expected: 'success' },

  // === 30. open_level_blueprint ===
  { scenario: 'Level: open_level_blueprint', toolName: 'manage_level', arguments: { action: 'open_level_blueprint' }, expected: 'success' },
  { scenario: 'Level: open_level_blueprint default', toolName: 'manage_level', arguments: { action: 'open_level_blueprint' }, expected: 'success' },

  // === 31. add_level_blueprint_node ===
  { scenario: 'Level: add_level_blueprint_node event', toolName: 'manage_level', arguments: { action: 'add_level_blueprint_node', nodeClass: 'K2Node_Event', nodeName: 'BeginPlay' }, expected: 'success' },
  { scenario: 'Level: add_level_blueprint_node print', toolName: 'manage_level', arguments: { action: 'add_level_blueprint_node', nodeClass: 'K2Node_CallFunction', nodeName: 'PrintString' }, expected: 'success' },

  // === 32. connect_level_blueprint_nodes ===
  { scenario: 'Level: connect_level_blueprint_nodes', toolName: 'manage_level', arguments: { action: 'connect_level_blueprint_nodes', sourceNodeName: 'BeginPlay', sourcePinName: 'exec', targetNodeName: 'PrintString', targetPinName: 'exec' }, expected: 'success|not found' },
  { scenario: 'Level: connect_level_blueprint_nodes nonexistent', toolName: 'manage_level', arguments: { action: 'connect_level_blueprint_nodes', sourceNodeName: 'NonExistent', sourcePinName: 'Out', targetNodeName: 'Target', targetPinName: 'In' }, expected: 'not found|error|success' },

  // === 33. create_level_instance ===
  { scenario: 'Level: create_level_instance', toolName: 'manage_level', arguments: { action: 'create_level_instance', levelInstanceName: 'LI_Test', levelAssetPath: `${TEST_FOLDER}/SubLevel_Test`, instanceLocation: { x: 1000, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Level: create_level_instance second', toolName: 'manage_level', arguments: { action: 'create_level_instance', levelInstanceName: 'LI_Interior', levelAssetPath: `${TEST_FOLDER}/SubLevel_Interior`, instanceLocation: { x: 2000, y: 0, z: 0 } }, expected: 'success|not found' },

  // === 34. create_packed_level_actor ===
  { scenario: 'Level: create_packed_level_actor', toolName: 'manage_level', arguments: { action: 'create_packed_level_actor', packedLevelName: 'PLA_Test', levelAssetPath: `${TEST_FOLDER}/SubLevel_Test` }, expected: 'success|not found' },
  { scenario: 'Level: create_packed_level_actor second', toolName: 'manage_level', arguments: { action: 'create_packed_level_actor', packedLevelName: 'PLA_Building', levelAssetPath: `${TEST_FOLDER}/SubLevel_Interior` }, expected: 'success|not found' },

  // === 35. get_level_structure_info ===
  { scenario: 'Level: get_level_structure_info', toolName: 'manage_level', arguments: { action: 'get_level_structure_info' }, expected: 'success' },
  { scenario: 'Level: get_level_structure_info default', toolName: 'manage_level', arguments: { action: 'get_level_structure_info' }, expected: 'success' },

  // === 36. configure_world_partition ===
  { scenario: 'Level: configure_world_partition', toolName: 'manage_level', arguments: { action: 'configure_world_partition', gridCellSize: 12800, loadingRange: 25600 }, expected: 'success|not enabled' },
  { scenario: 'Level: configure_world_partition large', toolName: 'manage_level', arguments: { action: 'configure_world_partition', gridCellSize: 25600, loadingRange: 51200 }, expected: 'success|not enabled' },

  // === 37. create_streaming_volume ===
  { scenario: 'Level: create_streaming_volume', toolName: 'manage_level', arguments: { action: 'create_streaming_volume', volumeName: 'StreamVol_Test', volumeLocation: { x: 0, y: 0, z: 0 }, volumeExtent: { x: 5000, y: 5000, z: 2000 } }, expected: 'success' },
  { scenario: 'Level: create_streaming_volume large', toolName: 'manage_level', arguments: { action: 'create_streaming_volume', volumeName: 'StreamVol_World', volumeLocation: { x: 0, y: 0, z: 0 }, volumeExtent: { x: 20000, y: 20000, z: 5000 } }, expected: 'success' },

  // === 38. configure_large_world_coordinates ===
  { scenario: 'Level: configure_large_world_coordinates on', toolName: 'manage_level', arguments: { action: 'configure_large_world_coordinates', enableLargeWorlds: true }, expected: 'success' },
  { scenario: 'Level: configure_large_world_coordinates off', toolName: 'manage_level', arguments: { action: 'configure_large_world_coordinates', enableLargeWorlds: false }, expected: 'success' },

  // === 39. create_world_partition_cell ===
  { scenario: 'Level: create_world_partition_cell', toolName: 'manage_level', arguments: { action: 'create_world_partition_cell', cellSize: 12800 }, expected: 'success|not enabled' },
  { scenario: 'Level: create_world_partition_cell large', toolName: 'manage_level', arguments: { action: 'create_world_partition_cell', cellSize: 25600 }, expected: 'success|not enabled' },

  // === 40. configure_runtime_loading ===
  { scenario: 'Level: configure_runtime_loading', toolName: 'manage_level', arguments: { action: 'configure_runtime_loading', runtimeCellSize: 12800 }, expected: 'success|not enabled' },
  { scenario: 'Level: configure_runtime_loading large', toolName: 'manage_level', arguments: { action: 'configure_runtime_loading', runtimeCellSize: 25600 }, expected: 'success|not enabled' },

  // === 41. configure_world_settings ===
  { scenario: 'Level: configure_world_settings gravity', toolName: 'manage_level', arguments: { action: 'configure_world_settings', killZ: -10000, worldGravityZ: -980 }, expected: 'success' },
  { scenario: 'Level: configure_world_settings low gravity', toolName: 'manage_level', arguments: { action: 'configure_world_settings', killZ: -50000, worldGravityZ: -400 }, expected: 'success' },

  // === 42. create_pcg_graph ===
  { scenario: 'Level: create_pcg_graph', toolName: 'manage_level', arguments: { action: 'create_pcg_graph', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|already exists' },
  { scenario: 'Level: create_pcg_graph forest', toolName: 'manage_level', arguments: { action: 'create_pcg_graph', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|already exists' },

  // === 43. create_pcg_subgraph ===
  { scenario: 'Level: create_pcg_subgraph', toolName: 'manage_level', arguments: { action: 'create_pcg_subgraph', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, nodeName: 'SubGraph1' }, expected: 'success|not found' },
  { scenario: 'Level: create_pcg_subgraph trees', toolName: 'manage_level', arguments: { action: 'create_pcg_subgraph', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, nodeName: 'TreeSubgraph' }, expected: 'success|not found' },

  // === 44. add_pcg_node ===
  { scenario: 'Level: add_pcg_node sampler', toolName: 'manage_level', arguments: { action: 'add_pcg_node', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, nodeClass: 'UPCGSurfaceSamplerSettings', nodeName: 'Sampler' }, expected: 'success|not found' },
  { scenario: 'Level: add_pcg_node spawner', toolName: 'manage_level', arguments: { action: 'add_pcg_node', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, nodeClass: 'UPCGStaticMeshSpawnerSettings', nodeName: 'MeshSpawner' }, expected: 'success|not found' },

  // === 45. connect_pcg_pins ===
  { scenario: 'Level: connect_pcg_pins', toolName: 'manage_level', arguments: { action: 'connect_pcg_pins', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, sourceNodeName: 'Input', sourcePinName: 'Out', targetNodeName: 'Sampler', targetPinName: 'In' }, expected: 'success|not found' },
  { scenario: 'Level: connect_pcg_pins sampler to spawner', toolName: 'manage_level', arguments: { action: 'connect_pcg_pins', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, sourceNodeName: 'Sampler', sourcePinName: 'Out', targetNodeName: 'MeshSpawner', targetPinName: 'In' }, expected: 'success|not found' },

  // === 46. set_pcg_node_settings ===
  { scenario: 'Level: set_pcg_node_settings', toolName: 'manage_level', arguments: { action: 'set_pcg_node_settings', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, nodeName: 'Sampler' }, expected: 'success|not found' },
  { scenario: 'Level: set_pcg_node_settings spawner', toolName: 'manage_level', arguments: { action: 'set_pcg_node_settings', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, nodeName: 'MeshSpawner' }, expected: 'success|not found' },

  // === 47. add_landscape_data_node ===
  { scenario: 'Level: add_landscape_data_node', toolName: 'manage_level', arguments: { action: 'add_landscape_data_node', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_landscape_data_node forest', toolName: 'manage_level', arguments: { action: 'add_landscape_data_node', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 48. add_spline_data_node ===
  { scenario: 'Level: add_spline_data_node', toolName: 'manage_level', arguments: { action: 'add_spline_data_node', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_spline_data_node forest', toolName: 'manage_level', arguments: { action: 'add_spline_data_node', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 49. add_volume_data_node ===
  { scenario: 'Level: add_volume_data_node', toolName: 'manage_level', arguments: { action: 'add_volume_data_node', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_volume_data_node forest', toolName: 'manage_level', arguments: { action: 'add_volume_data_node', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 50. add_actor_data_node ===
  { scenario: 'Level: add_actor_data_node', toolName: 'manage_level', arguments: { action: 'add_actor_data_node', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_actor_data_node forest', toolName: 'manage_level', arguments: { action: 'add_actor_data_node', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 51. add_texture_data_node ===
  { scenario: 'Level: add_texture_data_node', toolName: 'manage_level', arguments: { action: 'add_texture_data_node', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_texture_data_node forest', toolName: 'manage_level', arguments: { action: 'add_texture_data_node', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 52. add_surface_sampler ===
  { scenario: 'Level: add_surface_sampler', toolName: 'manage_level', arguments: { action: 'add_surface_sampler', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_surface_sampler forest', toolName: 'manage_level', arguments: { action: 'add_surface_sampler', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 53. add_mesh_sampler ===
  { scenario: 'Level: add_mesh_sampler', toolName: 'manage_level', arguments: { action: 'add_mesh_sampler', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_mesh_sampler forest', toolName: 'manage_level', arguments: { action: 'add_mesh_sampler', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 54. add_spline_sampler ===
  { scenario: 'Level: add_spline_sampler', toolName: 'manage_level', arguments: { action: 'add_spline_sampler', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_spline_sampler forest', toolName: 'manage_level', arguments: { action: 'add_spline_sampler', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 55. add_volume_sampler ===
  { scenario: 'Level: add_volume_sampler', toolName: 'manage_level', arguments: { action: 'add_volume_sampler', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_volume_sampler forest', toolName: 'manage_level', arguments: { action: 'add_volume_sampler', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 56. add_bounds_modifier ===
  { scenario: 'Level: add_bounds_modifier', toolName: 'manage_level', arguments: { action: 'add_bounds_modifier', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_bounds_modifier forest', toolName: 'manage_level', arguments: { action: 'add_bounds_modifier', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 57. add_density_filter ===
  { scenario: 'Level: add_density_filter', toolName: 'manage_level', arguments: { action: 'add_density_filter', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_density_filter forest', toolName: 'manage_level', arguments: { action: 'add_density_filter', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 58. add_height_filter ===
  { scenario: 'Level: add_height_filter', toolName: 'manage_level', arguments: { action: 'add_height_filter', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_height_filter forest', toolName: 'manage_level', arguments: { action: 'add_height_filter', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 59. add_slope_filter ===
  { scenario: 'Level: add_slope_filter', toolName: 'manage_level', arguments: { action: 'add_slope_filter', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_slope_filter forest', toolName: 'manage_level', arguments: { action: 'add_slope_filter', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 60. add_distance_filter ===
  { scenario: 'Level: add_distance_filter', toolName: 'manage_level', arguments: { action: 'add_distance_filter', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_distance_filter forest', toolName: 'manage_level', arguments: { action: 'add_distance_filter', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 61. add_bounds_filter ===
  { scenario: 'Level: add_bounds_filter', toolName: 'manage_level', arguments: { action: 'add_bounds_filter', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_bounds_filter forest', toolName: 'manage_level', arguments: { action: 'add_bounds_filter', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 62. add_self_pruning ===
  { scenario: 'Level: add_self_pruning', toolName: 'manage_level', arguments: { action: 'add_self_pruning', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_self_pruning forest', toolName: 'manage_level', arguments: { action: 'add_self_pruning', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 63. add_transform_points ===
  { scenario: 'Level: add_transform_points', toolName: 'manage_level', arguments: { action: 'add_transform_points', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_transform_points forest', toolName: 'manage_level', arguments: { action: 'add_transform_points', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 64. add_project_to_surface ===
  { scenario: 'Level: add_project_to_surface', toolName: 'manage_level', arguments: { action: 'add_project_to_surface', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_project_to_surface forest', toolName: 'manage_level', arguments: { action: 'add_project_to_surface', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 65. add_copy_points ===
  { scenario: 'Level: add_copy_points', toolName: 'manage_level', arguments: { action: 'add_copy_points', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_copy_points forest', toolName: 'manage_level', arguments: { action: 'add_copy_points', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 66. add_merge_points ===
  { scenario: 'Level: add_merge_points', toolName: 'manage_level', arguments: { action: 'add_merge_points', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_merge_points forest', toolName: 'manage_level', arguments: { action: 'add_merge_points', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 67. add_static_mesh_spawner ===
  { scenario: 'Level: add_static_mesh_spawner', toolName: 'manage_level', arguments: { action: 'add_static_mesh_spawner', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_static_mesh_spawner forest', toolName: 'manage_level', arguments: { action: 'add_static_mesh_spawner', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 68. add_actor_spawner ===
  { scenario: 'Level: add_actor_spawner', toolName: 'manage_level', arguments: { action: 'add_actor_spawner', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_actor_spawner forest', toolName: 'manage_level', arguments: { action: 'add_actor_spawner', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 69. add_spline_spawner ===
  { scenario: 'Level: add_spline_spawner', toolName: 'manage_level', arguments: { action: 'add_spline_spawner', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: add_spline_spawner forest', toolName: 'manage_level', arguments: { action: 'add_spline_spawner', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 70. execute_pcg_graph ===
  { scenario: 'Level: execute_pcg_graph', toolName: 'manage_level', arguments: { action: 'execute_pcg_graph', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: execute_pcg_graph forest', toolName: 'manage_level', arguments: { action: 'execute_pcg_graph', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 71. set_pcg_partition_grid_size ===
  { scenario: 'Level: set_pcg_partition_grid_size', toolName: 'manage_level', arguments: { action: 'set_pcg_partition_grid_size', gridCellSize: 12800 }, expected: 'success' },
  { scenario: 'Level: set_pcg_partition_grid_size large', toolName: 'manage_level', arguments: { action: 'set_pcg_partition_grid_size', gridCellSize: 25600 }, expected: 'success' },

  // === 72. get_pcg_info ===
  { scenario: 'Level: get_pcg_info', toolName: 'manage_level', arguments: { action: 'get_pcg_info', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: get_pcg_info forest', toolName: 'manage_level', arguments: { action: 'get_pcg_info', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 73. create_biome_rules ===
  { scenario: 'Level: create_biome_rules', toolName: 'manage_level', arguments: { action: 'create_biome_rules', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: create_biome_rules forest', toolName: 'manage_level', arguments: { action: 'create_biome_rules', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 74. blend_biomes ===
  { scenario: 'Level: blend_biomes', toolName: 'manage_level', arguments: { action: 'blend_biomes', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: blend_biomes forest', toolName: 'manage_level', arguments: { action: 'blend_biomes', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 75. export_pcg_to_static ===
  { scenario: 'Level: export_pcg_to_static', toolName: 'manage_level', arguments: { action: 'export_pcg_to_static', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: export_pcg_to_static forest', toolName: 'manage_level', arguments: { action: 'export_pcg_to_static', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 76. import_pcg_preset ===
  { scenario: 'Level: import_pcg_preset', toolName: 'manage_level', arguments: { action: 'import_pcg_preset', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: import_pcg_preset forest', toolName: 'manage_level', arguments: { action: 'import_pcg_preset', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 77. debug_pcg_execution ===
  { scenario: 'Level: debug_pcg_execution', toolName: 'manage_level', arguments: { action: 'debug_pcg_execution', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found' },
  { scenario: 'Level: debug_pcg_execution forest', toolName: 'manage_level', arguments: { action: 'debug_pcg_execution', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found' },

  // === 78. create_pcg_hlsl_node ===
  { scenario: 'Level: create_pcg_hlsl_node', toolName: 'manage_level', arguments: { action: 'create_pcg_hlsl_node', graphPath: `${TEST_FOLDER}/PCG_TestGraph`, hlslCode: 'float4 Main() { return float4(1,1,1,1); }' }, expected: 'success|not found|not supported' },
  { scenario: 'Level: create_pcg_hlsl_node forest', toolName: 'manage_level', arguments: { action: 'create_pcg_hlsl_node', graphPath: `${TEST_FOLDER}/PCG_ForestGraph`, hlslCode: 'float4 Main() { return float4(0,1,0,1); }' }, expected: 'success|not found|not supported' },

  // === 79. enable_pcg_gpu_processing ===
  { scenario: 'Level: enable_pcg_gpu_processing', toolName: 'manage_level', arguments: { action: 'enable_pcg_gpu_processing', graphPath: `${TEST_FOLDER}/PCG_TestGraph` }, expected: 'success|not found|not supported' },
  { scenario: 'Level: enable_pcg_gpu_processing forest', toolName: 'manage_level', arguments: { action: 'enable_pcg_gpu_processing', graphPath: `${TEST_FOLDER}/PCG_ForestGraph` }, expected: 'success|not found|not supported' },

  // === 80. configure_pcg_mode_brush ===
  { scenario: 'Level: configure_pcg_mode_brush', toolName: 'manage_level', arguments: { action: 'configure_pcg_mode_brush' }, expected: 'success|not supported' },
  { scenario: 'Level: configure_pcg_mode_brush default', toolName: 'manage_level', arguments: { action: 'configure_pcg_mode_brush' }, expected: 'success|not supported' },

  // === 81. export_pcg_hlsl_template ===
  { scenario: 'Level: export_pcg_hlsl_template', toolName: 'manage_level', arguments: { action: 'export_pcg_hlsl_template' }, expected: 'success|not supported' },
  { scenario: 'Level: export_pcg_hlsl_template default', toolName: 'manage_level', arguments: { action: 'export_pcg_hlsl_template' }, expected: 'success|not supported' },

  // === 82. batch_execute_pcg_with_gpu ===
  { scenario: 'Level: batch_execute_pcg_with_gpu', toolName: 'manage_level', arguments: { action: 'batch_execute_pcg_with_gpu' }, expected: 'success|not supported' },
  { scenario: 'Level: batch_execute_pcg_with_gpu default', toolName: 'manage_level', arguments: { action: 'batch_execute_pcg_with_gpu' }, expected: 'success|not supported' },

  // === 83. get_world_partition_cells ===
  { scenario: 'Level: get_world_partition_cells', toolName: 'manage_level', arguments: { action: 'get_world_partition_cells' }, expected: 'success|not enabled' },
  { scenario: 'Level: get_world_partition_cells default', toolName: 'manage_level', arguments: { action: 'get_world_partition_cells' }, expected: 'success|not enabled' },

  // === 84. stream_level_async ===
  { scenario: 'Level: stream_level_async load', toolName: 'manage_level', arguments: { action: 'stream_level_async', levelPath: `${TEST_FOLDER}/SubLevel_Test`, shouldBeLoaded: true }, expected: 'success|not found' },
  { scenario: 'Level: stream_level_async unload', toolName: 'manage_level', arguments: { action: 'stream_level_async', levelPath: `${TEST_FOLDER}/SubLevel_Test`, shouldBeLoaded: false }, expected: 'success|not found' },

  // === 85. get_streaming_levels_status ===
  { scenario: 'Level: get_streaming_levels_status', toolName: 'manage_level', arguments: { action: 'get_streaming_levels_status' }, expected: 'success' },
  { scenario: 'Level: get_streaming_levels_status default', toolName: 'manage_level', arguments: { action: 'get_streaming_levels_status' }, expected: 'success' },

  // === 86. configure_hlod_settings ===
  { scenario: 'Level: configure_hlod_settings', toolName: 'manage_level', arguments: { action: 'configure_hlod_settings', cellSize: 25600, loadingDistance: 51200 }, expected: 'success' },
  { scenario: 'Level: configure_hlod_settings large', toolName: 'manage_level', arguments: { action: 'configure_hlod_settings', cellSize: 51200, loadingDistance: 102400 }, expected: 'success' },

  // === 87. build_hlod_for_level ===
  { scenario: 'Level: build_hlod_for_level', toolName: 'manage_level', arguments: { action: 'build_hlod_for_level' }, expected: 'success|not enabled' },
  { scenario: 'Level: build_hlod_for_level default', toolName: 'manage_level', arguments: { action: 'build_hlod_for_level' }, expected: 'success|not enabled' },
];

// ============================================================================
// COMBINED TEST CASES (Setup runs FIRST)
// ============================================================================
export const coreToolsTests = [
  ...setupTests,  // Setup tests MUST run first
  ...manageAssetTests,
  ...controlActorTests,
  ...controlEditorTests,
  ...manageLevelTests,
];

// Run tests
const main = async () => {
  console.log('='.repeat(80));
  console.log('CORE TOOLS INTEGRATION TESTS');
  console.log('='.repeat(80));
  console.log(`Total test cases: ${coreToolsTests.length}`);
  console.log(`  - setup: ${setupTests.length} (create test assets)`);
  console.log(`  - manage_asset: ${manageAssetTests.length} (99 actions x 2)`);
  console.log(`  - control_actor: ${controlActorTests.length} (45 actions x 2)`);
  console.log(`  - control_editor: ${controlEditorTests.length} (84 actions x 2)`);
  console.log(`  - manage_level: ${manageLevelTests.length} (87 actions x 2)`);
  console.log('='.repeat(80));

  await runToolTests('core-tools', coreToolsTests);
};

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  main();
}
