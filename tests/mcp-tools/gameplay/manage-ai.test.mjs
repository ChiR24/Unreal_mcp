#!/usr/bin/env node
/**
 * manage_ai Tool Integration Tests
 * Covers all 43 actions with real captured asset paths.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayAssets/ManageAI_${ts}`;
const testActorName = `TestAIActor_${ts}`;
const controllerName = `TestAIController_${ts}`;
const blackboardName = `TestBlackboard_${ts}`;
const aliasBlackboardName = `TestAliasBlackboard_${ts}`;
const behaviorTreeName = `TestBehaviorTree_${ts}`;
const eqsQueryName = `TestEQSQuery_${ts}`;
const stateTreeName = `TestStateTree_${ts}`;
const smartObjectName = `TestSmartObject_${ts}`;
const massConfigName = `TestMassConfig_${ts}`;
const blueprintName = `BP_TestAI_${ts}`;
const navLinkName = `BP_TestNavLink_${ts}`;
const blackboardKeyName = `TargetActor_${ts}`;
const childStateName = `Patrol_${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success' },
  {
    scenario: 'Setup: create test blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', name: blueprintName, path: TEST_FOLDER, parentClass: 'Actor' },
    expected: 'success',
    captureResult: { key: 'blueprintPath', fromField: 'result.assetPath' }
  },
  { scenario: 'Setup: spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: testActorName, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },

  // === CREATE / CONTROLLER ===
  {
    scenario: 'CREATE: create_ai_controller',
    toolName: 'manage_ai',
    arguments: { action: 'create_ai_controller', name: controllerName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'controllerPath', fromField: 'result.assetPath' }
  },
  {
    scenario: 'CREATE: create_blackboard_asset',
    toolName: 'manage_ai',
    arguments: { action: 'create_blackboard_asset', name: blackboardName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'blackboardPath', fromField: 'result.assetPath' }
  },
  {
    scenario: 'CREATE: create_behavior_tree',
    toolName: 'manage_ai',
    arguments: { action: 'create_behavior_tree', name: behaviorTreeName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'behaviorTreePath', fromField: 'result.assetPath' }
  },
  { scenario: 'CONNECT: assign_behavior_tree', toolName: 'manage_ai', arguments: { action: 'assign_behavior_tree', controllerPath: '${captured:controllerPath}', behaviorTreePath: '${captured:behaviorTreePath}' }, expected: 'success' },
  { scenario: 'CONNECT: assign_blackboard', toolName: 'manage_ai', arguments: { action: 'assign_blackboard', controllerPath: '${captured:controllerPath}', blackboardPath: '${captured:blackboardPath}' }, expected: 'success' },

  // === BLACKBOARD ===
  { scenario: 'ADD: add_blackboard_key', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName, keyType: 'Bool', isInstanceSynced: false }, expected: 'success' },
  { scenario: 'CONFIG: set_key_instance_synced', toolName: 'manage_ai', arguments: { action: 'set_key_instance_synced', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName, isInstanceSynced: true }, expected: 'success' },
  { scenario: 'CONFIG: set_blackboard_value', toolName: 'manage_ai', arguments: { action: 'set_blackboard_value', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName, value: 'true' }, expected: 'success' },
  { scenario: 'INFO: get_blackboard_value', toolName: 'manage_ai', arguments: { action: 'get_blackboard_value', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName }, expected: 'success' },

  // === BEHAVIOR TREE ===
  { scenario: 'ADD: add_composite_node', toolName: 'manage_ai', arguments: { action: 'add_composite_node', behaviorTreePath: '${captured:behaviorTreePath}', compositeType: 'Sequence' }, expected: 'success' },
  { scenario: 'ADD: add_task_node', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: '${captured:behaviorTreePath}', taskType: 'Wait' }, expected: 'success' },
  { scenario: 'ADD: add_decorator', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: '${captured:behaviorTreePath}', decoratorType: 'Blackboard' }, expected: 'success' },
  { scenario: 'ADD: add_service', toolName: 'manage_ai', arguments: { action: 'add_service', behaviorTreePath: '${captured:behaviorTreePath}', serviceType: 'DefaultFocus' }, expected: 'success' },
  { scenario: 'CONFIG: configure_bt_node', toolName: 'manage_ai', arguments: { action: 'configure_bt_node', behaviorTreePath: '${captured:behaviorTreePath}', nodeId: 'Root' }, expected: 'success' },
  { scenario: 'ACTION: run_behavior_tree', toolName: 'manage_ai', arguments: { action: 'run_behavior_tree', controllerPath: '${captured:controllerPath}', behaviorTreePath: '${captured:behaviorTreePath}' }, expected: 'success' },
  { scenario: 'PLAYBACK: stop_behavior_tree', toolName: 'manage_ai', arguments: { action: 'stop_behavior_tree', controllerPath: '${captured:controllerPath}' }, expected: 'success' },

  // === EQS ===
  {
    scenario: 'CREATE: create_eqs_query',
    toolName: 'manage_ai',
    arguments: { action: 'create_eqs_query', name: eqsQueryName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'queryPath', fromField: 'result.assetPath' }
  },
  { scenario: 'ADD: add_eqs_generator', toolName: 'manage_ai', arguments: { action: 'add_eqs_generator', queryPath: '${captured:queryPath}', generatorType: 'ActorsOfClass', generatorSettings: { searchRadius: 600, actorClass: '/Script/Engine.Actor' } }, expected: 'success' },
  { scenario: 'ADD: add_eqs_context', toolName: 'manage_ai', arguments: { action: 'add_eqs_context', queryPath: '${captured:queryPath}', contextType: 'Querier' }, expected: 'success' },
  { scenario: 'ADD: add_eqs_test', toolName: 'manage_ai', arguments: { action: 'add_eqs_test', queryPath: '${captured:queryPath}', testType: 'Distance' }, expected: 'success' },
  { scenario: 'CONFIG: configure_test_scoring', toolName: 'manage_ai', arguments: { action: 'configure_test_scoring', queryPath: '${captured:queryPath}', testIndex: 0, testSettings: { scoringEquation: 'Square', clampMin: 0, clampMax: 1, filterType: 'Range', floatMin: 0, floatMax: 5000 } }, expected: 'success' },

  // === PERCEPTION ===
  { scenario: 'ADD: add_ai_perception_component', toolName: 'manage_ai', arguments: { action: 'add_ai_perception_component', blueprintPath: '${captured:blueprintPath}' }, expected: 'success' },
  { scenario: 'CONFIG: configure_sight_config', toolName: 'manage_ai', arguments: { action: 'configure_sight_config', blueprintPath: '${captured:blueprintPath}', sightConfig: { sightRadius: 3000, loseSightRadius: 3500, peripheralVisionAngle: 90 } }, expected: 'success' },
  { scenario: 'CONFIG: configure_hearing_config', toolName: 'manage_ai', arguments: { action: 'configure_hearing_config', blueprintPath: '${captured:blueprintPath}', hearingConfig: { hearingRange: 2000 } }, expected: 'success' },
  { scenario: 'CONFIG: configure_damage_sense_config', toolName: 'manage_ai', arguments: { action: 'configure_damage_sense_config', blueprintPath: '${captured:blueprintPath}', damageConfig: { maxAge: 10 } }, expected: 'success' },
  { scenario: 'CONFIG: set_perception_team', toolName: 'manage_ai', arguments: { action: 'set_perception_team', blueprintPath: '${captured:blueprintPath}', teamId: 1 }, expected: 'success' },
  { scenario: 'ACTION: setup_perception', toolName: 'manage_ai', arguments: { action: 'setup_perception', blueprintPath: '${captured:blueprintPath}', enableSight: true, sightRadius: 2800, loseSightRadius: 3300, peripheralVisionAngle: 85, enableHearing: true, hearingRange: 1900, enableDamage: true, dominantSense: 'Sight' }, expected: 'success' },

  // === STATE TREE ===
  {
    scenario: 'CREATE: create_state_tree',
    toolName: 'manage_ai',
    arguments: { action: 'create_state_tree', name: stateTreeName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'stateTreePath', fromField: 'result.assetPath' }
  },
  { scenario: 'ADD: add_state_tree_state', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: '${captured:stateTreePath}', stateName: childStateName, parentStateName: 'Root', stateType: 'State' }, expected: 'success' },
  { scenario: 'ADD: add_state_tree_transition', toolName: 'manage_ai', arguments: { action: 'add_state_tree_transition', stateTreePath: '${captured:stateTreePath}', fromState: 'Root', toState: childStateName, triggerType: 'OnStateCompleted' }, expected: 'success' },
  { scenario: 'CONFIG: configure_state_tree_task', toolName: 'manage_ai', arguments: { action: 'configure_state_tree_task', stateTreePath: '${captured:stateTreePath}', stateName: childStateName }, expected: 'success' },

  // === SMART OBJECTS ===
  {
    scenario: 'CREATE: create_smart_object_definition',
    toolName: 'manage_ai',
    arguments: { action: 'create_smart_object_definition', name: smartObjectName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'definitionPath', fromField: 'result.definitionPath' }
  },
  { scenario: 'ADD: add_smart_object_slot', toolName: 'manage_ai', arguments: { action: 'add_smart_object_slot', definitionPath: '${captured:definitionPath}', offset: { x: 100, y: 0, z: 0 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, enabled: true }, expected: 'success' },
  { scenario: 'CONFIG: configure_slot_behavior', toolName: 'manage_ai', arguments: { action: 'configure_slot_behavior', definitionPath: '${captured:definitionPath}', slotIndex: 0, enabled: true }, expected: 'success' },
  { scenario: 'ADD: add_smart_object_component', toolName: 'manage_ai', arguments: { action: 'add_smart_object_component', blueprintPath: '${captured:blueprintPath}' }, expected: 'success' },

  // === MASS AI ===
  {
    scenario: 'CREATE: create_mass_entity_config',
    toolName: 'manage_ai',
    arguments: { action: 'create_mass_entity_config', name: massConfigName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'configPath', fromField: 'result.configPath' }
  },
  { scenario: 'CONFIG: configure_mass_entity', toolName: 'manage_ai', arguments: { action: 'configure_mass_entity', configPath: '${captured:configPath}' }, expected: 'success' },
  { scenario: 'ADD: add_mass_spawner', toolName: 'manage_ai', arguments: { action: 'add_mass_spawner', blueprintPath: '${captured:blueprintPath}', configPath: '${captured:configPath}', componentName: 'MassSpawner', spawnCount: 10 }, expected: 'success' },

  // === UTILITY / ALIASES ===
  { scenario: 'INFO: get_ai_info', toolName: 'manage_ai', arguments: { action: 'get_ai_info', controllerPath: '${captured:controllerPath}' }, expected: 'success' },
  {
    scenario: 'CREATE: create_blackboard',
    toolName: 'manage_ai',
    arguments: { action: 'create_blackboard', name: aliasBlackboardName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'aliasBlackboardPath', fromField: 'result.blackboardPath' }
  },
  {
    scenario: 'CREATE: create_nav_link_proxy',
    toolName: 'manage_ai',
    arguments: {
      action: 'create_nav_link_proxy',
      actorName: navLinkName,
      location: { x: 250, y: 0, z: 100 },
      startPoint: { x: -100, y: 0, z: 0 },
      endPoint: { x: 100, y: 0, z: 0 },
      direction: 'BothWays'
    },
    expected: 'success',
    captureResult: { key: 'navLinkPath', fromField: 'result.actorPath' }
  },
  { scenario: 'CONFIG: set_focus', toolName: 'manage_ai', arguments: { action: 'set_focus', controllerPath: '${captured:controllerPath}', focusActorName: testActorName }, expected: 'success' },
  { scenario: 'DELETE: clear_focus', toolName: 'manage_ai', arguments: { action: 'clear_focus', controllerPath: '${captured:controllerPath}' }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: testActorName }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' }
];

// === BEHAVIOR TREE GRAPH ACTIONS ===
{
  /**
   * manage_ai behavior tree action integration tests
   * Covers all 6 actions with proper setup/teardown sequencing.
   */

  const TEST_FOLDER = '/Game/MCPTest/GameplayAssets';
  const ts = Date.now();
  const behaviorTreeName = `BT_Test_${ts}`;

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },

    // === ACTION ===
    {
      scenario: 'ACTION: create',
      toolName: 'manage_ai',
      arguments: { action: 'create', name: behaviorTreeName, savePath: TEST_FOLDER },
      expected: 'success',
      captureResult: { key: 'behaviorTreePath', fromField: 'assetPath' }
    },
    // === ADD ===
    {
      scenario: 'ADD: add sequence node',
      toolName: 'manage_ai',
      arguments: { action: 'add_node', assetPath: '${captured:behaviorTreePath}', nodeType: 'Sequence', x: 0, y: 0 },
      expected: 'success',
      captureResult: { key: 'sequenceNodeId', fromField: 'nodeId' }
    },
    {
      scenario: 'ADD: add wait task node',
      toolName: 'manage_ai',
      arguments: { action: 'add_node', assetPath: '${captured:behaviorTreePath}', nodeType: 'Wait', x: 300, y: 120 },
      expected: 'success',
      captureResult: { key: 'waitNodeId', fromField: 'nodeId' }
    },
    // === CONNECT ===
    {
      scenario: 'CONNECT: connect_nodes',
      toolName: 'manage_ai',
      arguments: {
        action: 'connect_nodes',
        assetPath: '${captured:behaviorTreePath}',
        parentNodeId: '${captured:sequenceNodeId}',
        childNodeId: '${captured:waitNodeId}'
      },
      expected: 'success'
    },
    // === CONFIG ===
    {
      scenario: 'CONFIG: set_node_properties',
      toolName: 'manage_ai',
      arguments: {
        action: 'set_node_properties',
        assetPath: '${captured:behaviorTreePath}',
        nodeId: '${captured:waitNodeId}',
        comment: 'MCP behavior tree live test node',
        properties: { WaitTime: 1.0 }
      },
      expected: 'success'
    },
    // === ACTION ===
    {
      scenario: 'ACTION: break_connections',
      toolName: 'manage_ai',
      arguments: { action: 'break_connections', assetPath: '${captured:behaviorTreePath}', nodeId: '${captured:sequenceNodeId}' },
      expected: 'success'
    },
    // === DELETE ===
    {
      scenario: 'DELETE: remove_node',
      toolName: 'manage_ai',
      arguments: { action: 'remove_node', assetPath: '${captured:behaviorTreePath}', nodeId: '${captured:waitNodeId}' },
      expected: 'success'
    },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete behavior tree', toolName: 'manage_asset', arguments: { action: 'delete', path: `${TEST_FOLDER}/${behaviorTreeName}`, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

