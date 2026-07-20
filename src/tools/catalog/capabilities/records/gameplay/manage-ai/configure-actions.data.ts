/**
 * manage_ai records, shard 2 of 4: assign / break / clear / configure / connect.
 *
 * Exact per-action properties only. The `configure_nav_*` and
 * `configure_smart_link_behavior` records carry no required field beyond
 * `action` because the native navigation handlers read every tunable through
 * `Payload->HasField` and skip what is absent (NavMeshSettings.cpp,
 * SmartLinks.cpp) -- they resolve their target from the EDITOR world, so they
 * stay editorState 'edit' like the rest of manage_ai.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { BT, EQS, MASS_AI, SMART_OBJECTS, STATE_TREE, aiRecord } from './builder.js';
import { NAV } from './properties-navigation.js';
import { AI } from './properties.js';

const A = AI;
const N = NAV;

export const AI_CONFIGURE_RECORDS: readonly CapabilityRecordSource[] = [
  aiRecord({
    action: 'assign_behavior_tree', summary: 'Assign a Behavior Tree to an AIController asset.',
    use: 'A controller asset should run a specific Behavior Tree.',
    avoid: 'Use assign_blackboard to bind the Blackboard.',
    props: { action: A.action, controllerPath: A.controllerPath, behaviorTreePath: A.behaviorTreePath },
    required: ['controllerPath', 'behaviorTreePath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { controllerPath: '/Game/AI/AIC_Enemy', behaviorTreePath: '/Game/AI/BT_Enemy' },
    result: 'Behavior Tree assigned',
  }),
  aiRecord({
    action: 'assign_blackboard', summary: 'Assign a Blackboard to an AIController or Behavior Tree asset.',
    use: 'A controller or Behavior Tree needs its Blackboard bound.',
    avoid: 'Use add_blackboard_key to author the Blackboard contents.',
    props: {
      action: A.action, blackboardPath: A.blackboardPath,
      controllerPath: A.controllerPath, behaviorTreePath: A.behaviorTreePath,
    },
    required: ['blackboardPath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', blackboardPath: '/Game/AI/BB_Enemy' },
    result: 'Blackboard assigned',
  }),
  aiRecord({
    action: 'break_connections', summary: 'Break Behavior Tree graph connections.',
    use: 'A graph node must be detached from its links.',
    avoid: 'Use remove_node to delete the node outright.',
    props: { action: A.action, assetPath: A.assetPath, nodeId: A.nodeId },
    required: ['assetPath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { assetPath: '/Game/AI/BT_Enemy', nodeId: 'Node_0' }, result: 'Connections broken',
  }),
  aiRecord({
    action: 'clear_focus', summary: 'Clear the authored focus target on an AIController asset.',
    use: 'A controller asset should stop targeting a focus actor.',
    avoid: 'Use set_focus to assign a target.',
    props: { action: A.action, controllerPath: A.controllerPath }, required: ['controllerPath'],
    out: { assetPath: A.assetPath },
    example: { controllerPath: '/Game/AI/AIC_Enemy' }, result: 'Focus cleared',
  }),
  aiRecord({
    action: 'configure_bt_node', summary: 'Configure a Behavior Tree node.',
    use: 'An existing Behavior Tree node needs property edits.',
    avoid: 'Use set_node_properties for the graph route.',
    props: {
      action: A.action, behaviorTreePath: A.behaviorTreePath,
      nodeId: A.nodeId, properties: A.properties,
    },
    required: ['behaviorTreePath', 'nodeId'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', nodeId: 'Root' }, result: 'Node configured',
  }),
  aiRecord({
    action: 'configure_damage_sense_config', summary: 'Configure damage perception on a Blueprint asset.',
    use: 'An AIPerception component needs damage-sense tuning.',
    avoid: 'Use configure_sight_config or configure_hearing_config for other senses.',
    props: { action: A.action, blueprintPath: A.blueprintPath, damageConfig: A.damageConfig },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', damageConfig: { maxAge: 10 } },
    result: 'Damage sense configured',
  }),
  aiRecord({
    action: 'configure_hearing_config', summary: 'Configure hearing perception on a Blueprint asset.',
    use: 'An AIPerception component needs hearing-sense tuning.',
    avoid: 'Use setup_perception to configure several senses at once.',
    props: {
      action: A.action, blueprintPath: A.blueprintPath,
      hearingConfig: A.hearingConfig, hearingRange: A.hearingRange,
    },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', hearingConfig: { hearingRange: 2000 } },
    result: 'Hearing sense configured',
  }),
  aiRecord({
    action: 'configure_mass_entity', summary: 'Configure a Mass Entity asset.',
    use: 'A Mass Entity config needs its parent config set.',
    avoid: 'Use add_mass_spawner to spawn from the config.',
    props: { action: A.action, configPath: A.configPath }, required: ['configPath'], plugins: MASS_AI,
    out: { configPath: A.configPath },
    example: { configPath: '/Game/AI/Mass/MEC_Crowd' }, result: 'Mass entity configured',
  }),
  aiRecord({
    action: 'configure_nav_area_cost', summary: 'Configure navigation area cost data.',
    use: 'A navigation area class needs a different traversal cost.',
    avoid: 'Use set_nav_area_class to apply an area to an actor.',
    props: { action: A.action, areaClass: N.areaClass, areaCost: N.areaCost },
    required: ['areaClass'],
    example: { areaClass: '/Script/NavigationSystem.NavArea_Obstacle', areaCost: 1 },
    result: 'Nav area cost configured',
  }),
  aiRecord({
    action: 'configure_nav_link', summary: 'Configure an authored navigation link.',
    use: 'A placed nav link actor needs endpoint or snap tuning.',
    avoid: 'Use create_nav_link_proxy to place the link first.',
    props: {
      action: A.action, actorName: A.actorName, startPoint: N.startPoint,
      endPoint: N.endPoint, direction: N.direction, snapRadius: N.snapRadius,
    },
    required: ['actorName'],
    out: { actorName: A.actorName },
    example: { actorName: 'NavLinkProxy_0', snapRadius: 30 }, result: 'Nav link configured',
  }),
  aiRecord({
    action: 'configure_nav_mesh_settings', summary: 'Configure project NavMesh generation settings.',
    use: 'The loaded level RecastNavMesh needs generation tuning.',
    avoid: 'Use set_nav_agent_properties for agent dimensions.',
    props: {
      action: A.action, cellSize: N.cellSize, cellHeight: N.cellHeight, tileSizeUU: N.tileSizeUU,
      minRegionArea: N.minRegionArea, mergeRegionSize: N.mergeRegionSize,
      maxSimplificationError: N.maxSimplificationError, agentStepHeight: N.agentStepHeight,
    },
    example: { cellSize: 19, cellHeight: 10, tileSizeUU: 1000 }, result: 'NavMesh settings configured',
  }),
  aiRecord({
    action: 'configure_sight_config', summary: 'Configure sight perception on a Blueprint asset.',
    use: 'An AIPerception component needs sight-sense tuning.',
    avoid: 'Use setup_perception to configure several senses at once.',
    props: {
      action: A.action, blueprintPath: A.blueprintPath, sightConfig: A.sightConfig,
      sightRadius: A.sightRadius, loseSightRadius: A.loseSightRadius,
      peripheralVisionAngle: A.peripheralVisionAngle,
    },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', sightConfig: { sightRadius: 3000 } },
    result: 'Sight sense configured',
  }),
  aiRecord({
    action: 'configure_slot_behavior', summary: 'Configure Smart Object slot behavior.',
    use: 'An existing Smart Object slot needs enable/index edits.',
    avoid: 'Use add_smart_object_slot to create the slot.',
    props: {
      action: A.action, definitionPath: A.definitionPath,
      slotIndex: A.slotIndex, enabled: A.enabled,
    },
    required: ['definitionPath'], plugins: SMART_OBJECTS,
    out: { definitionPath: A.definitionPath },
    example: { definitionPath: '/Game/AI/SO_Bench', slotIndex: 0, enabled: true },
    result: 'Slot behavior configured',
  }),
  aiRecord({
    action: 'configure_smart_link_behavior', summary: 'Configure smart navigation link behavior.',
    use: 'A smart link needs area-class switching or an obstacle.',
    avoid: 'Use configure_nav_link for a simple link.',
    props: {
      action: A.action, actorName: A.actorName, linkEnabled: N.linkEnabled,
      enabledAreaClass: N.enabledAreaClass, disabledAreaClass: N.disabledAreaClass,
      broadcastRadius: N.broadcastRadius, broadcastInterval: N.broadcastInterval,
      bCreateBoxObstacle: N.bCreateBoxObstacle, obstacleAreaClass: N.obstacleAreaClass,
      obstacleExtent: N.obstacleExtent, obstacleOffset: N.obstacleOffset,
    },
    required: ['actorName'],
    out: { actorName: A.actorName },
    example: { actorName: 'SmartLink_0', linkEnabled: true }, result: 'Smart link behavior configured',
  }),
  aiRecord({
    action: 'configure_state_tree_task', summary: 'Configure a StateTree task.',
    use: 'A StateTree state needs its task settings changed.',
    avoid: 'Use add_state_tree_state to create the state.',
    props: { action: A.action, stateTreePath: A.stateTreePath, stateName: A.stateName },
    required: ['stateTreePath', 'stateName'], plugins: STATE_TREE,
    out: { assetPath: A.assetPath },
    example: { stateTreePath: '/Game/AI/ST_Enemy', stateName: 'Patrol' }, result: 'StateTree task configured',
  }),
  aiRecord({
    action: 'configure_test_scoring', summary: 'Configure Environment Query test scoring.',
    use: 'An existing Environment Query test needs scoring or filter tuning.',
    avoid: 'Use add_eqs_test to create the test.',
    props: {
      action: A.action, queryPath: A.queryPath,
      testIndex: A.testIndex, testSettings: A.testSettings,
    },
    required: ['queryPath'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { queryPath: '/Game/AI/EQS_Cover', testIndex: 0 }, result: 'Test scoring configured',
  }),
  aiRecord({
    action: 'connect_nodes', summary: 'Connect Behavior Tree graph nodes.',
    use: 'A parent graph node must be linked to a child.',
    avoid: 'Use break_connections to detach them again.',
    props: {
      action: A.action, assetPath: A.assetPath,
      parentNodeId: A.parentNodeId, childNodeId: A.childNodeId,
    },
    required: ['assetPath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { assetPath: '/Game/AI/BT_Enemy', parentNodeId: 'Node_0', childNodeId: 'Node_1' },
    result: 'Nodes connected',
  }),
];
