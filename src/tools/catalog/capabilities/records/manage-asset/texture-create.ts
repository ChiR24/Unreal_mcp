// Texture generation records: procedural noise, gradient, pattern,
// normal-from-height, and ambient-occlusion-from-mesh generation.

import type { RecordSpec } from './builder.js';
import { bool, ex, MEDIUM, num, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const TEXTURES = '/Game/Textures';
const OUTPUT_PATH = str('Full output asset path, e.g. /Game/Textures/T_Noise; replaces name and path.');
const COLOR = (what: string): { description: string } => ({ description: `${what} as {r, g, b, a} or [r, g, b, a] in 0-1.` });
const DONE = { success: true };

export const TEXTURE_CREATE_RECORDS: readonly RecordSpec[] = [
  r('create_noise_texture', 'texture', 'Generate a procedural noise texture.', schema({ name: str('Texture name.'), path: str('Package path.'), outputPath: OUTPUT_PATH, noiseType: str('Perlin (default), FBM (same as Perlin), Ridged or Billow.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), seed: num('Random seed.'), octaves: num('Noise octaves, 1-16 (default 4).'), scale: num('Noise scale (default 1).'), persistence: num('Amplitude kept per octave (default 0.5).'), lacunarity: num('Frequency gain per octave (default 2).'), seamless: bool('Tile seamlessly (default false).') }, [], ['name', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Generate a 1024px Perlin noise texture', { name: 'T_Noise', path: TEXTURES, noiseType: 'Perlin', width: 1024, height: 1024, seed: 1337, octaves: 4, scale: 8 }, DONE)] }),
  r('create_gradient_texture', 'texture', 'Generate a gradient texture.', schema({ name: str('Texture name.'), path: str('Package path.'), outputPath: OUTPUT_PATH, gradientType: str('Linear (default), Radial or Angular.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), angle: num('Linear gradient direction in degrees (default 0).'), centerX: num('Radial or angular centre X, 0-1 (default 0.5).'), centerY: num('Radial or angular centre Y, 0-1 (default 0.5).'), radius: num('Radial gradient radius, 0-1 (default 0.5).'), startColor: COLOR('Start colour (default black)'), endColor: COLOR('End colour (default white)') }, [], ['name', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Generate a linear gradient ramp', { name: 'T_Gradient', path: TEXTURES, gradientType: 'Linear', width: 512, height: 512 }, DONE)] }),
  r('create_pattern_texture', 'texture', 'Generate a pattern texture (checker, grid, brick, stripes, dots).', schema({ name: str('Texture name.'), path: str('Package path.'), outputPath: OUTPUT_PATH, patternType: str('Checker (default), Grid, Brick, Stripes or Dots.'), width: num('Texture width in pixels (default 1024).'), height: num('Texture height in pixels (default 1024).'), tilesX: num('Pattern repeats across, 1-1024 (default 8).'), tilesY: num('Pattern repeats down, 1-1024 (default 8).'), lineWidth: num('Grid line width as a fraction of a tile (default 0.02).'), brickRatio: num('Brick width to height ratio (default 2).'), offset: num('Brick row offset, 0-1 (default 0.5).'), primaryColor: COLOR('Main colour (default white)'), secondaryColor: COLOR('Second colour (default black)') }, [], ['name', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Generate a brick pattern', { name: 'T_Brick', path: TEXTURES, patternType: 'Brick', width: 1024, height: 1024 }, DONE)] }),
  r('create_normal_from_height', 'texture', 'Generate a normal map from a heightmap texture.', schema({ sourceTexture: str('Source heightmap texture path.'), name: str('Output texture name (default <source>_N).'), path: str('Package path (default the source folder).'), outputPath: OUTPUT_PATH, strength: num('Normal strength.') }, ['sourceTexture']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Derive a normal map from a heightmap', { sourceTexture: `${TEXTURES}/T_Height`, name: 'T_Normal', path: TEXTURES, strength: 2 }, DONE)] }),
];
