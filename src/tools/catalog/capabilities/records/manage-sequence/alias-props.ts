/**
 * Evidence-backed alias and secondary input properties for manage_sequence.
 *
 * Every entry is declared ONLY because a bridge handler genuinely reads the key
 * as an INPUT, and each carries the source citation that proves the read.
 * Output-only fields (for example `sectionName`, which handlers emit via
 * `OutResult->SetStringField`) are deliberately absent: they belong to output
 * schemas, not inputs.
 *
 * Paths are relative to
 * plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Sequence/.
 */
import type { JsonObject } from '../../index.js';

const strProp = (desc: string): JsonObject => ({ type: 'string', description: desc });
const intProp = (desc: string): JsonObject => ({ type: 'integer', description: desc });
const numProp = (desc: string): JsonObject => ({ type: 'number', description: desc });
const boolProp = (desc: string): JsonObject => ({ type: 'boolean', description: desc });
const strArrProp = (itemDesc: string, desc: string): JsonObject => ({
  type: 'array',
  items: strProp(itemDesc),
  description: desc,
});

/**
 * ApplyNumberAliases(Primary, Alias, NestedObject, PropertyPath) in
 * Cinematics/McpAutomationBridge_SequenceCinematicsCameras.cpp:49-88 makes
 * `lens`, `filmback`, and `focus` OBJECTS whose members mirror the top-level
 * primary/alias pair -- they are not scalars.
 */
const lensSettings: JsonObject = {
  type: 'object',
  description: 'Nested lens overrides (Cameras.cpp:71-76 nested object "lens").',
  additionalProperties: false,
  properties: {
    currentFocalLength: numProp('Focal length in millimetres.'),
    focalLength: numProp('Focal length alias in millimetres.'),
    currentAperture: numProp('Aperture as an f-stop.'),
    aperture: numProp('Aperture alias as an f-stop.'),
  },
};

const filmbackSettings: JsonObject = {
  type: 'object',
  description: 'Nested filmback overrides (Cameras.cpp:77-80 nested object "filmback").',
  additionalProperties: false,
  properties: {
    sensorWidth: numProp('Sensor width in millimetres.'),
    sensorHeight: numProp('Sensor height in millimetres.'),
  },
};

const focusSettings: JsonObject = {
  type: 'object',
  description: 'Nested focus overrides (Cameras.cpp:81-83 nested object "focus").',
  additionalProperties: false,
  properties: {
    manualFocusDistance: numProp('Manual focus distance in centimetres.'),
    focusDistance: numProp('Manual focus distance alias in centimetres.'),
  },
};

