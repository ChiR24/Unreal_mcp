// Texture generation records: procedural noise, gradient, pattern,
// normal-from-height, and ambient-occlusion-from-mesh generation.

import type { RecordSpec } from './builder.js';
import { bool, ex, HIGH, MEDIUM, num, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const TEXTURES = '/Game/Textures';
const OUTPUT_PATH = str('Full output asset path, e.g. /Game/Textures/T_Noise; replaces name and path.');
const COLOR = (what: string): { description: string } => ({ description: `${what} as {r, g, b, a} or [r, g, b, a] in 0-1.` });
const DONE = { success: true };

export const TEXTURE_CREATE_RECORDS: readonly RecordSpec[] = [
  r('create_noise_texture', 'texture', 'Generate a procedural noise texture.', schema({ name: str('Texture name.'), path: str('Package path.'), outputPath: OUTPUT_PATH, noiseType: str('Perlin (default), FBM (same as Perlin), Ridged or Billow.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), seed: num('Random seed.'), octaves: num('Noise octaves, 1-16 (default 4).'), scale: num('Noise scale (default 1).'), persistence: num('Amplitude kept per octave (default 0.5).'), lacunarity: num('Frequency gain per octave (default 2).'), seamless: bool('Tile seamlessly (default false).') }, [], ['name', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Generate a 1024px Perlin noise texture', { name: 'T_Noise', path: TEXTURES, noiseType: 'Perlin', width: 1024, height: 1024, seed: 1337, octaves: 4, scale: 8 }, DONE)],
      whenToUse: ['A procedural Perlin, ridged or billow noise texture is needed as a mask, cloud or roughness variation; it can tile.'],
      whenNotToUse: ['The noise can be computed in the material graph instead of baked (use material.add_material_node with nodeKind=noise).'] }),
  r('create_gradient_texture', 'texture', 'Generate a gradient texture.', schema({ name: str('Texture name.'), path: str('Package path.'), outputPath: OUTPUT_PATH, gradientType: str('Linear (default), Radial or Angular.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), angle: num('Linear gradient direction in degrees (default 0).'), centerX: num('Radial or angular centre X, 0-1 (default 0.5).'), centerY: num('Radial or angular centre Y, 0-1 (default 0.5).'), radius: num('Radial gradient radius, 0-1 (default 0.5).'), startColor: COLOR('Start colour (default black)'), endColor: COLOR('End colour (default white)') }, [], ['name', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Generate a linear gradient ramp', { name: 'T_Gradient', path: TEXTURES, gradientType: 'Linear', width: 512, height: 512 }, DONE)],
      whenToUse: ['A linear, radial or angular gradient ramp texture is needed, for example a mask or falloff.'],
      whenNotToUse: ['A gradient is only needed inside one material and can come from its UVs and math nodes (use material.add_material_node).'] }),
  r('create_pattern_texture', 'texture', 'Generate a pattern texture (checker, grid, brick, stripes, dots).', schema({ name: str('Texture name.'), path: str('Package path.'), outputPath: OUTPUT_PATH, patternType: str('Checker (default), Grid, Brick, Stripes or Dots.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), tilesX: num('Pattern repeats across, 1-1024 (default 8).'), tilesY: num('Pattern repeats down, 1-1024 (default 8).'), lineWidth: num('Grid line width as a fraction of a tile (default 0.02).'), brickRatio: num('Brick width to height ratio (default 2).'), offset: num('Brick row offset, 0-1 (default 0.5).'), primaryColor: COLOR('Main colour (default white)'), secondaryColor: COLOR('Second colour (default black)') }, [], ['name', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Generate a brick pattern', { name: 'T_Brick', path: TEXTURES, patternType: 'Brick', width: 1024, height: 1024 }, DONE)],
      whenToUse: ['A checker, grid, brick, stripe or dot pattern texture is needed, for example a debug or placeholder surface.'],
      whenNotToUse: ['A photo or authored image file must be brought into the project (use asset.import).'] }),
  r('create_normal_from_height', 'texture', 'Generate a normal map from a heightmap texture.', schema({ sourceTexture: str('Source heightmap texture path.'), name: str('Output texture name (default <source>_N).'), path: str('Package path (default the source folder).'), outputPath: OUTPUT_PATH, strength: num('Normal strength.') }, ['sourceTexture']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Derive a normal map from a heightmap', { sourceTexture: `${TEXTURES}/T_Height`, name: 'T_Normal', path: TEXTURES, strength: 2 }, DONE)],
      whenToUse: ['A normal map must be derived from a heightmap texture; strength scales the bump depth.'],
      whenNotToUse: ['An existing normal map only needs a channel flipped (use texture.adjust_texture with adjust=invert).'] }),
  r('create_ao_from_mesh', 'texture', 'Bake a real ray-traced ambient occlusion texture of a static mesh in its own UV space (grayscale, linear): crevices come out dark, open surfaces white.', schema({ meshPath: str('Static mesh to bake, e.g. /Game/Meshes/SM_Rock.'), name: str('Output texture name.'), path: str('Package path (default /Game/Textures).'), outputPath: OUTPUT_PATH, width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), samples: num('Occlusion rays per texel, 1-1024 (default 64); more is smoother and slower.'), rayDistance: num('Farthest occluder counted, in cm; 0 (default) means unlimited, negative is refused.'), uvChannel: num('UV channel to bake into (default 0, the one materials sample); a lightmap channel avoids overlapping UVs.') }, ['meshPath'], ['name', 'outputPath']),
    schema({ success: bool('Operation succeeded.'), assetPath: str('The baked texture.'), width: num('Texture width.'), height: num('Texture height.'), minValue: num('Darkest texel, 0-1.'), meanValue: num('Average texel over the whole texture, 0-1 (unused texels count as 1).'), maxValue: num('Brightest texel, 0-1.'), convexMesh: bool('True when the bake is flat because the mesh is convex and cannot occlude itself.'), saved: bool('Whether the texture was saved.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']),
    { ...WRITE, longRunning: true }, WRITE_POLICY, HIGH,
    { examples: [ex('Bake ambient occlusion of a mesh', { meshPath: '/Engine/EngineMeshes/SM_MatPreviewMesh_01', name: 'T_AO_Preview', path: TEXTURES, width: 256, height: 256, samples: 32 }, DONE)],
      whenToUse: ['A static mesh needs a ray-traced ambient occlusion map in its UV space; a convex mesh comes out flat white.'],
      whenNotToUse: ['Scene lighting must be built, not a per-mesh texture (use build_environment.build_lighting).'] }),
];
