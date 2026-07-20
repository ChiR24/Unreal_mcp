/**
 * inspect capability record catalog.
 *
 * Exactly 36 canonical CapabilityRecord entries mapped 1:1 to the 36 inspect
 * actions in inspect-tool.ts. Each record is grounded in the TypeScript
 * inspect handlers (src/tools/handlers/inspect/), native Inspect domain
 * dispatch (Private/Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspect.cpp),
 * and the normalization inventory (all 36 inspect actions cataloged;
 * get_project_settings is the primary of cap:shared:get_project_settings,
 * class A shared with system_control).
 *
 * Families:
 * - object (7): inspect_object, get_actor_details, get_blueprint_details,
 *   get_mesh_details, get_texture_details, get_material_details,
 *   get_level_details
 * - property (2): get_property, set_property
 * - class (3): inspect_class, inspect_cdo, inspect_struct
 * - components (4): get_components, get_component_property,
 *   set_component_property, get_component_details
 * - actor (8): get_metadata, add_tag, find_by_tag, export, delete_object,
 *   list_objects, find_by_class, get_bounding_box
 * - snapshot (2): create_snapshot, restore_snapshot
 * - runtime (2): runtime_report, pie_report
 * - global (5): get_project_settings, get_world_settings, get_viewport_info,
 *   get_selected_actors, get_editor_settings
 * - stats (3): get_scene_stats, get_performance_stats, get_memory_stats
 *
 * Total: 7 + 2 + 3 + 4 + 8 + 2 + 2 + 5 + 3 = 36
 *
 * TS/native/action mismatches surfaced in record normalization rationale
 * rather than normalized away: find_by_tag (TS control_actor vs native
 * global), get_component_details (dispatches get_components to control_actor),
 * get_level_details (aliases to get_world_settings), get_blueprint_details
 * (separate blueprint_get route).
 */
import { type CapabilityRecord, type CapabilityRecordSource, createCapabilityRecord } from '../../index.js';

import { COMPONENT_ACTOR_RECORDS } from './component-actor.data.js';
import { GLOBAL_RUNTIME_RECORDS } from './global-runtime.data.js';
import { OBJECT_PROPERTY_RECORDS } from './object-property.data.js';

/**
 * Record order is the authored data-file concatenation; this module does not
 * re-derive an action order.
 */
const SOURCES: readonly CapabilityRecordSource[] = [
  ...OBJECT_PROPERTY_RECORDS,
  ...COMPONENT_ACTOR_RECORDS,
  ...GLOBAL_RUNTIME_RECORDS,
];

export const INSPECT_SOURCES: readonly CapabilityRecordSource[] = SOURCES;

export const INSPECT_RECORDS: readonly CapabilityRecord[] = SOURCES.map(
  (source) => createCapabilityRecord(source),
);

export const INSPECT_RECORD_COUNT = INSPECT_RECORDS.length;
