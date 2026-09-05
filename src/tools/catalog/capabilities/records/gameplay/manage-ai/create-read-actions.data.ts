/**
 * manage_ai records, shard 3 of 4: the create_* actions and the read actions.
 *
 * create_nav_link_proxy deliberately does NOT require blueprintPath. The
 * action is a member of NAVIGATION_ACTIONS, so
 * orchestration/consolidated-handler-registration.ts:206 routes it to
 * handlers/navigation/navigation-handlers.ts BEFORE the manage_ai fallthrough;
 * the blueprintPath check in handlers/ai/ai-utility-actions.ts:37 is therefore
 * unreachable for manage_ai. The native side mirrors this: the AI dispatcher's
 * create_nav_link_proxy branch is a stale reference and the live handler is
 * McpNavigationHandlers::HandleCreateNavLinkProxy, which reads actorName /
 * location / startPoint / endPoint / direction instead.
 */
import type { CapabilityRecordSource, JsonObject } from '../../../index.js';
import { BT, EQS, MASS_AI, SMART_OBJECTS, STATE_TREE, aiRecord } from './builder.js';
import { NAV } from './properties-navigation.js';
import { AI } from './properties.js';

const A = AI;
const PROMOTED = 'Promoted from a hidden native manage_ai route after the gateway migration.';
const POST = 'post-migration' as const;
const N = NAV;

const reflObj: JsonObject = { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true };

/** Every create_* asset action shares the same name/path pair. */
const createProps = { action: A.action, name: A.name, path: A.path };

