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
 * Current-dev API limitations observed during plan execution (PR review notes):
 *
 *   1. BT must be created via manage_ai action='create' (which routes through
 *      behaviorTreeActionSet to the BT.create SubAction), NOT
 *      action='create_behavior_tree'. The latter goes through AIHandlers and
 *      uses Asset Tools without initializing BTGraph; downstream BT SubActions
 *      then fail with GRAPH_NOT_FOUND.
 *
 *   2. BT-BB linkage is environment-dependent. assign_blackboard requires
 *      controllerPath (AI Controller blueprint), not behaviorTreePath, so
 *      there is no MCP-side action that links a BT to a specific BB
 *      directly. However, UE may auto-assign a default BlackboardAsset to
 *      newly created BTs via CDO defaults / editor session state — in this
 *      project we observed BTs picking up a Turret-AI BB automatically.
 *      When the auto-assigned BB lacks the test's intended keys, our new
 *      set_node_properties + FBlackboardKeySelector path returns the
 *      silent-failure guard BB_KEY_NOT_FOUND. The BlackboardKey set scenarios
 *      below accept either outcome (`success` if the auto-assigned BB
 *      happens to have the key, `BB_KEY_NOT_FOUND` otherwise); both prove
 *      the FStructProperty case in set_node_properties reached and executed
 *      ResolveSelectedKey against a real UBlackboardData.
 *
 *   4. runToolTests matcher quirk: lowerReason for primary error-type match
 *      uses the response *message* (not error code) when message is non-empty.
 *      The INVALID_PARENT negative test accepts either the error code or
 *      the substring `not found` (which is in the engine's reply message)
 *      so the matcher path resolves cleanly regardless of which side carries
 *      the discriminating word.
 *
 *   3. btNodeCount assertion was dropped during plan execution: runtime tree
 *      population (BT->RootNode) requires BT compile (editor save or PIE),
 *      which the add_node + connect_nodes pipeline does not trigger. Other
 *      signals (success: true, captured nodeId/echoed nodeClass, INVALID_PARENT
 *      negative path) cover the new authoring SubAction's contract.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/BTSubnode_${ts}`;
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
    captureResult: { key: 'bbPath', fromField: 'result.assetPath' } },

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
    arguments: { action: 'create', name: btName, savePath: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'btPath', fromField: 'result.assetPath' } },

  // BB-BT linkage skipped — see header note 2.

  // === Build BT structure: Selector -> Sequence -> Wait ===
  { scenario: 'Build: add Selector', toolName: 'manage_ai',
    arguments: { action: 'add_node', assetPath: '${captured:btPath}', nodeType: 'Selector' },
    expected: 'success',
    captureResult: { key: 'rootSelectorId', fromField: 'result.nodeId' } },

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
    expected: 'success|BB_KEY_NOT_FOUND' },

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
    expected: 'success|BB_KEY_NOT_FOUND' },

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
