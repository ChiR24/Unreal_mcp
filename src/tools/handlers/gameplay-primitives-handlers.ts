/**
 * Gameplay Primitives Handlers
 * 
 * Universal gameplay building blocks: state machines, values, factions, zones, 
 * conditions, spawners, schedules, interactions, and attachments.
 * 
 * 62 actions across 10 systems with multiplayer replication support.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';
import type { HandlerResult } from '../../types/handler-types.js';

// Use Record<string, unknown> pattern (matches existing handlers)
type GameplayPrimitivesArgs = Record<string, unknown>;

/**
 * Handle all gameplay primitives actions
 */
export async function handleGameplayPrimitivesTools(
  action: string,
  args: GameplayPrimitivesArgs,
  tools: ITools
): Promise<HandlerResult> {
  
  switch (action) {
    // ==================== VALUE TRACKER (8 actions) ====================
    case 'create_value_tracker': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.trackerKey, 'trackerKey');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to create value tracker'
      )) as HandlerResult;
    }

    case 'modify_value': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.trackerKey, 'trackerKey');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to modify value'
      )) as HandlerResult;
    }

    case 'set_value': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.trackerKey, 'trackerKey');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to set value'
      )) as HandlerResult;
    }

    case 'get_value': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.trackerKey, 'trackerKey');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to get value'
      )) as HandlerResult;
    }

    case 'add_value_threshold':
    case 'configure_value_decay':
    case 'configure_value_regen':
    case 'pause_value_changes': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.trackerKey, 'trackerKey');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    // ==================== STATE MACHINE (6 actions) ====================
    case 'create_actor_state_machine': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to create state machine'
      )) as HandlerResult;
    }

    case 'add_actor_state': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.stateName, 'stateName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to add state'
      )) as HandlerResult;
    }

    case 'add_actor_state_transition': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.fromState, 'fromState');
      requireNonEmptyString(args.toState, 'toState');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to add state transition'
      )) as HandlerResult;
    }

    case 'set_actor_state': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.stateName, 'stateName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to set actor state'
      )) as HandlerResult;
    }

    case 'get_actor_state': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to get actor state'
      )) as HandlerResult;
    }

    case 'configure_state_timer': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.stateName, 'stateName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to configure state timer'
      )) as HandlerResult;
    }

    // ==================== FACTION/REPUTATION (8 actions) ====================
    case 'create_faction': {
      requireNonEmptyString(args.factionId, 'factionId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to create faction'
      )) as HandlerResult;
    }

    case 'set_faction_relationship': {
      requireNonEmptyString(args.factionA, 'factionA');
      requireNonEmptyString(args.factionB, 'factionB');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to set faction relationship'
      )) as HandlerResult;
    }

    case 'assign_to_faction': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.factionId, 'factionId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to assign to faction'
      )) as HandlerResult;
    }

    case 'get_faction': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to get faction'
      )) as HandlerResult;
    }

    case 'modify_reputation':
    case 'get_reputation': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.factionId, 'factionId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    case 'add_reputation_threshold': {
      requireNonEmptyString(args.factionId, 'factionId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to add reputation threshold'
      )) as HandlerResult;
    }

    case 'check_faction_relationship': {
      requireNonEmptyString(args.actorA, 'actorA');
      requireNonEmptyString(args.actorB, 'actorB');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to check faction relationship'
      )) as HandlerResult;
    }

    // ==================== ATTACHMENT (6 actions) ====================
    case 'attach_to_socket': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.parentActorName, 'parentActorName');
      // socketName is optional - empty string attaches to root
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to attach to socket'
      )) as HandlerResult;
    }

    case 'detach_from_parent': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to detach from parent'
      )) as HandlerResult;
    }

    case 'transfer_control': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to transfer control'
      )) as HandlerResult;
    }

    case 'configure_attachment_rules':
    case 'get_attached_actors':
    case 'get_attachment_parent': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    // ==================== SCHEDULE (5 actions) ====================
    case 'create_schedule': {
      requireNonEmptyString(args.actorName, 'actorName');
      requireNonEmptyString(args.scheduleId, 'scheduleId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to create schedule'
      )) as HandlerResult;
    }

    case 'add_schedule_entry':
    case 'set_schedule_active':
    case 'get_current_schedule_entry':
    case 'skip_to_schedule_entry': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    // ==================== WORLD TIME (7 actions) ====================
    case 'create_world_time':
    case 'set_world_time':
    case 'get_world_time':
    case 'set_time_scale':
    case 'pause_world_time':
    case 'add_time_event':
    case 'get_time_period': {
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    // ==================== ZONE (6 actions) ====================
    case 'create_zone': {
      requireNonEmptyString(args.zoneId, 'zoneId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to create zone'
      )) as HandlerResult;
    }

    case 'set_zone_property':
    case 'get_zone_property': {
      requireNonEmptyString(args.zoneId, 'zoneId');
      requireNonEmptyString(args.propertyKey, 'propertyKey');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    case 'get_actor_zone': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        'Failed to get actor zone'
      )) as HandlerResult;
    }

    case 'add_zone_enter_event':
    case 'add_zone_exit_event': {
      requireNonEmptyString(args.zoneId, 'zoneId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    // ==================== CONDITION (4 actions) ====================
    case 'create_condition':
    case 'create_compound_condition': {
      requireNonEmptyString(args.conditionId, 'conditionId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    case 'evaluate_condition':
    case 'add_condition_listener': {
      requireNonEmptyString(args.conditionId, 'conditionId');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    // ==================== INTERACTION (6 actions) ====================
    case 'add_interactable_component':
    case 'configure_interaction':
    case 'set_interaction_enabled': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    case 'get_nearby_interactables': {
      // location and radius based - no actorName required
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    case 'focus_interaction':
    case 'execute_interaction': {
      requireNonEmptyString(args.actorName, 'actorName');
      // focusingActorName/interactingActorName are optional in C++
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    // ==================== SPAWNER (6 actions) ====================
    case 'create_spawner':
    case 'configure_spawner':
    case 'set_spawner_enabled':
    case 'configure_spawn_conditions':
    case 'despawn_managed_actors':
    case 'get_spawned_count': {
      requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to ${action.replace(/_/g, ' ')}`
      )) as HandlerResult;
    }

    // ==================== UNKNOWN ACTION ====================
    default:
      // Pass through to C++ - it will return NOT_IMPLEMENTED for unfinished actions
      return cleanObject(await executeAutomationRequest(
        tools, 'manage_gameplay_primitives', args,
        `Failed to execute ${action}`
      )) as HandlerResult;
  }
}
