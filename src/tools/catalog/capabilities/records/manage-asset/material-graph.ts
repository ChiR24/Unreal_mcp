// Material graph operation records: connections, queries, node lifecycle,
// and the connect_material_pins/break_material_connections/rebuild_material
// transport aliases (C++ rewrites them to connect_nodes/disconnect_nodes/compile_material).

import type { RecordSpec } from './builder.js';
import { aliasCanonical, aliasOf, arr, arrObj, bool, ex, LOW, num, READ, READ_POLICY, r, refObj, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const MAT = str('Material /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);
// set_node_position echoes the applied coordinates and re-runs the same overlap
// check the node adders use, so a caller can confirm the move actually cleared
// the collision instead of trading one overlap for another.
const MOVE_OUT = schema({
  success: bool('Operation succeeded.'),
  nodeId: str('Moved node ID.'),
  posX: num('X coordinate the node now sits at.'),
  posY: num('Y coordinate the node now sits at.'),
  estimatedWidth: num('Approximate node width; an estimate, not a measurement.'),
  estimatedHeight: num('Approximate node height from connector count plus the inline default-value widget allowance.'),
  overlappingNodes: { type: 'array', items: { type: 'string' }, description: 'Names of expressions still overlapping after the move. Absent when the placement is clear.' },
  placementWarning: str('Human-readable overlap warning, present only when overlappingNodes is non-empty.'),
}, ['success']);

// Read capabilities must declare the fields their handler actually emits. The generic
// {success, details} envelope above plus additionalProperties:false silently dropped every
// real field, so these queries returned a bare {"success": true} and were useless for
// verifying anything — the handlers were building full payloads the whole time.
const CONNECTIONS_OUT = schema({
  success: bool('Operation succeeded.'),
  nodeId: str('Node the traversal started from.'),
  type: str('Expression class name of the start node.'),
  connectionCount: num('Number of connections returned.'),
  connections: arrObj('Edges found, each with sourceNodeId, sourceOutputIndex, targetNodeId, targetInput, hop and direction. A targetNodeId of "Main" is the material output node.'),
}, ['success']);

const NODE_DETAILS_OUT = schema({
  success: bool('Operation succeeded.'),
  nodeId: str('Resolved node ID.'),
  nodeType: str('Expression class name.'),
  nodeName: str('Expression object name.'),
  assetType: str('Material or MaterialFunction.'),
  parameterName: str('Parameter name, for parameter expressions.'),
  scalarDefault: num('DefaultValue, for scalar parameter expressions.'),
  vectorDefault: refObj('DefaultValue as rgba, for vector parameter expressions.'),
  inputName: str('Pin name, for a function input.'),
  inputType: str('Pin type, for a function input.'),
  outputName: str('Pin name, for a function output.'),
  sortPriority: num('Pin sort priority, for function input/output expressions.'),
  usePreviewValueAsDefault: bool('Whether a function input previews its default.'),
}, ['success']);

const M = '/Game/Materials/M_Base';
const SAMPLE = 'MaterialExpressionTextureSample_0';
const MULTIPLY = 'MaterialExpressionMultiply_0';
const DONE = { success: true };

export const MATERIAL_GRAPH_RECORDS: readonly RecordSpec[] = [
  r('connect_nodes', 'material', 'Connect two nodes in a material graph.', schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath).'), sourceNodeId: str('Source node ID.'), sourcePin: str('Source pin name.'), targetNodeId: str('Target node ID.'), targetPin: str('Target pin name.'), inputName: str('Input pin name.') }, ['sourceNodeId', 'targetNodeId'], ['materialPath', 'assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', normalization: aliasCanonical('connect_material_pins'), examples: [ex('Feed a texture sample into a multiply', { materialPath: M, sourceNodeId: SAMPLE, sourcePin: 'RGB', targetNodeId: MULTIPLY, targetPin: 'A' }, DONE)] }),
  r('connect_material_pins', 'material', 'Connect material pins (alias of connect_nodes).', schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath).'), sourceNodeId: str('Source node ID.'), sourcePin: str('Source pin name.'), targetNodeId: str('Target node ID.'), targetPin: str('Target pin name.') }, ['sourceNodeId', 'targetNodeId'], ['materialPath', 'assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { normalization: aliasOf('material.connect_nodes'), dispatchAction: 'connect_material_pins', dispatchMode: 'tool', examples: [ex('Connect pins via the alias route', { materialPath: M, sourceNodeId: SAMPLE, sourcePin: 'RGB', targetNodeId: MULTIPLY, targetPin: 'A' }, DONE)] }),
  r('disconnect_nodes', 'material', 'Disconnect two nodes in a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID.'), pinName: str('Pin name.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', normalization: aliasCanonical('break_material_connections'), examples: [ex('Break the A input of a multiply', { materialPath: M, nodeId: MULTIPLY, pinName: 'A' }, DONE)] }),
  r('break_material_connections', 'material', 'Break material connections (alias of disconnect_nodes).', schema({ materialPath: MAT, nodeId: str('Node ID.'), pinName: str('Pin name.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW,
    { normalization: aliasOf('material.disconnect_nodes'), dispatchAction: 'break_material_connections', dispatchMode: 'tool', examples: [ex('Break connections via the alias route', { materialPath: M, nodeId: MULTIPLY, pinName: 'A' }, DONE)] }),
  r('find_node', 'material', 'Find a node in a material graph by type or name.', schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath, as the sibling read actions spell it).'), nodeType: str('Node type to find.'), nodeName: str('Node name to find.') }, [], ['materialPath', 'assetPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Locate every texture sample', { materialPath: M, nodeType: 'TextureSample' }, DONE)] }),
  r('get_node_connections', 'material', 'Retrieve connections for a node in a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID.'), direction: str('Connection direction to report (inputs or outputs).'), depth: num('Traversal depth; -1 walks the whole graph.'), upstream: bool('Walk every upstream producer, overriding direction and depth.'), downstream: bool('Report downstream connections instead of upstream.') }, ['materialPath', 'nodeId']), CONNECTIONS_OUT, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('List what feeds a multiply node', { materialPath: M, nodeId: MULTIPLY, direction: 'inputs', downstream: false }, DONE)] }),
  r('get_node_properties', 'material', 'Retrieve properties of a node in a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Read a texture sample node\'s properties', { materialPath: M, nodeId: SAMPLE }, DONE)] }),
  r('set_static_switch_parameter_value', 'material', 'Set a static switch parameter value on a material instance.', schema({ assetPath: str('Material instance /Game asset path.'), parameterName: str('Parameter name.'), value: bool('Switch value.') }, ['assetPath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Enable a detail-map switch on an instance', { assetPath: '/Game/Materials/MI_Base_Rusty', parameterName: 'UseDetailMap', value: true }, DONE)] }),
  r('delete_node', 'material', 'Delete a node from a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID to delete.'), nodeIds: arr('Node IDs to delete in one batch, in place of nodeId.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Delete an unused multiply node', { materialPath: M, nodeId: MULTIPLY }, DONE)] }),
  r('update_custom_expression', 'material', 'Update the HLSL code of a custom expression node.', schema({ materialPath: MAT, nodeId: str('Node ID.'), code: str('Updated HLSL code.'), additionalOutputs: arrObj('Additional named output definitions.') }, ['materialPath', 'nodeId', 'code']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Rewrite a custom node\'s HLSL', { materialPath: M, nodeId: 'MaterialExpressionCustom_0', code: 'return saturate(A * 3.0f);' }, DONE)] }),
  r('get_node_chain', 'material', 'Retrieve the chain of nodes connected to a starting node.', schema({ materialPath: MAT, nodeId: str('Starting node ID.'), startNodeId: str('Starting node ID accepted by the handler in place of nodeId.'), endPin: str('Terminal pin name to stop the chain walk at.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Walk the chain feeding BaseColor', { materialPath: M, nodeId: SAMPLE, endPin: 'BaseColor' }, DONE)] }),
  r('get_connected_subgraph', 'material', 'Retrieve the connected subgraph from a starting node.', schema({ materialPath: MAT, nodeId: str('Starting node ID.'), orphansOnly: bool('Report only orphaned nodes; accepted in place of nodeId.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Collect the subgraph under a node', { materialPath: M, nodeId: SAMPLE, orphansOnly: false }, DONE)] }),
  r('add_material_node', 'material', 'Add a generic material node by type.', schema({ materialPath: MAT, nodeType: str('Node type.'), type: str('Node type (alias of nodeType).'), posX: num('Node X position.'), posY: num('Node Y position.'), x: num('Node X position (preferred spelling; posX is the fallback).'), y: num('Node Y position (preferred spelling; posY is the fallback).') }, ['materialPath', 'nodeType']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add a Constant3Vector by type name', { materialPath: M, nodeType: 'Constant3Vector', x: -300, y: 100 }, DONE)] }),
  r('rebuild_material', 'material', 'Rebuild/compile a material (alias of compile_material).', schema({ materialPath: MAT }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { normalization: aliasOf('material.compile_material'), dispatchAction: 'rebuild_material', dispatchMode: 'tool', examples: [ex('Rebuild after graph edits', { materialPath: M }, DONE)] }),
  r('set_material_parameter', 'material', 'Set a material parameter value.', schema({ assetPath: MAT, parameterName: str('Parameter name.'), parameterType: str('Parameter kind: scalar (default), vector, or texture. Selects which parameter expression the value is written to.'), value: { description: 'Parameter value.' } }, ['assetPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW,
    { topics: ['material parameter', 'set parameter', 'scalar parameter', 'vector parameter', 'texture parameter'], dispatchMode: 'tool', examples: [ex('Set a roughness parameter', { assetPath: M, parameterName: 'Roughness', value: 0.35 }, DONE)] }),
  // assetPath is the spelling the handler reads. Declaring only materialPath made this
  // capability uncallable by any input: the schema-correct call died in the handler, the
  // handler-correct call failed schema validation, and sending both was rejected as
  // undeclared. Accept either, exactly as connect_nodes does.
  r('get_material_node_details', 'material', 'Retrieve details of a material node.', schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath).'), nodeId: str('Node ID.'), expressionIndex: num('Expression index.') }, ['nodeId'], ['materialPath', 'assetPath']), NODE_DETAILS_OUT, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Inspect one node in detail', { materialPath: M, nodeId: SAMPLE, expressionIndex: 0 }, DONE)] }),
  r('remove_material_node', 'material', 'Remove a node from a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID to remove.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Remove a node from the graph', { materialPath: M, nodeId: SAMPLE }, DONE)] }),
  // Nodes could be placed at a coordinate but never moved, so a graph laid out
  // badly stayed that way — the only recourse was remove plus re-add, which
  // drops the node's connections. This is what makes the overlap warning the
  // node adders now return actionable after the fact.
  r('set_node_position', 'material', 'Move an existing material graph node to new coordinates, preserving its connections.',
    schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath).'), nodeId: str('Node ID to move.'), x: num('New X coordinate (posX is the fallback spelling).'), y: num('New Y coordinate (posY is the fallback spelling).'), posX: num('New X coordinate (fallback spelling).'), posY: num('New Y coordinate (fallback spelling).') }, ['nodeId'], ['materialPath', 'assetPath']),
    MOVE_OUT, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Space a stacked parameter node out', { materialPath: M, nodeId: SAMPLE, x: -400, y: 260 }, { success: true, nodeId: SAMPLE, posX: -400, posY: 260 })] })
];
