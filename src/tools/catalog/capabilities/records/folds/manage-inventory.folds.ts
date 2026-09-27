// Fold specs for manage_inventory. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_INVENTORY_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_inventory_asset', selector: 'kind',
    summary: 'Create an inventory asset: item data asset, item category, loot table, crafting recipe, crafting station.',
    topics: ['item data asset', 'item category', 'loot table', 'crafting recipe', 'crafting station'],
    members: { item_data_asset: 'create_item_data_asset', item_category: 'create_item_category', loot_table: 'create_loot_table', crafting_recipe: 'create_crafting_recipe', crafting_station: 'create_crafting_station' },
  },
  {
    primary: 'configure_item', selector: 'setting',
    summary: 'Configure an item: properties, icon, stacking, category.',
    topics: ['item properties', 'item icon', 'item stacking', 'item category'],
    members: { properties: 'set_item_properties', icon: 'set_item_icon', stacking: 'configure_item_stacking', category: 'assign_item_category' },
  },
  {
    primary: 'configure_loot', selector: 'setting',
    summary: 'Configure a loot table: add or remove entries, set quality tiers.',
    topics: ['loot entry', 'loot quality'],
    members: { add_entry: 'add_loot_entry', remove_entry: 'remove_loot_entry', quality_tiers: 'set_loot_quality_tiers' },
  },
  {
    primary: 'configure_crafting', selector: 'setting',
    summary: 'Configure a crafting recipe: add ingredients or set its requirements.',
    topics: ['recipe ingredient', 'recipe requirements'],
    members: { add_recipe_ingredient: 'add_recipe_ingredient', recipe_requirements: 'configure_recipe_requirements' },
  },
];
