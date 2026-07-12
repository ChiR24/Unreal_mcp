#!/usr/bin/env node
/**
 * T5 lane L3 — manage_sequence Media action coverage.
 *
 * Genuine static/integration cases for the eight Media actions:
 *   create_media_player, create_media_source, create_media_texture,
 *   create_media_sound_component, create_media_playlist,
 *   play_media, pause_media, seek_media
 * and their audited optional parameters. Asset dependencies are created with
 * unique, timestamped names and torn down in reverse-dependency order.
 *
 * Expectations follow the grammar: narrow alternatives only (no broad masks).
 * These cases are captured by the static parameter audit and, against a live
 * editor with the Electra Player plugin and a provisioned sample clip, exercise
 * the Media framework end to end.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/Media';
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1); // 'Game/MCPTest/Media'
const ts = Date.now();

// Media asset identities (created under TEST_FOLDER).
const PLAYER_NAME = `MP_Test_${ts}`;
const PLAYER_PATH = `${TEST_FOLDER}/${PLAYER_NAME}`;
const SRC_FILE_NAME = `MS_File_${ts}`;
const SRC_FILE_PATH = `${TEST_FOLDER}/${SRC_FILE_NAME}`;
const SRC_PLAT_NAME = `MS_Platform_${ts}`;
const SRC_PLAT_PATH = `${TEST_FOLDER}/${SRC_PLAT_NAME}`;
const TEX_NAME = `MT_Test_${ts}`;
const TEX_PATH = `${TEST_FOLDER}/${TEX_NAME}`;
const PLAYLIST_NAME = `MPL_Test_${ts}`;
const PLAYLIST_PATH = `${TEST_FOLDER}/${PLAYLIST_NAME}`;
const ACTOR_MEDIA = `MediaActor_${ts}`;

// Representative on-disk media clip for file-backed sources/playback.
const MEDIA_FILE = 'Content/Movies/SampleMedia.mp4';

const testCases = [
  // === SETUP ===
  {
    scenario: 'Setup: create media test folder',
    toolName: 'manage_asset',
    arguments: { action: 'create_folder', path: TEST_FOLDER },
    expected: 'success|already exists'
  },
  {
    scenario: 'Setup: spawn media host actor',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: '/Engine/BasicShapes/Cube',
      actorName: ACTOR_MEDIA,
      location: { x: 0, y: 0, z: 100 }
    },
    expected: 'success|already exists'
  },

  // === CREATE_MEDIA_PLAYER (optional: autoPlay, loop, looping, playOnOpen) ===
  {
    scenario: 'MEDIA: create_media_player',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_player',
      name: PLAYER_NAME,
      path: TEST_FOLDER_ALIAS,
      autoPlay: false,
      loop: true,
      looping: false,
      playOnOpen: false
    },
    expected: 'success|already exists'
  },

  // === CREATE_MEDIA_SOURCE — file (optional: sourceType, mediaPath, precacheFile) ===
  {
    scenario: 'MEDIA: create_media_source file',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_source',
      name: SRC_FILE_NAME,
      path: TEST_FOLDER_ALIAS,
      sourceType: 'file',
      mediaPath: MEDIA_FILE,
      precacheFile: false
    },
    expected: 'success|already exists'
  },
  // === CREATE_MEDIA_SOURCE — stream (validation: network URLs disabled) ===
  {
    scenario: 'MEDIA: create_media_source stream rejected',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_source',
      name: `MS_Stream_${ts}`,
      path: TEST_FOLDER_ALIAS,
      sourceType: 'stream',
      streamUrl: 'http://example.invalid/stream'
    },
    expected: 'error|MEDIA_URL_NOT_ALLOWED'
  },
  // === CREATE_MEDIA_SOURCE — platform (optional: sourcePath, defaultSourcePath, platformSources) ===
  {
    scenario: 'MEDIA: create_media_source platform',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_source',
      name: SRC_PLAT_NAME,
      path: TEST_FOLDER_ALIAS,
      sourceType: 'platform',
      defaultSourcePath: SRC_FILE_PATH,
      sourcePath: SRC_FILE_PATH,
      platformSources: { Windows: SRC_FILE_PATH }
    },
    expected: 'success|already exists'
  },

  // === CREATE_MEDIA_TEXTURE (optional: mediaPlayerPath, playerPath, autoClear) ===
  {
    scenario: 'MEDIA: create_media_texture',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_texture',
      name: TEX_NAME,
      path: TEST_FOLDER_ALIAS,
      mediaPlayerPath: PLAYER_PATH,
      playerPath: PLAYER_PATH,
      autoClear: true
    },
    expected: 'success|already exists'
  },

  // === CREATE_MEDIA_SOUND_COMPONENT (optional: actorName, targetActor, mediaPlayerPath, componentName, activate) ===
  {
    scenario: 'MEDIA: create_media_sound_component',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_sound_component',
      actorName: ACTOR_MEDIA,
      targetActor: ACTOR_MEDIA,
      mediaPlayerPath: PLAYER_PATH,
      componentName: 'MediaSound',
      activate: true
    },
    expected: 'success|already exists'
  },

  // === CREATE_MEDIA_PLAYLIST — sources (optional: sourcePaths) ===
  {
    scenario: 'MEDIA: create_media_playlist sources',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_playlist',
      name: PLAYLIST_NAME,
      path: TEST_FOLDER_ALIAS,
      sourcePaths: [SRC_FILE_PATH]
    },
    expected: 'success|already exists'
  },
  // === CREATE_MEDIA_PLAYLIST — url (validation: network URLs disabled) ===
  {
    scenario: 'MEDIA: create_media_playlist url rejected',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_playlist',
      name: `MPL_Url_${ts}`,
      path: TEST_FOLDER_ALIAS,
      urls: ['http://example.invalid/playlist']
    },
    expected: 'error|MEDIA_URL_NOT_ALLOWED'
  },
  // === CREATE_MEDIA_PLAYLIST — filePaths (validation: missing file) ===
  {
    scenario: 'MEDIA: create_media_playlist missing file',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_media_playlist',
      name: `MPL_File_${ts}`,
      path: TEST_FOLDER_ALIAS,
      filePaths: ['Content/Movies/DoesNotExist.mp4']
    },
    expected: 'error|MEDIA_FILE_NOT_FOUND'
  },

  // === PLAY_MEDIA — open source (optional: mediaPlayerPath, mediaSourcePath) ===
  {
    scenario: 'MEDIA: play_media source',
    toolName: 'manage_sequence',
    arguments: {
      action: 'play_media',
      mediaPlayerPath: PLAYER_PATH,
      mediaSourcePath: SRC_FILE_PATH
    },
    expected: 'success'
  },
  // === PLAY_MEDIA — open playlist index (optional: playlistPath, playlistIndex) ===
  {
    scenario: 'MEDIA: play_media playlist index',
    toolName: 'manage_sequence',
    arguments: {
      action: 'play_media',
      mediaPlayerPath: PLAYER_PATH,
      playlistPath: PLAYLIST_PATH,
      playlistIndex: 0
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.playlistIndex', equals: 0 }
    ]
  },
  // === PLAY_MEDIA — file path (validation: missing file) ===
  {
    scenario: 'MEDIA: play_media missing file',
    toolName: 'manage_sequence',
    arguments: {
      action: 'play_media',
      mediaPlayerPath: PLAYER_PATH,
      filePath: 'Content/Movies/DoesNotExist.mp4'
    },
    expected: 'error|MEDIA_FILE_NOT_FOUND'
  },
  // === PLAY_MEDIA — url (validation: network URLs disabled) ===
  {
    scenario: 'MEDIA: play_media url rejected',
    toolName: 'manage_sequence',
    arguments: {
      action: 'play_media',
      mediaPlayerPath: PLAYER_PATH,
      url: 'http://example.invalid/clip'
    },
    expected: 'error|MEDIA_URL_NOT_ALLOWED'
  },
  // === PLAY_MEDIA — player only ===
  {
    scenario: 'MEDIA: play_media player only',
    toolName: 'manage_sequence',
    arguments: {
      action: 'play_media',
      mediaPlayerPath: PLAYER_PATH
    },
    expected: 'success'
  },

  // === PAUSE_MEDIA (optional: mediaPlayerPath) ===
  {
    scenario: 'MEDIA: pause_media',
    toolName: 'manage_sequence',
    arguments: {
      action: 'pause_media',
      mediaPlayerPath: PLAYER_PATH
    },
    expected: 'success'
  },

  // === SEEK_MEDIA (optional: mediaPlayerPath, timeSeconds, seconds, time) ===
  {
    scenario: 'MEDIA: seek_media',
    toolName: 'manage_sequence',
    arguments: {
      action: 'seek_media',
      mediaPlayerPath: PLAYER_PATH,
      timeSeconds: 5,
      seconds: 5,
      time: 5
    },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.timeSeconds', equals: 5 }
    ]
  },

  // === CLEANUP (reverse dependency order) ===
  {
    scenario: 'Cleanup: delete media texture',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: TEX_PATH, force: true },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete media host actor (removes sound component)',
    toolName: 'control_actor',
    arguments: { action: 'delete', actorName: ACTOR_MEDIA },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete media playlist',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: PLAYLIST_PATH, force: true },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete platform media source',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: SRC_PLAT_PATH, force: true },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete file media source',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: SRC_FILE_PATH, force: true },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete media player',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: PLAYER_PATH, force: true },
    expected: 'success|not found'
  },
  {
    scenario: 'Cleanup: delete media test folder',
    toolName: 'manage_asset',
    arguments: { action: 'delete', path: TEST_FOLDER, force: true },
    expected: 'success|not found'
  }
];

runToolTests('cinematics-media', testCases);
