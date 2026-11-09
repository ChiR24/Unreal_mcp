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

// SmartObjects, required above, requires these plugins itself. The editor's plugin walk records a plugin name the first
// time any reference names it, before it drops an optional reference the engine's own build lacks, so an optional
// reference from EngineWire hides the name from SmartObjects' required one. UE 5.5's build lacks PropertyBindingUtils:
// in a Blueprint-only project it stayed off and the editor stopped with "Plugin 'SmartObjects' failed to load because
// module 'SmartObjectsModule' could not be loaded". SmartObjects turns each of them on where it needs them.
describe('plugins SmartObjects requires', () => {
  const descriptor = JSON.parse(readFileSync(join('plugins', 'EngineWire', 'EngineWire.uplugin'), 'utf8')) as { readonly Plugins: readonly PluginReference[] };

  it.each(['GameplayAbilities', 'TargetingSystem', 'WorldConditions', 'PropertyAccessEditor', 'PropertyBindingUtils'])('never references %s as optional', (name) => {
    expect(descriptor.Plugins.find((each) => each.Name === name)?.Optional).toBeUndefined();
  });
});