export const AI_CREATE_READ_RECORDS: readonly CapabilityRecordSource[] = [
  aiRecord({
    action: 'create', summary: 'Create a Behavior Tree through the graph compatibility route.',
    use: 'A caller uses the short create verb with savePath.',
    avoid: 'Use create_behavior_tree for the asset-level route.',
    props: { action: A.action, name: A.name, savePath: A.savePath }, required: ['name'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { name: 'BT_Enemy', savePath: '/Game/AI' }, result: 'Behavior Tree created',
  }),
  aiRecord({
    topics: ['ai controller', 'aicontroller', 'npc controller', 'enemy ai'],
    action: 'create_ai_controller', summary: 'Create an AIController Blueprint asset.',
    use: 'A pawn needs a dedicated AIController asset.',
    avoid: 'Use manage_blueprint for a general Blueprint.',
    props: createProps, required: ['name'],
    out: { assetPath: A.assetPath },
    example: { name: 'AIC_Enemy', path: '/Game/AI' }, result: 'AIController created',
  }),
  aiRecord({
    topics: ['behavior tree', 'bt asset', 'ai behavior', 'new behavior tree'],
    action: 'create_behavior_tree', summary: 'Create a Behavior Tree asset.',
    use: 'An AI needs a new Behavior Tree asset.',
    avoid: 'Use create for the graph compatibility route.',
    props: createProps, required: ['name'], plugins: BT,
    out: { behaviorTreePath: A.behaviorTreePath },
    example: { name: 'BT_Enemy', path: '/Game/AI' }, result: 'Behavior Tree created',
  }),
  aiRecord({
    action: 'create_blackboard', summary: 'Create a Blackboard asset through the compatibility route.',
    use: 'A caller uses the short create_blackboard verb.',
    avoid: 'Use create_blackboard_asset for the canonical route.',
    props: createProps, required: ['name'], plugins: BT,
    out: {
      blackboardPath: A.blackboardPath,
      alreadyExisted: { type: 'boolean', description: 'Whether the Blackboard already existed and was reused.' },
    },
    example: { name: 'BB_Enemy', path: '/Game/AI' }, result: 'Blackboard created',
  }),
  aiRecord({
    action: 'create_blackboard_asset', summary: 'Create a Blackboard data asset.',
    use: 'A Behavior Tree needs a backing Blackboard asset.',
    avoid: 'Use add_blackboard_key to populate it.',
    props: createProps, required: ['name'], plugins: BT,
    out: { blackboardPath: A.blackboardPath },
    example: { name: 'BB_Enemy', path: '/Game/AI' }, result: 'Blackboard created',
  }),
  aiRecord({
    action: 'create_eqs_query', summary: 'Create an Environment Query asset.',
    use: 'An AI needs a spatial query asset.',
    avoid: 'Use add_eqs_generator to give the query an item set.',
    props: createProps, required: ['name'], plugins: EQS,
    out: { assetPath: A.assetPath },
    example: { name: 'EQS_Cover', path: '/Game/AI' }, result: 'Environment Query created',
  }),
  aiRecord({
    action: 'create_mass_entity_config', summary: 'Create a Mass Entity configuration asset.',
    use: 'Crowd simulation needs an entity config asset.',
    avoid: 'Use configure_mass_entity to edit an existing config.',
    props: createProps, required: ['name'], plugins: MASS_AI,
    out: { configPath: A.configPath },
    example: { name: 'MEC_Crowd', path: '/Game/AI/Mass' }, result: 'Mass entity config created',
  }),
  aiRecord({
    action: 'create_nav_link_proxy', summary: 'Create a NavLinkProxy actor in the editor world.',
    use: 'A level needs an authored navigation link between two points.',
    avoid: 'Use create_smart_link for a state-switching link.',
    props: {
      action: A.action, actorName: A.actorName, location: N.location, rotation: A.rotation,
      startPoint: N.startPoint, endPoint: N.endPoint, direction: N.direction,
    },
    out: { actorName: A.actorName },
    example: { actorName: 'NavLinkProxy_0', location: { x: 0, y: 0, z: 100 } },
    result: 'NavLinkProxy created',
  }),
  aiRecord({
    action: 'create_nav_modifier_component', summary: 'Add a navigation modifier component to a Blueprint asset.',
    use: 'A Blueprint should carve or alter navigation around itself.',
    avoid: 'Use set_nav_area_class to change a placed actor area.',
    props: {
      action: A.action, blueprintPath: A.blueprintPath, componentName: A.componentName,
      areaClass: N.areaClass, failsafeExtent: N.failsafeExtent, save: A.save,
    },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Blocker', componentName: 'NavModifier' },
    result: 'Nav modifier component added',
  }),
  aiRecord({
    action: 'create_smart_link', summary: 'Create a smart navigation link.',
    use: 'A level needs a navigation link that can toggle its area class.',
    avoid: 'Use create_nav_link_proxy for a static link.',
    props: {
      action: A.action, actorName: A.actorName, location: N.location, rotation: A.rotation,
      startPoint: N.startPoint, endPoint: N.endPoint, direction: N.direction,
    },
    out: { actorName: A.actorName },
    example: { actorName: 'SmartLink_0', location: { x: 0, y: 200, z: 100 } },
    result: 'Smart link created',
  }),
  aiRecord({
    action: 'create_smart_object_definition', summary: 'Create a Smart Object definition.',
    use: 'An interactable needs a Smart Object definition asset.',
    avoid: 'Use add_smart_object_slot to add interaction slots.',
    props: createProps, required: ['name'], plugins: SMART_OBJECTS,
    out: { definitionPath: A.definitionPath },
    example: { name: 'SO_Bench', path: '/Game/AI' }, result: 'Smart Object definition created',
  }),
  aiRecord({
    action: 'create_state_tree', summary: 'Create a StateTree asset.',
    use: 'An AI needs a StateTree instead of a Behavior Tree.',
    avoid: 'Use create_behavior_tree for Behavior Tree logic.',
    props: createProps, required: ['name'], plugins: STATE_TREE,
    out: { assetPath: A.assetPath },
    example: { name: 'ST_Enemy', path: '/Game/AI' }, result: 'StateTree created',
  }),
  aiRecord({
    action: 'get_ai_info', summary: 'Read AI asset information.',
    use: 'A caller needs the current state of an AI asset.',
    avoid: 'Use get_navigation_info for navigation state.',
    props: { action: A.action, controllerPath: A.controllerPath, behaviorTreePath: A.behaviorTreePath, blackboardPath: A.blackboardPath, queryPath: A.queryPath, stateTreePath: A.stateTreePath, blueprintPath: A.blueprintPath },
    effect: 'read',
    out: {
      aiInfo: {
        type: 'object',
        description: 'AI asset state (HandleGetAIInfo).',
        properties: {
          keyCount: { type: 'number', description: 'Blackboard key count.' },
          rootDecoratorClasses: { type: 'array', items: { type: 'string' }, description: 'Class paths of decorators attached to the root node.' },
          rootDecorators: { type: 'array', items: reflObj, description: 'Decorator objects attached to the root node.' },
          childDecorators: { type: 'array', items: reflObj, description: 'Child decorators.' },
          services: { type: 'array', items: reflObj, description: 'Services.' },
          blackboardKeys: { type: 'array', items: reflObj, description: 'Blackboard keys.' },
          parentBlackboard: { type: 'string', description: 'Parent blackboard asset path.' },
          rootGraphBlackboard: { type: 'string', description: 'Blackboard assigned on the root graph node.' },
          rootGraphBlackboardMatchesAssigned: { type: 'boolean', description: 'Whether the root graph blackboard matches the assigned blackboard.' },
          controllerClass: { type: 'string', description: 'AIController class path.' },
          assignedBehaviorTree: { type: 'string', description: 'Behavior Tree asset path assigned to the controller.' },
          assignedBlackboard: { type: 'string', description: 'Blackboard asset path assigned to the tree/controller.' },
          hasRootNode: { type: 'boolean', description: 'Whether the Behavior Tree has a root node.' },
          rootDecoratorCount: { type: 'number', description: 'Number of root decorators.' },
          btNodeCount: { type: 'number', description: 'Number of Behavior Tree nodes.' },
          queryName: { type: 'string', description: 'Environment Query asset name.' },
          queryPath: { type: 'string', description: 'Environment Query asset object path.' },
          optionCount: { type: 'number', description: 'Number of query options (generator + tests).' },
          testCount: { type: 'number', description: 'Total number of tests across all options.' },
          options: { type: 'array', items: reflObj, description: 'Query options: generator class, item type and their tests.' },
        },
        additionalProperties: false,
      },
    },
    example: { controllerPath: '/Game/AI/AIC_Enemy' }, result: 'AI info read',
  }),
  aiRecord({
    action: 'get_blackboard_value', summary: 'Read a Blackboard key value.',
    use: 'A caller needs the authored default of a Blackboard key.',
    avoid: 'Use set_blackboard_value to change it.',
    props: { action: A.action, blackboardPath: A.blackboardPath, keyName: A.keyName },
    required: ['blackboardPath', 'keyName'], effect: 'read', plugins: BT,
    out: {
      valueAvailable: { type: 'boolean', description: 'Whether the typed default value was available to read (false on UE 5.0-5.4).' },
      value: A.value,
    },
    example: { blackboardPath: '/Game/AI/BB_Enemy', keyName: 'TargetActor' },
    result: 'Blackboard value read',
  }),
  aiRecord({
    action: 'get_navigation_info', summary: 'Read navigation state and settings.',
    use: 'A caller needs current NavMesh and agent settings.',
    avoid: 'Use get_ai_info for AI asset state.',
    props: { action: A.action }, effect: 'read',
    out: {
      navMeshInfo: {
        type: 'object',
        description: 'Navigation mesh and agent settings reported by the native Navigation domain (HandleGetNavigationInfo).',
        properties: {
          agentRadius: N.agentRadius, agentHeight: N.agentHeight, agentMaxSlope: N.agentMaxSlope,
          tileSizeUU: N.tileSizeUU, cellSize: N.cellSize, cellHeight: N.cellHeight,
          agentStepHeight: N.agentStepHeight,
          navLinkCount: { type: 'number', description: 'Number of navigation links.' },
          boundsVolumes: { type: 'number', description: 'Number of navigation bounds volumes.' },
          isNavigationBuildInProgress: { type: 'boolean', description: 'Whether a navigation build is running.' },
        },
        additionalProperties: false,
      },
    },
    example: {}, result: 'Navigation info read',
  }),
  aiRecord({
    action: 'get_tree', summary: 'Read a Behavior Tree graph.',
    use: 'A caller needs the node graph of a Behavior Tree.',
    avoid: 'Use get_ai_info for asset-level metadata.',
    props: { action: A.action, assetPath: A.assetPath },
    required: ['assetPath'], effect: 'read', plugins: BT,
    out: {
      assetPath: A.assetPath,
      tree: {
        type: 'object',
        description: 'Serialized Behavior Tree graph (root node, children, decorators, services) reported by the native BehaviorTree domain (HandleGetTree).',
        additionalProperties: true,
        'x-unreal-reflection-boundary': true,
      },
    },
      example: { assetPath: '/Game/AI/BT_Enemy' }, result: 'Behavior Tree read',
    }),
    aiRecord({
      action: 'create_nav_modifier',
      summary: 'Add a NavModifier component to a Blueprint so the actor stamps a navigation area.',
      use: 'An actor must mark the navmesh under it as a different area class.',
      avoid: 'The navmesh area is a level volume rather than an actor; use create_nav_modifier_volume.',
      props: {
        action: A.action, blueprintPath: A.blueprintPath, componentName: A.componentName,
        areaClass: N.areaClass, failsafeToDefaultNavmesh: A.failsafeToDefaultNavmesh,
      },
      required: ['blueprintPath'],
      out: { blueprintPath: A.blueprintPath, componentName: A.componentName, areaClass: N.areaClass },
      example: { blueprintPath: '/Game/AI/BP_Obstacle', componentName: 'NavModifier', areaClass: 'NavArea_Obstacle' },
      result: 'Nav modifier component added',
      provenance: POST, rationale: PROMOTED,
    }),
  ];
