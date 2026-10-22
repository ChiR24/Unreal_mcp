import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildWorldRecord } from './builder.js';

const graph: JsonObject = {
  graphPath: { type: 'string', description: 'PCG graph asset path.' },
  expectedGraphRevision: { type: 'string', description: 'Authored revision returned by get_pcg_graph; mandatory for batches.' },
  save: { type: 'boolean', description: 'Save the graph after the edit, default true. Saving is not part of in-memory rollback.' },
};
const nodeId = { type: 'string', description: 'Exact stable model node identifier, or input/output; numeric indices are not accepted.' };
const operations = {
  type: 'array', minItems: 1, maxItems: 100,
  description: 'Ordered single-graph operations: add_node(settingsClass,settings,tempId,title,x,y,meshPath,actorClass,texturePath), set_settings(nodeId,settings,title,x,y,meshPath,actorClass,texturePath), move_node(nodeId,x,y), connect/disconnect(sourceNodeId,targetNodeId,sourcePin,targetPin), remove_node(nodeId). Earlier tempId values can be used as node IDs. No saves, execution, graph parameters, world changes or cross-asset writes inside a batch.',
  items: { type: 'object', 'x-unreal-reflection-boundary': true },
};
const output: JsonObject = {
  committed: { type: 'boolean', description: 'All in-memory edits committed.' },
  saved: { type: 'boolean', description: 'The edited graph was successfully saved.' },
  rolledBack: { type: 'boolean', description: 'Explicit recovery verified restoration after a commit failure; preflight failures need no rollback.' },
  residualChanges: { type: 'boolean', description: 'Recovery could not verify full restoration; the graph was not saved.' },
  revision: { type: 'string', description: 'Authored graph revision after commit.' },
  previousRevision: { type: 'string', description: 'Authored graph revision before commit.' },
  graphPath: { type: 'string', description: 'Resolved target graph path.' },
  phase: { type: 'string', description: 'Failure phase: preflight or commit.' },
  failedIndex: { type: 'integer', description: 'Zero-based failed step, or -1 for graph synchronization failure.' },
  results: { type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: 'Committed operation results in input order.' },
  nodeIds: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Temporary node IDs mapped to committed model node IDs.' },
  editorGraph: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Editor/model consistency after commit.' },
  undo: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Editor undo availability; unrelated to explicit recovery and disk persistence.' },
};
const specs: readonly { action: string; effect: 'write' | 'destructive'; summary: string; inputProps: JsonObject; required: readonly string[] }[] = [
  { action: 'move_pcg_node', effect: 'write', summary: 'Move a PCG model node and synchronize its existing editor representation.',
    inputProps: { ...graph, nodeId, x: { type: 'number', description: 'Horizontal graph coordinate in int32 range.' }, y: { type: 'number', description: 'Vertical graph coordinate in int32 range.' } }, required: ['graphPath', 'nodeId', 'x', 'y'] },
  { action: 'disconnect_pcg_pins', effect: 'destructive', summary: 'Remove one PCG edge without touching other connections.',
    inputProps: { ...graph, sourceNodeId: nodeId, targetNodeId: nodeId,
      sourcePin: { type: 'string', description: 'Output pin label; omitted selects the native default.' },
      targetPin: { type: 'string', description: 'Input pin label; omitted selects the native default.' } }, required: ['graphPath', 'sourceNodeId', 'targetNodeId'] },
  { action: 'remove_pcg_node', effect: 'destructive', summary: 'Remove one PCG node and its incident edges, never the graph input or output.',
    inputProps: { ...graph, nodeId }, required: ['graphPath', 'nodeId'] },
  { action: 'batch_edit_pcg_graph', effect: 'write', summary: 'Prevalidate and atomically commit up to 100 single-graph edits without destructive steps.',
    inputProps: { ...graph, operations }, required: ['graphPath', 'expectedGraphRevision', 'operations'] },
  { action: 'batch_edit_pcg_graph_destructive', effect: 'destructive', summary: 'Atomically edit a single PCG graph, including explicit disconnections and node deletion.',
    inputProps: { ...graph, operations }, required: ['graphPath', 'expectedGraphRevision', 'operations'] },
];
export const PCG_MUTATION_RECORDS: readonly CapabilityRecordSource[] = specs.map(spec => buildWorldRecord({
  parentTool: 'manage_pcg', ...spec, plugins: ['PCG'], family: spec.effect === 'destructive' ? 'pcg-removal' : spec.action === 'move_pcg_node' ? 'pcg' : 'pcg-editing',
  topics: ['pcg batch editing', 'move pcg node', 'disconnect pcg pins', 'delete pcg node'],
  whenToUse: ['Edit an observed PCG graph without leaving partial changes after validation failure.'],
  whenNotToUse: ['Cross-asset, runtime or asynchronous generation changes are needed.'],
  costLatency: 'interactive', costResources: 'medium', outputProps: output,
  exampleInput: { action: spec.action, graphPath: '/Game/PCG/Graph',
    ...(spec.action.startsWith('batch_') ? { expectedGraphRevision: 'revision-from-read', operations: [{ op: 'move_node', nodeId: 'Node_0', x: 400, y: 200 }] } :
      spec.action === 'disconnect_pcg_pins' ? { sourceNodeId: 'Node_0', targetNodeId: 'Node_1' } :
        { nodeId: 'Node_0', ...(spec.action === 'move_pcg_node' ? { x: 400, y: 200 } : {}) }) },
}));
