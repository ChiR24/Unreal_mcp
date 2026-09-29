// Texture configuration and info records: compression, texture group,
// LOD bias, virtual texture streaming, streaming priority, and texture info.

import type { RecordSpec } from './builder.js';
import { bool, ex, LOW, num, READ, READ_POLICY, r, str, WRITE, WRITE_POLICY } from './builder.js';
import { schema } from '../shared/record-presets.js';

const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const T = '/Game/Textures/T_Rock';
const DONE = { success: true };

export const TEXTURE_CONFIG_RECORDS: readonly RecordSpec[] = [
  r('set_compression_settings', 'texture', 'Set compression settings on a texture.', schema({ assetPath: str('Texture /Game path.'), compressionSettings: str('Compression format.'), save: bool('Save after change.') }, ['assetPath', 'compressionSettings']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Compress a normal map as TC_Normalmap', { assetPath: '/Game/Textures/T_Normal', compressionSettings: 'TC_Normalmap', save: true }, DONE)],
      whenToUse: ['A texture needs the right compression such as TC_Normalmap or TC_Masks; an unknown name falls back to TC_Default.'],
      whenNotToUse: ['The pixels or size of the texture must change, not its asset settings (use texture.adjust_texture or texture.create_texture).'] }),
  r('set_texture_group', 'texture', 'Set the texture group on a texture.', schema({ assetPath: str('Texture /Game path.'), textureGroup: str('Texture group name.'), save: bool('Save after change.') }, ['assetPath', 'textureGroup']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Assign a texture to the World group', { assetPath: T, textureGroup: 'TEXTUREGROUP_World', save: true }, DONE)],
      whenToUse: ['A texture must move to another group, such as UI, Character or Effects, so it follows that group\'s LOD rules; an unrecognised name falls back to World.'],
      whenNotToUse: ['The pixels or size of the texture must change, not its asset settings (use texture.adjust_texture or texture.create_texture).'] }),
  r('set_lod_bias', 'texture', 'Set the LOD bias on a texture.', schema({ assetPath: str('Texture /Game path.'), lodBias: num('LOD bias value.'), save: bool('Save after change.') }, ['assetPath', 'lodBias']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Drop one mip on a background texture', { assetPath: T, lodBias: 1, save: true }, DONE)],
      whenToUse: ['A background or distant texture uses too much memory and can lose its largest mip levels; a lodBias of 1 drops one level.'],
      whenNotToUse: ['The pixels or size of the texture must change, not its asset settings (use texture.adjust_texture or texture.create_texture).', 'A smaller copy of the source image is wanted, not a runtime mip drop (use texture.create_texture with kind=resized).'] }),
  r('configure_virtual_texture', 'texture', 'Configure virtual texture streaming settings on a texture.', schema({ assetPath: str('Texture /Game path.'), virtualTextureStreaming: bool('Enable VT streaming.'), save: bool('Save after change.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Enable virtual texture streaming', { assetPath: T, virtualTextureStreaming: true, save: true }, DONE)],
      whenToUse: ['Virtual texture streaming must be switched on or off for a large texture; leaving virtualTextureStreaming out switches it off.'],
      whenNotToUse: ['The pixels or size of the texture must change, not its asset settings (use texture.adjust_texture or texture.create_texture).'] }),
  r('set_streaming_priority', 'texture', 'Set never-stream on a texture (keep every mip resident). UTexture has no per-texture streaming priority to set.', schema({ assetPath: str('Texture /Game path.'), neverStream: bool('Never stream this texture.'), save: bool('Save after change.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { examples: [ex('Prioritise a hero texture and pin it resident', { assetPath: T, neverStream: true, save: true }, DONE)],
      whenToUse: ['A texture must stay sharp because every mip is kept resident instead of streaming in late; there is no per-texture streaming priority to set.'],
      whenNotToUse: ['The pixels or size of the texture must change, not its asset settings (use texture.adjust_texture or texture.create_texture).', 'The global texture streaming budget must change, not one texture (use system_control.configure_performance with setting=texture_streaming).'] }),
  r('get_texture_info', 'texture', 'Read a 2D texture asset: width and height, pixel format, mip count, sRGB, compression setting, LOD bias, and virtual texture and never-stream flags.', schema({ assetPath: str('Texture /Game path.') }, ['assetPath']), OK, READ, READ_POLICY, LOW,
    { topics: ['size and resolution', 'pixel format', 'compression setting', 'mip count', 'how big is a texture'], examples: [ex('Read a texture\'s size and format', { assetPath: T }, DONE)],
      whenToUse: ['A texture\'s size, pixel format, mip count, sRGB flag and compression must be read before its pixels or settings are changed.','A texture setting must be verified after it was changed.'],
      whenNotToUse: ['A texture setting must be changed, not read (use texture.configure_texture).', 'The texture group or another property not listed here must be read (use inspect.get_property).'] })
];
