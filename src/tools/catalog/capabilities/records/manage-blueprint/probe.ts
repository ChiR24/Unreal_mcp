/**
 * Probe handle record: probe_handle.
 *
 * probe_handle validates that a Blueprint asset handle is reachable and
 * optionally applies a batch of operations. It is the only action that
 * accepts an operations array for batch dispatch.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

export const PROBE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.probe_handle',
    action: 'probe_handle',
    family: 'probe',
    domain: 'blueprint',
    summary: 'Probe a Blueprint handle for reachability and optionally apply a batch of operations.',
    whenToUse: ['A Blueprint handle must be validated before a sequence of operations.'],
    whenNotToUse: ['A single operation is needed (call it directly).'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, operations: P.operations },
    required: ['action', 'blueprintPath'],
    outputProps: {
      reachable: { type: 'boolean', description: 'Whether the Blueprint handle is reachable.' },
      results: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Per-operation results when operations are provided.', 'x-unreal-reflection-boundary': true },
    },
    outputRequired: ['reachable'],
    effect: 'read',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'probe_handle', blueprintPath: '/Game/Blueprints/BP_Test' },
    exampleOutput: { success: true, reachable: true, results: [] },
  }),
];
