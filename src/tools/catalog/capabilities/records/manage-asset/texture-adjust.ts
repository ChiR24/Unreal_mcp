// Texture adjustment records: resize, levels, curves, blur, sharpen,
// invert, desaturate, channel pack/extract, and combine.

import type { RecordSpec } from './builder.js';
import { bool, ex, MEDIUM, num, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const T = '/Game/Textures/T_Rock';
const TEXTURES = '/Game/Textures';
const DONE = { success: true };

// Source formats the pixel operations accept; anything else fails naming the format found.
const OTHER_FORMATS = 'Other source formats (G16, RGBA16, float, HDR) fail with an error naming the format found.';
const READ_FORMATS = `an 8-bit BGRA8 or single-channel G8 texture (a channel_extract result, read as gray in R, G and B with alpha 255). ${OTHER_FORMATS}`;
const EDIT_PATH = str(`Texture /Game path. Its source must be 8-bit BGRA8 or single-channel G8 (a channel_extract result); a G8 texture is rewritten as BGRA8 with its gray in R, G and B and alpha 255. ${OTHER_FORMATS}`);

export const TEXTURE_ADJUST_RECORDS: readonly RecordSpec[] = [
  r('resize_texture', 'texture', 'Resize a texture to new dimensions.', schema({ sourcePath: str(`Source texture path; its source must be ${READ_FORMATS}`), name: str('Output name (default <source>_Resized).'), path: str('Package path (default the source folder).'), outputPath: str('Full output asset path; replaces name and path.'), newWidth: num('New width.'), newHeight: num('New height.'), filterMethod: str('Resampling filter.') }, ['sourcePath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Halve a texture to 512px', { sourcePath: T, name: 'T_Rock_512', path: TEXTURES, newWidth: 512, newHeight: 512, filterMethod: 'Bilinear' }, DONE)],
      whenToUse: ['A texture must be copied at a smaller or larger size such as 512px; the source texture is left as it is.'],
      whenNotToUse: ['Only runtime mip detail or memory must drop, not the source size (use texture.configure_texture).'] }),
  r('adjust_levels', 'texture', 'Adjust levels (black/white points, gamma) on a texture.', schema({ assetPath: EDIT_PATH, inBlack: num('Input black point.'), inWhite: num('Input white point.'), gamma: num('Gamma.'), channel: str('Channel to adjust.'), save: bool('Save after adjustment.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Lift the black point and regamma', { assetPath: T, inBlack: 0.05, inWhite: 0.95, gamma: 1.1, channel: 'RGB', save: true }, DONE)],
      whenToUse: ['Brightness or contrast must change through black point, white point or gamma, on one channel or on all colour channels.'],
      whenNotToUse: ['The original pixels must be kept, since these edits are made in place (duplicate the texture first with asset.duplicate).'] }),
  r('adjust_curves', 'texture', 'Apply curve-based adjustments to a texture.', schema({ assetPath: EDIT_PATH, channel: str('Channel to adjust.'), curvePoints: { type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: 'Curve control points as {x, y} pairs in 0-1; at least two.' }, save: bool('Save after adjustment.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Apply an S-curve to the red channel', { assetPath: T, channel: 'R', curvePoints: [{ x: 0, y: 0 }, { x: 0.25, y: 0.18 }, { x: 0.75, y: 0.82 }, { x: 1, y: 1 }], save: true }, DONE)],
      whenToUse: ['A tonal curve such as an S-curve for contrast must be applied to one colour channel or to all of them.'],
      whenNotToUse: ['The original pixels must be kept, since these edits are made in place (duplicate the texture first with asset.duplicate).', 'Two textures must be blended, not one edited (use texture.create_texture with kind=combined).'] }),
  r('blur', 'texture', 'Apply a blur filter to a texture.', schema({ assetPath: EDIT_PATH, radius: num('Blur radius.'), save: bool('Save after blur.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Soften a texture', { assetPath: T, radius: 4, save: true }, DONE)],
      whenToUse: ['A texture must be softened; radius is a whole number from 1 to 10.'],
      whenNotToUse: ['The original pixels must be kept, since these edits are made in place (duplicate the texture first with asset.duplicate).', 'A smaller copy is wanted rather than softer pixels (use texture.create_texture with kind=resized).'] }),
  r('sharpen', 'texture', 'Apply a sharpen filter to a texture.', schema({ assetPath: EDIT_PATH, strength: num('Sharpen strength.'), save: bool('Save after sharpen.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Sharpen a downsampled texture', { assetPath: T, strength: 0.6, save: true }, DONE)],
      whenToUse: ['A soft or downsampled texture needs crisper edges; strength runs from 0 to 5.'],
      whenNotToUse: ['The original pixels must be kept, since these edits are made in place (duplicate the texture first with asset.duplicate).', 'The texture only looks soft at a distance (use texture.configure_texture for mip bias and streaming).'] }),
  r('invert', 'texture', 'Invert color channels of a texture.', schema({ assetPath: EDIT_PATH, channel: str('Channel to invert.'), save: bool('Save after inversion.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Invert a green channel for normal-map handedness', { assetPath: '/Game/Textures/T_Normal', channel: 'G', save: true }, DONE)],
      whenToUse: ['Colour channels must be flipped, for example the green channel of a normal map to change its handedness.'],
      whenNotToUse: ['The original pixels must be kept, since these edits are made in place (duplicate the texture first with asset.duplicate).', 'A normal map only needs its compression fixed (use texture.configure_texture).'] }),
  r('desaturate', 'texture', 'Desaturate a texture by an amount.', schema({ assetPath: EDIT_PATH, amount: num('Desaturation amount (0-1).'), save: bool('Save after desaturation.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Half-desaturate a texture', { assetPath: T, amount: 0.5, save: true }, DONE)],
      whenToUse: ['A texture must lose colour, fully or by an amount from 0 to 1.'],
      whenNotToUse: ['The original pixels must be kept, since these edits are made in place (duplicate the texture first with asset.duplicate).', 'A grayscale copy of one channel is wanted as a new texture (use texture.create_texture with kind=channel_extract).'] }),
  r('channel_pack', 'texture', 'Pack individual channels from multiple textures into one.', schema({ redTexture: str('Red channel source: an 8-bit BGRA8 or G8 texture.'), greenTexture: str('Green channel source: an 8-bit BGRA8 or G8 texture.'), blueTexture: str('Blue channel source: an 8-bit BGRA8 or G8 texture.'), alphaTexture: str('Alpha channel source: an 8-bit BGRA8 or G8 texture.'), outputPath: str('Output texture path; or give path and name.'), path: str('Output package path (default /Game/Textures).'), name: str('Output texture name.') }, [], ['redTexture', 'greenTexture', 'blueTexture', 'alphaTexture']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Pack AO, roughness and metallic into one map', { redTexture: `${TEXTURES}/T_Rock_AO`, greenTexture: `${TEXTURES}/T_Rock_Roughness`, blueTexture: `${TEXTURES}/T_Rock_Metallic`, outputPath: `${TEXTURES}/T_Rock_ORM` }, DONE)],
      whenToUse: ['Separate AO, roughness and metallic maps must be packed into the channels of one linear texture.', 'Sources are 8-bit BGRA8 or single-channel G8 textures such as channel_extract results; a BGRA8 source gives its same channel (alpha from its alpha), a G8 source gives its gray to whichever channel it is assigned, and any other source format (G16, RGBA16, float, HDR) fails naming the format found.'],
      whenNotToUse: ['Separate textures can simply be sampled in a material (use material.add_material_node with nodeKind=texture_sample).'] }),
  r('channel_extract', 'texture', 'Extract a single channel from a texture.', schema({ assetPath: str(`Texture /Game path; its source must be ${READ_FORMATS}`), channel: str('Channel to extract.'), outputPath: str('Output texture path; or give path and name.'), path: str('Output package path (default the source folder).'), name: str('Output texture name (default <source>_<channel>).') }, ['assetPath', 'channel']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Pull roughness out of a packed map', { assetPath: `${TEXTURES}/T_Rock_ORM`, channel: 'G', outputPath: `${TEXTURES}/T_Rock_Roughness` }, DONE)],
      whenToUse: ['One channel of a packed texture must become its own grayscale texture.'],
      whenNotToUse: ['A material only needs one channel of a texture (use material.add_material_node with a texture sample).'] }),
  r('combine_textures', 'texture', 'Blend two textures together.', schema({ baseTexture: str(`Base texture path; its source must be ${READ_FORMATS}`), blendTexture: str(`Blend texture path; it must have the width and height of baseTexture. Its source can be BGRA8 or G8 whichever baseTexture is. ${OTHER_FORMATS}`), blendType: str('Blend mode.'), opacity: num('Blend opacity.'), outputPath: str('Output texture path; or give path and name.'), path: str('Output package path (default /Game/Textures).'), name: str('Output texture name.') }, ['baseTexture', 'blendTexture']), OK, WRITE, WRITE_POLICY, MEDIUM,
    { examples: [ex('Overlay grime onto a base texture', { baseTexture: T, blendTexture: `${TEXTURES}/T_Grime`, blendType: 'Overlay', opacity: 0.4, outputPath: `${TEXTURES}/T_Rock_Weathered` }, DONE)],
      whenToUse: ['Two same-size textures must be blended into a new texture with Multiply, Screen, Overlay or Add at a set opacity.'],
      whenNotToUse: ['Two textures should blend per pixel at render time (use material.add_material_node).'] })
];
