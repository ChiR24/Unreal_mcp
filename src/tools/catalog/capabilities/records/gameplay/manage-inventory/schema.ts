/**
 * Exact per-action schema vocabulary for manage_inventory.
 *
 * Every parameter name below is consumed by a native handler under
 * plugins/EngineWire/.../Private/Domains/Inventory/ (the
 * GetPayloadString/Number/Bool reads in the *Handlers*.cpp shards). Nothing is
 * declared that no handler reads, which is why `actorName` and an input
 * `assetPath` are absent: no inventory sub-action reads either one.
 *
 * A parameter name maps to exactly ONE fragment object and every action reuses
 * it, so the record-only parent derivation collapses the union to a single
 * shape instead of emitting a `oneOf`
 * (see scripts/canonical-registry/schema-merge.ts).
 */
import type { CapabilityRecordSource, JsonObject } from '../../../model.js';
import { buildRecord } from '../helpers.js';
import { P } from '../properties.js';
import type { PropertyMap } from '../properties.js';
import { str, num, bool } from '../../shared/schema-props.js';


/** Inventory parameter vocabulary, keyed by the exact payload field name. */
export const IP: PropertyMap = {
  name: P.name,
  path: P.path,
  save: bool('Save the asset to disk after the change (default true, for every inventory action). With false the change stays in memory only and is lost when the editor restarts.'),
  properties: P.properties,
  blueprintPath: P.blueprintPath,
  itemPath: P.itemPath,
  lootTablePath: P.lootTablePath,
  categoryPath: str('Item category asset, as create_inventory_asset kind=item_category makes it; one that names no asset is refused (ASSET_NOT_FOUND).'),
  replicated: bool('true: the inventory variables replicate under replicationCondition. false, the default when omitted: they stop replicating.'),
  replicationCondition: str('When they replicate: an ELifetimeCondition name with or without COND_ (None, the default, OwnerOnly, SkipOwner, SimulatedOnly, InitialOnly ...). Ignored when replicated is false; an unknown name is refused.'),
  pickupPath: str('Canonical /Game pickup Blueprint asset path.'),
  lootWeight: num('Chance of this entry relative to the others in the table, 0 or more (default 1).'),
  minQuantity: num('Fewest the entry grants, 0 or more (default 1).'),
  maxQuantity: num('Most the entry grants, at least minQuantity (default minQuantity, or 1).'),
  entryIndex: num('Number n of the LootEntry_<n> key of the entry to remove: the entryIndex add_loot_entry returned, or the n in a LootEntry_<n> key of get_inventory_info properties. It is a key suffix, not a position: removing an entry renumbers nothing, and a new entry takes one past the highest suffix in use.'),
  tiers: {
    type: 'array',
    items: {
      type: 'object',
      additionalProperties: false,
      properties: {
        name: { type: 'string', description: 'Quality tier name.' },
        dropWeight: { type: 'number', description: 'Relative drop weight for the tier.' },
      },
      required: ['name', 'dropWeight'],
    },
    description: 'Quality tiers, each {name, dropWeight}: a name without commas or equals signs and a weight of 0 or more. A malformed entry or an empty list is refused; omitted, the five defaults are written (Common 60, Uncommon 25, Rare 10, Epic 4, Legendary 1). Replaces the tiers already there.',
  },
  outputItemPath: str('Item asset the recipe produces; one that names no asset is refused (ASSET_NOT_FOUND).'),
  outputQuantity: num('Items produced per craft (default 1).'),
  craftTime: num('Craft duration in seconds (default 1).'),
  recipePath: str('Canonical /Game crafting recipe asset path.'),
  requiredLevel: num('Minimum character level to craft, 0 or more. Only the requirement fields sent are written.'),
  requiredStation: str('Crafting station the recipe needs, stored as given (a station type such as Forge). Only the requirement fields sent are written.'),
  ingredientItemPath: str('Ingredient item asset; one that names no asset is refused (ASSET_NOT_FOUND).'),
  quantity: num('How many of the ingredient one craft consumes, 1 or more (default 1).'),
  stationType: str('Crafting station type.'),
  recipePaths: P.arrayOfStrings,
  iconPath: str('Texture or material asset used as the item icon; it must exist (ASSET_NOT_FOUND). An empty string clears the icon.'),
  stackable: bool('Whether the item stacks. Only the stacking fields sent are written.'),
  maxStackSize: num('Most items one stack holds, 1 or more. Only the stacking fields sent are written.'),
  uniqueItems: bool('Whether duplicate instances of the item are disallowed (bUniqueItem). Only the stacking fields sent are written.'),
};

export interface InventoryActionSpec {
  readonly action: string;
  readonly summary: string;
  readonly topics?: readonly string[];
  readonly inputProps: PropertyMap;
  readonly required?: readonly string[];
  readonly requiredOneOf?: readonly string[];
  readonly exampleInput: JsonObject;
  /** Only get_inventory_info reads without writing. */
  readonly read?: boolean;
  readonly outputProps?: PropertyMap;
  readonly outputRequired?: readonly string[];
  readonly whenToUse?: readonly string[];
  readonly whenNotToUse?: readonly string[];
}

/**
 * Build one manage_inventory record. Non-input facets (output handle, editor
 * state, cost, effect) are held identical to the previous compact records so
 * the 33 records and their route metadata stay intact; only the input contract
 * becomes exact.
 */
export function inventoryRecord(spec: InventoryActionSpec): CapabilityRecordSource {
  return buildRecord({
    parentTool: 'manage_inventory',
    id: `manage_inventory.${spec.action}`,
    action: spec.action,
    family: 'inventory',
    summary: spec.summary,
    topics: spec.topics,
    whenToUse: [...(spec.whenToUse ?? [])],
    whenNotToUse: [...(spec.whenNotToUse ?? [])],
    inputProps: { ...spec.inputProps },
    required: [...(spec.required ?? [])],
    requiredOneOf: spec.requiredOneOf,
    outputProps: spec.outputProps ?? { assetPath: P.assetPath },
    outputRequired: spec.outputRequired ?? [],
    effect: spec.read === true ? 'read' : 'write',
    latency: 'interactive',
    resources: 'medium',
    editorStates: ['edit'],
    exampleInput: spec.exampleInput,
    exampleOutput: { success: true, message: `${spec.action} handled` },
  });
}
