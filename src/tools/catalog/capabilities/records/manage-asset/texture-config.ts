// Texture configuration and info records: compression, texture group,
// LOD bias, virtual texture streaming, streaming priority, and texture info.

import type { RecordSpec } from './builder.js';
import { bool, ex, LOW, num, READ, READ_POLICY, r, schema, str, WRITE, WRITE_POLICY } from './builder.js';

const OK = schema({ success: bool('Operation succeeded.'), details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Operation details.' } }, ['success']);

const T = '/Game/Textures/T_Rock';
const DONE = { success: true };

export const TEXTURE_CONFIG_RECORDS: readonly RecordSpec[] = [
  r('set_compression_settings', 'texture', 'Set compression settings on a texture.', schema({ assetPath: str('Texture /Game path.'), compressionSettings: str('Compression format.'), save: bool('Save after change.') }, ['assetPath', 'compressionSettings']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Compress a normal map as TC_Normalmap', { assetPath: '/Game/Textures/T_Normal', compressionSettings: 'TC_Normalmap', save: true }, DONE)] }),
  r('set_texture_group', 'texture', 'Set the texture group on a texture.', schema({ assetPath: str('Texture /Game path.'), textureGroup: str('Texture group name.'), save: bool('Save after change.') }, ['assetPath', 'textureGroup']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Assign a texture to the World group', { assetPath: T, textureGroup: 'TEXTUREGROUP_World', save: true }, DONE)] }),
  r('set_lod_bias', 'texture', 'Set the LOD bias on a texture.', schema({ assetPath: str('Texture /Game path.'), lodBias: num('LOD bias value.'), save: bool('Save after change.') }, ['assetPath', 'lodBias']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Drop one mip on a background texture', { assetPath: T, lodBias: 1, save: true }, DONE)] }),
  r('configure_virtual_texture', 'texture', 'Configure virtual texture streaming settings on a texture.', schema({ assetPath: str('Texture /Game path.'), virtualTextureStreaming: bool('Enable VT streaming.'), save: bool('Save after change.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Enable virtual texture streaming', { assetPath: T, virtualTextureStreaming: true, save: true }, DONE)] }),
  r('set_streaming_priority', 'texture', 'Set streaming priority and never-stream on a texture.', schema({ assetPath: str('Texture /Game path.'), streamingPriority: num('Texture streaming priority (default 0).'), neverStream: bool('Never stream this texture.'), save: bool('Save after change.') }, ['assetPath']), OK, WRITE, WRITE_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Prioritise a hero texture and pin it resident', { assetPath: T, streamingPriority: 2, neverStream: true, save: true }, DONE)] }),
  r('get_texture_info', 'texture', 'Retrieve texture information (size, format, etc.).', schema({ assetPath: str('Texture /Game path.') }, ['assetPath']), OK, READ, READ_POLICY, LOW,
    { dispatchMode: 'tool', examples: [ex('Read a texture\'s size and format', { assetPath: T }, DONE)] })
];
