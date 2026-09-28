/**
 * build_graph: many graph edits in one call. Each step is an ordinary member edit
 * (add_variable, add_function, add_event, add_event_dispatcher) or node edit
 * (create_node, connect_pins, set_pin_default_value, set_node_property,
 * create_reroute_node), run in-process by the same single-step handler, so
 * wiring an event chain no longer costs a round trip per node, link and pin.
 * Steps name the nodes they create with `id` and later steps refer to them as
 * "$id"; `nodeIds` maps each id to its real node guid for follow-up calls.
 * McpBlueprintBehaviour (plugin Domains/BlueprintGraph/Behaviour) authors its
 * recipes through this same batch.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

const ITEM = { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true } as const;

export const GRAPH_BATCH_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.build_graph',
    action: 'build_graph',
    family: 'graph',
    domain: 'blueprint',
    topics: ['batch graph edit', 'build event graph', 'wire many nodes', 'blueprint graph batch', 'function with body'],
    summary: 'Run many graph edits in one call: add variables, functions (with their bodies), events and event dispatchers, create nodes, connect pins, set pin defaults and node properties, with $id references between steps.',
    whenToUse: ['More than a couple of nodes or links must be added to one Blueprint graph.', 'A new function and the nodes inside it must be built in one call.'],
    whenNotToUse: ['Nodes or links must be deleted (use delete_node; destructive edits are not batched).'],
    inputProps: {
      blueprintPath: P.blueprintPath,
      graphName: { type: 'string', description: 'Graph every step targets unless the step names its own graphName (default EventGraph).' },
      operations: {
        type: 'array',
        items: ITEM,
        'x-unreal-reflection-boundary': true,
        description: 'Steps run in order, 1-200. Each is {edit, ...that edit\'s own params}. Member steps come before the nodes '
          + 'that use them: add_variable (variableName, variableType, defaultValue, isPublic, category), add_function '
          + '(functionName, inputs, outputs, pure, isPublic; its body steps set graphName to the function name; with an id, "$id" '
          + 'is its entry node and "$id_return" its return node, which exists when outputs are declared), add_event '
          + '(customEventName and parameters, or componentName plus eventName for a component delegate) and add_event_dispatcher '
          + '(dispatcherName, parameters [{name, type}]). Node steps: create_node, connect_pins, set_pin_default_value, '
          + 'set_node_property, create_reroute_node. create_node also takes nodeType CallDelegate (memberName: a dispatcher), '
          + 'Message (memberClass: an interface, memberName: its function), AsyncTask (memberClass: the task or async-action '
          + 'class, memberName: its static factory; event graphs only) and FunctionResult (one more return node in a function '
          + 'graph). Optional per step: id (names the created node; later steps use "$id" in fromNodeId, toNodeId and nodeId), '
          + 'from and to ("$id.PinName" shorthand for connect_pins), pinDefaults (create_node only: {PinName: value} applied '
          + 'to the new node; a read-only pin such as Set Text\'s Value gets a MakeLiteral node wired into it). "$entry" is '
          + 'the graph\'s own entry node (a Construction Script or function graph starts there: from "$entry.then"). A create '
          + 'step without posX and posY is auto-placed. Every function, variable, dispatcher and async factory a step names is '
          + 'checked before any step runs, so a misspelled one fails the batch with nothing applied; any other failure stops '
          + 'the batch at that step, and that step leaves nothing behind, in whichever graph it ran.',
      },
    },
    required: ['blueprintPath', 'operations'],
    outputProps: {
      results: { type: 'array', items: ITEM, 'x-unreal-reflection-boundary': true, description: 'Per-step outcome: index, edit, id, success, error, nodeGuid, resultNodeGuid (add_function), pins (for created nodes), connected, appliedValue.' },
      nodeIds: { type: 'object', additionalProperties: { type: 'string' }, description: 'Step id -> node guid for every node the batch created or reused; an add_function step with an id also adds "<id>_return" for its return node.' },
      succeeded: { type: 'number', description: 'Steps that completed.' },
      failedIndex: { type: 'number', description: 'Index of the step that stopped the batch (failures only).' },
      compiled: { type: 'boolean', description: 'True when the Blueprint compiles after the batch (warnings allowed).' },
      compilerStatus: { type: 'string', description: 'Blueprint status after the final compile.' },
      diagnostics: { type: 'array', items: ITEM, 'x-unreal-reflection-boundary': true, description: 'Compile messages: { severity, message }.' },
      saved: { type: 'boolean', description: 'Whether the Blueprint was saved after the batch.' },
    },
    outputRequired: [],
    effect: 'write',
    behavior: { idempotency: 'non-idempotent', safeToRetry: false },
    latency: 'interactive',
    resources: 'medium',
    plugins: BP_PLUGINS,
    exampleInput: {
      action: 'build_graph',
      blueprintPath: '/Game/Blueprints/BP_Test',
      graphName: 'EventGraph',
      operations: [
        { edit: 'create_node', id: 'begin', nodeType: 'Event', eventName: 'BeginPlay' },
        { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'Hello' } },
        { edit: 'connect_pins', from: '$begin.then', to: '$print.execute' },
      ],
    },
    exampleOutput: {
      success: true,
      message: 'Ran 3 graph operations; the blueprint compiles.',
      nodeIds: { begin: '8C1D0E6A4F2B4C1D9E0F1A2B3C4D5E6F', print: '1A2B3C4D5E6F40718293A4B5C6D7E8F9' },
      succeeded: 3,
      compiled: true,
      saved: true,
    },
  }),
];
