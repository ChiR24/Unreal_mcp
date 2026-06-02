#!/usr/bin/env node
/**
 * manage_behavior_tree.get_tree read introspection integration test.
 *
 * Builds a BT fixture (Root -> Selector -> Sequence -> Wait) with a root-level Service,
 * stacked edge decorators (Blackboard + CompareBBEntries + Cooldown) on the Selector->Sequence
 * edge, and a root-sentinel decorator, then calls get_tree and asserts the navigable hierarchy,
 * per-edge decorator attribution, multi-selector keyProperties (R16), and the null-RootNode
 * contract. Also asserts the get_ai_info BB enrichment added in PR1a.
 *
 * MUST run via the dev dist (the test runner spawns `node dist/cli.js`) — the Claude Code MCP
 * connection caches session-start TS (memory reference_claude_hook_timing). Build first:
 *   npm run build:core
 *   node tests/mcp-tools/utility/behavior-tree-get-tree.test.mjs
 *
 * Spec: docs/superpowers/specs/2026-05-03-bt-bb-introspect-design.md
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GetTree_${ts}`;
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1); // leading-slash stripped (savePath form)
const bbName = `BB_GetTreeFixture_${ts}`;
const btName = `BT_GetTreeFixture_${ts}`;
const btEmptyName = `BT_GetTreeEmpty_${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: folder', toolName: 'manage_asset',
    arguments: { action: 'create_folder', path: TEST_FOLDER },
    expected: 'success|already exists' },

  { scenario: 'Setup: BB asset', toolName: 'manage_ai',
    arguments: { action: 'create_blackboard_asset', name: bbName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'bbPath', fromField: 'result.blackboardPath' } },

  { scenario: 'Setup: BB key Spotted (Bool)', toolName: 'manage_ai',
    arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:bbPath}',
                 keyName: 'Spotted', keyType: 'Bool' },
    expected: 'success' },

  { scenario: 'Setup: BB key TargetActor (Object, baseObjectClass Actor)', toolName: 'manage_ai',
    arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:bbPath}',
                 keyName: 'TargetActor', keyType: 'Object', baseObjectClass: 'Actor' },
    expected: 'success' },

  // BT via action='create' (graph-init Root node so RootNode is traversable; NOT create_behavior_tree)
  { scenario: 'Setup: BT asset (via BT.create for BTGraph init)', toolName: 'manage_ai',
    arguments: { action: 'create', name: btName, savePath: TEST_FOLDER_ALIAS },
    expected: 'success',
    captureResult: { key: 'btPath', fromField: 'result.assetPath' } },

  { scenario: 'Setup: bind BT to BB', toolName: 'manage_ai',
    arguments: { action: 'assign_blackboard', behaviorTreePath: '${captured:btPath}',
                 blackboardPath: '${captured:bbPath}' },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.saved', equals: true, label: 'BT blackboard binding saved' }
    ] },

  // === Build: Root -> Selector -> Sequence -> Wait ===
  { scenario: 'Build: add Selector', toolName: 'manage_ai',
    arguments: { action: 'add_node', assetPath: '${captured:btPath}', nodeType: 'Selector' },
    expected: 'success',
    captureResult: { key: 'rootSelectorId', fromField: 'result.nodeId' } },

  { scenario: 'Build: connect Root->Selector', toolName: 'manage_ai',
    arguments: { action: 'connect_nodes', assetPath: '${captured:btPath}',
                 parentNodeId: 'BehaviorTreeGraphNode_Root_0', childNodeId: '${captured:rootSelectorId}' },
    expected: 'success' },

  { scenario: 'Build: add Sequence', toolName: 'manage_ai',
    arguments: { action: 'add_node', assetPath: '${captured:btPath}', nodeType: 'Sequence' },
    expected: 'success',
    captureResult: { key: 'sequenceId', fromField: 'result.nodeId' } },

  { scenario: 'Build: connect Selector->Sequence', toolName: 'manage_ai',
    arguments: { action: 'connect_nodes', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:rootSelectorId}', childNodeId: '${captured:sequenceId}' },
    expected: 'success' },

  { scenario: 'Build: add Wait', toolName: 'manage_ai',
    arguments: { action: 'add_node', assetPath: '${captured:btPath}', nodeType: 'Wait' },
    expected: 'success',
    captureResult: { key: 'waitId', fromField: 'result.nodeId' } },

  { scenario: 'Build: connect Sequence->Wait', toolName: 'manage_ai',
    arguments: { action: 'connect_nodes', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:sequenceId}', childNodeId: '${captured:waitId}' },
    expected: 'success' },

  // === Subnodes ===
  { scenario: 'Service DefaultFocus on Selector', toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:rootSelectorId}', subnodeType: 'Service', nodeClass: 'DefaultFocus' },
    expected: 'success',
    captureResult: { key: 'serviceId', fromField: 'result.nodeId' } },

  { scenario: 'Service: set BlackboardKey=TargetActor', toolName: 'manage_ai',
    arguments: { action: 'set_node_properties', assetPath: '${captured:btPath}',
                 nodeId: '${captured:serviceId}', properties: { BlackboardKey: 'TargetActor' } },
    expected: 'success' },

  { scenario: 'Decorator Blackboard on Sequence', toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:sequenceId}', subnodeType: 'Decorator', nodeClass: 'Blackboard' },
    expected: 'success',
    captureResult: { key: 'bbDecId', fromField: 'result.nodeId' } },

  { scenario: 'Decorator Blackboard: set BlackboardKey=Spotted', toolName: 'manage_ai',
    arguments: { action: 'set_node_properties', assetPath: '${captured:btPath}',
                 nodeId: '${captured:bbDecId}', properties: { BlackboardKey: 'Spotted' } },
    expected: 'success' },

  // R16: a 2-selector node (BlackboardKeyA + BlackboardKeyB) on the SAME edge
  { scenario: 'Decorator CompareBBEntries on Sequence (R16 multi-selector)', toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:sequenceId}', subnodeType: 'Decorator', nodeClass: 'CompareBBEntries' },
    expected: 'success',
    captureResult: { key: 'cmpId', fromField: 'result.nodeId' } },

  { scenario: 'CompareBBEntries: set BlackboardKeyA=Spotted', toolName: 'manage_ai',
    arguments: { action: 'set_node_properties', assetPath: '${captured:btPath}',
                 nodeId: '${captured:cmpId}', properties: { BlackboardKeyA: 'Spotted' } },
    expected: 'success' },

  { scenario: 'CompareBBEntries: set BlackboardKeyB=Spotted', toolName: 'manage_ai',
    arguments: { action: 'set_node_properties', assetPath: '${captured:btPath}',
                 nodeId: '${captured:cmpId}', properties: { BlackboardKeyB: 'Spotted' } },
    expected: 'success' },

  { scenario: 'Decorator Cooldown on Sequence (3rd edge decorator)', toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:sequenceId}', subnodeType: 'Decorator', nodeClass: 'Cooldown' },
    expected: 'success' },

  { scenario: 'Root-sentinel Decorator Cooldown (-> UBehaviorTree::RootDecorators)', toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: 'root', subnodeType: 'Decorator', nodeClass: 'Cooldown' },
    expected: 'success' },

  // === get_tree assertions (paths under structuredContent.result.tree.*) ===
  { scenario: 'get_tree returns navigable hierarchy', toolName: 'manage_ai',
    arguments: { action: 'get_tree', assetPath: '${captured:btPath}' },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.tree.hasRootNode', equals: true,
        label: 'hasRootNode true' },
      { path: 'structuredContent.result.tree.rootNode.nodeClass', equals: 'BTComposite_Selector',
        label: 'runtime root is the Selector composite' },
      { path: 'structuredContent.result.tree.rootNode.nodeType', equals: 'composite',
        label: 'root nodeType composite' },
      { path: 'structuredContent.result.tree.rootNode.services',
        includesObject: { nodeClass: 'BTService_DefaultFocus', keyProperties: { BlackboardKey: 'TargetActor' } },
        label: 'root service resolves TargetActor' },
      { path: 'structuredContent.result.tree.rootNode.children', length: 1,
        label: 'Selector has exactly one child (Sequence)' },
      { path: 'structuredContent.result.tree.rootNode.children.0.nodeClass', equals: 'BTComposite_Sequence',
        label: 'child is the Sequence composite' },
      { path: 'structuredContent.result.tree.rootNode.children.0.entryDecorators',
        includesObject: { nodeClass: 'BTDecorator_Blackboard', keyProperties: { BlackboardKey: 'Spotted' } },
        label: 'Sequence entry edge has Blackboard(Spotted)' },
      { path: 'structuredContent.result.tree.rootNode.children.0.entryDecorators',
        includesObject: { nodeClass: 'BTDecorator_CompareBBEntries',
                          keyProperties: { BlackboardKeyA: 'Spotted', BlackboardKeyB: 'Spotted' } },
        label: 'R16: CompareBBEntries exposes BOTH selectors' },
      // NOTE: entryDecoratorOpsRaw is intentionally NOT asserted here. The Task 8 smoke probe
      // confirmed UE leaves the postfix DecoratorOps array EMPTY for simple AND-stacked decorators
      // (it is populated only for explicit decorator-logic composition: And/Or/Not grouping).
      // The field still serializes correctly (as []); proving non-empty ops would require an
      // editor-configured logic group, which no MCP authoring action exposes. (cross-model review F4)
      { path: 'structuredContent.result.tree.rootNode.children.0.children.0.nodeClass', equals: 'BTTask_Wait',
        label: 'Sequence leaf is the Wait task' },
      { path: 'structuredContent.result.tree.rootNode.children.0.children.0.nodeType', equals: 'task',
        label: 'leaf nodeType task' },
      { path: 'structuredContent.result.tree.rootDecorators',
        includesObject: { nodeClass: 'BTDecorator_Cooldown' },
        label: 'root-sentinel decorator surfaced in rootDecorators' }
    ] },

  // === get_ai_info BB enrichment (PR1a adds inherited/sourceBlackboard via shared serializer) ===
  // We assert the deterministic `inherited:false` enrichment field, NOT `baseClass`: the
  // add_blackboard_key authoring action does not set UBlackboardKeyType_Object::BaseClass
  // (AIHandlers.cpp:957-960 reads baseObjectClass but never assigns it — "Could set base class
  // here"), so baseClass is non-deterministic (default Object / absent). The serializer's
  // baseClass code is correct; there is just no MCP path to populate BaseClass for this fixture.
  // Populating BaseClass (and thus a baseClass assertion) is a separate authoring gap, out of
  // PR1a scope. (cross-model review F2)
  { scenario: 'get_ai_info BB enrichment: per-key inherited flag present', toolName: 'manage_ai',
    arguments: { action: 'get_ai_info', blackboardPath: '${captured:bbPath}' },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.aiInfo.blackboardKeys',
        includesObject: { name: 'TargetActor', type: 'BlackboardKeyType_Object', inherited: false },
        label: 'enrichment present: TargetActor is own (non-inherited) key' }
    ] },

  // === Null-RootNode contract: a graphless BT (create_behavior_tree leaves BTGraph/RootNode null) ===
  // NOTE: create_behavior_tree returns its path under result.behaviorTreePath (AIHandlers.cpp:1067),
  // NOT result.assetPath (that is the BT.create SubAction's field, used for btPath above).
  { scenario: 'Setup: empty BT via create_behavior_tree (graphless)', toolName: 'manage_ai',
    arguments: { action: 'create_behavior_tree', name: btEmptyName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'btEmptyPath', fromField: 'result.behaviorTreePath' } },

  { scenario: 'get_tree on graphless BT returns null-RootNode contract (success, not error)',
    toolName: 'manage_ai',
    arguments: { action: 'get_tree', assetPath: '${captured:btEmptyPath}' },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.tree.hasRootNode', equals: false,
        label: 'graphless BT -> hasRootNode false (no GRAPH_NOT_FOUND error)' },
      { path: 'structuredContent.result.tree.executionNodeCount', equals: 0,
        label: 'graphless BT -> executionNodeCount 0' }
    ] },

  // === Cleanup (assets first, then folder is left for the runner / next-run uniqueness) ===
  { scenario: 'Cleanup: delete BT', toolName: 'manage_asset',
    arguments: { action: 'delete', assetPath: '${captured:btPath}', force: true },
    expected: 'success|not found' },

  { scenario: 'Cleanup: delete empty BT', toolName: 'manage_asset',
    arguments: { action: 'delete', assetPath: '${captured:btEmptyPath}', force: true },
    expected: 'success|not found' },

  { scenario: 'Cleanup: delete BB', toolName: 'manage_asset',
    arguments: { action: 'delete', assetPath: '${captured:bbPath}', force: true },
    expected: 'success|not found' },
];

await runToolTests('manage_behavior_tree.get_tree', testCases);
