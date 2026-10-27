/**
 * read_graph_as_code and write_graph_from_code: a Blueprint's graphs as the S-expression text of Epic's UE 5.8
 * Blueprint DSL, and that text written back as nodes (plugin Domains/BlueprintGraph/Code). Own records: folded into a
 * family, their action names' words would join the family primary's aliases.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

const ROW = { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true } as const;

export const GRAPH_CODE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.read_graph_as_code',
    action: 'read_graph_as_code',
    family: 'code',
    domain: 'blueprint',
    topics: ['blueprint as code', 'read blueprint logic as text', 'decompile blueprint', 'blueprint to text', 'what does this blueprint do'],
    summary: 'Read a Blueprint\'s event graphs and functions as text, in the S-expression grammar of Epic\'s UE 5.8 Blueprint DSL (read_graph_dsl / write_graph_dsl), on every UE 5.x: one (event Name (Params) ...) or (fn Name (Params) ...) per event or function; a statement per node that runs, (Category|Title args...) named by the node\'s action-menu id (Development|PrintString, Variables|Default|SetHealth), with trailing arguments still at their defaults left out; (bind var (Call ...)) where an output is read later (by output position, _ for one not read); (if cond ... (elif cond ...) (else ...)), (for i (range n) ...), (for elem array ...), (while cond ...), (break), (switch Type value (:Case ...)), (return ...); pure nodes inline: (+ a b), (== a b), (and a b), (not x), (select cond a b), (.x vector), (.yaw rotator), (.location transform), self, literals ("text", 5, true); a node with named exec outputs takes (:Pin ...) continuations (pin names with spaces quoted). Every statement ends with a ; comment naming its node by id: n and the first hex digits of the node\'s GUID (6, more where two nodes read together share them), so ids do not change between reads; nodes gives each id its nodeId (the full GUID other graph actions take) and type, links every wire as "id.Pin -> id.Pin". Where a path runs on into a node already printed, (goto id) says so (Epic\'s own reader leaves that path out); a pure value two statements read is bound once at the top, tagged "pure, runs at each read" (Blueprint works it out again at each read). Reads only: nothing is created, compiled or changed. The first read after the editor starts takes several seconds while the editor builds its node menu.',
    whenToUse: [
      'What a Blueprint does must be read or reviewed in one call, instead of node by node.',
      'Before editing a graph: the code shows what runs and in what order, and the node ids to edit.',
      'A Blueprint\'s logic should be explained, compared with another, or checked against what it is meant to do.',
    ],
    whenNotToUse: [
      'One node\'s pins, defaults or position are needed: get_node_details.',
      'Mistakes that show only in Play should be found: lint_blueprint.',
    ],
    inputProps: {
      blueprintPath: P.blueprintPath,
      graphName: { type: 'string', description: 'One graph: EventGraph, a function\'s name, or a macro or collapsed graph by name (GRAPH_NOT_FOUND lists the graphs). Default: every event graph and function.' },
    },
    required: ['blueprintPath'],
    outputProps: {
      code: { type: 'string', description: 'The text: each graph after a "; graph Name" comment line, its events and functions top to bottom.' },
      graphs: { type: 'array', items: ROW, 'x-unreal-reflection-boundary': true, description: 'One row per graph read: name, kind (event_graph, function, macro or graph), nodes, and notInCode (the ids of nodes no event or function reaches, so the code leaves them out; reroutes are never printed).' },
      nodes: { type: 'array', items: ROW, 'x-unreal-reflection-boundary': true, description: 'Every node: id (as the code names it), nodeId (its GUID) and type (its action-menu id). At most 500.' },
      links: { type: 'array', items: { type: 'string' }, description: 'Every wire as "id.OutPin -> id.InPin". At most 1000.' },
      nodeCount: { type: 'number', description: 'Nodes in the graphs read, listed or not.' },
      linkCount: { type: 'number', description: 'Wires in the graphs read, listed or not.' },
      truncated: { type: 'boolean', description: 'Present and true when nodes or links stopped at their limit.' },
    },
    outputRequired: ['code'],
    effect: 'read',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'read_graph_as_code', blueprintPath: '/Game/Blueprints/BP_Door', graphName: 'EventGraph' },
    exampleOutput: {
      success: true,
      message: 'BP_Door as code: 1 graph(s), 2 node(s).',
      code: '; graph EventGraph\n(event EventBeginPlay  ; n8c1d0e\n  (Development|PrintString "Door ready"))  ; n1f2e3d\n',
      graphs: [{ name: 'EventGraph', kind: 'event_graph', nodes: 2 }],
      nodes: [{ id: 'n8c1d0e', nodeId: '8C1D0E6A4F2B4C1D9E0F1A2B3C4D5E6F', type: 'AddEvent|EventBeginPlay' }, { id: 'n1f2e3d', nodeId: '1F2E3D4C5B6A49788796A5B4C3D2E1F0', type: 'Development|PrintString' }],
      links: ['n8c1d0e.then -> n1f2e3d.execute'],
      nodeCount: 2,
      linkCount: 1,
    },
  }),
  buildRecord({
    id: 'blueprint.write_graph_from_code',
    action: 'write_graph_from_code',
    family: 'code',
    domain: 'blueprint',
    topics: ['write blueprint as code', 'blueprint from text', 'rewrite event body', 'rewrite function body', 'code into blueprint nodes'],
    summary: 'Write a Blueprint\'s logic from text, on every UE 5.x: each (event Name ...) or (fn Name (Params) ...) form replaces that event\'s or function\'s body with nodes made from its statements, in the grammar read_graph_as_code prints (Epic\'s UE 5.8 Blueprint DSL), so the way to edit is read, change the text, write it back. A statement is (Category|Title args... :Pin value (:ExecPin stmts...)), the node named by its action-menu id as read_graph_as_code prints it (positional arguments fill the data inputs in order, :Pin names one, a (:Pin ...) continuation runs from that exec output and sees the node\'s outputs as _pin_name); (bind name (Call ...)) or (bind (first _ third) (Call ...)) names outputs for later statements (a name is bound once); (if cond ... (elif cond ...) (else ...)), (for i (range n) ...), (for i (range a b) ...), (for item array ...), (while cond ...), (break), (switch int|string|name value (:Case ...) (:Default ...)), (return values...), (goto id [Pin]) on to the node with that id (a statement of this code by its ; comment, or a node the write leaves in the graph, so one event can be rewritten alone). Values: names, literals ("text", 5, 1.5, true), (+ - * / % == != < <= > >= and or xor a b), (not x), (- x), (select cond a b), (.x .y .z .pitch .yaw .roll .location .rotation .scale value), self. An entry\'s inputs are names by their pin names, as read_graph_as_code lists them (an (fn) head must list only names the function has; an (event) head\'s list is optional): code names inputs, add_function or add_event makes them. An event the graph lacks is made (a custom event as Custom|Name); a function must exist (FUNCTION_NOT_FOUND). A form with no statements leaves that body as it is. What goes: the nodes only that event or function reached (its flow and the values that flow read); a node another event also runs stays, and events and functions the code does not name are not touched; a function keeps its return nodes when the new body makes none. A statement whose ; comment names a node of the old body by its id (as read_graph_as_code tags every statement) keeps that node when it is of the same kind (its GUID, position and comment stay; its links and inputs are set afresh), so read, edit, write changes only what was edited; new nodes then go beside the nodes they are wired to. Code without such ids makes every node new, laid out in columns from its event, a new event below the graph. All or nothing: a parse error, an unknown node id (the reply lists the ids the menu offers for that title), an undefined name, too many arguments, a pin the node lacks, or code that leaves a Blueprint that compiled not compiling is refused with its line and nothing changes (CODE_PARSE_ERROR, CODE_INVALID, CODE_BREAKS_COMPILE with compileMessages). The reply reads the written graphs back as code with the new nodes\' ids. Refused during Play In Editor (PIE_ACTIVE). The first call after the editor starts takes several seconds while the editor builds its node menu.',
    whenToUse: [
      'An event\'s or function\'s logic should be written or rewritten in one call instead of node by node.',
      'A graph read with read_graph_as_code should be changed: edit the text and write it back.',
    ],
    whenNotToUse: [
      'One node, link or pin default should change and the rest stay exactly as it is (node GUIDs included): create_node, connect_pins, set_pin_default_value.',
      'Variables, functions or event inputs must be added: add_variable, add_function, add_event.',
    ],
    inputProps: {
      blueprintPath: P.blueprintPath,
      code: { type: 'string', description: 'One or more (event Name [(Params)] stmts...) and (fn Name (Params) stmts...) forms, as read_graph_as_code prints them; ; comments are ignored except the node ids gotos name.' },
      graphName: { type: 'string', description: 'The event graph (event) forms go into (default: the first). An (fn) form always goes into its own function.' },
    },
    required: ['blueprintPath', 'code'],
    outputProps: {
      code: { type: 'string', description: 'The written graphs read back as read_graph_as_code prints them, with the new nodes\' ids.' },
      forms: { type: 'array', items: ROW, 'x-unreal-reflection-boundary': true, description: 'One row per form: form ("event Name" or "fn Name"), graph, nodes (made for it) and entryMade (true when its event was made).' },
      created: { type: 'number', description: 'Nodes made, new events included.' },
      kept: { type: 'number', description: 'Old-body nodes kept by the ids in the code\'s ; comments.' },
      removed: { type: 'number', description: 'Nodes of the old bodies removed.' },
      compileMessages: { type: 'array', items: { type: 'string' }, description: 'Compiler messages on the nodes made, as "line N error: ..." or "line N warning: ...", N the line of the statement that made the node.' },
      compiled: { type: 'boolean', description: 'True when the Blueprint compiles after the write (warnings allowed).' },
      compilerStatus: { type: 'string', description: 'Blueprint status after the compile.' },
      diagnostics: { type: 'array', items: ROW, 'x-unreal-reflection-boundary': true, description: 'Compile messages: { severity, message }.' },
      saved: { type: 'boolean', description: 'Whether the Blueprint was saved.' },
    },
    outputRequired: ['code'],
    effect: 'destructive',
    behavior: { safeToRetry: false },
    latency: 'interactive',
    resources: 'medium',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'write_graph_from_code', blueprintPath: '/Game/Blueprints/BP_Door', code: '(event EventBeginPlay\n  (Development|PrintString "Door ready"))' },
    exampleOutput: {
      success: true,
      message: 'Wrote 1 form(s) into BP_Door: 1 node(s) made, 0 kept, 1 removed.',
      code: '; graph EventGraph\n(event EventBeginPlay  ; n8c1d0e\n  (Development|PrintString "Door ready"))  ; n5a6b7c\n',
      forms: [{ form: 'event EventBeginPlay', graph: 'EventGraph', nodes: 1, entryMade: false }],
      created: 1,
      kept: 0,
      removed: 1,
      compiled: true,
      compilerStatus: 'UpToDate',
      saved: true,
    },
  }),
];
