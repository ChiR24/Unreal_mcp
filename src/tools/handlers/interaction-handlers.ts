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
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all interaction-related actions for the manage_interaction tool.
 */
export async function handleInteractionTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<HandlerResult> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_interaction',
      payload as HandlerArgs,
      `Automation bridge not available for interaction action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // =========================================================================
    // 18.1 Interaction Component (4 actions)
    // =========================================================================

    case 'create_interaction_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Optional: componentName, traceDistance, traceChannel
      return sendRequest('create_interaction_component');
    }

    case 'configure_interaction_trace': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: traceType (line, sphere, box), traceChannel, traceDistance, traceRadius, traceFrequency
      return sendRequest('configure_interaction_trace');
    }

    case 'configure_interaction_widget': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: widgetClass, widgetOffset, showOnHover, showPromptText, promptTextFormat
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
      // Creates door actor blueprint with pivot, rotation animation, sound
      // Optional: folder, meshPath, openAngle, openTime, autoClose, autoCloseDelay, requiresKey
      return sendRequest('create_door_actor');
    }

    case 'configure_door_properties': {
      requireNonEmptyString(argsRecord.doorPath, 'doorPath', 'Missing required parameter: doorPath');
      // Accepts: openAngle, openTime, openDirection, pivotOffset, locked, keyItemPath, openSound, closeSound
      return sendRequest('configure_door_properties');
    }

    case 'create_switch_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates switch/button/lever actor with on/off states
      // Optional: folder, meshPath, switchType (button, lever, pressure_plate), oneShot, resetTime
      return sendRequest('create_switch_actor');
    }

    case 'configure_switch_properties': {
      requireNonEmptyString(argsRecord.switchPath, 'switchPath', 'Missing required parameter: switchPath');
      // Accepts: switchType, toggleable, oneShot, resetTime, activateSound, deactivateSound, targetActors
      return sendRequest('configure_switch_properties');
    }

    case 'create_chest_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates chest/container actor with lid animation, loot integration
      // Optional: folder, meshPath, lidMeshPath, openAngle, lootTablePath, locked, keyItemPath
      return sendRequest('create_chest_actor');
    }

    case 'configure_chest_properties': {
      requireNonEmptyString(argsRecord.chestPath, 'chestPath', 'Missing required parameter: chestPath');
      // Accepts: lootTablePath, locked, keyItemPath, openSound, closeSound, respawnable, respawnTime
      return sendRequest('configure_chest_properties');
    }

    case 'create_lever_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates lever with rotation/translation animation
      // Optional: folder, meshPath, leverType (rotate, translate), moveDistance, moveTime
      return sendRequest('create_lever_actor');
    }

    // =========================================================================
    // 18.3 Destructibles (5 actions)
    // =========================================================================

    case 'setup_destructible_mesh': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Sets up GeometryCollection or legacy destructible
      // Optional: fractureMode (voronoi, uniform, radial), fracturePieces, enablePhysics
      return sendRequest('setup_destructible_mesh');
    }

    case 'configure_destruction_levels': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Accepts: levels array with { damageThreshold, meshIndex, enablePhysics, removeAfterTime }
      return sendRequest('configure_destruction_levels');
    }

    case 'configure_destruction_effects': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Accepts: destroySound, destroyParticle, debrisPhysicsMaterial, debrisLifetime
      return sendRequest('configure_destruction_effects');
    }

    case 'configure_destruction_damage': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Accepts: maxHealth, damageThresholds, impactDamageMultiplier, radialDamageMultiplier
      return sendRequest('configure_destruction_damage');
    }

    case 'add_destruction_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Adds a component to manage destruction state, health, damage reception
      // Optional: componentName, maxHealth, autoDestroy
      return sendRequest('add_destruction_component');
    }

    // =========================================================================
    // 18.4 Trigger System (4 actions)
    // =========================================================================

    case 'create_trigger_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates trigger volume actor
      // Optional: folder, triggerShape (box, sphere, capsule), size, filterByTag, filterByClass
      return sendRequest('create_trigger_actor');
    }

    case 'configure_trigger_events': {
      requireNonEmptyString(argsRecord.triggerPath, 'triggerPath', 'Missing required parameter: triggerPath');
      // Accepts: onEnterEvent, onExitEvent, onStayEvent, stayInterval
      return sendRequest('configure_trigger_events');
    }

    case 'configure_trigger_filter': {
      requireNonEmptyString(argsRecord.triggerPath, 'triggerPath', 'Missing required parameter: triggerPath');
      // Accepts: filterByClass, filterByTag, filterByInterface, ignoreClasses, ignoreTags
      return sendRequest('configure_trigger_filter');
    }

    case 'configure_trigger_response': {
      requireNonEmptyString(argsRecord.triggerPath, 'triggerPath', 'Missing required parameter: triggerPath');
      // Accepts: responseType (once, repeatable, toggle), cooldown, maxActivations
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
