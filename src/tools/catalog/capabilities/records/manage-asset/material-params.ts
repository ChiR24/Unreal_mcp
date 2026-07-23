// Material parameter, property, function, and instance records.

import type { RecordSpec } from './builder.js';
import { aliasCanonical, arrObj, bool, ex, LOW, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const MAT = str('Material /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const M = '/Game/Materials/M_Base';
const MI = '/Game/Materials/MI_Base_Rusty';
const MF = '/Game/Materials/Functions/MF_HeightBlend';
const DONE = { success: true };

export const MATERIAL_PARAMS_RECORDS: readonly RecordSpec[] = [
  r('set_blend_mode', 'material', 'Set the blend mode of a material.', schema({ materialPath: MAT, blendMode: str('Blend mode.') }, ['materialPath', 'blendMode']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Switch a material to masked blending', { materialPath: M, blendMode: 'Masked' }, DONE)] }),
  r('set_shading_model', 'material', 'Set the shading model of a material.', schema({ materialPath: MAT, shadingModel: str('Shading model.') }, ['materialPath', 'shadingModel']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Use the default lit shading model', { materialPath: M, shadingModel: 'DefaultLit' }, DONE)] }),
  r('set_material_domain', 'material', 'Set the material domain of a material.', schema({ materialPath: MAT, domain: str('Material domain.') }, ['materialPath', 'domain']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Keep a material in the surface domain', { materialPath: M, domain: 'Surface' }, DONE)] }),
  r('compile_material', 'material', 'Compile a material.', schema({ materialPath: MAT }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', normalization: aliasCanonical('rebuild_material'), examples: [ex('Compile after editing the graph', { materialPath: M }, DONE)] }),
  r('get_material_info', 'material', 'Retrieve material information.', schema({ materialPath: MAT }, ['materialPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Read a material\'s configuration', { materialPath: M }, DONE)] }),
  r('set_two_sided', 'material', 'Set the two-sided flag on a material.', schema({ materialPath: MAT, value: bool('Two-sided value.') }, ['materialPath', 'value']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Render a material from both sides', { materialPath: M, value: true }, DONE)] }),
  r('add_function_input', 'material', 'Add a function input to a material function.', schema({ functionPath: str('Material function /Game path.'), name: str('Input name.'), inputType: str('Input type.'), x: num('Node X position (preferred spelling; posX is the fallback).'), y: num('Node Y position (preferred spelling; posY is the fallback).') }, ['functionPath', 'name']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add a scalar height input', { functionPath: MF, name: 'Height', inputType: 'Scalar', x: -400, y: 0 }, DONE)] }),
  r('add_function_output', 'material', 'Add a function output to a material function.', schema({ functionPath: str('Material function /Game path.'), name: str('Output name.'), inputType: str('Output type.'), x: num('Node X position (preferred spelling; posX is the fallback).'), y: num('Node Y position (preferred spelling; posY is the fallback).') }, ['functionPath', 'name']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add the blended result output', { functionPath: MF, name: 'Result', inputType: 'Scalar', x: 400, y: 0 }, DONE)] }),
  r('use_material_function', 'material', 'Insert a material function reference into a material graph.', schema({ materialPath: MAT, functionPath: str('Material function /Game path.'), x: num('Node X position (preferred spelling; posX is the fallback).'), y: num('Node Y position (preferred spelling; posY is the fallback).') }, ['materialPath', 'functionPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Reference a blend function from a material', { materialPath: M, functionPath: MF, x: -200, y: 500 }, DONE)] }),
  r('get_material_function_info', 'material', 'Retrieve information about a material function.', schema({ functionPath: str('Material function /Game path.') }, ['functionPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Read a function\'s inputs and outputs', { functionPath: MF }, DONE)] }),
  r('set_scalar_parameter_value', 'material', 'Set a scalar parameter value on a material instance.', schema({ instancePath: str('Material instance /Game path.'), parameterName: str('Parameter name.'), value: num('Scalar value.') }, ['instancePath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Override roughness on an instance', { instancePath: MI, parameterName: 'Roughness', value: 0.8 }, DONE)] }),
  r('set_vector_parameter_value', 'material', 'Set a vector parameter value on a material instance.', schema({ instancePath: str('Material instance /Game path.'), parameterName: str('Parameter name.'), value: { description: 'Vector value.' } }, ['instancePath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Tint an instance rust-orange', { instancePath: MI, parameterName: 'BaseTint', value: [0.55, 0.27, 0.1, 1] }, DONE)] }),
  r('set_texture_parameter_value', 'material', 'Set a texture parameter value on a material instance.', schema({ instancePath: str('Material instance /Game path.'), parameterName: str('Parameter name.'), texturePath: str('Texture /Game path.') }, ['instancePath', 'parameterName', 'texturePath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Swap the base colour texture', { instancePath: MI, parameterName: 'BaseColor', texturePath: '/Game/Textures/T_Rust' }, DONE)] }),
  r('add_landscape_layer', 'material', 'Add a landscape layer to a landscape material.', schema({ materialPath: MAT, layerName: str('Layer name.') }, ['materialPath', 'layerName']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Add a grass weight-blended layer', { materialPath: '/Game/Materials/Landscape/M_Landscape', layerName: 'Grass' }, DONE)] }),
  r('configure_layer_blend', 'material', 'Configure layer blend settings on a landscape material.', schema({ materialPath: MAT, layers: arrObj('Layer blend definitions.'), blendType: str('Blend type.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Weight-blend the landscape layers', { materialPath: '/Game/Materials/Landscape/M_Landscape', blendType: 'LB_WeightBlend' }, DONE)] })
];