// === NAVIGATION ACTIONS ===
{
  /**
   * manage_ai navigation action integration tests
   * Covers all 12 actions with proper setup/teardown sequencing.
   */

  const ts = Date.now();
  const TEST_FOLDER = `/Game/MCPTest/WorldAssets_${ts}`;
  const NAV_BLUEPRINT = `BP_NavTest_${ts}`;
  const NAV_ACTOR = `NavTestActor_${ts}`;
  const NAV_LINK = `TestNavLinkProxy_${ts}`;
  const SMART_LINK = `TestSmartLink_${ts}`;
  const NAV_MODIFIER = `NavModifier_${ts}`;
  const OBSTACLE_AREA = '/Script/NavigationSystem.NavArea_Obstacle';

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: create navigation blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: NAV_BLUEPRINT, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success', captureResult: { key: 'navBlueprintPath', fromField: 'result.assetPath' } },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_nav_mesh_settings', toolName: 'manage_ai', arguments: {"action": "configure_nav_mesh_settings", "cellSize": 19, "cellHeight": 10, "tileSizeUU": 1000, "minRegionArea": 0, "mergeRegionSize": 400, "maxSimplificationError": 1.3}, expected: 'success' },
    { scenario: 'CONFIG: set_nav_agent_properties', toolName: 'manage_ai', arguments: {"action": "set_nav_agent_properties", "agentRadius": 35, "agentHeight": 144, "agentStepHeight": 35, "agentMaxSlope": 44}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: rebuild_navigation', toolName: 'manage_ai', arguments: {"action": "rebuild_navigation"}, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_nav_modifier_component', toolName: 'manage_ai', arguments: {"action": "create_nav_modifier_component", "blueprintPath": "${captured:navBlueprintPath}", "componentName": NAV_MODIFIER, "areaClass": OBSTACLE_AREA, "failsafeExtent": {"x": 120, "y": 80, "z": 60}, "save": true}, expected: 'success' },
    { scenario: 'Setup: spawn nav test actor', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: '${captured:navBlueprintPath}', actorName: NAV_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_nav_area_class', toolName: 'manage_ai', arguments: {"action": "set_nav_area_class", "actorName": NAV_ACTOR, "areaClass": OBSTACLE_AREA}, expected: 'success' },
    { scenario: 'CONFIG: configure_nav_area_cost', toolName: 'manage_ai', arguments: {"action": "configure_nav_area_cost", "areaClass": OBSTACLE_AREA, "areaCost": 1.0}, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_nav_link_proxy', toolName: 'manage_ai', arguments: {"action": "create_nav_link_proxy", "actorName": NAV_LINK, "location": {"x": 0, "y": 0, "z": 100}, "startPoint": {"x": -100, "y": 0, "z": 0}, "endPoint": {"x": 100, "y": 0, "z": 0}, "direction": "BothWays"}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_nav_link', toolName: 'manage_ai', arguments: {"action": "configure_nav_link", "actorName": NAV_LINK, "snapRadius": 30}, expected: 'success' },
    { scenario: 'CONFIG: set_nav_link_type', toolName: 'manage_ai', arguments: {"action": "set_nav_link_type", "actorName": NAV_LINK, "linkType": "smart"}, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_smart_link', toolName: 'manage_ai', arguments: {"action": "create_smart_link", "actorName": SMART_LINK, "location": {"x": 0, "y": 200, "z": 100}, "startPoint": {"x": -100, "y": 0, "z": 0}, "endPoint": {"x": 100, "y": 0, "z": 0}, "direction": "BothWays"}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_smart_link_behavior', toolName: 'manage_ai', arguments: {"action": "configure_smart_link_behavior", "actorName": SMART_LINK, "linkEnabled": true, "enabledAreaClass": "/Script/NavigationSystem.NavArea_Default", "disabledAreaClass": "/Script/NavigationSystem.NavArea_Null", "broadcastRadius": 900, "broadcastInterval": 0, "bCreateBoxObstacle": true, "obstacleAreaClass": "/Script/NavigationSystem.NavArea_Null", "obstacleExtent": {"x": 75, "y": 50, "z": 40}, "obstacleOffset": {"x": 0, "y": 0, "z": 20}}, expected: 'success' },
    // === INFO ===
    { scenario: 'INFO: get_navigation_info', toolName: 'manage_ai', arguments: {"action": "get_navigation_info"}, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete nav test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: NAV_ACTOR }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete nav link proxy', toolName: 'control_actor', arguments: { action: 'delete', actorName: NAV_LINK }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete smart link proxy', toolName: 'control_actor', arguments: { action: 'delete', actorName: SMART_LINK }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('manage-ai', testCases);
