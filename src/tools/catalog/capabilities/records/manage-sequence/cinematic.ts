/**
 * Cinematic records: master sequences, shots, cine cameras and rigs, and the
 * cut/shake/fade/visibility/material/particle/animation/transform/event/property
 * tracks. Grounded in CINEMATICS_ACTIONS and native SequenceCinematics* bodies.
 * Gated by LevelSequenceEditor plugin.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { A } from './alias-props.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';
import type { JsonObject } from '../../model.js';

const F = 'cinematic';
const D = 'cinematics';

/** Output-only: set at Assets.cpp:184 (add_subsequence) and :242 (add_shot_track); never read as input. */
const sectionNameOutput = {
  type: 'string',
  description: 'Name of the created section, assigned by Sequencer.',
};

// `spec` keeps the required list and the example values that satisfy it in one
// place, so a track that requires an extra parameter cannot ship an example without it.
function trackRecord(id: string, action: string, summary: string, extraProps: Record<string, unknown> = {}, spec: { required: string[]; example: JsonObject; requiredOneOf?: string[] } = { required: ['path'], example: {} }): CapabilityRecordSource {
  const { required, example, requiredOneOf } = spec;
  return buildRecord({
    id, action, family: F, domain: D,
    summary,
    whenToUse: [`${summary}`],
    whenNotToUse: ['The track is not needed for this sequence.'],
    inputProps: { path: P.path, save: A.save, ...extraProps },
    required,
    ...(requiredOneOf === undefined ? {} : { requiredOneOf }),
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action, path: '/Game/Cinematics/SEQ_Master', ...example },
  });
}

