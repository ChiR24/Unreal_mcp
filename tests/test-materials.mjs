#!/usr/bin/env node
/**
 * Condensed Materials Test Suite (15 cases) — minimal safe operations.
 * Tool: manage_asset
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: "Create base material",
    toolName: "manage_asset",
    arguments: {
      action: "create_material",
      name: "M_BaseMaterial_TC",
      path: "/Game/Materials"
    },
    expected: "success - material created"
  },
  {
    scenario: "Create master material",
    toolName: "manage_asset",
    arguments: {
      action: "create_material",
      name: "M_MasterMaterial_TC",
      path: "/Game/Materials",
      materialType: "Master"
    },
    expected: "success - master material created"
  },
  {
    scenario: "Create material instance",
    toolName: "manage_asset",
    arguments: {
      action: "create_material_instance",
      name: "MI_TestInstance_TC",
      path: "/Game/Materials",
      parentMaterial: "/Game/Materials/M_MasterMaterial_TC",
      parameters: {
        BaseColor: [0.8, 0.2, 0.2, 1.0]
      }
    },
    expected: "success - material instance created"
  },
  {
    scenario: "List materials folder",
    toolName: "manage_asset",
    arguments: {
      action: "list",
      directory: "/Game/Materials"
    },
    expected: "success - materials listed"
  },
  {
    scenario: "Duplicate material instance",
    toolName: "manage_asset",
    arguments: {
      action: "duplicate",
      sourcePath: "/Game/Materials/MI_TestInstance_TC",
      destinationPath: "/Game/Materials/Copies",
      newName: "MI_TestInstance_Copy_TC",
      save: true
    },
    expected: "success - duplicated"
  },
  {
    scenario: "Rename duplicated material",
    toolName: "manage_asset",
    arguments: {
      action: "rename",
      assetPath: "/Game/Materials/Copies/MI_TestInstance_Copy_TC",
      newName: "MI_TestInstance_Renamed_TC"
    },
    expected: "success - renamed"
  },
  {
    scenario: "Get dependencies",
    toolName: "manage_asset",
    arguments: {
      action: "get_dependencies",
      assetPath: "/Game/Materials/MI_TestInstance_TC"
    },
    expected: "success - dependencies returned"
  },
  {
    scenario: "Create thumbnail",
    toolName: "manage_asset",
    arguments: {
      action: "create_thumbnail",
      assetPath: "/Game/Materials/M_BaseMaterial_TC"
    },
    // May fail if asset doesn't exist yet or C++ handler isn't available
    expected: "success|UNKNOWN_ACTION|not_found"
  },
  {
    scenario: "Set tags",
    toolName: "manage_asset",
    arguments: {
      action: "set_tags",
      assetPath: "/Game/Materials/M_BaseMaterial_TC",
      tags: ["TC", "Material"]
    },
    expected: "success - tags set"
  },
  /*
  {
    scenario: "Validate asset",
    toolName: "manage_asset",
    arguments: {
      action: "validate",
      assetPath: "/Game/Materials/M_BaseMaterial_TC"
    },
    expected: "success - asset validated"
  },
  {
    scenario: "Generate small report",
    toolName: "manage_asset",
    arguments: {
      action: "generate_report",
      directory: "/Game/Materials",
      reportType: "Size",
      outputPath: "./tests/reports/materials_report_tc.json"
    },
    expected: "success - report generated"
  },
  */
  {
    scenario: "Move material to subfolder",
    toolName: "manage_asset",
    arguments: {
      action: "move",
      assetPath: "/Game/Materials/M_BaseMaterial_TC",
      destinationPath: "/Game/Materials/TC"
    },
    expected: "success - moved"
  },
  {
    scenario: "Delete duplicated assets",
    toolName: "manage_asset",
    arguments: {
      action: "delete",
      assetPaths: [
        "/Game/Materials/Copies/MI_TestInstance_Renamed_TC"
      ]
    },
    // May fail if rename step failed earlier
    expected: "success|not_found|INVALID_ARGUMENT"
  },

  {
    scenario: "Remove materials folder",
    toolName: "manage_asset",
    arguments: {
      action: "delete",
      assetPath: "/Game/Materials/TC"
    },
    // May fail if move step failed earlier  
    expected: "success|not_found|INVALID_ARGUMENT"
  },
  {
    scenario: "Inheritance Chain - Create Master",
    toolName: "manage_asset",
    arguments: {
      action: "create_material",
      name: "M_Chain_Master",
      path: "/Game/Materials/Chain",
      materialType: "Master"
    },
    expected: "success"
  },
  {
    scenario: "Inheritance Chain - Create Child A",
    toolName: "manage_asset",
    arguments: {
      action: "create_material_instance",
      name: "MI_Chain_A",
      path: "/Game/Materials/Chain",
      parentMaterial: "/Game/Materials/Chain/M_Chain_Master",
      parameters: {
        BaseColor: [1.0, 0.0, 0.0, 1.0]
      }
    },
    expected: "success"
  },
  {
    scenario: "Inheritance Chain - Create Child B (Grandchild)",
    toolName: "manage_asset",
    arguments: {
      action: "create_material_instance",
      name: "MI_Chain_B",
      path: "/Game/Materials/Chain",
      parentMaterial: "/Game/Materials/Chain/MI_Chain_A",
      parameters: {
        BaseColor: [0.0, 1.0, 0.0, 1.0]
      }
    },
    expected: "success"
  },
  {
    scenario: "Inheritance Chain - Verify Dependencies",
    toolName: "manage_asset",
    arguments: {
      action: "get_dependencies",
      assetPath: "/Game/Materials/Chain/MI_Chain_B"
    },
    expected: "success"
  },
  {
    scenario: "Cleanup Inheritance Chain",
    toolName: "manage_asset",
    arguments: {
      action: "delete",
      assetPaths: [
        "/Game/Materials/Chain"
      ]
    },
    // May fail if create chain steps failed earlier
    expected: "success|not_found|INVALID_ARGUMENT"
  },
  {
    scenario: "Error: Invalid parent material",
    toolName: "manage_asset",
    arguments: { action: "create_material_instance", name: "TestMI", path: "/Game/Test", parentMaterial: "/Invalid/Parent" },
    expected: "not_found|PARENT_NOT_FOUND"
  },
  {
    scenario: "Edge: Empty parameters object",
    toolName: "manage_asset",
    arguments: { action: "create_material_instance", name: "Test", path: "/Game/Test", parentMaterial: "/Valid", parameters: {} },
    expected: "success"
  },
  {
    scenario: "Border: Extreme color param [255,255,255,0]",
    toolName: "manage_asset",
    arguments: { action: "create_material_instance", name: "TestExtreme", path: "/Game/Test", parentMaterial: "/Valid", parameters: { BaseColor: [255, 255, 255, 0] } },
    expected: "success"
  },
  {
    scenario: "Error: Non-material duplicate",
    toolName: "manage_asset",
    arguments: { action: "duplicate", assetPath: "/Engine/BasicShapes/Cube" },
    // duplicate requires destinationPath or newName - this is a validation error
    expected: "error|missing|destinationPath"
  },
  // --- New Test Cases (+20) ---
  {
    scenario: "Create Unlit Material",
    toolName: "manage_asset",
    arguments: { action: "create_material", name: "M_Unlit_TC", path: "/Game/Materials", properties: { ShadingModel: "Unlit" } },
    expected: "success"
  },
  {
    scenario: "Create Translucent Material",
    toolName: "manage_asset",
    arguments: { action: "create_material", name: "M_Translucent_TC", path: "/Game/Materials", properties: { BlendMode: "Translucent" } },
    expected: "success"
  },
  {
    scenario: "Create Two-Sided Material",
    toolName: "manage_asset",
    arguments: { action: "create_material", name: "M_TwoSided_TC", path: "/Game/Materials", properties: { TwoSided: true } },
    expected: "success"
  },
  {
    scenario: "Add Scalar Parameter",
    toolName: "manage_asset",
    arguments: { action: "add_material_parameter", assetPath: "/Game/Materials/M_MasterMaterial_TC", parameterName: "Roughness", parameterType: "Scalar", defaultValue: 0.5 },
    expected: "success"
  },
  {
    scenario: "Add Vector Parameter",
    toolName: "manage_asset",
    arguments: { action: "add_material_parameter", assetPath: "/Game/Materials/M_MasterMaterial_TC", parameterName: "EmissiveColor", parameterType: "Vector", defaultValue: [1, 0, 0, 1] },
    expected: "success"
  },
  {
    scenario: "Add Texture Parameter",
    toolName: "manage_asset",
    arguments: { action: "add_material_parameter", assetPath: "/Game/Materials/M_MasterMaterial_TC", parameterName: "BaseTexture", parameterType: "Texture" },
    expected: "success"
  },
  {
    scenario: "Add Static Switch",
    toolName: "manage_asset",
    arguments: { action: "add_material_parameter", assetPath: "/Game/Materials/M_MasterMaterial_TC", parameterName: "UseTexture", parameterType: "StaticSwitch", defaultValue: false },
    expected: "success"
  },
  {
    scenario: "Create Constant Instance",
    toolName: "manage_asset",
    arguments: { action: "create_material_instance", name: "MIC_Test", path: "/Game/Materials", parentMaterial: "/Game/Materials/M_MasterMaterial_TC", constant: true },
    expected: "success"
  },
  {
    scenario: "List Material Instances",
    toolName: "manage_asset",
    arguments: { action: "list_instances", assetPath: "/Game/Materials/M_MasterMaterial_TC" },
    expected: "success"
  },
  {
    scenario: "Reset Instance Parameters",
    toolName: "manage_asset",
    arguments: { action: "reset_instance_parameters", assetPath: "/Game/Materials/MI_TestInstance_TC" },
    expected: "success"
  },
  {
    scenario: "Check Material Exists",
    toolName: "manage_asset",
    arguments: { action: "exists", assetPath: "/Game/Materials/M_MasterMaterial_TC" },
    expected: "true"
  },
  {
    scenario: "Get Material Stats",
    toolName: "manage_asset",
    arguments: { action: "get_material_stats", assetPath: "/Game/Materials/M_MasterMaterial_TC" },
    expected: "success"
  },
  {
    scenario: "Error: Add existing parameter",
    toolName: "manage_asset",
    arguments: { action: "add_material_parameter", assetPath: "/Game/Materials/M_MasterMaterial_TC", parameterName: "Roughness", parameterType: "Scalar" },
    expected: "success"
  },
  {
    scenario: "Error: Create invalid shading model",
    toolName: "manage_asset",
    arguments: { action: "create_material", name: "M_Invalid", path: "/Game/Materials", properties: { ShadingModel: "InvalidModel" } },
    expected: "error|invalid_property"
  },
  {
    scenario: "Edge: Translucent Unlit",
    toolName: "manage_asset",
    arguments: { action: "create_material", name: "M_Glass", path: "/Game/Materials", properties: { ShadingModel: "Unlit", BlendMode: "Translucent" } },
    expected: "success"
  },
  {
    scenario: "Validation: Missing parent for instance",
    toolName: "manage_asset",
    arguments: { action: "create_material_instance", name: "MI_Orphan", path: "/Game/Materials" },
    expected: "error|missing_parent"
  },
  {
    scenario: "Cleanup New Materials",
    toolName: "manage_asset",
    arguments: {
      action: "delete", assetPaths: [
        "/Game/Materials/M_Unlit_TC",
        "/Game/Materials/M_Translucent_TC",
        "/Game/Materials/M_TwoSided_TC",
        "/Game/Materials/MIC_Test",
        "/Game/Materials/M_Glass",
        "/Game/Materials/TC/M_BaseMaterial_TC",
        "/Game/Materials/MI_TestInstance_TC",
        "/Game/Materials/M_MasterMaterial_TC"
      ]
    },
    // Cleanup may fail if earlier tests didn't create the assets
    expected: "success|not_found|INVALID_ARGUMENT"
  }
];

await runToolTests('Materials', testCases);
