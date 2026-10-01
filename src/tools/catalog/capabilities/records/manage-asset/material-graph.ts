// Material graph operation records: connections, queries, node lifecycle,
// and the connect_material_pins/break_material_connections/rebuild_material
// transport aliases (C++ rewrites them to connect_nodes/disconnect_nodes/compile_material).

import type { RecordSpec } from './builder.js';
import { arr, arrObj, bool, ex, LOW, MATERIAL_PARAMETER_LIST, num, READ, READ_POLICY, r, refObj, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const MAT = str('Material /Game asset path.');
// Every material read takes the path under either spelling (LOAD_MATERIAL_OR_FUNCTION_OR_RETURN reads
// assetPath, then materialPath). A read that declared one spelling only made the other look like a
// parameter of the sibling variants, so a folded info call was told it ignored a path it had read.
const MAT_ALIAS = str('Material asset path (accepted in place of materialPath).');
const SOURCE_PIN = str('Source output: its name, its index, or channel letters of the default output ("G", "RG"; X/Y/Z/W work too). Omit for the default output.');
const SAVE = bool('Save the asset afterwards. Defaults to true; pass false to keep the change in memory only.');
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

// build_material_graph: a whole node graph under one consent. Each step is an
// ordinary node adder, connect_nodes, set_node_position, update_custom_expression
// or material property setter, run in-process by the same single-step handler.
const OPERATIONS = {
  type: 'array',
  items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
  'x-unreal-reflection-boundary': true,
  description: 'Steps run in order, 1-200. Each is {edit, ...the params of that edit}: edit is a node adder (add_material_node, '
    + 'add_scalar_parameter, add_vector_parameter, add_texture_sample, add_texture_coordinate, add_math_node, add_noise, '
    + 'add_custom_expression, ...), use_material_function, connect_nodes, set_node_position, update_custom_expression, '
    + 'set_blend_mode, set_shading_model, set_material_domain or set_two_sided. Optional per step: id (names the created '
    + 'node; later steps use "$id" in sourceNodeId/targetNodeId/nodeId), from/to ("$id.Pin" shorthand for connect_nodes, where '
    + 'a source pin may be channel letters like "$uv.G"; '
    + '"Main.EmissiveColor" is the material output). A created node without x/y is laid out automatically. Deleting and '
    + 'disconnecting are not batched. The batch stops at the first failing step; when every step ran, the material is '
    + 'compiled and saved once.',
} as const;
const BATCH_OUT = schema({
  success: bool('True when every step ran.'),
  assetPath: str('The material the batch edited, compiled and saved.'),
  results: arrObj('Per-step outcome: index, edit, id, success, nodeId, placementWarning, error.'),
  nodeIds: { type: 'object', additionalProperties: { type: 'string' }, description: 'Step id -> node id for every node the batch created.' },
  succeeded: num('Steps that completed.'),
  failedIndex: num('Index of the step that stopped the batch (failures only).'),
  compiled: bool('False when the material does not compile after the batch; compileErrors says why.'),
  compileErrors: arr('Compile errors after the batch, empty when the material compiles.'),
  saved: bool('Whether the material was saved after the batch.'),
}, ['success']);
// update_custom_expression compiles and saves the material when it is called on its own, as compile_material does; as a
// step of build_material_graph it leaves both to the batch, so these three are absent there. `details` takes the rest
// (nodeId, code, inputCount, additionalOutputCount, assetPath).
const CUSTOM_UPDATE_OUT = schema({
  success: bool('Operation succeeded.'),
  compiled: bool('False when the material does not compile after the edit (the default material renders in its place); compileErrors says why. A material function has no translation of its own to read, so this is true for one.'),
  compileErrors: arr('Compile errors the material translator reported after the edit, empty when it compiles.'),
  saved: bool('Whether the material or function was saved after the edit.'),
  details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' },
}, ['success']);

const M = '/Game/Materials/M_Base';
const SAMPLE = 'MaterialExpressionTextureSample_0';
const MULTIPLY = 'MaterialExpressionMultiply_0';
const DONE = { success: true };

export const MATERIAL_GRAPH_RECORDS: readonly RecordSpec[] = [
  r('connect_nodes', 'material', 'Connect two nodes in a material graph.', schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath).'), sourceNodeId: str('Source node ID.'), sourcePin: SOURCE_PIN, targetNodeId: str('Target node ID.'), targetPin: str('Target pin name.'), inputName: str('Input pin name.') }, ['sourceNodeId', 'targetNodeId'], ['materialPath', 'assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['The output of one material node must feed an input of another node in the same material or function.', 'A node result must drive a material output such as BaseColor or Roughness (targetNodeId Main).'],
      whenNotToUse: ['A whole graph of nodes and wires is being built (use material.add_material_node with nodeKind=batch).', 'A wire must be removed (use material.disconnect_nodes).'],
      examples: [ex('Feed a texture sample into a multiply', { materialPath: M, sourceNodeId: SAMPLE, sourcePin: 'RGB', targetNodeId: MULTIPLY, targetPin: 'A' }, DONE)] }),
  r('connect_material_pins', 'material', 'Connect material pins (alias of connect_nodes).', schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath).'), sourceNodeId: str('Source node ID.'), sourcePin: SOURCE_PIN, targetNodeId: str('Target node ID.'), targetPin: str('Target pin name.') }, ['sourceNodeId', 'targetNodeId'], ['materialPath', 'assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['The output of one material node must feed an input of another node in the same material or function.', 'A node result must drive a material output such as BaseColor or Roughness (targetNodeId Main).'],
      whenNotToUse: ['A whole graph of nodes and wires is being built (use material.add_material_node with nodeKind=batch).', 'A wire must be removed (use material.disconnect_nodes).'],
      dispatchAction: 'connect_material_pins', examples: [ex('Connect pins via the alias route', { materialPath: M, sourceNodeId: SAMPLE, sourcePin: 'RGB', targetNodeId: MULTIPLY, targetPin: 'A' }, DONE)] }),
  r('disconnect_nodes', 'material', 'Disconnect two nodes in a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID.'), pinName: str('Input to unplug: a pin of nodeId by name or label (a custom node input such as OB), or for nodeId Main a material output input (BaseColor, Normal...). A name that matches none fails listing the inputs; only a material function\'s output node takes none, unplugging every output.') }, ['materialPath', 'nodeId']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['The wire into one input pin of a node must be cleared while both nodes stay.', 'A material output such as BaseColor must be unplugged (nodeId Main with pinName BaseColor).'],
      whenNotToUse: ['A node is no longer needed (use material.delete_node, which also clears every wire that used it).', 'A pin must take a different source (use material.connect_nodes, which replaces the current wire).'],
      examples: [ex('Break the A input of a multiply', { materialPath: M, nodeId: MULTIPLY, pinName: 'A' }, DONE)] }),
  r('find_node', 'material', 'Find a node in a material graph by type or name.', schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath, as the sibling read actions spell it).'), nodeType: str('Node type to find.'), nodeName: str('Name to find: a node id, parameter name, function input or output name, or custom node title (substring match).') }, [], ['materialPath', 'assetPath']), OK, READ, READ_POLICY, LOW,
    { whenToUse: ['A node id is unknown and the node must be found by expression class, parameter name, function pin name or Custom node title.'],
      whenNotToUse: ['The material asset itself must be located by name or class (use asset.query_asset).'],
      examples: [ex('Locate every texture sample', { materialPath: M, nodeType: 'TextureSample' }, DONE)] }),
  r('get_node_connections', 'material', 'Retrieve connections for a node in a material graph.', schema({ materialPath: MAT, assetPath: MAT_ALIAS, nodeId: str('Node ID.'), direction: str('Connection direction to report (inputs or outputs).'), depth: num('Traversal depth; -1 walks the whole graph.'), upstream: bool('Walk every upstream producer, overriding direction and depth.'), downstream: bool('Report downstream connections instead of upstream.') }, ['nodeId'], ['materialPath', 'assetPath']), CONNECTIONS_OUT, READ, READ_POLICY, LOW,
    { whenToUse: ['The wiring of a node must be traced across the graph, for example to see where its result ends up at the material output.'],
      whenNotToUse: ['A wire must be added, cut or rerouted, not traced (use material.connect_nodes or material.disconnect_nodes).'],
      examples: [ex('List what feeds a multiply node', { materialPath: M, nodeId: MULTIPLY, direction: 'inputs', downstream: false }, DONE)] }),
  r('get_node_properties', 'material', 'Retrieve properties of a node in a material graph.', schema({ materialPath: MAT, assetPath: MAT_ALIAS, nodeId: str('Node ID.') }, ['nodeId'], ['materialPath', 'assetPath']), OK, READ, READ_POLICY, LOW,
    { whenToUse: ['The settings of one node must be read back, such as a scalar or vector parameter default or a function input or output pin.'],
      whenNotToUse: ['A parameter value or Custom node code must change, not be read (use material.set_material_parameter or material.update_custom_expression).', 'Any other property of a node must change, such as the Texture or SamplerType of a texture parameter (use inspect.set_property with objectPath "<material path>.<material name>:<nodeId>").'],
      examples: [ex('Read a texture sample node\'s properties', { materialPath: M, nodeId: SAMPLE }, DONE)] }),
  r('set_static_switch_parameter_value', 'material', 'Set a static switch parameter value on a material instance.', schema({ assetPath: str('Material instance /Game asset path.'), parameterName: str('Parameter name.'), value: bool('Switch value.'), save: SAVE }, ['assetPath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A static switch parameter must be turned on or off; on an instance this rebuilds the shader permutation.'],
      whenNotToUse: ['The parameter is not on the base material yet (add it there with asset.edit_material_instance edit=add_parameter or material.add_material_node).'],
      examples: [ex('Enable a detail-map switch on an instance', { assetPath: '/Game/Materials/MI_Base_Rusty', parameterName: 'UseDetailMap', value: true }, DONE)] }),
  r('delete_node', 'material', 'Delete one node, or several with nodeIds, from a material graph; the material is recompiled and saved.', schema({ materialPath: MAT, nodeId: str('Node ID to delete.'), nodeIds: arr('Node IDs to delete in one batch, in place of nodeId.'), save: SAVE }, ['materialPath'], ['nodeId', 'nodeIds']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['An unused or wrong node must leave a material or function graph, taking its wires with it.', 'Several stray nodes must go in one call (nodeIds).'],
      whenNotToUse: ['Only the wire to a node must be cleared while the node stays (use material.disconnect_nodes).', 'Unused nodes must be found first (use material.get_material_info with info=subgraph and orphansOnly=true).'],
      examples: [ex('Delete an unused multiply node', { materialPath: M, nodeId: MULTIPLY }, DONE)] }),
  r('update_custom_expression', 'material', 'Update a custom expression node: its HLSL code, inputs, output type, additional outputs or title. An input that keeps its name keeps its wire. The material is then compiled and saved, and compiled, compileErrors and saved in the reply say how that went (a step of build_material_graph leaves both to the batch).', schema({ materialPath: MAT, nodeId: str('Node ID.'), code: str('Updated HLSL code.'), inputs: arrObj('Replacement input list, each {name}; inputs whose names stay keep their connections.'), outputType: str('Output type: Float1, Float2, Float3, Float4 or MaterialAttributes.'), description: str('Node title shown in the material editor.'), additionalOutputs: arrObj('Extra output pins after the return value, each {name, type}: type Float1 (default), Float2, Float3, Float4 or MaterialAttributes. Assign each by name in the HLSL (Emis = ...;) and wire it as "$node.Name".') }, ['materialPath', 'nodeId', 'code']), CUSTOM_UPDATE_OUT, WRITE, WRITE_POLICY, LOW,
    { topics: ['edit shader code', 'custom hlsl code'],
      whenToUse: ['The HLSL code of an existing Custom node must be rewritten without rebuilding its wiring.', 'Inputs, output type, extra outputs or the title of a Custom node must change; inputs that keep their name keep their wire.'],
      whenNotToUse: ['A new Custom node is needed (use material.add_material_node with nodeKind=custom_expression).', 'The node is a parameter or any other non-Custom expression; only Custom nodes are accepted (parameter values use material.set_material_parameter).'],
      examples: [ex('Rewrite a custom node\'s HLSL', { materialPath: M, nodeId: 'MaterialExpressionCustom_0', code: 'return saturate(A * 3.0f);' }, DONE)] }),
  r('get_node_chain', 'material', 'Retrieve the chain of nodes connected to a starting node.', schema({ materialPath: MAT, assetPath: MAT_ALIAS, nodeId: str('Starting node ID.'), startNodeId: str('Starting node ID accepted by the handler in place of nodeId.'), endPin: str('Terminal pin name to stop the chain walk at.') }, ['nodeId'], ['materialPath', 'assetPath']), OK, READ, READ_POLICY, LOW,
    { whenToUse: ['The wiring of a node must be traced across the graph, for example to see where its result ends up at the material output.'],
      whenNotToUse: ['A wire must be added, cut or rerouted, not traced (use material.connect_nodes or material.disconnect_nodes).'],
      examples: [ex('Walk the chain feeding BaseColor', { materialPath: M, nodeId: SAMPLE, endPin: 'BaseColor' }, DONE)] }),
  r('get_connected_subgraph', 'material', 'Retrieve the connected subgraph from a starting node.', schema({ materialPath: MAT, assetPath: MAT_ALIAS, nodeId: str('Starting node ID.'), orphansOnly: bool('Report only orphaned nodes; accepted in place of nodeId.') }, ['nodeId'], ['materialPath', 'assetPath']), OK, READ, READ_POLICY, LOW,
    { whenToUse: ['Nodes not wired to any output (orphans) must be found, or the connected island around one node listed.'],
      whenNotToUse: ['Orphaned nodes are to be removed (use material.delete_node once their ids are known).'],
      examples: [ex('Collect the subgraph under a node', { materialPath: M, nodeId: SAMPLE, orphansOnly: false }, DONE)] }),
  r('add_material_node', 'material', 'Add a generic material node by type.', schema({ materialPath: MAT, nodeType: str('Node type.'), type: str('Node type (alias of nodeType).'), name: str('Parameter name, when the node type is a parameter of any kind (scalar, vector, static switch, TextureObjectParameter, TextureSampleParameter2D).'), texturePath: str('Texture for a texture node (TextureObjectParameter, TextureSample...): its default, with the sampler type set to match (a normal map samples as Normal). A path that does not load adds nothing and fails.'), defaultValue: { description: 'Initial value for a Constant (number) or Constant3Vector (rgb array or {r,g,b}) node.' }, posX: num('Node X position.'), posY: num('Node Y position.'), x: num('Node X position (preferred spelling; posX is the fallback).'), y: num('Node Y position (preferred spelling; posY is the fallback).') }, ['materialPath', 'nodeType']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['An expression is needed by class name with no dedicated adder, such as Constant, Constant3Vector (with defaultValue), ComponentMask or Saturate.', 'A texture object must feed a Custom node or a material function input (nodeType TextureObjectParameter with name and texturePath).'],
      whenNotToUse: ['The node already exists and only needs wiring or moving (use material.connect_nodes or material.set_node_position).'],
      examples: [ex('Add a Constant3Vector by type name', { materialPath: M, nodeType: 'Constant3Vector', x: -300, y: 100 }, DONE)] }),
  r('rebuild_material', 'material', 'Rebuild and compile a material (alias of compile_material).', schema({ materialPath: MAT, assetPath: MAT_ALIAS, save: SAVE }, [], ['materialPath', 'assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['The shader must be regenerated after graph edits; the compile, error check and save are identical to a plain compile.'],
      whenNotToUse: ['A property or parameter was just set (material.set_material_property and material.set_material_parameter already recompile).'],
      dispatchAction: 'rebuild_material', examples: [ex('Rebuild after graph edits', { materialPath: M }, DONE)] }),
  r('set_material_parameter', 'material', 'Set a material parameter value, or several at once with parameters.', schema({ assetPath: MAT, parameterName: str('Parameter name.'), parameterType: str('Parameter kind: scalar (default), vector, or texture. Selects which parameter expression the value is written to.'), value: { description: 'Parameter value.' }, texturePath: str('Texture /Game path, for parameterType texture.'), parameters: MATERIAL_PARAMETER_LIST, save: SAVE }, ['assetPath'], ['parameterName', 'parameters']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['Several parameter values on a base material or an instance must change in one call (parameters list).', 'The default of a parameter node on a base material must change; on an instance the value is stored as an override.'],
      whenNotToUse: ['The parameter is not on the base material yet (add it there with asset.edit_material_instance edit=add_parameter or material.add_material_node).', 'Every override on an instance must be cleared back to its parent (use asset.edit_material_instance edit=reset_parameters).'],
      topics: ['material parameter', 'set parameter', 'scalar parameter', 'vector parameter', 'texture parameter'], examples: [ex('Set a roughness parameter', { assetPath: M, parameterName: 'Roughness', value: 0.35 }, DONE)] }),
  // assetPath is the spelling the handler reads. Declaring only materialPath made this
  // capability uncallable by any input: the schema-correct call died in the handler, the
  // handler-correct call failed schema validation, and sending both was rejected as
  // undeclared. Accept either, exactly as connect_nodes does.
  r('get_material_node_details', 'material', 'Retrieve details of a material node.', schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath).'), nodeId: str('Node ID.'), expressionIndex: num('Zero-based position in the expression list; picks the node when nodeId is omitted.') }, [], ['materialPath', 'assetPath']), NODE_DETAILS_OUT, READ, READ_POLICY, LOW,
    { whenToUse: ['The settings of one node must be read back, such as a scalar or vector parameter default or a function input or output pin.'],
      whenNotToUse: ['A parameter value or Custom node code must change, not be read (use material.set_material_parameter or material.update_custom_expression).', 'Any other property of a node must change, such as the Texture or SamplerType of a texture parameter (use inspect.set_property with objectPath "<material path>.<material name>:<nodeId>").'],
      examples: [ex('Inspect one node in detail', { materialPath: M, nodeId: SAMPLE, expressionIndex: 0 }, DONE)] }),
  r('remove_material_node', 'material', 'Remove a node from a material graph.', schema({ materialPath: MAT, nodeId: str('Node ID to remove.'), nodeIds: arr('Node IDs to remove in one batch, in place of nodeId.') }, ['materialPath'], ['nodeId', 'nodeIds']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['An unused or wrong node must leave a material or function graph, taking its wires with it.', 'Several stray nodes must go in one call (nodeIds).'],
      whenNotToUse: ['Only the wire to a node must be cleared while the node stays (use material.disconnect_nodes).', 'Unused nodes must be found first (use material.get_material_info with info=subgraph and orphansOnly=true).'],
      examples: [ex('Remove a node from the graph', { materialPath: M, nodeId: SAMPLE }, DONE)] }),
  // Nodes could be placed at a coordinate but never moved, so a graph laid out
  // badly stayed that way — the only recourse was remove plus re-add, which
  // drops the node's connections. This is what makes the overlap warning the
  // node adders now return actionable after the fact.
  r('set_node_position', 'material', 'Move an existing material graph node to new coordinates, preserving its connections.',
    schema({ materialPath: MAT, assetPath: str('Material asset path (accepted in place of materialPath).'), nodeId: str('Node ID to move.'), x: num('New X coordinate (posX is the fallback spelling).'), y: num('New Y coordinate (posY is the fallback spelling).'), posX: num('New X coordinate (fallback spelling).'), posY: num('New Y coordinate (fallback spelling).') }, ['nodeId'], ['materialPath', 'assetPath']),
    MOVE_OUT, WRITE, WRITE_POLICY, LOW,
    { topics: ['move material node'],
      whenToUse: ['Overlapping nodes reported by a node add (overlappingNodes) must be spread out without losing wires.', 'A cluttered graph must be tidied by moving an existing node to new x and y.'],
      whenNotToUse: ['A new node needs a position (pass x and y when adding it with material.add_material_node).', 'Wires must change (use material.connect_nodes or material.disconnect_nodes).'],
      examples: [ex('Space a stacked parameter node out', { materialPath: M, nodeId: SAMPLE, x: -400, y: 260 }, { success: true, nodeId: SAMPLE, posX: -400, posY: 260 })] }),
  r('build_material_graph', 'material', 'Build a material graph in one call: add nodes, wire them to each other and to the material output, and set material properties, with $id references between steps.',
    schema({ materialPath: MAT, operations: OPERATIONS }, ['materialPath', 'operations']), BATCH_OUT, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['Many nodes, wires and material properties must be built at once, with the material compiled and saved only at the end.', 'New nodes must be wired to each other in the same call by id ($id) without waiting for node ids.'],
      whenNotToUse: ['Only one wire is needed (use material.connect_nodes).', 'Nodes must be deleted or unwired; batches do not run those (use material.delete_node or material.disconnect_nodes).'],
      topics: ['batch material edit', 'build material graph', 'wire many material nodes', 'material graph batch'],
      examples: [ex('Emissive vertical gradient from two colors', { materialPath: M, operations: [
        { edit: 'add_texture_coordinate', id: 'uv' },
        { edit: 'add_vector_parameter', id: 'top', parameterName: 'TopColor', defaultValue: { r: 0.01, g: 0.02, b: 0.08, a: 1 } },
        { edit: 'add_vector_parameter', id: 'horizon', parameterName: 'HorizonColor', defaultValue: { r: 0.9, g: 0.2, b: 0.6, a: 1 } },
        { edit: 'add_material_node', id: 'lerp', nodeType: 'Lerp' },
        { edit: 'connect_nodes', from: '$top', to: '$lerp.A' },
        { edit: 'connect_nodes', from: '$horizon', to: '$lerp.B' },
        { edit: 'connect_nodes', from: '$uv.G', to: '$lerp.Alpha' },
        { edit: 'connect_nodes', from: '$lerp', to: 'Main.EmissiveColor' },
      ] }, { success: true, succeeded: 8, nodeIds: { uv: 'MaterialExpressionTextureCoordinate_0', top: 'MaterialExpressionVectorParameter_0', horizon: 'MaterialExpressionVectorParameter_1', lerp: 'MaterialExpressionLinearInterpolate_0' }, compiled: true, saved: true })] })
];
