#!/usr/bin/env node
// manage_audio measure_loudness and normalize_loudness: Sound Waves measured as broadcast measures them (EBU R128) and set
// to play at one loudness. Two of the editor's own notification sounds, copied into a folder of this run's own.
// (tests/mcp-tools/core/loudness.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = `/Game/MCPTest/Loudness_${ts}`;
const SUCCESS = `${FOLDER}/SW_LoudSuccess`;
const FAILED = `${FOLDER}/SW_LoudFailed`;

const testCases = [
  { scenario: 'Setup: a copy of one notification sound', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/EditorSounds/Notifications/CompileSuccess', destinationPath: SUCCESS }, expected: 'success' },
  { scenario: 'Setup: a copy of another', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/EditorSounds/Notifications/CompileFailed', destinationPath: FAILED }, expected: 'success' },
  { scenario: 'READ: how loud each is', toolName: 'manage_audio', arguments: { action: 'measure_loudness', soundPaths: [SUCCESS, FAILED] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.sounds', length: 2, label: 'both measured' },
    { path: 'structuredContent.result.spreadLu', gte: 0, label: 'how far apart they play' },
  ] },
  { scenario: 'UPDATE: both set to play at -20 LUFS', toolName: 'manage_audio', arguments: { action: 'normalize_loudness', soundPaths: [SUCCESS, FAILED], targetLufs: -20, save: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.spreadLu', approximately: 0, tolerance: 0.1, label: 'they now play at one loudness' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'READ: measured again, within 1 LU', toolName: 'manage_audio', arguments: { action: 'measure_loudness', soundPaths: [SUCCESS, FAILED] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.spreadLu', approximately: 0, tolerance: 1, label: 'within one loudness unit' },
  ] },
  { scenario: 'UPDATE: one copy set to half volume', toolName: 'manage_audio', arguments: { action: 'set_sound_volume', soundPaths: [FAILED], volume: 0.5, save: false }, expected: 'success', assertions: [
    { path: 'structuredContent.result.sounds.0.volume', approximately: 0.5, tolerance: 0.001, label: 'the volume read back' },
  ] },
  { scenario: 'UPDATE: then 6 dB louder', toolName: 'manage_audio', arguments: { action: 'set_sound_volume', soundPaths: [FAILED], changeDb: 6 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.sounds.0.volume', approximately: 0.998, tolerance: 0.01, label: 'half volume plus 6 dB is about 1' },
  ] },
  { scenario: 'ERROR: volume and changeDb together', toolName: 'manage_audio', arguments: { action: 'set_sound_volume', soundPaths: [FAILED], volume: 1, changeDb: 3 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: an engine sound is never changed', toolName: 'manage_audio', arguments: { action: 'set_sound_volume', soundPaths: ['/Engine/EditorSounds/Notifications/CompileSuccess_Cue'], volume: 1 }, expected: 'error|ENGINE_CONTENT' },
  { scenario: 'ERROR: engine content is never changed', toolName: 'manage_audio', arguments: { action: 'normalize_loudness', soundPaths: ['/Engine/EditorSounds/Notifications/CompileSuccess'], targetLufs: -20 }, expected: 'error|ENGINE_CONTENT' },
  { scenario: 'ERROR: a Sound Cue is not a wave', toolName: 'manage_audio', arguments: { action: 'measure_loudness', soundPaths: ['/Engine/EditorSounds/Notifications/CompileSuccess_Cue'] }, expected: 'error|ASSET_NOT_FOUND' },
  { scenario: 'UPDATE: a copy made to loop', toolName: 'manage_audio', arguments: { action: 'set_sound_looping', soundPaths: [SUCCESS] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.sounds.0.wasLooping', equals: false, label: 'it played once' },
    { path: 'structuredContent.result.sounds.0.looping', equals: true, label: 'and loops now' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'in one undo step' },
  ] },
  { scenario: 'UPDATE: asked again, nothing changes', toolName: 'manage_audio', arguments: { action: 'set_sound_looping', soundPaths: [SUCCESS], looping: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.sounds.0.wasLooping', equals: true, label: 'it looped already' },
    { path: 'structuredContent.result.sounds.0.saved', equals: false, label: 'so nothing was saved' },
  ] },
  { scenario: 'UPDATE: back to playing once', toolName: 'manage_audio', arguments: { action: 'set_sound_looping', soundPaths: [SUCCESS], looping: false, save: false }, expected: 'success', assertions: [
    { path: 'structuredContent.result.sounds.0.looping', equals: false, label: 'it plays once again' },
  ] },
  { scenario: 'ERROR: a Sound Cue loops through its own graph', toolName: 'manage_audio', arguments: { action: 'set_sound_looping', soundPaths: ['/Engine/EditorSounds/Notifications/CompileSuccess_Cue'] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: an engine wave never loops by this', toolName: 'manage_audio', arguments: { action: 'set_sound_looping', soundPaths: ['/Engine/EditorSounds/Notifications/CompileSuccess'] }, expected: 'error|ENGINE_CONTENT' },
  { scenario: 'Cleanup: delete the copies', toolName: 'manage_asset', arguments: { action: 'delete', paths: [SUCCESS, FAILED], force: true }, expected: 'success|not found' },
];

runToolTests('loudness', testCases, {});
