/**
 * Inventory & Items System Handlers (Phase 17)
 *
 * Complete inventory implementation including:
 * - Data Assets (item creation, properties, stacking)
 * - Inventory Component (component creation, slots, functions)
 * - Pickups (pickup actors, interaction, respawn)
 * - Equipment (equipment component, slots, effects)
 * - Loot System (loot tables, drops, quality tiers)
 * - Crafting (recipes, workbenches, requirements)
 *
 * @module inventory-handlers
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
 * Handles all inventory-related actions for the manage_inventory tool.
 */
export async function handleInventoryTools(
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
      'manage_inventory',
      payload as HandlerArgs,
      `Automation bridge not available for inventory action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // =========================================================================
    // 17.1 Data Assets (4 actions)
    // =========================================================================

    case 'create_item_data_asset': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Optional: parentClass, folder
      return sendRequest('create_item_data_asset');
    }

    case 'set_item_properties': {
      requireNonEmptyString(argsRecord.itemPath, 'itemPath', 'Missing required parameter: itemPath');
      // Accepts: displayName, description, icon, mesh, stackSize, weight, rarity, value, tags, customProperties
      return sendRequest('set_item_properties');
    }

    case 'create_item_category': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Optional: parentCategory, description, icon
      return sendRequest('create_item_category');
    }

    case 'assign_item_category': {
      requireNonEmptyString(argsRecord.itemPath, 'itemPath', 'Missing required parameter: itemPath');
      requireNonEmptyString(argsRecord.categoryPath, 'categoryPath', 'Missing required parameter: categoryPath');
      return sendRequest('assign_item_category');
    }

    // =========================================================================
    // 17.2 Inventory Component (5 actions)
    // =========================================================================

    case 'create_inventory_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Optional: componentName, slotCount, maxWeight, allowStacking
      return sendRequest('create_inventory_component');
    }

    case 'configure_inventory_slots': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: slotCount, slotSize, slotCategories (array), slotRestrictions
      return sendRequest('configure_inventory_slots');
    }

    case 'add_inventory_functions': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Creates standard inventory functions: AddItem, RemoveItem, GetItemCount, HasItem, TransferItem, etc.
      return sendRequest('add_inventory_functions');
    }

    case 'configure_inventory_events': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Creates event dispatchers: OnItemAdded, OnItemRemoved, OnInventoryChanged, OnSlotUpdated
      return sendRequest('configure_inventory_events');
    }

    case 'set_inventory_replication': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: replicated (bool), replicationCondition
      return sendRequest('set_inventory_replication');
    }

    // =========================================================================
    // 17.3 Pickups (4 actions)
    // =========================================================================

    case 'create_pickup_actor': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Optional: meshPath, itemDataPath, interactionRadius
      return sendRequest('create_pickup_actor');
    }

    case 'configure_pickup_interaction': {
      requireNonEmptyString(argsRecord.pickupPath, 'pickupPath', 'Missing required parameter: pickupPath');
      // Accepts: interactionType (overlap, interact, key), interactionKey, prompt, highlightMaterial
      return sendRequest('configure_pickup_interaction');
    }

    case 'configure_pickup_respawn': {
      requireNonEmptyString(argsRecord.pickupPath, 'pickupPath', 'Missing required parameter: pickupPath');
      // Accepts: respawnable (bool), respawnTime, respawnEffect
      return sendRequest('configure_pickup_respawn');
    }

    case 'configure_pickup_effects': {
      requireNonEmptyString(argsRecord.pickupPath, 'pickupPath', 'Missing required parameter: pickupPath');
      // Accepts: pickupSound, pickupParticle, bobbing (bool), rotation (bool), glowEffect
      return sendRequest('configure_pickup_effects');
    }

    // =========================================================================
    // 17.4 Equipment System (5 actions)
    // =========================================================================

    case 'create_equipment_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Optional: componentName
      return sendRequest('create_equipment_component');
    }

    case 'define_equipment_slots': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: slots array with { name, socketName, allowedCategories, restrictedCategories }
      return sendRequest('define_equipment_slots');
    }

    case 'configure_equipment_effects': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: statModifiers, abilityGrants, passiveEffects
      return sendRequest('configure_equipment_effects');
    }

    case 'add_equipment_functions': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Creates: EquipItem, UnequipItem, GetEquippedItem, CanEquip, SwapEquipment
      return sendRequest('add_equipment_functions');
    }

    case 'configure_equipment_visuals': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: attachToSocket (bool), meshComponent, animationOverrides
      return sendRequest('configure_equipment_visuals');
    }

    // =========================================================================
    // 17.5 Loot System (4 actions)
    // =========================================================================

    case 'create_loot_table': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Optional: folder, description
      return sendRequest('create_loot_table');
    }

    case 'add_loot_entry': {
      requireNonEmptyString(argsRecord.lootTablePath, 'lootTablePath', 'Missing required parameter: lootTablePath');
      requireNonEmptyString(argsRecord.itemPath, 'itemPath', 'Missing required parameter: itemPath');
      // Accepts: weight, minQuantity, maxQuantity, conditions
      return sendRequest('add_loot_entry');
    }

    case 'configure_loot_drop': {
      requireNonEmptyString(argsRecord.actorPath, 'actorPath', 'Missing required parameter: actorPath');
      requireNonEmptyString(argsRecord.lootTablePath, 'lootTablePath', 'Missing required parameter: lootTablePath');
      // Accepts: dropCount, guaranteedDrops, dropRadius, dropForce
      return sendRequest('configure_loot_drop');
    }

    case 'set_loot_quality_tiers': {
      requireNonEmptyString(argsRecord.lootTablePath, 'lootTablePath', 'Missing required parameter: lootTablePath');
      // Accepts: tiers array with { name, color, dropWeight, statMultiplier }
      return sendRequest('set_loot_quality_tiers');
    }

    // =========================================================================
    // 17.6 Crafting System (4 actions)
    // =========================================================================

    case 'create_crafting_recipe': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      requireNonEmptyString(argsRecord.outputItemPath, 'outputItemPath', 'Missing required parameter: outputItemPath');
      // Accepts: ingredients (array of {itemPath, quantity}), outputQuantity, craftTime
      return sendRequest('create_crafting_recipe');
    }

    case 'configure_recipe_requirements': {
      requireNonEmptyString(argsRecord.recipePath, 'recipePath', 'Missing required parameter: recipePath');
      // Accepts: requiredLevel, requiredSkills, requiredStation, unlockConditions
      return sendRequest('configure_recipe_requirements');
    }

    case 'create_crafting_station': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Optional: meshPath, recipes (array), stationType
      return sendRequest('create_crafting_station');
    }

    case 'add_crafting_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Creates UCraftingComponent with recipes, crafting queue, etc.
      return sendRequest('add_crafting_component');
    }

    // =========================================================================
    // Utility (1 action)
    // =========================================================================

    case 'get_inventory_info': {
      // At least one path is required
      const hasPath = argsRecord.blueprintPath || argsRecord.itemPath ||
                      argsRecord.lootTablePath || argsRecord.recipePath ||
                      argsRecord.pickupPath;
      if (!hasPath) {
        return cleanObject({
          success: false,
          error: 'MISSING_PARAMETER',
          message: 'At least one path parameter is required (blueprintPath, itemPath, lootTablePath, recipePath, or pickupPath)'
        });
      }
      return sendRequest('get_inventory_info');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown inventory action: ${action}`
      });
  }
}
