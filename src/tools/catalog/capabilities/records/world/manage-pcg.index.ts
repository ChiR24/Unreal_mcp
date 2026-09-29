/**
 * manage_pcg capability record catalog.
 *
 * 30 authored CapabilityRecordSource entries in manage_pcg action-enum
 * (PCG_ACTIONS) order -- 29 synchronous graph-authoring operations plus the
 * async execute_pcg_graph entry (returns a taskId) -- folded by MANAGE_PCG_FOLDS
 * into the shipped records (counted by ALL_CAPABILITY_RECORD_COUNT in records/aggregate.ts).
 * Every authored action stays callable as a folded legacy pair. Every record
 * requires the PCG optional plugin and is grounded in the world tool definition
 * and native PCG domain dispatch.
 */
import type { CapabilityRecordSource } from '../../model.js';

import { PCG_GRAPH_RECORDS } from './manage-pcg.graph.data.js';
import { PCG_ASYNC_RECORDS } from './manage-pcg.async.data.js';
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
];

export const MANAGE_PCG_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_PCG_UNFOLDED_SOURCES, MANAGE_PCG_FOLDS, 'manage_pcg');

