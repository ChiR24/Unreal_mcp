/**
 * Exact per-action schema vocabulary for manage_inventory.
 *
 * Every parameter name below is consumed by a native handler under
 * plugins/McpAutomationBridge/.../Private/Domains/Inventory/ (the
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
  save: P.save,
  properties: P.properties,
  blueprintPath: P.blueprintPath,
  itemPath: P.itemPath,
  lootTablePath: P.lootTablePath,
  categoryPath: str('Canonical /Game item category asset path.'),
  replicated: bool('Whether the inventory component replicates.'),
  replicationCondition: str('Replication condition name applied to inventory state.'),
  pickupPath: str('Canonical /Game pickup Blueprint asset path.'),
  lootWeight: num('Relative selection weight for the loot entry.'),
  minQuantity: num('Minimum quantity granted by the loot entry.'),
  maxQuantity: num('Maximum quantity granted by the loot entry.'),
  entryIndex: num('Zero-based loot entry index to remove.'),
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
    description: 'Quality tiers, each with a name and a dropWeight.',
  },
  outputItemPath: str('Canonical /Game item asset path produced by the recipe.'),
  outputQuantity: num('Quantity produced per craft.'),
  craftTime: num('Craft duration in seconds.'),
  recipePath: str('Canonical /Game crafting recipe asset path.'),
  requiredLevel: num('Minimum character level required to craft.'),
  requiredStation: str('Crafting station type required by the recipe.'),
  ingredientItemPath: str('Canonical /Game ingredient item asset path.'),
  quantity: num('Ingredient quantity consumed per craft.'),
  stationType: str('Crafting station type.'),
  recipePaths: P.arrayOfStrings,
  iconPath: str('Canonical /Game texture or material path used as the item icon.'),
  stackable: bool('Whether the item stacks.'),
  maxStackSize: num('Maximum stack size.'),
  uniqueItems: bool('Whether duplicate instances of the item are disallowed.'),
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
    whenToUse: [],
    whenNotToUse: [],
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
