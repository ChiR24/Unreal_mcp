#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();
const projectImportSource = path.join('/data/Game/MCPtest/Saved', `mcp-manage-asset-${ts}.obj`);
const relativeImportSource = `Saved/mcp-manage-asset-${ts}.obj`;

fs.mkdirSync(path.dirname(projectImportSource), { recursive: true });
fs.writeFileSync(projectImportSource, [
  'o MCPManageAssetImport',
  'v 0 0 0',
  'v 0 100 0',
  'v 100 0 0',
  'f 1 2 3',
  ''
].join('\n'));

const asset = (name) => `${TEST_FOLDER}/${name}`;

const BASE_MATERIAL = asset(`M_AssetBase_${ts}`);
const INSTANCE = asset(`MI_AssetBase_${ts}`);
const RENDER_TARGET = `RT_Asset_${ts}`;
const IMPORTED_MESH = asset(`SM_Imported_${ts}`);
const EXISTING_STATIC_MESH = '/Game/MCPTest/TestMesh';
const DUPLICATE_SOURCE = asset(`M_DuplicateSource_${ts}`);
const DUPLICATE_DEST = asset(`M_DuplicateDest_${ts}`);
const DUPLICATE_ALIAS_SOURCE = asset(`M_DuplicateAliasSource_${ts}`);
const DUPLICATE_ALIAS_DEST = asset(`M_DuplicateAliasDest_${ts}`);
const RENAME_SOURCE = asset(`M_RenameSource_${ts}`);
const RENAME_DEST_NAME = `M_RenameDest_${ts}`;
const RENAME_ALIAS_SOURCE = asset(`M_RenameAliasSource_${ts}`);
const RENAME_ALIAS_DEST_NAME = `M_RenameAliasDest_${ts}`;
const MOVE_SOURCE = asset(`M_MoveSource_${ts}`);
const MOVE_DEST = asset(`Moved/M_MoveSource_${ts}`);
const MOVE_ALIAS_SOURCE = asset(`M_MoveAliasSource_${ts}`);
const MOVE_ALIAS_DEST = asset(`Moved/M_MoveAliasSource_${ts}`);
const DELETE_SOURCE = asset(`M_DeleteSource_${ts}`);
const DELETE_ALIAS_SOURCE = asset(`M_DeleteAliasSource_${ts}`);
const DELETE_BULK_SOURCE = asset(`M_DeleteBulkSource_${ts}`);
const BULK_RENAME_SOURCE = asset(`M_BulkRenameSource_${ts}`);
const BULK_DELETE_SOURCE = asset(`M_BulkDeleteSource_${ts}`);
const TAG_KEY = `MCPAssetTag_${ts}`;
const TAG_VALUE = 'real-live';

