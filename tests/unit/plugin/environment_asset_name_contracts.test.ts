import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const privateSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts,
    ),
    'utf8',
  );

const handlerSource = privateSource(
  'Domains',
  'Environment',
  'Runtime',
  'McpAutomationBridge_EnvironmentHandlersFoliageAssets.cpp',
);
const validationSource = privateSource(
  'Domains',
  'Environment',
  'Runtime',
  'McpAutomationBridge_EnvironmentAssetValidation.cpp',
);

const functionSource = (
  source: string,
  signature: string,
  nextSignature: string,
): string => {
  const start = source.indexOf(signature);
  const end = source.indexOf(nextSignature, start + signature.length);

  expect(start).toBeGreaterThanOrEqual(0);
  expect(end).toBeGreaterThan(start);
  return source.slice(start, end);
};

describe('environment asset name contracts', () => {
  it('fails foliage configuration when the edited asset cannot be saved', () => {
    const handler = functionSource(
      handlerSource,
      'bool McpConfigureFoliageType',
      'bool McpCreateLandscapeLayerInfo',
    );

    expect(handler).toContain('if (!McpSafeAssetSave(FoliageType))');
    expect(handler).toContain('TEXT("SAVE_FAILED")');
  });

  it('validates hostile names and the final package path through Unreal APIs', () => {
    const helper = functionSource(
      validationSource,
      'bool McpBuildValidatedEnvironmentAssetPath',
      '\n}\n#endif',
    );
    const hostileExamples = [
      ['../Escape', 'AssetName.Contains(TEXT(".."))'],
      ['Layer//Nested', 'AssetName.Contains(TEXT("/"))'],
      ['Layer\\\\Nested', 'AssetName.Contains(TEXT("\\\\"))'],
      ['Layer.Name', 'IsValidObjectName(ObjectNameReason)'],
      ['Layer:Name', 'IsValidObjectName(ObjectNameReason)'],
    ] as const;

    for (const [hostileName, expectedGuard] of hostileExamples) {
      expect(hostileName.length).toBeGreaterThan(0);
      expect(helper).toContain(expectedGuard);
    }
    expect(helper).toContain('FPackageName::IsValidLongPackageName');
    expect(helper).toContain('PackageValidationReason.ToString()');
    expect(helper).toContain('TEXT("INVALID_ARGUMENT")');
    expect(helper).toContain('TEXT("SECURITY_VIOLATION")');
  });

  it('validates and saves landscape layer info before reporting success', () => {
    const handler = functionSource(
      handlerSource,
      'bool McpCreateLandscapeLayerInfo',
      'bool McpCreateLinearColorCurve',
    );
    const validation = handler.indexOf(
      'McpBuildValidatedEnvironmentAssetPath(Path, LayerName',
    );
    const creation = handler.indexOf('CreatePackage(*PackagePath)');

    expect(validation).toBeGreaterThanOrEqual(0);
    expect(creation).toBeGreaterThan(validation);
    expect(handler).toContain('if (!McpSafeAssetSave(LayerInfo))');
    expect(handler).toContain('if (!Package)');
    expect(handler).toContain('TEXT("PACKAGE_CREATION_FAILED")');
    expect(handler).toContain('TEXT("SAVE_FAILED")');
  });

  it('validates and saves linear color curves before reporting success', () => {
    const handler = functionSource(
      handlerSource,
      'bool McpCreateLinearColorCurve',
      '} // namespace McpEnvironmentHandlers',
    );
    const validation = handler.indexOf(
      'McpBuildValidatedEnvironmentAssetPath(Path, Name',
    );
    const creation = handler.indexOf('CreatePackage(*PackagePath)');

    expect(validation).toBeGreaterThanOrEqual(0);
    expect(creation).toBeGreaterThan(validation);
    expect(handler).toContain('if (!McpSafeAssetSave(Curve))');
    expect(handler).toContain('if (!Package)');
    expect(handler).toContain('TEXT("PACKAGE_CREATION_FAILED")');
    expect(handler).toContain('TEXT("SAVE_FAILED")');
  });
});
