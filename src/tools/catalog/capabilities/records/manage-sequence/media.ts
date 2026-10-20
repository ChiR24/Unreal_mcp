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
    summary: 'Create a MediaPlayer asset named name in path and save it: loop (default false) and playOnOpen (default true). mediaSourcePath opens that source on it at once (openStatus pending); a source that does not validate or that the player cannot play is refused with nothing created, and so is a taken name (MEDIA_ASSET_ALREADY_EXISTS). Its picture shows through a media texture and its sound through a media sound component; play_media plays it.',
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
    summary: 'Create a media source asset and save it: sourceType file (default) points at filePath, a video file under the project\'s Content or Saved folder that must exist (MEDIA_PATH_NOT_ALLOWED, MEDIA_FILE_NOT_FOUND; precacheFile loads it into memory on open); platform maps platformSources to source assets with defaultSourcePath as the fallback. Network streams are refused. A taken name is MEDIA_ASSET_ALREADY_EXISTS.',
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
    exampleInput: { action: 'create_media_source', name: 'MS_Cinematics', path: '/Game/Media', sourceType: 'file', filePath: 'Content/Movies/Intro.mp4' },
    exampleOutput: { success: true, mediaSourcePath: '/Game/Media/MS_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.create_media_texture', action: 'create_media_texture', family: F, domain: D,
    summary: 'Create a MediaTexture asset and save it, bound to the MediaPlayer at mediaPlayerPath (MEDIA_PLAYER_NOT_FOUND when it does not load): use it in a material on a mesh or in a widget to show the player\'s picture. autoClear (default true) clears it when nothing plays; a taken name is MEDIA_ASSET_ALREADY_EXISTS.',
    whenToUse: ['A media texture must be created for video rendering.'],
    whenNotToUse: ['A media texture is not needed for audio-only playback.'],
    inputProps: { name: P.name, path: P.path, mediaPlayerPath: P.mediaPlayerPath, playerPath: P.playerPath, autoClear: A.autoClear },
    required: ['name', 'path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'create_media_texture', name: 'MT_Cinematics', path: '/Game/Media', mediaPlayerPath: '/Game/Media/MP_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.create_media_sound_component', action: 'create_media_sound_component', family: F, domain: D,
    summary: 'Add a MediaSoundComponent to a placed actor (actorName) that plays the sound of the MediaPlayer at mediaPlayerPath, named componentName (default MediaSound_<player>); a name the actor already has is refused (NAME_TAKEN). activate (default true) starts it. It lives on the level actor, so no asset is created; save the level to keep it.',
    whenToUse: ['Audio from media playback must be routed through a sound component.'],
    whenNotToUse: ['Audio is not needed for the media playback.'],
    // Native HandleCreateMediaSoundComponent (MediaComponents.cpp) adds the
    // component to a level actor; it creates no asset, so there is no path.
    inputProps: { name: P.name, mediaPlayerPath: { ...P.mediaPlayerPath, description: 'Media Player the sound component plays; required, or send it as playerPath.' }, playerPath: { type: 'string', description: 'Alias of mediaPlayerPath.' }, actorName: P.actorName, targetActor: A.targetActor, componentName: A.componentName, activate: A.activate },
    required: [], requiredOneOf: ['actorName', 'targetActor'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'create_media_sound_component', actorName: 'MediaScreen', mediaPlayerPath: '/Game/Media/MP_Cinematics', componentName: 'MediaSound' },
  }),
  buildRecord({
    id: 'sequence.media.create_media_playlist', action: 'create_media_playlist', family: F, domain: D,
    summary: 'Create a MediaPlaylist asset and save it, holding the media source assets sourcePaths and then the local files filePaths (under the project\'s Content or Saved folder) in order; a source that does not load or a file that is missing or elsewhere is refused with nothing created. play_media plays it from playlistIndex.',
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
    required: [], requiredOneOf: ['playerPath', 'mediaPlayerPath'],
    effect: 'write', latency: 'instant', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'play_media', playerPath: '/Game/Media/MP_Cinematics', mediaSourcePath: '/Game/Media/MS_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.pause_media', action: 'pause_media', family: F, domain: D,
    summary: 'Pause media playback on a MediaPlayer.',
    whenToUse: ['Media playback must be paused.'],
    whenNotToUse: ['The media player is not playing.'],
    inputProps: { playerPath: P.playerPath, mediaPlayerPath: P.mediaPlayerPath },
    required: [], requiredOneOf: ['playerPath', 'mediaPlayerPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'pause_media', playerPath: '/Game/Media/MP_Cinematics' },
  }),
  buildRecord({
    id: 'sequence.media.seek_media', action: 'seek_media', family: F, domain: D,
    summary: 'Seek to a specific time in media playback.',
    whenToUse: ['The media playhead must be moved to a specific time.'],
    whenNotToUse: ['The media player is not open.'],
    inputProps: { playerPath: P.playerPath, seekTime: { type: 'number', description: 'Seek time in seconds.' }, mediaPlayerPath: P.mediaPlayerPath, timeSeconds: { type: 'number', description: 'Seek time in seconds (<=86400).' }, seconds: { type: 'number', description: 'Seek time in seconds (alias of timeSeconds).' }, time: A.time },
    required: ['seekTime'], requiredOneOf: ['playerPath', 'mediaPlayerPath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low', plugins: MEDIA_PLUGINS,
    exampleInput: { action: 'seek_media', playerPath: '/Game/Media/MP_Cinematics', seekTime: 5.0 },
  }),
];
