#!/usr/bin/env node
/**
 * manage_behavior_tree subnode authoring integration test (PR0b).
 *
 * Exercises the four fixture patterns PR1a's live test will depend on:
 *   - Scenario A: root-level Service with FBlackboardKeySelector binding (TargetActor)
 *   - Scenario B: non-root edge decorator stacking with BB-key (Spotted) + Cooldown
 *   - Scenario C: root decorator via "root" sentinel
 *   - Negative: invalid GUID parentNodeId -> INVALID_PARENT
 *
 * Spec: docs/superpowers/specs/2026-05-03-pr0b-bt-authoring-subnode-design.md
 *
 * Test notes:
 *
 *   1. BT must be created via manage_ai action='create' (which routes through
 *      behaviorTreeActionSet to the BT.create SubAction), NOT
 *      action='create_behavior_tree'. The latter goes through AIHandlers and
 *      uses Asset Tools without initializing BTGraph; downstream BT SubActions
 *      then fail with GRAPH_NOT_FOUND.
 *
 *   2. The fixture binds the newly created BT to the newly created BB before
 *      setting BlackboardKey properties, so FBlackboardKeySelector resolution
 *      runs against known keys.
 *
 *   3. runToolTests matcher quirk: lowerReason for primary error-type match
 *      uses the response *message* (not error code) when message is non-empty.
 *      The INVALID_PARENT negative test accepts either the error code or
 *      the substring `not found` (which is in the engine's reply message)
 *      so the matcher path resolves cleanly regardless of which side carries
 *      the discriminating word.
 *
 *   4. btNodeCount is intentionally not asserted here. The diagnostics below
 *      focus on the BB binding, FBlackboardKeySelector resolution, and root
 *      decorator propagation contracts covered by this fixture.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/BTSubnode_${ts}`;
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const bbName = `BB_PR0bFixture_${ts}`;
const btName = `BT_PR0bFixture_${ts}`;

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

  { scenario: 'Setup: BB key TargetActor (Object)', toolName: 'manage_ai',
    arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:bbPath}',
                 keyName: 'TargetActor', keyType: 'Object' },
    expected: 'success' },

  // BT must use action='create' (BT.create SubAction routes via behaviorTreeActionSet,
  // initializes BTGraph + default Root node). See header note 1.
  { scenario: 'Setup: BT asset (via BT.create for BTGraph init)',
    toolName: 'manage_ai',
    arguments: { action: 'create', name: btName, savePath: TEST_FOLDER_ALIAS },
    expected: 'success',
    captureResult: { key: 'btPath', fromField: 'result.assetPath' } },

  { scenario: 'Setup: bind BT to BB', toolName: 'manage_ai',
    arguments: { action: 'assign_blackboard', behaviorTreePath: '${captured:btPath}',
                 blackboardPath: '${captured:bbPath}' },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.saved', equals: true,
        label: 'BT blackboard binding saved' }
    ] },

  // === Build BT structure: Selector -> Sequence -> Wait ===
  { scenario: 'Build: add Selector', toolName: 'manage_ai',
    arguments: { action: 'add_node', assetPath: '${captured:btPath}', nodeType: 'Selector' },
    expected: 'success',
    captureResult: { key: 'rootSelectorId', fromField: 'result.nodeId' } },

  { scenario: 'Build: connect Root->Selector', toolName: 'manage_ai',
    arguments: { action: 'connect_nodes', assetPath: '${captured:btPath}',
                 parentNodeId: 'BehaviorTreeGraphNode_Root_0',
                 childNodeId: '${captured:rootSelectorId}' },
    expected: 'success' },

  { scenario: 'Build: add Sequence', toolName: 'manage_ai',
    arguments: { action: 'add_node', assetPath: '${captured:btPath}', nodeType: 'Sequence' },
    expected: 'success',
    captureResult: { key: 'sequenceId', fromField: 'result.nodeId' } },

  { scenario: 'Build: connect Selector->Sequence', toolName: 'manage_ai',
    arguments: { action: 'connect_nodes', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:rootSelectorId}',
                 childNodeId: '${captured:sequenceId}' },
    expected: 'success' },

  { scenario: 'Build: add Wait', toolName: 'manage_ai',
    arguments: { action: 'add_node', assetPath: '${captured:btPath}', nodeType: 'Wait' },
    expected: 'success',
    captureResult: { key: 'waitId', fromField: 'result.nodeId' } },

  { scenario: 'Build: connect Sequence->Wait', toolName: 'manage_ai',
    arguments: { action: 'connect_nodes', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:sequenceId}',
                 childNodeId: '${captured:waitId}' },
    expected: 'success' },

  // === Scenario A: root-level Service with FBlackboardKeySelector binding ===
  { scenario: 'A: add_subnode Service DefaultFocus on Selector',
    toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:rootSelectorId}',
                 subnodeType: 'Service', nodeClass: 'DefaultFocus' },
    expected: 'success',
    captureResult: { key: 'serviceId', fromField: 'result.nodeId' },
    assertions: [
      { path: 'structuredContent.result.nodeClass', equals: 'BTService_DefaultFocus',
        label: 'service nodeClass echoed back' }
    ] },

  { scenario: 'A: set BlackboardKey=TargetActor on service (FBlackboardKeySelector path)',
    toolName: 'manage_ai',
    arguments: { action: 'set_node_properties', assetPath: '${captured:btPath}',
                 nodeId: '${captured:serviceId}',
                 properties: { BlackboardKey: 'TargetActor' } },
    expected: 'success' },

  // === Scenario B: stacked decorators on non-root edge ===
  { scenario: 'B: add_subnode Decorator Blackboard on Sequence',
    toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:sequenceId}',
                 subnodeType: 'Decorator', nodeClass: 'Blackboard' },
    expected: 'success',
    captureResult: { key: 'bbDecId', fromField: 'result.nodeId' } },

  { scenario: 'B: set BlackboardKey=Spotted on Blackboard decorator',
    toolName: 'manage_ai',
    arguments: { action: 'set_node_properties', assetPath: '${captured:btPath}',
                 nodeId: '${captured:bbDecId}',
                 properties: { BlackboardKey: 'Spotted' } },
    expected: 'success' },

  { scenario: 'B: add_subnode Decorator Cooldown on Sequence (stacking proof)',
    toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:sequenceId}',
                 subnodeType: 'Decorator', nodeClass: 'Cooldown' },
    expected: 'success' },

  // === Scenario C: root decorator via "root" sentinel ===
  { scenario: 'C: add_subnode Decorator Cooldown via "root" sentinel',
    toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: 'root',
                 subnodeType: 'Decorator', nodeClass: 'Cooldown' },
    expected: 'success' },

  { scenario: 'C: verify runtime BT diagnostics after root sentinel subnode',
    toolName: 'manage_ai',
    arguments: { action: 'get_ai_info', behaviorTreePath: '${captured:btPath}' },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.aiInfo.assignedBlackboard', equals: bbName,
        label: 'BT reports the bound BlackboardAsset' },
      { path: 'structuredContent.result.aiInfo.rootGraphBlackboard', equals: bbName,
        label: 'BT root graph node reports the bound BlackboardAsset' },
      { path: 'structuredContent.result.aiInfo.rootGraphBlackboardMatchesAssigned', equals: true,
        label: 'BT root graph BlackboardAsset matches runtime BT BlackboardAsset' },
      { path: 'structuredContent.result.aiInfo.rootDecoratorCount', equals: 1,
        label: 'root decorator count propagated to runtime BT' },
      { path: 'structuredContent.result.aiInfo.rootDecorators', includesObject: { className: 'BTDecorator_Cooldown' },
        label: 'runtime root decorators include Cooldown' },
      { path: 'structuredContent.result.aiInfo.services', includesObject: { className: 'BTService_DefaultFocus', selectedBlackboardKey: 'TargetActor' },
        label: 'runtime service resolved TargetActor key' },
      { path: 'structuredContent.result.aiInfo.childDecorators', includesObject: { className: 'BTDecorator_Blackboard', selectedBlackboardKey: 'Spotted' },
        label: 'runtime child decorator resolved Spotted key' }
    ] },

  // === Negative: nonexistent GUID -> INVALID_PARENT ===
  // The error code is INVALID_PARENT; the engine's message reads "Parent node not
  // found: <guid>". runToolTests matches primary error-type against the message
  // when present, so we list both alternatives — either side satisfies the matcher.
  { scenario: 'Negative: nonexistent parentNodeId rejects with INVALID_PARENT',
    toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '00000000-0000-0000-0000-000000000000',
                 subnodeType: 'Decorator', nodeClass: 'Cooldown' },
    expected: 'INVALID_PARENT|not found' },

  // Symmetry with the other 4 BT SubActions: add_subnode also walks
  // UAIGraphNode::SubNodes via FindGraphNodeByIdOrName, so passing a subnode's
  // own GUID as parentNodeId now lands on the parent-class validation path and
  // rejects with INVALID_PARENT_FOR_SUBNODE instead of a misleading "not found"
  // (caught in cross-model review, F1).
  { scenario: 'Negative: subnode GUID as parentNodeId rejects with INVALID_PARENT_FOR_SUBNODE',
    toolName: 'manage_ai',
    arguments: { action: 'add_subnode', assetPath: '${captured:btPath}',
                 parentNodeId: '${captured:bbDecId}',
                 subnodeType: 'Decorator', nodeClass: 'Cooldown' },
    expected: 'INVALID_PARENT_FOR_SUBNODE|cannot host' },

  // === Cleanup (two-step: assets first, then folder — UE 5.7 folder-delete
  // modal workaround per reference_mcp_integration_test_patterns memory) ===
  { scenario: 'Cleanup: delete BT', toolName: 'manage_asset',
    arguments: { action: 'delete', assetPath: '${captured:btPath}', force: true },
    expected: 'success|not found' },

  { scenario: 'Cleanup: delete BB', toolName: 'manage_asset',
    arguments: { action: 'delete', assetPath: '${captured:bbPath}', force: true },
    expected: 'success|not found' },
];

await runToolTests('manage_behavior_tree', testCases);
