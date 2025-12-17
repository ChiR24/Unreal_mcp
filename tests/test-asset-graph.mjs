#!/usr/bin/env node
/**
 * Material Graph & Advanced Asset Test Suite
 * Tool: manage_asset (material graph operations)
 * Coverage: Material graph nodes, search, analyze, nanite
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
    // === PRE-CLEANUP ===
    {
        scenario: 'Pre-cleanup: Delete test materials',
        toolName: 'manage_asset',
        arguments: { action: 'delete', assetPaths: ['/Game/Tests/Materials/M_GraphTest', '/Game/Tests/Materials/M_NaniteTest'] },
        expected: 'success|not_found'
    },
    {
        scenario: 'Create test folder',
        toolName: 'manage_asset',
        arguments: { action: 'create_folder', path: '/Game/Tests/Materials' },
        expected: 'success'
    },

    // === SEARCH ASSETS ===
    {
        scenario: 'Search assets by name',
        toolName: 'manage_asset',
        arguments: { action: 'search_assets', filter: 'Cube', packagePaths: ['/Engine'] },
        expected: 'success'
    },
    {
        scenario: 'Search assets by class',
        toolName: 'manage_asset',
        arguments: { action: 'search_assets', classNames: ['Material'], packagePaths: ['/Engine'], limit: 10 },
        expected: 'success'
    },
    {
        scenario: 'Search assets recursive',
        toolName: 'manage_asset',
        arguments: { action: 'search_assets', packagePaths: ['/Game'], recursiveClasses: true, recursivePaths: true },
        expected: 'success'
    },

    // === ANALYZE GRAPH ===
    {
        scenario: 'Analyze material graph',
        toolName: 'manage_asset',
        arguments: { action: 'analyze_graph', assetPath: '/Engine/EngineMaterials/WorldGridMaterial', maxDepth: 3 },
        expected: 'success'
    },
    {
        scenario: 'Get asset graph',
        toolName: 'manage_asset',
        arguments: { action: 'get_asset_graph', assetPath: '/Engine/EngineMaterials/WorldGridMaterial' },
        expected: 'success'
    },

    // === CREATE MATERIAL FOR GRAPH TESTS ===
    {
        scenario: 'Create material for graph tests',
        toolName: 'manage_asset',
        arguments: { action: 'create_material', name: 'M_GraphTest', path: '/Game/Tests/Materials', materialType: 'Master' },
        expected: 'success'
    },

    // === MATERIAL GRAPH: ADD NODES ===
    {
        scenario: 'Add Constant3Vector node',
        toolName: 'manage_asset',
        arguments: {
            action: 'add_material_node',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            nodeType: 'Constant3Vector',
            x: -300,
            y: 0
        },
        expected: 'success'
    },
    {
        scenario: 'Add Texture Sample node',
        toolName: 'manage_asset',
        arguments: {
            action: 'add_material_node',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            nodeType: 'TextureSample',
            x: -400,
            y: 100
        },
        expected: 'success'
    },
    {
        scenario: 'Add Multiply node',
        toolName: 'manage_asset',
        arguments: {
            action: 'add_material_node',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            nodeType: 'Multiply',
            x: -150,
            y: 50
        },
        expected: 'success'
    },
    {
        scenario: 'Add Scalar Parameter node',
        toolName: 'manage_asset',
        arguments: {
            action: 'add_material_node',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            nodeType: 'ScalarParameter',
            parameterName: 'Roughness',
            x: -300,
            y: 200
        },
        expected: 'success'
    },
    {
        scenario: 'Add Vector Parameter node',
        toolName: 'manage_asset',
        arguments: {
            action: 'add_material_node',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            nodeType: 'VectorParameter',
            parameterName: 'BaseColor',
            x: -300,
            y: 300
        },
        expected: 'success'
    },
    {
        scenario: 'Add Add node',
        toolName: 'manage_asset',
        arguments: {
            action: 'add_material_node',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            nodeType: 'Add',
            x: -200,
            y: 150
        },
        expected: 'success'
    },

    // === MATERIAL GRAPH: GET NODE DETAILS ===
    {
        scenario: 'Get material node details',
        toolName: 'manage_asset',
        arguments: {
            action: 'get_material_node_details',
            assetPath: '/Game/Tests/Materials/M_GraphTest'
        },
        expected: 'success'
    },

    // === MATERIAL GRAPH: CONNECT PINS ===
    {
        scenario: 'Connect Constant3Vector to BaseColor',
        toolName: 'manage_asset',
        arguments: {
            action: 'connect_material_pins',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            fromNodeId: 'Constant3Vector_0',
            fromPin: 'RGB',
            toNodeId: 'Material',
            toPin: 'BaseColor'
        },
        expected: 'success|not_found'
    },
    {
        scenario: 'Connect ScalarParameter to Roughness',
        toolName: 'manage_asset',
        arguments: {
            action: 'connect_material_pins',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            fromNodeId: 'ScalarParameter_Roughness',
            fromPin: 'Output',
            toNodeId: 'Material',
            toPin: 'Roughness'
        },
        expected: 'success|not_found'
    },

    // === MATERIAL GRAPH: BREAK CONNECTIONS ===
    {
        scenario: 'Break material connections',
        toolName: 'manage_asset',
        arguments: {
            action: 'break_material_connections',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            nodeId: 'Material',
            pinName: 'BaseColor'
        },
        expected: 'success|not_found'
    },

    // === MATERIAL GRAPH: REMOVE NODE ===
    {
        scenario: 'Remove Add node',
        toolName: 'manage_asset',
        arguments: {
            action: 'remove_material_node',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            nodeId: 'Add_0'
        },
        expected: 'success|not_found'
    },

    // === REBUILD MATERIAL ===
    {
        scenario: 'Rebuild material',
        toolName: 'manage_asset',
        arguments: {
            action: 'rebuild_material',
            assetPath: '/Game/Tests/Materials/M_GraphTest',
            save: true
        },
        expected: 'success'
    },

    // === NANITE OPERATIONS ===
    {
        scenario: 'Create dummy mesh for Nanite test',
        toolName: 'manage_asset',
        arguments: { action: 'duplicate', sourcePath: '/Engine/BasicShapes/Cube', destinationPath: '/Game/Tests/Materials', newName: 'SM_NaniteTest' },
        expected: 'success'
    },
    {
        scenario: 'Nanite rebuild mesh',
        toolName: 'manage_asset',
        arguments: {
            action: 'nanite_rebuild_mesh',
            meshPath: '/Game/Tests/Materials/SM_NaniteTest'
        },
        expected: 'success|not_supported'
    },

    // === GENERATE LODs ===
    {
        scenario: 'Generate LODs for mesh',
        toolName: 'manage_asset',
        arguments: {
            action: 'generate_lods',
            meshPath: '/Game/Tests/Materials/SM_NaniteTest',
            lodCount: 3
        },
        expected: 'success'
    },

    // === CREATE RENDER TARGET ===
    {
        scenario: 'Create render target',
        toolName: 'manage_asset',
        arguments: {
            action: 'create_render_target',
            name: 'RT_Test',
            path: '/Game/Tests/Materials',
            width: 512,
            height: 512
        },
        expected: 'success'
    },

    // === ERROR CASES ===
    {
        scenario: 'Error: Add node to invalid material',
        toolName: 'manage_asset',
        arguments: { action: 'add_material_node', assetPath: '/Game/Invalid/Material', nodeType: 'Constant' },
        expected: 'error|not_found'
    },
    {
        scenario: 'Error: Connect invalid pins',
        toolName: 'manage_asset',
        arguments: { action: 'connect_material_pins', assetPath: '/Game/Tests/Materials/M_GraphTest', fromNodeId: 'Invalid', fromPin: 'Invalid', toNodeId: 'Invalid', toPin: 'Invalid' },
        expected: 'error|not_found'
    },
    {
        scenario: 'Error: Search with empty paths',
        toolName: 'manage_asset',
        arguments: { action: 'search_assets', packagePaths: [] },
        expected: 'success|error'
    },
    {
        scenario: 'Error: Nanite on non-mesh',
        toolName: 'manage_asset',
        arguments: { action: 'nanite_rebuild_mesh', meshPath: '/Game/Tests/Materials/M_GraphTest' },
        expected: 'error'
    },

    // === EDGE CASES ===
    {
        scenario: 'Edge: Negative node position',
        toolName: 'manage_asset',
        arguments: { action: 'add_material_node', assetPath: '/Game/Tests/Materials/M_GraphTest', nodeType: 'Constant', x: -9999, y: -9999 },
        expected: 'success'
    },
    {
        scenario: 'Edge: Very large render target',
        toolName: 'manage_asset',
        arguments: { action: 'create_render_target', name: 'RT_Large', path: '/Game/Tests/Materials', width: 4096, height: 4096 },
        expected: 'success'
    },

    // === CLEANUP ===
    {
        scenario: 'Cleanup: Delete test materials folder',
        toolName: 'manage_asset',
        arguments: { action: 'delete', assetPaths: ['/Game/Tests/Materials'] },
        expected: 'success|not_found'
    }
];

await runToolTests('Material Graph & Advanced Assets', testCases);
