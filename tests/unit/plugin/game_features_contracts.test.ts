// Game feature plugins (list_game_features, set_game_feature_active, create_plugin template game_feature) reach the
// Game Features subsystem through the EngineWireGameFeatures module. Two traps found live and in packaging: the
// subsystem registers a plugin only when its GameFeatureData package is on disk, and a plugin reference that turned
// GameFeatures on would enable it (and the plugins it needs) in every project that installs EngineWire.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PLUGIN = join('plugins', 'EngineWire');
const read = (...segments: readonly string[]): string => readFileSync(join(PLUGIN, ...segments), 'utf8').replace(/\r\n/gu, '\n');

interface PluginReference { readonly Name: string; readonly Enabled: boolean; readonly Optional?: boolean }
interface ModuleEntry { readonly Name: string; readonly LoadingPhase: string }
interface Descriptor { readonly Plugins: readonly PluginReference[]; readonly Modules: readonly ModuleEntry[] }

describe('game feature plugins', () => {
  const descriptor = JSON.parse(read('EngineWire.uplugin')) as Descriptor;

  it('names GameFeatures without turning it on, and never loads the adapter by itself', () => {
    expect(descriptor.Plugins.find((each) => each.Name === 'GameFeatures')).toEqual({ Name: 'GameFeatures', Enabled: false, Optional: true });
    expect(descriptor.Modules.find((each) => each.Name === 'EngineWireGameFeatures')?.LoadingPhase).toBe('None');
    const startup = read('Source', 'EngineWire', 'Private', 'Core', 'Module', 'EngineWireModule.cpp');
    expect(startup).toContain('ModuleExists(TEXT("EngineWireGameFeatures")) && FModuleManager::Get().ModuleExists(TEXT("GameFeatures"))');
  });

  it('saves the GameFeatureData before the subsystem is handed the plugin', () => {
    const adapter = read('Source', 'EngineWireGameFeatures', 'Private', 'EngineWireGameFeaturesModule.cpp');
    const save = adapter.indexOf('!Save(Data)');
    expect(save).toBeGreaterThan(-1);
    expect(adapter.indexOf('LoadBuiltInGameFeaturePlugin(', save)).toBeGreaterThan(save);
    const create = read('Source', 'EngineWire', 'Private', 'Domains', 'SystemControl', 'Plugins', 'McpPluginCreate.cpp');
    expect(create).toContain('AdoptNewPlugin(Name, [](UObject* Data) { return McpSafeAssetSave(Data); })');
  });

  it('names states from the engine\'s own list, so every version names its own', () => {
    const adapter = read('Source', 'EngineWireGameFeatures', 'Private', 'EngineWireGameFeaturesModule.cpp');
    expect(adapter).toContain('GAME_FEATURE_PLUGIN_STATE_LIST(MCP_GAME_FEATURE_STATE)');
  });

  it('tells a build without the adapter apart from a project without GameFeatures', () => {
    const handlers = read('Source', 'EngineWire', 'Private', 'Domains', 'SystemControl', 'Plugins', 'McpGameFeatures.cpp');
    expect(handlers).toContain('TEXT("GAME_FEATURES_UNAVAILABLE")');
    expect(handlers).toContain('TEXT("GAME_FEATURES_DISABLED")');
    expect(handlers).toContain('TEXT("ENGINE_VERSION_UNSUPPORTED")');
  });
});
