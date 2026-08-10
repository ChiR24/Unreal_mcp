/**
 * manage_pcg capability record catalog.
 *
 * Exactly 30 canonical CapabilityRecordSource entries mapped 1:1 to the
 * manage_pcg action enum (PCG_ACTIONS), in definition order. 29 are
 * synchronous graph-authoring operations; execute_pcg_graph is the async
 * execution entry (returns a taskId). Every record requires the PCG optional
 * plugin and is grounded in the world tool definition and native PCG domain
 * dispatch.
 */
import type { CapabilityRecordSource } from '../../index.js';

import { PCG_GRAPH_RECORDS } from './manage-pcg.graph.data.js';
import { PCG_ASYNC_RECORDS } from './manage-pcg.async.data.js';

// Records are emitted in the exact legacy manage_pcg action-enum order. The
// data shards below are authored in definition order, so concatenating them
// preserves that order verbatim. Do NOT re-sort: the record order is a
// contractual parity assertion against consolidatedToolDefinitions (see
// tests/unit/world-capability-records.test.ts), not a free-standing ordering.
export const MANAGE_PCG_SOURCES: readonly CapabilityRecordSource[] = [
  ...PCG_GRAPH_RECORDS,
  ...PCG_ASYNC_RECORDS,
];

export const MANAGE_PCG_RECORDS: readonly CapabilityRecordSource[] = MANAGE_PCG_SOURCES;

export const MANAGE_PCG_RECORD_COUNT = MANAGE_PCG_RECORDS.length;
