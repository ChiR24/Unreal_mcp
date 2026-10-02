/**
 * manage_geometry capability record catalog.
 *
 * 88 authored CapabilityRecordSource entries -- primitives 15, operations 21,
 * deform 13, optimize 28, dynamicmesh 11 -- in manage_geometry action-enum
 * order, folded by MANAGE_GEOMETRY_FOLDS into the shipped records (counted by
 * ALL_CAPABILITY_RECORD_COUNT in records/aggregate.ts). Every authored action stays
 * callable as a folded legacy pair. Each record requires the GeometryScripting
 * plugin and is grounded in the world tool definition and native Geometry
 * domain dispatch.
 */
import type { CapabilityRecordSource } from '../../model.js';

import { GEOMETRY_PRIMITIVES_RECORDS } from './manage-geometry.primitives.data.js';
import { GEOMETRY_OPERATIONS_RECORDS } from './manage-geometry.operations.data.js';
import { GEOMETRY_DEFORM_RECORDS } from './manage-geometry.deform.data.js';
import { GEOMETRY_OPTIMIZE_RECORDS } from './manage-geometry.optimize.data.js';
import { GEOMETRY_DYNAMICMESH_RECORDS } from './manage-geometry.dynamicmesh.data.js';
import { applyFolds } from '../shared/fold.js';
import { MANAGE_GEOMETRY_FOLDS } from '../folds/manage-geometry.folds.js';

// Records are emitted in the exact legacy manage_geometry action-enum order.
// The data shards below are authored in definition order (primitives, then
// operations/deform/optimize), so concatenating them preserves that order
// verbatim. Keep that historical order: the generated registry sorts by id, so
// re-sorting here only reshuffles generated output.
/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_GEOMETRY_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...GEOMETRY_PRIMITIVES_RECORDS,
  ...GEOMETRY_OPERATIONS_RECORDS,
  ...GEOMETRY_DEFORM_RECORDS,
  ...GEOMETRY_OPTIMIZE_RECORDS,
  ...GEOMETRY_DYNAMICMESH_RECORDS,
];

export const MANAGE_GEOMETRY_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_GEOMETRY_UNFOLDED_SOURCES, MANAGE_GEOMETRY_FOLDS, 'manage_geometry');

