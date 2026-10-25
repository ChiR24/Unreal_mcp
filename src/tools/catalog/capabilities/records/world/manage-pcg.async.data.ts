/**
 * PCG async execution family records (1 action): execute_pcg_graph.
 *
 * The only asynchronous PCG action. It dispatches graph generation as a
 * game-thread task and returns a numeric taskId (an opaque scheduling receipt,
 * not a completion marker). It is long-running and not safe to retry
 * (re-execution regenerates actors). Per the native PCG handler
 * (McpPCGComponentActions.cpp) the leaf emits taskId
 * as a number and NO cancellation state and NO poll action. When createComponent
 * is false the caller must supply a component selector (actorName/
 * componentName/componentPath). Requires the PCG optional plugin.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'pcg-execution';
const PLUGIN = ['PCG'] as const;

export const PCG_ASYNC_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'execute_pcg_graph', topics: ['run pcg graph', 'generate pcg'], plugins: PLUGIN,
    family: F, summary: 'Generate a PCG graph on a placed PCG component: the one actorName, componentName or componentPath names, or with createComponent a new one on actorName (one it already has is used; createdComponent says which). graphPath becomes the component\'s graph and generation is scheduled to run over the next frames (force, default true, regenerates an up-to-date component); read PCGComponent.bGenerated with inspect get_property to see it finished (partitioned actors keep it false). GENERATION_NOT_SCHEDULED when it is already generating, has no graph or has invalid bounds; save also saves the level.', whenToUse: ['A PCG graph must be generated in the level.'], whenNotToUse: ['Graph authoring is needed; use add_pcg_node or connect_pcg_pins.'],
    inputProps: {
      graphPath: P.graphPath, actorName: P.actorName, componentName: P.componentName,
      componentPath: P.componentPath, createComponent: P.createComponent, force: P.force, save: P.save,
    }, required: ['graphPath'], effect: 'write',
    behavior: { longRunning: true, safeToRetry: false }, costLatency: 'long-running', costResources: 'high',
    exampleInput: { action: 'execute_pcg_graph', graphPath: '/Game/PCG/PCG_MyGraph', createComponent: true, save: true },
    exampleOutput: { success: true, message: 'PCG graph execution started', taskId: 5231 },
    outputProps: {
      taskId: { type: 'number', description: 'The PCG scheduler\'s id for the generation task (nothing here polls it; read bGenerated instead).' },
    },
  }),
];
