// Fold specs for manage_inventory. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_INVENTORY_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_inventory_asset', selector: 'kind',
    summary: 'Create an inventory asset: item data asset, item category, loot table, crafting recipe, crafting station, pickup actor.',
    topics: ['item data asset', 'item category', 'loot table', 'crafting recipe', 'crafting station', 'pickup actor'],
    members: { item_data_asset: 'create_item_data_asset', item_category: 'create_item_category', loot_table: 'create_loot_table', crafting_recipe: 'create_crafting_recipe', crafting_station: 'create_crafting_station', pickup_actor: 'create_pickup_actor' },
  },
  {
    primary: 'configure_inventory', selector: 'setting',
    summary: 'Set up inventory on a Blueprint: create the component, add functions, configure slots, weight, events and replication.',
    topics: ['inventory component', 'inventory slots', 'inventory weight', 'inventory replication', 'create inventory'],
    members: { create_component: 'create_inventory_component', add_functions: 'add_inventory_functions', slots: 'configure_inventory_slots', weight: 'configure_inventory_weight', events: 'configure_inventory_events', replication: 'set_inventory_replication' },
  },
  {
    primary: 'configure_equipment', selector: 'setting',
    summary: 'Set up equipment on a Blueprint: create the component, add functions, define slots, configure visuals and effects.',
    topics: ['equipment component', 'equipment slots', 'equipment visuals', 'equipment effects'],
    members: { create_component: 'create_equipment_component', add_functions: 'add_equipment_functions', define_slots: 'define_equipment_slots', visuals: 'configure_equipment_visuals', effects: 'configure_equipment_effects' },
  },
  {
    primary: 'configure_item', selector: 'setting',
    summary: 'Configure an item: properties, icon, stacking, category.',
    topics: ['item properties', 'item icon', 'item stacking', 'item category'],
    members: { properties: 'set_item_properties', icon: 'set_item_icon', stacking: 'configure_item_stacking', category: 'assign_item_category' },
  },
  {
    primary: 'configure_loot', selector: 'setting',
    summary: 'Configure loot: add or remove loot table entries, set quality tiers, configure drops on an actor.',
    topics: ['loot entry', 'loot drop', 'loot quality'],
    members: { add_entry: 'add_loot_entry', remove_entry: 'remove_loot_entry', quality_tiers: 'set_loot_quality_tiers', drop: 'configure_loot_drop' },
  },
  {
    primary: 'configure_pickup', selector: 'setting',
    summary: 'Configure a pickup actor: interaction, effects, respawn.',
    topics: ['pickup interaction', 'pickup effects', 'pickup respawn'],
    members: { interaction: 'configure_pickup_interaction', effects: 'configure_pickup_effects', respawn: 'configure_pickup_respawn' },
  },
  {
    primary: 'configure_crafting', selector: 'setting',
    summary: 'Configure crafting: add the component, add recipe ingredients, set recipe requirements, assign station recipes.',
    topics: ['crafting component', 'recipe ingredient', 'recipe requirements', 'station recipes'],
    members: { add_component: 'add_crafting_component', add_recipe_ingredient: 'add_recipe_ingredient', recipe_requirements: 'configure_recipe_requirements', station_recipes: 'configure_station_recipes' },
  },
];
