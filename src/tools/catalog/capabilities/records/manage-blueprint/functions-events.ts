/**
 * Functions and events are Blueprint graph members with distinct lifecycle:
 * add_function creates a function graph; add_event creates an event node in
 * the EventGraph. Both return the member name for subsequent graph operations.
 */
import type { CapabilityRecordSource } from '../../model.js';
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
    topics: ['new function', 'custom function', 'function graph', 'define function'],
    summary: 'Add a new function graph to a Blueprint with optional inputs and outputs; an existing function of that name is reused only when it matches the pure, inputs and outputs given.',
    whenToUse: ['A new callable function must be created on the Blueprint.'],
    whenNotToUse: ['An event handler is needed (use add_event).'],
    inputProps: {
      blueprintPath: P.blueprintPath, functionName: P.functionName, inputs: P.inputs, outputs: P.outputs,
      isPublic: { type: 'boolean', description: 'Access specifier: true public (the default), false private (callable only from this Blueprint).' },
      pure: { type: 'boolean', description: 'Pure function (the Details panel\'s Pure checkbox): its call nodes have no exec pins. Default false.' },
    },
    required: ['blueprintPath', 'functionName'],
    // add_function is the first member of its fold, so nodeGuid's description
    // speaks for add_event too.
    outputProps: {
      functionName: P.functionName,
      nodeGuid: { type: 'string', description: 'Node guid: the function\'s entry node, or the event node. A built-in event that already exists in the graph may be bound without one.' },
      resultNodeGuid: { type: 'string', description: 'The function\'s return node, present when the function has one (it is made when outputs are declared).' },
    },
    outputRequired: ['functionName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_function', blueprintPath: '/Game/Blueprints/BP_Test', functionName: 'CalculateDamage', inputs: [{ name: 'BaseDamage', type: 'Float' }], outputs: [{ name: 'Result', type: 'Float' }], pure: true },
    exampleOutput: { success: true, functionName: 'CalculateDamage', nodeGuid: '8C1D0E6A4F2B4C1D9E0F1A2B3C4D5E6F', resultNodeGuid: '1A2B3C4D5E6F40718293A4B5C6D7E8F9' },
  }),
  buildRecord({
    id: 'blueprint.remove_function',
    action: 'remove_function',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Permanently remove a function graph from a Blueprint.',
    whenToUse: ['A function must be permanently deleted from the Blueprint.'],
    whenNotToUse: ['The function should be renamed rather than removed.'],
    inputProps: { blueprintPath: P.blueprintPath, functionName: P.functionName },
    required: ['blueprintPath', 'functionName'],
    effect: 'destructive',
    behavior: { safeToRetry: false },
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'remove_function', blueprintPath: '/Game/Blueprints/BP_Test', functionName: 'OldFunction' },
  }),
  buildRecord({
    id: 'blueprint.add_event',
    action: 'add_event',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['custom event', 'event node', 'begin play', 'tick event', 'event graph'],
    summary: 'Add an event node to a Blueprint event graph: built-in, custom, or bound to a component delegate (componentName plus eventName, e.g. OnComponentBeginOverlap on a BoxComponent).',
    whenToUse: ['An event handler node must be created in the EventGraph.', 'A component event such as OnComponentBeginOverlap or OnComponentHit must be handled.'],
    whenNotToUse: ['A callable function is needed (use add_function).'],
    inputProps: {
      blueprintPath: P.blueprintPath, eventType: P.eventType, customEventName: P.customEventName, posX: P.posX, posY: P.posY, parameters: P.parameters,
      graphName: { ...P.graphName, description: 'Event graph page to add the event to; omitted, the main EventGraph.' },
      eventName: { ...P.eventName, description: 'Custom event name; with componentName, the component delegate to bind (OnComponentBeginOverlap, OnComponentHit, ...).' },
      componentName: { ...P.componentName, description: 'Component whose delegate fires the event, added by the Blueprint or inherited (a Character\'s CapsuleComponent). Makes a component-bound event (K2Node_ComponentBoundEvent) named by eventName.' },
    },
    required: ['blueprintPath'],
    outputProps: {
      nodeGuid: { type: 'string', description: 'Event node identifier. Returned for custom events; the built-in-event path may bind an event that already exists in the graph and reports no new node.' },
      eventName: P.eventName,
    },
    // NOT required. The built-in path (e.g. ReceiveActorBeginOverlap) creates or
    // binds the event without reporting a guid, so demanding one turned a
    // successful call into OUTPUT_SCHEMA_VIOLATION -- the node was really added
    // and only the receipt was rejected, which reads as "the operation failed".
    outputRequired: [],
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
    summary: 'Remove an event node from a Blueprint event graph by node id, or a custom event by name.',
    whenToUse: ['An event node must be permanently removed from the graph.'],
    whenNotToUse: ['The event should be reconnected rather than removed.'],
    inputProps: {
      blueprintPath: P.blueprintPath, nodeId: P.nodeId,
      graphName: { ...P.graphName, description: 'Event graph page to search; omitted, the main EventGraph.' },
      eventName: { ...P.eventName, description: 'Custom event to remove by name, in place of nodeId.' },
    },
    required: ['blueprintPath'],
    requiredOneOf: ['nodeId', 'eventName'],
    effect: 'destructive',
    behavior: { safeToRetry: false },
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'remove_event', blueprintPath: '/Game/Blueprints/BP_Test', graphName: 'EventGraph', nodeId: 'V1W2X3Y4' },
  }),
];
