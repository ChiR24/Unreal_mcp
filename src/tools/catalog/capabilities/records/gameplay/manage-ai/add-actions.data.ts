/**
 * manage_ai records, shard 1 of 4: the 16 `add_*` actions.
 *
 * Each record declares the EXACT properties its handler reads -- never the
 * parent-wide union. Required sets come from the TS validators
 * (handlers/ai/ai-handlers.ts, ai-utility-actions.ts) and the native manual
 * IsEmpty()/INVALID_ARGUMENT checks; the native JSON accessors
 * (GetStringFieldAI/GetNumberFieldAI/GetBoolFieldAI) always default and never
 * early-return, so optionality is decided by those explicit checks alone.
 *
 * Order is the canonical manage_ai action sequence (shards 1-4 concatenate to
 * it), which the generator turns into the parent action enum verbatim.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { BT, EQS, MASS_AI, SMART_OBJECTS, STATE_TREE, aiRecord } from './builder.js';
import { AI } from './properties.js';

const A = AI;

export const AI_ADD_RECORDS: readonly CapabilityRecordSource[] = [
  aiRecord({
    action: 'add_ai_perception_component', summary: 'Add an AI perception component to a Blueprint asset.',
    use: 'A Blueprint needs an AIPerception component before senses are configured.',
    avoid: 'Use setup_perception to add and configure senses in one call.',
    props: { action: A.action, blueprintPath: A.blueprintPath }, required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy' }, result: 'Perception component added',
  }),
  aiRecord({
    action: 'add_blackboard_key', summary: 'Add a typed key to a Blackboard asset.',
    use: 'A Blackboard needs a new typed entry.',
    avoid: 'Use set_key_instance_synced to change sync on an existing key.',
    props: {
      action: A.action, blackboardPath: A.blackboardPath, keyName: A.keyName, keyType: A.keyType,
      baseObjectClass: A.baseObjectClass, isInstanceSynced: A.isInstanceSynced,
    },
    required: ['blackboardPath', 'keyName', 'keyType'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { blackboardPath: '/Game/AI/BB_Enemy', keyName: 'TargetActor', keyType: 'Object' },
    result: 'Blackboard key added',
  }),
  aiRecord({
    action: 'add_composite_node', summary: 'Add a composite node to a Behavior Tree asset.',
    use: 'A Behavior Tree needs a Selector or Sequence branch.',
    avoid: 'Use add_task_node for leaf behavior.',
    // `nodeName` names the created node, and the response now returns the
    // resolved name plus isRoot. Without them a composite was created unnamed
    // and unaddressable, so add_decorator/add_service/add_task_node had no
    // handle to attach to and a tree could be created but never assembled.
    props: {
      action: A.action, behaviorTreePath: A.behaviorTreePath,
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
    action: 'add_decorator', summary: 'Add a decorator to a Behavior Tree asset.',
    use: 'A Behavior Tree branch needs a conditional gate.',
    avoid: 'Use add_service for recurring background ticks.',
    props: { action: A.action, behaviorTreePath: A.behaviorTreePath, decoratorType: A.decoratorType },
    required: ['behaviorTreePath', 'decoratorType'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', decoratorType: 'Blackboard' },
    result: 'Decorator added',
  }),
  aiRecord({
    action: 'add_eqs_context', summary: 'Add a context to an Environment Query.',
    use: 'An Environment Query needs a querier or item context.',
    avoid: 'Use add_eqs_generator to produce the item set.',
    props: { action: A.action, queryPath: A.queryPath, contextType: A.contextType },
    required: ['queryPath', 'contextType'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { queryPath: '/Game/AI/EQS_Cover', contextType: 'Querier' }, result: 'EQS context added',
  }),
  aiRecord({
    action: 'add_eqs_generator', summary: 'Add a generator to an Environment Query.',
    use: 'An Environment Query needs its candidate item set.',
    avoid: 'Use add_eqs_test to score existing items.',
    props: {
      action: A.action, queryPath: A.queryPath, generatorType: A.generatorType,
      generatorSettings: A.generatorSettings,
    },
    required: ['queryPath', 'generatorType'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { queryPath: '/Game/AI/EQS_Cover', generatorType: 'ActorsOfClass' },
    result: 'EQS generator added',
  }),
  aiRecord({
    action: 'add_eqs_test', summary: 'Add a test to an Environment Query.',
    use: 'Generated Environment Query items need scoring or filtering.',
    avoid: 'Use configure_test_scoring to tune a test that already exists.',
    props: { action: A.action, queryPath: A.queryPath, testType: A.testType },
    required: ['queryPath', 'testType'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { queryPath: '/Game/AI/EQS_Cover', testType: 'Distance' }, result: 'EQS test added',
  }),
  aiRecord({
    action: 'add_mass_spawner', summary: 'Add Mass spawner reference variables to a Blueprint asset (no AMassSpawner actor is placed).',
    use: 'A Blueprint should spawn Mass entities from a config.',
    avoid: 'Use configure_mass_entity to edit the config itself.',
    props: {
      action: A.action, blueprintPath: A.blueprintPath, configPath: A.configPath,
      componentName: A.componentName, spawnCount: A.spawnCount,
    },
    required: ['blueprintPath'], plugins: MASS_AI,
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Crowd', spawnCount: 100 }, result: 'Mass spawner added',
  }),
  aiRecord({
    action: 'add_node', summary: 'Add a node through the Behavior Tree graph route.',
    use: 'A Behavior Tree graph needs a node placed at explicit coordinates.',
    avoid: 'Use add_task_node or add_composite_node for the asset-level route.',
    props: {
      action: A.action, assetPath: A.assetPath, nodeType: A.nodeType,
      nodeId: A.nodeId, x: A.x, y: A.y,
    },
    required: ['assetPath'], plugins: BT,
    out: { nodeId: A.nodeId },
    example: { assetPath: '/Game/AI/BT_Enemy', nodeType: 'Sequence', x: 0, y: 0 },
    result: 'Graph node added',
  }),
  aiRecord({
    action: 'add_service', summary: 'Add a service to a Behavior Tree asset.',
    use: 'A Behavior Tree branch needs recurring background work.',
    avoid: 'Use add_decorator for a pass/fail condition.',
    props: { action: A.action, behaviorTreePath: A.behaviorTreePath, serviceType: A.serviceType },
    required: ['behaviorTreePath', 'serviceType'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', serviceType: 'DefaultFocus' },
    result: 'Service added',
  }),
  aiRecord({
    action: 'add_smart_object_component', summary: 'Add a Smart Object component to a Blueprint asset.',
    use: 'An actor Blueprint should expose a Smart Object definition.',
    avoid: 'Use create_smart_object_definition to author the definition asset.',
    props: {
      action: A.action, blueprintPath: A.blueprintPath,
      definitionPath: A.definitionPath, componentName: A.componentName,
    },
    required: ['blueprintPath'], plugins: SMART_OBJECTS,
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Bench' }, result: 'Smart Object component added',
  }),
  aiRecord({
    action: 'add_smart_object_slot', summary: 'Add a slot to a Smart Object definition.',
    use: 'A Smart Object definition needs another interaction slot.',
    avoid: 'Use configure_slot_behavior to edit an existing slot.',
    props: {
      action: A.action, definitionPath: A.definitionPath,
      offset: A.offset, rotation: A.rotation, enabled: A.enabled,
    },
    required: ['definitionPath'], plugins: SMART_OBJECTS,
    out: { definitionPath: A.definitionPath },
    example: { definitionPath: '/Game/AI/SO_Bench', enabled: true }, result: 'Smart Object slot added',
  }),
  aiRecord({
    action: 'add_state_tree_state', summary: 'Add a state to a StateTree asset.',
    use: 'A StateTree needs another state under a parent.',
    avoid: 'Use add_state_tree_transition to link existing states.',
    props: {
      action: A.action, stateTreePath: A.stateTreePath, stateName: A.stateName,
      parentStateName: A.parentStateName, stateType: A.stateType,
    },
    required: ['stateTreePath', 'stateName'], plugins: STATE_TREE,
    out: { assetPath: A.assetPath },
    example: { stateTreePath: '/Game/AI/ST_Enemy', stateName: 'Patrol' }, result: 'State added',
  }),
  aiRecord({
    action: 'add_state_tree_transition', summary: 'Add a transition to a StateTree asset.',
    use: 'Two StateTree states need a trigger-driven link.',
    avoid: 'Use add_state_tree_state to create the endpoints first.',
    props: {
      action: A.action, stateTreePath: A.stateTreePath, fromState: A.fromState,
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
    props: {
      action: A.action, assetPath: A.assetPath, parentNodeId: A.parentNodeId,
      subnodeType: A.subnodeType, nodeClass: A.nodeClass,
    },
    required: ['assetPath', 'parentNodeId', 'subnodeType', 'nodeClass'], plugins: BT,
    out: { nodeId: A.nodeId },
    example: {
      assetPath: '/Game/AI/BT_Enemy', parentNodeId: 'Node_0',
      subnodeType: 'Decorator', nodeClass: '/Script/AIModule.BTDecorator_Blackboard',
    },
    result: 'Subnode added',
  }),
  aiRecord({
    action: 'add_task_node', summary: 'Add a task node to a Behavior Tree asset.',
    use: 'A Behavior Tree branch needs a leaf task.',
    avoid: 'Use add_composite_node for flow control.',
    props: { action: A.action, behaviorTreePath: A.behaviorTreePath, taskType: A.taskType, parentNodeId: A.parentNodeId },
    required: ['behaviorTreePath', 'taskType'], plugins: BT,
    out: { assetPath: A.assetPath, nodeId: A.nodeId },
    example: { behaviorTreePath: '/Game/AI/BT_Enemy', taskType: 'Wait' }, result: 'Task node added',
  }),
];
