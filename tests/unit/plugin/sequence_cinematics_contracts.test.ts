import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  cinematicsSource,
  privateSource,
} from './sequence_contract_test_utils.js';

const forbiddenNumberedPrefix = String.fromCharCode(112, 104, 97, 115, 101);
const forbiddenNumberedIdentifier = new RegExp(
  `${forbiddenNumberedPrefix}[ _-]*\\d+`,
  'i',
);

describe('sequence cinematics contracts', () => {
  it('does not retain unused process-global sequence path sets', () => {
    const source = readFileSync(
      resolve(
        process.cwd(),
        'src',
        'tools',
        'handlers',
        'sequence',
        'sequence-handler-state.ts',
      ),
      'utf8',
    );

    expect(source).not.toContain('managedSequences');
    expect(source).not.toContain('deletedSequences');
    expect(source).not.toContain('markSequenceCreated');
    expect(source).not.toContain('markSequenceDeleted');
  });

  it('accepts only particle evaluator-compatible bindings', () => {
    const source = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsTracks.cpp',
    );

    expect(source).toContain('BoundTemplate->IsA<UFXSystemComponent>()');
    expect(source).toContain('BoundTemplate->IsA<AEmitter>()');
    expect(source).not.toContain(
      'FindComponentByClass<UFXSystemComponent>()',
    );
  });

  it('rejects camera no-ops and cleans failed spawned rigs', () => {
    const camera = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsCameras.cpp',
    );
    const rigs = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsCameraRigs.cpp',
    );

    expect(camera).toContain('CAMERA_COMPONENT_NOT_FOUND');
    expect(camera).toContain('if (AppliedCount == 0)');
    expect(rigs).toContain('RIG_CLASS_MISMATCH');
    expect(rigs).toContain('No valid camera rig settings were provided');
    expect(rigs).toMatch(/if \(bSpawned\)\s+Actor->Destroy\(\)/);
    expect(rigs).toContain('appliedProperties');
  });

  it('validates property types and requires real sections', () => {
    const property = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsPropertyTrack.cpp',
    );
    const bindings = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsBindingTracks.cpp',
    );
    const transformStart = bindings.indexOf('bool HandleAddTransformTrack');
    const eventStart = bindings.indexOf('bool HandleAddEventTrack');

    expect(property).toContain('PROPERTY_TYPE_MISMATCH');
    expect(property).toContain(
      'SetPropertyNameAndPath(Property->GetFName(), PropertyPath)',
    );
    expect(bindings.slice(transformStart, eventStart)).toContain(
      'SECTION_CREATION_FAILED',
    );
    expect(bindings.slice(eventStart)).toContain('SECTION_CREATION_FAILED');
  });

  it('uses one strict frame-rate parser across sequence surfaces', () => {
    const parser = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequenceFrameRate.cpp',
    );
    const consumers = [
      privateSource(
        'Domains',
        'Sequence',
        'McpAutomationBridge_SequenceHandlersFrameRate.cpp',
      ),
      privateSource(
        'Domains',
        'Sequence',
        'McpAutomationBridge_SequenceHandlersProperties.cpp',
      ),
      cinematicsSource(
        'McpAutomationBridge_SequenceCinematicsAssets.cpp',
      ),
      privateSource(
        'Domains',
        'Sequence',
        'MovieRender',
        'McpAutomationBridge_SequenceMovieRenderOutput.cpp',
      ),
      privateSource(
        'Domains',
        'Sequence',
        'RecordReplay',
        'McpAutomationBridge_SequenceTakeRecorderRuntime.cpp',
      ),
    ];

    expect(parser).toContain('Denominator <= 0');
    expect(parser).toContain('FMath::IsFinite(Value)');
    expect(parser).toContain('EndsWith(TEXT("fps")');
    for (const consumer of consumers) {
      expect(consumer).toContain('McpSequenceFrameRate::TryParse');
    }
  });

  it('rejects failed shot, fade, and visibility section creation', () => {
    const assets = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsAssets.cpp',
    );
    const tracks = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsTracks.cpp',
    );

    expect(assets).toContain('A valid shotSequencePath is required');
    expect(assets).toContain('SECTION_CREATION_FAILED');
    expect(assets.match(/RemoveTrackAfterSectionFailure/g)).toHaveLength(2);
    expect(tracks.match(/SECTION_CREATION_FAILED/g)?.length).toBeGreaterThanOrEqual(2);
    expect(tracks.match(/RemoveTrackAfterSectionFailure/g)?.length)
      .toBeGreaterThanOrEqual(3);
  });

  it('rolls back every newly created track and camera binding on section failure', () => {
    const helper = cinematicsSource(
      'McpAutomationBridge_SequenceCinematics.cpp',
    );
    const bindings = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsBindingTracks.cpp',
    );
    const property = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsPropertyTrack.cpp',
    );
    const cameras = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsCameraTracks.cpp',
    );

    expect(helper).toContain('RemoveTrackAfterSectionFailure');
    expect(bindings.match(/RemoveTrackAfterSectionFailure/g)?.length)
      .toBeGreaterThanOrEqual(2);
    expect(property).toContain('RemoveTrackAfterSectionFailure');
    expect(cameras.match(/RemoveTrackAfterSectionFailure/g)?.length)
      .toBeGreaterThanOrEqual(2);
    expect(cameras).toContain('Sequence->UnbindPossessableObjects(Guid)');
    expect(cameras).toContain('MovieScene->RemovePossessable(Guid)');
  });

  it('restricts sequence mutation targets to project content', () => {
    const helper = cinematicsSource(
      'McpAutomationBridge_SequenceCinematics.cpp',
    );
    const assets = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsAssets.cpp',
    );
    const security = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequencePathSecurity.cpp',
    );

    expect(security).toContain('ValidateWritableAssetPath');
    expect(security).toContain('TEXT("/Game/")');
    expect(helper).toContain('SEQUENCE_PATH_NOT_WRITABLE');
    expect(assets).toContain('ValidateWritableAssetPath');
  });

  it('rejects an existing non-LevelSequence asset', () => {
    const source = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsAssets.cpp',
    );

    expect(source).toContain('Cast<ULevelSequence>(Existing)');
    expect(source).toContain('ASSET_TYPE_MISMATCH');
  });

  it('propagates requested save failures', () => {
    const helper = cinematicsSource(
      'McpAutomationBridge_SequenceCinematics.cpp',
    );
    const header = cinematicsSource(
      'McpAutomationBridge_SequenceCinematics.h',
    );
    const files = [
      'McpAutomationBridge_SequenceCinematicsAssets.cpp',
      'McpAutomationBridge_SequenceCinematicsBindingTracks.cpp',
      'McpAutomationBridge_SequenceCinematicsCameraTracks.cpp',
      'McpAutomationBridge_SequenceCinematicsPropertyTrack.cpp',
      'McpAutomationBridge_SequenceCinematicsTracks.cpp',
    ].map(cinematicsSource).join('\n');

    expect(header).toContain('bool MaybeSaveSequence(');
    expect(helper).toContain('TEXT("ASSET_SAVE_FAILED")');
    expect(helper).toContain('TEXT("ASSET_PREFLIGHT_SAVE_FAILED")');
    expect(helper).toContain('UPackageTools::ReloadPackages(');
    expect(helper).toContain('EReloadPackagesInteractionMode::AssumePositive');
    expect(helper).not.toContain('Package->SetDirtyFlag(false)');
    expect(helper).toContain('HasAnyPackageFlags(PKG_NewlyCreated)');
    expect(helper).toContain('ClearPackageFlags(PKG_NewlyCreated)');
    expect(helper).toContain('IFileManager::Get().FileExists(');
    expect(helper).toContain('TEXT("rollbackError")');
    expect(helper).toContain('TEXT("rolledBack")');
    expect(files).toContain('if (!MaybeSaveSequence(');
    expect(files).not.toMatch(/^\s*MaybeSaveSequence\(/m);
  });

  it('does not treat stale package existence as a successful save', () => {
    const source = privateSource(
      'Safety',
      'McpSafeOperationsAssetSave.h',
    );

    expect(source).toContain(
      '(bPromptSaveSucceeded || bEditorSaveSucceeded) && bExistsOnDisk',
    );
    expect(source).not.toContain(
      'bPromptSaveSucceeded || bEditorSaveSucceeded || bExistsOnDisk',
    );
  });

  it('reuses existing actor bindings', () => {
    const source = cinematicsSource(
      'McpAutomationBridge_SequenceCinematics.cpp',
    );

    expect(source).toContain(
      'FindExistingBinding(Sequence, Actor, Actor->GetWorld())',
    );
    expect(source.indexOf('FindExistingBinding')).toBeLessThan(
      source.indexOf('AddPossessable'),
    );
  });

  it('resolves every public sequence path alias', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequenceHandlersRegistry.cpp',
    );

    expect(source).toContain(
      '{TEXT("path"), TEXT("sequencePath"), TEXT("assetPath")}',
    );
  });

  it('creates material tracks on primitive component bindings', () => {
    const source = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsMaterialTrack.cpp',
    );

    expect(source).toContain('ResolveMaterialComponent(');
    expect(source).toContain('MATERIAL_COMPONENT_REQUIRED');
    expect(source).toContain('MATERIAL_SLOT_MISMATCH');
    expect(source).toContain(
      'Possessable->SetParent(ParentGuid, MovieScene)',
    );
    const handler = source.slice(
      source.indexOf('bool HandleAddMaterialParameterTrack'),
    );
    expect(handler.indexOf('Component->GetMaterial(MaterialIndex)')).toBeLessThan(
      handler.indexOf('CreateMaterialComponentBinding('),
    );
    expect(source).toContain('Sequence->UnbindPossessableObjects(ComponentGuid)');
    expect(source).toContain(
      'Sequence->GetMovieScene()->RemovePossessable(ComponentGuid)',
    );
    expect(source).toContain('Track->SetMaterialInfo(Info)');
    expect(source).toContain('Track->SetMaterialIndex(MaterialIndex)');
    expect(source).toContain('AddScalarParameterKey');
    expect(source).toContain('AddColorParameterKey');
    expect(source).toContain('MATERIAL_PARAMETER_VALUE_INVALID');
    expect(source).toContain('parameterType');
  });

  it('converts display frames to MovieScene tick frames', () => {
    const source = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsFrames.cpp',
    );
    const frameMath = privateSource(
      'Domains',
      'Sequence',
      'Validation',
      'McpAutomationBridge_SequenceFrameMath.cpp',
    );
    const tests = cinematicsSource(
      'McpAutomationBridge_SequenceCinematicsFrameTests.cpp',
    );

    expect(source).toContain('McpSequenceFrameMath::TryTransformFrame');
    expect(source).toContain('MovieScene->GetDisplayRate()');
    expect(source).toContain('MovieScene->GetTickResolution()');
    expect(frameMath).toContain('MIN_int32');
    expect(frameMath).toContain('MAX_int32');
    expect(tests).toContain('DisplayFramesConvertToTicks');
    expect(tests).toContain('24024');
  });

  it('keeps the domain dispatcher free of numbered roadmap names', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequenceHandlers.cpp',
    );

    expect(source).toContain('TryHandleCinematics');
    expect(source).not.toMatch(forbiddenNumberedIdentifier);
  });
});