export const A = {
  /** LoadSequence Cinematics.cpp:77 and MaybeSaveSequence Cinematics.cpp:194. */
  save: boolProp('Whether to save the sequence asset after the mutation.'),
  /** ValidateCinematicFrameRequest FrameMath.cpp:170 and GetDuration. */
  durationFrames: intProp('Section duration in display-rate frames.'),
  /** SetSectionRange Cinematics.cpp:105-107. */
  rowIndex: intProp('Sequencer row index for the created section.'),

  currentAperture: numProp('Aperture as an f-stop (alias of aperture).'),
  currentFocalLength: numProp('Focal length in millimetres (alias of focalLength).'),
  manualFocusDistance: numProp('Manual focus distance in centimetres (alias of focusDistance).'),
  lens: lensSettings,
  filmback: filmbackSettings,
  focus: focusSettings,
  /** GetString(Params, "actorName", "label") Cameras.cpp:126, CameraRigs.cpp:44. */
  label: strProp('Actor label alias for the camera or rig actor (alias of actorName).'),

  /** HandleAddFadeTrack Tracks.cpp:96. */
  from: numProp('Fade start opacity value.'),
  /** HandleAddFadeTrack Tracks.cpp:97. */
  to: numProp('Fade end opacity value.'),
  /** HandleAddLevelVisibilityTrack Tracks.cpp:140. */
  visibility: strProp('Level visibility state: Visible or Hidden.'),
  /** HandleAddParticleTrack Tracks.cpp:189; MediaComponents.cpp:122. */
  activate: boolProp('Whether the key activates (true) or deactivates (false).'),
  /** HandleAddShotTrack Assets.cpp:233; ShotSettings.cpp:56,65. */
  displayName: strProp('Shot display name (alias of shotName).'),
  /** HandleConfigureShotSettings ShotSettings.cpp:51. */
  sectionIndex: intProp('Index of the shot section to configure.'),
  /** HandleAddCameraShakeTrack CameraTracks.cpp:19. */
  cameraShakePath: strProp('Camera shake asset path.'),
  /** HandleAddSkeletalAnimationTrack BindingTracks.cpp:55. */
  animationPath: strProp('Animation sequence asset path (alias of animationSequencePath).'),
  /** HandleAddMaterialParameterTrack MaterialTrack.cpp:186. */
  componentName: strProp('Name of the component owning the target material.'),
  /** HandleAddMaterialParameterTrack MaterialTrack.cpp:137. */
  materialIndex: intProp('Material slot index on the component.'),
  /** HandleAddMaterialParameterTrack MaterialTrack.cpp:129. */
  parameterName: strProp('Material parameter name to animate.'),
  /** HandleAddPropertyTrack PropertyTrack.cpp:86. */
  propertyName: strProp('Property name to animate (alias of property).'),
  /** HandleAddPropertyTrack PropertyTrack.cpp:93. */
  propertyPath: strProp('Nested property path to animate.'),
  /** HandleAddPropertyTrack PropertyTrack.cpp:112. */
  propertyType: strProp('Property value type hint (alias of type).'),
  /** ReadBindingGuid Cinematics.cpp:113. */
  bindingGuid: strProp('Sequencer binding GUID (alias of bindingId).'),

  /** GetBoolAny MediaAssets.cpp:69. */
  autoPlay: boolProp('Whether the media player plays automatically on open.'),
  /** GetBoolAny MediaAssets.cpp:69 (alias of autoPlay). */
  playOnOpen: boolProp('Whether the media player plays on open (alias of autoPlay).'),
  /** GetBoolAny MediaAssets.cpp:67. */
  loop: boolProp('Whether media playback loops.'),
  /** GetBoolAny MediaAssets.cpp:67 (alias of loop). */
  looping: boolProp('Whether media playback loops (alias of loop).'),
  /** GetBoolAny MediaAssets.cpp:188. */
  autoClear: boolProp('Whether the media texture clears when playback stops.'),
  /** GetStringAny MediaSources.cpp:31. */
  mediaPath: strProp('Media file path (alias of filePath).'),
  /** GetStringAny MediaComponents.cpp:49. */
  targetActor: strProp('Actor receiving the media sound component (alias of actorName).'),
  /** GetNumberAny MediaPlaybackOpen.cpp:68. */
  playlistIndex: intProp('Zero-based index into the media playlist.'),
  /** Seek alias list MediaPlaybackControls.cpp:69. */
  time: numProp('Seek time in seconds (alias of timeSeconds).'),

  /** Alias list TakeRecorderTracks.cpp:157. */
  properties: strArrProp('Property name.', 'Recorded property names (alias of tracks).'),
  /** Alias list TakeRecorderTracks.cpp:157. */
  trackNames: strArrProp('Track name.', 'Recorded track names (alias of tracks).'),
  /** Alias list TakeRecorderTracks.cpp:179, SourcePreparation.cpp:62. */
  actors: strArrProp('Actor name.', 'Actor names to record (alias of actorNames).'),
  /** ReadBool TakeRecorderTracks.cpp:161. */
  enabled: boolProp('Whether the matched recorded tracks are enabled.'),
  /** ReadBool TakeRecorderTracks.cpp:162. */
  disableOthers: boolProp('Whether non-matching recorded tracks are disabled.'),
  /** HasField TakeRecorderTracks.cpp:109,171; SourceReflection.cpp:32,118. */
  recordParentHierarchy: boolProp('Whether the source records its parent hierarchy.'),

  /** GetCreationString JobCreation.cpp:75; GetString State.cpp:90. */
  jobName: strProp('Render job name (alias of renderJobName).'),
  /** GetString State.cpp:88. */
  renderJobId: strProp('Render job identifier (alias of jobId).'),
  /** TryGetStringEither MovieRenderSettings.cpp:126-127. */
  method: strProp('Anti-aliasing method (alias of antiAliasingMethod).'),
};
