// Texture adjustment records: resize, levels, curves, blur, sharpen,
// invert, desaturate, channel pack/extract, and combine.

import type { RecordSpec } from './builder.js';
import { bool, MEDIUM, num, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

export const TEXTURE_ADJUST_RECORDS: readonly RecordSpec[] = [
  r('resize_texture', 'texture', 'Resize a texture to new dimensions.', schema({ sourcePath: str('Source texture path.'), name: str('Output name.'), path: str('Package path.'), newWidth: num('New width.'), newHeight: num('New height.'), filterMethod: str('Resampling filter.') }, ['sourcePath', 'name']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('adjust_levels', 'texture', 'Adjust levels (black/white points, gamma) on a texture.', schema({ assetPath: str('Texture /Game path.'), inBlack: num('Input black point.'), inWhite: num('Input white point.'), gamma: num('Gamma.'), channel: str('Channel to adjust.'), save: bool('Save after adjustment.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('adjust_curves', 'texture', 'Apply curve-based adjustments to a texture.', schema({ assetPath: str('Texture /Game path.'), channel: str('Channel to adjust.'), curvePoints: { type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: 'Curve control points.' }, save: bool('Save after adjustment.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('blur', 'texture', 'Apply a blur filter to a texture.', schema({ assetPath: str('Texture /Game path.'), radius: num('Blur radius.'), samples: num('Sample count.'), save: bool('Save after blur.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('sharpen', 'texture', 'Apply a sharpen filter to a texture.', schema({ assetPath: str('Texture /Game path.'), strength: num('Sharpen strength.'), save: bool('Save after sharpen.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('invert', 'texture', 'Invert color channels of a texture.', schema({ assetPath: str('Texture /Game path.'), channel: str('Channel to invert.'), save: bool('Save after inversion.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('desaturate', 'texture', 'Desaturate a texture by an amount.', schema({ assetPath: str('Texture /Game path.'), amount: num('Desaturation amount (0-1).'), save: bool('Save after desaturation.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('channel_pack', 'texture', 'Pack individual channels from multiple textures into one.', schema({ redTexture: str('Red channel source.'), greenTexture: str('Green channel source.'), blueTexture: str('Blue channel source.'), outputPath: str('Output texture path.') }, ['redTexture', 'greenTexture', 'blueTexture', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('channel_extract', 'texture', 'Extract a single channel from a texture.', schema({ assetPath: str('Texture /Game path.'), channel: str('Channel to extract.'), outputPath: str('Output texture path.') }, ['assetPath', 'channel', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' }),
  r('combine_textures', 'texture', 'Blend two textures together.', schema({ baseTexture: str('Base texture path.'), blendTexture: str('Blend texture path.'), blendType: str('Blend mode.'), opacity: num('Blend opacity.'), outputPath: str('Output texture path.') }, ['baseTexture', 'blendTexture', 'outputPath']), OK, WRITE, WRITE_POLICY, MEDIUM, { dispatchMode: 'tool' })
];
