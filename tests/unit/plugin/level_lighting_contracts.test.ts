import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginSource = (...parts: string[]): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
      ...parts,
    ),
    'utf8',
  );

describe('level and lighting build contracts', () => {
  it('uses typed lighting build APIs and reports their result', () => {
    const source = pluginSource(
      'Domains',
      'Lighting',
      'McpAutomationBridge_LightingHandlersBuild.cpp',
    );
    const compatibilitySource = pluginSource(
      'Domains',
      'Lighting',
      'McpAutomationBridge_LightingBuildCompatibility.cpp',
    );

    expect(source).toContain('Payload.IsValid()');
    expect(source).toContain(
      'ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3',
    );
    expect(source).toContain('BuildLightMaps(QualityEnum, bBuildReflectionCaptures)');
    expect(source).toContain('RunLegacyLightingBuild(*World, QualityEnum)');
    expect(compatibilitySource).toContain(
      'FEditorBuildUtils::EditorBuild(',
    );
    expect(source).toContain('TEXT("UNSUPPORTED_BUILD_OPTION")');
    expect(source).not.toContain('TEXT("BuildLighting %s")');
    expect(source).not.toContain('GEditor->Exec(');
  });

  it('preserves persisted full-build settings around legacy engine builds', () => {
    const source = pluginSource(
      'Domains',
      'Lighting',
      'McpAutomationBridge_LightingBuildCompatibility.cpp',
    );

    for (const key of [
      'OnlyBuildSelected',
      'OnlyBuildCurrentLevel',
      'OnlyBuildSelectedLevels',
      'OnlyBuildVisibility',
    ]) {
      expect(source).toContain(`TEXT("${key}")`);
    }
    expect(source).toContain('GConfig->GetBool(');
    expect(source).toContain('GConfig->SetBool(');
    expect(source).toContain('GConfig->RemoveKey(');
    expect(source).toContain('RestoreBoolSetting');
  });

  it('restores level lighting scenario mutation and enumeration', () => {
    const source = pluginSource(
      'Domains',
      'Level',
      'World',
      'McpAutomationBridge_LevelHandlersLighting.cpp',
    );

    expect(source).toContain(
      'TargetLevel->SetLightingScenario(bIsLightingScenario);',
    );
    expect(source).toContain('Level->bIsLightingScenario');
    expect(source).toContain('TEXT("scenarios")');
    expect(source).not.toContain('SendUnsupportedLevelAction');
    expect(source).not.toContain('TEXT("NOT_IMPLEMENTED")');
  });

  it('checks the level navigation and full-build results', () => {
    const source = pluginSource(
      'Domains',
      'Level',
      'World',
      'McpAutomationBridge_LevelHandlersLighting.cpp',
    );

    expect(source).toContain(
      'FEditorBuildUtils::EditorBuild(World, FBuildOptions::BuildAIPaths)',
    );
    expect(source).toContain(
      'FEditorBuildUtils::EditorBuild(World, FBuildOptions::BuildAll)',
    );
    expect(source).toContain('TEXT("BUILD_FAILED")');
    expect(source).not.toContain('GEditor->Exec(World, TEXT("Build"))');
    expect(source).not.toContain('NavSys->Build()');
  });

  it('advertises rendering method values in the active render sources', () => {
    const postProcessSource = pluginSource(
      'Domains',
      'Render',
      'McpAutomationBridge_RenderPostProcessColor.cpp',
    );
    const supportEnumsSource = pluginSource(
      'Domains',
      'Render',
      'McpAutomationBridge_RenderSupportEnums.h',
    );

    expect(postProcessSource).toContain('TEXT("Filmic")');
    for (const value of ['CinematicDOF', 'Manual']) {
      expect(supportEnumsSource).toContain(`TEXT("${value}")`);
    }
  });

  it('does not report scene-capture resolution changes without a render target', () => {
    const source = pluginSource(
      'Domains',
      'Render',
      'McpAutomationBridge_RenderSceneCapture.cpp',
    );

    expect(source).toContain('RENDER_TARGET_NOT_ASSIGNED');
    expect(source).toContain('Capture2D->TextureTarget->ResizeTarget(Resolution, Resolution);');
    expect(source).toContain('Result->SetNumberField(TEXT("resolution")');
  });

});
