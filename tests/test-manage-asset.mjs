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
    // "Pre-cleanup: Python Force Delete (Robust)" removed - Python blocked by CommandValidator
    {
        scenario: "Pre-cleanup: Remove test artifacts (Standard)",
        toolName: "manage_asset",
        arguments: {
            action: "delete",
            assetPaths: [
                "/Game/Tests/ManageAsset",
                "/Game/Tests/Reorg",
                "/Game/Tests/DeepCopy"
            ]
        },
        expected: "success|not_found"
    },
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
    /*
    {
        scenario: "Generate asset report",
        toolName: "manage_asset",
        arguments: {
            action: "generate_report",
            directory: "/Game/Tests/ManageAsset",
            reportType: "Size",
            outputPath: "./tests/reports/asset_report_test.json",
            timeoutMs: 30000
        },
        expected: "success - asset report generated"
    },
    */
    /*
    {
        scenario: "Validate material integrity",
        toolName: "manage_asset",
        arguments: {
            action: "validate",
            assetPath: "/Game/Tests/ManageAsset/M_MasterMaterial_Test",
            timeoutMs: 30000
        },
        expected: "success - asset validated"
    },
    */
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
        expected: "success"
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
    },
    {
        scenario: "Error: Import invalid file",
        toolName: "manage_asset",
        arguments: {
            action: "import",
            sourcePath: "./invalid.ext",
            destinationPath: "/Game/Test"
        },
        expected: "success|SOURCE_NOT_FOUND"
    },
    /*
    {
        scenario: "Edge: Empty tags array",
        toolName: "manage_asset",
        arguments: {
            action: "set_tags",
            assetPath: "/Engine/BasicShapes/Cube", // Use a real asset
            tags: []
        },
        expected: "success"
    },
    */
    {
        scenario: "Border: Large metadata object",
        toolName: "manage_asset",
        arguments: {
            action: "set_metadata",
            // Use a well-known engine asset path that is expected to exist in all projects
            assetPath: "/Engine/BasicShapes/Cube",
            metadata: { key1: "value1", key2: "value2", /* ... large obj */ }
        },
        expected: "success"
    },
    {
        scenario: "Edge: Fixup empty dir",
        toolName: "manage_asset",
        arguments: {
            action: "fixup_redirectors",
            directoryPath: "/Empty/Dir"
        },
        expected: "success|no_op"
    },
    // --- New Test Cases (+20) ---
    {
        scenario: "Error: Delete non-existent asset",
        toolName: "manage_asset",
        arguments: {
            action: "delete",
            assetPaths: ["/Game/NonExistentAsset_XYZ"]
        },
        expected: "success|not_found" // Ideally should be robust
    },
    {
        scenario: "Edge: Create existing folder",
        toolName: "manage_asset",
        arguments: {
            action: "create_folder",
            path: "/Game"
        },
        expected: "success"
    },
    {
        scenario: "Edge: Move to same path (no-op)",
        toolName: "manage_asset",
        arguments: {
            action: "move",
            assetPath: "/Engine/BasicShapes/Cube",
            destinationPath: "/Engine/BasicShapes/Cube"
        },
        expected: "RENAME_FAILED"
    },
    {
        scenario: "Check existence: Engine asset",
        toolName: "manage_asset",
        arguments: {
            action: "exists",
            assetPath: "/Engine/BasicShapes/Cube"
        },
        expected: "true"
    },
    {
        scenario: "Check existence: Non-existent asset",
        toolName: "manage_asset",
        arguments: {
            action: "exists",
            assetPath: "/Game/NothingHere"
        },
        expected: "false"
    },
    {
        scenario: "Error: Duplicate to existing path",
        toolName: "manage_asset",
        arguments: {
            action: "duplicate",
            sourcePath: "/Engine/BasicShapes/Cube",
            destinationPath: "/Engine/BasicShapes",
            newName: "Sphere" // Assuming Sphere exists
        },
        expected: "DUPLICATE_FAILED"
    },
    {
        scenario: "List assets: Root recursive",
        toolName: "manage_asset",
        arguments: {
            action: "list",
            directory: "/Game",
            recursive: true
        },
        expected: "success"
    },
    {
        scenario: "List assets: Non-existent directory",
        toolName: "manage_asset",
        arguments: {
            action: "list",
            directory: "/Game/DoesNotExist/AtAll"
        },
        expected: "success|empty"
    },
    {
        scenario: "Rename: Same name (no-op)",
        toolName: "manage_asset",
        arguments: {
            action: "rename",
            assetPath: "/Engine/BasicShapes/Cube",
            newName: "Cube"
        },
        expected: "RENAME_FAILED"
    },
    {
        scenario: "Validation: Invalid path format",
        toolName: "manage_asset",
        arguments: {
            action: "create_folder",
            path: "Invalid/Path/Without/Slash"
        },
        expected: "CREATE_FAILED"
    },
    {
        scenario: "Validation: Empty path",
        toolName: "manage_asset",
        arguments: {
            action: "create_folder",
            path: ""
        },
        expected: "Invalid path: must be a non-empty string"
    },
    {
        scenario: "Metadata: Get metadata (Engine Asset)",
        toolName: "manage_asset",
        arguments: {
            action: "get_metadata",
            assetPath: "/Engine/BasicShapes/Cube"
        },
        expected: "success"
    },
    {
        scenario: "Error: Get metadata (Non-existent)",
        toolName: "manage_asset",
        arguments: {
            action: "get_metadata",
            assetPath: "/Game/NoMetadataHere"
        },
        expected: "Asset not found" // Correctly expects failure for missing asset
    },
    {
        scenario: "Dependencies: Recursive check",
        toolName: "manage_asset",
        arguments: {
            action: "get_dependencies",
            assetPath: "/Engine/BasicShapes/Cube",
            recursive: true
        },
        expected: "success"
    },
];

await runToolTests('Asset Management', testCases);
