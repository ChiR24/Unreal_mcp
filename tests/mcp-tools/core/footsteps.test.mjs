#!/usr/bin/env node
// animation_physics add_footsteps: a Play Sound notify wherever a foot is set down. A skeleton of a root and one foot,
// and a one-second clip whose foot lifts (frame 0 to 10, up 20 cm) and comes down again by frame 20, staying down to
// the loop's end: with contactHeight 5 it is down again from frame 18, and the frames it is down at the loop's start
// continue the plant the loop ends with, so the clip holds one step, at 0.6 s. An idle keeps the foot down throughout.
// (tests/mcp-tools/core/footsteps.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = `/Game/MCPTest/Footsteps_${ts}`;
const SKELETON = `${FOLDER}/SK_Steps`;
const STEP = `${FOLDER}/A_Step`;
const IDLE = `${FOLDER}/A_Idle`;
const SOUND = '/Engine/EditorSounds/Notifications/CompileSuccess';
const key = (frame, z) => ({ scenario: `Setup: the foot at frame ${frame}`, toolName: 'animation_physics', arguments: { action: 'set_bone_key', assetPath: STEP, boneName: 'foot_l', frame, location: { x: 10, y: 0, z }, save: true }, expected: 'success' });
const clip = (name) => [
  { scenario: `Setup: ${name}`, toolName: 'animation_physics', arguments: { action: 'create_animation_sequence', name, path: FOLDER, skeletonPath: SKELETON }, expected: 'success' },
  { scenario: `Setup: ${name} lasts 1 s`, toolName: 'animation_physics', arguments: { action: 'set_sequence_length', assetPath: `${FOLDER}/${name}`, length: 1, frameRate: 30 }, expected: 'success' },
];

const testCases = [
  { scenario: 'Setup: a skeleton of a root and one foot', toolName: 'animation_physics', arguments: { action: 'create_skeleton', path: SKELETON, rootBoneName: 'Root', save: true }, expected: 'success' },
  { scenario: 'Setup: its foot', toolName: 'animation_physics', arguments: { action: 'add_bone', skeletonPath: SKELETON, boneName: 'foot_l', parentBoneName: 'Root', location: [10, 0, 0], save: true }, expected: 'success' },
  ...clip('A_Step'),
  { scenario: 'Setup: the step keys the foot', toolName: 'animation_physics', arguments: { action: 'add_bone_track', assetPath: STEP, boneName: 'foot_l' }, expected: 'success|already exists' },
  key(0, 0), key(10, 20), key(20, 0), key(30, 0),
  ...clip('A_Idle'),
  { scenario: 'UPDATE: a footstep where the foot is set down, in one undo step', toolName: 'animation_physics', arguments: { action: 'add_footsteps', assetPath: STEP, soundPath: SOUND, feet: ['foot_l'], contactHeight: 5 }, expected: 'success', assertions: [
    { path: 'structuredContent.result.steps', length: 1, label: 'one step: the plant at the loop start is the one it ends with' },
    { path: 'structuredContent.result.steps.0.foot', equals: 'foot_l', label: 'the left foot' },
    { path: 'structuredContent.result.steps.0.time', approximately: 0.6, tolerance: 0.04, label: 'down again at frame 18' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'UPDATE: a run again replaces its footsteps', toolName: 'animation_physics', arguments: { action: 'add_footsteps', assetPath: STEP, soundPath: SOUND, save: false }, expected: 'success', assertions: [
    { path: 'structuredContent.result.removed', equals: 1, label: 'the earlier step went' },
    { path: 'structuredContent.result.steps', length: 1, label: 'and one came back' },
  ] },
  { scenario: 'READ: the clip holds one Play Sound notify', toolName: 'animation_physics', arguments: { action: 'get_animation_info', assetPath: STEP }, expected: 'success', assertions: [
    { path: 'structuredContent.result.notifies', includesObject: { name: 'Footstep', notifyClass: 'AnimNotify_PlaySound' }, label: 'a Footstep notify' },
    { path: 'structuredContent.result.numNotifies', equals: 1, label: 'only one' },
  ] },
  { scenario: 'UPDATE: an idle gets no footstep', toolName: 'animation_physics', arguments: { action: 'add_footsteps', assetPath: IDLE, soundPath: SOUND, feet: ['foot_l'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.steps', length: 0, label: 'the foot never lifts' },
    { path: 'structuredContent.result.saved', equals: false, label: 'nothing changed, nothing saved' },
  ] },
  { scenario: 'ERROR: a sound that does not load', toolName: 'animation_physics', arguments: { action: 'add_footsteps', assetPath: STEP, soundPath: `${FOLDER}/SW_Missing` }, expected: 'error|SOUND_NOT_FOUND' },
  { scenario: 'ERROR: a foot the skeleton lacks', toolName: 'animation_physics', arguments: { action: 'add_footsteps', assetPath: STEP, soundPath: SOUND, feet: ['foot_r'] }, expected: 'error|BONE_NOT_FOUND' },
  { scenario: 'ERROR: a clip that does not load', toolName: 'animation_physics', arguments: { action: 'add_footsteps', assetPath: `${FOLDER}/A_Missing`, soundPath: SOUND }, expected: 'error|SEQUENCE_NOT_FOUND' },
  { scenario: 'Cleanup: the test assets', toolName: 'animation_physics', arguments: { action: 'cleanup', artifacts: [STEP, IDLE, SKELETON] }, expected: 'success' },
];

runToolTests('footsteps', testCases, {});
