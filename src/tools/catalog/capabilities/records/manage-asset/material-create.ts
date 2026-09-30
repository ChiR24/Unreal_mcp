// Material creation records: base material, instances, functions, and
// specialized material types (landscape, decal, post-process).

import type { RecordSpec } from './builder.js';
import { bool, ex, MATERIAL_PARAMETER_LIST, MEDIUM, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const SAVE = bool('Save the asset afterwards. Defaults to true; pass false to keep it in memory only.');
// The decal, landscape and post-process creators are create_material with a
// domain and blend preset, so they take the same material settings; a value
// given here overrides the preset.
const PRESET_SETTINGS = {
  materialDomain: str('Material domain; overrides the preset domain.'),
  blendMode: str('Blend mode; overrides the preset blend mode.'),
  shadingModel: str('Shading model.'),
  twoSided: bool('Two-sided flag.'),
  save: SAVE,
};
const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const MATERIAL_CREATE_RECORDS: readonly RecordSpec[] = [
  r('create_material', 'material', 'Create a new material asset.',
    schema({ name: str('Material name.'), path: str('Package path.'), materialDomain: str('Material domain.'), blendMode: str('Blend mode.'), shadingModel: str('Shading model.'), twoSided: bool('Two-sided flag.'), save: SAVE }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { whenToUse: ['A mesh, surface or effect needs a shader of its own and no material asset exists for it yet.', 'A translucent, unlit or two-sided material is wanted from the start (materialDomain, blendMode, shadingModel, twoSided).'],
      whenNotToUse: ['A variation of an existing material with different parameter values is wanted (use material.create_material_instance).', 'An existing material must be copied as it is (use asset.duplicate).'],
      topics: ['new material', 'make material', 'material asset', 'shader'],
      examples: [ex('Create an opaque lit surface material', { name: 'M_Base', path: '/Game/Materials', materialDomain: 'Surface', blendMode: 'Opaque', shadingModel: 'DefaultLit', twoSided: false, save: true }, { success: true })] }
  ),
  r('create_material_instance', 'material', 'Create a material instance from a parent material, optionally with its parameter values already set.',
    schema({ name: str('Instance name.'), parentMaterial: str('Parent material /Game path.'), savePath: str('Package path for the instance.'), parameters: MATERIAL_PARAMETER_LIST, save: SAVE }, ['name', 'parentMaterial']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { whenToUse: ['A variation of a base material is needed with new parameter values and no second node graph.', 'An instance should start with scalar, vector or texture overrides already applied (pass parameters).'],
      whenNotToUse: ['The material needs its own node graph (use material.create_material).', 'An existing instance needs new values (use material.set_material_parameter); the parent must be a base material, not another instance.'],
      topics: ['material instance', 'mi', 'instance material', 'child material', 'make material instance'], dispatchAction: 'create_material_instance',
      examples: [ex('Instance a base material', { name: 'MI_Base_Rusty', parentMaterial: '/Game/Materials/M_Base', savePath: '/Game/Materials' }, { success: true })] }
  ),
  r('create_material_function', 'material', 'Create a new material function asset.',
    schema({ name: str('Function name.'), path: str('Package path.'), save: SAVE, description: str('Function description.'), exposeToLibrary: bool('Expose in the material function library.') }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { whenToUse: ['A chunk of graph logic must be reused by several materials as one node, listed in the function library unless exposeToLibrary is false.'],
      whenNotToUse: ['The logic is used by one material only (build it in that material with material.add_material_node).'],
      examples: [ex('Create a reusable blend function', { name: 'MF_HeightBlend', path: '/Game/Materials/Functions', save: true }, { success: true })] }
  ),
  r('create_landscape_material', 'material', 'Create a landscape material asset (Surface domain, Opaque by default).',
    schema({ name: str('Material name.'), path: str('Package path.'), ...PRESET_SETTINGS }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { whenToUse: ['A material is being made for a landscape; it starts empty (Surface domain, Opaque) and its layers are added afterwards.'],
      whenNotToUse: ['The material exists and only needs assigning to a landscape (use build_environment.sculpt with sculptOp=material).'],
      examples: [ex('Create a terrain material', { name: 'M_Landscape', path: '/Game/Materials/Landscape' }, { success: true })] }
  ),
  r('create_decal_material', 'material', 'Create a decal material asset (DeferredDecal domain, Translucent by default).',
    schema({ name: str('Material name.'), path: str('Package path.'), ...PRESET_SETTINGS }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { whenToUse: ['A material is needed to project onto surfaces as a deferred decal, such as scorch marks or stains.'],
      whenNotToUse: ['A decal material already exists and only its blend mode must change (use material.set_material_property).'],
      examples: [ex('Create a decal material', { name: 'M_Decal_Scorch', path: '/Game/Materials/Decals' }, { success: true })] }
  ),
  r('create_post_process_material', 'material', 'Create a post-process material asset (PostProcess domain by default).',
    schema({ name: str('Material name.'), path: str('Package path.'), ...PRESET_SETTINGS }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { whenToUse: ['A full-screen effect such as a vignette or color grade needs a post-process domain material.'],
      whenNotToUse: ['A built-in post-process setting such as bloom or exposure is enough (use build_environment.configure_post_process).'],
      examples: [ex('Create a vignette post-process material', { name: 'M_PP_Vignette', path: '/Game/Materials/PostProcess' }, { success: true })] }
  )
];
