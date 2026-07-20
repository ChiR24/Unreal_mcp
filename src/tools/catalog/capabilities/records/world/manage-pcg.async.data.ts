/**
 * PCG async execution family records (1 action): execute_pcg_graph.
 *
 * The only asynchronous PCG action. It dispatches graph generation as a
 * game-thread task and returns a numeric taskId (an opaque scheduling receipt,
 * not a completion marker). It is long-running and not safe to retry
 * (re-execution regenerates actors). Per the native PCG handler
 * (McpAutomationBridge_PCGHandlersComponentActions.cpp) the leaf emits taskId
 * as a number and NO cancellation state and NO poll action. When createComponent
 * is false the caller must supply a component selector (actorName/
 * componentName/componentPath). Requires the PCG optional plugin.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'pcg-execution';
const NR = 'Distinct manage_pcg async execution verb and target; no cross-tool duplicate.';
const PLUGIN = ['PCG'] as const;

export const PCG_ASYNC_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'execute_pcg_graph', plugins: PLUGIN,
    family: F, summary: 'Execute a PCG graph asynchronously, returning a numeric task identifier.', whenToUse: ['A PCG graph must be generated and the caller needs the scheduling task id.'], whenNotToUse: ['Graph authoring is needed; use add_pcg_node or connect_pcg_pins.'],
    inputProps: {
      graphPath: P.graphPath, actorName: P.actorName, componentName: P.componentName,
      componentPath: P.componentPath, createComponent: P.createComponent, force: P.force, save: P.save,
      timeoutMs: P.timeoutMs,
    }, required: ['graphPath'], effect: 'write',
    behavior: { longRunning: true, safeToRetry: false }, costLatency: 'long-running', costResources: 'high',
    exampleInput: { action: 'execute_pcg_graph', graphPath: '/Game/PCG/PCG_MyGraph', createComponent: true, save: true },
    exampleOutput: { success: true, message: 'PCG graph execution started', taskId: 5231 },
    outputProps: {
      taskId: { type: 'number', description: 'Opaque numeric scheduling receipt for the PCG task.' },
    },
    normalizationRationale: NR,
  }),
];
