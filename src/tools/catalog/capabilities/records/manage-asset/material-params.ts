// Material parameter, property, function, and instance records.

import type { RecordSpec } from './builder.js';
import { aliasCanonical, arrObj, bool, LOW, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const MAT = str('Material /Game asset path.');
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const MATERIAL_PARAMS_RECORDS: readonly RecordSpec[] = [
  r('set_blend_mode', 'material', 'Set the blend mode of a material.', schema({ materialPath: MAT, blendMode: str('Blend mode.') }, ['materialPath', 'blendMode']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_shading_model', 'material', 'Set the shading model of a material.', schema({ materialPath: MAT, shadingModel: str('Shading model.') }, ['materialPath', 'shadingModel']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_material_domain', 'material', 'Set the material domain of a material.', schema({ materialPath: MAT, domain: str('Material domain.') }, ['materialPath', 'domain']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('compile_material', 'material', 'Compile a material.', schema({ materialPath: MAT }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool', normalization: aliasCanonical('rebuild_material') }),
  r('get_material_info', 'material', 'Retrieve material information.', schema({ materialPath: MAT }, ['materialPath']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_two_sided', 'material', 'Set the two-sided flag on a material.', schema({ materialPath: MAT, value: bool('Two-sided value.') }, ['materialPath', 'value']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_function_input', 'material', 'Add a function input to a material function.', schema({ functionPath: str('Material function /Game path.'), name: str('Input name.'), inputType: str('Input type.') }, ['functionPath', 'name']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_function_output', 'material', 'Add a function output to a material function.', schema({ functionPath: str('Material function /Game path.'), name: str('Output name.'), inputType: str('Output type.') }, ['functionPath', 'name']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('use_material_function', 'material', 'Insert a material function reference into a material graph.', schema({ materialPath: MAT, functionPath: str('Material function /Game path.') }, ['materialPath', 'functionPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('get_material_function_info', 'material', 'Retrieve information about a material function.', schema({ functionPath: str('Material function /Game path.') }, ['functionPath']), OK, READ, READ_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_scalar_parameter_value', 'material', 'Set a scalar parameter value on a material instance.', schema({ instancePath: str('Material instance /Game path.'), parameterName: str('Parameter name.'), value: num('Scalar value.') }, ['instancePath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_vector_parameter_value', 'material', 'Set a vector parameter value on a material instance.', schema({ instancePath: str('Material instance /Game path.'), parameterName: str('Parameter name.'), value: { description: 'Vector value.' } }, ['instancePath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('set_texture_parameter_value', 'material', 'Set a texture parameter value on a material instance.', schema({ instancePath: str('Material instance /Game path.'), parameterName: str('Parameter name.'), texturePath: str('Texture /Game path.') }, ['instancePath', 'parameterName', 'texturePath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('add_landscape_layer', 'material', 'Add a landscape layer to a landscape material.', schema({ materialPath: MAT, layerName: str('Layer name.') }, ['materialPath', 'layerName']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' }),
  r('configure_layer_blend', 'material', 'Configure layer blend settings on a landscape material.', schema({ materialPath: MAT, layers: arrObj('Layer blend definitions.'), blendType: str('Blend type.') }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW, { dispatchMode: 'tool' })
];
