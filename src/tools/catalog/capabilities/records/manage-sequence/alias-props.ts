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
import type { JsonObject } from '../../model.js';
import { bool, int, num, str, strArr } from '../shared/schema-props.js';


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
    currentFocalLength: num('Focal length in millimetres.'),
    focalLength: num('Focal length alias in millimetres.'),
    currentAperture: num('Aperture as an f-stop.'),
    aperture: num('Aperture alias as an f-stop.'),
  },
};

const filmbackSettings: JsonObject = {
  type: 'object',
  description: 'Nested filmback overrides (Cameras.cpp:77-80 nested object "filmback").',
  additionalProperties: false,
  properties: {
    sensorWidth: num('Sensor width in millimetres.'),
    sensorHeight: num('Sensor height in millimetres.'),
  },
};

const focusSettings: JsonObject = {
  type: 'object',
  description: 'Nested focus overrides (Cameras.cpp:81-83 nested object "focus").',
  additionalProperties: false,
  properties: {
    manualFocusDistance: num('Manual focus distance in centimetres.'),
    focusDistance: num('Manual focus distance alias in centimetres.'),
  },
};

export const A = {
  /** LoadSequence Cinematics.cpp:77 and MaybeSaveSequence Cinematics.cpp:194. */
  save: bool('Whether to save the sequence asset after the mutation.'),
  /** ValidateCinematicFrameRequest FrameMath.cpp:170 and GetDuration. */
  durationFrames: int('Section duration in display-rate frames.'),
  /** SetSectionRange Cinematics.cpp:105-107. */
  rowIndex: int('Sequencer row index for the created section.'),
  /** GetFrame(startFrame) in SetSectionRange and the track handlers. */
  startFrame: int('Section start in display-rate frames (default 0).'),
  /** GetDuration Frames.cpp: endFrame minus startFrame when durationFrames is absent. */
  endFrame: int('Section end in display-rate frames; used when durationFrames is absent.'),

  currentAperture: num('Aperture as an f-stop (read before aperture).'),
  currentFocalLength: num('Focal length in millimetres (read before focalLength).'),
  manualFocusDistance: num('Manual focus distance in centimetres (read before focusDistance).'),
  lens: lensSettings,
  filmback: filmbackSettings,
  focus: focusSettings,
  /** GetString(Params, "actorName", "label") Cameras.cpp:126, CameraRigs.cpp:44. */
  label: str('Actor label alias for the camera or rig actor (alias of actorName).'),

  /** HandleAddFadeTrack Tracks.cpp:96. */
  from: num('Fade start opacity value.'),
  /** HandleAddFadeTrack Tracks.cpp:97. */
  to: num('Fade end opacity value.'),
  /** HandleAddLevelVisibilityTrack Tracks.cpp:140. */
  visibility: str('Level visibility state: Visible or Hidden.'),
  /** HandleAddParticleTrack Tracks.cpp:189; MediaComponents.cpp:122. */
  activate: bool('Whether the key activates (true) or deactivates (false).'),
  /** HandleAddShotTrack Assets.cpp:233; ShotSettings.cpp:56,65. */
  displayName: str('Shot display name (alias of shotName).'),
  /** HandleConfigureShotSettings ShotSettings.cpp:51. */
  sectionIndex: int('Index of the shot section to configure.'),
  /** HandleAddCameraShakeTrack CameraTracks.cpp:19. */
  cameraShakePath: str('Camera shake asset path.'),
  /** HandleAddSkeletalAnimationTrack BindingTracks.cpp:55. */
  animationPath: str('Animation sequence asset path (alias of animationSequencePath).'),
  /** HandleAddMaterialParameterTrack MaterialTrack.cpp:186. */
  componentName: str('Name of the component owning the target material.'),
  /** HandleAddMaterialParameterTrack MaterialTrack.cpp:137. */
  materialIndex: int('Material slot index on the component.'),
  /** HandleAddMaterialParameterTrack MaterialTrack.cpp:129. */
  parameterName: str('Material parameter name to animate.'),
  /** HandleAddPropertyTrack PropertyTrack.cpp:86. */
  propertyName: str('Property name to animate (alias of property).'),
  /** HandleAddPropertyTrack PropertyTrack.cpp:93. */
  propertyPath: str('Nested property path to animate.'),
  /** HandleAddPropertyTrack PropertyTrack.cpp:112. */
  propertyType: str('Property value type hint (alias of type).'),
  /** ReadBindingGuid Cinematics.cpp:113. */
  bindingGuid: str('Sequencer binding GUID (alias of bindingId).'),

  /** GetBoolAny MediaAssets.cpp:69. */
  autoPlay: bool('Whether the media player plays automatically on open.'),
  /** GetBoolAny MediaAssets.cpp:69 (alias of autoPlay). */
  playOnOpen: bool('Whether the media player plays on open (alias of autoPlay).'),
  /** GetBoolAny MediaAssets.cpp:67. */
  loop: bool('Whether media playback loops.'),
  /** GetBoolAny MediaAssets.cpp:67 (alias of loop). */
  looping: bool('Whether media playback loops (alias of loop).'),
  /** GetBoolAny MediaAssets.cpp:188. */
  autoClear: bool('Whether the media texture clears when playback stops.'),
  /** GetStringAny MediaSources.cpp:31. */
  mediaPath: str('Media file path (alias of filePath).'),
  /** GetStringAny MediaComponents.cpp:49. */
  targetActor: str('Actor receiving the media sound component (alias of actorName).'),
  /** GetNumberAny MediaPlaybackOpen.cpp:68. */
  playlistIndex: int('Zero-based index into the media playlist.'),
  /** Seek alias list MediaPlaybackControls.cpp:69. */
  time: num('Seek time in seconds (alias of timeSeconds).'),

  /** Alias list TakeRecorderTracks.cpp:157. */
  properties: strArr('Property name.', 'Recorded property names (alias of tracks).'),
  /** Alias list TakeRecorderTracks.cpp:157. */
  trackNames: strArr('Track name.', 'Recorded track names (alias of tracks).'),
  /** Alias list TakeRecorderTracks.cpp:179, SourcePreparation.cpp:62. */
  actors: strArr('Actor name.', 'Actor names to record (alias of actorNames).'),
  /** ReadBool TakeRecorderTracks.cpp:161. */
  enabled: bool('Whether the matched recorded tracks are enabled.'),
  /** ReadBool TakeRecorderTracks.cpp:162. */
  disableOthers: bool('Whether non-matching recorded tracks are disabled.'),
  /** HasField TakeRecorderTracks.cpp:109,171; SourceReflection.cpp:32,118. */
  recordParentHierarchy: bool('Whether the source records its parent hierarchy.'),

  /** GetCreationString JobCreation.cpp:75; GetString State.cpp:90. */
  jobName: str('Render job name (alias of renderJobName).'),
  /** GetString State.cpp:88. */
  renderJobId: str('Render job identifier (alias of jobId).'),
  /** TryGetStringEither MovieRenderSettings.cpp:126-127. */
  method: str('Anti-aliasing method (alias of antiAliasingMethod).'),
};
