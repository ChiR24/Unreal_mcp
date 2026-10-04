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
import { int } from '../shared/schema-props.js';

const F = 'cinematic';
const D = 'cinematics';

/** SetSectionRange (Cinematics.cpp): start, then duration or end, on a row. */
const SECTION_RANGE = { startFrame: A.startFrame, durationFrames: A.durationFrames, endFrame: A.endFrame, rowIndex: A.rowIndex };

/** One wording for both tracks that read it: a fold keeps a shared parameter's first description. */
const TRACK_COMPONENT = {
  type: 'string',
  description: 'Component of the actor the track works on. skeletal_animation: the skeletal mesh that plays the clip '
    + '(default the actor\'s root when that is a skeletal mesh; it gets its own binding under the actor, because '
    + 'Sequencer otherwise plays the clip on the first skeletal mesh it finds, which on a character built from several '
    + 'meshes copies another\'s pose and shows nothing). material_parameter: the component owning the material.',
};

/** LoadSequence reads the sequence to edit from path; for shots that is the master. */
const masterPath = { type: 'string', description: 'Canonical /Game path of the master sequence that owns the shot track.' };

/** ApplyCameraSettings (Cameras.cpp:73-84): each primary field, else its nested object. */
const CAMERA_SETTINGS = {
  currentFocalLength: A.currentFocalLength, currentAperture: A.currentAperture,
  sensorWidth: { type: 'number', description: 'Sensor width in mm.' }, sensorHeight: { type: 'number', description: 'Sensor height in mm.' },
  manualFocusDistance: A.manualFocusDistance, lens: A.lens, filmback: A.filmback, focus: A.focus,
};

/** Output-only: set at Assets.cpp:184 (add_subsequence) and :242 (add_shot_track); never read as input. */
const sectionNameOutput = {
  type: 'string',
  description: 'Name of the created section, assigned by Sequencer.',
};