export const CINEMATIC_RECORDS: readonly CapabilityRecordSource[] = [
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
    inputProps: { name: P.name, sequencePath: P.sequencePath, path: P.path, assetPath: P.assetPath, frameRate: P.frameRate, startFrame: P.startFrame, durationFrames: A.durationFrames },
    required: ['name', 'sequencePath'],
    outputProps: { sequencePath: P.sequencePath },
    outputRequired: ['sequencePath'],
    effect: 'write', latency: 'interactive', resources: 'medium', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'create_master_sequence', name: 'SEQ_Master', sequencePath: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, sequencePath: '/Game/Cinematics/SEQ_Master' },
  }),
  buildRecord({
    id: 'sequence.cinematic.add_subsequence', action: 'add_subsequence', family: F, domain: D,
    summary: 'Add a sub-sequence to a master sequence shot track.',
    whenToUse: ['A sub-sequence must be nested inside the master sequence.'],
    whenNotToUse: ['The sequence should remain flat.'],
    inputProps: { masterSequencePath: P.masterSequencePath, subsequencePath: P.subsequencePath, rowIndex: A.rowIndex, durationFrames: A.durationFrames, save: A.save },
    required: ['masterSequencePath', 'subsequencePath'],
    outputProps: { sectionName: sectionNameOutput },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_subsequence', masterSequencePath: '/Game/Cinematics/SEQ_Master', subsequencePath: '/Game/Cinematics/SEQ_Shot01' },
  }),
  buildRecord({
    id: 'sequence.cinematic.add_shot_track', action: 'add_shot_track', family: F, domain: D,
    summary: 'Add a cinematic shot track to a master sequence.',
    whenToUse: ['A shot track must be added to organize camera cuts.'],
    whenNotToUse: ['The master sequence already has a shot track.'],
    inputProps: { masterSequencePath: P.masterSequencePath, shotSequencePath: P.shotSequencePath, displayName: A.displayName, durationFrames: A.durationFrames, rowIndex: A.rowIndex, save: A.save },
    required: ['masterSequencePath'],
    outputProps: { sectionName: sectionNameOutput },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_shot_track', masterSequencePath: '/Game/Cinematics/SEQ_Master' },
  }),
  buildRecord({
    id: 'sequence.cinematic.configure_shot_settings', action: 'configure_shot_settings', family: F, domain: D,
    summary: 'Configure shot settings (display name, range) for a cinematic shot.',
    whenToUse: ['Shot display name or frame range must be set.'],
    whenNotToUse: ['The shot does not exist on the shot track.'],
    inputProps: { shotSequencePath: P.shotSequencePath, masterSequencePath: { type: 'string', description: 'Master sequence that owns the shot track; sectionIndex or sectionName picks the shot (alias of shotSequencePath).' }, sectionName: { type: 'string', description: 'Shot section display name to configure (alternative to sectionIndex).' }, shotName: P.name, start: P.start, end: P.end, displayName: A.displayName, sectionIndex: A.sectionIndex, durationFrames: A.durationFrames, save: A.save },
    requiredOneOf: ['shotSequencePath', 'masterSequencePath'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_shot_settings', shotSequencePath: '/Game/Cinematics/SEQ_Shot01', shotName: 'Shot 01', start: 0, end: 120 },
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
    inputProps: { path: P.path, actorName: P.actorName, label: A.label, location: { type: 'object', description: 'Camera location.', additionalProperties: false, properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, required: ['x', 'y', 'z'] }, rotation: { type: 'object', description: 'Camera rotation as {pitch, yaw, roll} (x/y/z are accepted as aliases).', additionalProperties: false, properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }, x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } } },
    required: ['path'],
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
  }),
  buildRecord({
    id: 'sequence.cinematic.configure_camera_settings', action: 'configure_camera_settings', family: F, domain: D,
    summary: 'Configure CineCamera lens, filmback, and focus settings on a sequence binding.',
    whenToUse: ['Camera lens, focal length, aperture, or focus must be set.'],
    whenNotToUse: ['Default camera settings are acceptable.'],
    inputProps: { path: P.path, cameraActorName: P.actorName, focalLength: { type: 'number', description: 'Focal length in mm.' }, aperture: { type: 'number', description: 'Aperture f-stop.' }, focusDistance: { type: 'number', description: 'Focus distance.' }, sensorWidth: { type: 'number', description: 'Sensor width in mm.' }, sensorHeight: { type: 'number', description: 'Sensor height in mm.' }, cameraName: P.actorName, actorName: P.actorName, currentFocalLength: A.currentFocalLength, currentAperture: A.currentAperture, manualFocusDistance: A.manualFocusDistance, lens: A.lens, filmback: A.filmback, focus: A.focus },
    required: ['path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_camera_settings', path: '/Game/Cinematics/SEQ_Master', cameraActorName: 'CineCam_01', focalLength: 35, aperture: 2.8 },
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
    inputProps: { path: P.path, actorName: P.actorName, cameraName: P.actorName, actorPath: { type: 'string', description: 'Actor path (alias of actorName).' }, bindingGuid: A.bindingGuid, startFrame: P.startFrame, rowIndex: A.rowIndex, durationFrames: A.durationFrames, save: A.save },
    required: ['path'],
    outputProps: { bindingGuid: { type: 'string', description: 'Binding GUID of the targeted camera or created cut.' } },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_camera_cut_track', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, bindingGuid: 'ABC-123' },
  }),
  buildRecord({
    id: 'sequence.cinematic.add_camera_shake_track', action: 'add_camera_shake_track', family: F, domain: D,
    summary: 'Add a camera shake track to a cinematic sequence.',
    whenToUse: ['Camera shake must be animated along the sequence.'],
    whenNotToUse: ['No camera shake is needed.'],
    inputProps: { path: P.path, cameraShakeClass: P.cameraShakeClass, cameraShakePath: A.cameraShakePath, cameraName: P.actorName, save: A.save },
    required: ['path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_camera_shake_track', path: '/Game/Cinematics/SEQ_Master', cameraShakeClass: '/Script/EngineCameras.DefaultCameraShakeBase' },
  }),
  buildRecord({
    id: 'sequence.cinematic.configure_camera_rig_rail', action: 'configure_camera_rig_rail', family: F, domain: D,
    summary: 'Configure a camera rig rail for dolly camera movement.',
    whenToUse: ['A camera must move along a rail for dolly shots.'],
    whenNotToUse: ['No rail-based camera movement is needed.'],
    inputProps: { path: P.path, positionOnRail: { type: 'number', description: 'Position on the rail (0-1).' }, actorName: P.actorName, label: A.label, save: A.save },
    required: ['path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_camera_rig_rail', path: '/Game/Cinematics/SEQ_Master', positionOnRail: 0.5 },
  }),
  trackRecord('sequence.cinematic.configure_camera_rig_crane', 'configure_camera_rig_crane',
    'Configure a camera rig crane for boom camera movement.',
    { cranePitch: { type: 'number', description: 'Crane pitch in degrees.' }, craneYaw: { type: 'number', description: 'Crane yaw in degrees.' }, craneArmLength: { type: 'number', description: 'Crane arm length.' }, actorName: P.actorName, label: A.label }),
  trackRecord('sequence.cinematic.add_fade_track', 'add_fade_track',
    'Add a fade track for cinematic fade-in/fade-out transitions.',
    { from: A.from, to: A.to, durationFrames: A.durationFrames, rowIndex: A.rowIndex }),
  trackRecord('sequence.cinematic.add_level_visibility_track', 'add_level_visibility_track',
    'Add a level visibility track to control level streaming during cinematic.',
    { levelNames: P.levelNames, visibility: A.visibility, durationFrames: A.durationFrames, rowIndex: A.rowIndex }),
  trackRecord('sequence.cinematic.add_material_parameter_track', 'add_material_parameter_track',
    'Add a material parameter collection track to animate material parameters.',
    { materialPath: P.materialPath, componentName: A.componentName, materialIndex: A.materialIndex, parameterName: A.parameterName, bindingGuid: A.bindingGuid, actorName: P.actorName }),
  trackRecord('sequence.cinematic.add_particle_track', 'add_particle_track',
    'Add a particle track to trigger particle systems during cinematic.',
    { activate: A.activate, durationFrames: A.durationFrames, rowIndex: A.rowIndex, bindingGuid: A.bindingGuid }),
  trackRecord('sequence.cinematic.add_skeletal_animation_track', 'add_skeletal_animation_track',
    'Add a skeletal animation track to play animations on a skeletal mesh.',
    { animationSequencePath: { type: 'string', description: 'Animation sequence asset path.' }, skeletalMeshPath: { type: 'string', description: 'Skeletal mesh asset path.' }, animationPath: A.animationPath, actorName: P.actorName }),
  trackRecord('sequence.cinematic.add_transform_track', 'add_transform_track',
    'Add a transform track to animate actor transforms during cinematic.',
    { actorName: P.actorName, bindingGuid: A.bindingGuid },
    { required: ['path'], example: { actorName: 'CineCameraActor_1' }, requiredOneOf: ['actorName', 'bindingGuid'] }),
  trackRecord('sequence.cinematic.add_event_track', 'add_event_track',
    'Add an event track to trigger events at specific frames.',
    { actorName: P.actorName }),
  trackRecord('sequence.cinematic.add_property_track', 'add_property_track',
    'Add a property track to animate a specific property on a bound actor.',
    { property: P.property, actorName: P.actorName, bindingGuid: A.bindingGuid, propertyName: A.propertyName, propertyPath: A.propertyPath, propertyType: A.propertyType },
    { required: ['path', 'property'], example: { actorName: 'CineCameraActor_1', property: 'Transform' }, requiredOneOf: ['actorName', 'bindingGuid'] }),
];
