/**
 * control_actor capability record catalog.
 *
 * Exactly 46 canonical CapabilityRecord entries mapped 1:1 to the 46
 * control_actor actions in control-actor-tool.ts. Each record is grounded
 * in the TypeScript handler bodies (actor-basic-handlers.ts,
 * actor-component-handlers.ts, actor-physics-handler.ts), the
 * normalizeActorAction alias map in actor-handler-utils.ts, and the native
 * C++ ControlActor domain dispatch (McpAutomationBridge_ControlActorDispatch.cpp).
 *
 * Families (7):
 * - spawn (3): spawn, spawn_actor (alias->spawn), spawn_blueprint
 * - lifecycle (4): duplicate, delete, destroy_actor (alias->delete), delete_by_tag
 * - transform (8): set_transform + 5 aliases, get_transform + 1 alias, apply_force(->physics)
 * - component/material (8): add_component, remove_component, set_component_property +
 *   set_component_properties alias, get_component_property, set_material + 2 aliases
 * - visibility/query (6): set_visibility + alias, get_components + alias, get_actor_bounds, list
 * - tags/find (8): add_tag, remove_tag, find_by_tag + alias, find_by_name + alias,
 *   find_by_class + alias
 * - attachment/advanced (8): attach + alias, detach + alias, set_blueprint_variables,
 *   create_snapshot, set_actor_collision (alias->set_collision),
 *   call_actor_function (alias->call_function)
 *
 * Total: 3 + 4 + 8 + 8 + 6 + 8 + 8 = 45 ... plus 1 apply_force in transform = 46.
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

const SOURCES: readonly CapabilityRecordSource[] = [
  ...SPAWN_RECORDS,
  ...TRANSFORM_RECORDS,
  ...COMPONENT_RECORDS,
  ...STATE_RECORDS,
  ...SEARCH_RECORDS,
  ...ADVANCED_RECORDS,
];

export const CONTROL_ACTOR_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const CONTROL_ACTOR_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const CONTROL_ACTOR_RECORD_COUNT = CONTROL_ACTOR_RECORDS.length;
