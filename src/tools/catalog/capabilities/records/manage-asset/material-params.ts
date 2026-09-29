// Material parameter, property, function, and instance records.
// Path param names mirror the field the native handler reads, which is assetPath for some
// actions and materialPath for others; the split is deliberate, not an inconsistency to tidy.

import type { RecordSpec } from './builder.js';
import { arr, arrObj, bool, ex, LOW, num, READ, READ_POLICY, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const MAT = str('Material /Game asset path.');
const MATFN = str('Material function /Game asset path.');
const MINST = str('Material instance /Game asset path.');
// Every material edit below reads `save` and defaults it to true.
const SAVE = bool('Save the asset afterwards. Defaults to true; pass false to keep the change in memory only.');
// A material info read can narrow its output: which sections, and which nodes' connections.
const INFO_FILTERS = {
  filter: str('Which sections to return: parameters, expressions, connections, or all (default).'),
  nodeId: str('Only report connections touching this node.'),
  nodeIds: arr('Only report connections touching these nodes.'),
};
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

// get_material_info shares none of `OK`'s fields: the handler
// (…/MaterialAuthoring/Queries/…GetMaterialInfo.cpp) emits assetType/nodeCount/
// domain/blendMode/twoSided/parameters/inputs and never sets `details`. Under
// the generic OK schema, output projection therefore discarded the entire
// payload and "Material info retrieved." carried nothing — leaving no way to
// verify anything create_material claims to have set.
// A material that fails to translate renders as the default material; compiled
// used to be true regardless, so the caller never learned the graph was broken.
const COMPILE_OK = schema({
  success: bool('Operation succeeded.'),
  assetPath: str('Compiled asset path.'),
  assetType: str('Material or MaterialFunction.'),
  compiled: bool('False when the material does not compile; compileErrors says why.'),
  compileErrors: arr('Compile errors reported by the material translator, empty when it compiles.'),
  saved: bool('Whether the asset was saved.'),
}, ['success']);

const MATERIAL_INFO_OK = schema({
  success: bool('Operation succeeded.'),
  assetType: str('Asset type: Material, MaterialFunction or MaterialInstance.'),
  parent: str('MaterialInstance: the material or instance it overrides.'),
  baseMaterial: str('MaterialInstance: the material at the root of its parent chain, which owns the node graph.'),
  parameterOverrides: arrObj('MaterialInstance: each overridden parameter (name, type scalar/vector/texture, value).'),
  nodeCount: num('Number of expression nodes in the graph.'),
  domain: str('Material domain, e.g. Surface, PostProcess, UI.'),
  blendMode: str('Blend mode, e.g. Opaque, Masked, Translucent.'),
  twoSided: bool('Whether the material renders two-sided.'),
  description: str('Material function description.'),
  exposeToLibrary: bool('Whether a material function is exposed to the library.'),
  parameters: arrObj('Material parameters (name, type, nodeId).'),
  inputs: arrObj('Material function inputs (name, type, nodeId).'),
}, ['success']);

const M = '/Game/Materials/M_Base';
const MI = '/Game/Materials/MI_Base_Rusty';
const MF = '/Game/Materials/Functions/MF_HeightBlend';
const DONE = { success: true };

export const MATERIAL_PARAMS_RECORDS: readonly RecordSpec[] = [
  r('set_blend_mode', 'material', 'Set the blend mode of a material.', schema({ assetPath: MAT, blendMode: str('Blend mode.'), save: SAVE }, ['assetPath', 'blendMode']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A material must become translucent, cut out (Masked) or additive instead of Opaque.'],
      whenNotToUse: ['The target is a material instance; only a base material can be edited here (find it with material.get_material_info).', 'A new material is being created with the blend mode already known (use material.create_material with blendMode).'],
      examples: [ex('Switch a material to masked blending', { assetPath: M, blendMode: 'Masked' }, DONE)] }),
  r('set_shading_model', 'material', 'Set the shading model of a material.', schema({ assetPath: MAT, shadingModel: str('Shading model.'), save: SAVE }, ['assetPath', 'shadingModel']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A surface needs a different shading model, such as DefaultLit, Unlit, Subsurface, ClearCoat or Hair.'],
      whenNotToUse: ['The target is a material instance; only a base material can be edited here (find it with material.get_material_info).', 'A new material is being created with the shading model already known (use material.create_material with shadingModel).'],
      examples: [ex('Use the default lit shading model', { assetPath: M, shadingModel: 'DefaultLit' }, DONE)] }),
  r('set_material_domain', 'material', 'Set the material domain of a material.', schema({ assetPath: MAT, materialDomain: str('Material domain.'), save: SAVE }, ['assetPath', 'materialDomain']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A material must change domain, for example between Surface, DeferredDecal, PostProcess and UI.'],
      whenNotToUse: ['The target is a material instance; only a base material can be edited here (find it with material.get_material_info).', 'A new material is being created with the domain already known (use material.create_material with materialDomain).'],
      examples: [ex('Keep a material in the surface domain', { assetPath: M, materialDomain: 'Surface' }, DONE)] }),
  r('compile_material', 'material', 'Compile a material and report its compile errors.', schema({ assetPath: MAT, save: SAVE }, ['assetPath']), COMPILE_OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['The compile errors of an edited graph must be read; a material that fails to compile renders as the default material.', 'Graph edits left the asset unsaved, so it must be compiled and saved.'],
      whenNotToUse: ['A property or parameter was just set (material.set_material_property and material.set_material_parameter already recompile).', 'Domain, blend mode or node counts are wanted rather than compile errors (use material.get_material_info).'],
      examples: [ex('Compile after editing the graph', { assetPath: M }, DONE)] }),
  r('get_material_info', 'material', 'Read a material or material function (domain, blend mode, parameters, node count), or a material instance (its parent and parameter overrides).', schema({ assetPath: MAT, ...INFO_FILTERS }, ['assetPath']), MATERIAL_INFO_OK, READ, READ_POLICY, LOW,
    { whenToUse: ['An existing material must be understood before editing: node ids, parameter names, wiring, domain and blend mode, or the parent and overrides of an instance.'],
      whenNotToUse: ['Shading model, sampler counts or the instances of a parent are wanted (use asset.query_asset lookup=material_stats or asset.list kind=material_instances).'],
      examples: [ex('Read a material\'s configuration', { assetPath: M }, { success: true, assetType: 'Material', nodeCount: 4, domain: 'Surface', blendMode: 'Opaque', twoSided: false })] }),
  r('set_two_sided', 'material', 'Set the two-sided flag on a material.', schema({ assetPath: MAT, value: bool('Two-sided value: true renders both faces, false only the front.'), save: SAVE }, ['assetPath', 'value']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['Both faces of a mesh must render, as with leaves, cloth or thin sheets.'],
      whenNotToUse: ['The target is a material instance; only a base material can be edited here (find it with material.get_material_info).'],
      examples: [ex('Render a material from both sides', { assetPath: M, value: true }, DONE)] }),
  r('add_function_input', 'material', 'Add a function input to a material function.', schema({ assetPath: MATFN, inputName: str('Input name.'), inputType: str('Input type.'), x: num('Node X position (preferred spelling; posX is the fallback).'), y: num('Node Y position (preferred spelling; posY is the fallback).') }, ['assetPath', 'inputName']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A material function needs an input pin (scalar, vector, Texture2D, TextureCube, Bool or MaterialAttributes) that callers wire into.'],
      whenNotToUse: ['The asset is a base material; only material functions take input or output pins (a material result goes to Main with material.connect_nodes).', 'The function does not exist yet (use material.create_material with kind=function).'],
      examples: [ex('Add a scalar height input', { assetPath: MF, inputName: 'Height', inputType: 'Scalar', x: -400, y: 0 }, DONE)] }),
  r('add_function_output', 'material', 'Add a function output to a material function.', schema({ assetPath: MATFN, inputName: str('Output name.'), inputType: str('Output type.'), x: num('Node X position (preferred spelling; posX is the fallback).'), y: num('Node Y position (preferred spelling; posY is the fallback).') }, ['assetPath', 'inputName']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A material function needs a named result pin so a graph can wire its result out.'],
      whenNotToUse: ['The asset is a base material; only material functions take input or output pins (a material result goes to Main with material.connect_nodes).', 'The function does not exist yet (use material.create_material with kind=function).'],
      examples: [ex('Add the blended result output', { assetPath: MF, inputName: 'Result', inputType: 'Scalar', x: 400, y: 0 }, DONE)] }),
  r('use_material_function', 'material', 'Insert a material function reference into a material graph.', schema({ materialPath: MAT, functionPath: str('Material function /Game path.'), x: num('Node X position (preferred spelling; posX is the fallback).'), y: num('Node Y position (preferred spelling; posY is the fallback).') }, ['materialPath', 'functionPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A reusable material function must be called from a material or another function as a single node.'],
      whenNotToUse: ['The function does not exist yet (use material.create_material with kind=function).', 'The function needs another input or output pin (use material.add_function_io).'],
      examples: [ex('Reference a blend function from a material', { materialPath: M, functionPath: MF, x: -200, y: 500 }, DONE)] }),
  r('get_material_function_info', 'material', 'Retrieve information about a material function.', schema({ assetPath: MATFN, ...INFO_FILTERS }, ['assetPath']), OK, READ, READ_POLICY, LOW,
    { whenToUse: ['A material function must be understood before calling or editing it: its inputs, outputs, description and library exposure.'],
      whenNotToUse: ['The materials that call the function must be found (use asset.inspect_asset with lookup=dependencies and referencers=true).'],
      examples: [ex('Read a function\'s inputs and outputs', { assetPath: MF }, DONE)] }),
  r('set_scalar_parameter_value', 'material', 'Set a scalar parameter value on a material instance.', schema({ assetPath: MINST, parameterName: str('Parameter name.'), value: num('Scalar value.'), save: SAVE }, ['assetPath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A float parameter such as roughness, opacity or a speed must change on an instance or as a base material default.'],
      whenNotToUse: ['The parameter is not on the base material yet (add it there with asset.edit_material_instance edit=add_parameter or material.add_material_node).'],
      examples: [ex('Override roughness on an instance', { assetPath: MI, parameterName: 'Roughness', value: 0.8 }, DONE)] }),
  r('set_vector_parameter_value', 'material', 'Set a vector parameter value on a material instance.', schema({ assetPath: MINST, parameterName: str('Parameter name.'), value: { description: 'Vector value.' }, save: SAVE }, ['assetPath', 'parameterName', 'value']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A color or vector parameter such as a tint must change; value takes an rgba object or an array of at least three numbers.'],
      whenNotToUse: ['The parameter is not on the base material yet (add it there with asset.edit_material_instance edit=add_parameter or material.add_material_node).'],
      examples: [ex('Tint an instance rust-orange', { assetPath: MI, parameterName: 'BaseTint', value: [0.55, 0.27, 0.1, 1] }, DONE)] }),
  r('set_texture_parameter_value', 'material', 'Set a texture parameter value on a material instance.', schema({ assetPath: MINST, parameterName: str('Parameter name.'), texturePath: str('Texture /Game path.'), save: SAVE }, ['assetPath', 'parameterName', 'texturePath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A texture parameter must point at a different texture asset (texturePath).'],
      whenNotToUse: ['The parameter is not on the base material yet (add it there with asset.edit_material_instance edit=add_parameter or material.add_material_node).', 'The texture asset itself needs other compression or LOD settings (use texture.configure_texture).'],
      examples: [ex('Swap the base colour texture', { assetPath: MI, parameterName: 'BaseColor', texturePath: '/Game/Textures/T_Rust' }, DONE)] }),
  r('add_landscape_layer', 'material', 'Create the landscape layer info asset (the weight layer a landscape material paints) for a layer name, beside the material or in path.', schema({ materialPath: str('Landscape material the layer belongs to; the layer info asset is created in its folder unless path is given.'), layerName: str('Layer name.'), path: str('Folder for the layer info asset, e.g. /Game/Landscape/Layers (overrides the material folder).'), hardness: num('Layer hardness, 0-1 (default 0.5).'), physicalMaterialPath: str('Physical material the layer applies, e.g. /Game/Physics/PM_Grass.'), noWeightBlend: bool('Exclude the layer from weight blending (default false).'), save: SAVE }, ['materialPath', 'layerName']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A landscape weight layer needs its layer info asset, with hardness, physical material and weight-blend option, so it can be painted.'],
      whenNotToUse: ['The material needs a layer blend node that lists the layers (use material.configure_layer_blend).', 'The landscape must be painted with the layer (use build_environment.edit_landscape with edit=paint_landscape_layer).'],
      examples: [ex('Add a grass weight-blended layer', { materialPath: '/Game/Materials/Landscape/M_Landscape', layerName: 'Grass' }, DONE)] }),
  r('configure_layer_blend', 'material', 'Configure layer blend settings on a landscape material.', schema({ materialPath: MAT, layers: arrObj('Layer blend definitions.'), blendType: str('Blend type.'), x: num('Node X position of the layer blend node.'), y: num('Node Y position of the layer blend node.'), save: SAVE }, ['materialPath']), OK, WRITE, WRITE_POLICY, LOW,
    { whenToUse: ['A landscape material needs a Landscape Layer Blend node listing its paint layers, each with a weight, height or alpha blend.', 'All named layers must be added to one blend node in one call (layers takes [{name, blendType}] or plain names).'],
      whenNotToUse: ['Layer info assets for the layers must be created (use material.add_material_node with nodeKind=landscape_layer).', 'A finished material must be assigned to a landscape (use build_environment.sculpt with sculptOp=material).'],
      examples: [ex('Weight-blend the landscape layers', { materialPath: '/Game/Materials/Landscape/M_Landscape', blendType: 'LB_WeightBlend' }, DONE)] })
];
