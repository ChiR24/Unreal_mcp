/**
 * control_actor capability record catalog.
 *
 * 47 authored CapabilityRecordSource entries, folded by CONTROL_ACTOR_FOLDS
 * into the 22 shipped records that carry 49 callable legacy {tool, action}
 * pairs. Each record is grounded in the TypeScript handler bodies
 * (actor-basic-handlers.ts, actor-component-handlers.ts,
 * actor-physics-handler.ts), the normalizeActorAction alias map in
 * actor-handler-utils.ts, and the native C++ ControlActor domain dispatch
 * (McpAutomationBridge_ControlActorDispatch.cpp).
 *
 * Authored families (6 files, 47 records):
 * - spawn/lifecycle (7): spawn, spawn_actor (alias->spawn), spawn_blueprint,
 *   duplicate, delete, destroy_actor (alias->delete), delete_by_tag
 * - transform (9): set_transform + 5 aliases, get_transform + 1 alias, apply_force
 * - component/material (8): add_component, remove_component, set_component_property +
 *   set_component_properties alias, get_component_property, set_material + 2 aliases
 * - visibility/query (6): set_visibility + alias, get_components + alias,
 *   get_actor_bounds, list
 * - tags/find (9): add_tag, remove_tag, find_by_tag + alias, find_by_name + alias,
 *   find_by_class + alias, audit_placement
 * - attachment/advanced (8): attach + alias, detach + alias, set_blueprint_variables,
 *   create_snapshot, set_actor_collision (alias->set_collision),
 *   call_actor_function (alias->call_function)
 *
 * The counts above are pinned by control-actor-records.test.ts; do not restate
 * them anywhere else.
 *
 * All records preserve the normalization inventory's C/keep classification as
 * C_SAME_VERB_DIFFERENT_TARGET/retain. Runtime aliases remain visible in each
 * record's routing and rationale. Record order is the authored family-file
 * concatenation below; this module does not re-derive an action order.
 */
import type { CapabilityRecord, CapabilityRecordSource } from '../../index.js';
import { createCapabilityRecord } from '../../index.js';

import { ADVANCED_RECORDS } from './advanced-records.js';
import { COMPONENT_RECORDS } from './component-records.js';
import { SEARCH_RECORDS } from './search-records.js';
import { SPAWN_RECORDS } from './spawn-records.js';
import { STATE_RECORDS } from './state-records.js';
import { TRANSFORM_RECORDS } from './transform-records.js';
import { applyFolds } from '../shared/fold.js';
import { CONTROL_ACTOR_FOLDS } from '../folds/control-actor.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const CONTROL_ACTOR_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...SPAWN_RECORDS,
  ...TRANSFORM_RECORDS,
  ...COMPONENT_RECORDS,
  ...STATE_RECORDS,
  ...SEARCH_RECORDS,
  ...ADVANCED_RECORDS,
];

const SOURCES: readonly CapabilityRecordSource[] = applyFolds(CONTROL_ACTOR_UNFOLDED_SOURCES, CONTROL_ACTOR_FOLDS, 'control_actor');

export const CONTROL_ACTOR_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const CONTROL_ACTOR_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const CONTROL_ACTOR_RECORD_COUNT = CONTROL_ACTOR_RECORDS.length;
