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
    { topics: ['new material', 'make material', 'material asset', 'shader'],
      examples: [ex('Create an opaque lit surface material', { name: 'M_Base', path: '/Game/Materials', materialDomain: 'Surface', blendMode: 'Opaque', shadingModel: 'DefaultLit', twoSided: false, save: true }, { success: true })] }
  ),
  r('create_material_instance', 'material', 'Create a material instance from a parent material, optionally with its parameter values already set.',
    schema({ name: str('Instance name.'), parentMaterial: str('Parent material /Game path.'), savePath: str('Package path for the instance.'), parameters: MATERIAL_PARAMETER_LIST, save: SAVE }, ['name', 'parentMaterial']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { topics: ['material instance', 'mi', 'instance material', 'child material'], dispatchAction: 'create_material_instance',
      examples: [ex('Instance a base material', { name: 'MI_Base_Rusty', parentMaterial: '/Game/Materials/M_Base', savePath: '/Game/Materials' }, { success: true })] }
  ),
  r('create_material_function', 'material', 'Create a new material function asset.',
    schema({ name: str('Function name.'), path: str('Package path.'), save: SAVE, description: str('Function description.'), exposeToLibrary: bool('Expose in the material function library.') }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Create a reusable blend function', { name: 'MF_HeightBlend', path: '/Game/Materials/Functions', save: true }, { success: true })] }
  ),
  r('create_landscape_material', 'material', 'Create a landscape material asset (Surface domain, Opaque by default).',
    schema({ name: str('Material name.'), path: str('Package path.'), ...PRESET_SETTINGS }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Create a terrain material', { name: 'M_Landscape', path: '/Game/Materials/Landscape' }, { success: true })] }
  ),
  r('create_decal_material', 'material', 'Create a decal material asset (DeferredDecal domain, Translucent by default).',
    schema({ name: str('Material name.'), path: str('Package path.'), ...PRESET_SETTINGS }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Create a decal material', { name: 'M_Decal_Scorch', path: '/Game/Materials/Decals' }, { success: true })] }
  ),
  r('create_post_process_material', 'material', 'Create a post-process material asset (PostProcess domain by default).',
    schema({ name: str('Material name.'), path: str('Package path.'), ...PRESET_SETTINGS }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Create a vignette post-process material', { name: 'M_PP_Vignette', path: '/Game/Materials/PostProcess' }, { success: true })] }
  )
];
