// src/tools/catalog/capabilities/records/aggregate.ts
//
// Every canonical capability record, in AUTHORED order: the canonical-registry
// generator derives each parent's action enum from this first-seen sequence,
// so an id-sorted feed would alphabetise the generated enums.

import type { CapabilityRecord, CapabilityRecordSource } from '../model.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../parser.js';
import { BUILD_ENVIRONMENT_SOURCES } from './build-environment/index.js';
import { CONTROL_ACTOR_SOURCES } from './control-actor/index.js';
import { CONTROL_EDITOR_SOURCES } from './control-editor/index.js';
import { ANIMATION_PHYSICS_SOURCES } from './gameplay/animation-physics/index.js';
import { MANAGE_AI_SOURCES } from './gameplay/manage-ai/records.js';
import { MANAGE_CHARACTER_SOURCES } from './gameplay/manage-character/index.js';
import { MANAGE_COMBAT_SOURCES } from './gameplay/manage-combat/index.js';
import { MANAGE_EFFECT_SOURCES } from './gameplay/manage-effect/index.js';
import { MANAGE_GAS_SOURCES } from './gameplay/manage-gas/index.js';
import { MANAGE_INTERACTION_SOURCES } from './gameplay/manage-interaction/index.js';
import { MANAGE_INVENTORY_SOURCES } from './gameplay/manage-inventory/index.js';
import { INSPECT_SOURCES } from './inspect/index.js';
import { MANAGE_ASSET_SOURCES } from './manage-asset/index.js';
import { MANAGE_AUDIO_SOURCES } from './manage-audio/index.js';
import { MANAGE_BLUEPRINT_SOURCES } from './manage-blueprint/index.js';
import { MANAGE_LEVEL_SOURCES } from './manage-level/index.js';
import { MANAGE_NETWORKING_SOURCES } from './manage-networking/index.js';
import { MANAGE_SEQUENCE_SOURCES } from './manage-sequence/index.js';
import { MANAGE_TOOLS_SOURCES } from './manage-tools/index.js';
import { SYSTEM_CONTROL_SOURCES } from './system-control/index.js';
import { MANAGE_GEOMETRY_SOURCES } from './world/manage-geometry.index.js';
import { MANAGE_LEVEL_STRUCTURE_SOURCES } from './world/manage-level-structure.index.js';
import { MANAGE_PCG_SOURCES } from './world/manage-pcg.index.js';

export const ALL_CAPABILITY_RECORD_COUNT = 407 as const;

const ALL_SOURCES: readonly CapabilityRecordSource[] = [
  // world
  ...BUILD_ENVIRONMENT_SOURCES,
  ...MANAGE_LEVEL_STRUCTURE_SOURCES,
  ...MANAGE_GEOMETRY_SOURCES,
  ...MANAGE_PCG_SOURCES,
  // gameplay
  ...ANIMATION_PHYSICS_SOURCES,
  ...MANAGE_EFFECT_SOURCES,
  ...MANAGE_GAS_SOURCES,
  ...MANAGE_CHARACTER_SOURCES,
  ...MANAGE_COMBAT_SOURCES,
  ...MANAGE_AI_SOURCES,
  ...MANAGE_INVENTORY_SOURCES,
  ...MANAGE_INTERACTION_SOURCES,
  // utility
  ...MANAGE_SEQUENCE_SOURCES,
  ...MANAGE_AUDIO_SOURCES,
  ...MANAGE_NETWORKING_SOURCES,
  // core
  ...MANAGE_ASSET_SOURCES,
  ...MANAGE_BLUEPRINT_SOURCES,
  ...CONTROL_ACTOR_SOURCES,
  ...CONTROL_EDITOR_SOURCES,
  ...MANAGE_LEVEL_SOURCES,
  ...SYSTEM_CONTROL_SOURCES,
  ...INSPECT_SOURCES,
  ...MANAGE_TOOLS_SOURCES,
];

export const ALL_CAPABILITY_RECORDS: readonly CapabilityRecord[] = (() => {
  const parsed = parseCapabilityCatalog(ALL_SOURCES.map(createCapabilityRecord));
  const uniqueIdCount = new Set(parsed.map((record) => record.id)).size;
  if (parsed.length !== ALL_CAPABILITY_RECORD_COUNT || uniqueIdCount !== ALL_CAPABILITY_RECORD_COUNT) {
    throw new TypeError(
      `Capability record aggregate must contain exactly ${ALL_CAPABILITY_RECORD_COUNT} `
      + `records and unique IDs; received ${parsed.length} records and ${uniqueIdCount} unique IDs`,
    );
  }
  return Object.freeze(parsed);
})();
