/**
 * Interaction System Handlers (Phase 18)
 *
 * Complete interaction framework including:
 * - Interaction Component (trace, widget, interaction system)
 * - Interactables (doors, switches, chests, levers)
 * - Destructibles (destructible meshes, damage, levels)
 * - Utility (info queries)
 *
 * @module interaction-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { createSubActionDispatcher, requireNonEmptyString } from './common-handlers.js';


/**
 * Handles all interaction-related actions for the manage_interaction tool.
 */
export async function handleInteractionTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { argsRecord, sendRequest } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_interaction',
    domainName: 'interaction',
    pathFields: [
      'blueprintPath',
      'doorPath',
      'switchPath',
      'chestPath',
      'triggerPath',
      'lootTablePath',
      'folder'
    ]
  });

  switch (action) {
    // =========================================================================
    // 18.1 Interaction Component (4 actions)
    // =========================================================================

    case 'create_interaction_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Optional: componentName, traceDistance
      return sendRequest('create_interaction_component');
    }

    case 'configure_interaction_trace': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: traceType (line, sphere, box), traceDistance, traceRadius
      return sendRequest('configure_interaction_trace');
    }

    case 'configure_interaction_widget': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: widgetClass, showOnHover, showPromptText, promptTextFormat
      return sendRequest('configure_interaction_widget');
    }

    case 'add_interaction_events': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Creates event dispatchers: OnInteractionStart, OnInteractionEnd, OnInteractableFound, OnInteractableLost
      return sendRequest('add_interaction_events');
    }

    // =========================================================================
    // 18.2 Interactables (8 actions)
    // =========================================================================

    case 'create_interactable_interface': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates a UInterface with Interact, GetInteractionPrompt, CanInteract functions
      // Optional: folder
      return sendRequest('create_interactable_interface');
    }

    case 'create_door_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates door actor blueprint with pivot and rotation animation
      // Optional: folder, openAngle, openTime, autoClose, autoCloseDelay, requiresKey
      return sendRequest('create_door_actor');
    }

    case 'configure_door_properties': {
      requireNonEmptyString(argsRecord.doorPath, 'doorPath', 'Missing required parameter: doorPath');
      // Accepts: openAngle, openTime, locked
      return sendRequest('configure_door_properties');
    }

    case 'create_switch_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates switch/button/lever actor with on/off states
      // Optional: folder, switchType (button, lever, pressure_plate), resetTime
      return sendRequest('create_switch_actor');
    }

    case 'configure_switch_properties': {
      requireNonEmptyString(argsRecord.switchPath, 'switchPath', 'Missing required parameter: switchPath');
      // Accepts: switchType, canToggle, resetTime
      return sendRequest('configure_switch_properties');
    }

    case 'create_chest_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates chest/container actor with lid animation
      // Optional: folder, openAngle, lootTablePath, locked
      return sendRequest('create_chest_actor');
    }

    case 'configure_chest_properties': {
      requireNonEmptyString(argsRecord.chestPath, 'chestPath', 'Missing required parameter: chestPath');
      // Accepts: lootTablePath, locked, openAngle, openTime
      return sendRequest('configure_chest_properties');
    }

    case 'create_lever_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates lever with rotation animation
      // Optional: folder
      return sendRequest('create_lever_actor');
    }

    // =========================================================================
    // 18.3 Destructibles (5 actions)
    // =========================================================================

    case 'setup_destructible_mesh': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Tags an actor as destructible-ready
      return sendRequest('setup_destructible_mesh');
    }

    case 'configure_destruction_levels': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Tags an actor with destruction-level configuration state
      return sendRequest('configure_destruction_levels');
    }

    case 'configure_destruction_effects': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Tags an actor with destruction-effects configuration state
      return sendRequest('configure_destruction_effects');
    }

    case 'configure_destruction_damage': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Tags an actor with destruction-damage configuration state
      return sendRequest('configure_destruction_damage');
    }

    case 'add_destruction_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Adds a component to manage destruction state, health, damage reception
      // Optional: componentName
      return sendRequest('add_destruction_component');
    }

    // =========================================================================
    // 18.4 Trigger System (4 actions)
    // =========================================================================

    case 'create_trigger_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates trigger volume actor
      // Optional: folder, triggerShape (box, sphere, capsule)
      return sendRequest('create_trigger_actor');
    }

    case 'configure_trigger_events': {
      requireNonEmptyString(argsRecord.triggerPath, 'triggerPath', 'Missing required parameter: triggerPath');
      // Adds fixed trigger enter/exit/activated event dispatchers
      return sendRequest('configure_trigger_events');
    }

    case 'configure_trigger_filter': {
      requireNonEmptyString(argsRecord.triggerPath, 'triggerPath', 'Missing required parameter: triggerPath');
      // Adds fixed trigger filter variables
      return sendRequest('configure_trigger_filter');
    }

    case 'configure_trigger_response': {
      requireNonEmptyString(argsRecord.triggerPath, 'triggerPath', 'Missing required parameter: triggerPath');
      // Adds fixed trigger response variables
      return sendRequest('configure_trigger_response');
    }

    // =========================================================================
    // Utility (1 action)
    // =========================================================================

    case 'get_interaction_info': {
      // At least one path is required
      const hasPath = argsRecord.blueprintPath || argsRecord.actorName ||
                      argsRecord.doorPath || argsRecord.switchPath ||
                      argsRecord.chestPath || argsRecord.triggerPath;
      if (!hasPath) {
        return cleanObject({
          success: false,
          error: 'MISSING_PARAMETER',
          message: 'At least one path parameter is required (blueprintPath, actorName, doorPath, switchPath, chestPath, or triggerPath)'
        });
      }
      return sendRequest('get_interaction_info');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown interaction action: ${action}`
      });
  }
}