// `spec` keeps the required list and the example values that satisfy it in one
// place, so a track that requires an extra parameter cannot ship an example without it.
function trackRecord(id: string, action: string, summary: string, extraProps: Record<string, unknown> = {}, spec: { required: string[]; example: JsonObject; requiredOneOf?: string[]; notFor?: string } = { required: ['path'], example: {} }): CapabilityRecordSource {
  const { required, example, requiredOneOf, notFor } = spec;
  return buildRecord({
    id, action, family: F, domain: D,
    summary,
    whenToUse: [`${summary}`],
    whenNotToUse: ['The track is not needed for this sequence.', ...(notFor === undefined ? [] : [notFor])],
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
    // (:120-121), and endFrame through GetDuration when durationFrames is
    // absent. masterSequencePath and mapPath are never read; the sequence
    // is saved unconditionally (McpSafeAssetSave :122), so save is dead too.
    inputProps: { name: P.name, sequencePath: P.sequencePath, path: P.path, assetPath: P.assetPath, frameRate: P.frameRate, startFrame: P.startFrame, durationFrames: A.durationFrames, endFrame: int('Playback range end frame (exclusive); used when durationFrames is absent.') },
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
    // Native HandleAddSubsequence (Assets.cpp) loads the master through
    // LoadSequence, which reads path; the section range comes from
    // startFrame and durationFrames or endFrame, on rowIndex.
    inputProps: { path: masterPath, subsequencePath: P.subsequencePath, ...SECTION_RANGE, save: A.save },
    required: ['path', 'subsequencePath'],
    outputProps: { sectionName: sectionNameOutput },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_subsequence', path: '/Game/Cinematics/SEQ_Master', subsequencePath: '/Game/Cinematics/SEQ_Shot01' },
  }),
  buildRecord({
    id: 'sequence.cinematic.add_shot_track', action: 'add_shot_track', family: F, domain: D,
    summary: 'Add a cinematic shot track to a master sequence.',
    whenToUse: ['A shot track must be added to organize camera cuts.'],
    whenNotToUse: ['The master sequence already has a shot track.'],
    // Native HandleAddShotTrack (Assets.cpp) loads the master from path and
    // requires the shot sequence it adds as a section.
    inputProps: { path: masterPath, shotSequencePath: P.shotSequencePath, displayName: A.displayName, ...SECTION_RANGE, save: A.save },
    required: ['path', 'shotSequencePath'],
    outputProps: { sectionName: sectionNameOutput },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_shot_track', path: '/Game/Cinematics/SEQ_Master', shotSequencePath: '/Game/Cinematics/SEQ_Shot01' },
  }),
  buildRecord({
    id: 'sequence.cinematic.configure_shot_settings', action: 'configure_shot_settings', family: F, domain: D,
    summary: 'Configure shot settings (display name, range) for a cinematic shot.',
    whenToUse: ['Shot display name or frame range must be set.'],
    whenNotToUse: ['The shot does not exist on the shot track.'],
    // Native HandleConfigureShotSettings (ShotSettings.cpp) loads the master
    // from path and picks the shot by sectionIndex, then shotSequencePath,
    // then name. Only the range fields sent move the shot.
    inputProps: { path: masterPath, shotSequencePath: { type: 'string', description: 'Picks the shot whose section plays this shot sequence (alternative to sectionIndex or sectionName).' }, sectionName: { type: 'string', description: 'Shot section display name to configure (alternative to sectionIndex).' }, shotName: P.name, displayName: A.displayName, sectionIndex: A.sectionIndex, ...SECTION_RANGE, save: A.save },
    required: ['path'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_shot_settings', path: '/Game/Cinematics/SEQ_Master', sectionIndex: 0, displayName: 'Shot 01', startFrame: 0, endFrame: 120 },
  }),
  buildRecord({
    id: 'sequence.cinematic.create_cine_camera_actor', action: 'create_cine_camera_actor', family: F, domain: D,
    summary: 'Create a CineCameraActor in the level and bind it to the sequence.',
    whenToUse: ['A cinematic camera actor must be created for a shot.'],
    whenNotToUse: ['An existing camera should be used.'],
    // Native HandleCreateCineCameraActor (Cinematics/Cameras.cpp:93-156) reads
    // actorName/label (:126, passed to SpawnActorInActiveWorld which honors it)
    // and location/rotation (:122-125). cameraName/cameraActorName/save are
    // never read. ApplyCameraSettings (:73-84) reads the lens, filmback and
    // focus fields. Emits actorName/actorPath (+bindingGuid when a sequence is
    // supplied) on success, so they are declared as receipt-visible outputs.
    inputProps: { ...CAMERA_SETTINGS, path: P.path, actorName: P.actorName, label: A.label, location: { type: 'object', description: 'Camera location.', additionalProperties: false, properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }, required: ['x', 'y', 'z'] }, rotation: { type: 'object', description: 'Camera rotation as {pitch, yaw, roll} (x/y/z are accepted as aliases).', additionalProperties: false, properties: { pitch: { type: 'number' }, yaw: { type: 'number' }, roll: { type: 'number' }, x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } } } },
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
    summary: 'Configure CineCamera lens, filmback, and focus settings on a camera actor in the level.',
    whenToUse: ['Camera lens, focal length, aperture, or focus must be set.'],
    whenNotToUse: ['Default camera settings are acceptable.'],
    // Native HandleConfigureCameraSettings (Cameras.cpp) edits the level actor
    // ResolveActor finds from actorName or cameraName; it never loads a sequence.
    inputProps: { focalLength: { type: 'number', description: 'Focal length in mm.' }, aperture: { type: 'number', description: 'Aperture f-stop.' }, focusDistance: { type: 'number', description: 'Focus distance.' }, cameraName: P.actorName, actorName: P.actorName, ...CAMERA_SETTINGS },
    requiredOneOf: ['actorName', 'cameraName'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_camera_settings', actorName: 'CineCam_01', focalLength: 35, aperture: 2.8 },
  }),
  buildRecord({
    id: 'sequence.cinematic.add_camera_cut_track', action: 'add_camera_cut_track', family: F, domain: D,
    summary: 'Add a camera cut track to a cinematic sequence.',
    whenToUse: ['A camera cut track must be added for shot transitions.'],
    whenNotToUse: ['The sequence already has a camera cut track.'],
    // Native HandleAddCameraCutTrack (Cinematics/CameraTracks.cpp:44-105) reads
    // bindingGuid/bindingId (ReadBindingGuid Cinematics.cpp:112-115) or an
    // actor resolved by actorName/cameraName/actorPath/cameraActorPath
    // (ResolveActor Cinematics.cpp:117-124) plus the SetSectionRange fields
    // and save. cameraActorName is never read, so it is dropped.
    inputProps: { path: P.path, actorName: P.actorName, cameraName: P.actorName, actorPath: { type: 'string', description: 'Actor path (alias of actorName).' }, bindingGuid: A.bindingGuid, ...SECTION_RANGE, save: A.save },
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
    whenNotToUse: ['No camera shake is needed.', 'The shake is gameplay feedback during play (a hit, a landing): create a Blueprint with parentClass /Script/EngineCameras.LegacyCameraShake (blueprint.create) and call PlayerCameraManager StartCameraShake from a graph (blueprint.edit_graph).'],
    // Native HandleAddCameraShakeTrack (CameraTracks.cpp) binds the track to
    // the cameraName actor, as Sequencer does; without it the track is unbound.
    inputProps: { path: P.path, cameraShakeClass: P.cameraShakeClass, cameraShakePath: A.cameraShakePath, cameraName: { type: 'string', description: 'Level actor with a camera component that the shake track is bound to.' }, ...SECTION_RANGE, save: A.save },
    required: ['path'],
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'add_camera_shake_track', path: '/Game/Cinematics/SEQ_Master', cameraShakeClass: '/Script/EngineCameras.DefaultCameraShakeBase', cameraName: 'CineCam_01' },
  }),
  // The rig handlers (CameraRigs.cpp ConfigureRig) edit or spawn a level actor;
  // they never load a sequence or save one, so path and save are not declared.
  buildRecord({
    id: 'sequence.cinematic.configure_camera_rig_rail', action: 'configure_camera_rig_rail', family: F, domain: D,
    summary: 'Configure a camera rig rail for dolly camera movement.',
    whenToUse: ['A camera must move along a rail for dolly shots.'],
    whenNotToUse: ['No rail-based camera movement is needed.'],
    inputProps: { positionOnRail: { type: 'number', description: 'Position on the rail (0-1).' }, actorName: P.actorName, label: A.label },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_camera_rig_rail', positionOnRail: 0.5 },
  }),
  buildRecord({
    id: 'sequence.cinematic.configure_camera_rig_crane', action: 'configure_camera_rig_crane', family: F, domain: D,
    summary: 'Configure a camera rig crane for boom camera movement.',
    whenToUse: ['A camera must boom or swing on a crane arm.'],
    whenNotToUse: ['No crane-based camera movement is needed.'],
    inputProps: { cranePitch: { type: 'number', description: 'Crane pitch in degrees.' }, craneYaw: { type: 'number', description: 'Crane yaw in degrees.' }, craneArmLength: { type: 'number', description: 'Crane arm length.' }, actorName: P.actorName, label: A.label },
    effect: 'write', latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'configure_camera_rig_crane', craneArmLength: 300 },
  }),
  trackRecord('sequence.cinematic.add_fade_track', 'add_fade_track',
    'Add a fade track for cinematic fade-in/fade-out transitions.',
    { from: A.from, to: A.to, ...SECTION_RANGE },
    { required: ['path'], example: {}, notFor: 'The fade happens during play (level start, death, level change): call PlayerCameraManager StartCameraFade from a graph (blueprint.edit_graph).' }),
  trackRecord('sequence.cinematic.add_level_visibility_track', 'add_level_visibility_track',
    'Add a level visibility track to control level streaming during cinematic.',
    { levelNames: P.levelNames, visibility: A.visibility, ...SECTION_RANGE }),
  // MaterialTrack.cpp requires parameterName and value, and keys value at startFrame.
  trackRecord('sequence.cinematic.add_material_parameter_track', 'add_material_parameter_track',
    'Add a material parameter collection track to animate material parameters.',
    { materialPath: P.materialPath, componentName: TRACK_COMPONENT, materialIndex: A.materialIndex, parameterName: A.parameterName, bindingGuid: A.bindingGuid, actorName: P.actorName, value: { description: 'Key value at startFrame: a number for a scalar parameter, or an {r, g, b, a} object for a color parameter.' }, startFrame: A.startFrame },
    { required: ['path', 'parameterName', 'value'], example: { actorName: 'Cube', parameterName: 'Opacity', value: 0.5 }, requiredOneOf: ['actorName', 'bindingGuid'] }),
  // Tracks.cpp resolves the binding from bindingGuid or actorName like the other bound tracks.
  trackRecord('sequence.cinematic.add_particle_track', 'add_particle_track',
    'Add a particle track to trigger particle systems during cinematic.',
    { activate: A.activate, bindingGuid: A.bindingGuid, actorName: P.actorName, ...SECTION_RANGE },
    { required: ['path'], example: { actorName: 'NiagaraActor_1' }, requiredOneOf: ['actorName', 'bindingGuid'] }),
  trackRecord('sequence.cinematic.add_skeletal_animation_track', 'add_skeletal_animation_track',
    'Add a skeletal animation track to play animations on a skeletal mesh. A clip whose skeleton the mesh cannot play is refused with SKELETON_MISMATCH instead of silently showing the rest pose. Overlap the clips before and after it and give easeInFrames/easeOutFrames to crossfade between them instead of cutting in one frame.',
    { animationSequencePath: { type: 'string', description: 'Animation sequence asset path.' }, animationPath: A.animationPath, actorName: P.actorName, componentName: TRACK_COMPONENT, bindingGuid: A.bindingGuid, ...SECTION_RANGE,
      easeInFrames: { type: 'number', minimum: 0, description: 'Frames at the start of the section over which the clip blends in from the clip it overlaps (0 or omitted: it takes over at once).' },
      easeOutFrames: { type: 'number', minimum: 0, description: 'Frames at the end of the section over which the clip blends out into the clip that overlaps it.' } },
    { required: ['path'], example: { actorName: 'SkeletalMeshActor_1', animationSequencePath: '/Game/Animations/AS_Walk' }, requiredOneOf: ['actorName', 'bindingGuid'] }),
  trackRecord('sequence.cinematic.add_transform_track', 'add_transform_track',
    'Add a transform track to animate actor transforms during cinematic.',
    { actorName: P.actorName, bindingGuid: A.bindingGuid, ...SECTION_RANGE },
    { required: ['path'], example: { actorName: 'CineCameraActor_1' }, requiredOneOf: ['actorName', 'bindingGuid'] }),
  trackRecord('sequence.cinematic.add_event_track', 'add_event_track',
    'Add an event track to trigger events at specific frames.',
    { actorName: P.actorName, bindingGuid: A.bindingGuid, ...SECTION_RANGE }),
  trackRecord('sequence.cinematic.add_property_track', 'add_property_track',
    'Add a property track to animate a specific property on a bound actor.',
    { property: P.property, actorName: P.actorName, bindingGuid: A.bindingGuid, propertyName: A.propertyName, propertyPath: A.propertyPath, propertyType: A.propertyType, ...SECTION_RANGE },
    { required: ['path', 'property'], example: { actorName: 'CineCameraActor_1', property: 'Transform' }, requiredOneOf: ['actorName', 'bindingGuid'] }),
];
