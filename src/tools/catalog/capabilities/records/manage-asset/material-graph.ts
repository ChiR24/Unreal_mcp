// Material graph operation records: connections, queries, node lifecycle,
// and the connect_material_pins/break_material_connections/rebuild_material
// transport aliases (C++ rewrites them to connect_nodes/disconnect_nodes/compile_material).

import type { RecordSpec } from './builder.js';
import { aliasCanonical, aliasOf, bool, LOW, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const MAT = str('Material /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const MATERIAL_GRAPH_RECORDS: readonly RecordSpec[] = [
  r('connect_nodes', 'material', 'Connect two nodes in a material graph.', schema({ materialPath: MAT, sourceNodeId: str('Source node ID.'), sourcePin: str('Source pin name.'), targetNodeId: str('Target node ID.'), targetPin: str('Target pin name.'), inputName: str('Input pin name.') }, ['materialPath', 'sourceNodeId', 'targetNodeId']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool', normalization: aliasCanonical('connect_material_pins') }),
  r('connect_material_pins', 'material', 'Connect material pins (alias of connect_nodes).', schema({ materialPath: MAT, sourceNodeId: str('Source node ID.'), sourcePin: str('Source pin name.'), targetNodeId: str('Target node ID.'), targetPin: str('Target pin name.') }, ['materialPath', 'sourceNodeId', 'targetNodeId']), OK, WRITE, WRITE_POLICY, LOW, { normalization: aliasOf('material.connect_nodes'), dispatchAction: 'connect_material_pins', dispatchMode: 'tool' }),
  r('disconnect_nodes', 'material', 'Disconnect two nodes in a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID.'), pinName: str('Pin name.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool', normalization: aliasCanonical('break_material_connections') }),
  r('break_material_connections', 'material', 'Break material connections (alias of disconnect_nodes).', schema({ materialPath: MAT, nodeId: str('Node ID.'), pinName: str('Pin name.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW, { normalization: aliasOf('material.disconnect_nodes'), dispatchAction: 'break_material_connections', dispatchMode: 'tool' }),
  r('find_node', 'material', 'Find a node in a material graph by type or name.', schema({ materialPath: MAT, nodeType: str('Node type to find.') }, ['materialPath']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('get_node_connections', 'material', 'Retrieve connections for a node in a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('get_node_properties', 'material', 'Retrieve properties of a node in a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_static_switch_parameter_value', 'material', 'Set a static switch parameter value on a material instance.', schema({ instancePath: str('Material instance /Game path.'), parameterName: str('Parameter name.'), value: bool('Switch value.') }, ['instancePath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('delete_node', 'material', 'Delete a node from a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID to delete.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('update_custom_expression', 'material', 'Update the HLSL code of a custom expression node.', schema({ materialPath: MAT, nodeId: str('Node ID.'), code: str('Updated HLSL code.') }, ['materialPath', 'nodeId', 'code']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('get_node_chain', 'material', 'Retrieve the chain of nodes connected to a starting node.', schema({ materialPath: MAT, nodeId: str('Starting node ID.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('get_connected_subgraph', 'material', 'Retrieve the connected subgraph from a starting node.', schema({ materialPath: MAT, nodeId: str('Starting node ID.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_material_node', 'material', 'Add a generic material node by type.', schema({ materialPath: MAT, nodeType: str('Node type.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath', 'nodeType']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('rebuild_material', 'material', 'Rebuild/compile a material (alias of compile_material).', schema({ materialPath: MAT }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { normalization: aliasOf('material.compile_material'), dispatchAction: 'rebuild_material', dispatchMode: 'tool' }),
  r('set_material_parameter', 'material', 'Set a material parameter value.', schema({ materialPath: MAT, parameterName: str('Parameter name.'), value: { description: 'Parameter value.' } }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('get_material_node_details', 'material', 'Retrieve details of a material node.', schema({ materialPath: MAT, nodeId: str('Node ID.'), expressionIndex: num('Expression index.') }, ['materialPath', 'nodeId']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('remove_material_node', 'material', 'Remove a node from a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID to remove.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' })
];
