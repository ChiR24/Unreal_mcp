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
import type { CapabilityRecordSource } from '../../../model.js';
import { BT, EQS, MASS_AI, SMART_OBJECTS, STATE_TREE, aiRecord } from './builder.js';
import { NAV } from './properties-navigation.js';
import { AI } from './properties.js';
import { CONTROLLER_LOGIC_OUT, REMOVED_OUT } from './set-actions.data.js';

const A = AI;
const N = NAV;

export const AI_CONFIGURE_RECORDS: readonly CapabilityRecordSource[] = [
  aiRecord({
    action: 'assign_behavior_tree', summary: 'Put a Behavior Tree in an AIController Blueprint: its BehaviorTree variable (one it declares, else a new instance-editable DefaultBehaviorTree) holds behaviorTreePath by default, compiled and saved. Nothing runs the tree until run_behavior_tree writes Run Behavior Tree on possess, or the controller calls it itself. NOT_AI_CONTROLLER for another Blueprint, NOT_FOUND for a missing tree.',
    use: 'A controller should hold a Behavior Tree that its own logic runs.',
    avoid: 'The tree should run whenever the controller possesses a pawn; run_behavior_tree sets the variable too.',
    props: { controllerPath: A.controllerPath, behaviorTreePath: A.behaviorTreePath },
    required: ['controllerPath', 'behaviorTreePath'], plugins: BT,
    out: { ...CONTROLLER_LOGIC_OUT, behaviorTreePath: { type: 'string', description: 'The Behavior Tree, by object path.' }, variable: { type: 'string', description: 'The BehaviorTree variable that holds it.' } },
    example: { controllerPath: '/Game/AI/AIC_Enemy', behaviorTreePath: '/Game/AI/BT_Enemy' },
    result: 'Behavior Tree assigned',
  }),
  aiRecord({
    action: 'assign_blackboard', summary: 'Assign a Blackboard to an AIController or Behavior Tree asset.',
    use: 'A controller or Behavior Tree needs its Blackboard bound.',
    avoid: 'Use add_blackboard_key to author the Blackboard contents.',
    props: {
      blackboardPath: A.blackboardPath,
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
    props: { assetPath: A.assetPath, behaviorTreePath: A.behaviorTreePath, nodeId: A.nodeId },
    requiredOneOf: ['assetPath', 'behaviorTreePath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { assetPath: '/Game/AI/BT_Enemy', nodeId: 'Node_0' }, result: 'Connections broken',
  }),
  aiRecord({
    action: 'clear_focus', summary: 'Take out the focus logic set_focus wrote into an AIController Blueprint, so it stops seeking a target each frame (a focus already set in a running game stays until something clears it), compiled and saved; removed false, and nothing changed, when there was none.',
    use: 'A controller should stop tracking its focus target.',
    avoid: 'Use set_focus to give it a target.',
    props: { controllerPath: A.controllerPath }, required: ['controllerPath'],
    out: REMOVED_OUT,
    example: { controllerPath: '/Game/AI/AIC_Enemy' }, result: 'Focus cleared',
  }),
  aiRecord({
    action: 'configure_bt_node', summary: 'Configure a Behavior Tree node.',
    use: 'An existing Behavior Tree node needs property edits.',
    avoid: 'Use set_node_properties for the graph route.',
    props: {
      behaviorTreePath: A.behaviorTreePath,
      nodeId: A.nodeId, properties: A.properties,
    },
    required: ['behaviorTreePath', 'nodeId'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', nodeId: 'Root' }, result: 'Node configured',
  }),
  aiRecord({
    action: 'configure_damage_sense_config', summary: 'Set the damage sense of an AIPerception component (added when missing) and save: damageConfig.maxAge, the seconds a hit is remembered (a new sense starts at 10; 0 is forever). Hits reach it only through Report Damage Event, not from Apply Damage alone.',
    use: 'An AIPerception component needs damage-sense tuning.',
    avoid: 'Use configure_sight_config or configure_hearing_config for other senses.',
    props: { blueprintPath: A.blueprintPath, damageConfig: A.damageConfig },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', damageConfig: { maxAge: 10 } },
    result: 'Damage sense configured',
  }),
  aiRecord({
    action: 'configure_hearing_config', summary: 'Set the hearing sense of an AIPerception component (added when missing) and save: hearingRange, and in hearingConfig detectFriendly and maxAge (seconds, 0 forever); a hearingConfig field wins over the top-level one. Only what is sent changes; a new hearing sense hears 3000 units, enemies and neutrals, for 5 s, and the reply carries its settings under these names. Sounds reach it only through Report Noise Event or a Pawn Noise Emitter, not from audio.',
    use: 'An AIPerception component needs hearing-sense tuning.',
    avoid: 'Use setup_perception to configure several senses at once.',
    props: {
      blueprintPath: A.blueprintPath,
      hearingConfig: A.hearingConfig, hearingRange: A.hearingRange,
    },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', hearingConfig: { hearingRange: 2000 } },
    result: 'Hearing sense configured',
  }),
  aiRecord({
    action: 'configure_mass_entity', summary: 'Change a Mass entity config and save: traitClass adds that trait when the config has none of it (a new config starts with no traits, and traits such as MassMovementTrait or a visualization trait are what give entities data and behaviour), properties go onto the trait traitIndex or traitClass names, else the first trait that takes them, and parentConfigPath makes it derive from another config (one that already derives from it is refused). A call with none of these, or a traitClass that is no Mass trait, is refused; traits lists the config\'s traits after the call, traitAdded says whether one was added, applied and failed name the fields.',
    use: 'A Mass entity config needs a trait, trait settings or a parent config.',
    avoid: 'Use add_mass_spawner to spawn from the config.',
    props: { configPath: A.configPath, parentConfigPath: { type: 'string', description: 'Parent Mass entity config asset to inherit from.' }, traitClass: { type: 'string', description: 'Mass trait class (short name or path, a UMassEntityTraitBase subclass): edited when the config has it, added when it does not.' }, traitIndex: { type: 'number', description: 'Zero-based trait index (alternative to traitClass).' }, properties: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Trait property values keyed by UPROPERTY name.' } }, required: ['configPath'], plugins: MASS_AI,
    out: { configPath: A.configPath },
    example: { configPath: '/Game/AI/Mass/MEC_Crowd', traitClass: 'MassMovementTrait' }, result: 'Mass entity configured',
  }),
  aiRecord({
    action: 'configure_nav_area_cost', summary: 'Configure navigation area cost data.',
    use: 'A navigation area class needs a different traversal cost.',
    avoid: 'Use set_nav_area_class to apply an area to an actor.',
    props: {
      areaClass: N.areaClass, areaCost: N.areaCost,
      fixedAreaEnteringCost: { type: 'number', description: 'Flat cost added once when a path enters the area; left unchanged when omitted.' },
    },
    required: ['areaClass'],
    example: { areaClass: '/Script/NavigationSystem.NavArea_Obstacle', areaCost: 1 },
    result: 'Nav area cost configured',
  }),
  aiRecord({
    action: 'configure_nav_link', summary: 'Change the link of a placed NavLinkProxy (actorName): startPoint and endPoint (relative to the actor) and direction go on its smart link when it uses one, else on its point link, and snapRadius on the point link (refused for a smart link). Only what is sent changes, and a call that sends none of them is refused. The navmesh takes the change at once; one undo step, the level left unsaved; the reply gives the link as it is now.',
    use: 'A placed nav link actor needs endpoint or snap tuning.',
    avoid: 'Use create_nav_link_proxy to place the link first.',
    props: {
      actorName: A.actorName, startPoint: N.startPoint,
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
      cellSize: N.cellSize, cellHeight: N.cellHeight, tileSizeUU: N.tileSizeUU,
      minRegionArea: N.minRegionArea, mergeRegionSize: N.mergeRegionSize,
      maxSimplificationError: N.maxSimplificationError, agentStepHeight: N.agentStepHeight,
    },
    example: { cellSize: 19, cellHeight: 10, tileSizeUU: 1000 }, result: 'NavMesh settings configured',
  }),
  aiRecord({
    action: 'configure_sight_config', summary: 'Set the sight sense of an AIPerception component (added when missing) and save: sightRadius, loseSightRadius, peripheralVisionAngle (half-angle in degrees), and in sightConfig also pointOfViewBackwardOffset, nearClippingRadius, autoSuccessRange, maxAge (seconds a sighting is remembered, 0 forever) and detectionByAffiliation {enemies, neutrals, friendlies}; a sightConfig field wins over the top-level one. Only what is sent changes; a new sight sense starts at 3000, losing sight 500 further, 90 degrees, enemies and neutrals, 5 s. An actor with no team counts as neutral, so neutrals must stay detected for an AI to see the player.',
    use: 'An AIPerception component needs sight-sense tuning.',
    avoid: 'Use setup_perception to configure several senses at once.',
    props: {
      blueprintPath: A.blueprintPath, sightConfig: A.sightConfig,
      sightRadius: A.sightRadius, loseSightRadius: A.loseSightRadius,
      peripheralVisionAngle: A.peripheralVisionAngle,
    },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', sightConfig: { sightRadius: 3000 } },
    result: 'Sight sense configured',
  }),
  aiRecord({
    action: 'configure_slot_behavior', summary: 'Change slot slotIndex (default 0) of a Smart Object definition and save: enabled, and activityTags added to its activity tags (registered gameplay tags only: an unregistered one is refused and nothing changes). A call with neither is refused; behaviorCount reports its behavior definitions.',
    use: 'An existing Smart Object slot needs enable/index edits.',
    avoid: 'Use add_smart_object_slot to create the slot.',
    props: {
      definitionPath: A.definitionPath,
      slotIndex: A.slotIndex, enabled: A.enabled,
      activityTags: { type: 'array', items: { type: 'string' }, description: 'Registered gameplay tags added to the slot activity tags; an unregistered tag fails the call.' },
    },
    required: ['definitionPath'], plugins: SMART_OBJECTS,
    out: { definitionPath: A.definitionPath },
    example: { definitionPath: '/Game/AI/SO_Bench', slotIndex: 0, enabled: true },
    result: 'Slot behavior configured',
  }),
  aiRecord({
    action: 'configure_smart_link_behavior', summary: 'Change the smart link of a NavLinkProxy (actorName): linkEnabled switches it on or off; enabledAreaClass and disabledAreaClass are the navigation areas it marks while on and off (NavArea_Null blocks it); broadcastRadius and broadcastInterval set who hears it switch, heard only while its Notify When Enabled or Disabled is on, and the one left out keeps its value; bCreateBoxObstacle true sets its box obstacle of obstacleAreaClass (default NavArea_Null), obstacleExtent (default 100) and obstacleOffset. An area class that does not load is refused before anything changes, as is a call that sets nothing. The settings stay on a proxy that uses its point link and apply once set_nav_link_type makes it smart. One undo step; the level is left unsaved.',
    use: 'A smart link needs area-class switching or an obstacle.',
    avoid: 'Use configure_nav_link for a simple link.',
    props: {
      actorName: A.actorName, linkEnabled: N.linkEnabled,
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
    action: 'configure_state_tree_task', summary: 'Change a task of a State Tree state (taskName, else taskIndex, default the first) by its properties (UPROPERTY names on the task, its instance data or its Blueprint instance) and/or the state selectionBehavior (TryEnterState, TrySelectChildrenInOrder, TrySelectChildrenAtRandom, TryFollowTransitions), compiled and saved. A call with neither, a task that is not found, or properties no part of the task takes are refused; applied and failed name the fields.',
    use: 'A StateTree state needs its task settings changed.',
    avoid: 'Use add_state_tree_state to create the state.',
    props: { stateTreePath: A.stateTreePath, stateName: A.stateName, taskName: { type: 'string', description: 'Task to configure, matched by class or instance name (defaults to the first task).' }, taskIndex: { type: 'number', description: 'Zero-based task index in the state (alternative to taskName).' }, selectionBehavior: { type: 'string', description: 'State selection behavior (e.g. TryEnterState, TrySelectChildrenInOrder, TryFollowTransitions).' }, properties: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Task property values keyed by UPROPERTY name, imported by reflection onto the task instance.' } },
    required: ['stateTreePath', 'stateName'], plugins: STATE_TREE,
    out: { assetPath: A.assetPath },
    example: { stateTreePath: '/Game/AI/ST_Enemy', stateName: 'Patrol', selectionBehavior: 'TrySelectChildrenInOrder' }, result: 'StateTree task configured',
  }),
  aiRecord({
    action: 'configure_test_scoring', summary: 'Set how a test of an Environment Query (testIndex, counted across options) filters and scores, and save: scoringEquation (Linear, Square, InverseLinear, SquareRoot or Constant), clampMin and clampMax, filterType (Minimum, Maximum, Range or Match) with floatMin and floatMax (a float range filters by Range unless filterType says otherwise), in testSettings or at the top level. The test becomes Filter and Score; unknown names and a call that sets nothing are refused.',
    use: 'An existing Environment Query test needs scoring or filter tuning.',
    avoid: 'Use add_eqs_test to create the test.',
    props: {
      queryPath: A.queryPath,
      testIndex: A.testIndex, testSettings: A.testSettings,
    },
    required: ['queryPath'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { queryPath: '/Game/AI/EQS_Cover', testIndex: 0, testSettings: { scoringEquation: 'InverseLinear' } }, result: 'Test scoring configured',
  }),
  aiRecord({
    action: 'connect_nodes', summary: 'Connect Behavior Tree graph nodes.',
    use: 'A parent graph node must be linked to a child.',
    avoid: 'Use break_connections to detach them again.',
    props: {
      assetPath: A.assetPath, behaviorTreePath: A.behaviorTreePath,
      parentNodeId: A.parentNodeId, childNodeId: A.childNodeId,
    },
    requiredOneOf: ['assetPath', 'behaviorTreePath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { assetPath: '/Game/AI/BT_Enemy', parentNodeId: 'Node_0', childNodeId: 'Node_1' },
    result: 'Nodes connected',
  }),
];
