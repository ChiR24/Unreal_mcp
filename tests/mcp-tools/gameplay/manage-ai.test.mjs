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
const massSpawnerName = `TestMassSpawner_${ts}`;
const blackboardKeyName = `TargetActor_${ts}`;
const moveGoalKeyName = `MoveGoal_${ts}`;
const childStateName = `Patrol_${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success' },
  {
    scenario: 'Setup: create test blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', name: blueprintName, savePath: TEST_FOLDER, parentClass: 'Actor' },
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
  // The controller actions write engine nodes into the Blueprint; they used to add an empty variable and say done.
  { scenario: 'CONNECT: assign_behavior_tree', toolName: 'manage_ai', arguments: { action: 'assign_behavior_tree', controllerPath: '${captured:controllerPath}', behaviorTreePath: '${captured:behaviorTreePath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the variable compiles with its default' }, { path: 'structuredContent.result.variable', equals: 'DefaultBehaviorTree', label: 'a new variable, as the controller had none' }] },
  { scenario: 'CONNECT: assign_blackboard', toolName: 'manage_ai', arguments: { action: 'assign_blackboard', controllerPath: '${captured:controllerPath}', blackboardPath: '${captured:blackboardPath}' }, expected: 'success' },

  // === BLACKBOARD ===
  { scenario: 'ADD: add_blackboard_key', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName, keyType: 'Bool', isInstanceSynced: false }, expected: 'success' },
  { scenario: 'ADD: add_blackboard_key vector goal', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:blackboardPath}', keyName: moveGoalKeyName, keyType: 'Vector' }, expected: 'success' },
  { scenario: 'ERROR: add_blackboard_key refuses a name the Blackboard already has', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName, keyType: 'Int' }, expected: 'error|NAME_TAKEN' },
  { scenario: 'ADD: add_blackboard_key enum with its enum type', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:blackboardPath}', keyName: `Channel_${ts}`, keyType: 'Enum', enumType: '/Script/Engine.ECollisionChannel' }, expected: 'success' },
  { scenario: 'ADD: add_blackboard_key object based on Actor', toolName: 'manage_ai', arguments: { action: 'add_blackboard_key', blackboardPath: '${captured:blackboardPath}', keyName: `Chased_${ts}`, keyType: 'Object', baseObjectClass: '/Script/Engine.Actor' }, expected: 'success', assertions: [{ path: 'structuredContent.result.baseClass', equals: '/Script/Engine.Actor', label: 'the key holds actors, so a MoveTo target accepts it' }] },
  { scenario: 'CONFIG: set_key_instance_synced', toolName: 'manage_ai', arguments: { action: 'set_key_instance_synced', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName, isInstanceSynced: true }, expected: 'success' },
  { scenario: 'CONFIG: set_blackboard_value', toolName: 'manage_ai', arguments: { action: 'set_blackboard_value', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName, value: 'true' }, expected: 'success' },
  { scenario: 'ERROR: set_blackboard_value refuses a value the key cannot hold', toolName: 'manage_ai', arguments: { action: 'set_blackboard_value', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName, value: 'maybe' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CONFIG: set_blackboard_value sets a vector default', toolName: 'manage_ai', arguments: { action: 'set_blackboard_value', blackboardPath: '${captured:blackboardPath}', keyName: moveGoalKeyName, value: { x: 100, y: 200, z: 0 } }, expected: 'success' },
  { scenario: 'INFO: get_blackboard_value', toolName: 'manage_ai', arguments: { action: 'get_blackboard_value', blackboardPath: '${captured:blackboardPath}', keyName: blackboardKeyName }, expected: 'success' },

  // === BEHAVIOR TREE ===
  { scenario: 'ADD: add_composite_node', toolName: 'manage_ai', arguments: { action: 'add_composite_node', behaviorTreePath: '${captured:behaviorTreePath}', compositeType: 'Sequence' }, expected: 'success' },
  { scenario: 'ADD: add_composite_node named', toolName: 'manage_ai', arguments: { action: 'add_composite_node', behaviorTreePath: '${captured:behaviorTreePath}', compositeType: 'Selector', nodeName: 'RootSelector' }, expected: 'success' },
  { scenario: 'ADD: add_composite_node SimpleParallel', toolName: 'manage_ai', arguments: { action: 'add_composite_node', behaviorTreePath: '${captured:behaviorTreePath}', compositeType: 'SimpleParallel', parentNodeId: 'RootSelector' }, expected: 'success' },
  { scenario: 'ADD: add_task_node', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: '${captured:behaviorTreePath}', taskType: 'Wait' }, expected: 'success' },
  { scenario: 'ADD: add_decorator', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: '${captured:behaviorTreePath}', decoratorType: 'Blackboard' }, expected: 'success' },
  { scenario: 'ADD: add_decorator on a named node', toolName: 'manage_ai', arguments: { action: 'add_decorator', behaviorTreePath: '${captured:behaviorTreePath}', decoratorType: 'Cooldown', parentNodeId: 'RootSelector' }, expected: 'success', assertions: [{ path: 'structuredContent.result.attachedTo', equals: 'RootSelector' }] },
  { scenario: 'ADD: add_task_node MoveTo', toolName: 'manage_ai', arguments: { action: 'add_task_node', behaviorTreePath: '${captured:behaviorTreePath}', taskType: 'MoveTo' }, expected: 'success', captureResult: { key: 'moveToNodeId', fromField: 'result.nodeId' } },
  { scenario: 'CONFIG: configure_bt_node sets a key selector by key name', toolName: 'manage_ai', arguments: { action: 'configure_bt_node', behaviorTreePath: '${captured:behaviorTreePath}', nodeId: '${captured:moveToNodeId}', properties: { BlackboardKey: moveGoalKeyName } }, expected: 'success' },
  { scenario: 'ADD: add_service', toolName: 'manage_ai', arguments: { action: 'add_service', behaviorTreePath: '${captured:behaviorTreePath}', serviceType: 'DefaultFocus' }, expected: 'success' },
  { scenario: 'CONFIG: configure_bt_node', toolName: 'manage_ai', arguments: { action: 'configure_bt_node', behaviorTreePath: '${captured:behaviorTreePath}', nodeId: 'Root' }, expected: 'success' },
  { scenario: 'ACTION: run_behavior_tree', toolName: 'manage_ai', arguments: { action: 'run_behavior_tree', controllerPath: '${captured:controllerPath}', behaviorTreePath: '${captured:behaviorTreePath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'On Possess runs the tree and the Blueprint compiles' }, { path: 'structuredContent.result.variable', equals: 'DefaultBehaviorTree', label: 'it reads the variable assign_behavior_tree set' }] },
  { scenario: 'PLAYBACK: stop_behavior_tree', toolName: 'manage_ai', arguments: { action: 'stop_behavior_tree', controllerPath: '${captured:controllerPath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.removed', equals: true, label: 'the run logic came out' }] },
  { scenario: 'PLAYBACK: stop_behavior_tree again finds nothing to take out', toolName: 'manage_ai', arguments: { action: 'stop_behavior_tree', controllerPath: '${captured:controllerPath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.removed', equals: false, label: 'nothing was installed, and it says so' }] },
  { scenario: 'ERROR: run_behavior_tree refuses a Blueprint that is not an AI controller', toolName: 'manage_ai', arguments: { action: 'run_behavior_tree', controllerPath: '${captured:blueprintPath}', behaviorTreePath: '${captured:behaviorTreePath}' }, expected: 'error|NOT_AI_CONTROLLER' },

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
  { scenario: 'ADD: add_eqs_test Dot, one of the engine tests that used to fail', toolName: 'manage_ai', arguments: { action: 'add_eqs_test', queryPath: '${captured:queryPath}', testType: 'Dot' }, expected: 'success' },
  { scenario: 'ADD: add_eqs_context measures the Distance test to the querier', toolName: 'manage_ai', arguments: { action: 'add_eqs_context', queryPath: '${captured:queryPath}', contextType: 'Querier', testIndex: 0, contextProperty: 'DistanceTo' }, expected: 'success' },
  { scenario: 'ADD: add_eqs_context with a context class on the generator', toolName: 'manage_ai', arguments: { action: 'add_eqs_context', queryPath: '${captured:queryPath}', contextType: 'Custom', contextClass: '/Script/AIModule.EnvQueryContext_Querier', optionIndex: 0 }, expected: 'success' },
  { scenario: 'ERROR: add_eqs_context refuses a property the generator lacks', toolName: 'manage_ai', arguments: { action: 'add_eqs_context', queryPath: '${captured:queryPath}', contextType: 'Querier', contextProperty: 'NoSuchContextProperty' }, expected: 'error|NO_CONTEXT_PROPERTY' },
  { scenario: 'CONFIG: configure_test_scoring', toolName: 'manage_ai', arguments: { action: 'configure_test_scoring', queryPath: '${captured:queryPath}', testIndex: 0, testSettings: { scoringEquation: 'Square', clampMin: 0, clampMax: 1, filterType: 'Range', floatMin: 0, floatMax: 5000 } }, expected: 'success' },
  { scenario: 'ERROR: configure_test_scoring refuses a call that sets nothing', toolName: 'manage_ai', arguments: { action: 'configure_test_scoring', queryPath: '${captured:queryPath}', testIndex: 0 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ADD: add_eqs_generator Donut takes its radii', toolName: 'manage_ai', arguments: { action: 'add_eqs_generator', queryPath: '${captured:queryPath}', generatorType: 'Donut', generatorSettings: { innerRadius: 200, outerRadius: 600 } }, expected: 'success' },
  { scenario: 'ERROR: add_eqs_generator refuses a setting the generator does not read', toolName: 'manage_ai', arguments: { action: 'add_eqs_generator', queryPath: '${captured:queryPath}', generatorType: 'SimpleGrid', generatorSettings: { innerRadius: 100 } }, expected: 'error|INVALID_ARGUMENT' },

  // === PERCEPTION ===
  { scenario: 'ADD: add_ai_perception_component', toolName: 'manage_ai', arguments: { action: 'add_ai_perception_component', blueprintPath: '${captured:blueprintPath}' }, expected: 'success' },
  { scenario: 'ADD: add_ai_perception_component keeps the component already there', toolName: 'manage_ai', arguments: { action: 'add_ai_perception_component', blueprintPath: '${captured:blueprintPath}' }, expected: 'success' },
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
  { scenario: 'ERROR: add_state_tree_state refuses a name the tree already has', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: '${captured:stateTreePath}', stateName: childStateName, parentStateName: 'Root' }, expected: 'error|NAME_TAKEN' },
  { scenario: 'ERROR: add_state_tree_state refuses an unknown state type', toolName: 'manage_ai', arguments: { action: 'add_state_tree_state', stateTreePath: '${captured:stateTreePath}', stateName: `Odd_${ts}`, stateType: 'Wobbly' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ADD: add_state_tree_transition', toolName: 'manage_ai', arguments: { action: 'add_state_tree_transition', stateTreePath: '${captured:stateTreePath}', fromState: 'Root', toState: childStateName, triggerType: 'OnStateCompleted' }, expected: 'success' },
  { scenario: 'ERROR: add_state_tree_transition refuses an unknown trigger', toolName: 'manage_ai', arguments: { action: 'add_state_tree_transition', stateTreePath: '${captured:stateTreePath}', fromState: 'Root', toState: childStateName, triggerType: 'Whenever' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CREATE: create_state_tree with the AI controller schema', toolName: 'manage_ai', arguments: { action: 'create_state_tree', name: `${stateTreeName}_AI`, path: TEST_FOLDER, schema: 'ai' }, expected: 'success' },
  { scenario: 'ERROR: create_state_tree refuses a schema class that is no State Tree schema', toolName: 'manage_ai', arguments: { action: 'create_state_tree', name: `${stateTreeName}_Bad`, path: TEST_FOLDER, schema: '/Script/Engine.Actor' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: configure_state_tree_task refuses a call that sets nothing', toolName: 'manage_ai', arguments: { action: 'configure_state_tree_task', stateTreePath: '${captured:stateTreePath}', stateName: childStateName }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CONFIG: configure_state_tree_task with task properties', toolName: 'manage_ai', arguments: { action: 'configure_state_tree_task', stateTreePath: '${captured:stateTreePath}', stateName: childStateName, taskIndex: 0, taskName: 'Delay', selectionBehavior: 'TryEnterState', properties: { Duration: 1.5 } }, expected: 'success|not found' },

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
  { scenario: 'CONFIG: configure_slot_behavior refuses an unregistered activity tag', toolName: 'manage_ai', arguments: { action: 'configure_slot_behavior', definitionPath: '${captured:definitionPath}', slotIndex: 0, activityTags: ['MCP.Test.UnregisteredTag'] }, expected: 'error' },
  { scenario: 'ERROR: configure_slot_behavior refuses a call that sets nothing', toolName: 'manage_ai', arguments: { action: 'configure_slot_behavior', definitionPath: '${captured:definitionPath}', slotIndex: 0 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ADD: add_smart_object_component', toolName: 'manage_ai', arguments: { action: 'add_smart_object_component', blueprintPath: '${captured:blueprintPath}' }, expected: 'success' },
  { scenario: 'ERROR: add_smart_object_component refuses a component name the Blueprint has', toolName: 'manage_ai', arguments: { action: 'add_smart_object_component', blueprintPath: '${captured:blueprintPath}' }, expected: 'error|ALREADY_EXISTS' },
  { scenario: 'ERROR: add_smart_object_component refuses a definition that does not load', toolName: 'manage_ai', arguments: { action: 'add_smart_object_component', blueprintPath: '${captured:blueprintPath}', componentName: `SmartObject_${ts}`, definitionPath: `${TEST_FOLDER}/NoSuchDefinition` }, expected: 'error|NOT_FOUND' },

  // === MASS AI ===
  {
    scenario: 'CREATE: create_mass_entity_config',
    toolName: 'manage_ai',
    arguments: { action: 'create_mass_entity_config', name: massConfigName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'configPath', fromField: 'result.configPath' }
  },
  { scenario: 'ERROR: configure_mass_entity refuses a call that sets nothing', toolName: 'manage_ai', arguments: { action: 'configure_mass_entity', configPath: '${captured:configPath}' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: configure_mass_entity refuses a config as its own parent', toolName: 'manage_ai', arguments: { action: 'configure_mass_entity', configPath: '${captured:configPath}', parentConfigPath: '${captured:configPath}', traitIndex: 0, traitClass: 'MassMovementTrait', properties: {} }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CONFIG: configure_mass_entity adds a trait the new config lacks', toolName: 'manage_ai', arguments: { action: 'configure_mass_entity', configPath: '${captured:configPath}', traitClass: 'MassMovementTrait' }, expected: 'success' },
  { scenario: 'ERROR: configure_mass_entity refuses a traitClass that is no Mass trait', toolName: 'manage_ai', arguments: { action: 'configure_mass_entity', configPath: '${captured:configPath}', traitClass: 'StaticMeshActor' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ADD: add_mass_spawner places a spawner', toolName: 'manage_ai', arguments: { action: 'add_mass_spawner', configPath: '${captured:configPath}', queryPath: '${captured:queryPath}', actorName: massSpawnerName, spawnCount: 10, location: { x: 0, y: 400, z: 100 } }, expected: 'success' },
  { scenario: 'ERROR: add_mass_spawner refuses a label already in the level', toolName: 'manage_ai', arguments: { action: 'add_mass_spawner', configPath: '${captured:configPath}', queryPath: '${captured:queryPath}', actorName: massSpawnerName }, expected: 'error|NAME_TAKEN' },
  { scenario: 'ERROR: add_mass_spawner refuses a query path that is no Environment Query', toolName: 'manage_ai', arguments: { action: 'add_mass_spawner', configPath: '${captured:configPath}', queryPath: '${captured:configPath}' }, expected: 'error|TYPE_MISMATCH' },

  // === UTILITY / ALIASES ===
  { scenario: 'INFO: get_ai_info', toolName: 'manage_ai', arguments: { action: 'get_ai_info', controllerPath: '${captured:controllerPath}' }, expected: 'success' },
  // `params` is the gateway passthrough merged into top-level arguments by
  // consolidated-call-utils.ts for clients that cannot send arbitrary top-level
  // fields; this case proves the merged form reaches the handler identically.
  { scenario: 'INFO: get_ai_info via params passthrough', toolName: 'manage_ai', arguments: { action: 'get_ai_info', params: { controllerPath: '${captured:controllerPath}' } }, expected: 'success' },
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
  { scenario: 'CONFIG: set_focus on the player', toolName: 'manage_ai', arguments: { action: 'set_focus', controllerPath: '${captured:controllerPath}', focusPlayer: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Tick logic compiles' }, { path: 'structuredContent.result.focusTarget', equals: 'player', label: 'the player is the target' }] },
  { scenario: 'CONFIG: set_focus on a tag replaces the player target', toolName: 'manage_ai', arguments: { action: 'set_focus', controllerPath: '${captured:controllerPath}', focusTag: 'FocusTarget' }, expected: 'success', assertions: [{ path: 'structuredContent.result.previousRemoved', gte: 1, label: 'the player logic was taken out first' }] },
  { scenario: 'ERROR: set_focus refuses two targets', toolName: 'manage_ai', arguments: { action: 'set_focus', controllerPath: '${captured:controllerPath}', focusPlayer: true, focusTag: 'FocusTarget' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'DELETE: clear_focus', toolName: 'manage_ai', arguments: { action: 'clear_focus', controllerPath: '${captured:controllerPath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.removed', equals: true, label: 'the focus logic came out' }] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: testActorName }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete mass spawner', toolName: 'control_actor', arguments: { action: 'delete', actorName: massSpawnerName }, expected: 'success|not found' },
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
    // The graph route reads the tree as assetPath or behaviorTreePath (ReadBehaviorTreePath); every variant declares both.
    {
      scenario: 'CONFIG: set_node_properties names the tree as behaviorTreePath',
      toolName: 'manage_ai',
      arguments: {
        action: 'set_node_properties',
        behaviorTreePath: '${captured:behaviorTreePath}',
        nodeId: '${captured:waitNodeId}',
        comment: 'MCP behavior tree test node, named by behaviorTreePath'
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
  const NAV_VOLUME = `NavModifierVolume_${ts}`;
  const OBSTACLE_AREA = '/Script/NavigationSystem.NavArea_Obstacle';
  // get_navigation_path: an open floor with a walled room on it, far from everything else.
  const PATH_TAG = `MCPPathTag_${ts}`;
  const PATH_BOUNDS = `MCP_PathBounds_${ts}`;
  const PATH_MARK = `MCP_PathMark_${ts}`;
  const wall = (side, x, y, sx, sy) => ({ actorName: `MCP_PathWall${side}_${ts}`, location: { x, y, z: 150 }, scale: [sx, sy, 3] });
  // analyze_level_layout: floor slabs far from everything else, read as spaces. A 10 m room joined by a 3 m wide
  // corridor to an 8 m room with a 2 x 3 m alcove off it, and a 6 m room 4 m away that nothing joins.
  const LAYOUT_TAG = `MCPLayoutTag_${ts}`;
  const LAYOUT_BOUNDS = `MCP_LayoutBounds_${ts}`;
  const LAYOUT_START = `MCP_LayoutStart_${ts}`;
  const LAYOUT_GOAL = `MCP_LayoutGoal_${ts}`;
  const LO = -34000;
  const slab = (name, x0, y0, x1, y1) => ({ actorName: `MCP_Layout${name}_${ts}`, location: { x: LO + (x0 + x1) / 2, y: LO + (y0 + y1) / 2, z: -10 }, scale: [(x1 - x0) / 100, (y1 - y0) / 100, 0.2] });
  const LAYOUT_BOX = { center: { x: LO + 1800, y: LO + 500, z: 0 }, extent: { x: 1900, y: 600, z: 200 } };

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: create navigation blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: NAV_BLUEPRINT, savePath: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success', captureResult: { key: 'navBlueprintPath', fromField: 'result.assetPath' } },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_nav_mesh_settings', toolName: 'manage_ai', arguments: {"action": "configure_nav_mesh_settings", "cellSize": 19, "cellHeight": 10, "tileSizeUU": 1000, "minRegionArea": 0, "mergeRegionSize": 400, "maxSimplificationError": 1.3}, expected: 'success' },
    { scenario: 'CONFIG: set_nav_agent_properties', toolName: 'manage_ai', arguments: {"action": "set_nav_agent_properties", "agentRadius": 35, "agentHeight": 144, "agentStepHeight": 35, "agentMaxSlope": 44}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: rebuild_navigation', toolName: 'manage_ai', arguments: {"action": "rebuild_navigation"}, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_nav_modifier_component', toolName: 'manage_ai', arguments: {"action": "create_nav_modifier_component", "blueprintPath": "${captured:navBlueprintPath}", "componentName": NAV_MODIFIER, "areaClass": OBSTACLE_AREA, "failsafeExtent": {"x": 120, "y": 80, "z": 60}, "save": true}, expected: 'success' },
    { scenario: 'Setup: spawn nav test actor', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: '${captured:navBlueprintPath}', actorName: NAV_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_nav_area_class', toolName: 'manage_ai', arguments: {"action": "set_nav_area_class", "actorName": NAV_ACTOR, "areaClass": OBSTACLE_AREA, "componentName": NAV_MODIFIER}, expected: 'success' },
    { scenario: 'CREATE: create_nav_modifier_component on a placed actor', toolName: 'manage_ai', arguments: { action: 'create_nav_modifier_component', actorName: NAV_ACTOR, componentName: `NavModifierInstance_${ts}`, areaClass: OBSTACLE_AREA }, expected: 'success' },
    { scenario: 'CREATE: create_nav_modifier places a volume without a Blueprint', toolName: 'manage_ai', arguments: { action: 'create_nav_modifier', actorName: NAV_VOLUME, location: { x: 0, y: 400, z: 0 }, extent: { x: 150, y: 150, z: 100 }, areaClass: OBSTACLE_AREA }, expected: 'success' },
    { scenario: 'CONFIG: configure_nav_area_cost', toolName: 'manage_ai', arguments: {"action": "configure_nav_area_cost", "areaClass": OBSTACLE_AREA, "areaCost": 1.0, "fixedAreaEnteringCost": 0}, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_nav_link_proxy', toolName: 'manage_ai', arguments: {"action": "create_nav_link_proxy", "actorName": NAV_LINK, "location": {"x": 0, "y": 0, "z": 100}, "startPoint": {"x": -100, "y": 0, "z": 0}, "endPoint": {"x": 100, "y": 0, "z": 0}, "direction": "BothWays"}, expected: 'success' },
    { scenario: 'ERROR: create_nav_link_proxy refuses a label already in the level', toolName: 'manage_ai', arguments: {"action": "create_nav_link_proxy", "actorName": NAV_LINK, "location": {"x": 0, "y": 0, "z": 100}, "startPoint": {"x": -100, "y": 0, "z": 0}, "endPoint": {"x": 100, "y": 0, "z": 0}}, expected: 'error|NAME_TAKEN' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_nav_link', toolName: 'manage_ai', arguments: {"action": "configure_nav_link", "actorName": NAV_LINK, "snapRadius": 30}, expected: 'success' },
    { scenario: 'ERROR: configure_nav_link refuses a call that sets nothing', toolName: 'manage_ai', arguments: { action: 'configure_nav_link', actorName: NAV_LINK }, expected: 'error|INVALID_ARGUMENT' },
    { scenario: 'CONFIG: configure_nav_link direction alone', toolName: 'manage_ai', arguments: { action: 'configure_nav_link', actorName: NAV_LINK, direction: 'LeftToRight' }, expected: 'success' },
    { scenario: 'CONFIG: set_nav_link_type', toolName: 'manage_ai', arguments: {"action": "set_nav_link_type", "actorName": NAV_LINK, "linkType": "smart"}, expected: 'success' },
    // === CREATE ===
    { scenario: 'CREATE: create_smart_link', toolName: 'manage_ai', arguments: {"action": "create_smart_link", "actorName": SMART_LINK, "location": {"x": 0, "y": 200, "z": 100}, "startPoint": {"x": -100, "y": 0, "z": 0}, "endPoint": {"x": 100, "y": 0, "z": 0}, "direction": "BothWays"}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_smart_link_behavior', toolName: 'manage_ai', arguments: {"action": "configure_smart_link_behavior", "actorName": SMART_LINK, "linkEnabled": true, "enabledAreaClass": "/Script/NavigationSystem.NavArea_Default", "disabledAreaClass": "/Script/NavigationSystem.NavArea_Null", "broadcastRadius": 900, "broadcastInterval": 0, "bCreateBoxObstacle": true, "obstacleAreaClass": "/Script/NavigationSystem.NavArea_Null", "obstacleExtent": {"x": 75, "y": 50, "z": 40}, "obstacleOffset": {"x": 0, "y": 0, "z": 20}}, expected: 'success' },
    { scenario: 'ERROR: configure_smart_link_behavior refuses an area class that does not load', toolName: 'manage_ai', arguments: { action: 'configure_smart_link_behavior', actorName: SMART_LINK, enabledAreaClass: '/Script/NavigationSystem.NoSuchNavArea' }, expected: 'error|INVALID_AREA_CLASS' },
    // === INFO ===
    { scenario: 'INFO: get_navigation_info', toolName: 'manage_ai', arguments: {"action": "get_navigation_info"}, expected: 'success' },
    { scenario: 'Setup: a floor with a closed room on it, and a marker', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [PATH_TAG] }, actors: [
      { actorName: `MCP_PathFloor_${ts}`, location: { x: 34000, y: 34000, z: -10 }, scale: [20, 20, 0.2] },
      wall('N', 34600, 34760, 3.4, 0.2), wall('S', 34600, 34440, 3.4, 0.2), wall('E', 34760, 34600, 0.2, 3.4), wall('W', 34440, 34600, 0.2, 3.4),
      { actorName: PATH_MARK, classPath: '/Script/Engine.TargetPoint', location: { x: 33300, y: 33300, z: 50 } },
    ] }, expected: 'success' },
    { scenario: 'Setup: nav mesh bounds around the floor', toolName: 'manage_level_structure', arguments: { action: 'create_nav_mesh_bounds_volume', volumeName: PATH_BOUNDS, location: { x: 34000, y: 34000, z: 0 }, extent: { x: 1100, y: 1100, z: 400 } }, expected: 'success' },
    { scenario: 'Setup: build the nav mesh', toolName: 'manage_ai', arguments: { action: 'rebuild_navigation' }, expected: 'success' },
    { scenario: 'READ: a path across the open floor', toolName: 'manage_ai', arguments: { action: 'get_navigation_path', from: { x: 33200, y: 33200, z: 20 }, to: { x: 33200, y: 34800, z: 20 } }, expected: 'success', assertions: [
      { path: 'structuredContent.result.reachable', equals: true, label: 'the open floor connects' },
      { path: 'structuredContent.result.reason', equals: 'ok', label: 'reason ok' },
    ] },
    { scenario: 'READ: no full path into the closed room', toolName: 'manage_ai', arguments: { action: 'get_navigation_path', fromActor: PATH_MARK, to: { x: 34600, y: 34600, z: 20 }, projectExtent: { x: 50, y: 50, z: 250 } }, expected: 'success', assertions: [
      { path: 'structuredContent.result.reachable', equals: false, label: 'the walls close it off' },
    ] },
    { scenario: 'READ: a goal high above the floor is off the nav mesh', toolName: 'manage_ai', arguments: { action: 'get_navigation_path', from: { x: 33200, y: 33200, z: 20 }, to: { x: 33200, y: 33200, z: 3000 } }, expected: 'success', assertions: [
      { path: 'structuredContent.result.reason', equals: 'goal_off_navmesh', label: 'nothing walkable near the goal' },
    ] },
    { scenario: 'ERROR: get_navigation_path needs a goal', toolName: 'manage_ai', arguments: { action: 'get_navigation_path', from: { x: 33200, y: 33200, z: 20 } }, expected: 'error|INVALID_ARGUMENT' },
    { scenario: 'ERROR: get_navigation_path with an actor not in the level', toolName: 'manage_ai', arguments: { action: 'get_navigation_path', from: { x: 33200, y: 33200, z: 20 }, toActor: `MCP_NoSuchActor_${ts}` }, expected: 'error|INVALID_ARGUMENT' },
    { scenario: 'Cleanup: the path scene', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: PATH_TAG }, expected: 'success|not found' },
    { scenario: 'Cleanup: the path nav bounds', toolName: 'control_actor', arguments: { action: 'delete', actorName: PATH_BOUNDS }, expected: 'success|not found' },
    { scenario: 'Setup: two rooms, a corridor, an alcove and a room nothing joins', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [LAYOUT_TAG] }, actors: [
      slab('RoomA', 0, 0, 1000, 1000), slab('Corridor', 1000, 350, 1600, 650), slab('RoomB', 1600, 100, 2400, 900), slab('Alcove', 2400, 600, 2600, 900), slab('Island', 3000, 200, 3600, 800),
      { actorName: LAYOUT_START, classPath: '/Script/Engine.TargetPoint', location: { x: LO + 300, y: LO + 500, z: 50 } },
      { actorName: LAYOUT_GOAL, classPath: '/Script/Engine.TargetPoint', location: { x: LO + 2500, y: LO + 800, z: 50 } },
    ] }, expected: 'success' },
    { scenario: 'Setup: nav mesh bounds around the slabs', toolName: 'manage_level_structure', arguments: { action: 'create_nav_mesh_bounds_volume', volumeName: LAYOUT_BOUNDS, location: { x: LO + 1800, y: LO + 500, z: 0 }, extent: { x: 2000, y: 700, z: 400 } }, expected: 'success' },
    { scenario: 'Setup: build the layout nav mesh', toolName: 'manage_ai', arguments: { action: 'rebuild_navigation' }, expected: 'success' },
    { scenario: 'READ: analyze_level_layout reads rooms, corridors, the choke and the dead ends', toolName: 'manage_ai', arguments: { action: 'analyze_level_layout', ...LAYOUT_BOX, startActor: LAYOUT_START, goalActor: LAYOUT_GOAL, image: true }, expected: 'success', assertions: [
      { path: 'structuredContent.result.message', includes: '3 room(s) and 2 corridor(s)', label: 'three rooms (one cut off) and two corridors (the passage and the alcove)' },
      { path: 'structuredContent.result.chokes.0', equals: 'Corridor 1', label: 'the passage between the rooms is the top choke' },
      { path: 'structuredContent.result.deadEnds', length: 2, label: 'two dead ends' },
      { path: 'structuredContent.result.deadEnds.1', equals: 'Corridor 2', label: 'the alcove is one' },
      { path: 'structuredContent.result.loops', equals: 0, label: 'no loop' },
      { path: 'structuredContent.result.unreachable.0', equals: 'Room 3', label: 'the room nothing joins cannot be walked to' },
      { path: 'structuredContent.result.path.spaces', length: 4, label: 'the walk crosses room, passage, room, alcove' },
      { path: 'structuredContent.result.imagePath', exists: true, label: 'the picture was written' },
    ] },
    { scenario: 'READ: analyze_level_layout says a goal on the cut-off room cannot be walked to', toolName: 'manage_ai', arguments: { action: 'analyze_level_layout', ...LAYOUT_BOX, startActor: LAYOUT_START, goal: { x: LO + 3300, y: LO + 500, z: 20 }, cellCm: 100, roomWidthCm: 400 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.path.reachable', equals: false, label: 'no walk reaches it' },
    ] },
    { scenario: 'READ: analyze_level_layout widens the cell past maxNodes and says so', toolName: 'manage_ai', arguments: { action: 'analyze_level_layout', ...LAYOUT_BOX, startActor: LAYOUT_START, maxNodes: 100 }, expected: 'success', assertions: [
      { path: 'structuredContent.result.message', includes: 'Sampled every', label: 'the wider cell is named' },
    ] },
    { scenario: 'ERROR: analyze_level_layout needs center and extent together', toolName: 'manage_ai', arguments: { action: 'analyze_level_layout', center: LAYOUT_BOX.center }, expected: 'error|INVALID_ARGUMENT' },
    { scenario: 'ERROR: analyze_level_layout with a goal outside the area read', toolName: 'manage_ai', arguments: { action: 'analyze_level_layout', ...LAYOUT_BOX, startActor: LAYOUT_START, goal: { x: LO + 9000, y: LO + 500, z: 20 } }, expected: 'error|INVALID_ARGUMENT' },
    { scenario: 'ERROR: analyze_level_layout with a start not in the level', toolName: 'manage_ai', arguments: { action: 'analyze_level_layout', ...LAYOUT_BOX, startActor: `MCP_NoSuchActor_${ts}` }, expected: 'error|ACTOR_NOT_FOUND' },
    { scenario: 'ERROR: analyze_level_layout over a box with no nav mesh', toolName: 'manage_ai', arguments: { action: 'analyze_level_layout', center: { x: LO - 6000, y: LO - 6000, z: 0 }, extent: { x: 300, y: 300, z: 100 } }, expected: 'error|NO_WALKABLE_AREA' },
    { scenario: 'Cleanup: the layout scene', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: LAYOUT_TAG }, expected: 'success|not found' },
    { scenario: 'Cleanup: the layout nav bounds', toolName: 'control_actor', arguments: { action: 'delete', actorName: LAYOUT_BOUNDS }, expected: 'success|not found' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete nav test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: NAV_ACTOR }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete nav modifier volume', toolName: 'control_actor', arguments: { action: 'delete', actorName: NAV_VOLUME }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete nav link proxy', toolName: 'control_actor', arguments: { action: 'delete', actorName: NAV_LINK }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete smart link proxy', toolName: 'control_actor', arguments: { action: 'delete', actorName: SMART_LINK }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('manage-ai', testCases);
