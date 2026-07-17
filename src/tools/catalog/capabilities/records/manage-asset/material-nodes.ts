// Material node creation records: texture samples, parameters, math nodes,
// scene data nodes, and conditional/custom expression nodes.

import type { RecordSpec } from './builder.js';
import { arrObj, bool, LOW, num, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const MAT = str('Material /Game asset path.');
const TEX = str('Texture /Game asset path.');
const PARAM = str('Parameter name.');
const OK = schema({ success: bool('Operation succeeded.'), nodeId: str('Created node ID.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Node details.' } }, ['success']);

export const MATERIAL_NODES_RECORDS: readonly RecordSpec[] = [
  r('add_texture_sample', 'material', 'Add a texture sample node to a material graph.', schema({ materialPath: MAT, texturePath: TEX, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_texture_coordinate', 'material', 'Add a texture coordinate node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_scalar_parameter', 'material', 'Add a scalar parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, group: str('Parameter group.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_vector_parameter', 'material', 'Add a vector parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, defaultValue: { description: 'Default RGBA value.' }, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_static_switch_parameter', 'material', 'Add a static switch parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, value: bool('Default switch value.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_math_node', 'material', 'Add a math operation node to a material graph.', schema({ materialPath: MAT, operation: str('Math operation (Add, Multiply, etc.).'), constA: num('Constant A.'), constB: num('Constant B.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath', 'operation']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_world_position', 'material', 'Add a world position node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_vertex_normal', 'material', 'Add a vertex normal node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_pixel_depth', 'material', 'Add a pixel depth node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_fresnel', 'material', 'Add a fresnel node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_reflection_vector', 'material', 'Add a reflection vector node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_panner', 'material', 'Add a panner node to a material graph.', schema({ materialPath: MAT, speedX: num('Pan speed X.'), speedY: num('Pan speed Y.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_rotator', 'material', 'Add a rotator node to a material graph.', schema({ materialPath: MAT, speed: num('Rotation speed.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_noise', 'material', 'Add a noise node to a material graph.', schema({ materialPath: MAT, scale: num('Noise scale.'), octaves: num('Noise octaves.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_voronoi', 'material', 'Add a voronoi noise node to a material graph.', schema({ materialPath: MAT, scale: num('Voronoi scale.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_if', 'material', 'Add a conditional If node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_switch', 'material', 'Add a switch node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_custom_expression', 'material', 'Add a custom HLSL expression node to a material graph.', schema({ materialPath: MAT, code: str('HLSL code.'), outputType: str('Output type.'), inputs: arrObj('Input definitions.'), posX: num('Node X position.'), posY: num('Node Y position.') }, ['materialPath', 'code']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' })
];
