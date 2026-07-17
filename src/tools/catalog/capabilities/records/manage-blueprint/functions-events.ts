/**
 * Function and event records: add_function, remove_function, add_event,
 * remove_event.
 *
 * Functions and events are Blueprint graph members with distinct lifecycle:
 * add_function creates a function graph; add_event creates an event node in
 * the EventGraph. Both return the member name for subsequent graph operations.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'functions';
const DOMAIN = 'blueprint';

export const FUNCTIONS_EVENTS_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.add_function',
    action: 'add_function',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a new function graph to a Blueprint with optional inputs and outputs.',
    whenToUse: ['A new callable function must be created on the Blueprint.'],
    whenNotToUse: ['An event handler is needed (use add_event).'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, functionName: P.functionName, inputs: P.inputs, outputs: P.outputs },
    required: ['action', 'blueprintPath', 'functionName'],
    outputProps: { functionName: P.functionName },
    outputRequired: ['functionName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_function', blueprintPath: '/Game/Blueprints/BP_Test', functionName: 'CalculateDamage', inputs: [{ name: 'BaseDamage', type: 'Float' }], outputs: [{ name: 'Result', type: 'Float' }] },
    exampleOutput: { success: true, functionName: 'CalculateDamage' },
  }),
  buildRecord({
    id: 'blueprint.remove_function',
    action: 'remove_function',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Permanently remove a function graph from a Blueprint.',
    whenToUse: ['A function must be permanently deleted from the Blueprint.'],
    whenNotToUse: ['The function should be renamed rather than removed.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, functionName: P.functionName },
    required: ['action', 'blueprintPath', 'functionName'],
    effect: 'destructive',
    behavior: { safeToRetry: false, supportsUndo: false },
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'remove_function', blueprintPath: '/Game/Blueprints/BP_Test', functionName: 'OldFunction' },
    exampleOutput: { success: true, message: 'Function removed' },
  }),
  buildRecord({
    id: 'blueprint.add_event',
    action: 'add_event',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add an event node (built-in or custom) to the EventGraph of a Blueprint.',
    whenToUse: ['An event handler node must be created in the EventGraph.'],
    whenNotToUse: ['A callable function is needed (use add_function).'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, graphName: P.graphName, eventType: P.eventType, eventName: P.eventName, customEventName: P.customEventName, posX: P.posX, posY: P.posY },
    required: ['action', 'blueprintPath'],
    outputProps: {
      nodeGuid: { type: 'string', description: 'Event node identifier.' },
      eventName: P.eventName,
    },
    outputRequired: ['nodeGuid'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_event', blueprintPath: '/Game/Blueprints/BP_Test', graphName: 'EventGraph', eventType: 'CustomEvent', customEventName: 'OnDamaged', posX: 0, posY: 0 },
    exampleOutput: { success: true, nodeGuid: 'V1W2X3Y4', eventName: 'OnDamaged' },
  }),
  buildRecord({
    id: 'blueprint.remove_event',
    action: 'remove_event',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Remove an event node from a Blueprint graph by nodeGuid.',
    whenToUse: ['An event node must be permanently removed from the graph.'],
    whenNotToUse: ['The event should be reconnected rather than removed.'],
    inputProps: { action: P.action, blueprintPath: P.blueprintPath, graphName: P.graphName, nodeId: P.nodeId },
    required: ['action', 'blueprintPath', 'nodeId'],
    effect: 'destructive',
    behavior: { safeToRetry: false, supportsUndo: false },
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'remove_event', blueprintPath: '/Game/Blueprints/BP_Test', graphName: 'EventGraph', nodeId: 'V1W2X3Y4' },
    exampleOutput: { success: true, message: 'Event removed' },
  }),
];
