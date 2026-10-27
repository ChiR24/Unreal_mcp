#!/usr/bin/env node
// animation_physics auto_blend_space: a locomotion blend space laid out from its clips' own motion. A skeleton of a root
// and one foot, and clips keyed to known motion: a walk (root 150 cm/s forward, +Y), a run (450), a walk back (150
// backward), an in-place walk (root still, the foot sliding back at 120 cm/s while planted) and an idle.
// (tests/mcp-tools/core/auto-blend-space.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const FOLDER = `/Game/MCPTest/AutoBlend_${ts}`;
const SKELETON = `${FOLDER}/SK_AutoBlend`;
const BS2D = `${FOLDER}/BS_Locomotion`;
const BS1D = `${FOLDER}/BS_Forward`;
const clip = (name) => `${FOLDER}/${name}`;
// A one-second clip at 30 fps whose bone moves from `from` to `to` (no bone: it stands still).
const keyed = (name, bone, from, to) => [
  { scenario: `Setup: ${name}`, toolName: 'animation_physics', arguments: { action: 'create_animation_sequence', name, path: FOLDER, skeletonPath: SKELETON }, expected: 'success' },
  { scenario: `Setup: ${name} lasts 1 s`, toolName: 'animation_physics', arguments: { action: 'set_sequence_length', assetPath: clip(name), length: 1, frameRate: 30 }, expected: 'success' },
  ...(bone ? [
    { scenario: `Setup: ${name} keys ${bone}`, toolName: 'animation_physics', arguments: { action: 'add_bone_track', assetPath: clip(name), boneName: bone }, expected: 'success|already exists' },
    { scenario: `Setup: ${name} first key`, toolName: 'animation_physics', arguments: { action: 'set_bone_key', assetPath: clip(name), boneName: bone, frame: 0, location: from }, expected: 'success' },
    { scenario: `Setup: ${name} last key`, toolName: 'animation_physics', arguments: { action: 'set_bone_key', assetPath: clip(name), boneName: bone, frame: 30, location: to, save: true }, expected: 'success' },
  ] : []),
];
const ALL = ['A_Idle', 'A_Walk', 'A_Run', 'A_WalkBack', 'A_InPlace'].map(clip);

const testCases = [
  { scenario: 'Setup: a skeleton of a root and one foot', toolName: 'animation_physics', arguments: { action: 'create_skeleton', path: SKELETON, rootBoneName: 'Root', save: true }, expected: 'success' },
  { scenario: 'Setup: its foot', toolName: 'animation_physics', arguments: { action: 'add_bone', skeletonPath: SKELETON, boneName: 'foot_l', parentBoneName: 'Root', location: [10, 0, 0], save: true }, expected: 'success' },
  ...keyed('A_Idle'),
  ...keyed('A_Walk', 'Root', { x: 0, y: 0, z: 0 }, { x: 0, y: 150, z: 0 }),
  ...keyed('A_Run', 'Root', { x: 0, y: 0, z: 0 }, { x: 0, y: 450, z: 0 }),
  ...keyed('A_WalkBack', 'Root', { x: 0, y: 0, z: 0 }, { x: 0, y: -150, z: 0 }),
  ...keyed('A_InPlace', 'foot_l', { x: 10, y: 60, z: 0 }, { x: 10, y: -60, z: 0 }),
  { scenario: 'Setup: a 2D blend space', toolName: 'animation_physics', arguments: { action: 'create_blend_space_2d', name: 'BS_Locomotion', path: FOLDER, skeletonPath: SKELETON }, expected: 'success' },
  { scenario: 'Setup: a 1D blend space', toolName: 'animation_physics', arguments: { action: 'create_blend_space_1d', name: 'BS_Forward', path: FOLDER, skeletonPath: SKELETON }, expected: 'success' },
  { scenario: 'READ: dryRun measures and lays out without writing', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: BS2D, animationPaths: ALL, dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.clips', includesObject: { animationPath: `${clip('A_Walk')}.A_Walk`, speed: 150, direction: 0, source: 'root' }, label: 'the walk reads its root' },
    { path: 'structuredContent.result.clips', includesObject: { animationPath: `${clip('A_InPlace')}.A_InPlace`, speed: 120, direction: 0, source: 'feet' }, label: 'the in-place walk reads its foot' },
    { path: 'structuredContent.result.clips', includesObject: { animationPath: `${clip('A_WalkBack')}.A_WalkBack`, positions: { length: 2 } }, label: 'the walk back takes both edges' },
    { path: 'structuredContent.result.clips', includesObject: { animationPath: `${clip('A_Idle')}.A_Idle`, idle: true, positions: { length: 3 } }, label: 'the idle takes every direction used' },
    { path: 'structuredContent.result.samples', equals: 8, label: 'eight samples planned' },
  ] },
  { scenario: 'UPDATE: the clips replace the samples, in one undo step', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: BS2D, animationPaths: ALL, axes: ['direction', 'speed'], feet: ['foot_l'], contactHeight: 5, save: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.samples', equals: 8, label: 'eight samples' },
    { path: 'structuredContent.result.axes', includesObject: { axisName: 'Speed', minValue: 0, maxValue: 450 }, label: 'speed up to the run' },
    { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one undo step' },
  ] },
  { scenario: 'UPDATE: a kept sample refuses its position', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: BS2D, animationPaths: [clip('A_Run')], keepSamples: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.clips', includesObject: { refused: 1 }, label: 'the run is already there' },
    { path: 'structuredContent.result.samples', equals: 8, label: 'nothing added' },
  ] },
  { scenario: 'UPDATE: a 1D blend space on forward speed', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: BS1D, animationPaths: ALL, axes: ['forwardSpeed'] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.axes', includesObject: { axisName: 'Forward Speed', minValue: -450, maxValue: 450 }, label: 'symmetric' },
    { path: 'structuredContent.result.clips', includesObject: { animationPath: `${clip('A_WalkBack')}.A_WalkBack`, forwardSpeed: -150 }, label: 'the walk back is negative' },
  ] },
  { scenario: 'READ: a character facing +X turns the walk to its right', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: BS2D, animationPaths: [clip('A_Walk')], forwardAxis: '+X', dryRun: true }, expected: 'success', assertions: [
    { path: 'structuredContent.result.clips', includesObject: { direction: 90, rightSpeed: 150 }, label: '+Y is right of +X' },
  ] },
  { scenario: 'ERROR: an unknown axis', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: BS2D, animationPaths: ALL, axes: ['speed', 'lean'] }, expected: 'error' },
  { scenario: 'ERROR: a foot the skeleton lacks', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: BS2D, animationPaths: ALL, feet: ['foot_r'] }, expected: 'error|BONE_NOT_FOUND' },
  { scenario: 'ERROR: a clip that does not exist', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: BS2D, animationPaths: [`${FOLDER}/A_Missing`] }, expected: 'error|ANIMATION_NOT_FOUND' },
  { scenario: 'ERROR: a blend space that does not exist', toolName: 'animation_physics', arguments: { action: 'auto_blend_space', assetPath: `${FOLDER}/BS_Missing`, animationPaths: ALL }, expected: 'error|BLENDSPACE_NOT_FOUND' },
  { scenario: 'Cleanup: the test assets', toolName: 'animation_physics', arguments: { action: 'cleanup', artifacts: [BS2D, BS1D, ...ALL, SKELETON] }, expected: 'success' },
];

runToolTests('auto-blend-space', testCases, {});
