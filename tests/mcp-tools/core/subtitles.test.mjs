#!/usr/bin/env node
// manage_audio set_subtitles: a Sound Wave's subtitle cues, shown in the game while it plays. One of the editor's own
// notification sounds, copied into a folder of this run's own.
// (tests/mcp-tools/core/subtitles.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = `/Game/MCPTest/Subtitles_${ts}`;
const WAVE = `${FOLDER}/SW_Subtitled`;

const testCases = [
  { scenario: 'Setup: a copy of a notification sound', toolName: 'manage_asset', arguments: { action: 'duplicate', sourcePath: '/Engine/EditorSounds/Notifications/CompileSuccess', destinationPath: WAVE }, expected: 'success' },
  { scenario: 'UPDATE: two cues, given out of order, come back in time order', toolName: 'manage_audio', arguments: { action: 'set_subtitles', soundPath: WAVE, subtitles: [{ text: 'It worked.', time: 0.2 }, { text: 'Compiling done.', time: 0 }], save: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.subtitles', length: 2, label: 'both cues kept' },
    { path: 'structuredContent.result.subtitles.0.text', equals: 'Compiling done.', label: 'the earlier cue first' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
    { path: 'structuredContent.result.saved', equals: true, label: 'the wave saved' },
  ] },
  { scenario: 'UPDATE: an empty list takes them off, unsaved', toolName: 'manage_audio', arguments: { action: 'set_subtitles', soundPath: WAVE, subtitles: [], save: false }, expected: 'success', assertions: [
    { path: 'structuredContent.result.subtitles', length: 0, label: 'no cues left' },
  ] },
  { scenario: 'ERROR: a cue past the end of the sound changes nothing', toolName: 'manage_audio', arguments: { action: 'set_subtitles', soundPath: WAVE, subtitles: [{ text: 'Too late.', time: 999 }] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: a cue with no words', toolName: 'manage_audio', arguments: { action: 'set_subtitles', soundPath: WAVE, subtitles: [{ text: '   ', time: 0 }] }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ERROR: engine content is never changed', toolName: 'manage_audio', arguments: { action: 'set_subtitles', soundPath: '/Engine/EditorSounds/Notifications/CompileSuccess', subtitles: [] }, expected: 'error|ENGINE_CONTENT' },
  { scenario: 'ERROR: a Sound Cue is not a wave', toolName: 'manage_audio', arguments: { action: 'set_subtitles', soundPath: '/Engine/EditorSounds/Notifications/CompileSuccess_Cue', subtitles: [] }, expected: 'error|ASSET_NOT_FOUND' },
  { scenario: 'Cleanup: delete the copy', toolName: 'manage_asset', arguments: { action: 'delete', paths: [WAVE], force: true }, expected: 'success|not found' },
];

runToolTests('subtitles', testCases, {});
