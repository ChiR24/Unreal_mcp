/**
 * manage_pcg capability record catalog.
 *
 * Authored graph, parameter, execution and data-view contracts in stable
 * definition order, folded by MANAGE_PCG_FOLDS into the shipped records
 * (counted by ALL_CAPABILITY_RECORD_COUNT in records/aggregate.ts).
 * Native generation scheduling IDs and gateway task IDs are different identities.
 * Every authored action stays callable as a folded legacy pair. Every record
 * requires the PCG optional plugin and is grounded in the world tool definition
 * and native PCG domain dispatch.
 */
import type { CapabilityRecordSource } from '../../model.js';

import { PCG_GRAPH_RECORDS } from './manage-pcg.graph.data.js';
import { PCG_ASYNC_RECORDS } from './manage-pcg.async.data.js';
import { PCG_READ_RECORDS } from './manage-pcg.read.data.js';
import { PCG_MUTATION_RECORDS } from './manage-pcg.mutations.data.js';
import { PCG_PARAMETER_RECORDS } from './manage-pcg.parameters.data.js';
import { PCG_EXECUTION_RECORDS } from './manage-pcg.execution.data.js';
import { PCG_DATA_VIEW_RECORDS } from './manage-pcg.data-views.data.js';
import { PCG_NODE_SCHEMA_RECORDS } from './manage-pcg.node-schema.data.js';
import { applyFolds } from '../shared/fold.js';
import { MANAGE_PCG_FOLDS } from '../folds/manage-pcg.folds.js';

// Records are emitted in the exact legacy manage_pcg action-enum order. The
// data shards below are authored in definition order, so concatenating them
// preserves that order verbatim. Keep that historical order: the generated
// registry sorts by id, so re-sorting here only reshuffles generated output.
/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_PCG_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...PCG_GRAPH_RECORDS,
  ...PCG_ASYNC_RECORDS,
  ...PCG_READ_RECORDS,
  ...PCG_MUTATION_RECORDS,
  ...PCG_PARAMETER_RECORDS,
  ...PCG_EXECUTION_RECORDS,
  ...PCG_DATA_VIEW_RECORDS,
  ...PCG_NODE_SCHEMA_RECORDS,
];

export const MANAGE_PCG_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_PCG_UNFOLDED_SOURCES, MANAGE_PCG_FOLDS, 'manage_pcg');

