/**
 * manage_inventory exact-schema record contract.
 *
 * The expected parameter sets below are transcribed from the native shards
 * under plugins/McpAutomationBridge/.../Private/Domains/Inventory/ — a field is
 * expected here only where a GetPayloadString/Number/Bool or TryGet*Field call
 * reads it for that sub-action.
 */
import { describe, expect, it } from 'vitest';
import type { CapabilityRecordSource } from '../../../index.js';
import { MANAGE_INVENTORY_SOURCES } from './index.js';

const records = MANAGE_INVENTORY_SOURCES;
const byAction = new Map<string, CapabilityRecordSource>(
  records.map((r) => [r.legacyIds[0].action, r]),
);

const inputProps = (action: string): Record<string, unknown> =>
  byAction.get(action)?.schemas.input.properties ?? {};
const requiredOf = (action: string): readonly string[] =>
  byAction.get(action)?.schemas.input.required ?? [];

/** Exactly the fields each native sub-action reads. */
const EXPECTED: Readonly<Record<string, readonly string[]>> = {
  add_crafting_component: ['blueprintPath', 'componentName', 'save'],
  add_equipment_functions: ['blueprintPath', 'save'],
  add_inventory_functions: ['blueprintPath', 'save'],
  add_loot_entry: ['lootTablePath', 'itemPath', 'lootWeight', 'minQuantity', 'maxQuantity', 'save'],
  add_recipe_ingredient: ['recipePath', 'ingredientItemPath', 'quantity', 'save'],
  assign_item_category: ['itemPath', 'categoryPath', 'save'],
  configure_equipment_effects: ['blueprintPath', 'statModifiers', 'abilityGrants', 'passiveEffects', 'save'],
  configure_equipment_visuals: ['blueprintPath', 'attachToSocket', 'defaultSocket', 'save'],
  configure_inventory_events: ['blueprintPath', 'save'],
  configure_inventory_slots: ['blueprintPath', 'slotCount', 'save'],
  configure_inventory_weight: [
    'blueprintPath', 'maxWeight', 'enableWeight', 'encumberanceSystem', 'encumberanceThreshold', 'save',
  ],
  configure_item_stacking: ['itemPath', 'stackable', 'maxStackSize', 'uniqueItems', 'save'],
  configure_loot_drop: ['actorPath', 'lootTablePath', 'dropCount', 'dropRadius', 'dropOnDeath', 'save'],
  configure_pickup_effects: ['pickupPath', 'bobbing', 'rotation', 'glowEffect', 'save'],
  configure_pickup_interaction: ['pickupPath', 'interactionType', 'prompt', 'save'],
  configure_pickup_respawn: ['pickupPath', 'respawnable', 'respawnTime', 'save'],
  configure_recipe_requirements: ['recipePath', 'requiredLevel', 'requiredStation', 'save'],
  configure_station_recipes: ['stationPath', 'recipePaths', 'stationType', 'craftingSpeedMultiplier', 'save'],
  create_crafting_recipe: ['name', 'path', 'outputItemPath', 'outputQuantity', 'craftTime', 'save'],
  create_crafting_station: ['name', 'path', 'stationType', 'save'],
  create_equipment_component: ['blueprintPath', 'componentName', 'save'],
  create_inventory_component: ['blueprintPath', 'componentName', 'slotCount', 'save'],
  create_item_category: ['name', 'path', 'save'],
  create_item_data_asset: ['name', 'path', 'save'],
  create_loot_table: ['name', 'path', 'save'],
  create_pickup_actor: ['name', 'path', 'save'],
  define_equipment_slots: ['blueprintPath', 'slots', 'save'],
  get_inventory_info: ['blueprintPath', 'itemPath', 'lootTablePath', 'recipePath', 'pickupPath'],
  remove_loot_entry: ['lootTablePath', 'entryIndex', 'itemPath', 'save'],
  set_inventory_replication: ['blueprintPath', 'replicated', 'replicationCondition', 'save'],
  set_item_icon: ['itemPath', 'iconPath', 'save'],
  set_item_properties: ['itemPath', 'properties', 'save'],
  set_loot_quality_tiers: ['lootTablePath', 'tiers', 'save'],
};

/** Mirrors the requireNonEmptyString/requireAssetName guards in the TS handler. */
const REQUIRED: Readonly<Record<string, readonly string[]>> = {
  add_loot_entry: ['lootTablePath', 'itemPath'],
  add_recipe_ingredient: ['recipePath', 'ingredientItemPath'],
  assign_item_category: ['itemPath', 'categoryPath'],
  configure_loot_drop: ['actorPath', 'lootTablePath'],
  create_crafting_recipe: ['name', 'outputItemPath'],
  create_item_data_asset: ['name'],
  get_inventory_info: [],
  set_item_properties: ['itemPath'],
};

describe('manage_inventory capability records', () => {
  it('keeps all 33 records with unique canonical ids', () => {
    expect(records).toHaveLength(33);
    expect(new Set(records.map((r) => r.id)).size).toBe(33);
    expect(Object.keys(EXPECTED)).toHaveLength(33);
  });

  it('preserves route metadata for every record', () => {
    for (const record of records) {
      const action = record.legacyIds[0].action;
      expect(record.id).toBe(`manage_inventory.${action}`);
      expect(record.legacyIds[0].tool).toBe('manage_inventory');
      expect(record.routing.parentTool).toBe('manage_inventory');
      expect(record.routing.dispatchAction).toBe(action);
      expect(record.routing.dispatchMode).toBe('tool');
    }
  });

  it('declares exactly the parameters the native handler reads', () => {
    for (const [action, expected] of Object.entries(EXPECTED)) {
      expect(Object.keys(inputProps(action)).sort()).toEqual([...expected].sort());
    }
  });

  it('marks the handler-guarded parameters required', () => {
    for (const [action, expected] of Object.entries(REQUIRED)) {
      expect([...requiredOf(action)].sort()).toEqual([...expected].sort());
    }
  });

  it('drops the generic fields no inventory action consumes', () => {
    for (const record of records) {
      const props = Object.keys(record.schemas.input.properties);
      expect(props).not.toContain('actorName');
      expect(props).not.toContain('assetPath');
      expect(props).not.toContain('action');
    }
  });

  it('preserves the creation output handle on every record', () => {
    for (const record of records) {
      expect(Object.keys(record.schemas.output.properties)).toContain('assetPath');
    }
  });

  it('keeps get_inventory_info the only read action', () => {
    const reads = records.filter((r) => r.behavior.effect === 'read').map((r) => r.legacyIds[0].action);
    expect(reads).toEqual(['get_inventory_info']);
  });

  it('gives each parameter name a single shape so parent derivation emits no oneOf', () => {
    const shapes = new Map<string, Set<string>>();
    for (const record of records) {
      for (const [name, shape] of Object.entries(record.schemas.input.properties)) {
        const seen = shapes.get(name) ?? new Set<string>();
        seen.add(JSON.stringify(shape));
        shapes.set(name, seen);
      }
    }
    const conflicting = [...shapes.entries()].filter(([, seen]) => seen.size > 1).map(([name]) => name);
    expect(conflicting).toEqual([]);
    expect(shapes.size).toBe(55);
  });
});
