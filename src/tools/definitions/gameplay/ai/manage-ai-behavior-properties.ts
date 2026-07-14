import { commonSchemas } from '../../../catalog/tool-definition-utils.js';
import { BEHAVIOR_TREE_ACTIONS, NAVIGATION_ACTIONS } from '../../shared/action-sets.js';

export const manageAiBehaviorProperties = {
action: {
          type: 'string',
          enum: [
            'create_ai_controller', 'assign_behavior_tree', 'assign_blackboard',
            'create_blackboard_asset', 'add_blackboard_key', 'set_key_instance_synced',
            'create_behavior_tree', 'add_composite_node', 'add_task_node', 'add_decorator', 'add_service', 'configure_bt_node',
            'create_eqs_query', 'add_eqs_generator', 'add_eqs_context', 'add_eqs_test', 'configure_test_scoring',
            'add_ai_perception_component', 'configure_sight_config', 'configure_hearing_config', 'configure_damage_sense_config', 'set_perception_team',
            'create_state_tree', 'add_state_tree_state', 'add_state_tree_transition', 'configure_state_tree_task',
            'create_smart_object_definition', 'add_smart_object_slot', 'configure_slot_behavior', 'add_smart_object_component',
            'create_mass_entity_config', 'configure_mass_entity', 'add_mass_spawner',
            'get_ai_info',
            'create_blackboard', 'setup_perception',
            'set_focus', 'clear_focus',
            'set_blackboard_value', 'get_blackboard_value',
            'run_behavior_tree', 'stop_behavior_tree',
            // Phase 42: NPC Plugins
            'create_npc_dialogue_tree', 'add_dialogue_node', 'link_dialogue_nodes',
            'set_dialogue_speaker', 'set_dialogue_condition', 'trigger_dialogue',
            'setup_patrol_mode', 'setup_alert_mode', 'setup_combat_mode', 'setup_idle_mode',
            'configure_mode_transitions', 'add_patrol_waypoint',
            'create_npc_spawner', 'configure_spawn_limits', 'set_spawn_conditions',
            'create_npc_group', 'configure_group_tactics', 'get_npc_state',
            'create_npc_memory', 'add_memory_record', 'query_npc_memory',
            'set_npc_personality', 'configure_reputation_system', 'get_npc_info'
          ,
            ...BEHAVIOR_TREE_ACTIONS, ...NAVIGATION_ACTIONS],
          description: 'AI action to perform'
        },
name: commonSchemas.name,
path: commonSchemas.directoryPathForCreation,
blueprintPath: commonSchemas.blueprintPath,
controllerPath: commonSchemas.controllerPath,
behaviorTreePath: commonSchemas.behaviorTreePath,
blackboardPath: commonSchemas.blackboardPath,
keyName: commonSchemas.keyName,
keyType: {
          type: 'string',
          enum: ['Bool', 'Int', 'Float', 'Vector', 'Rotator', 'Object', 'Class', 'Enum', 'Name', 'String'],
          description: 'Blackboard key data type.'
        },
baseObjectClass: commonSchemas.stringProp,
isInstanceSynced: { type: 'boolean', description: 'Sync key across instances.' },
compositeType: {
          type: 'string',
          enum: ['Selector', 'Sequence', 'Parallel', 'SimpleParallel'],
          description: 'Composite node type.'
        },
taskType: {
          type: 'string',
          enum: [
            'MoveTo', 'MoveDirectlyToward', 'RotateToFaceBBEntry', 'Wait', 'WaitBlackboardTime',
            'PlayAnimation', 'PlaySound', 'RunEQSQuery', 'RunBehaviorDynamic', 'SetBlackboardValue',
            'PushPawnAction', 'FinishWithResult', 'MakeNoise', 'GameplayTaskBase', 'Custom'
          ],
          description: 'Task node type.'
        },
decoratorType: {
          type: 'string',
          enum: [
            'Blackboard', 'BlackboardBased', 'CompareBBEntries', 'Cooldown', 'ConeCheck',
            'DoesPathExist', 'IsAtLocation', 'IsBBEntryOfClass', 'KeepInCone', 'Loop',
            'SetTagCooldown', 'TagCooldown', 'TimeLimit', 'ForceSuccess', 'ConditionalLoop', 'Custom'
          ],
          description: 'Decorator node type.'
        },
serviceType: {
          type: 'string',
          enum: ['DefaultFocus', 'RunEQS', 'Custom'],
          description: 'Service node type.'
        },
subnodeType: {
          type: 'string',
          enum: ['Decorator', 'Service'],
          description: 'Behavior Tree subnode kind for add_subnode.'
        },
nodeClass: commonSchemas.nodeClass,
parentNodeId: commonSchemas.nodeId,
nodeId: commonSchemas.nodeId,
queryPath: commonSchemas.queryPath,
generatorType: {
          type: 'string',
          enum: ['ActorsOfClass', 'CurrentLocation', 'Donut', 'OnCircle', 'PathingGrid', 'SimpleGrid', 'Composite', 'Custom'],
          description: 'EQS generator type.'
        },
contextType: {
          type: 'string',
          enum: ['Querier', 'Item', 'EnvQueryContext_BlueprintBase', 'Custom'],
          description: 'EQS context type.'
        },
testType: {
          type: 'string',
          enum: ['Distance', 'Dot', 'GameplayTags', 'Overlap', 'Pathfinding', 'PathfindingBatch', 'Project', 'Random', 'Trace', 'Custom'],
          description: 'EQS test type.'
        },
generatorSettings: {
          type: 'object',
          properties: {
            searchRadius: commonSchemas.numberProp,
            searchCenter: commonSchemas.stringProp,
            actorClass: commonSchemas.stringProp,
            gridSize: commonSchemas.numberProp,
            spacesBetween: commonSchemas.numberProp,
            innerRadius: commonSchemas.numberProp,
            outerRadius: commonSchemas.numberProp
          },
          description: 'Generator-specific settings.'
        },
testSettings: {
          type: 'object',
          properties: {
            scoringEquation: { type: 'string', enum: ['Linear', 'Square', 'InverseLinear', 'Constant'] },
            clampMin: commonSchemas.numberProp,
            clampMax: commonSchemas.numberProp,
            filterType: { type: 'string', enum: ['Minimum', 'Maximum', 'Range'] },
            floatMin: commonSchemas.numberProp,
            floatMax: commonSchemas.numberProp
          },
          description: 'Test scoring and filter settings.'
        },
testIndex: { type: 'number', description: 'Index of test to configure.' },
sightConfig: {
          type: 'object',
          properties: {
            sightRadius: commonSchemas.numberProp,
            loseSightRadius: commonSchemas.numberProp,
            peripheralVisionAngle: commonSchemas.numberProp,
            pointOfViewBackwardOffset: commonSchemas.numberProp,
            nearClippingRadius: commonSchemas.numberProp,
            autoSuccessRange: commonSchemas.numberProp,
            maxAge: commonSchemas.numberProp,
            detectionByAffiliation: {
              type: 'object',
              properties: {
                enemies: commonSchemas.booleanProp,
                neutrals: commonSchemas.booleanProp,
                friendlies: commonSchemas.booleanProp
              }
            }
          },
          description: 'AI sight sense configuration.'
        },
hearingConfig: {
          type: 'object',
          properties: {
            hearingRange: commonSchemas.numberProp,
            loSHearingRange: commonSchemas.numberProp,
            detectFriendly: commonSchemas.booleanProp,
            maxAge: commonSchemas.numberProp
          },
          description: 'AI hearing sense configuration.'
        },
damageConfig: {
          type: 'object',
          properties: {
            maxAge: commonSchemas.numberProp
          },
          description: 'AI damage sense configuration.'
        },
teamId: { type: 'number', description: 'Team ID for perception affiliation (0=Neutral, 1=Player, 2=Enemy, etc.).' },
dominantSense: {
          type: 'string',
          enum: ['Sight', 'Hearing', 'Damage', 'Touch', 'None'],
          description: 'Dominant sense for perception prioritization.'
        },
stateTreePath: commonSchemas.stateTreePath,
stateName: commonSchemas.stateName,
fromState: commonSchemas.fromState,
toState: commonSchemas.toState,
definitionPath: commonSchemas.definitionPath,
slotIndex: { type: 'number', description: 'Index of slot to configure.' },
configPath: commonSchemas.configPath,
actorName: commonSchemas.actorName
};
