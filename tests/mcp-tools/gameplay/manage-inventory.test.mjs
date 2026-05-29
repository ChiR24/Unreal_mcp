#!/usr/bin/env node
/**
 * manage_inventory Tool Integration Tests
 * Covers all 33 actions with real asset paths captured from setup and creation.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayInventory_${ts}`;
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const BLUEPRINT_NAME = `BP_MCP_InventoryActor_${ts}`;
const ITEM_NAME = `DA_MCP_Item_${ts}`;
const CATEGORY_NAME = `DA_MCP_Category_${ts}`;
const PICKUP_NAME = `BP_MCP_Pickup_${ts}`;
const LOOT_TABLE_NAME = `DA_MCP_LootTable_${ts}`;
const RECIPE_NAME = `DA_MCP_Recipe_${ts}`;
const STATION_NAME = `BP_MCP_CraftingStation_${ts}`;

const EXPECTED_BLUEPRINT_PATH = `${TEST_FOLDER}/${BLUEPRINT_NAME}`;
const EXPECTED_ITEM_PATH = `${TEST_FOLDER}/${ITEM_NAME}`;
const EXPECTED_CATEGORY_PATH = `${TEST_FOLDER}/${CATEGORY_NAME}`;
const EXPECTED_PICKUP_PATH = `${TEST_FOLDER}/${PICKUP_NAME}`;
const EXPECTED_LOOT_TABLE_PATH = `${TEST_FOLDER}/${LOOT_TABLE_NAME}`;
const EXPECTED_RECIPE_PATH = `${TEST_FOLDER}/${RECIPE_NAME}`;

const blueprintPath = '${captured:blueprintPath}';
const itemPath = '${captured:itemPath}';
const categoryPath = '${captured:categoryPath}';
const pickupPath = '${captured:pickupPath}';
const lootTablePath = '${captured:lootTablePath}';
const recipePath = '${captured:recipePath}';
const stationPath = '${captured:stationPath}';

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  {
    scenario: 'Setup: create inventory actor blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', name: BLUEPRINT_NAME, path: TEST_FOLDER, parentClass: 'Actor' },
    expected: 'success',
    captureResult: { key: 'blueprintPath', fromField: 'result.assetPath' },
    assertions: [{ path: 'structuredContent.result.existsAfter', equals: true, label: 'inventory actor blueprint exists after creation' }]
  },

  // === ITEM DATA ===
  {
    scenario: 'CREATE: create_item_data_asset',
    toolName: 'manage_inventory',
    arguments: { action: 'create_item_data_asset', name: ITEM_NAME, path: TEST_FOLDER_ALIAS },
    expected: 'success',
    captureResult: { key: 'itemPath', fromField: 'result.assetPath' },
    assertions: [{ path: 'structuredContent.result.assetClass', equals: 'McpGenericDataAsset', label: 'item data asset class reported' }]
  },
  { scenario: 'CONFIG: set_item_properties', toolName: 'manage_inventory', arguments: { action: 'set_item_properties', itemPath, properties: {}, save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.propertiesModified', equals: 0, label: 'generic item accepts properties payload without false modification' }, { path: 'structuredContent.result.assetPath', equals: EXPECTED_ITEM_PATH, label: 'item properties target verified' }] },
  { scenario: 'CONFIG: configure_item_stacking', toolName: 'manage_inventory', arguments: { action: 'configure_item_stacking', itemPath, stackable: true, maxStackSize: 25, uniqueItems: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.maxStackSize', equals: 25, label: 'stack size request returned' }, { path: 'structuredContent.result.configured', equals: true, label: 'stacking persisted into asset data' }, { path: 'structuredContent.result.modifiedProperties', length: 3, label: 'stacking properties modified' }] },
  { scenario: 'CONFIG: set_item_icon', toolName: 'manage_inventory', arguments: { action: 'set_item_icon', itemPath, iconPath: '/Game/MCPTest/FakeInventoryIcon' }, expected: 'success', assertions: [{ path: 'structuredContent.result.iconPath', equals: '/Game/MCPTest/FakeInventoryIcon', label: 'icon path request returned' }, { path: 'structuredContent.result.itemPath', equals: EXPECTED_ITEM_PATH, label: 'icon target item returned' }, { path: 'structuredContent.result.iconSet', equals: true, label: 'icon path persisted into asset data' }, { path: 'structuredContent.result.propertyModified', equals: 'Properties.IconPath', label: 'generic item icon storage reported' }] },

  // === CATEGORY ===
  {
    scenario: 'CREATE: create_item_category',
    toolName: 'manage_inventory',
    arguments: { action: 'create_item_category', name: CATEGORY_NAME, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'categoryPath', fromField: 'result.categoryPath' },
    assertions: [{ path: 'structuredContent.result.categoryPath', equals: EXPECTED_CATEGORY_PATH, label: 'category path returned' }]
  },
  { scenario: 'CONNECT: assign_item_category', toolName: 'manage_inventory', arguments: { action: 'assign_item_category', itemPath, categoryPath, save: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.itemPath', equals: EXPECTED_ITEM_PATH, label: 'category assignment item target returned' }, { path: 'structuredContent.result.categoryPath', equals: EXPECTED_CATEGORY_PATH, label: 'category assignment category target returned' }] },

  // === INVENTORY COMPONENT ===
  { scenario: 'CREATE: create_inventory_component', toolName: 'manage_inventory', arguments: { action: 'create_inventory_component', blueprintPath, componentName: 'InventoryComponent', slotCount: 12 }, expected: 'success', assertions: [{ path: 'structuredContent.result.componentName', equals: 'InventoryComponent', label: 'inventory component name returned' }, { path: 'structuredContent.result.componentAdded', equals: true, label: 'inventory component added' }] },
  { scenario: 'CONFIG: configure_inventory_slots', toolName: 'manage_inventory', arguments: { action: 'configure_inventory_slots', blueprintPath, slotCount: 16 }, expected: 'success', assertions: [{ path: 'structuredContent.result.slotCount', equals: 16, label: 'inventory slot count configured' }, { path: 'structuredContent.result.configured', equals: true, label: 'inventory slots configured flag returned' }] },
  { scenario: 'ADD: add_inventory_functions', toolName: 'manage_inventory', arguments: { action: 'add_inventory_functions', blueprintPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.functionsAdded', length: 8, label: 'inventory helper functions/events returned' }, { path: 'structuredContent.result.variablesAdded', length: 5, label: 'inventory helper variables added' }] },
  { scenario: 'CONFIG: configure_inventory_events', toolName: 'manage_inventory', arguments: { action: 'configure_inventory_events', blueprintPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.eventsAdded', length: 4, label: 'inventory events configured' }, { path: 'structuredContent.result.blueprintPath', equals: EXPECTED_BLUEPRINT_PATH, label: 'inventory events target blueprint returned' }] },
  { scenario: 'CONFIG: set_inventory_replication', toolName: 'manage_inventory', arguments: { action: 'set_inventory_replication', blueprintPath, replicated: true, replicationCondition: 'OwnerOnly' }, expected: 'success', assertions: [{ path: 'structuredContent.result.replicated', equals: true, label: 'inventory replication enabled' }, { path: 'structuredContent.result.replicationCondition', equals: 'OwnerOnly', label: 'inventory replication condition applied' }, { path: 'structuredContent.result.modifiedVariables', length: 4, label: 'inventory variables marked for replication' }] },
  { scenario: 'CONFIG: configure_inventory_weight', toolName: 'manage_inventory', arguments: { action: 'configure_inventory_weight', blueprintPath, maxWeight: 125, enableWeight: true, encumberanceSystem: true, encumberanceThreshold: 0.8 }, expected: 'success', assertions: [{ path: 'structuredContent.result.maxWeight', equals: 125, label: 'max inventory weight configured' }, { path: 'structuredContent.result.encumberanceThreshold', equals: 0.8, label: 'encumberance threshold configured' }] },

  // === PICKUPS ===
  {
    scenario: 'CREATE: create_pickup_actor',
    toolName: 'manage_inventory',
    arguments: { action: 'create_pickup_actor', name: PICKUP_NAME, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'pickupPath', fromField: 'result.pickupPath' },
    assertions: [{ path: 'structuredContent.result.blueprintName', equals: PICKUP_NAME, label: 'pickup blueprint name returned' }]
  },
  { scenario: 'CONFIG: configure_pickup_interaction', toolName: 'manage_inventory', arguments: { action: 'configure_pickup_interaction', pickupPath, interactionType: 'Overlap', prompt: 'Collect item' }, expected: 'success', assertions: [{ path: 'structuredContent.result.interactionType', equals: 'Overlap', label: 'pickup interaction type configured' }, { path: 'structuredContent.result.configured', equals: true, label: 'pickup interaction configured flag returned' }] },
  { scenario: 'CONFIG: configure_pickup_respawn', toolName: 'manage_inventory', arguments: { action: 'configure_pickup_respawn', pickupPath, respawnable: true, respawnTime: 45 }, expected: 'success', assertions: [{ path: 'structuredContent.result.respawnable', equals: true, label: 'pickup respawn enabled' }, { path: 'structuredContent.result.respawnTime', equals: 45, label: 'pickup respawn time configured' }] },
  { scenario: 'CONFIG: configure_pickup_effects', toolName: 'manage_inventory', arguments: { action: 'configure_pickup_effects', pickupPath, bobbing: true, rotation: false, glowEffect: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.bobbing', equals: true, label: 'pickup bobbing configured' }, { path: 'structuredContent.result.rotation', equals: false, label: 'pickup rotation configured' }, { path: 'structuredContent.result.glowEffect', equals: true, label: 'pickup glow configured' }] },

  // === EQUIPMENT ===
  { scenario: 'CREATE: create_equipment_component', toolName: 'manage_inventory', arguments: { action: 'create_equipment_component', blueprintPath, componentName: 'EquipmentComponent' }, expected: 'success', assertions: [{ path: 'structuredContent.result.componentName', equals: 'EquipmentComponent', label: 'equipment component name returned' }, { path: 'structuredContent.result.variablesAdded', length: 3, label: 'equipment state variables added' }] },
  { scenario: 'ACTION: define_equipment_slots', toolName: 'manage_inventory', arguments: { action: 'define_equipment_slots', blueprintPath, slots: ['Head', 'Weapon', 'Ring'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.slotCount', equals: 3, label: 'equipment slot count configured' }, { path: 'structuredContent.result.slotsConfigured', length: 3, label: 'equipment slots returned' }] },
  { scenario: 'CONFIG: configure_equipment_effects', toolName: 'manage_inventory', arguments: { action: 'configure_equipment_effects', blueprintPath, statModifiers: true, abilityGrants: false, passiveEffects: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.statModifiersConfigured', equals: true, label: 'equipment stat modifiers configured' }, { path: 'structuredContent.result.abilityGrantsConfigured', equals: false, label: 'equipment ability grants configured' }, { path: 'structuredContent.result.passiveEffectsConfigured', equals: true, label: 'equipment passive effects configured' }] },
  { scenario: 'ADD: add_equipment_functions', toolName: 'manage_inventory', arguments: { action: 'add_equipment_functions', blueprintPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.functionsAdded', length: 9, label: 'equipment helper functions/events returned' }, { path: 'structuredContent.result.variablesAdded', length: 6, label: 'equipment helper variables added' }] },
  { scenario: 'CONFIG: configure_equipment_visuals', toolName: 'manage_inventory', arguments: { action: 'configure_equipment_visuals', blueprintPath, attachToSocket: true, defaultSocket: 'hand_r' }, expected: 'success', assertions: [{ path: 'structuredContent.result.attachToSocket', equals: true, label: 'equipment visual socket attach enabled' }, { path: 'structuredContent.result.defaultSocket', equals: 'hand_r', label: 'equipment default socket configured' }] },

  // === LOOT ===
  {
    scenario: 'CREATE: create_loot_table',
    toolName: 'manage_inventory',
    arguments: { action: 'create_loot_table', name: LOOT_TABLE_NAME, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'lootTablePath', fromField: 'result.lootTablePath' },
    assertions: [{ path: 'structuredContent.result.lootTablePath', equals: EXPECTED_LOOT_TABLE_PATH, label: 'loot table path returned' }]
  },
  { scenario: 'ADD: add_loot_entry', toolName: 'manage_inventory', arguments: { action: 'add_loot_entry', lootTablePath, itemPath, lootWeight: 2.5, minQuantity: 1, maxQuantity: 3 }, expected: 'success', assertions: [{ path: 'structuredContent.result.lootTablePath', equals: EXPECTED_LOOT_TABLE_PATH, label: 'loot entry table target returned' }, { path: 'structuredContent.result.itemPath', equals: EXPECTED_ITEM_PATH, label: 'loot entry item target returned' }, { path: 'structuredContent.result.weight', equals: 2.5, label: 'loot entry weight returned' }, { path: 'structuredContent.result.added', equals: true, label: 'loot entry persisted into table data' }, { path: 'structuredContent.result.storage', equals: 'Properties', label: 'generic loot table storage reported' }] },
  { scenario: 'CONFIG: configure_loot_drop', toolName: 'manage_inventory', arguments: { action: 'configure_loot_drop', actorPath: blueprintPath, lootTablePath, dropCount: 2, dropRadius: 250, dropOnDeath: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.dropCount', equals: 2, label: 'loot drop count configured' }, { path: 'structuredContent.result.dropRadius', equals: 250, label: 'loot drop radius configured' }, { path: 'structuredContent.result.configured', equals: true, label: 'loot drop configured flag returned' }] },
  { scenario: 'CONFIG: set_loot_quality_tiers', toolName: 'manage_inventory', arguments: { action: 'set_loot_quality_tiers', lootTablePath, tiers: [{ name: 'Common', dropWeight: 70 }, { name: 'Rare', dropWeight: 30 }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.tierCount', equals: 2, label: 'loot quality tier count configured' }, { path: 'structuredContent.result.tiersConfigured', length: 2, label: 'loot quality tiers returned' }] },
  { scenario: 'ACTION: remove_loot_entry', toolName: 'manage_inventory', arguments: { action: 'remove_loot_entry', lootTablePath, entryIndex: 0 }, expected: 'success', assertions: [{ path: 'structuredContent.result.lootTablePath', equals: EXPECTED_LOOT_TABLE_PATH, label: 'remove loot entry table target returned' }, { path: 'structuredContent.result.removed', equals: false, label: 'generic loot table reports no removable backing array' }] },

  // === CRAFTING ===
  {
    scenario: 'CREATE: create_crafting_recipe',
    toolName: 'manage_inventory',
    arguments: { action: 'create_crafting_recipe', name: RECIPE_NAME, path: TEST_FOLDER, outputItemPath: itemPath, outputQuantity: 2, craftTime: 4.5 },
    expected: 'success',
    captureResult: { key: 'recipePath', fromField: 'result.recipePath' },
    assertions: [{ path: 'structuredContent.result.outputItemPath', equals: EXPECTED_ITEM_PATH, label: 'crafting recipe output item configured' }, { path: 'structuredContent.result.outputQuantity', equals: 2, label: 'crafting output quantity configured' }]
  },
  { scenario: 'CONFIG: configure_recipe_requirements', toolName: 'manage_inventory', arguments: { action: 'configure_recipe_requirements', recipePath, requiredLevel: 5, requiredStation: 'Workbench' }, expected: 'success', assertions: [{ path: 'structuredContent.result.recipePath', equals: EXPECTED_RECIPE_PATH, label: 'recipe requirements target returned' }, { path: 'structuredContent.result.requiredLevel', equals: 5, label: 'recipe required level configured' }, { path: 'structuredContent.result.requiredStation', equals: 'Workbench', label: 'recipe required station configured' }, { path: 'structuredContent.result.configured', equals: true, label: 'recipe requirements persisted into asset data' }, { path: 'structuredContent.result.propertiesModified', equals: 2, label: 'recipe requirement properties modified' }] },
  { scenario: 'ADD: add_recipe_ingredient', toolName: 'manage_inventory', arguments: { action: 'add_recipe_ingredient', recipePath, ingredientItemPath: itemPath, quantity: 3 }, expected: 'success', assertions: [{ path: 'structuredContent.result.recipePath', equals: EXPECTED_RECIPE_PATH, label: 'recipe ingredient target returned' }, { path: 'structuredContent.result.ingredientItemPath', equals: EXPECTED_ITEM_PATH, label: 'recipe ingredient item returned' }, { path: 'structuredContent.result.quantity', equals: 3, label: 'recipe ingredient quantity returned' }, { path: 'structuredContent.result.added', equals: true, label: 'recipe ingredient persisted into recipe data' }, { path: 'structuredContent.result.storage', equals: 'Properties', label: 'generic recipe ingredient storage reported' }] },
  {
    scenario: 'CREATE: create_crafting_station',
    toolName: 'manage_inventory',
    arguments: { action: 'create_crafting_station', name: STATION_NAME, path: TEST_FOLDER, stationType: 'Workbench' },
    expected: 'success',
    captureResult: { key: 'stationPath', fromField: 'result.stationPath' },
    assertions: [{ path: 'structuredContent.result.stationType', equals: 'Workbench', label: 'crafting station type returned' }]
  },
  { scenario: 'ADD: add_crafting_component', toolName: 'manage_inventory', arguments: { action: 'add_crafting_component', blueprintPath, componentName: 'CraftingComponent' }, expected: 'success', assertions: [{ path: 'structuredContent.result.componentName', equals: 'CraftingComponent', label: 'crafting component name returned' }, { path: 'structuredContent.result.componentAdded', equals: true, label: 'crafting component added' }] },
  { scenario: 'CONFIG: configure_station_recipes', toolName: 'manage_inventory', arguments: { action: 'configure_station_recipes', stationPath, recipePaths: [recipePath], stationType: 'Workbench', craftingSpeedMultiplier: 1.25 }, expected: 'success', assertions: [{ path: 'structuredContent.result.stationType', equals: 'Workbench', label: 'station recipe type configured' }, { path: 'structuredContent.result.recipeCount', equals: 1, label: 'station recipe count configured' }, { path: 'structuredContent.result.recipePaths', length: 1, label: 'station recipe path returned' }] },

  // === INFO ===
  { scenario: 'INFO: get_inventory_info blueprint', toolName: 'manage_inventory', arguments: { action: 'get_inventory_info', blueprintPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.assetType', equals: 'Blueprint', label: 'inventory info reports blueprint asset type' }, { path: 'structuredContent.result.blueprintPath', equals: EXPECTED_BLUEPRINT_PATH, label: 'inventory info returns blueprint path' }, { path: 'structuredContent.result.components', length: 3, label: 'inventory info reports inventory, equipment, and crafting components' }] },
  { scenario: 'INFO: get_inventory_info item', toolName: 'manage_inventory', arguments: { action: 'get_inventory_info', itemPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.assetType', equals: 'Item', label: 'inventory info reports item asset type' }, { path: 'structuredContent.result.itemPath', equals: EXPECTED_ITEM_PATH, label: 'inventory info returns item path' }, { path: 'structuredContent.result.properties.bStackable', equals: 'true', label: 'item stacking flag read back from asset data' }, { path: 'structuredContent.result.properties.MaxStackSize', equals: '25', label: 'item stack size read back from asset data' }, { path: 'structuredContent.result.properties.IconPath', equals: '/Game/MCPTest/FakeInventoryIcon', label: 'item icon path read back from asset data' }] },
  { scenario: 'INFO: get_inventory_info loot table', toolName: 'manage_inventory', arguments: { action: 'get_inventory_info', lootTablePath }, expected: 'success', assertions: [{ path: 'structuredContent.result.assetType', equals: 'LootTable', label: 'inventory info reports loot table asset type' }, { path: 'structuredContent.result.lootTablePath', equals: EXPECTED_LOOT_TABLE_PATH, label: 'inventory info returns loot table path' }, { path: 'structuredContent.result.properties.LootEntry_0', equals: `ItemPath=${EXPECTED_ITEM_PATH};Weight=2.5;MinQuantity=1;MaxQuantity=3`, label: 'loot entry read back from table data' }] },
  { scenario: 'INFO: get_inventory_info recipe', toolName: 'manage_inventory', arguments: { action: 'get_inventory_info', recipePath }, expected: 'success', assertions: [{ path: 'structuredContent.result.assetType', equals: 'Recipe', label: 'inventory info reports recipe asset type' }, { path: 'structuredContent.result.recipePath', equals: EXPECTED_RECIPE_PATH, label: 'inventory info returns recipe path' }, { path: 'structuredContent.result.properties.RequiredLevel', equals: '5', label: 'recipe required level read back from asset data' }, { path: 'structuredContent.result.properties.RequiredStation', equals: 'Workbench', label: 'recipe station read back from asset data' }, { path: 'structuredContent.result.properties.Ingredient_2', equals: `ItemPath=${EXPECTED_ITEM_PATH};Quantity=3`, label: 'recipe ingredient read back from asset data' }] },
  { scenario: 'INFO: get_inventory_info pickup', toolName: 'manage_inventory', arguments: { action: 'get_inventory_info', pickupPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.assetType', equals: 'Pickup', label: 'inventory info reports pickup asset type' }, { path: 'structuredContent.result.pickupPath', equals: EXPECTED_PICKUP_PATH, label: 'inventory info returns pickup path' }] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-inventory', testCases);
