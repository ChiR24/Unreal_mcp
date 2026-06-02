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
import { createSubActionDispatcher, requireNonEmptyString, requireAssetName } from './common-handlers.js';


/**
 * Handles all inventory-related actions for the manage_inventory tool.
 */
export async function handleInventoryTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { argsRecord, sendRequest } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_inventory',
    domainName: 'inventory',
    pathFields: [
      'itemPath',
      'categoryPath',
      'blueprintPath',
      'pickupPath',
      'lootTablePath',
      'actorPath',
      'outputItemPath',
      'recipePath',
      'ingredientItemPath',
      'stationPath',
      'iconPath',
      'path'
    ]
  });

  switch (action) {
    // =========================================================================
    // 17.1 Data Assets (4 actions)
    // =========================================================================

  case 'create_item_data_asset': {
    requireAssetName(argsRecord.name, 'name', 'Missing required parameter: name');
    return sendRequest('create_item_data_asset');
  }

    case 'set_item_properties': {
      requireNonEmptyString(argsRecord.itemPath, 'itemPath', 'Missing required parameter: itemPath');
      // Accepts: properties
      return sendRequest('set_item_properties');
    }

  case 'create_item_category': {
    requireAssetName(argsRecord.name, 'name', 'Missing required parameter: name');
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
      // Optional: componentName, slotCount
      return sendRequest('create_inventory_component');
    }

    case 'configure_inventory_slots': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: slotCount
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
    requireAssetName(argsRecord.name, 'name', 'Missing required parameter: name');
    return sendRequest('create_pickup_actor');
  }

    case 'configure_pickup_interaction': {
      requireNonEmptyString(argsRecord.pickupPath, 'pickupPath', 'Missing required parameter: pickupPath');
      // Accepts: interactionType (overlap, interact, key), prompt
      return sendRequest('configure_pickup_interaction');
    }

    case 'configure_pickup_respawn': {
      requireNonEmptyString(argsRecord.pickupPath, 'pickupPath', 'Missing required parameter: pickupPath');
      // Accepts: respawnable (bool), respawnTime
      return sendRequest('configure_pickup_respawn');
    }

    case 'configure_pickup_effects': {
      requireNonEmptyString(argsRecord.pickupPath, 'pickupPath', 'Missing required parameter: pickupPath');
      // Accepts: bobbing (bool), rotation (bool), glowEffect
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
      // Accepts: slots string array
      return sendRequest('define_equipment_slots');
    }

    case 'configure_equipment_effects': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: statModifiers, abilityGrants, passiveEffects flags
      return sendRequest('configure_equipment_effects');
    }

    case 'add_equipment_functions': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Creates: EquipItem, UnequipItem, GetEquippedItem, CanEquip, SwapEquipment
      return sendRequest('add_equipment_functions');
    }

    case 'configure_equipment_visuals': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: attachToSocket (bool), defaultSocket
      return sendRequest('configure_equipment_visuals');
    }

    // =========================================================================
    // 17.5 Loot System (4 actions)
    // =========================================================================

  case 'create_loot_table': {
    requireAssetName(argsRecord.name, 'name', 'Missing required parameter: name');
    // Optional: path, save
    return sendRequest('create_loot_table');
  }

    case 'add_loot_entry': {
      requireNonEmptyString(argsRecord.lootTablePath, 'lootTablePath', 'Missing required parameter: lootTablePath');
      requireNonEmptyString(argsRecord.itemPath, 'itemPath', 'Missing required parameter: itemPath');
      // Accepts: lootWeight, minQuantity, maxQuantity
      return sendRequest('add_loot_entry');
    }

    case 'configure_loot_drop': {
      requireNonEmptyString(argsRecord.actorPath, 'actorPath', 'Missing required parameter: actorPath');
      requireNonEmptyString(argsRecord.lootTablePath, 'lootTablePath', 'Missing required parameter: lootTablePath');
      // Accepts: dropCount, dropRadius, dropOnDeath
      return sendRequest('configure_loot_drop');
    }

    case 'set_loot_quality_tiers': {
      requireNonEmptyString(argsRecord.lootTablePath, 'lootTablePath', 'Missing required parameter: lootTablePath');
      // Accepts: tiers array with { name, dropWeight }
      return sendRequest('set_loot_quality_tiers');
    }

    // =========================================================================
    // 17.6 Crafting System (4 actions)
    // =========================================================================

  case 'create_crafting_recipe': {
    requireAssetName(argsRecord.name, 'name', 'Missing required parameter: name');
    requireNonEmptyString(argsRecord.outputItemPath, 'outputItemPath', 'Missing required parameter: outputItemPath');
    // Accepts: outputQuantity, craftTime
    return sendRequest('create_crafting_recipe');
  }

    case 'configure_recipe_requirements': {
      requireNonEmptyString(argsRecord.recipePath, 'recipePath', 'Missing required parameter: recipePath');
      // Accepts: requiredLevel, requiredStation
      return sendRequest('configure_recipe_requirements');
    }

  case 'create_crafting_station': {
    requireAssetName(argsRecord.name, 'name', 'Missing required parameter: name');
    // Optional: stationType
    return sendRequest('create_crafting_station');
  }

    case 'add_crafting_component': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Creates UCraftingComponent with recipes, crafting queue, etc.
      return sendRequest('add_crafting_component');
    }

    // =========================================================================
    // 17.7 Additional Actions (6 actions)
    // =========================================================================

    case 'configure_item_stacking': {
      requireNonEmptyString(argsRecord.itemPath, 'itemPath', 'Missing required parameter: itemPath');
      // Accepts: stackable (bool), maxStackSize, uniqueItems (bool)
      return sendRequest('configure_item_stacking');
    }

    case 'set_item_icon': {
      requireNonEmptyString(argsRecord.itemPath, 'itemPath', 'Missing required parameter: itemPath');
      // Accepts: iconPath (texture/material path)
      return sendRequest('set_item_icon');
    }

    case 'add_recipe_ingredient': {
      requireNonEmptyString(argsRecord.recipePath, 'recipePath', 'Missing required parameter: recipePath');
      requireNonEmptyString(argsRecord.ingredientItemPath, 'ingredientItemPath', 'Missing required parameter: ingredientItemPath');
      // Accepts: quantity
      return sendRequest('add_recipe_ingredient');
    }

    case 'remove_loot_entry': {
      requireNonEmptyString(argsRecord.lootTablePath, 'lootTablePath', 'Missing required parameter: lootTablePath');
      // Accepts: entryIndex OR itemPath
      return sendRequest('remove_loot_entry');
    }

    case 'configure_inventory_weight': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Accepts: maxWeight, enableWeight (bool), encumberanceSystem (bool), encumberanceThreshold
      return sendRequest('configure_inventory_weight');
    }

    case 'configure_station_recipes': {
      requireNonEmptyString(argsRecord.stationPath, 'stationPath', 'Missing required parameter: stationPath');
      // Accepts: recipePaths (array), stationType, craftingSpeedMultiplier
      return sendRequest('configure_station_recipes');
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
