/**
 * Probe handle record: probe_handle.
 *
 * probe_handle checks, without loading it, that a Blueprint asset handle is
 * reachable. It declared an `operations` batch that no handler ever ran;
 * batched graph edits go through edit_graph with edit "batch".
 */
import type { CapabilityRecordSource } from '../../model.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

export const PROBE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.probe_handle',
    action: 'probe_handle',
    family: 'probe',
    domain: 'blueprint',
    summary: 'Probe a Blueprint handle for reachability without loading the asset; reports its asset class.',
    whenToUse: ['A Blueprint handle must be validated before a sequence of operations.'],
    whenNotToUse: ['Graph edits must be batched (use edit_graph with edit batch).'],
    inputProps: { blueprintPath: P.blueprintPath },
    required: ['blueprintPath'],
    outputProps: {
      reachable: { type: 'boolean', description: 'Whether the Blueprint handle is reachable.' },
      exists: { type: 'boolean', description: 'Whether the asset exists.' },
      path: { type: 'string', description: 'Normalized asset path that was checked.' },
      assetClass: { type: 'string', description: 'Asset class from the registry, when it exists.' },
    },
    outputRequired: ['reachable'],
    effect: 'read',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'probe_handle', blueprintPath: '/Game/Blueprints/BP_Test' },
    exampleOutput: { success: true, reachable: true, exists: true, path: '/Game/Blueprints/BP_Test', assetClass: 'Blueprint' },
  }),
];