const createMaterial = (name) => ({
  toolName: 'manage_asset',
  arguments: { action: 'create_material', name, path: TEST_FOLDER },
  expected: 'success|already exists'
});

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create moved folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: `${TEST_FOLDER}/Moved` }, expected: 'success|already exists' },
  { scenario: 'Setup: create bulk folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: `${TEST_FOLDER}/BulkFolder` }, expected: 'success|already exists' },
  { scenario: 'Setup: create base material', ...createMaterial(`M_AssetBase_${ts}`) },
  { scenario: 'Setup: create duplicate source', ...createMaterial(`M_DuplicateSource_${ts}`) },
  { scenario: 'Setup: create duplicate alias source', ...createMaterial(`M_DuplicateAliasSource_${ts}`) },
  { scenario: 'Setup: create rename source', ...createMaterial(`M_RenameSource_${ts}`) },
  { scenario: 'Setup: create rename alias source', ...createMaterial(`M_RenameAliasSource_${ts}`) },
  { scenario: 'Setup: create move source', ...createMaterial(`M_MoveSource_${ts}`) },
  { scenario: 'Setup: create move alias source', ...createMaterial(`M_MoveAliasSource_${ts}`) },
  { scenario: 'Setup: create delete source', ...createMaterial(`M_DeleteSource_${ts}`) },
  { scenario: 'Setup: create delete alias source', ...createMaterial(`M_DeleteAliasSource_${ts}`) },
  { scenario: 'Setup: create bulk delete source', ...createMaterial(`M_DeleteBulkSource_${ts}`) },
  { scenario: 'Setup: create bulk rename source', ...createMaterial(`M_BulkRenameSource_${ts}`) },
  { scenario: 'Setup: create bulk delete action source', ...createMaterial(`M_BulkDeleteSource_${ts}`) },
  { scenario: 'Setup: create bulk folder rename source', toolName: 'manage_asset', arguments: { action: 'create_material', name: `M_BulkFolderSource_${ts}`, path: `${TEST_FOLDER}/BulkFolder` }, expected: 'success|already exists' },

  // === CORE ASSET ACTIONS ===
  { scenario: 'ACTION: list', toolName: 'manage_asset', arguments: { action: 'list', path: TEST_FOLDER, recursive: true }, expected: 'success' },
  { scenario: 'ACTION: import', toolName: 'manage_asset', arguments: { action: 'import', sourcePath: relativeImportSource, destinationPath: IMPORTED_MESH, overwrite: true, save: true }, expected: 'success' },
  { scenario: 'ACTION: duplicate', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: DUPLICATE_SOURCE, destinationPath: DUPLICATE_DEST }, expected: 'success' },
  { scenario: 'ACTION: duplicate_asset', toolName: 'manage_asset', arguments: { action: 'duplicate_asset', sourcePath: DUPLICATE_ALIAS_SOURCE, destinationPath: DUPLICATE_ALIAS_DEST }, expected: 'success' },
  { scenario: 'ACTION: rename', toolName: 'manage_asset', arguments: { action: 'rename', sourcePath: RENAME_SOURCE, newName: RENAME_DEST_NAME }, expected: 'success' },
  { scenario: 'ACTION: rename_asset', toolName: 'manage_asset', arguments: { action: 'rename_asset', sourcePath: RENAME_ALIAS_SOURCE, newName: RENAME_ALIAS_DEST_NAME }, expected: 'success' },
  { scenario: 'ACTION: move', toolName: 'manage_asset', arguments: { action: 'move', sourcePath: MOVE_SOURCE, destinationPath: MOVE_DEST }, expected: 'success' },
  { scenario: 'ACTION: move_asset', toolName: 'manage_asset', arguments: { action: 'move_asset', sourcePath: MOVE_ALIAS_SOURCE, destinationPath: MOVE_ALIAS_DEST }, expected: 'success' },

  // === DELETE VARIANTS ===
  { scenario: 'DELETE: delete', toolName: 'manage_asset', arguments: { action: 'delete', path: DELETE_SOURCE, force: true }, expected: 'success' },
  { scenario: 'DELETE: delete_asset', toolName: 'manage_asset', arguments: { action: 'delete_asset', assetPath: DELETE_ALIAS_SOURCE, force: true }, expected: 'success' },
  { scenario: 'DELETE: delete_assets', toolName: 'manage_asset', arguments: { action: 'delete_assets', paths: [DELETE_BULK_SOURCE], force: true }, expected: 'success' },

  // === INFO / METADATA ===
  { scenario: 'CREATE: create_folder', toolName: 'manage_asset', arguments: { action: 'create_folder', directoryPath: `${TEST_FOLDER}/SubFolder` }, expected: 'success|already exists' },
  { scenario: 'INFO: search_assets', toolName: 'manage_asset', arguments: { action: 'search_assets', searchText: `AssetBase_${ts}`, packagePaths: [TEST_FOLDER], recursivePaths: true, recursiveClasses: true, limit: 5, offset: 0 }, expected: 'success' },
  { scenario: 'INFO: get_dependencies', toolName: 'manage_asset', arguments: { action: 'get_dependencies', assetPath: BASE_MATERIAL }, expected: 'success' },
  { scenario: 'INFO: get_source_control_state', toolName: 'manage_asset', arguments: { action: 'get_source_control_state', assetPath: BASE_MATERIAL }, expected: { condition: 'success', errorPattern: 'SC_DISABLED' } },
  { scenario: 'ACTION: analyze_graph', toolName: 'manage_asset', arguments: { action: 'analyze_graph', assetPath: BASE_MATERIAL, maxDepth: 2 }, expected: 'success' },
  { scenario: 'INFO: get_asset_graph', toolName: 'manage_asset', arguments: { action: 'get_asset_graph', assetPath: BASE_MATERIAL }, expected: 'success' },
  { scenario: 'CREATE: create_thumbnail', toolName: 'manage_asset', arguments: { action: 'create_thumbnail', assetPath: BASE_MATERIAL, width: 64, height: 64 }, expected: 'success' },
  { scenario: 'CONFIG: set_tags', toolName: 'manage_asset', arguments: { action: 'set_tags', assetPath: BASE_MATERIAL, tags: [TAG_KEY] }, expected: 'success' },
  { scenario: 'INFO: get_metadata', toolName: 'manage_asset', arguments: { action: 'get_metadata', assetPath: BASE_MATERIAL }, expected: 'success' },
  { scenario: 'CONFIG: set_metadata', toolName: 'manage_asset', arguments: { action: 'set_metadata', assetPath: BASE_MATERIAL, metadata: { [TAG_KEY]: TAG_VALUE } }, expected: 'success' },
  { scenario: 'ACTION: validate', toolName: 'manage_asset', arguments: { action: 'validate', assetPath: BASE_MATERIAL }, expected: 'success' },
  { scenario: 'ACTION: fixup_redirectors', toolName: 'manage_asset', arguments: { action: 'fixup_redirectors', directoryPath: TEST_FOLDER, checkoutFiles: false }, expected: 'success' },
  { scenario: 'INFO: find_by_tag', toolName: 'manage_asset', arguments: { action: 'find_by_tag', tag: TAG_KEY }, expected: 'success' },
  { scenario: 'ACTION: generate_report', toolName: 'manage_asset', arguments: { action: 'generate_report', directory: TEST_FOLDER, reportType: 'Summary' }, expected: 'success' },

  // === MATERIAL / MESH ACTIONS ===
  { scenario: 'CREATE: create_material', toolName: 'manage_asset', arguments: { action: 'create_material', name: `M_CreateAction_${ts}`, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_material_instance', toolName: 'manage_asset', arguments: { action: 'create_material_instance', name: `MI_AssetBase_${ts}`, parentMaterial: BASE_MATERIAL, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_render_target', toolName: 'manage_asset', arguments: { action: 'create_render_target', name: RENDER_TARGET, path: TEST_FOLDER, width: 256, height: 256, format: 'RGBA16F' }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_render_target save false', toolName: 'manage_asset', arguments: { action: 'create_render_target', name: `RT_Asset_NoSave_${ts}`, path: TEST_FOLDER, width: 128, height: 128, save: false }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.data.saved', equals: false, label: 'save flag preserved' }] },
  { scenario: 'ACTION: generate_lods', toolName: 'manage_asset', arguments: { action: 'generate_lods', assetPath: EXISTING_STATIC_MESH, lodCount: 2 }, expected: 'success' },
  { scenario: 'ADD: add_material_parameter', toolName: 'manage_asset', arguments: { action: 'add_material_parameter', assetPath: BASE_MATERIAL, parameterName: `ScalarParam_${ts}`, parameterType: 'Scalar', value: 0.5 }, expected: 'success|already exists' },
  { scenario: 'INFO: list_instances', toolName: 'manage_asset', arguments: { action: 'list_instances', assetPath: BASE_MATERIAL }, expected: 'success' },
  { scenario: 'ACTION: reset_instance_parameters', toolName: 'manage_asset', arguments: { action: 'reset_instance_parameters', assetPath: INSTANCE }, expected: 'success' },
  { scenario: 'INFO: exists', toolName: 'manage_asset', arguments: { action: 'exists', assetPath: BASE_MATERIAL }, expected: 'success' },
  { scenario: 'INFO: get_material_stats', toolName: 'manage_asset', arguments: { action: 'get_material_stats', assetPath: BASE_MATERIAL }, expected: 'success' },
  { scenario: 'ACTION: nanite_rebuild_mesh', toolName: 'manage_asset', arguments: { action: 'nanite_rebuild_mesh', assetPath: EXISTING_STATIC_MESH }, expected: 'success' },
  { scenario: 'ACTION: bulk_rename', toolName: 'manage_asset', arguments: { action: 'bulk_rename', assetPaths: [BULK_RENAME_SOURCE], pattern: `M_BulkRenameSource_${ts}`, replacement: `M_BulkRenamed_${ts}` }, expected: 'success' },
  { scenario: 'ACTION: bulk_rename folderPath options', toolName: 'manage_asset', arguments: { action: 'bulk_rename', folderPath: `${TEST_FOLDER}/BulkFolder`, searchText: `M_BulkFolderSource_${ts}`, replaceText: `M_BulkFolderRenamed_${ts}`, prefix: 'P_', suffix: '_S', checkoutFiles: false }, expected: 'success' },
  { scenario: 'ACTION: bulk_delete', toolName: 'manage_asset', arguments: { action: 'bulk_delete', assetPaths: [BULK_DELETE_SOURCE], showConfirmation: false, fixupRedirectors: false }, expected: 'success' },
  { scenario: 'ACTION: source_control_checkout', toolName: 'manage_asset', arguments: { action: 'source_control_checkout', assetPaths: [BASE_MATERIAL] }, expected: { condition: 'success', errorPattern: 'SOURCE_CONTROL_DISABLED' } },
  { scenario: 'ACTION: source_control_submit', toolName: 'manage_asset', arguments: { action: 'source_control_submit', assetPaths: [BASE_MATERIAL] }, expected: { condition: 'success', errorPattern: 'SOURCE_CONTROL_DISABLED' } },

  // === MATERIAL GRAPH ACTIONS ===
  { scenario: 'ADD: add_material_node constant', toolName: 'manage_asset', arguments: { action: 'add_material_node', materialPath: BASE_MATERIAL, type: 'Constant', x: -200, y: 0 }, expected: 'success|already exists', captureResult: { key: 'constantNodeId', fromField: 'nodeId' } },
  { scenario: 'ADD: add_material_node multiply', toolName: 'manage_asset', arguments: { action: 'add_material_node', assetPath: BASE_MATERIAL, nodeType: 'Multiply', posX: 0, posY: 0 }, expected: 'success|already exists', captureResult: { key: 'multiplyNodeId', fromField: 'nodeId' } },
  { scenario: 'CONNECT: connect_material_pins', toolName: 'manage_asset', arguments: { action: 'connect_material_pins', assetPath: BASE_MATERIAL, sourceNodeId: '${captured:constantNodeId}', sourcePin: '0', targetNodeId: '${captured:multiplyNodeId}', targetPin: 'A' }, expected: 'success' },
  { scenario: 'ACTION: break_material_connections', toolName: 'manage_asset', arguments: { action: 'break_material_connections', assetPath: BASE_MATERIAL, nodeId: '${captured:multiplyNodeId}', pinName: 'A' }, expected: 'success' },
  { scenario: 'INFO: get_material_node_details', toolName: 'manage_asset', arguments: { action: 'get_material_node_details', assetPath: BASE_MATERIAL, nodeId: '${captured:multiplyNodeId}' }, expected: 'success' },
  { scenario: 'DELETE: remove_material_node', toolName: 'manage_asset', arguments: { action: 'remove_material_node', assetPath: BASE_MATERIAL, nodeId: '${captured:multiplyNodeId}' }, expected: 'success' },
  { scenario: 'ACTION: rebuild_material', toolName: 'manage_asset', arguments: { action: 'rebuild_material', assetPath: BASE_MATERIAL }, expected: 'success' },
];

