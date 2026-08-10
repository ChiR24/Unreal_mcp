// Texture generation records: procedural noise, gradient, pattern,
// normal-from-height, and ambient-occlusion-from-mesh generation.

import type { RecordSpec } from './builder.js';
import { bool, ex, MEDIUM, num, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const TEXTURES = '/Game/Textures';
const DONE = { success: true };

export const TEXTURE_CREATE_RECORDS: readonly RecordSpec[] = [
  r('create_noise_texture', 'texture', 'Generate a procedural noise texture.', schema({ name: str('Texture name.'), path: str('Package path.'), noiseType: str('Noise type.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), seed: num('Random seed.'), octaves: num('Noise octaves.'), scale: num('Noise scale.') }, ['name']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Generate a 1024px Perlin noise texture', { name: 'T_Noise', path: TEXTURES, noiseType: 'Perlin', width: 1024, height: 1024, seed: 1337, octaves: 4, scale: 8 }, DONE)] }),
  r('create_gradient_texture', 'texture', 'Generate a gradient texture.', schema({ name: str('Texture name.'), path: str('Package path.'), gradientType: str('Gradient type.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).') }, ['name']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Generate a linear gradient ramp', { name: 'T_Gradient', path: TEXTURES, gradientType: 'Linear', width: 512, height: 512 }, DONE)] }),
  r('create_pattern_texture', 'texture', 'Generate a pattern texture (grid, brick, etc.).', schema({ name: str('Texture name.'), path: str('Package path.'), patternType: str('Pattern type.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).') }, ['name']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Generate a brick pattern', { name: 'T_Brick', path: TEXTURES, patternType: 'Brick', width: 1024, height: 1024 }, DONE)] }),
  r('create_normal_from_height', 'texture', 'Generate a normal map from a heightmap texture.', schema({ sourceTexture: str('Source heightmap texture path.'), name: str('Output texture name.'), path: str('Package path.'), strength: num('Normal strength.') }, ['sourceTexture', 'name']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Derive a normal map from a heightmap', { sourceTexture: `${TEXTURES}/T_Height`, name: 'T_Normal', path: TEXTURES, strength: 2 }, DONE)] }),
  r('create_ao_from_mesh', 'texture', 'Generate an ambient occlusion texture from a mesh.', schema({ meshPath: str('Mesh /Game path.'), name: str('Output texture name.'), path: str('Package path.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), samples: num('AO samples.') }, ['meshPath', 'name']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { dispatchMode: 'tool', examples: [ex('Bake ambient occlusion for a prop', { meshPath: '/Game/Meshes/SM_Crate', name: 'T_Crate_AO', path: TEXTURES, width: 1024, height: 1024, samples: 64 }, DONE)] })
];
