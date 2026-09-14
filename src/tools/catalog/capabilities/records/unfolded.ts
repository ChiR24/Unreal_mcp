// src/tools/catalog/capabilities/records/unfolded.ts
//
// Every authored record BEFORE folding, parsed and hashed. The shipped catalog
// (aggregate.ts) folds sibling records into families; the per-action contract
// tests keep pinning handler-level facts (required parameters, output fields,
// effects, dispatch) against these unfolded records, which is where those facts
// are authored. Not consumed at runtime.

import { createCapabilityRecord } from '../parser.js';
import type { CapabilityRecord, CapabilityRecordSource } from '../model.js';
import { ANIMATION_PHYSICS_UNFOLDED_SOURCES } from './gameplay/animation-physics/index.js';
import { BUILD_ENVIRONMENT_UNFOLDED_SOURCES } from './build-environment/index.js';
import { CONTROL_ACTOR_UNFOLDED_SOURCES } from './control-actor/index.js';
import { CONTROL_EDITOR_UNFOLDED_SOURCES } from './control-editor/index.js';
import { INSPECT_UNFOLDED_SOURCES } from './inspect/index.js';
import { MANAGE_AI_UNFOLDED_SOURCES } from './gameplay/manage-ai/records.js';
import { MANAGE_ASSET_UNFOLDED_SOURCES } from './manage-asset/index.js';
import { MANAGE_AUDIO_UNFOLDED_SOURCES } from './manage-audio/index.js';
import { MANAGE_BLUEPRINT_UNFOLDED_SOURCES } from './manage-blueprint/index.js';
import { MANAGE_CHARACTER_UNFOLDED_SOURCES } from './gameplay/manage-character/index.js';
import { MANAGE_COMBAT_UNFOLDED_SOURCES } from './gameplay/manage-combat/index.js';
import { MANAGE_EFFECT_UNFOLDED_SOURCES } from './gameplay/manage-effect/index.js';
import { MANAGE_GAS_UNFOLDED_SOURCES } from './gameplay/manage-gas/index.js';
import { MANAGE_GEOMETRY_UNFOLDED_SOURCES } from './world/manage-geometry.index.js';
import { MANAGE_INTERACTION_UNFOLDED_SOURCES } from './gameplay/manage-interaction/index.js';
import { MANAGE_INVENTORY_UNFOLDED_SOURCES } from './gameplay/manage-inventory/index.js';
import { MANAGE_LEVEL_STRUCTURE_UNFOLDED_SOURCES } from './world/manage-level-structure.index.js';
import { MANAGE_LEVEL_UNFOLDED_SOURCES } from './manage-level/index.js';
import { MANAGE_NETWORKING_UNFOLDED_SOURCES } from './manage-networking/index.js';
import { MANAGE_PCG_UNFOLDED_SOURCES } from './world/manage-pcg.index.js';
import { MANAGE_SEQUENCE_UNFOLDED_SOURCES } from './manage-sequence/index.js';
import { MANAGE_TOOLS_SOURCES } from './manage-tools/index.js';
import { SYSTEM_CONTROL_UNFOLDED_SOURCES } from './system-control/index.js';

export const ALL_UNFOLDED_CAPABILITY_SOURCES: readonly CapabilityRecordSource[] = Object.freeze([
  ...BUILD_ENVIRONMENT_UNFOLDED_SOURCES,
  ...MANAGE_LEVEL_STRUCTURE_UNFOLDED_SOURCES,
  ...MANAGE_GEOMETRY_UNFOLDED_SOURCES,
  ...MANAGE_PCG_UNFOLDED_SOURCES,
  ...ANIMATION_PHYSICS_UNFOLDED_SOURCES,
  ...MANAGE_EFFECT_UNFOLDED_SOURCES,
  ...MANAGE_GAS_UNFOLDED_SOURCES,
  ...MANAGE_CHARACTER_UNFOLDED_SOURCES,
  ...MANAGE_COMBAT_UNFOLDED_SOURCES,
  ...MANAGE_AI_UNFOLDED_SOURCES,
  ...MANAGE_INVENTORY_UNFOLDED_SOURCES,
  ...MANAGE_INTERACTION_UNFOLDED_SOURCES,
  ...MANAGE_SEQUENCE_UNFOLDED_SOURCES,
  ...MANAGE_AUDIO_UNFOLDED_SOURCES,
  ...MANAGE_NETWORKING_UNFOLDED_SOURCES,
  ...MANAGE_ASSET_UNFOLDED_SOURCES,
  ...MANAGE_BLUEPRINT_UNFOLDED_SOURCES,
  ...CONTROL_ACTOR_UNFOLDED_SOURCES,
  ...CONTROL_EDITOR_UNFOLDED_SOURCES,
  ...MANAGE_LEVEL_UNFOLDED_SOURCES,
  ...SYSTEM_CONTROL_UNFOLDED_SOURCES,
  ...INSPECT_UNFOLDED_SOURCES,
  ...MANAGE_TOOLS_SOURCES,
]);

export const ALL_UNFOLDED_CAPABILITY_RECORDS: readonly CapabilityRecord[] = Object.freeze(
  ALL_UNFOLDED_CAPABILITY_SOURCES.map((source) => createCapabilityRecord(source)),
);
