/**
 * read_graph_as_code: a Blueprint's graphs as the S-expression text of Epic's UE 5.8 Blueprint DSL (plugin
 * Domains/BlueprintGraph/Code). Its own record: folded into a family, its action name's words would join the
 * family primary's aliases.
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
];
