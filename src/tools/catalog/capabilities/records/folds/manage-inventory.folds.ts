// Fold specs for manage_inventory. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_INVENTORY_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_inventory_asset', selector: 'kind',
    summary: 'Create an inventory asset, chosen by kind. item_data_asset, item_category, loot_table and crafting_recipe make generic data assets: the configure actions store their fields and get_inventory_info reads them back. They are data only, for gameplay to read; nothing runs on its own. crafting_station makes an Actor Blueprint with a StationMesh and an InteractionBox. name goes in path (defaults /Game/Items, /Game/Items/Categories, /Game/Data/LootTables, /Game/Data/Recipes and /Game/Blueprints/CraftingStations). An asset already there is refused (ASSET_ALREADY_EXISTS), never replaced. A recipe needs outputItemPath, an existing item. Saved unless save is false (saved).',
    topics: ['item data asset', 'item category', 'loot table', 'crafting recipe', 'crafting station', 'make an item', 'add an inventory or items'],
    members: { item_data_asset: 'create_item_data_asset', item_category: 'create_item_category', loot_table: 'create_loot_table', crafting_recipe: 'create_crafting_recipe', crafting_station: 'create_crafting_station' },
  },
  {
    primary: 'configure_item', selector: 'setting',
    summary: 'Change an item asset, chosen by setting. properties writes each field of properties: a typed property of a custom item class, else a field of the generic item; failedProperties lists any that did not take, and none taking is INVALID_ARGUMENT. icon sets iconPath, an existing texture or material (empty clears it). stacking writes stackable, maxStackSize and uniqueItems, only the ones sent. category files the item under categoryPath, an existing category asset. A missing item or reference is ASSET_NOT_FOUND. Saved unless save is false (saved).',
    topics: ['item properties', 'item icon', 'item stacking', 'item category'],
    members: { properties: 'set_item_properties', icon: 'set_item_icon', stacking: 'configure_item_stacking', category: 'assign_item_category' },
  },
  {
    primary: 'configure_loot', selector: 'setting',
    summary: 'Edit a loot table made by create_inventory_asset, chosen by setting. add_entry adds itemPath (an existing item) with lootWeight (default 1) and a quantity range (default 1 to 1) as LootEntry_<n>, returning n as entryIndex. remove_entry drops the entry with that entryIndex, or every entry for itemPath (removedCount; no match is NOT_FOUND). quality_tiers replaces the tiers with tiers ({name, dropWeight}) or the five defaults. Data only: the game reads the table, and nothing rolls loot by itself. Saved unless save is false (saved).',
    topics: ['loot entry', 'loot quality', 'loot drop rate'],
    members: { add_entry: 'add_loot_entry', remove_entry: 'remove_loot_entry', quality_tiers: 'set_loot_quality_tiers' },
  },
  {
    primary: 'configure_crafting', selector: 'setting',
    summary: 'Edit a crafting recipe made by create_inventory_asset, chosen by setting. add_recipe_ingredient adds ingredientItemPath (an existing item), consumed quantity at a time (default 1), as Ingredient_<n>. recipe_requirements writes requiredLevel and requiredStation, only the ones sent. A recipe of a custom class is refused, so set its own fields with inspect.set_property. Saved unless save is false (saved).',
    topics: ['recipe ingredient', 'recipe requirements'],
    members: { add_recipe_ingredient: 'add_recipe_ingredient', recipe_requirements: 'configure_recipe_requirements' },
  },
];
