#!/usr/bin/env node
/**
 * Asset Management Test Suite (condensed)
 * Tool: manage_asset
 *
 * This file contains a focused 15-case suite that bootstraps the
 * minimal assets required and cleans up after itself. It avoids
 * large, editor-version-specific operations.
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
    {
        scenario: "List /Game root assets",
        toolName: "manage_asset",
        arguments: {
            action: "list",
            directory: "/Game"
        },
        expected: "returns list of assets"
    },
    {
        scenario: "Create test folder",
        toolName: "manage_asset",
        arguments: {
            action: "create_folder",
            path: "/Game/Tests/ManageAsset"
        },
        expected: "success - folder structure created"
    },
    {
        scenario: "Create master material",
        toolName: "manage_asset",
        arguments: {
            action: "create_material",
            name: "M_MasterMaterial_Test",
            path: "/Game/Tests/ManageAsset",
            materialType: "Master"
        },
        expected: "success - master material created"
    },
    {
        scenario: "Create material instance",
        toolName: "manage_asset",
        arguments: {
            action: "create_material_instance",
            name: "MI_TestInstance",
            path: "/Game/Tests/ManageAsset",
            parentMaterial: "/Game/Tests/ManageAsset/M_MasterMaterial_Test",
            parameters: {
                BaseColor: [0.5, 0.5, 0.8, 1.0]
            }
        },
        expected: "success - material instance created"
    },
    {
        scenario: "List test folder assets",
        toolName: "manage_asset",
        arguments: {
            action: "list",
            directory: "/Game/Tests/ManageAsset"
        },
        expected: "returns assets"
    },
    {
        scenario: "Duplicate material instance",
        toolName: "manage_asset",
        arguments: {
            action: "duplicate",
            sourcePath: "/Game/Tests/ManageAsset/MI_TestInstance",
            destinationPath: "/Game/Tests/ManageAsset/Copies",
            newName: "MI_TestInstance_Copy",
            save: true
        },
        expected: "success - asset duplicated with new name"
    },
    {
        scenario: "Rename duplicated material",
        toolName: "manage_asset",
        arguments: {
            action: "rename",
            assetPath: "/Game/Tests/ManageAsset/Copies/MI_TestInstance_Copy",
            newName: "MI_TestInstance_Renamed"
        },
        expected: "success - asset renamed"
    },
    {
        scenario: "Get dependencies of material instance",
        toolName: "manage_asset",
        arguments: {
            action: "get_dependencies",
            assetPath: "/Game/Tests/ManageAsset/MI_TestInstance"
        },
        expected: "success - asset dependencies retrieved"
    },
    {
        scenario: "Create thumbnail for material",
        toolName: "manage_asset",
        arguments: {
            action: "create_thumbnail",
            assetPath: "/Game/Tests/ManageAsset/M_MasterMaterial_Test"
        },
        expected: "success - thumbnail created"
    },
    {
        scenario: "Set tags on material",
        toolName: "manage_asset",
        arguments: {
            action: "set_tags",
            assetPath: "/Game/Tests/ManageAsset/M_MasterMaterial_Test",
            tags: ["TestMaterial", "AutoCreated"]
        },
        expected: "success - asset tags set"
    },
    {
        scenario: "List materials in test folder",
        toolName: "manage_asset",
        arguments: {
            action: "list",
            directory: "/Game/Tests/ManageAsset",
            filter: "Material"
        },
        expected: "success - filtered assets listed"
    },
    {
        scenario: "Generate asset report",
        toolName: "manage_asset",
        arguments: {
            action: "generate_report",
            directory: "/Game/Tests/ManageAsset",
            reportType: "Size",
            outputPath: "./tests/reports/asset_report_test.json"
        },
        expected: "success - asset report generated"
    },
    {
        scenario: "Validate material integrity",
        toolName: "manage_asset",
        arguments: {
            action: "validate",
            assetPath: "/Game/Tests/ManageAsset/M_MasterMaterial_Test"
        },
        expected: "success - asset validated"
    },
    {
        scenario: "Delete test copies and instances",
        toolName: "manage_asset",
        arguments: {
            action: "delete",
            assetPaths: [
                "/Game/Tests/ManageAsset/MI_TestInstance",
                "/Game/Tests/ManageAsset/Copies/MI_TestInstance_Renamed",
                "/Game/Tests/ManageAsset/M_MasterMaterial_Test"
            ]
        },
        expected: "success - assets deleted"
    },
    {
        scenario: "Asset Reorganization - Setup",
        toolName: "manage_asset",
        arguments: {
            action: "create_folder",
            path: "/Game/Tests/Reorg"
        },
        expected: "success"
    },
    {
        scenario: "Asset Reorganization - Create Assets",
        toolName: "manage_asset",
        arguments: {
            action: "create_material",
            name: "M_Original",
            path: "/Game/Tests/Reorg",
            materialType: "Master"
        },
        expected: "success"
    },
    {
        scenario: "Asset Reorganization - Move Asset",
        toolName: "manage_asset",
        arguments: {
            action: "move",
            assetPath: "/Game/Tests/Reorg/M_Original",
            destinationPath: "/Game/Tests/Reorg/Moved/M_Moved"
        },
        expected: "success"
    },
    {
        scenario: "Asset Reorganization - Fixup Redirectors",
        toolName: "manage_asset",
        arguments: {
            action: "fixup_redirectors",
            directory: "/Game/Tests/Reorg"
        },
        expected: "NOT_IMPLEMENTED"
    },
    {
        scenario: "Deep Duplication - Setup Source",
        toolName: "manage_asset",
        arguments: {
            action: "create_folder",
            path: "/Game/Tests/DeepCopy/Source"
        },
        expected: "success"
    },
    {
        scenario: "Deep Duplication - Create Source Asset",
        toolName: "manage_asset",
        arguments: {
            action: "create_material",
            name: "M_Source",
            path: "/Game/Tests/DeepCopy/Source",
            materialType: "Master"
        },
        expected: "success"
    },
    {
        scenario: "Deep Duplication - Duplicate Folder",
        toolName: "manage_asset",
        arguments: {
            action: "duplicate",
            sourcePath: "/Game/Tests/DeepCopy/Source",
            destinationPath: "/Game/Tests/DeepCopy/Target",
            save: true
        },
        expected: "success"
    },
    {
        scenario: "Cleanup Reorg and DeepCopy",
        toolName: "manage_asset",
        arguments: {
            action: "delete",
            assetPaths: [
                "/Game/Tests/Reorg",
                "/Game/Tests/DeepCopy"
            ]
        },
        expected: "success or handled"
    }
];

await runToolTests('Asset Management', testCases);
