/**
 * Media records: create_media_player, create_media_source,
 * create_media_texture, create_media_sound_component,
 * create_media_playlist, play_media, pause_media, seek_media.
 *
 * Grounded in MEDIA_ACTIONS and native SequenceMedia* bodies.
 * Gated by MCP_HAS_MEDIA_ASSETS (MediaAssets module) + ElectraPlayer
 * plugin for file-backed playback.
 *
 * Media playback is immediate/transient - no long-running operation,
 * no cancellation semantics. create_* actions produce UE assets
 * (MediaPlayer, MediaSource, MediaTexture, MediaSoundComponent, MediaPlaylist).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { A } from './alias-props.js';
import { buildRecord, MEDIA_PLUGINS, P } from './helpers.js';

const F = 'media';
const D = 'media';

export const MEDIA_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.media.create_media_player', action: 'create_media_player', family: F, domain: D,
    summary: 'Create a MediaPlayer asset for media playback.',
    whenToUse: ['A media player asset must be created for video/audio playback.'],
    whenNotToUse: ['An existing media player should be reused.'],
    inputProps: { name: P.name, path: P.path, assetPath: P.assetPath, mediaSourcePath: P.mediaSourcePath, sourcePath: P.sourcePath, autoPlay: A.autoPlay, playOnOpen: A.playOnOpen, loop: A.loop, looping: A.looping },
    required: ['name', 'path'],
    // Native HandleCreateMediaPlayer (Media/MediaAssets.cpp:63-156) emits
    // assetType/assetPath/packageName/created/saved/classPath (+verification)
    // via BuildAssetResponse (MediaReflection.cpp:242-257) plus looping and
    // playOnOpen (:143-144); mediaSourcePath/openRequested/openStatus appear
    // only when a source was supplied. mediaPlayerPath is never emitted.
    outputProps: {
      assetType: { type: 'string', description: 'Created asset type (MediaPlayer).' },
      assetPath: { type: 'string', description: 'Created media player asset path.' },
      packageName: { type: 'string', description: 'Package name of the created asset.' },
      classPath: { type: 'string', description: 'Class path of the created asset.' },
      assetName: { type: 'string', description: 'Created asset name (verification).' },
      assetClass: { type: 'string', description: 'Created asset class (verification).' },
      created: { type: 'boolean', description: 'Whether the asset was created.' },
      saved: { type: 'boolean', description: 'Whether the asset was saved.' },
      existsAfter: { type: 'boolean', description: 'Whether the asset existed after the call.' },
      looping: { type: 'boolean', description: 'Whether the player loops.' },
      playOnOpen: { type: 'boolean', description: 'Whether the player starts on open.' },
      mediaSourcePath: { type: 'string', description: 'Resolved media source path (when a source was supplied).' },
      openRequested: { type: 'boolean', description: 'Whether opening the source was requested (when a source was supplied).' },
      openStatus: { type: 'string', description: 'Open status (pending) when a source was supplied.' },
    },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'create_media_player', name: 'MP_Cinematics', path: '/Game/Media' },
    exampleOutput: { success: true, assetType: 'MediaPlayer', assetPath: '/Game/Media/MP_Cinematics', created: true, saved: true, looping: false, playOnOpen: true },
  }),
  buildRecord({
    id: 'sequence.media.create_media_source', action: 'create_media_source', family: F, domain: D,
    summary: 'Create a MediaSource asset (FileMediaSource or PlatformMediaSource).',
    whenToUse: ['A media source asset must be created for a media file.'],
    whenNotToUse: ['An existing media source should be reused.'],
    inputProps: {
      name: P.name, path: P.path, assetPath: P.assetPath,
      sourceType: P.sourceType, filePath: P.filePath, precacheFile: P.precacheFile,
      defaultSourcePath: P.defaultSourcePath, platformSources: P.platformSources,
      mediaPath: A.mediaPath,
    },
    required: ['name', 'path'],
    outputProps: { mediaSourcePath: P.mediaSourcePath },
    outputRequired: ['mediaSourcePath'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'create_media_source', name: 'MS_Cinematics', path: '/Game/Media', sourceType: 'File', filePath: '/tmp/cinematics/source.mp4' },
    exampleOutput: { success: true, mediaSourcePath: '/Game/Media/MS_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.create_media_texture', action: 'create_media_texture', family: F, domain: D,
    summary: 'Create a MediaTexture asset linked to a MediaPlayer.',
    whenToUse: ['A media texture must be created for video rendering.'],
    whenNotToUse: ['A media texture is not needed for audio-only playback.'],
    inputProps: { name: P.name, path: P.path, mediaPlayerPath: P.mediaPlayerPath, playerPath: P.playerPath, autoClear: A.autoClear },
    required: ['name', 'path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'create_media_texture', name: 'MT_Cinematics', path: '/Game/Media', mediaPlayerPath: '/Game/Media/MP_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.create_media_sound_component', action: 'create_media_sound_component', family: F, domain: D,
    summary: 'Create a MediaSoundComponent linked to a MediaPlayer.',
    whenToUse: ['Audio from media playback must be routed through a sound component.'],
    whenNotToUse: ['Audio is not needed for the media playback.'],
    inputProps: { name: P.name, path: P.path, mediaPlayerPath: P.mediaPlayerPath, actorName: P.actorName, targetActor: A.targetActor, componentName: A.componentName, activate: A.activate },
    required: ['name', 'path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'create_media_sound_component', name: 'MSC_Cinematics', path: '/Game/Media', mediaPlayerPath: '/Game/Media/MP_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.create_media_playlist', action: 'create_media_playlist', family: F, domain: D,
    summary: 'Create a MediaPlaylist asset for sequential media source playback.',
    whenToUse: ['A playlist of multiple media sources must be created.'],
    whenNotToUse: ['Only a single media source is needed.'],
    inputProps: {
      name: P.name, path: P.path,
      sourcePaths: P.sourcePaths, filePaths: P.filePaths,
    },
    required: ['name', 'path'],
    outputProps: { playlistPath: P.playlistPath },
    outputRequired: ['playlistPath'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'create_media_playlist', name: 'MPL_Cinematics', path: '/Game/Media' },
    exampleOutput: { success: true, playlistPath: '/Game/Media/MPL_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.play_media', action: 'play_media', family: F, domain: D,
    summary: 'Open a media source on a MediaPlayer and start playback.',
    whenToUse: ['Media playback must be started on an existing MediaPlayer.'],
    whenNotToUse: ['The media player is already playing.'],
    inputProps: { playerPath: P.playerPath, mediaPlayerPath: P.mediaPlayerPath, mediaSourcePath: P.mediaSourcePath, sourcePath: P.sourcePath, playlistPath: P.playlistPath, filePath: P.filePath, mediaPath: A.mediaPath, playlistIndex: A.playlistIndex },
    required: ['playerPath'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'play_media', playerPath: '/Game/Media/MP_Cinematics', mediaSourcePath: '/Game/Media/MS_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.pause_media', action: 'pause_media', family: F, domain: D,
    summary: 'Pause media playback on a MediaPlayer.',
    whenToUse: ['Media playback must be paused.'],
    whenNotToUse: ['The media player is not playing.'],
    inputProps: { playerPath: P.playerPath },
    required: ['playerPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'pause_media', playerPath: '/Game/Media/MP_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.seek_media', action: 'seek_media', family: F, domain: D,
    summary: 'Seek to a specific time in media playback.',
    whenToUse: ['The media playhead must be moved to a specific time.'],
    whenNotToUse: ['The media player is not open.'],
    inputProps: { playerPath: P.playerPath, seekTime: { type: 'number', description: 'Seek time in seconds.' }, mediaPlayerPath: P.mediaPlayerPath, timeSeconds: { type: 'number', description: 'Seek time in seconds (<=86400).' }, seconds: { type: 'number', description: 'Seek time in seconds (alias of timeSeconds).' }, time: A.time },
    required: ['playerPath', 'seekTime'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'seek_media', playerPath: '/Game/Media/MP_Cinematics', seekTime: 5.0 },
  }),
];
