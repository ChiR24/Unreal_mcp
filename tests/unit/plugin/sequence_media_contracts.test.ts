import { describe, expect, it } from 'vitest';

import {
  privateSource,
} from './sequence_contract_test_utils.js';

describe('sequence media contracts', () => {
  it('does not mutate existing media assets during create actions', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaReflection.cpp',
    );

    expect(source).toContain('MEDIA_ASSET_ALREADY_EXISTS');
    expect(source).not.toContain('OutCreated.Object = Existing');
  });

  it('restricts media asset creation to the exact Game mount', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaReflection.cpp',
    );

    expect(source).toContain('ValidateWritableAssetPath');
    expect(source).not.toContain(
      'Folder.StartsWith(TEXT("/Game"))',
    );
  });

  it('restricts local media and render paths to project-owned roots', () => {
    const source = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequencePathSecurity.cpp',
    );

    expect(source).toContain('Path.StartsWith(TEXT("/Saved/")');
    expect(source).toContain('FPaths::ProjectSavedDir()');
    expect(source).toContain('FPaths::ProjectContentDir()');
    expect(source).not.toContain('Path.StartsWith(TEXT("/Temp/")');
    expect(source).not.toContain('FPlatformProcess::UserTempDir()');
    expect(source).not.toContain('OutRoots.Add(TEXT("/tmp"))');
    expect(source).not.toContain('OutRoots.Add(TEXT("/var/tmp"))');
    expect(source).toContain(
      'Render output directories must be under Project Saved.',
    );
  });

  it('disables all network-backed media URLs', () => {
    const remoteSecurity = privateSource(
      'Domains',
      'Sequence',
      'McpAutomationBridge_SequenceRemoteMediaSecurity.cpp',
    );
    const mediaSources = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaSources.cpp',
    );

    expect(remoteSecurity).toContain('REMOTE_MEDIA_NETWORK_DISABLED');
    expect(remoteSecurity).not.toContain('bAllowLoopbackMediaUrls');
    expect(remoteSecurity).not.toContain('AllowedLoopbackMediaUrlPrefix');
    expect(remoteSecurity).not.toContain('ProbeLoopbackEndpoint');
    expect(remoteSecurity).toContain('ERemoteMediaUrlError::NotAllowed');
    expect(mediaSources).toContain('MEDIA_URL_NOT_ALLOWED');
  });

  it('rejects empty supplied playback aliases before closing the player', () => {
    const playback = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaPlayback.cpp',
    );

    expect(playback).toContain('ValidateMediaOpenRequestAliases');
    expect(playback).toContain('A supplied media open value must not be empty');
    expect(playback.indexOf('ValidateMediaOpenRequestAliases')).toBeLessThan(
      playback.indexOf('CallVoidFunction(Player, TEXT("Close"))'),
    );
  });

  it('revalidates existing media assets before use', () => {
    const assets = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaAssets.cpp',
    );
    const playback = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaPlaybackOpen.cpp',
    );
    const playlists = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaPlaylist.cpp',
    );
    const sources = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaSources.cpp',
    );

    expect(playback).toContain('ValidateMediaSourcePolicy(');
    expect(playback).not.toContain('OpenUrl');
    expect(playback).not.toContain('CanPlayUrl');
    expect(playlists).toContain('ValidateMediaSourcePolicy(');
    expect(sources).toContain('ValidateMediaSourcePolicy(');
    expect(assets.indexOf('ValidateMediaSourcePolicy(')).toBeLessThan(
      assets.indexOf('OpenSource'),
    );
  });

  it('lets pause supersede pending asynchronous play', () => {
    const asyncPlayback = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaPlaybackAsync.cpp',
    );
    const controls = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaPlaybackControls.cpp',
    );

    expect(asyncPlayback).toContain('Player->Close()');
    expect(asyncPlayback).toContain(
      'bool InvalidatePendingMediaPlayback(UObject *PlayerObject, ' +
        'bool bClosePlayer)',
    );
    expect(asyncPlayback).toContain('return bHadPendingPlayback;');
    expect(controls).toMatch(
      /const bool bCancelledPendingOpen =\s*InvalidatePendingMediaPlayback\(Player, true\);/,
    );
    expect(controls).toContain('if (bCancelledPendingOpen)');
    expect(controls).toContain(
      'TEXT("Pending media open cancelled")',
    );
    expect(controls).toContain(
      'Result->SetStringField(TEXT("openStatus"), TEXT("cancelled"));',
    );
    expect(controls).toContain(
      'Result->SetBoolField(TEXT("isPlaying"), false);',
    );
    expect(controls).toContain(
      'Result->SetBoolField(TEXT("isPaused"), false);',
    );

    const cancellationBranch = controls.indexOf('if (bCancelledPendingOpen)');
    const pauseCall = controls.indexOf(
      'CallBoolFunction(Player, TEXT("Pause"))',
    );
    expect(cancellationBranch).toBeGreaterThan(-1);
    expect(pauseCall).toBeGreaterThan(cancellationBranch);
  });

  it('defers the first playback poll so a newer ready-media request can supersede it', () => {
    const asyncPlayback = privateSource(
      'Domains',
      'Sequence',
      'Media',
      'McpAutomationBridge_SequenceMediaPlaybackAsync.cpp',
    );

    expect(asyncPlayback).not.toContain(
      'AdvanceMediaPlayback(State, 0.0f)',
    );
    expect(asyncPlayback.indexOf('ActivePlaybackGenerations.Add')).toBeLessThan(
      asyncPlayback.indexOf('FTSTicker::GetCoreTicker().AddTicker'),
    );
  });
});
