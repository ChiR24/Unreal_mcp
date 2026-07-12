import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

import {
  privateSource,
} from './sequence_contract_test_utils.js';
import { manageSequenceToolDefinition } from '../../../src/tools/definitions/utility/manage-sequence-tool.js';

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

describe('manage_sequence dead-schema-drift guard', () => {
  // Dead-schema-drift guard: the fields below are not consumed by any Media or
  // RecordReplay handler (speculative aliases only) and must stay out of the
  // contract. clearSources/additionalOptions are genuinely RecordReplay-owned
  // (configure_take_sources / configure_demo_settings) and must stay declared.
  const DEAD_FIELDS = ['mediaPlaylistPath', 'mediaTexturePath', 'targetActorName'];
  const RECORDREPLAY_FIELDS = ['clearSources', 'additionalOptions'];
  const MEDIA_ACTIONS = [
    'create_media_player',
    'create_media_source',
    'create_media_texture',
    'create_media_sound_component',
    'create_media_playlist',
    'play_media',
    'pause_media',
    'seek_media',
  ];

  it('omits verified-dead fields from the canonical TS schema', () => {
    const inputSchema = manageSequenceToolDefinition.inputSchema as {
      properties?: Record<string, unknown>;
    };
    const properties = inputSchema.properties ?? {};
    for (const field of DEAD_FIELDS) {
      expect(properties[field], `canonical schema must not declare dead field ${field}`).toBeUndefined();
    }
  });

  it('retains RecordReplay-owned fields and does not claim them as Media', () => {
    const inputSchema = manageSequenceToolDefinition.inputSchema as {
      properties?: Record<string, unknown>;
    };
    const properties = inputSchema.properties ?? {};
    for (const field of RECORDREPLAY_FIELDS) {
      expect(properties[field], `canonical schema must retain RecordReplay field ${field}`).toBeDefined();
    }
  });

  it('keeps all eight Media actions in the canonical action enum', () => {
    const inputSchema = manageSequenceToolDefinition.inputSchema as {
      properties?: { action?: { enum?: string[] } };
    };
    const actions = inputSchema.properties?.action?.enum ?? [];
    for (const action of MEDIA_ACTIONS) {
      expect(actions, `canonical schema must declare Media action ${action}`).toContain(action);
    }
  });

  it('omits dead fields from the generated gateway manifest and native schema mirror', () => {
    const manifest = JSON.parse(
      readFileSync(resolve(process.cwd(), 'src/gateway/gateway-manifest.generated.json'), 'utf8'),
    ) as { tools: Array<{ name: string; parameterNames: string[] }> };
    const manageSequence = manifest.tools.find((tool) => tool.name === 'manage_sequence');
    const parameterNames = manageSequence?.parameterNames ?? [];
    for (const field of DEAD_FIELDS) {
      expect(
        parameterNames,
        `generated gateway manifest must not declare dead field ${field}`,
      ).not.toContain(field);
    }

    const nativeSchemaSource = readFileSync(
      resolve(
        process.cwd(),
        'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/Utility/McpTool_ManageSequenceSchemaFields.cpp',
      ),
      'utf8',
    );
    for (const field of DEAD_FIELDS) {
      expect(
        nativeSchemaSource,
        `native schema mirror must not declare dead field ${field}`,
      ).not.toContain(`.String(TEXT("${field}")`);
    }
  });
});
