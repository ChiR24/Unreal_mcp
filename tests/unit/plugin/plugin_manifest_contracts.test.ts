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
  FileVersion: z.number(),
  Version: z.number(),
  VersionName: z.string(),
  EngineVersion: z.string().optional(),
  Installed: z.boolean().optional(),
  CanContainContent: z.boolean(),
  Plugins: z.array(pluginDependencySchema),
});

const readPluginManifest = (): z.infer<typeof pluginManifestSchema> =>
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
  it('keeps the source descriptor portable across supported engine versions', () => {
    // Given
    const manifest = readPluginManifest();

    // When
    const sourceDistributionFields = {
      EngineVersion: manifest.EngineVersion,
      Installed: manifest.Installed,
    };

    // Then
    expect(sourceDistributionFields).toEqual({
      EngineVersion: undefined,
      Installed: undefined,
    });
  });

  it('preserves the plugin release version', () => {
    // Given
    const manifest = readPluginManifest();

    // When
    const version = {
      FileVersion: manifest.FileVersion,
      Version: manifest.Version,
      VersionName: manifest.VersionName,
    };

    // Then
    expect(version).toEqual({
      FileVersion: 3,
      Version: 530,
      VersionName: '0.5.30',
    });
  });

  it('keeps required engine integrations enabled and nonoptional', () => {
    // Given
    const manifest = readPluginManifest();
    const requiredDependencies = [
      'PythonScriptPlugin',
      'EditorScriptingUtilities',
      'Niagara',
      'GameplayAbilities',
      'SmartObjects',
    ] as const;

    // When
    const dependencyContracts = requiredDependencies.map((name) =>
      manifest.Plugins.find((dependency) => dependency.Name === name),
    );

    // Then
    for (const [index, dependency] of dependencyContracts.entries()) {
      expect(dependency).toEqual({
        Name: requiredDependencies[index],
        Enabled: true,
      });
    }
  });

  it('keeps supported optional integrations enabled when available', () => {
    // Given
    const manifest = readPluginManifest();
    const optionalDependencies = [
      'LevelSequenceEditor',
      'NiagaraEditor',
      'BehaviorTreeEditor',
      'EnvironmentQueryEditor',
      'ControlRig',
      'RigVM',
      'IKRig',
      'ChaosVehiclesPlugin',
      'AnimationData',
      'ProceduralMeshComponent',
      'Interchange',
      'InterchangeOpenUSD',
      'DataValidation',
      'EnhancedInput',
      'GeometryScripting',
      'GeometryProcessing',
      'ChaosCloth',
      'StructUtils',
      'Metasound',
      'StateTree',
      'MassGameplay',
      'OnlineSubsystem',
      'OnlineSubsystemUtils',
      'Synthesis',
      'PCG',
      'ElectraPlayer',
    ] as const;

    // When
    const dependencyContracts = optionalDependencies.map((name) =>
      manifest.Plugins.find((dependency) => dependency.Name === name),
    );

    // Then
    for (const [index, dependency] of dependencyContracts.entries()) {
      expect(dependency).toEqual({
        Name: optionalDependencies[index],
        Enabled: true,
        Optional: true,
      });
    }
  });

  it('retains a code-only descriptor for the packaging placeholder', () => {
    // Given
    const manifest = readPluginManifest();

    // When
    const canContainContent = manifest.CanContainContent;

    // Then
    expect(canContainContent).toBe(false);
  });
});
