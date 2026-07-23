// Material node creation records: texture samples, parameters, math nodes,
// scene data nodes, and conditional/custom expression nodes.

import type { JsonObject } from '../../model.js';
import type { RecordSpec } from './builder.js';
import { arrObj, bool, ex, LOW, num, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const MAT = str('Material /Game asset path.');
const TEX = str('Texture /Game asset path.');
const PARAM = str('Parameter name.');
// The material-authoring handlers read `x`/`y` first and fall back to `posX`/`posY`,
// so both spellings are part of the node-placement contract.
const X = num('Node X position (preferred spelling; posX is the fallback).');
const Y = num('Node Y position (preferred spelling; posY is the fallback).');
const OK = schema({ success: bool('Operation succeeded.'), nodeId: str('Created node ID.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Node details.' } }, ['success']);

const M = '/Game/Materials/M_Base';
// nodeId is MCP_NODE_ID(Expr) == UObject::GetName(), e.g. "MaterialExpressionCustom_0".
const node = (expression: string): JsonObject => ({ success: true, nodeId: `MaterialExpression${expression}_0` });

export const MATERIAL_NODES_RECORDS: readonly RecordSpec[] = [
  r('add_texture_sample', 'material', 'Add a texture sample node to a material graph.', schema({ materialPath: MAT, texturePath: TEX, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Sample a rock texture', { materialPath: M, texturePath: '/Game/Textures/T_Rock', x: -400, y: 0 }, node('TextureSample'))] }),
  r('add_texture_coordinate', 'material', 'Add a texture coordinate node to a material graph.', schema({ materialPath: MAT, coordinateIndex: num('UV channel index (default 0).'), uTiling: num('U tiling factor (default 1).'), vTiling: num('V tiling factor (default 1).'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Tile UV channel 0 four times', { materialPath: M, coordinateIndex: 0, uTiling: 4, vTiling: 4, x: -600, y: 0 }, node('TextureCoordinate'))] }),
  r('add_scalar_parameter', 'material', 'Add a scalar parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, group: str('Parameter group.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Expose a roughness scalar', { materialPath: M, parameterName: 'Roughness', group: 'Surface', x: -400, y: 200 }, node('ScalarParameter'))] }),
  r('add_vector_parameter', 'material', 'Add a vector parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, defaultValue: { description: 'Default RGBA value.' }, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Expose a tint colour', { materialPath: M, parameterName: 'BaseTint', defaultValue: [1, 1, 1, 1], x: -400, y: 300 }, node('VectorParameter'))] }),
  r('add_static_switch_parameter', 'material', 'Add a static switch parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, value: bool('Default switch value.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add a detail-map toggle', { materialPath: M, parameterName: 'UseDetailMap', value: false, x: -400, y: 400 }, node('StaticSwitchParameter'))] }),
  r('add_math_node', 'material', 'Add a math operation node to a material graph.', schema({ materialPath: MAT, operation: str('Math operation (Add, Multiply, etc.).'), constA: num('Constant A.'), constB: num('Constant B.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'operation']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Halve a value with a multiply', { materialPath: M, operation: 'Multiply', constA: 1, constB: 0.5, x: -200, y: 0 }, node('Multiply'))] }),
  r('add_world_position', 'material', 'Add a world position node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Drive shading from world position', { materialPath: M, x: -800, y: 0 }, node('WorldPosition'))] }),
  r('add_vertex_normal', 'material', 'Add a vertex normal node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Read the world-space vertex normal', { materialPath: M, x: -800, y: 200 }, node('VertexNormalWS'))] }),
  r('add_pixel_depth', 'material', 'Add a pixel depth node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Fade by pixel depth', { materialPath: M, x: -800, y: 400 }, node('PixelDepth'))] }),
  r('add_fresnel', 'material', 'Add a fresnel node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add a rim-light fresnel', { materialPath: M, x: -600, y: 600 }, node('Fresnel'))] }),
  r('add_reflection_vector', 'material', 'Add a reflection vector node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Sample a cubemap by reflection vector', { materialPath: M, x: -800, y: 600 }, node('ReflectionVectorWS'))] }),
  r('add_panner', 'material', 'Add a panner node to a material graph.', schema({ materialPath: MAT, speedX: num('Pan speed X.'), speedY: num('Pan speed Y.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Scroll UVs horizontally', { materialPath: M, speedX: 0.1, speedY: 0, x: -600, y: 200 }, node('Panner'))] }),
  r('add_rotator', 'material', 'Add a rotator node to a material graph.', schema({ materialPath: MAT, speed: num('Rotation speed.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Rotate UVs slowly', { materialPath: M, speed: 0.25, x: -600, y: 300 }, node('Rotator'))] }),
  r('add_noise', 'material', 'Add a noise node to a material graph.', schema({ materialPath: MAT, scale: num('Noise scale.'), octaves: num('Noise octaves.'), levels: num('Noise level count.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add three-octave noise', { materialPath: M, scale: 4, octaves: 3, levels: 3, x: -600, y: 400 }, node('Noise'))] }),
  r('add_voronoi', 'material', 'Add a voronoi noise node to a material graph.', schema({ materialPath: MAT, scale: num('Voronoi scale.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add a voronoi cell pattern', { materialPath: M, scale: 8, x: -600, y: 500 }, { success: true })] }),
  r('add_if', 'material', 'Add a conditional If node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Branch between two inputs', { materialPath: M, x: -200, y: 200 }, node('If'))] }),
  r('add_switch', 'material', 'Add a switch node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Select between graph branches', { materialPath: M, x: -200, y: 300 }, node('Switch'))] }),
  r('add_custom_expression', 'material', 'Add a custom HLSL expression node to a material graph.', schema({ materialPath: MAT, code: str('HLSL code.'), outputType: str('Output type.'), inputs: arrObj('Input definitions.'), additionalOutputs: arrObj('Additional named output definitions.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'code']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add a scalar HLSL expression', { materialPath: M, code: 'return saturate(A * 2.0f);', outputType: 'CMOT_Float1', x: -200, y: 400 }, node('Custom'))] })
];
