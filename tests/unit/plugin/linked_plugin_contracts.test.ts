// EngineWire links these plugins' modules without delay-loading them, and a Mac build delay-loads nothing, so the
// editor loads those modules wherever EngineWire loads. An optional plugin reference turns a plugin on only where the
// engine's own build lists it, and an installed UE 5.7 or 5.8 lists neither Movie Render Queue nor the OpenColorIO
// plugin it needs. In a Blueprint-only project the editor then loaded those modules with their plugins off: Windows
// could not load EngineWire, and on a Mac OpenColorIO registered its shaders too late and stopped the editor. A
// required reference turns each plugin on with EngineWire; all four ship with every UE 5.0-5.8.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

interface PluginReference { readonly Name: string; readonly Enabled: boolean; readonly Optional?: boolean }

describe('plugins whose modules EngineWire links', () => {
  const descriptor = JSON.parse(readFileSync(join('plugins', 'EngineWire', 'EngineWire.uplugin'), 'utf8')) as { readonly Plugins: readonly PluginReference[] };

  it.each(['MovieRenderPipeline', 'MoviePipelineMaskRenderPass', 'Takes', 'Metasound'])('requires %s', (name) => {
    expect(descriptor.Plugins.find((each) => each.Name === name)).toEqual({ Name: name, Enabled: true });
  });
});
