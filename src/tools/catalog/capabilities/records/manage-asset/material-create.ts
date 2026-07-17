// Material creation records: base material, instances, functions, and
// specialized material types (landscape, decal, post-process).

import type { RecordSpec } from './builder.js';
import { bool, MEDIUM, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const MATERIAL_CREATE_RECORDS: readonly RecordSpec[] = [
  r('create_material', 'material', 'Create a new material asset.',
    schema({ name: str('Material name.'), path: str('Package path.'), materialDomain: str('Material domain.'), blendMode: str('Blend mode.'), shadingModel: str('Shading model.'), twoSided: bool('Two-sided flag.'), save: bool('Save after creation.') }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }
  ),
  r('create_material_instance', 'material', 'Create a material instance from a parent material.',
    schema({ name: str('Instance name.'), parentMaterial: str('Parent material /Game path.'), savePath: str('Package path for the instance.') }, ['name', 'parentMaterial']),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchAction: 'create_material_instance', dispatchMode: 'action' }
  ),
  r('create_material_function', 'material', 'Create a new material function asset.',
    schema({ name: str('Function name.'), path: str('Package path.') }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }
  ),
  r('create_landscape_material', 'material', 'Create a landscape-specific material asset.',
    schema({ name: str('Material name.'), path: str('Package path.') }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }
  ),
  r('create_decal_material', 'material', 'Create a decal material asset.',
    schema({ name: str('Material name.'), path: str('Package path.') }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }
  ),
  r('create_post_process_material', 'material', 'Create a post-process material asset.',
    schema({ name: str('Material name.'), path: str('Package path.') }, ['name']),
    OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }
  )
];
