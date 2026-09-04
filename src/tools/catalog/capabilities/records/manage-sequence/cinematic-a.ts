/**
 * Cinematic records part 1: create_master_sequence, add_subsequence,
 * add_shot_track, configure_shot_settings, create_cine_camera_actor,
 * configure_camera_settings, add_camera_cut_track, add_camera_shake_track,
 * configure_camera_rig_rail.
 *
 * Grounded in CINEMATICS_ACTIONS and native SequenceCinematics* bodies.
 * Gated by LevelSequenceEditor plugin.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { A } from './alias-props.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'cinematic';
const D = 'cinematics';
const NR = 'Distinct cinematic track or camera operation with unique target.';

/** Output-only: set at Assets.cpp:184 (add_subsequence) and :242 (add_shot_track); never read as input. */
const sectionNameOutput = {
  type: 'string',
  description: 'Name of the created section, assigned by Sequencer.',
};

export const CINEMATIC_RECORDS_A: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.cinematic.create_master_sequence', action: 'create_master_sequence', family: F, domain: D,
    summary: 'Create a master cinematic sequence with sub-sequence and shot track structure.',
    whenToUse: ['A new cinematic with shots must be scaffolded.'],
    whenNotToUse: ['A flat sequence without shots is sufficient.'],
    // Native HandleCreateMasterSequence (Cinematics/Assets.cpp:45-141) resolves
    // the target from sequencePath/assetPath (ResolveAssetTarget :22-40) plus
    // name/path(folder); reads frameRate (:101), startFrame/durationFrames
    // (:120-121). masterSequencePath and mapPath are never read; the sequence
    // is saved unconditionally (McpSafeAssetSave :122), so save is dead too.
    inputProps: { action: P.action, name: P.name, sequencePath: P.sequencePath, path: P.path, assetPath: P.assetPath, frameRate: P.frameRate, startFrame: P.startFrame, durationFrames: A.durationFrames },
    required: ['action', 'name', 'sequencePath'],
    outputProps: { sequencePath: P.sequencePath },
    outputRequired: ['sequencePath'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'create_master_sequence', name: 'SEQ_Master', sequencePath: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, sequencePath: '/Game/Cinematics/SEQ_Master' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.cinematic.add_subsequence', action: 'add_subsequence', family: F, domain: D,
    summary: 'Add a sub-sequence to a master sequence shot track.',
    whenToUse: ['A sub-sequence must be nested inside the master sequence.'],
    whenNotToUse: ['The sequence should remain flat.'],
    inputProps: { action: P.action, masterSequencePath: P.masterSequencePath, subsequencePath: P.subsequencePath, rowIndex: A.rowIndex, durationFrames: A.durationFrames, save: A.save },
    required: ['action', 'masterSequencePath', 'subsequencePath'],
    outputProps: { sectionName: sectionNameOutput },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_subsequence', masterSequencePath: '/Game/Cinematics/SEQ_Master', subsequencePath: '/Game/Cinematics/SEQ_Shot01' },
    exampleOutput: { success: true, message: 'Subsequence added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.cinematic.add_shot_track', action: 'add_shot_track', family: F, domain: D,
    summary: 'Add a cinematic shot track to a master sequence.',
    whenToUse: ['A shot track must be added to organize camera cuts.'],
    whenNotToUse: ['The master sequence already has a shot track.'],
    inputProps: { action: P.action, masterSequencePath: P.masterSequencePath, shotSequencePath: P.shotSequencePath, displayName: A.displayName, durationFrames: A.durationFrames, rowIndex: A.rowIndex, save: A.save },
    required: ['action', 'masterSequencePath'],
    outputProps: { sectionName: sectionNameOutput },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_shot_track', masterSequencePath: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, message: 'Shot track added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.cinematic.configure_shot_settings', action: 'configure_shot_settings', family: F, domain: D,
    summary: 'Configure shot settings (display name, range) for a cinematic shot.',
    whenToUse: ['Shot display name or frame range must be set.'],
    whenNotToUse: ['The shot does not exist on the shot track.'],
    inputProps: { action: P.action, shotSequencePath: P.shotSequencePath, masterSequencePath: { type: 'string', description: 'Master sequence that owns the shot track; sectionIndex or sectionName picks the shot (alias of shotSequencePath).' }, sectionName: { type: 'string', description: 'Shot section display name to configure (alternative to sectionIndex).' }, shotName: P.name, start: P.start, end: P.end, displayName: A.displayName, sectionIndex: A.sectionIndex, durationFrames: A.durationFrames, save: A.save },
    required: ['action'],
    requiredOneOf: ['shotSequencePath', 'masterSequencePath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_shot_settings', shotSequencePath: '/Game/Cinematics/SEQ_Shot01', shotName: 'Shot 01', start: 0, end: 120 },
    exampleOutput: { success: true, message: 'Shot settings configured' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.cinematic.create_cine_camera_actor', action: 'create_cine_camera_actor', family: F, domain: D,
    summary: 'Create a CineCameraActor in the level and bind it to the sequence.',
    whenToUse: ['A cinematic camera actor must be created for a shot.'],
    whenNotToUse: ['An existing camera should be used.'],
    // Native HandleCreateCineCameraActor (Cinematics/Cameras.cpp:93-156) reads
    // actorName/label (:126, passed to SpawnActorInActiveWorld which honors it)
    // and location/rotation (:122-125). cameraName/cameraActorName/save are
    // never read. Emits actorName/actorPath (+bindingGuid when a sequence is
    // supplied) on success, so they are declared as receipt-visible outputs.
    inputProps: { action: P.action, path: P.path, actorName: P.actorName, label: A.label, location: { type: 'object', description: 'Camera location.', additionalProperties: false, properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, required: ['x', 'y', 'z'] }, rotation: { type: 'object', description: 'Camera rotation as {pitch, yaw, roll} (x/y/z are accepted as aliases).', additionalProperties: false, properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }, x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } } },
    required: ['action', 'path'],
    outputProps: {
      actorName: { type: 'string', description: 'Label of the created camera actor.' },
      actorPath: { type: 'string', description: 'Path to the created camera actor.' },
      bindingGuid: { type: 'string', description: 'Sequencer binding GUID (present when a sequence path was supplied).' },
      appliedProperties: { type: 'array', items: { type: 'string' }, description: 'Camera properties applied to the created actor.' },
    },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'create_cine_camera_actor', path: '/Game/Cinematics/SEQ_Master', actorName: 'CineCam_01' },
    exampleOutput: { success: true, actorName: 'CineCam_01', actorPath: '/Game/Level/SUB_01.CineCam_01' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.cinematic.configure_camera_settings', action: 'configure_camera_settings', family: F, domain: D,
    summary: 'Configure CineCamera lens, filmback, and focus settings on a sequence binding.',
    whenToUse: ['Camera lens, focal length, aperture, or focus must be set.'],
    whenNotToUse: ['Default camera settings are acceptable.'],
    inputProps: { action: P.action, path: P.path, cameraActorName: P.actorName, focalLength: { type: 'number', description: 'Focal length in mm.' }, aperture: { type: 'number', description: 'Aperture f-stop.' }, focusDistance: { type: 'number', description: 'Focus distance.' }, sensorWidth: { type: 'number', description: 'Sensor width in mm.' }, sensorHeight: { type: 'number', description: 'Sensor height in mm.' }, cameraName: P.actorName, actorName: P.actorName, currentFocalLength: A.currentFocalLength, currentAperture: A.currentAperture, manualFocusDistance: A.manualFocusDistance, lens: A.lens, filmback: A.filmback, focus: A.focus },
    required: ['action', 'path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_camera_settings', path: '/Game/Cinematics/SEQ_Master', cameraActorName: 'CineCam_01', focalLength: 35, aperture: 2.8 },
    exampleOutput: { success: true, message: 'Camera settings configured' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.cinematic.add_camera_cut_track', action: 'add_camera_cut_track', family: F, domain: D,
    summary: 'Add a camera cut track to a cinematic sequence.',
    whenToUse: ['A camera cut track must be added for shot transitions.'],
    whenNotToUse: ['The sequence already has a camera cut track.'],
    // Native HandleAddCameraCutTrack (Cinematics/CameraTracks.cpp:44-105) reads
    // bindingGuid/bindingId (ReadBindingGuid Cinematics.cpp:112-115) or an
    // actor resolved by actorName/cameraName/actorPath/cameraActorPath
    // (ResolveActor Cinematics.cpp:117-124) plus rowIndex/durationFrames/save.
    // cameraActorName is never read, so it is dropped.
    inputProps: { action: P.action, path: P.path, actorName: P.actorName, cameraName: P.actorName, actorPath: { type: 'string', description: 'Actor path (alias of actorName).' }, bindingGuid: A.bindingGuid, startFrame: P.startFrame, rowIndex: A.rowIndex, durationFrames: A.durationFrames, save: A.save },
    required: ['action', 'path'],
    outputProps: { bindingGuid: { type: 'string', description: 'Binding GUID of the targeted camera or created cut.' } },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_camera_cut_track', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, bindingGuid: 'ABC-123' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.cinematic.add_camera_shake_track', action: 'add_camera_shake_track', family: F, domain: D,
    summary: 'Add a camera shake track to a cinematic sequence.',
    whenToUse: ['Camera shake must be animated along the sequence.'],
    whenNotToUse: ['No camera shake is needed.'],
    inputProps: { action: P.action, path: P.path, cameraShakeClass: P.cameraShakeClass, cameraShakePath: A.cameraShakePath, cameraName: P.actorName, save: A.save },
    required: ['action', 'path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_camera_shake_track', path: '/Game/Cinematics/SEQ_Master', cameraShakeClass: '/Script/EngineCameras.DefaultCameraShakeBase' },
    exampleOutput: { success: true, message: 'Camera shake track added' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildRecord({
    id: 'sequence.cinematic.configure_camera_rig_rail', action: 'configure_camera_rig_rail', family: F, domain: D,
    summary: 'Configure a camera rig rail for dolly camera movement.',
    whenToUse: ['A camera must move along a rail for dolly shots.'],
    whenNotToUse: ['No rail-based camera movement is needed.'],
    inputProps: { action: P.action, path: P.path, positionOnRail: { type: 'number', description: 'Position on the rail (0-1).' }, actorName: P.actorName, label: A.label, save: A.save },
    required: ['action', 'path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_camera_rig_rail', path: '/Game/Cinematics/SEQ_Master', positionOnRail: 0.5 },
    exampleOutput: { success: true, message: 'Camera rig rail configured' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
