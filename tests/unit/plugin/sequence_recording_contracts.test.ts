import { describe, expect, it } from 'vitest';

import { privateSource } from './sequence_contract_test_utils.js';

describe('sequence recording contracts', () => {
  it('keeps Take Recorder panel setup idempotent', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderRuntime.cpp',
    );

    expect(source).toContain('FTakeRecorderPanelConfiguration');
    expect(source).toContain('RequestedSequence.Get() == CandidateSequence');
    expect(source).toContain('ActivePanelConfiguration.Matches(');
    expect(source).toContain('ActivePanelConfiguration.Set(');
    expect(source).toContain('Panel->GetMode() != RequestedMode');
    expect(source).toContain('ETakeRecorderPanelMode::RecordingInto');
    expect(source).toContain('ETakeRecorderPanelMode::NewRecording');
  });

  it('routes stop and active-state rejection before panel setup', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceRecordReplayTakeRecorder.cpp',
    );
    const stopDispatch = source.indexOf('Action == TEXT("stop_recording")');
    const activeGuard = source.indexOf(
      'UTakeRecorderBlueprintLibrary::GetActiveRecorder()',
    );
    const panelSetup = source.indexOf('ConfigurePanel(');

    expect(stopDispatch).toBeGreaterThan(-1);
    expect(activeGuard).toBeGreaterThan(stopDispatch);
    expect(panelSetup).toBeGreaterThan(activeGuard);
    expect(source).toContain('TEXT("RECORDING_ACTIVE")');
  });

  it('restores the complete Take Recorder panel snapshot after failures', () => {
    const dispatch = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceRecordReplayTakeRecorder.cpp',
    );
    const runtime = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderRuntime.cpp',
    );
    const snapshot = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderPanelSnapshot.cpp',
    );

    expect(dispatch.indexOf('CaptureTakeRecorderPanelSnapshot(')).toBeLessThan(
      dispatch.indexOf('ConfigurePanel('),
    );
    expect(dispatch).toContain('if (!bSucceeded)');
    expect(dispatch).toContain('RestoreTakeRecorderPanelSnapshot(');
    expect(snapshot).toContain('Panel->GetLevelSequence()');
    expect(snapshot).toContain('Panel->GetMode()');
    expect(snapshot).toContain('Panel->GetFrameRate()');
    expect(snapshot).toContain('MetaData->GetFrameRateFromTimecode()');
    expect(snapshot).not.toContain('DuplicateObject');
    expect(runtime).toContain('Panel->SetupForRecording_LevelSequence(');
    expect(runtime).toContain('Panel->SetupForRecordingInto_LevelSequence(');
    expect(runtime).toContain('Panel->SetupForEditing(');
    expect(runtime).toContain('Panel->SetupForViewing(');
    expect(runtime).not.toContain('RestoreTakeRecorderSourceSnapshot(');
  });

  it('keeps source updates guarded and rollback-capable', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderSources.cpp',
    );
    const preparation = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderSourcePreparation.cpp',
    );
    const prepare = source.indexOf('PrepareTakeRecorderSources(');
    const snapshot = source.indexOf('OriginalSources');
    const mutate = source.indexOf('Sources->Modify()');

    expect(source).toContain(
      'UTakeRecorderBlueprintLibrary::GetActiveRecorder()',
    );
    expect(preparation).toContain('OutPrepared.Actors.AddUnique(Actor)');
    expect(preparation).toContain(
      'OutPrepared.Classes.AddUnique(SourceClass)',
    );
    expect(prepare).toBeGreaterThan(-1);
    expect(snapshot).toBeGreaterThan(prepare);
    expect(mutate).toBeGreaterThan(snapshot);
    expect(source).toContain('RestoreActorSourceOptions(');
    expect(source).toContain('CopyPropertiesForUnrelatedObjects(');
    expect(source).not.toContain('DuplicateObject');
  });

  it('reports cleanup paths and records configured panel sources', () => {
    const results = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderResults.cpp',
    );
    const recording = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderRecording.cpp',
    );

    expect(results).toContain('TEXT("sequencePackagePath")');
    expect(results).toContain('TEXT("subsceneFolderPath")');
    expect(results).toContain('SequencePackagePath + TEXT("_Subscenes")');
    expect(recording).toContain(
      'bHasSourceConfiguration =\n        HasSourceConfigurationRequest(Payload)',
    );
    expect(recording).toContain('RestoreTakeRecorderSources(');
    expect(recording).toContain('TEXT("SOURCE_ROLLBACK_FAILED")');
    expect(recording).toContain('TEXT("sourcesRestored")');
  });

  it('carries Take Recorder rollback state through async start failures', () => {
    const recording = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderRecording.cpp',
    );
    const results = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderResults.cpp',
    );
    const internal = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceTakeRecorderInternal.h',
    );

    expect(internal).toContain('FTakeRecorderStartRollbackState');
    expect(internal).toContain('SourceSnapshots');
    expect(recording).toContain('RollbackState->PanelSnapshot = PanelSnapshot');
    expect(recording).toContain('MoveTemp(OriginalSourceSnapshots)');
    expect(results).toContain('RollBackTakeRecorderStart');
    expect(results).toContain('RestoreTakeRecorderSources(');
    expect(results).toContain('RestoreTakeRecorderPanelSnapshot(');
    expect(results).toContain('TEXT("rolledBack")');
    expect(results).toContain('START_ROLLBACK_FAILED');
  });

  it('binds add_actor possessables to the live actor object', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequenceHandlersBindings.cpp',
    );

    expect(source).toContain('BindPossessableObject(BindingGuid, *Found');
    expect(source).toContain('LevelSeq->MarkPackageDirty()');
  });

  it('bounds every replay numeric input before float conversion', () => {
    const internal = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceReplayInternal.h',
    );
    const validation = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceReplayValidation.cpp',
    );
    const routing = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceRecordReplayDemo.cpp',
    );
    const playback = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceReplayPlayback.cpp',
    );
    const runtime = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceReplayRuntime.cpp',
    );

    for (const constant of [
      'MaxCheckpointSaveMsPerFrame',
      'MaxReplayRecordTimeSeconds',
      'MaxReplayPlaybackSpeed',
      'MaxReplaySeekTimeSeconds',
      'MaxKillcamDurationSeconds',
    ]) {
      expect(validation).toContain(constant);
    }
    expect(validation).toContain('Value > Maximum');
    expect(validation).toContain('TEXT("speed")');
    expect(validation).toContain('TEXT("timeSeconds")');
    expect(validation).toContain('TEXT("seconds")');
    expect(validation).toMatch(
      /TEXT\("checkpointSaveMaxMSPerFrame"\), false,/,
    );
    expect(validation).toMatch(/TEXT\("maxRecordTimeSeconds"\), false,/);
    expect(internal).toMatch(
      /CheckpointSaveMaxMSPerFrame = [1-9][0-9]*(?:\.[0-9]+)?f;/,
    );
    expect(internal).toMatch(
      /MaxRecordTimeSeconds = [1-9][0-9]*(?:\.[0-9]+)?f;/,
    );
    expect(runtime).toMatch(
      /Replay->SetCheckpointSaveMaxMSPerFrame\(\s*GMcpReplaySettings\.CheckpointSaveMaxMSPerFrame\);/,
    );
    expect(runtime).toMatch(
      /Driver->SetMaxDesiredRecordTimeMS\(\s*GMcpReplaySettings\.MaxRecordTimeSeconds \* 1000\.0f\);/,
    );
    expect(runtime).not.toContain(
      'if (GMcpReplaySettings.CheckpointSaveMaxMSPerFrame > 0.0f)',
    );
    expect(runtime).not.toContain(
      'if (GMcpReplaySettings.MaxRecordTimeSeconds > 0.0f)',
    );
    expect(routing).toContain('ValidateReplayRequest(Payload');
    expect(playback).toContain('FMath::IsFinite');
  });

  it('waits for replay seek and killcam targets before reporting success', () => {
    const playback = privateSource(
      'Domains',
      'Sequence',
      'RecordReplay',
      'McpAutomationBridge_SequenceReplayPlayback.cpp',
    );

    expect(playback).toContain('FReplaySeekWaitState');
    expect(playback).toContain('RegisterAutomationRequestCancellation');
    expect(playback).toContain('GetDemoCurrentTime()');
    expect(playback).toContain('SeekToleranceSeconds');
    expect(playback).toContain('TEXT("seekCompleted"), true');
    expect(playback).toContain('TEXT("seekPending"), false');
    expect(playback).toContain('REPLAY_SEEK_TIMEOUT');
    expect(playback).not.toContain('TEXT("seekPending"), true');
  });
});
