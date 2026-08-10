// Material graph operation records: connections, queries, node lifecycle,
// and the connect_material_pins/break_material_connections/rebuild_material
// transport aliases (C++ rewrites them to connect_nodes/disconnect_nodes/compile_material).

import type { RecordSpec } from './builder.js';
import { aliasCanonical, aliasOf, arr, arrObj, bool, ex, LOW, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const MAT = str('Material /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

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
  r('find_node', 'material', 'Find a node in a material graph by type or name.', schema({ materialPath: MAT, nodeType: str('Node type to find.') }, ['materialPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Locate every texture sample', { materialPath: M, nodeType: 'TextureSample' }, DONE)] }),
  r('get_node_connections', 'material', 'Retrieve connections for a node in a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID.'), direction: str('Connection direction to report (inputs or outputs).'), downstream: bool('Report downstream connections instead of upstream.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW,
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
  r('set_material_parameter', 'material', 'Set a material parameter value.', schema({ assetPath: MAT, parameterName: str('Parameter name.'), value: { description: 'Parameter value.' } }, ['assetPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Set a roughness parameter', { assetPath: M, parameterName: 'Roughness', value: 0.35 }, DONE)] }),
  r('get_material_node_details', 'material', 'Retrieve details of a material node.', schema({ materialPath: MAT, nodeId: str('Node ID.'), expressionIndex: num('Expression index.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Inspect one node in detail', { materialPath: M, nodeId: SAMPLE, expressionIndex: 0 }, DONE)] }),
  r('remove_material_node', 'material', 'Remove a node from a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID to remove.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Remove a node from the graph', { materialPath: M, nodeId: SAMPLE }, DONE)] })
];