// === MATERIAL AUTHORING ACTIONS ===
{
  /**
   * manage_asset material authoring action integration tests
   * Exercises real material, material function, and material instance mutations.
   */

  const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
  const ts = Date.now();

  const MATERIAL_NAME = `Testmaterial_${ts}`;
  const MATERIAL_PATH = `${TEST_FOLDER}/${MATERIAL_NAME}`;
  const FUNCTION_NAME = `Testmaterial_function_${ts}`;
  const FUNCTION_PATH = `${TEST_FOLDER}/${FUNCTION_NAME}`;
  const INSTANCE_NAME = `Testmaterial_instance_${ts}`;
  const INSTANCE_PATH = `${TEST_FOLDER}/${INSTANCE_NAME}`;
  const LANDSCAPE_MATERIAL_NAME = `Testlandscape_material_${ts}`;
  const LANDSCAPE_MATERIAL_PATH = `${TEST_FOLDER}/${LANDSCAPE_MATERIAL_NAME}`;
  const LANDSCAPE_LAYER_PATH = `${LANDSCAPE_MATERIAL_PATH}/Grass`;
  const DECAL_MATERIAL_NAME = `Testdecal_material_${ts}`;
  const DECAL_MATERIAL_PATH = `${TEST_FOLDER}/${DECAL_MATERIAL_NAME}`;
  const POST_PROCESS_MATERIAL_NAME = `Testpost_process_material_${ts}`;
  const POST_PROCESS_MATERIAL_PATH = `${TEST_FOLDER}/${POST_PROCESS_MATERIAL_NAME}`;
  const TEXTURE_NAME = `Testmaterial_texture_${ts}`;
  const TEXTURE_PATH = `${TEST_FOLDER}/${TEXTURE_NAME}`;

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: create texture parameter asset', toolName: 'manage_asset', arguments: { action: 'create_noise_texture', name: TEXTURE_NAME, path: TEST_FOLDER, width: 32, height: 32 }, expected: 'success|already exists' },

    // === CREATE ===
    { scenario: 'CREATE: create_material', toolName: 'manage_asset', arguments: { action: 'create_material', name: MATERIAL_NAME, path: TEST_FOLDER, materialDomain: 'Surface', blendMode: 'Opaque', shadingModel: 'DefaultLit' }, expected: 'success|already exists' },

    // === CONFIG ===
    { scenario: 'CONFIG: set_blend_mode', toolName: 'manage_asset', arguments: { action: 'set_blend_mode', assetPath: MATERIAL_PATH, blendMode: 'Masked' }, expected: 'success' },
    { scenario: 'CONFIG: set_shading_model', toolName: 'manage_asset', arguments: { action: 'set_shading_model', assetPath: MATERIAL_PATH, shadingModel: 'DefaultLit' }, expected: 'success' },
    { scenario: 'CONFIG: set_material_domain', toolName: 'manage_asset', arguments: { action: 'set_material_domain', assetPath: MATERIAL_PATH, domain: 'Surface' }, expected: 'success' },
    { scenario: 'CONFIG: set_two_sided', toolName: 'manage_asset', arguments: { action: 'set_two_sided', assetPath: MATERIAL_PATH, twoSided: true, save: false }, expected: 'success' },

    // === ADD ===
    { scenario: 'ADD: add_texture_sample', toolName: 'manage_asset', arguments: { action: 'add_texture_sample', assetPath: MATERIAL_PATH, texturePath: TEXTURE_PATH, parameterName: 'AlbedoTex', x: -600, y: -300 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_texture_coordinate', toolName: 'manage_asset', arguments: { action: 'add_texture_coordinate', assetPath: MATERIAL_PATH, coordinateIndex: 0, uTiling: 1, vTiling: 1, x: -800, y: -300 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_scalar_parameter', toolName: 'manage_asset', arguments: { action: 'add_scalar_parameter', assetPath: MATERIAL_PATH, parameterName: 'RoughnessParam', defaultValue: 0.5, group: 'MCP', x: -400, y: 120 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_vector_parameter', toolName: 'manage_asset', arguments: { action: 'add_vector_parameter', assetPath: MATERIAL_PATH, parameterName: 'TintParam', defaultValue: { r: 0.1, g: 0.4, b: 0.8, a: 1 }, group: 'MCP', x: -400, y: 260 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_static_switch_parameter', toolName: 'manage_asset', arguments: { action: 'add_static_switch_parameter', assetPath: MATERIAL_PATH, parameterName: 'UseDetailParam', defaultValue: true, group: 'MCP', x: -400, y: 400 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_math_node', toolName: 'manage_asset', arguments: { action: 'add_math_node', assetPath: MATERIAL_PATH, operation: 'Multiply', constA: 0.5, constB: 2, x: -100, y: 120 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_world_position', toolName: 'manage_asset', arguments: { action: 'add_world_position', assetPath: MATERIAL_PATH, x: -100, y: -450 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_vertex_normal', toolName: 'manage_asset', arguments: { action: 'add_vertex_normal', assetPath: MATERIAL_PATH, x: -100, y: -320 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_pixel_depth', toolName: 'manage_asset', arguments: { action: 'add_pixel_depth', assetPath: MATERIAL_PATH, x: -100, y: -190 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_fresnel', toolName: 'manage_asset', arguments: { action: 'add_fresnel', assetPath: MATERIAL_PATH, x: -100, y: -60 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_reflection_vector', toolName: 'manage_asset', arguments: { action: 'add_reflection_vector', assetPath: MATERIAL_PATH, x: -100, y: 70 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_panner', toolName: 'manage_asset', arguments: { action: 'add_panner', assetPath: MATERIAL_PATH, speedX: 0.25, speedY: 0.5, x: -100, y: 200 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_rotator', toolName: 'manage_asset', arguments: { action: 'add_rotator', assetPath: MATERIAL_PATH, speed: 0.2, x: -100, y: 330 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_noise', toolName: 'manage_asset', arguments: { action: 'add_noise', assetPath: MATERIAL_PATH, scale: 8, levels: 2, x: 150, y: -320 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_voronoi', toolName: 'manage_asset', arguments: { action: 'add_voronoi', assetPath: MATERIAL_PATH, scale: 8, x: 150, y: -190 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_if', toolName: 'manage_asset', arguments: { action: 'add_if', assetPath: MATERIAL_PATH, x: 150, y: -60 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_switch', toolName: 'manage_asset', arguments: { action: 'add_switch', assetPath: MATERIAL_PATH, x: 150, y: 70 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_custom_expression', toolName: 'manage_asset', arguments: { action: 'add_custom_expression', assetPath: MATERIAL_PATH, code: 'return 0.25;', outputType: 'Float1', description: 'MCP test custom expression', inputs: [{ name: 'InputA' }], additionalOutputs: [{ name: 'AuxOut', type: 'Float1' }], x: 150, y: 200 }, expected: 'success|already exists', captureResult: { key: 'customExpressionNodeId', fromField: 'nodeId' } },
    { scenario: 'INFO: find_node by type and name', toolName: 'manage_asset', arguments: { action: 'find_node', assetPath: MATERIAL_PATH, nodeType: 'ScalarParameter', name: 'RoughnessParam' }, expected: 'success' },
    { scenario: 'INFO: get_node_properties', toolName: 'manage_asset', arguments: { action: 'get_node_properties', assetPath: MATERIAL_PATH, nodeId: 'RoughnessParam' }, expected: 'success' },
    { scenario: 'ACTION: update_custom_expression', toolName: 'manage_asset', arguments: { action: 'update_custom_expression', assetPath: MATERIAL_PATH, nodeId: '${captured:customExpressionNodeId}', code: 'return InputA + 0.5;', description: 'MCP updated custom expression', outputType: 'Float1', inputs: [{ name: 'InputA' }], additionalOutputs: [{ name: 'AuxOut2', type: 'Float1' }] }, expected: 'success' },
    { scenario: 'INFO: get_node_properties custom expression', toolName: 'manage_asset', arguments: { action: 'get_node_properties', assetPath: MATERIAL_PATH, nodeId: '${captured:customExpressionNodeId}' }, expected: 'success' },

    // === CONNECT ===
    { scenario: 'CONNECT: connect_nodes', toolName: 'manage_asset', arguments: { action: 'connect_nodes', assetPath: MATERIAL_PATH, sourceNodeId: 'RoughnessParam', targetNodeId: 'Main', inputName: 'Roughness' }, expected: 'success' },
    { scenario: 'INFO: get_node_connections downstream', toolName: 'manage_asset', arguments: { action: 'get_node_connections', assetPath: MATERIAL_PATH, nodeId: 'RoughnessParam', direction: 'outputs', depth: -1, downstream: true }, expected: 'success' },
    { scenario: 'INFO: get_node_chain to material pin', toolName: 'manage_asset', arguments: { action: 'get_node_chain', assetPath: MATERIAL_PATH, startNodeId: 'RoughnessParam', endPin: 'Roughness' }, expected: 'success' },
    { scenario: 'INFO: get_connected_subgraph from node', toolName: 'manage_asset', arguments: { action: 'get_connected_subgraph', assetPath: MATERIAL_PATH, nodeId: 'RoughnessParam' }, expected: 'success' },
    { scenario: 'ACTION: disconnect_nodes', toolName: 'manage_asset', arguments: { action: 'disconnect_nodes', assetPath: MATERIAL_PATH, nodeId: 'Main', pinName: 'Roughness' }, expected: 'success' },
    { scenario: 'INFO: get_connected_subgraph orphans only', toolName: 'manage_asset', arguments: { action: 'get_connected_subgraph', assetPath: MATERIAL_PATH, orphansOnly: true }, expected: 'success' },

    // === MATERIAL FUNCTIONS ===
    { scenario: 'CREATE: create_material_function', toolName: 'manage_asset', arguments: { action: 'create_material_function', name: FUNCTION_NAME, path: TEST_FOLDER, description: 'MCP material function test' }, expected: 'success|already exists' },
    { scenario: 'ADD: add_function_input', toolName: 'manage_asset', arguments: { action: 'add_function_input', functionPath: FUNCTION_PATH, inputName: 'InputColor', inputType: 'Vector3', x: -250, y: 0 }, expected: 'success|already exists' },
    { scenario: 'ADD: add_function_output', toolName: 'manage_asset', arguments: { action: 'add_function_output', functionPath: FUNCTION_PATH, inputName: 'OutputColor', inputType: 'Vector3', x: 250, y: 0 }, expected: 'success|already exists' },
    { scenario: 'INFO: get_material_function_info', toolName: 'manage_asset', arguments: { action: 'get_material_function_info', functionPath: FUNCTION_PATH }, expected: 'success' },
    { scenario: 'ACTION: use_material_function', toolName: 'manage_asset', arguments: { action: 'use_material_function', assetPath: MATERIAL_PATH, functionPath: FUNCTION_PATH, x: 350, y: 250 }, expected: 'success' },

    // === MATERIAL INSTANCES ===
    { scenario: 'CREATE: create_material_instance', toolName: 'manage_asset', arguments: { action: 'create_material_instance', name: INSTANCE_NAME, path: TEST_FOLDER, parentMaterial: MATERIAL_PATH }, expected: 'success|already exists' },
    { scenario: 'CONFIG: set_static_switch_parameter_value', toolName: 'manage_asset', arguments: { action: 'set_static_switch_parameter_value', instancePath: INSTANCE_PATH, parameterName: 'UseDetailParam', value: false, save: false }, expected: 'success' },
    { scenario: 'CONFIG: set_scalar_parameter_value', toolName: 'manage_asset', arguments: { action: 'set_scalar_parameter_value', instancePath: INSTANCE_PATH, parameterName: 'RoughnessParam', value: 0.35 }, expected: 'success' },
    { scenario: 'CONFIG: set_vector_parameter_value', toolName: 'manage_asset', arguments: { action: 'set_vector_parameter_value', instancePath: INSTANCE_PATH, parameterName: 'TintParam', value: { r: 0.8, g: 0.2, b: 0.1, a: 1 } }, expected: 'success' },
    { scenario: 'CONFIG: set_texture_parameter_value', toolName: 'manage_asset', arguments: { action: 'set_texture_parameter_value', instancePath: INSTANCE_PATH, parameterName: 'AlbedoTex', texturePath: TEXTURE_PATH }, expected: 'success' },
    { scenario: 'CONFIG: set_material_parameter ambiguous', toolName: 'manage_asset', arguments: { action: 'set_material_parameter', instancePath: INSTANCE_PATH, parameterName: 'RoughnessParam', parameterType: 'scalar', value: 0.5 }, expected: 'error|AMBIGUOUS_ACTION' },

    // === SPECIALIZED MATERIALS ===
    { scenario: 'CREATE: create_landscape_material', toolName: 'manage_asset', arguments: { action: 'create_landscape_material', name: LANDSCAPE_MATERIAL_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_decal_material', toolName: 'manage_asset', arguments: { action: 'create_decal_material', name: DECAL_MATERIAL_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_post_process_material', toolName: 'manage_asset', arguments: { action: 'create_post_process_material', name: POST_PROCESS_MATERIAL_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'ADD: add_landscape_layer', toolName: 'manage_asset', arguments: { action: 'add_landscape_layer', assetPath: LANDSCAPE_MATERIAL_PATH, layerName: 'Grass', blendType: 'LB_WeightBlend' }, expected: 'success|already exists' },
    { scenario: 'CONFIG: configure_layer_blend', toolName: 'manage_asset', arguments: { action: 'configure_layer_blend', assetPath: LANDSCAPE_MATERIAL_PATH, layers: [{ name: 'Grass', blendType: 'LB_WeightBlend' }, { name: 'Rock', blendType: 'LB_HeightBlend' }] }, expected: 'success' },
    { scenario: 'ACTION: compile_material', toolName: 'manage_asset', arguments: { action: 'compile_material', assetPath: MATERIAL_PATH }, expected: 'success' },
    { scenario: 'INFO: get_material_info', toolName: 'manage_asset', arguments: { action: 'get_material_info', assetPath: MATERIAL_PATH }, expected: 'success' },
    { scenario: 'DELETE: delete_node batch', toolName: 'manage_asset', arguments: { action: 'delete_node', assetPath: MATERIAL_PATH, nodeIds: ['${captured:customExpressionNodeId}'] }, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete material instance', toolName: 'manage_asset', arguments: { action: 'delete', path: INSTANCE_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete authored material', toolName: 'manage_asset', arguments: { action: 'delete', path: MATERIAL_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete material function', toolName: 'manage_asset', arguments: { action: 'delete', path: FUNCTION_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete landscape layer', toolName: 'manage_asset', arguments: { action: 'delete', path: LANDSCAPE_LAYER_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete landscape material', toolName: 'manage_asset', arguments: { action: 'delete', path: LANDSCAPE_MATERIAL_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete decal material', toolName: 'manage_asset', arguments: { action: 'delete', path: DECAL_MATERIAL_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete post process material', toolName: 'manage_asset', arguments: { action: 'delete', path: POST_PROCESS_MATERIAL_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete texture asset', toolName: 'manage_asset', arguments: { action: 'delete', path: TEXTURE_PATH, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

// === TEXTURE ACTIONS ===
{
  /**
   * manage_asset texture action integration tests
   * Covers all 21 actions with proper setup/teardown sequencing.
   */

  const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
  const ts = Date.now();
  const NOISE_TEXTURE = `${TEST_FOLDER}/Testnoise_texture`;
  const GRADIENT_TEXTURE = `${TEST_FOLDER}/Testgradient_texture`;
  const PATTERN_TEXTURE = `${TEST_FOLDER}/Testpattern_texture`;
  const ENGINE_CUBE = '/Engine/EngineMeshes/Cube';

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },

    // === CREATE ===
    { scenario: 'CREATE: create_noise_texture', toolName: 'manage_asset', arguments: { action: 'create_noise_texture', name: 'Testnoise_texture', path: TEST_FOLDER, width: 64, height: 64 }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_gradient_texture', toolName: 'manage_asset', arguments: { action: 'create_gradient_texture', name: 'Testgradient_texture', path: TEST_FOLDER, width: 64, height: 64 }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_pattern_texture', toolName: 'manage_asset', arguments: { action: 'create_pattern_texture', name: 'Testpattern_texture', path: TEST_FOLDER, width: 64, height: 64 }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_normal_from_height', toolName: 'manage_asset', arguments: { action: 'create_normal_from_height', sourceTexture: NOISE_TEXTURE, name: 'Testnormal_from_height', path: TEST_FOLDER, strength: 1.0 }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_ao_from_mesh', toolName: 'manage_asset', arguments: { action: 'create_ao_from_mesh', meshPath: ENGINE_CUBE, name: 'Testao_from_mesh', path: TEST_FOLDER, width: 64, height: 64, samples: 8 }, expected: 'success|already exists' },
    // === ACTION ===
    { scenario: 'ACTION: resize_texture', toolName: 'manage_asset', arguments: { action: 'resize_texture', sourcePath: NOISE_TEXTURE, name: `Testresize_texture_${ts}`, path: TEST_FOLDER, newWidth: 32, newHeight: 32 }, expected: 'success' },
    { scenario: 'ACTION: resize_texture filterMethod Lanczos', toolName: 'manage_asset', arguments: { action: 'resize_texture', sourcePath: NOISE_TEXTURE, name: `Testresize_texture_lanczos_${ts}`, path: TEST_FOLDER, newWidth: 24, newHeight: 24, filterMethod: 'Lanczos', save: false }, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: adjust_levels', toolName: 'manage_asset', arguments: { action: 'adjust_levels', assetPath: NOISE_TEXTURE, inBlack: 0.1, inWhite: 0.9, gamma: 1.0 }, expected: 'success' },
    { scenario: 'CONFIG: adjust_curves', toolName: 'manage_asset', arguments: { action: 'adjust_curves', assetPath: NOISE_TEXTURE, curvePoints: [{ x: 0, y: 0 }, { x: 1, y: 1 }] }, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: blur', toolName: 'manage_asset', arguments: { action: 'blur', assetPath: NOISE_TEXTURE, radius: 1.0 }, expected: 'success' },
    { scenario: 'ACTION: sharpen', toolName: 'manage_asset', arguments: { action: 'sharpen', assetPath: NOISE_TEXTURE, amount: 0.5 }, expected: 'success' },
    { scenario: 'ACTION: invert', toolName: 'manage_asset', arguments: { action: 'invert', assetPath: NOISE_TEXTURE }, expected: 'success' },
    { scenario: 'ACTION: desaturate', toolName: 'manage_asset', arguments: { action: 'desaturate', assetPath: NOISE_TEXTURE, amount: 0.5 }, expected: 'success' },
    { scenario: 'ACTION: channel_pack', toolName: 'manage_asset', arguments: { action: 'channel_pack', name: `Testchannel_pack_${ts}`, path: TEST_FOLDER, redTexture: NOISE_TEXTURE, greenTexture: GRADIENT_TEXTURE, blueTexture: PATTERN_TEXTURE, width: 64, height: 64 }, expected: 'success' },
    { scenario: 'ACTION: channel_extract', toolName: 'manage_asset', arguments: { action: 'channel_extract', texturePath: NOISE_TEXTURE, channel: 'Red', name: `Testchannel_extract_${ts}` }, expected: 'success' },
    { scenario: 'ACTION: combine_textures', toolName: 'manage_asset', arguments: { action: 'combine_textures', name: `Testcombine_textures_${ts}`, path: TEST_FOLDER, baseTexture: NOISE_TEXTURE, blendTexture: GRADIENT_TEXTURE, blendMode: 'Multiply', opacity: 0.5 }, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_compression_settings', toolName: 'manage_asset', arguments: { action: 'set_compression_settings', assetPath: NOISE_TEXTURE, compressionSettings: 'TC_Default' }, expected: 'success' },
    { scenario: 'CONFIG: set_texture_group', toolName: 'manage_asset', arguments: { action: 'set_texture_group', assetPath: NOISE_TEXTURE, textureGroup: 'TEXTUREGROUP_World' }, expected: 'success' },
    { scenario: 'CONFIG: set_lod_bias', toolName: 'manage_asset', arguments: { action: 'set_lod_bias', assetPath: NOISE_TEXTURE, lodBias: 1 }, expected: 'success' },
    { scenario: 'CONFIG: configure_virtual_texture', toolName: 'manage_asset', arguments: { action: 'configure_virtual_texture', assetPath: NOISE_TEXTURE, virtualTextureStreaming: false }, expected: 'success' },
    { scenario: 'CONFIG: set_streaming_priority', toolName: 'manage_asset', arguments: { action: 'set_streaming_priority', assetPath: NOISE_TEXTURE, neverStream: true, streamingPriority: 1 }, expected: 'success' },
    // === INFO ===
    { scenario: 'INFO: get_texture_info', toolName: 'manage_asset', arguments: { action: 'get_texture_info', assetPath: NOISE_TEXTURE }, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('manage-asset', testCases);
