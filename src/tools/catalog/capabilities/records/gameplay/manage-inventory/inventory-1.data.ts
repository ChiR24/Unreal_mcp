/**
 * manage_inventory records, shard 1 of 2 (add_* through configure_pickup_respawn).
 *
 * Action order is preserved from the previous compact record list because the
 * generated parent action enum is assembled in canonical record sequence.
 *
 * Each input map lists exactly the payload fields the matching native shard
 * reads; the `required` set mirrors the requireNonEmptyString/requireAssetName
 * guards in src/tools/handlers/inventory/inventory-handlers.ts.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { IP, inventoryRecord } from './schema.js';

export const INVENTORY_1: readonly CapabilityRecordSource[] = [
  inventoryRecord({
    action: 'add_crafting_component',
    summary: 'Add a crafting component to a Blueprint asset.',
    inputProps: { blueprintPath: IP.blueprintPath, componentName: IP.componentName, save: IP.save },
    required: ['blueprintPath'],
    exampleInput: { action: 'add_crafting_component', blueprintPath: '/Game/Blueprints/BP_Player' },
  }),
  inventoryRecord({
    action: 'add_equipment_functions',
    summary: 'Author standard equipment Blueprint functions.',
    inputProps: { blueprintPath: IP.blueprintPath, save: IP.save },
    required: ['blueprintPath'],
    exampleInput: { action: 'add_equipment_functions', blueprintPath: '/Game/Blueprints/BP_Player' },
  }),
  inventoryRecord({
    action: 'add_inventory_functions',
    summary: 'Author standard inventory Blueprint functions.',
    inputProps: { blueprintPath: IP.blueprintPath, save: IP.save },
    required: ['blueprintPath'],
    exampleInput: { action: 'add_inventory_functions', blueprintPath: '/Game/Blueprints/BP_Player' },
  }),
  inventoryRecord({
    action: 'add_loot_entry',
    summary: 'Add an entry to a loot table asset.',
    inputProps: {
      lootTablePath: IP.lootTablePath,
      itemPath: IP.itemPath,
      lootWeight: IP.lootWeight,
      minQuantity: IP.minQuantity,
      maxQuantity: IP.maxQuantity,
      save: IP.save,
    },
    required: ['lootTablePath', 'itemPath'],
    exampleInput: {
      action: 'add_loot_entry',
      lootTablePath: '/Game/Data/LootTables/LT_Common',
      itemPath: '/Game/Items/DA_Potion',
      lootWeight: 1,
    },
  }),
  inventoryRecord({
    action: 'add_recipe_ingredient',
    summary: 'Add an ingredient to a crafting recipe asset.',
    inputProps: {
      recipePath: IP.recipePath,
      ingredientItemPath: IP.ingredientItemPath,
      quantity: IP.quantity,
      save: IP.save,
    },
    required: ['recipePath', 'ingredientItemPath'],
    exampleInput: {
      action: 'add_recipe_ingredient',
      recipePath: '/Game/Data/Recipes/R_Potion',
      ingredientItemPath: '/Game/Items/DA_Herb',
      quantity: 2,
    },
  }),
  inventoryRecord({
    action: 'assign_item_category',
    summary: 'Assign a category to an item asset.',
    inputProps: { itemPath: IP.itemPath, categoryPath: IP.categoryPath, save: IP.save },
    required: ['itemPath', 'categoryPath'],
    exampleInput: {
      action: 'assign_item_category',
      itemPath: '/Game/Items/DA_Potion',
      categoryPath: '/Game/Items/Categories/DA_Consumable',
      save: true,
    },
  }),
  inventoryRecord({
    action: 'configure_equipment_effects',
    summary: 'Configure authored equipment effects.',
    inputProps: {
      blueprintPath: IP.blueprintPath,
      statModifiers: IP.statModifiers,
      abilityGrants: IP.abilityGrants,
      passiveEffects: IP.passiveEffects,
      save: IP.save,
    },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'configure_equipment_effects',
      blueprintPath: '/Game/Blueprints/BP_Player',
      statModifiers: true,
    },
  }),
  inventoryRecord({
    action: 'configure_equipment_visuals',
    summary: 'Configure authored equipment attachment visuals.',
    inputProps: {
      blueprintPath: IP.blueprintPath,
      attachToSocket: IP.attachToSocket,
      defaultSocket: IP.defaultSocket,
      save: IP.save,
    },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'configure_equipment_visuals',
      blueprintPath: '/Game/Blueprints/BP_Player',
      defaultSocket: 'hand_r',
    },
  }),
  inventoryRecord({
    action: 'configure_inventory_events',
    summary: 'Author inventory event dispatchers.',
    inputProps: { blueprintPath: IP.blueprintPath, save: IP.save },
    required: ['blueprintPath'],
    exampleInput: { action: 'configure_inventory_events', blueprintPath: '/Game/Blueprints/BP_Player' },
  }),
  inventoryRecord({
    action: 'configure_inventory_slots',
    summary: 'Configure persistent inventory slot data.',
    inputProps: { blueprintPath: IP.blueprintPath, slotCount: IP.slotCount, save: IP.save },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'configure_inventory_slots',
      blueprintPath: '/Game/Blueprints/BP_Player',
      slotCount: 30,
    },
  }),
  inventoryRecord({
    action: 'configure_inventory_weight',
    summary: 'Configure persistent inventory weight rules.',
    inputProps: {
      blueprintPath: IP.blueprintPath,
      maxWeight: IP.maxWeight,
      enableWeight: IP.enableWeight,
      encumberanceSystem: IP.encumberanceSystem,
      encumberanceThreshold: IP.encumberanceThreshold,
      save: IP.save,
    },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'configure_inventory_weight',
      blueprintPath: '/Game/Blueprints/BP_Player',
      maxWeight: 150,
      enableWeight: true,
    },
  }),
  inventoryRecord({
    action: 'configure_item_stacking',
    summary: 'Configure persistent item stacking rules.',
    inputProps: {
      itemPath: IP.itemPath,
      stackable: IP.stackable,
      maxStackSize: IP.maxStackSize,
      uniqueItems: IP.uniqueItems,
      save: IP.save,
    },
    required: ['itemPath'],
    exampleInput: {
      action: 'configure_item_stacking',
      itemPath: '/Game/Items/DA_Potion',
      stackable: true,
      maxStackSize: 99,
    },
  }),
  inventoryRecord({
    action: 'configure_loot_drop',
    summary: 'Configure authored loot-drop behavior.',
    inputProps: {
      actorPath: IP.actorPath,
      lootTablePath: IP.lootTablePath,
      dropCount: IP.dropCount,
      dropRadius: IP.dropRadius,
      dropOnDeath: IP.dropOnDeath,
      save: IP.save,
    },
    required: ['actorPath', 'lootTablePath'],
    exampleInput: {
      action: 'configure_loot_drop',
      actorPath: '/Game/Blueprints/BP_Enemy',
      lootTablePath: '/Game/Data/LootTables/LT_Common',
      dropOnDeath: true,
    },
  }),
  inventoryRecord({
    action: 'configure_pickup_effects',
    summary: 'Configure persistent pickup visual behavior.',
    inputProps: {
      pickupPath: IP.pickupPath,
      bobbing: IP.bobbing,
      rotation: IP.rotation,
      glowEffect: IP.glowEffect,
      save: IP.save,
    },
    required: ['pickupPath'],
    exampleInput: {
      action: 'configure_pickup_effects',
      pickupPath: '/Game/Blueprints/Pickups/BP_Pickup',
      bobbing: true,
      rotation: true,
    },
  }),
  inventoryRecord({
    action: 'configure_pickup_interaction',
    summary: 'Configure persistent pickup interaction data.',
    inputProps: {
      pickupPath: IP.pickupPath,
      interactionType: IP.interactionType,
      prompt: IP.prompt,
      save: IP.save,
    },
    required: ['pickupPath'],
    exampleInput: {
      action: 'configure_pickup_interaction',
      pickupPath: '/Game/Blueprints/Pickups/BP_Pickup',
      interactionType: 'Overlap',
    },
  }),
  inventoryRecord({
    action: 'configure_pickup_respawn',
    summary: 'Configure persistent pickup respawn data.',
    inputProps: {
      pickupPath: IP.pickupPath,
      respawnable: IP.respawnable,
      respawnTime: IP.respawnTime,
      save: IP.save,
    },
    required: ['pickupPath'],
    exampleInput: {
      action: 'configure_pickup_respawn',
      pickupPath: '/Game/Blueprints/Pickups/BP_Pickup',
      respawnable: true,
      respawnTime: 30,
    },
  }),
];
