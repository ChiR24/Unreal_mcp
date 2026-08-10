/**
 * manage_geometry capability record catalog.
 *
 * Exactly 76 canonical CapabilityRecordSource entries mapped 1:1 to the
 * manage_geometry action enum (primitives, booleans/operations, deform,
 * mirror/array/optimize/UV/normals/collision/Nanite, get_mesh_info), all in
 * definition order. Each record requires the GeometryScripting plugin and is
 * grounded in the world tool definition and native Geometry domain dispatch.
 */
import type { CapabilityRecordSource } from '../../index.js';

import { GEOMETRY_PRIMITIVES_RECORDS } from './manage-geometry.primitives.data.js';
import { GEOMETRY_OPERATIONS_RECORDS } from './manage-geometry.operations.data.js';
import { GEOMETRY_DEFORM_RECORDS } from './manage-geometry.deform.data.js';
import { GEOMETRY_OPTIMIZE_RECORDS } from './manage-geometry.optimize.data.js';

// Records are emitted in the exact legacy manage_geometry action-enum order.
// The data shards below are authored in definition order (primitives, then
// operations/deform/optimize), so concatenating them preserves that order
// verbatim. Do NOT re-sort: the record order is a contractual parity assertion
// against consolidatedToolDefinitions (see tests/unit/world-capability-records.test.ts),
// not a free-standing ordering.
export const MANAGE_GEOMETRY_SOURCES: readonly CapabilityRecordSource[] = [
  ...GEOMETRY_PRIMITIVES_RECORDS,
  ...GEOMETRY_OPERATIONS_RECORDS,
  ...GEOMETRY_DEFORM_RECORDS,
  ...GEOMETRY_OPTIMIZE_RECORDS,
];

export const MANAGE_GEOMETRY_RECORDS: readonly CapabilityRecordSource[] = MANAGE_GEOMETRY_SOURCES;

export const MANAGE_GEOMETRY_RECORD_COUNT = MANAGE_GEOMETRY_RECORDS.length;
