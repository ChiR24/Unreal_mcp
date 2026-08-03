/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { countPureLines } from './plugin-contract-fixtures.js';

const environmentSource = (filename: string): string =>
  readFileSync(
    resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Environment',
      filename,
    ),
    'utf8',
  );

const repositorySource = (filename: string): string =>
  readFileSync(resolve(process.cwd(), filename), 'utf8');

const adapterFiles = [
  'McpAutomationBridge_EnvironmentHandlersBuildAdapters.cpp',
  'McpAutomationBridge_EnvironmentHandlersBuildDeletion.cpp',
  'McpAutomationBridge_EnvironmentHandlersBuildLandscapeFoliage.cpp',
  'McpAutomationBridge_EnvironmentHandlersBuildSkyWeather.cpp',
  'McpAutomationBridge_EnvironmentHandlersBuildSnapshots.cpp',
  'McpAutomationBridge_EnvironmentHandlersBuildWater.cpp',
  'McpAutomationBridge_EnvironmentSnapshotPaths.cpp',
  'McpAutomationBridge_EnvironmentHandlersSnapshotData.cpp',
] as const;

describe('environment build adapter contracts', () => {
  it('keeps responsibility-based environment build shards within 250 pure lines', () => {
    // Given
    const sources = adapterFiles.map((file) => ({
      file,
      source: environmentSource(file),
    }));

    // When
    const oversized = sources
      .map(({ file, source }) => ({ file, pureLines: countPureLines(source) }))
      .filter(({ pureLines }) => pureLines > 250);

    // Then
    expect(oversized).toEqual([]);
  });

  it('evaluates environment operations before recording their result', () => {
    // Given
    const operationSources = [
      environmentSource(
        'McpAutomationBridge_EnvironmentHandlersBuildLandscapeFoliage.cpp',
      ),
      environmentSource(
        'McpAutomationBridge_EnvironmentHandlersBuildSkyWeather.cpp',
      ),
      environmentSource('McpAutomationBridge_EnvironmentHandlersBuildWater.cpp'),
    ].join('\n');

    // When
    const directResultCalls = operationSources.match(
      /MarkActorConfigurationResult\(\s*Context,\s*Mcp/gu,
    );

    // Then
    expect(directResultCalls).toBeNull();
    expect(operationSources).toContain('const bool bResult = Mcp');
  });

  it('configures environment actor components instead of only actor wrappers', () => {
    // Given
    const source = environmentSource(
      'McpAutomationBridge_EnvironmentHandlersBuildSkyWeather.cpp',
    );
    const adapterSource = environmentSource(
      'McpAutomationBridge_EnvironmentHandlersBuildAdapters.cpp',
    );

    // When
    const requiredComponentClasses = [
      '/Script/Engine.SkyAtmosphereComponent',
      '/Script/Engine.SkyLightComponent',
      '/Script/Engine.DirectionalLightComponent',
      '/Script/Engine.ExponentialHeightFogComponent',
      '/Script/Engine.VolumetricCloudComponent',
      '/Script/Engine.WindDirectionalSourceComponent',
    ];

    // Then
    for (const classPath of requiredComponentClasses) {
      expect(source).toContain(classPath);
    }
    expect(source).toContain('TEXT("componentPath")');
    expect(source).toContain('TEXT("COMPONENT_CREATION_FAILED")');
    expect(source).toContain('TEXT("configurationErrors")');
    expect(source).toContain('TEXT("CONFIGURATION_FAILED")');
    expect(adapterSource).toContain('TEXT("componentPath")');
    expect(adapterSource).toContain('TEXT("COMPONENT_CREATION_FAILED")');
    expect(adapterSource).toContain('TEXT("configurationErrors")');
    expect(adapterSource).toContain('TEXT("CONFIGURATION_FAILED")');
  });

  it('deletes exact actor paths and rejects ambiguous names through the editor subsystem', () => {
    // Given
    const source = environmentSource(
      'McpAutomationBridge_EnvironmentHandlersBuildDeletion.cpp',
    );

    // When
    const destroyResult = source.indexOf('ActorSubsystem->DestroyActors(ActorsToDestroy)');
    const deletedTarget = source.indexOf(
      'DeletedTargets.Add(ResolvedTarget.Target)',
      destroyResult,
    );

    // Then
    expect(source).toContain('GetEditorSubsystem<UEditorActorSubsystem>()');
    expect(source).toContain('Actor->GetPathName().Equals(Target');
    expect(source).toContain('TEXT("ambiguousTargets")');
    expect(source).toContain('TEXT("missingTargets")');
    expect(source).toContain('TEXT("AMBIGUOUS_ACTOR")');
    expect(source).toContain('TEXT("DELETE_PARTIAL")');
    expect(source).toContain('TSet<AActor *> ResolvedActors');
    expect(source).toContain('ResolvedActors.Contains(Actor)');
    expect(source).toContain('ResolvedActors.Add(Actor)');
    expect(source).not.toContain('EditorDestroyActor');
    expect(destroyResult).toBeGreaterThan(-1);
    expect(deletedTarget).toBeGreaterThan(destroyResult);
    expect(source).toContain('!AmbiguousTargets.IsEmpty() || !MissingTargets.IsEmpty()');
    expect(source).toContain('const TArray<AActor *> RemainingActors');
    expect(source).toContain('!RemainingActors.Contains(ResolvedTarget.Actor)');
    expect(source).toContain('GEditor->UndoTransaction(false)');
    expect(source).toContain('TEXT("rolledBack")');
    expect(source).toContain('ResolvedTarget.ActorPath, RestoredActors');
    expect(source).toContain('TEXT("DELETE_ROLLBACK_FAILED")');
    expect(source.indexOf('if (!AmbiguousTargets.IsEmpty() || !MissingTargets.IsEmpty())')).toBeLessThan(
      destroyResult,
    );
  });

  it('bounds snapshot JSON and resolves paths through the project security boundary', () => {
    // Given
    const handlerSource = environmentSource(
      'McpAutomationBridge_EnvironmentHandlersBuildSnapshots.cpp',
    );
    const pathSource = environmentSource(
      'McpAutomationBridge_EnvironmentSnapshotPaths.cpp',
    );

    // When
    const pathValidation = pathSource.indexOf('McpResolveProjectFilePath(');
    const actionHandler = handlerSource.slice(
      handlerSource.indexOf('bool HandleBuildSnapshotAction'),
    );
    const resolverCall = actionHandler.indexOf(
      'McpResolveEnvironmentSnapshotPath(',
    );
    const actionDispatch = actionHandler.indexOf('return LowerSub ==');
    const fileAccess = handlerSource.search(
      /(?:OpenRead|SaveStringToFile)\(/u,
    );

    // Then
    expect(handlerSource).toContain('MaxSnapshotBytes');
    expect(handlerSource).toContain('PlatformFile.OpenRead(*AbsolutePath)');
    expect(handlerSource).toContain('ReadHandle->Size()');
    expect(handlerSource).toContain('ReadHandle->Read(');
    expect(handlerSource).toContain('FileSize < 0');
    expect(handlerSource).toContain('FJsonSerializer::Deserialize');
    expect(handlerSource).toContain('FJsonSerializer::Serialize');
    expect(handlerSource).toContain('FFileHelper::SaveStringToFile');
    expect(pathSource).toContain('CanonicalizeAllowedSnapshotRelativePath');
    expect(pathSource).toContain('tmp/unreal-mcp/');
    expect(pathSource).toContain('Saved/unreal-mcp/');
    expect(handlerSource).not.toContain('MarkUnsupported');
    expect(pathValidation).toBeGreaterThan(-1);
    expect(resolverCall).toBeGreaterThan(-1);
    expect(fileAccess).toBeGreaterThan(-1);
    expect(actionDispatch).toBeGreaterThan(resolverCall);
  });

  it('canonicalizes leading dot segments before enforcing the snapshot allowlist', () => {
    // Given
    const source = environmentSource(
      'McpAutomationBridge_EnvironmentSnapshotPaths.cpp',
    );

    // When
    const leadingDotCanonicalization = source.indexOf(
      'while (Path.StartsWith(TEXT("./")',
    );
    const allowlistValidation = source.indexOf(
      'CanonicalizeAllowedSnapshotRelativePath(RelativePath)',
    );

    // Then
    expect(leadingDotCanonicalization).toBeGreaterThan(-1);
    expect(allowlistValidation).toBeGreaterThan(leadingDotCanonicalization);
  });

  it('captures and restores directional-light and skylight snapshot state', () => {
    // Given
    const source = environmentSource(
      'McpAutomationBridge_EnvironmentHandlersSnapshotData.cpp',
    );

    // When
    const requiredState = [
      'TEXT("timeOfDay")',
      'TEXT("directionalLightRotation")',
      'TEXT("sunIntensity")',
      'TEXT("skylightIntensity")',
      'SetActorRotation',
      'SetIntensity',
    ];

    // Then
    for (const stateContract of requiredState) {
      expect(source).toContain(stateContract);
    }
    expect(source).toContain('TActorIterator<ADirectionalLight>');
    expect(source).toContain('TActorIterator<ASkyLight>');
    expect(source).toContain('TNumericLimits<float>::Max()');
    expect(source).toContain('RotationTimeOfDay');
    expect(source).toContain('bConsistentTime');
    expect(source).toContain('TryGetStringField(TEXT("directionalLightActorPath")');
    expect(source).toContain('TryGetStringField(TEXT("skyLightActorPath")');
    expect(source).toContain('Actor->GetPathName().Equals(ActorPath');
    expect(source).toContain('Actor->GetPackage()->GetName().Equals(ActorPath');
    expect(source).toContain(
      'FindSnapshotDirectionalLight(DirectionalLightActorPath)',
    );
    expect(source).toContain('FindSnapshotSkyLight(SkyLightActorPath)');
    expect(source).toContain('ReadSnapshotNumber(*RotationObject, TEXT("pitch")');
    expect(source).toContain('ReadSnapshotNumber(*RotationObject, TEXT("yaw")');
    expect(source).toContain('ReadSnapshotNumber(*RotationObject, TEXT("roll")');
  });

  it('derives rotation only for unversioned snapshots with no rotation field', () => {
    // Given
    const source = environmentSource(
      'McpAutomationBridge_EnvironmentHandlersSnapshotData.cpp',
    );

    // When
    const legacyFallback = source.indexOf(
      'else if (!bHasVersion && bValidLightingFields)',
    );

    // Then
    expect(source).toContain(
      'const bool bHasVersion = VersionField != nullptr',
    );
    expect(source).toContain(
      'if (Snapshot->HasField(TEXT("directionalLightRotation")))',
    );
    expect(legacyFallback).toBeGreaterThan(-1);
    expect(source.indexOf('RotationFromTimeOfDay(TimeOfDay)')).toBeGreaterThan(
      legacyFallback,
    );
  });

  it('accepts only the supported integral snapshot version', () => {
    // Given
    const source = environmentSource(
      'McpAutomationBridge_EnvironmentHandlersSnapshotData.cpp',
    );

    // When
    const versionRead = source.indexOf(
      'TryGetNumberField(TEXT("version"), SnapshotVersion)',
    );
    const supportedVersionCheck = source.indexOf('SnapshotVersion != 1.0');

    // Then
    expect(versionRead).toBeGreaterThan(-1);
    expect(source).toContain('(*VersionField)->Type != EJson::Number');
    expect(source).toContain('!FMath::IsFinite(SnapshotVersion)');
    expect(supportedVersionCheck).toBeGreaterThan(versionRead);
  });

  it('mutates and verifies snapshot lighting before and after import', () => {
    // Given
    const source = repositorySource(
      'tests/mcp-tools/world/build-environment.test.mjs',
    );

    // When
    const skyLightSetup = source.indexOf('configure_sky_light');
    const directionalLightSetup = source.indexOf(
      'configure_directional_light_atmosphere',
    );
    const exportSnapshot = source.indexOf('export_snapshot');
    const mutateSun = source.indexOf('CONFIG: mutate snapshot sun rotation');
    const mutatedReadback = source.indexOf('INFO: snapshot sun rotation mutated');
    const importSnapshot = source.indexOf(
      'ACTION: import_snapshot restores lighting',
    );
    const restoredReadback = source.indexOf(
      'INFO: snapshot sun rotation restored',
    );

    // Then
    expect(skyLightSetup).toBeGreaterThan(-1);
    expect(directionalLightSetup).toBeGreaterThan(-1);
    expect(exportSnapshot).toBeGreaterThan(skyLightSetup);
    expect(exportSnapshot).toBeGreaterThan(directionalLightSetup);
    expect(mutateSun).toBeGreaterThan(exportSnapshot);
    expect(mutatedReadback).toBeGreaterThan(mutateSun);
    expect(importSnapshot).toBeGreaterThan(exportSnapshot);
    expect(importSnapshot).toBeGreaterThan(mutatedReadback);
    expect(restoredReadback).toBeGreaterThan(importSnapshot);
    expect(source).toContain(
      'structuredContent.result.data.rotation.0',
    );
    expect(source).toContain('structuredContent.result.data.value');
    expect(source).toContain(
      'structuredContent.result.directionalLightRotation.pitch',
    );
    expect(source).toContain('MUTATED_SUN_INTENSITY');
    expect(source).toContain('ORIGINAL_SKY_INTENSITY');
    expect(source).toContain(
      "directionalLightActorPath: '${captured:snapshotSunPath}'",
    );
    expect(source).toContain(
      "skyLightActorPath: '${captured:snapshotSkyPath}'",
    );
  });

  it('does not route the skylight SourceType enum through generic settings', () => {
    // Given
    const source = repositorySource(
      'tests/mcp-tools/world/build-environment.test.mjs',
    );
    const snapshotSetup = source.slice(
      source.indexOf('CONFIG: configure snapshot skylight'),
      source.indexOf('ACTION: export_snapshot'),
    );

    // When
    const unsupportedSourceType = snapshotSetup.indexOf('SourceType');

    // Then
    expect(unsupportedSourceType).toBe(-1);
  });

  it('creates a truthful three-actor sky rig for create_sky_sphere', () => {
    // Given
    const source = environmentSource(
      'McpAutomationBridge_EnvironmentHandlersBuildSkyWeather.cpp',
    );

    // When
    const skySphereBranch = source.slice(
      source.indexOf('LowerSub == TEXT("create_sky_sphere")'),
      source.indexOf('LowerSub == TEXT("configure_sky_atmosphere")'),
    );

    // Then
    expect(skySphereBranch).toContain('CreateSkySphereRig(Context)');
    expect(source).toContain(
      'TEXT("createdActorCount"), CreatedActorCount',
    );
    expect(source).toContain('TEXT("configuredActorCount"), ConfiguredActorCount');
    expect(source).toContain('bOutCreated = !bExisted && !ActorPath.IsEmpty()');
    expect(source).toContain('TEXT("skyAtmosphereActorPath")');
    expect(source).toContain('TEXT("directionalLightActorPath")');
    expect(source).toContain('TEXT("skyLightActorPath")');
  });
});
