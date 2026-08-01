import type { CapabilityCatalog, CapabilityRecord, CapabilityRecordSource } from '../../index.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../parser.js';
import { ANIMATION_PHYSICS_SOURCES } from './animation-physics/index.js';
import { GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS } from './hidden-routes.js';
import { MANAGE_AI_SOURCES } from './manage-ai/index.js';
import { MANAGE_CHARACTER_SOURCES } from './manage-character/index.js';
import { MANAGE_COMBAT_SOURCES } from './manage-combat/index.js';
import { MANAGE_EFFECT_SOURCES } from './manage-effect/index.js';
import { MANAGE_GAS_SOURCES } from './manage-gas/index.js';
import { MANAGE_INTERACTION_SOURCES } from './manage-interaction/index.js';
import { MANAGE_INVENTORY_SOURCES } from './manage-inventory/index.js';
import { compareById as compareCanonicalIds } from '../../../../../utils/serialization/ordering.js';

export { GAMEPLAY_HIDDEN_ROUTE_DISPOSITIONS };

export const GAMEPLAY_AGGREGATE_COUNT = 356 as const;

export const GAMEPLAY_PARENT_RECORDS: Readonly<Record<string, readonly CapabilityRecordSource[]>> = Object.freeze({
  animation_physics: ANIMATION_PHYSICS_SOURCES,
  manage_effect: MANAGE_EFFECT_SOURCES,
  manage_gas: MANAGE_GAS_SOURCES,
  manage_character: MANAGE_CHARACTER_SOURCES,
  manage_combat: MANAGE_COMBAT_SOURCES,
  manage_ai: MANAGE_AI_SOURCES,
  manage_inventory: MANAGE_INVENTORY_SOURCES,
  manage_interaction: MANAGE_INTERACTION_SOURCES,
});

export const GAMEPLAY_SOURCE_RECORDS: readonly CapabilityRecordSource[] = Object.freeze([
  ...ANIMATION_PHYSICS_SOURCES,
  ...MANAGE_EFFECT_SOURCES,
  ...MANAGE_GAS_SOURCES,
  ...MANAGE_CHARACTER_SOURCES,
  ...MANAGE_COMBAT_SOURCES,
  ...MANAGE_AI_SOURCES,
  ...MANAGE_INVENTORY_SOURCES,
  ...MANAGE_INTERACTION_SOURCES,
]);

const GAMEPLAY_BUILT_RECORDS: readonly CapabilityRecord[] = (() => {
  const parsed = parseCapabilityCatalog(GAMEPLAY_SOURCE_RECORDS.map(createCapabilityRecord));
  const uniqueIdCount = new Set(parsed.map((record) => record.id)).size;
  if (parsed.length !== GAMEPLAY_AGGREGATE_COUNT || uniqueIdCount !== GAMEPLAY_AGGREGATE_COUNT) {
    throw new TypeError(
      `Gameplay capability aggregate requires ${GAMEPLAY_AGGREGATE_COUNT} unique records; `
      + `received ${parsed.length} records and ${uniqueIdCount} unique IDs`,
    );
  }
  return Object.freeze([...parsed].sort(compareCanonicalIds));
})();

export const GAMEPLAY_CAPABILITY_CATALOG: CapabilityCatalog = GAMEPLAY_BUILT_RECORDS;
export const GAMEPLAY_CAPABILITY_RECORD_COUNT = GAMEPLAY_BUILT_RECORDS.length;
