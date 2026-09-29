// Material node creation records: texture samples, parameters, math nodes,
// scene data nodes, and conditional/custom expression nodes.

import type { JsonObject } from '../../model.js';
import type { RecordSpec } from './builder.js';
import { arrObj, bool, ex, LOW, num, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const MAT = str('Material /Game asset path.');
const TEX = str('Texture /Game asset path.');
const PARAM = str('Parameter name.');
// The material-authoring handlers read `x`/`y` first and fall back to `posX`/`posY`,
// so both spellings are part of the node-placement contract.
const X = num('Node X position (preferred spelling; posX is the fallback).');
const Y = num('Node Y position (preferred spelling; posY is the fallback).');
// Placement is echoed back because callers position expressions by coordinate
// and previously got nothing to lay out against, so successive parameter nodes
// stacked on top of one another in the graph. The size fields are estimates:
// Slate computes a node's real extent when the material editor draws it, which
// does not happen on this path.
const OK = schema({
  success: bool('Operation succeeded.'),
  nodeId: str('Created node ID.'),
  details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Node details.' },
  posX: num('X coordinate the node was placed at.'),
  posY: num('Y coordinate the node was placed at.'),
  estimatedWidth: num('Approximate node width; an estimate from the node name, not a measurement.'),
  estimatedHeight: num('Approximate node height, from connector count plus an allowance for a parameter node\'s inline default-value widget. Offset the next node by at least this much to avoid overlap.'),
  overlappingNodes: { type: 'array', items: { type: 'string' }, description: 'Names of existing expressions whose estimated bounds intersect this one. Absent when placement is clear.' },
  placementWarning: str('Human-readable overlap warning, present only when overlappingNodes is non-empty.'),
}, ['success']);

const M = '/Game/Materials/M_Base';
// nodeId is MCP_NODE_ID(Expr) == UObject::GetName(), e.g. "MaterialExpressionCustom_0".
const node = (expression: string): JsonObject => ({ success: true, nodeId: `MaterialExpression${expression}_0` });

export const MATERIAL_NODES_RECORDS: readonly RecordSpec[] = [
  r('add_texture_sample', 'material', 'Add a texture sample node to a material graph.', schema({ materialPath: MAT, texturePath: TEX, parameterName: str('Makes the node a TextureSampleParameter2D with this parameter name, overridable per instance.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A texture must be sampled in the graph; pass parameterName to make it a texture parameter that instances can override.'],
      whenNotToUse: ['An instance only needs a different texture (use material.set_material_parameter with parameterKind=texture).', 'The texture asset itself must be made, adjusted or configured (use texture.create_texture, texture.adjust_texture or texture.configure_texture).'],
      examples: [ex('Sample a rock texture', { materialPath: M, texturePath: '/Game/Textures/T_Rock', x: -400, y: 0 }, node('TextureSample'))] }),
  r('add_texture_coordinate', 'material', 'Add a texture coordinate node to a material graph.', schema({ materialPath: MAT, coordinateIndex: num('UV channel index (default 0).'), uTiling: num('U tiling factor (default 1).'), vTiling: num('V tiling factor (default 1).'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A texture must repeat more or less often across a surface, or read a UV channel other than 0.'],
      whenNotToUse: ['The UV node exists and only needs wiring (use material.connect_nodes).'],
      examples: [ex('Tile UV channel 0 four times', { materialPath: M, coordinateIndex: 0, uTiling: 4, vTiling: 4, x: -600, y: 0 }, node('TextureCoordinate'))] }),
  // defaultValue mirrors add_vector_parameter. The handler has always read it; leaving it out
  // of the schema meant additionalProperties:false stripped it, so every scalar authored
  // through the gateway landed on 0.0 — a Roughness of 0 being a mirror, not a sane default.
  r('add_scalar_parameter', 'material', 'Add a scalar parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, defaultValue: num('Default scalar value.'), group: str('Parameter group.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A float value must be tweakable per material instance, such as roughness, opacity or a speed.'],
      whenNotToUse: ['An existing scalar parameter value must change (use material.set_material_parameter with parameterKind=scalar).'],
      examples: [ex('Expose a roughness scalar', { materialPath: M, parameterName: 'Roughness', defaultValue: 0.5, group: 'Surface', x: -400, y: 200 }, node('ScalarParameter'))] }),
  r('add_vector_parameter', 'material', 'Add a vector parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, defaultValue: { description: 'Default RGBA value.' }, group: str('Parameter group.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A color or vector value must be tweakable per instance, such as a tint or an emissive color.'],
      whenNotToUse: ['An existing vector parameter value must change (use material.set_material_parameter with parameterKind=vector).'],
      examples: [ex('Expose a tint colour', { materialPath: M, parameterName: 'BaseTint', defaultValue: [1, 1, 1, 1], x: -400, y: 300 }, node('VectorParameter'))] }),
  r('add_static_switch_parameter', 'material', 'Add a static switch parameter node to a material graph.', schema({ materialPath: MAT, parameterName: PARAM, defaultValue: bool('Default switch value (default false).'), group: str('Parameter group.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'parameterName']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A compile-time on/off toggle must pick between two graph branches for each instance.'],
      whenNotToUse: ['The value of an existing switch must change on an instance (use material.set_material_parameter with parameterKind=static_switch).'],
      examples: [ex('Add a detail-map toggle', { materialPath: M, parameterName: 'UseDetailMap', defaultValue: false, x: -400, y: 400 }, node('StaticSwitchParameter'))] }),
  r('add_math_node', 'material', 'Add a math operation node to a material graph.', schema({ materialPath: MAT, operation: str('Math operation (Add, Multiply, etc.).'), constA: num('Value used while input A is unwired (Add, Subtract, Multiply, Divide, Lerp).'), constB: num('Value used while input B is unwired (Add, Subtract, Multiply, Divide, Lerp).'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'operation']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['Values must be combined or reshaped: Add, Subtract, Multiply, Divide, Lerp, Clamp, Power, Frac, OneMinus or Append.'],
      whenNotToUse: ['A math node already exists and only needs new wires (use material.connect_nodes).'],
      examples: [ex('Halve a value with a multiply', { materialPath: M, operation: 'Multiply', constA: 1, constB: 0.5, x: -200, y: 0 }, node('Multiply'))] }),
  r('add_world_position', 'material', 'Add a world position node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['The world position of each pixel is needed, for world-aligned textures or height-based blending.'],
      whenNotToUse: ['The world position node exists and only needs wiring (use material.connect_nodes).'],
      examples: [ex('Drive shading from world position', { materialPath: M, x: -800, y: 0 }, node('WorldPosition'))] }),
  r('add_vertex_normal', 'material', 'Add a vertex normal node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['The world-space vertex normal is needed, for example for slope-based blending.'],
      whenNotToUse: ['A normal map should shade the surface: wire a texture sample into Normal (use material.connect_nodes).'],
      examples: [ex('Read the world-space vertex normal', { materialPath: M, x: -800, y: 200 }, node('VertexNormalWS'))] }),
  r('add_pixel_depth', 'material', 'Add a pixel depth node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['Distance from the camera per pixel is needed, for depth fades or distance-based blending.'],
      whenNotToUse: ['A pixel depth node exists and only needs wiring (use material.connect_nodes).'],
      examples: [ex('Fade by pixel depth', { materialPath: M, x: -800, y: 400 }, node('PixelDepth'))] }),
  r('add_fresnel', 'material', 'Add a fresnel node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A rim light, edge glow or view-angle fade is needed.'],
      whenNotToUse: ['The Fresnel node already exists and only needs wiring (use material.connect_nodes).'],
      examples: [ex('Add a rim-light fresnel', { materialPath: M, x: -600, y: 600 }, node('Fresnel'))] }),
  r('add_reflection_vector', 'material', 'Add a reflection vector node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A reflection direction is needed, for example to sample a cubemap for fake reflections.'],
      whenNotToUse: ['The reflection vector node exists and only needs wiring (use material.connect_nodes).'],
      examples: [ex('Sample a cubemap by reflection vector', { materialPath: M, x: -800, y: 600 }, node('ReflectionVectorWS'))] }),
  r('add_panner', 'material', 'Add a panner node to a material graph.', schema({ materialPath: MAT, speedX: num('Pan speed along U.'), speedY: num('Pan speed along V.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['UVs must scroll over time, as with flowing water or a conveyor belt.'],
      whenNotToUse: ['The panner exists and only its wiring or position changes (use material.connect_nodes or material.set_node_position).'],
      examples: [ex('Scroll UVs horizontally', { materialPath: M, speedX: 0.1, speedY: 0, x: -600, y: 200 }, node('Panner'))] }),
  r('add_rotator', 'material', 'Add a rotator node to a material graph.', schema({ materialPath: MAT, speed: num('Rotation speed.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['UVs must rotate over time, as with spinning dials or radial effects.'],
      whenNotToUse: ['The rotator exists and only needs wiring (use material.connect_nodes).'],
      examples: [ex('Rotate UVs slowly', { materialPath: M, speed: 0.25, x: -600, y: 300 }, node('Rotator'))] }),
  r('add_noise', 'material', 'Add a noise node to a material graph.', schema({ materialPath: MAT, scale: num('Noise scale.'), levels: num('Number of noise octaves (levels) combined.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A surface needs procedural variation such as dirt, clouds or breakup, with a scale and a number of octaves (levels).'],
      whenNotToUse: ['A noise texture asset is wanted instead of a per-pixel node (use texture.create_texture with kind=noise).'],
      examples: [ex('Add three-octave noise', { materialPath: M, scale: 4, levels: 3, x: -600, y: 400 }, node('Noise'))] }),
  r('add_voronoi', 'material', 'Add a voronoi noise node to a material graph.', schema({ materialPath: MAT, scale: num('Voronoi scale.'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A cell, crack or cobble-like pattern is needed from a Voronoi noise function; scale is its only setting.'],
      whenNotToUse: ['The Voronoi node exists and only needs wiring (use material.connect_nodes).'],
      examples: [ex('Add a voronoi cell pattern', { materialPath: M, scale: 8, x: -600, y: 500 }, { success: true })] }),
  r('add_if', 'material', 'Add a conditional If node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A value must switch by comparing two numbers: A greater than, equal to or less than B.'],
      whenNotToUse: ['A per-instance toggle is wanted (add a static switch parameter, then use material.set_material_parameter with parameterKind=static_switch).'],
      examples: [ex('Branch between two inputs', { materialPath: M, x: -200, y: 200 }, node('If'))] }),
  r('add_switch', 'material', 'Add a switch node to a material graph.', schema({ materialPath: MAT, posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A compile-time two-way branch is needed with no named parameter, chosen by a bool input or the node default.'],
      whenNotToUse: ['The choice must change per instance (use a static switch parameter, set with material.set_material_parameter and parameterKind=static_switch).'],
      examples: [ex('Select between graph branches', { materialPath: M, x: -200, y: 300 }, node('Switch'))] }),
  r('add_custom_expression', 'material', 'Add a custom HLSL expression node to a material graph.', schema({ materialPath: MAT, code: str('HLSL code.'), outputType: str('Output type: Float1 (default), Float2, Float3, Float4 or MaterialAttributes.'), description: str('Node title shown in the material editor (the Custom node Description).'), inputs: arrObj('Input definitions, each {name}; wire one with connect_nodes targetPin set to that name.'), additionalOutputs: arrObj('Extra output pins after the return value, each {name, type}: type Float1 (default), Float2, Float3, Float4 or MaterialAttributes. Assign each by name in the HLSL (Emis = ...;) and wire it as "$node.Name".'), posX: num('Node X position.'), posY: num('Node Y position.'), x: X, y: Y }, ['materialPath', 'code']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A calculation the standard nodes cannot express is needed as HLSL, with named inputs and optional extra outputs.'],
      whenNotToUse: ['The code or inputs of an existing Custom node must change (use material.update_custom_expression).'],
      examples: [ex('Add a scalar HLSL expression', { materialPath: M, code: 'return saturate(A * 2.0f);', outputType: 'CMOT_Float1', x: -200, y: 400 }, node('Custom'))] })
];
