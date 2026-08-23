/**
 * manage_inventory records, shard 2 of 2 (configure_recipe_requirements onward).
 *
 * See inventory-1.data.ts for the ordering and grounding notes. get_inventory_info
 * is the only read action and the only one whose native shard reads no `save`
 * flag, so it declares none.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { IP, inventoryRecord } from './schema.js';
import { P } from '../properties.js';

export const INVENTORY_2: readonly CapabilityRecordSource[] = [
  inventoryRecord({
    action: 'configure_recipe_requirements',
    summary: 'Configure persistent crafting requirements.',
    inputProps: {
      recipePath: IP.recipePath,
      requiredLevel: IP.requiredLevel,
      requiredStation: IP.requiredStation,
      save: IP.save,
    },
    required: ['recipePath'],
    exampleInput: {
      action: 'configure_recipe_requirements',
      recipePath: '/Game/Data/Recipes/R_Potion',
      requiredLevel: 5,
    },
  }),
  inventoryRecord({
    action: 'configure_station_recipes',
    summary: 'Configure recipe references on a crafting station asset.',
    inputProps: {
      stationPath: IP.stationPath,
      recipePaths: IP.recipePaths,
      stationType: IP.stationType,
      craftingSpeedMultiplier: IP.craftingSpeedMultiplier,
      save: IP.save,
    },
    required: ['stationPath'],
    exampleInput: {
      action: 'configure_station_recipes',
      stationPath: '/Game/Blueprints/CraftingStations/BP_Forge',
      recipePaths: ['/Game/Data/Recipes/R_Sword'],
    },
  }),
  inventoryRecord({
    action: 'create_crafting_recipe',
    summary: 'Create a crafting recipe asset.',
    inputProps: {
      name: IP.name,
      path: IP.path,
      outputItemPath: IP.outputItemPath,
      outputQuantity: IP.outputQuantity,
      craftTime: IP.craftTime,
      save: IP.save,
    },
    required: ['name', 'outputItemPath'],
    exampleInput: {
      action: 'create_crafting_recipe',
      name: 'R_Potion',
      outputItemPath: '/Game/Items/DA_Potion',
      outputQuantity: 1,
    },
  }),
  inventoryRecord({
    action: 'create_crafting_station',
    summary: 'Create a crafting station Blueprint asset.',
    inputProps: { name: IP.name, path: IP.path, stationType: IP.stationType, save: IP.save },
    required: ['name'],
    exampleInput: { action: 'create_crafting_station', name: 'BP_Forge', stationType: 'Basic' },
  }),
  inventoryRecord({
    action: 'create_equipment_component',
    summary: 'Add an equipment component to a Blueprint asset.',
    inputProps: { blueprintPath: IP.blueprintPath, componentName: IP.componentName, save: IP.save },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'create_equipment_component',
      blueprintPath: '/Game/Blueprints/BP_Player',
      componentName: 'EquipmentComponent',
    },
  }),
  inventoryRecord({
    action: 'create_inventory_component',
    summary: 'Add an inventory component to a Blueprint asset.',
    inputProps: {
      blueprintPath: IP.blueprintPath,
      componentName: IP.componentName,
      slotCount: IP.slotCount,
      save: IP.save,
    },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'create_inventory_component',
      blueprintPath: '/Game/Blueprints/BP_Player',
      componentName: 'InventoryComponent',
      slotCount: 20,
    },
  }),
  inventoryRecord({
    action: 'create_item_category',
    summary: 'Create an item category asset.',
    inputProps: { name: IP.name, path: IP.path, save: IP.save },
    required: ['name'],
    exampleInput: { action: 'create_item_category', name: 'DA_Consumable', path: '/Game/Items/Categories' },
  }),
  inventoryRecord({
    action: 'create_item_data_asset',
    summary: 'Create an item data asset.',
    inputProps: { name: IP.name, path: IP.path, save: IP.save },
    required: ['name'],
    exampleInput: { action: 'create_item_data_asset', name: 'DA_Potion', path: '/Game/Items' },
  }),
  inventoryRecord({
    action: 'create_loot_table',
    summary: 'Create a loot table asset.',
    inputProps: { name: IP.name, path: IP.path, save: IP.save },
    required: ['name'],
    exampleInput: { action: 'create_loot_table', name: 'LT_Common', path: '/Game/Data/LootTables' },
  }),
  inventoryRecord({
    action: 'create_pickup_actor',
    summary: 'Create a pickup actor Blueprint asset.',
    inputProps: { name: IP.name, path: IP.path, save: IP.save },
    required: ['name'],
    exampleInput: { action: 'create_pickup_actor', name: 'BP_Pickup', path: '/Game/Blueprints/Pickups' },
  }),
  inventoryRecord({
    action: 'define_equipment_slots',
    summary: 'Define persistent equipment slot data.',
    inputProps: { blueprintPath: IP.blueprintPath, slots: IP.slots, save: IP.save },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'define_equipment_slots',
      blueprintPath: '/Game/Blueprints/BP_Player',
      slots: ['Head', 'Chest', 'MainHand'],
    },
  }),
  inventoryRecord({
    action: 'get_inventory_info',
    summary: 'Read inventory, item, pickup, loot, or recipe asset metadata.',
    read: true,
    inputProps: {
      blueprintPath: IP.blueprintPath,
      itemPath: IP.itemPath,
      lootTablePath: IP.lootTablePath,
      recipePath: IP.recipePath,
      pickupPath: IP.pickupPath,
    },
    outputProps: {
      assetPath: P.assetPath,
      assetType: { type: 'string', description: 'Resolved asset type (item, lootTable, recipe, pickup, blueprint).' },
      className: { type: 'string', description: 'UClass or data asset class name.' },
      itemPath: IP.itemPath,
      lootTablePath: IP.lootTablePath,
      recipePath: IP.recipePath,
      pickupPath: IP.pickupPath,
      blueprintPath: IP.blueprintPath,
      properties: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Authored item properties.' },
      category: { type: 'string', description: 'Item category asset path.' },
      stackable: IP.stackable,
      maxStackSize: IP.maxStackSize,
      icon: IP.iconPath,
      weight: { type: 'number', description: 'Item weight.' },
      replication: { type: 'string', description: 'Replication condition applied to inventory state.' },
      lootEntries: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Loot table entries.' },
      tiers: IP.tiers,
      entryCount: { type: 'number', description: 'Number of loot entries.' },
      ingredients: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Recipe ingredients.' },
      outputItem: { type: 'string', description: 'Recipe output item asset path.' },
      outputQuantity: IP.outputQuantity,
      craftTime: IP.craftTime,
      requiredLevel: IP.requiredLevel,
      requiredStation: IP.requiredStation,
      components: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Blueprint SCS components.' },
      variables: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Blueprint NewVariables (recipes, station settings).' },
      recipes: IP.recipePaths,
      unsupportedFields: { type: 'array', items: { type: 'string' }, description: 'Categories the handler could not project.' },
    },
    exampleInput: { action: 'get_inventory_info', blueprintPath: '/Game/Blueprints/BP_Player' },
  }),
  inventoryRecord({
    action: 'remove_loot_entry',
    summary: 'Remove an entry from a loot table asset.',
    inputProps: {
      lootTablePath: IP.lootTablePath,
      entryIndex: IP.entryIndex,
      itemPath: IP.itemPath,
      save: IP.save,
    },
    required: ['lootTablePath'],
    requiredOneOf: ['entryIndex', 'itemPath'],
    exampleInput: {
      action: 'remove_loot_entry',
      lootTablePath: '/Game/Data/LootTables/LT_Common',
      entryIndex: 0,
    },
  }),
  inventoryRecord({
    action: 'set_inventory_replication',
    summary: 'Set inventory component replication metadata.',
    inputProps: {
      blueprintPath: IP.blueprintPath,
      replicated: IP.replicated,
      replicationCondition: IP.replicationCondition,
      save: IP.save,
    },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'set_inventory_replication',
      blueprintPath: '/Game/Blueprints/BP_Player',
      replicated: true,
    },
  }),
  inventoryRecord({
    action: 'set_item_icon',
    summary: 'Set the icon reference on an item asset.',
    inputProps: { itemPath: IP.itemPath, iconPath: IP.iconPath, save: IP.save },
    required: ['itemPath'],
    exampleInput: {
      action: 'set_item_icon',
      itemPath: '/Game/Items/DA_Potion',
      iconPath: '/Game/UI/Icons/T_Potion',
    },
  }),
  inventoryRecord({
    action: 'set_item_properties',
    summary: 'Set persistent item asset properties.',
    inputProps: { itemPath: IP.itemPath, properties: IP.properties, save: IP.save },
    required: ['itemPath'],
    exampleInput: {
      action: 'set_item_properties',
      itemPath: '/Game/Items/DA_Potion',
      properties: { DisplayName: 'Potion' },
      save: true,
    },
  }),
  inventoryRecord({
    action: 'set_loot_quality_tiers',
    summary: 'Set quality tiers on a loot table asset.',
    inputProps: { lootTablePath: IP.lootTablePath, tiers: IP.tiers, save: IP.save },
    required: ['lootTablePath'],
    exampleInput: {
      action: 'set_loot_quality_tiers',
      lootTablePath: '/Game/Data/LootTables/LT_Common',
      tiers: [{ name: 'Rare', dropWeight: 0.1 }],
    },
  }),
];
