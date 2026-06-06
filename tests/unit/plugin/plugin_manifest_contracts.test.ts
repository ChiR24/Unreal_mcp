import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';
import { z } from 'zod';

const pluginDependencySchema = z.object({
  Name: z.string(),
  Enabled: z.boolean(),
  Optional: z.boolean().optional(),
});

const pluginManifestSchema = z.object({
  Plugins: z.array(pluginDependencySchema),
});

const readPluginManifest = () =>
  pluginManifestSchema.parse(
    JSON.parse(
      readFileSync(
        resolve(
          process.cwd(),
          'plugins/McpAutomationBridge/McpAutomationBridge.uplugin',
        ),
        'utf8',
      ),
    ),
  );

describe('plugin manifest contracts', () => {
  it('enables PCG during early startup when PCG registers shader types', () => {
    const manifest = readPluginManifest();
    const pcgDependency = manifest.Plugins.find(
      (dependency) => dependency.Name === 'PCG',
    );

    expect(pcgDependency).toBeDefined();
    expect(pcgDependency?.Enabled).toBe(true);
    expect(pcgDependency?.Optional).not.toBe(true);
  });

  it('enables GameplayAbilities when the bridge exposes GAS actions', () => {
    const manifest = readPluginManifest();
    const gameplayAbilitiesDependency = manifest.Plugins.find(
      (dependency) => dependency.Name === 'GameplayAbilities',
    );

    expect(gameplayAbilitiesDependency).toBeDefined();
    expect(gameplayAbilitiesDependency?.Enabled).toBe(true);
    expect(gameplayAbilitiesDependency?.Optional).not.toBe(true);
  });

  it('enables SmartObjects when the bridge exposes Smart Object actions', () => {
    const manifest = readPluginManifest();
    const smartObjectsDependency = manifest.Plugins.find(
      (dependency) => dependency.Name === 'SmartObjects',
    );

    expect(smartObjectsDependency).toBeDefined();
    expect(smartObjectsDependency?.Enabled).toBe(true);
    expect(smartObjectsDependency?.Optional).not.toBe(true);
  });
});
