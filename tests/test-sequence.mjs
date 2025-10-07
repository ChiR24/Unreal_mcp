#!/usr/bin/env node
/**
 * Sequence Test Suite
 * Tool: manage_sequence
 * Actions: create_sequence, add_track, set_keyframe, remove_keyframe, set_properties, render_sequence
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Create level sequence',
    toolName: 'manage_sequence',
    arguments: {
      action: 'create_sequence',
      name: 'MainSequence',
      path: '/Game/Cinematics',
      duration: 10.0
    },
    expected: 'success - sequence created'
  },
  {
    scenario: 'Add transform track',
    toolName: 'manage_sequence',
    arguments: {
      action: 'add_track',
      sequencePath: '/Game/Cinematics/MainSequence',
      actorPath: '/Game/Maps/TestLevel.TestLevel:PersistentLevel.Camera_1',
      trackType: 'Transform'
    },
    expected: 'success - transform track added'
  },
  {
    scenario: 'Add visibility track',
    toolName: 'manage_sequence',
    arguments: {
      action: 'add_track',
      sequencePath: '/Game/Cinematics/MainSequence',
      actorPath: '/Game/Maps/TestLevel.TestLevel:PersistentLevel.Light_1',
      trackType: 'Visibility'
    },
    expected: 'success - visibility track added'
  },
  {
    scenario: 'Set location keyframe',
    toolName: 'manage_sequence',
    arguments: {
      action: 'set_keyframe',
      sequencePath: '/Game/Cinematics/MainSequence',
      trackPath: 'Camera_1.Transform.Location',
      time: 0.0,
      value: { x: 0, y: 0, z: 100 }
    },
    expected: 'success - location keyframe set'
  },
  {
    scenario: 'Set rotation keyframe',
    toolName: 'manage_sequence',
    arguments: {
      action: 'set_keyframe',
      sequencePath: '/Game/Cinematics/MainSequence',
      trackPath: 'Camera_1.Transform.Rotation',
      time: 5.0,
      value: { pitch: 0, yaw: 90, roll: 0 }
    },
    expected: 'success - rotation keyframe set'
  },
  {
    scenario: 'Remove keyframe',
    toolName: 'manage_sequence',
    arguments: {
      action: 'remove_keyframe',
      sequencePath: '/Game/Cinematics/MainSequence',
      trackPath: 'Camera_1.Transform.Location',
      time: 0.0
    },
    expected: 'success - keyframe removed'
  },
  {
    scenario: 'Set sequence properties',
    toolName: 'manage_sequence',
    arguments: {
      action: 'set_properties',
      sequencePath: '/Game/Cinematics/MainSequence',
      properties: {
        playbackRange: { start: 0, end: 15.0 },
        frameRate: 30,
        looping: false
      }
    },
    expected: 'success - sequence properties set'
  },
  {
    scenario: 'Render sequence to video',
    toolName: 'manage_sequence',
    arguments: {
      action: 'render_sequence',
      sequencePath: '/Game/Cinematics/MainSequence',
      outputPath: 'C:\\Output\\Sequence.avi',
      settings: {
        resolution: { width: 1920, height: 1080 },
        frameRate: 30,
        quality: 'High'
      }
    },
    expected: 'success - sequence rendered'
  },
  {
    scenario: 'Add audio track to sequence',
    toolName: 'manage_sequence',
    arguments: {
      action: 'add_track',
      sequencePath: '/Game/Cinematics/MainSequence',
      trackType: 'Audio',
      audioPath: '/Game/Audio/Music_MainTheme'
    },
    expected: 'success - audio track added'
  },
  {
    scenario: 'Set camera FOV keyframe',
    toolName: 'manage_sequence',
    arguments: {
      action: 'set_keyframe',
      sequencePath: '/Game/Cinematics/MainSequence',
      trackPath: 'Camera_1.FieldOfView',
      time: 2.5,
      value: 90.0
    },
    expected: 'success - FOV keyframe set'
  }
];

await runToolTests('Sequence', testCases);
