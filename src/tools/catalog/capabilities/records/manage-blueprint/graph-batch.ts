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
    summary: 'Run many graph edits in one call: add variables, functions (with their bodies), events and event dispatchers, create nodes, connect pins, set pin defaults and node properties, with $id references between steps. dryRun checks a batch without running any step.',
    whenToUse: ['More than a couple of nodes or links must be added to one Blueprint graph.', 'A new function and the nodes inside it must be built in one call.', 'A long batch should be checked before it runs (dryRun: every problem at once, nothing changed).'],
    whenNotToUse: ['Nodes or links must be deleted (use delete_node; destructive edits are not batched).'],
    inputProps: {
      blueprintPath: P.blueprintPath,
      graphName: { type: 'string', description: 'Graph every step targets unless the step names its own graphName (default EventGraph).' },
      operations: {
        type: 'array',
        items: ITEM,
        'x-unreal-reflection-boundary': true,
        description: 'Steps run in order, 1-200. Each is {edit, ...that edit\'s own params}. Member steps come before the nodes '
          + 'that use them: add_variable (variableName, variableType as add_variable takes it: basic, class, struct, enum and container '
          + 'types, defaultValue, isPublic, exposeOnSpawn, category), add_function '
          + '(functionName, inputs, outputs, pure, isPublic; the create steps of its body set graphName to the function name, '
          + 'and a connect or pin step that leaves graphName out runs in the graph of the node it names; with an id, "$id" '
          + 'is its entry node and "$id_return" its return node, which exists when outputs are declared), add_event '
          + '(customEventName and parameters, or componentName plus eventName for a component or Widget Blueprint widget delegate, '
          + 'e.g. PlayButton with OnClicked) and add_event_dispatcher '
          + '(dispatcherName, parameters [{name, type}]). Node steps: create_node, connect_pins, set_pin_default_value, '
          + 'set_node_property, create_reroute_node. create_node also takes nodeType CallDelegate (memberName: a dispatcher, '
          + 'memberClass: another class\'s), BindEvent (the same: a Bind Event node and a custom event, eventName, wired to it; '
          + 'with an id, "$id_event" is that event, whose then pin runs when the dispatcher fires, and the object bound goes to '
          + '"$id.self"), '
          + 'Message (memberClass: an interface, memberName: its function), AsyncTask (memberClass: the task or async-action '
          + 'class, memberName: its static factory; event graphs only), FunctionResult (one more return node in a function '
          + 'graph) and MathExpression with expression (the formula, checked and set as set_node_property Expression sets '
          + 'it, before pinDefaults, which can then name its inputs; a name in the formula that is one of the Blueprint\'s variables reads that variable and gets no input pin, any other name becomes an input pin). Optional per step: id (names the created node; later steps use "$id" in fromNodeId, toNodeId and nodeId), '
          + 'from and to ("$id.PinName" shorthand for connect_pins), pinDefaults (create_node only: {PinName: value} applied '
          + 'to the new node; a read-only pin such as Set Text\'s Value gets a MakeLiteral node wired into it). "$entry" is '
          + 'the graph\'s own entry node (a Construction Script or function graph starts there: from "$entry.then"). A pure '
          + 'node (math, a getter) runs again for each node that reads it, so after a Set node read the new value from the '
          + 'Set node\'s Output_Get pin, not from the math that computed it. A create '
          + 'step without posX and posY is auto-placed, and once every step ran it moves beside a node it is wired to (right of what runs it, below-left of what reads it); one whose posX and posY overlap an existing node is placed at the nearest '
          + 'free position instead, and its result says where in placementWarning, so a crowded row never stops the batch (a '
          + 'single create_node call still refuses an overlap). Every function, variable, dispatcher and async factory a step names is '
          + 'checked before any step runs, so a misspelled one fails the batch with nothing applied; any other failure stops '
          + 'the batch at that step and undoes the steps before it (members, nodes, links and pin defaults, in every graph), '
          + 'so a failed batch leaves the Blueprint as it was: fix that step and send the whole batch again. Only a '
          + 'set_node_property on a node that was already there stays, and the error names it.',
      },
      dryRun: {
        type: 'boolean',
        description: 'Check the batch without running any step (default false): the members, functions, variables and '
          + 'expressions the steps name, every "$id" against the ids of the steps before it, and every node named by guid. '
          + 'Every problem comes back at once in problems, and the reply fails with the first one\'s code; the pins of the '
          + 'nodes the batch would create are checked only when it runs. Nothing is changed, compiled or saved.',
      },
    },
    required: ['blueprintPath', 'operations'],
    outputProps: {
      results: { type: 'array', items: ITEM, 'x-unreal-reflection-boundary': true, description: 'Per-step outcome: index, edit, id, success, error, nodeGuid, resultNodeGuid (add_function), pins (for created nodes), connected, appliedValue, placementWarning (a create step that was moved off an overlapping position, or placed where it still overlaps), inputPins and boundToMembers (a Math Expression\'s Expression step: the pins it made, and the names that read member variables instead). A step that ran and has nothing more to say (a plain connect_pins, an add_variable) is counted in succeeded, not listed; a failed batch lists only the step that failed, with its error.' },
      nodeIds: { type: 'object', additionalProperties: { type: 'string' }, description: 'Step id -> node guid for every node the batch created or reused; an add_function step with an id also adds "<id>_return" for its return node. Absent when a step failed: its nodes were removed.' },
      succeeded: { type: 'number', description: 'Steps that completed; 0 when a step failed, since the steps before it were undone.' },
      failedIndex: { type: 'number', description: 'Index of the step that stopped the batch (failures only).' },
      compiled: { type: 'boolean', description: 'True when the Blueprint compiles after the batch (warnings allowed).' },
      compilerStatus: { type: 'string', description: 'Blueprint status after the final compile.' },
      diagnostics: { type: 'array', items: ITEM, 'x-unreal-reflection-boundary': true, description: 'Compile messages: { severity, message }.' },
      saved: { type: 'boolean', description: 'Whether the Blueprint was saved after the batch.' },
      dryRun: { type: 'boolean', description: 'True when nothing ran (dryRun).' },
      checked: { type: 'number', description: 'dryRun: the steps checked.' },
      wouldCreate: { type: 'number', description: 'dryRun: the nodes the batch would create.' },
      problems: { type: 'array', items: ITEM, 'x-unreal-reflection-boundary': true, description: 'dryRun: every step that would fail a check, each with index, edit, errorCode and error.' },
    },
    outputRequired: [],
    effect: 'write',
    behavior: { idempotency: 'non-idempotent', safeToRetry: false },
    latency: 'interactive',
    resources: 'medium',
    plugins: BP_PLUGINS,
    exampleInput: {
      action: 'build_graph',
      blueprintPath: '/Game/Blueprints/BP_Gate',
      graphName: 'EventGraph',
      operations: [
        { edit: 'add_variable', variableName: 'Count', variableType: 'int' },
        { edit: 'create_node', id: 'hit', nodeType: 'Event', eventName: 'ActorBeginOverlap' },
        { edit: 'create_node', id: 'get', nodeType: 'VariableGet', memberName: 'Count' },
        { edit: 'create_node', id: 'add', nodeType: 'CallFunction', memberName: 'Add_IntInt', memberClass: 'KismetMathLibrary', pinDefaults: { B: 1 } },
        { edit: 'create_node', id: 'set', nodeType: 'VariableSet', memberName: 'Count' },
        { edit: 'create_node', id: 'enough', nodeType: 'CallFunction', memberName: 'GreaterEqual_IntInt', memberClass: 'KismetMathLibrary', pinDefaults: { B: 3 } },
        { edit: 'create_node', id: 'if', nodeType: 'Branch' },
        { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'Third time' } },
        { edit: 'connect_pins', from: '$get.Count', to: '$add.A' },
        { edit: 'connect_pins', from: '$add.ReturnValue', to: '$set.Count' },
        { edit: 'connect_pins', from: '$hit.then', to: '$set.execute' },
        { edit: 'connect_pins', from: '$set.Output_Get', to: '$enough.A' },
        { edit: 'connect_pins', from: '$set.then', to: '$if.execute' },
        { edit: 'connect_pins', from: '$enough.ReturnValue', to: '$if.Condition' },
        { edit: 'connect_pins', from: '$if.then', to: '$print.execute' },
      ],
    },
    exampleOutput: {
      success: true,
      message: 'Ran 15 graph operations; the blueprint compiles.',
      nodeIds: { hit: '8C1D0E6A4F2B4C1D9E0F1A2B3C4D5E6F', set: '1A2B3C4D5E6F40718293A4B5C6D7E8F9' },
      succeeded: 15,
      compiled: true,
      saved: true,
    },
  }),
];
