#!/usr/bin/env node
/**
 * Advanced Asset Tools Test Suite
 * Tool: manage_asset
 *
 * This file tests newer/advanced capabilities of the manage_asset tool:
 * - create_thumbnail
 * - analyze_graph
 * - get_source_control_state
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
    {
        scenario: "Setup: Create test folder",
        toolName: "manage_asset",
        arguments: {
            action: "create_folder",
            path: "/Game/Tests/AdvancedAssets"
        },
        expected: "success"
    },
    {
        scenario: "Setup: Create base material",
        toolName: "manage_asset",
        arguments: {
            action: "create_material",
            name: "M_Base",
            path: "/Game/Tests/AdvancedAssets",
            materialType: "Master"
        },
        expected: "success"
    },
    {
        scenario: "Create Thumbnail",
        toolName: "manage_asset",
        arguments: {
            action: "create_thumbnail",
            assetPath: "/Game/Tests/AdvancedAssets/M_Base"
        },
        expected: "success - thumbnail created"
    },
    {
        scenario: "Analyze Dependency Graph",
        toolName: "manage_asset",
        arguments: {
            action: "analyze_graph",
            assetPath: "/Game/Tests/AdvancedAssets/M_Base",
            maxDepth: 1
        },
        expected: "success - graph analyzed"
    },
    {
        scenario: "Get Source Control State",
        toolName: "manage_asset",
        arguments: {
            action: "get_source_control_state",
            assetPath: "/Game/Tests/AdvancedAssets/M_Base"
        },
        expected: "success - state retrieved"
    },
    {
        scenario: "Cleanup: Delete test folder",
        toolName: "manage_asset",
        arguments: {
            action: "delete_assets",
            assetPaths: ["/Game/Tests/AdvancedAssets"]
        },
        expected: "success"
    }
];

await runToolTests('Advanced Asset Tools', testCases);
