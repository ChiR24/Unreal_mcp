/**
 * AI System Handlers (Phase 16)
 *
 * Complete AI implementation including:
 * - AI Controller (creation, behavior tree assignment, blackboard)
 * - Blackboard (asset creation, keys, instance sync)
 * - Behavior Tree (expanded creation, composite nodes, tasks, decorators, services)
 * - EQS (queries, generators, contexts, tests)
 * - Perception System (sight, hearing, damage, touch, teams)
 * - State Trees (UE5.3+ state machine alternative)
 * - Smart Objects (definitions, slots, behaviors)
 * - Mass AI (crowd simulation, entity configs, spawners)
 *
 * @module ai-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all AI-related actions for the manage_ai tool.
 */
export async function handleAITools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<Record<string, unknown>> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_ai',
      payload as HandlerArgs,
      `Automation bridge not available for AI action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // =========================================================================
    // 16.1 AI Controller (3 actions)
    // =========================================================================

    case 'create_ai_controller': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_ai_controller');
    }

    case 'assign_behavior_tree': {
      requireNonEmptyString(argsRecord.controllerPath, 'controllerPath', 'Missing required parameter: controllerPath');
      requireNonEmptyString(argsRecord.behaviorTreePath, 'behaviorTreePath', 'Missing required parameter: behaviorTreePath');
      return sendRequest('assign_behavior_tree');
    }

    case 'assign_blackboard': {
      requireNonEmptyString(argsRecord.controllerPath, 'controllerPath', 'Missing required parameter: controllerPath');
      requireNonEmptyString(argsRecord.blackboardPath, 'blackboardPath', 'Missing required parameter: blackboardPath');
      return sendRequest('assign_blackboard');
    }

    // =========================================================================
    // 16.2 Blackboard (3 actions)
    // =========================================================================

    case 'create_blackboard_asset': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_blackboard_asset');
    }

    case 'add_blackboard_key': {
      requireNonEmptyString(argsRecord.blackboardPath, 'blackboardPath', 'Missing required parameter: blackboardPath');
      requireNonEmptyString(argsRecord.keyName, 'keyName', 'Missing required parameter: keyName');
      requireNonEmptyString(argsRecord.keyType, 'keyType', 'Missing required parameter: keyType');
      return sendRequest('add_blackboard_key');
    }

    case 'set_key_instance_synced': {
      requireNonEmptyString(argsRecord.blackboardPath, 'blackboardPath', 'Missing required parameter: blackboardPath');
      requireNonEmptyString(argsRecord.keyName, 'keyName', 'Missing required parameter: keyName');
      return sendRequest('set_key_instance_synced');
    }

    // =========================================================================
    // 16.3 Behavior Tree - Expanded (6 actions)
    // =========================================================================

    case 'create_behavior_tree': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_behavior_tree');
    }

    case 'add_composite_node': {
      requireNonEmptyString(argsRecord.behaviorTreePath, 'behaviorTreePath', 'Missing required parameter: behaviorTreePath');
      requireNonEmptyString(argsRecord.compositeType, 'compositeType', 'Missing required parameter: compositeType');
      return sendRequest('add_composite_node');
    }

    case 'add_task_node': {
      requireNonEmptyString(argsRecord.behaviorTreePath, 'behaviorTreePath', 'Missing required parameter: behaviorTreePath');
      requireNonEmptyString(argsRecord.taskType, 'taskType', 'Missing required parameter: taskType');
      return sendRequest('add_task_node');
    }

    case 'add_decorator': {
      requireNonEmptyString(argsRecord.behaviorTreePath, 'behaviorTreePath', 'Missing required parameter: behaviorTreePath');
      requireNonEmptyString(argsRecord.decoratorType, 'decoratorType', 'Missing required parameter: decoratorType');
      return sendRequest('add_decorator');
    }

    case 'add_service': {
      requireNonEmptyString(argsRecord.behaviorTreePath, 'behaviorTreePath', 'Missing required parameter: behaviorTreePath');
      requireNonEmptyString(argsRecord.serviceType, 'serviceType', 'Missing required parameter: serviceType');
      return sendRequest('add_service');
    }

    case 'configure_bt_node': {
      requireNonEmptyString(argsRecord.behaviorTreePath, 'behaviorTreePath', 'Missing required parameter: behaviorTreePath');
      requireNonEmptyString(argsRecord.nodeId, 'nodeId', 'Missing required parameter: nodeId');
      return sendRequest('configure_bt_node');
    }

    // =========================================================================
    // 16.4 Environment Query System - EQS (5 actions)
    // =========================================================================

    case 'create_eqs_query': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_eqs_query');
    }

    case 'add_eqs_generator': {
      requireNonEmptyString(argsRecord.queryPath, 'queryPath', 'Missing required parameter: queryPath');
      requireNonEmptyString(argsRecord.generatorType, 'generatorType', 'Missing required parameter: generatorType');
      return sendRequest('add_eqs_generator');
    }

    case 'add_eqs_context': {
      requireNonEmptyString(argsRecord.queryPath, 'queryPath', 'Missing required parameter: queryPath');
      requireNonEmptyString(argsRecord.contextType, 'contextType', 'Missing required parameter: contextType');
      return sendRequest('add_eqs_context');
    }

    case 'add_eqs_test': {
      requireNonEmptyString(argsRecord.queryPath, 'queryPath', 'Missing required parameter: queryPath');
      requireNonEmptyString(argsRecord.testType, 'testType', 'Missing required parameter: testType');
      return sendRequest('add_eqs_test');
    }

    case 'configure_test_scoring': {
      requireNonEmptyString(argsRecord.queryPath, 'queryPath', 'Missing required parameter: queryPath');
      requireNonEmptyString(argsRecord.testIndex, 'testIndex', 'Missing required parameter: testIndex');
      return sendRequest('configure_test_scoring');
    }

    // =========================================================================
    // 16.5 Perception System (5 actions)
    // =========================================================================

    case 'add_ai_perception_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('add_ai_perception_component');
    }

    case 'configure_sight_config': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_sight_config');
    }

    case 'configure_hearing_config': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_hearing_config');
    }

    case 'configure_damage_sense_config': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('configure_damage_sense_config');
    }

    case 'set_perception_team': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('set_perception_team');
    }

    // =========================================================================
    // 16.6 State Trees - UE5.3+ (4 actions)
    // =========================================================================

    case 'create_state_tree': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_state_tree');
    }

    case 'add_state_tree_state': {
      requireNonEmptyString(argsRecord.stateTreePath, 'stateTreePath', 'Missing required parameter: stateTreePath');
      requireNonEmptyString(argsRecord.stateName, 'stateName', 'Missing required parameter: stateName');
      return sendRequest('add_state_tree_state');
    }

    case 'add_state_tree_transition': {
      requireNonEmptyString(argsRecord.stateTreePath, 'stateTreePath', 'Missing required parameter: stateTreePath');
      requireNonEmptyString(argsRecord.fromState, 'fromState', 'Missing required parameter: fromState');
      requireNonEmptyString(argsRecord.toState, 'toState', 'Missing required parameter: toState');
      return sendRequest('add_state_tree_transition');
    }

    case 'configure_state_tree_task': {
      requireNonEmptyString(argsRecord.stateTreePath, 'stateTreePath', 'Missing required parameter: stateTreePath');
      requireNonEmptyString(argsRecord.stateName, 'stateName', 'Missing required parameter: stateName');
      return sendRequest('configure_state_tree_task');
    }

    // =========================================================================
    // 16.7 Smart Objects (4 actions)
    // =========================================================================

    case 'create_smart_object_definition': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_smart_object_definition');
    }

    case 'add_smart_object_slot': {
      requireNonEmptyString(argsRecord.definitionPath, 'definitionPath', 'Missing required parameter: definitionPath');
      return sendRequest('add_smart_object_slot');
    }

    case 'configure_slot_behavior': {
      requireNonEmptyString(argsRecord.definitionPath, 'definitionPath', 'Missing required parameter: definitionPath');
      requireNonEmptyString(argsRecord.slotIndex, 'slotIndex', 'Missing required parameter: slotIndex');
      return sendRequest('configure_slot_behavior');
    }

    case 'add_smart_object_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('add_smart_object_component');
    }

    // =========================================================================
    // 16.8 Mass AI / Crowds (3 actions)
    // =========================================================================

    case 'create_mass_entity_config': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_mass_entity_config');
    }

    case 'configure_mass_entity': {
      requireNonEmptyString(argsRecord.configPath, 'configPath', 'Missing required parameter: configPath');
      return sendRequest('configure_mass_entity');
    }

    case 'add_mass_spawner': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      return sendRequest('add_mass_spawner');
    }

    // =========================================================================
    // Utility (1 action)
    // =========================================================================

    case 'get_ai_info': {
      // At least one path is required
      const hasPath = argsRecord.controllerPath || argsRecord.behaviorTreePath ||
                      argsRecord.blackboardPath || argsRecord.queryPath ||
                      argsRecord.stateTreePath || argsRecord.blueprintPath;
      if (!hasPath) {
        return cleanObject({
          success: false,
          error: 'MISSING_PARAMETER',
          message: 'At least one path parameter is required (controllerPath, behaviorTreePath, blackboardPath, queryPath, stateTreePath, or blueprintPath)'
        });
      }
      return sendRequest('get_ai_info');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown AI action: ${action}`
      });
  }
}
