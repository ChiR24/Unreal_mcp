/**
 * manage_ai records, shard 1 of 4: the 16 `add_*` actions.
 */
import type { CapabilityRecordSource } from '../../../model.js';
import { BT, EQS, MASS_AI, SMART_OBJECTS, STATE_TREE, aiRecord } from './builder.js';
import { AI, xyz } from './properties.js';
import { str } from '../../shared/schema-props.js';

const A = AI;

export const AI_ADD_RECORDS: readonly CapabilityRecordSource[] = [
  aiRecord({
    action: 'add_ai_perception_component', summary: 'Add an AIPerception component to an AI controller or pawn Blueprint and save it; a Blueprint that already has one keeps it (alreadyExists true, nothing changed). Senses are added with configure_sight_config, configure_hearing_config and configure_damage_sense_config, or several at once with setup_perception.',
    use: 'A Blueprint needs an AIPerception component before senses are configured.',
    avoid: 'Use setup_perception to add and configure senses in one call.',
    props: { blueprintPath: A.blueprintPath }, required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy' }, result: 'Perception component added',
  }),
  aiRecord({
    action: 'add_blackboard_key', summary: 'Add a key to a Blackboard asset (blackboardPath) and save it: keyName of keyType Bool, Int, Float, Vector, Rotator, Object, Class, Enum, Name or String. An Object or Class key is based on Object unless baseObjectClass names another class, and actor-only key filters (the target key of MoveTo) skip a key not based on Actor, so the actor an AI chases wants baseObjectClass /Script/Engine.Actor; an Enum key takes its enumerators from enumType. isInstanceSynced (default false) shares the value across every AI using the asset, and Blackboards deriving from this one get the key too. A name this Blackboard or a parent already has is refused (NAME_TAKEN), as are a class or enum that does not load and engine content (ENGINE_CONTENT).',
    use: 'A Blackboard needs a new typed entry, such as the actor an AI chases or the place it moves to.',
    avoid: 'Use set_key_instance_synced to change sync on an existing key.',
    props: {
      blackboardPath: A.blackboardPath, keyName: A.keyName, keyType: A.keyType,
      baseObjectClass: A.baseObjectClass, enumType: A.enumType, isInstanceSynced: A.isInstanceSynced,
    },
    required: ['blackboardPath', 'keyName', 'keyType'], plugins: BT,
    out: {
      assetPath: A.assetPath,
      baseClass: { type: 'string', description: 'Class path an Object or Class key is based on.' },
      saved: { type: 'boolean', description: 'Whether the Blackboard reached disk.' },
    },
    example: { blackboardPath: '/Game/AI/BB_Enemy', keyName: 'TargetActor', keyType: 'Object', baseObjectClass: '/Script/Engine.Actor' },
    result: 'Blackboard key added',
  }),
  aiRecord({
    action: 'add_composite_node', summary: 'Add a composite to a Behavior Tree and save it: compositeType Selector (runs its children until one succeeds), Sequence (until one fails) or SimpleParallel (Parallel is the same: its first child is the main task, the second runs beside it). It goes under parentNodeId (a composite by nodeId or nodeName), else under the root, and a tree with no root takes it as the root (isRoot). nodeName names it; nodeId and nodeName in the reply address it for add_task_node, add_decorator and add_service. Another compositeType is refused (INVALID_ARGUMENT), as is a parent that is not found (PARENT_NOT_FOUND). One undo step.',
    use: 'A Behavior Tree needs a Selector, Sequence or Simple Parallel branch.',
    avoid: 'Use add_task_node for leaf behavior.',
    // `nodeName` names the created node, and the response now returns the
    // resolved name plus isRoot. Without them a composite was created unnamed
    // and unaddressable, so add_decorator/add_service/add_task_node had no
    // handle to attach to and a tree could be created but never assembled.
    props: {
      behaviorTreePath: A.behaviorTreePath,
      compositeType: A.compositeType,
      nodeName: { type: 'string', description: 'Name for the created composite node.' },
      parentNodeId: A.parentNodeId,
    },
    required: ['behaviorTreePath', 'compositeType'], plugins: BT,
    out: {
      assetPath: A.assetPath,
      nodeName: { type: 'string', description: 'Resolved name of the created composite node.' },
      isRoot: { type: 'boolean', description: 'Whether this node became the tree root.' },
    },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', compositeType: 'Sequence', nodeName: 'RootSelector' },
    result: 'Composite node added',
  }),
  aiRecord({
    action: 'add_decorator', summary: 'Add a decorator to a Behavior Tree node, or to the root when no node is named, in one undo step.',
    use: 'A Behavior Tree branch needs a conditional gate.',
    avoid: 'Use add_service for recurring background ticks.',
    props: {
      behaviorTreePath: A.behaviorTreePath, decoratorType: A.decoratorType,
      parentNodeId: { type: 'string', description: "Composite or task node the decorator guards (a nodeId from add_composite or add_task); omit or pass 'root' for a root decorator." },
    },
    required: ['behaviorTreePath', 'decoratorType'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', decoratorType: 'Blackboard' },
    result: 'Decorator added',
  }),
  aiRecord({
    action: 'add_eqs_context', summary: 'Point a generator or test of an Environment Query at a context and save: contextType Querier (the AI running the query), Item (each item being tested) or Custom with contextClass (a Blueprint based on EnvQueryContext_BlueprintBase). It sets the context property of the test at testIndex (counted across options), else of the generator of option optionIndex (default 0); contextProperty picks one (SearchCenter of ActorsOfClass, GenerateAround of SimpleGrid, DistanceTo of Distance, Context of Trace), else the first is taken. A query with no generator yet, a target with no context property (the reply lists them) or a class that is not a context is refused.',
    use: 'A generator should search around the querier or a test should measure to a context.',
    avoid: 'Use add_eqs_generator to produce the item set.',
    props: { queryPath: A.queryPath, contextType: A.contextType, contextClass: A.contextClass, contextProperty: A.contextProperty, testIndex: A.testIndex, optionIndex: A.optionIndex },
    required: ['queryPath', 'contextType'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { queryPath: '/Game/AI/EQS_Cover', contextType: 'Querier' }, result: 'EQS context added',
  }),
  aiRecord({
    action: 'add_eqs_generator', summary: 'Add an option to an Environment Query with a generator of generatorType and save it: ActorsOfClass, CurrentLocation, Donut, OnCircle, PathingGrid, SimpleGrid or Composite, with the generatorSettings that generator reads (a setting it does not read, or an actorClass that is no Actor, is refused). A generator centers on the querier unless add_eqs_context points it elsewhere; optionIndex in the reply addresses the option.',
    use: 'An Environment Query needs its candidate item set.',
    avoid: 'Use add_eqs_test to score existing items.',
    props: {
      queryPath: A.queryPath, generatorType: A.generatorType,
      generatorSettings: A.generatorSettings,
    },
    required: ['queryPath', 'generatorType'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { queryPath: '/Game/AI/EQS_Cover', generatorType: 'ActorsOfClass' },
    result: 'EQS generator added',
  }),
  aiRecord({
    action: 'add_eqs_test', summary: 'Add a test to the first option of an Environment Query and save it: testType Distance, Dot, GameplayTags, Overlap, Pathfinding, PathfindingBatch, Project, Random or Trace, with engine defaults; a query with no option yet gets one. testIndex in the reply addresses it for configure_test_scoring and add_eqs_context.',
    use: 'Generated Environment Query items need scoring or filtering.',
    avoid: 'Use configure_test_scoring to tune a test that already exists.',
    props: { queryPath: A.queryPath, testType: A.testType },
    required: ['queryPath', 'testType'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { queryPath: '/Game/AI/EQS_Cover', testType: 'Distance' }, result: 'EQS test added',
  }),
  aiRecord({
    action: 'add_mass_spawner', summary: 'Place a Mass Spawner actor in the current level: when play begins it spawns spawnCount entities (default 100) of the Mass entity config configPath at the items of the Environment Query queryPath, run with the spawner as querier (a SimpleGrid generator around the querier spreads them out). location places it (default the world origin) and actorName labels it; a taken label, a config or query that does not load (NOT_FOUND) or is another asset type (TYPE_MISMATCH) is refused. The entities exist only in play and are drawn only when the config has a visualization trait.',
    use: 'A level should spawn a crowd of Mass entities from a config.',
    avoid: 'Use configure_mass_entity to edit the config itself.',
    props: {
      configPath: A.configPath, queryPath: str('Environment Query whose items are the spawn points; it runs around the spawner.'),
      spawnCount: A.spawnCount, actorName: str('Label for the placed spawner; a label already in the level is refused.'),
      location: xyz('World location of the spawner (default the world origin).'),
    },
    required: ['configPath', 'queryPath'], plugins: MASS_AI,
    out: { actorName: A.actorName },
    example: { configPath: '/Game/AI/Mass/MEC_Crowd', queryPath: '/Game/AI/EQS_SpawnGrid', spawnCount: 50, location: { x: 0, y: 0, z: 100 } },
    result: 'Mass spawner placed',
  }),
  aiRecord({
    action: 'add_node', summary: 'Add a node through the Behavior Tree graph route, in one undo step. It joins the tree once connect_nodes links it under a parent; on a tree with no root composite yet, link the graph\'s Root node (BehaviorTreeGraphNode_Root_0) to a composite to make it the root.',
    use: 'A Behavior Tree graph needs a node placed at explicit coordinates.',
    avoid: 'Use add_task_node or add_composite_node for the asset-level route.',
    props: {
      assetPath: A.assetPath, behaviorTreePath: A.behaviorTreePath, nodeType: A.nodeType,
      nodeId: A.nodeId, x: A.x, y: A.y,
    },
    requiredOneOf: ['assetPath', 'behaviorTreePath'], plugins: BT,
    out: { nodeId: A.nodeId },
    example: { assetPath: '/Game/AI/BT_Enemy', nodeType: 'Sequence', x: 0, y: 0 },
    result: 'Graph node added',
  }),
  aiRecord({
    action: 'add_service', summary: 'Add a service to the root composite of a Behavior Tree asset, in one undo step.',
    use: 'A Behavior Tree branch needs recurring background work.',
    avoid: 'Use add_decorator for a pass/fail condition.',
    props: { behaviorTreePath: A.behaviorTreePath, serviceType: A.serviceType },
    required: ['behaviorTreePath', 'serviceType'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', serviceType: 'DefaultFocus' },
    result: 'Service added',
  }),
  aiRecord({
    action: 'add_smart_object_component', summary: 'Add a Smart Object component named componentName (default SmartObjectComponent) to an actor Blueprint and save, using definitionPath when given (refused when it does not load). A component name the Blueprint already has is refused (ALREADY_EXISTS).',
    use: 'An actor Blueprint should expose a Smart Object definition.',
    avoid: 'Use create_smart_object_definition to author the definition asset.',
    props: {
      blueprintPath: A.blueprintPath,
      definitionPath: A.definitionPath, componentName: A.componentName,
    },
    required: ['blueprintPath'], plugins: SMART_OBJECTS,
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Bench' }, result: 'Smart Object component added',
  }),
  aiRecord({
    action: 'add_smart_object_slot', summary: 'Add a slot to a Smart Object definition at offset and rotation from its origin, enabled (default true), and save; slotIndex in the reply addresses it for configure_slot_behavior.',
    use: 'A Smart Object definition needs another interaction slot.',
    avoid: 'Use configure_slot_behavior to edit an existing slot.',
    props: {
      definitionPath: A.definitionPath,
      offset: A.offset, rotation: A.rotation, enabled: A.enabled,
    },
    required: ['definitionPath'], plugins: SMART_OBJECTS,
    out: { definitionPath: A.definitionPath },
    example: { definitionPath: '/Game/AI/SO_Bench', enabled: true }, result: 'Smart Object slot added',
  }),
  aiRecord({
    action: 'add_state_tree_state', summary: 'Add a state named stateName under parentStateName (default Root) of a State Tree, compiled and saved: stateType State (default), Group, Linked, LinkedAsset (UE 5.4+) or Subtree. States are found by name, so a name the tree already has is refused (NAME_TAKEN), as is an unknown stateType. A state made here has no tasks yet. compiled false with compileMessages when the tree does not compile.',
    use: 'A StateTree needs another state under a parent.',
    avoid: 'Use add_state_tree_transition to link existing states.',
    props: {
      stateTreePath: A.stateTreePath, stateName: A.stateName,
      parentStateName: A.parentStateName, stateType: A.stateType,
    },
    required: ['stateTreePath', 'stateName'], plugins: STATE_TREE,
    out: { assetPath: A.assetPath },
    example: { stateTreePath: '/Game/AI/ST_Enemy', stateName: 'Patrol' }, result: 'State added',
  }),
  aiRecord({
    action: 'add_state_tree_transition', summary: 'Add a go-to-state transition from fromState to toState, compiled and saved: triggerType OnStateCompleted (default), OnStateSucceeded, OnStateFailed, OnTick, OnEvent (needs an event tag) or OnDelegate (newer engines). An unknown trigger or a state that is not found is refused; transitionId identifies it.',
    use: 'Two StateTree states need a trigger-driven link.',
    avoid: 'Use add_state_tree_state to create the endpoints first.',
    props: {
      stateTreePath: A.stateTreePath, fromState: A.fromState,
      toState: A.toState, triggerType: A.triggerType,
    },
    required: ['stateTreePath', 'fromState', 'toState'], plugins: STATE_TREE,
    out: { assetPath: A.assetPath },
    example: { stateTreePath: '/Game/AI/ST_Enemy', fromState: 'Root', toState: 'Patrol' },
    result: 'Transition added',
  }),
  aiRecord({
    action: 'add_subnode', summary: 'Add a Behavior Tree decorator or service subnode.',
    use: 'A graph node needs a decorator or service attached by class.',
    avoid: 'Use add_decorator or add_service for the asset-level route.',
    // Its siblings take behaviorTreePath; this one took only assetPath, so a
    // caller moving between them paid a round trip per spelling.
    props: {
      assetPath: A.assetPath, behaviorTreePath: A.behaviorTreePath,
      parentNodeId: A.parentNodeId, subnodeType: A.subnodeType, nodeClass: A.nodeClass,
    },
    required: ['parentNodeId', 'subnodeType', 'nodeClass'],
    requiredOneOf: ['assetPath', 'behaviorTreePath'], plugins: BT,
    out: { nodeId: A.nodeId },
    example: {
      assetPath: '/Game/AI/BT_Enemy', parentNodeId: 'BTComposite_Selector_0',
      subnodeType: 'Decorator', nodeClass: '/Script/AIModule.BTDecorator_Blackboard',
    },
    result: 'Subnode added',
  }),
  aiRecord({
    action: 'add_task_node', summary: 'Add a task node to a Behavior Tree asset, in one undo step.',
    use: 'A Behavior Tree branch needs a leaf task. An enemy that chases the player is a Sequence with a MoveTo task whose Blackboard key (an Object key such as TargetActor) holds the player, run on its AI controller with run_behavior_tree.',
    avoid: 'Use add_composite_node for flow control.',
    props: { behaviorTreePath: A.behaviorTreePath, taskType: A.taskType, parentNodeId: A.parentNodeId },
    required: ['behaviorTreePath', 'taskType'], plugins: BT,
    out: { assetPath: A.assetPath, nodeId: A.nodeId },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', taskType: 'Wait' }, result: 'Task node added',
  }),
];
