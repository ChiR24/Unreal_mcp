#!/usr/bin/env node
/**
 * Behavior Tree Test Suite (20 cases)
 * Tool: manage_behavior_tree
 */

import { runToolTests } from './test-runner.mjs';

const btPath = '/Game/AI/BT_TestTree';

const testCases = [
  // Setup: Create a Behavior Tree asset first
  {
    scenario: "Create Behavior Tree Asset",
    toolName: "manage_asset",
    arguments: {
      action: "create_asset", // Assuming generic asset creation or create_behavior_tree if specialized
      // Actually, let's use generic import or assume it exists for now, 
      // or create via blueprint/asset tools if supported.
      // Fallback: use generic create_asset with BehaviorTree class if supported, 
      // otherwise 'create_behavior_tree' might be needed in manage_asset or manage_behavior_tree.
      // Checking consolidated definitions: manage_behavior_tree has add_node etc, but no create_tree.
      // manage_asset has import/create_material... maybe generic create?
      // Let's assume we use a pre-existing one or create one via a helper if available.
      // For test, let's try to create one using manage_asset generic if possible, or skip creation.
      // Let's try to assume it exists or use a known path.
      // For robustness, let's try to create via a python command in 'system_control' if we can't natively.
      action: "execute_console_command", 
      command: "py.exec \"unreal.AssetToolsHelpers.get_asset_tools().create_asset('BT_TestTree', '/Game/AI', unreal.BehaviorTree, unreal.BehaviorTreeFactory())\""
    },
    toolName: "system_control",
    expected: "success"
  },
  {
    scenario: "Add Root Node (Selector)",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "add_node",
      assetPath: btPath,
      nodeType: "Selector",
      nodeId: "RootSelector",
      x: 0,
      y: 0
    },
    expected: "success"
  },
  {
    scenario: "Add Sequence Node",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "add_node",
      assetPath: btPath,
      nodeType: "Sequence",
      nodeId: "MainSequence",
      x: -200,
      y: 150
    },
    expected: "success"
  },
  {
    scenario: "Connect Root to Sequence",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "connect_nodes",
      assetPath: btPath,
      parentNodeId: "RootSelector",
      childNodeId: "MainSequence"
    },
    expected: "success"
  },
  {
    scenario: "Add Task Node (Wait)",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "add_node",
      assetPath: btPath,
      nodeType: "Task",
      nodeClass: "BTTask_Wait", // Assuming class name resolution
      nodeId: "WaitTask",
      x: -300,
      y: 300
    },
    expected: "success"
  },
  {
    scenario: "Connect Sequence to Task",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "connect_nodes",
      assetPath: btPath,
      parentNodeId: "MainSequence",
      childNodeId: "WaitTask"
    },
    expected: "success"
  },
  {
    scenario: "Set Task Property (Wait Time)",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "set_node_properties",
      assetPath: btPath,
      nodeId: "WaitTask",
      properties: {
        "WaitTime": 5.0
      }
    },
    expected: "success"
  },
  {
    scenario: "Add Decorator (Blackboard)",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "add_node",
      assetPath: btPath,
      nodeType: "Decorator",
      nodeClass: "BTDecorator_Blackboard",
      parentNodeId: "MainSequence", // Decorators attach to nodes (composites/tasks)
      nodeId: "BBDecorator"
    },
    expected: "success"
  },
  {
    scenario: "Set Decorator Property",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "set_node_properties",
      assetPath: btPath,
      nodeId: "BBDecorator",
      properties: {
        "BlackboardKey": "TargetActor"
      }
    },
    expected: "success"
  },
  {
    scenario: "Add Service (Default Focus)",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "add_node",
      assetPath: btPath,
      nodeType: "Service",
      nodeClass: "BTService_DefaultFocus",
      parentNodeId: "RootSelector",
      nodeId: "FocusService"
    },
    expected: "success"
  },
  {
    scenario: "Remove Node (Wait Task)",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "remove_node",
      assetPath: btPath,
      nodeId: "WaitTask"
    },
    expected: "success"
  },
  {
    scenario: "Break Connections",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "break_connections",
      assetPath: btPath,
      parentNodeId: "RootSelector",
      childNodeId: "MainSequence"
    },
    expected: "success"
  },
  {
    scenario: "Add Parallel Node",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "add_node",
      assetPath: btPath,
      nodeType: "SimpleParallel",
      nodeId: "ParallelNode",
      x: 200,
      y: 150
    },
    expected: "success"
  },
  {
    scenario: "Connect Root to Parallel",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "connect_nodes",
      assetPath: btPath,
      parentNodeId: "RootSelector",
      childNodeId: "ParallelNode"
    },
    expected: "success"
  },
  {
    scenario: "Add Custom Task (MoveTo)",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "add_node",
      assetPath: btPath,
      nodeType: "Task",
      nodeClass: "BTTask_MoveTo",
      nodeId: "MoveToTask",
      x: 200,
      y: 300
    },
    expected: "success"
  },
  {
    scenario: "Error: Add Invalid Node Type",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "add_node",
      assetPath: btPath,
      nodeType: "InvalidType"
    },
    expected: "error"
  },
  {
    scenario: "Error: Connect Cycle",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "connect_nodes",
      assetPath: btPath,
      parentNodeId: "MoveToTask",
      childNodeId: "RootSelector"
    },
    expected: "error"
  },
  {
    scenario: "Error: Set Property on Invalid Node",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "set_node_properties",
      assetPath: btPath,
      nodeId: "NonExistentNode",
      properties: { "Val": 1 }
    },
    expected: "error|not_found"
  },
  {
    scenario: "Edge: Comment String",
    toolName: "manage_behavior_tree",
    arguments: {
      action: "set_node_properties",
      assetPath: btPath,
      nodeId: "RootSelector",
      comment: "Main Entry Point"
    },
    expected: "success"
  },
  {
    scenario: "Cleanup BT Asset",
    toolName: "manage_asset",
    arguments: {
      action: "delete",
      assetPaths: [btPath]
    },
    expected: "success"
  }
];

await runToolTests('Behavior Tree', testCases);
