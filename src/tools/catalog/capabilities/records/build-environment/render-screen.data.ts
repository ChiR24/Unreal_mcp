/**
 * Render shard 3: remaining exposure, AO, screen, scene capture (13 actions).
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'render';
const WU = ['Exposure, ambient occlusion, screen, or capture settings must be configured.'];
const ID = 'build_environment.';
const R = (action: string, summary: string, inputProps: JsonObject, required: string[] = []): CapabilityRecordSource => buildRecord({
  id: ID + action, action, family: F, summary, whenToUse: WU,
  whenNotToUse: ['Default settings are sufficient.'],
  // actorName targets the PostProcessVolume (exposure/AO/screen effects) or the
  // scene capture actor explicitly; without it the handlers resolve the sole
  // candidate in the level.
  inputProps: { actorName: P.actorName, ...inputProps }, required,
  effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
  exampleInput: { action },
});
// The post-process variants also apply the volume's infiniteUnbound and blendWeight when passed.
const BLEND = { blendWeight: P.blendWeight, infiniteUnbound: P.infiniteUnbound };

export const RENDER_SCREEN_RECORDS: readonly CapabilityRecordSource[] = [
  R('set_exposure_compensation', 'Set AutoExposureBias to compensationValue, in stops: +1 is twice as bright, -1 half; it applies with every exposure method.', { compensationValue: { type: 'number', description: 'Exposure bias in stops.' }, ...BLEND }, ['compensationValue']),
  R('set_exposure_min_max', 'Set the auto exposure range, AutoExposureMinBrightness and AutoExposureMaxBrightness (-10 or more): in EV100 when the project extends the default luminance range, as UE 5 projects do, in luminance otherwise. Equal values fix the exposure. A bound left out keeps its value; send at least one.', { minBrightness: { type: 'number', description: 'Lowest auto exposure.' }, maxBrightness: { type: 'number', description: 'Highest auto exposure.' }, ...BLEND }),
  R('configure_ssao', 'Set screen-space ambient occlusion: amount (AmbientOcclusionIntensity, 0-1, 0.5 by default) written after settings, which takes other fields by engine name (AmbientOcclusionRadius in cm, AmbientOcclusionPower, AmbientOcclusionQuality 0-100). With Lumen on, its own occlusion replaces this. Send at least one.', { settings: P.ppSettings, amount: { type: 'number', description: 'Ambient occlusion intensity, 0 to 1; written after settings.' }, ...BLEND }),
  R('configure_gtao', 'Set ambient occlusion fields from settings, by engine field name (AmbientOcclusionQuality 0-100, AmbientOcclusionRadius, AmbientOcclusionIntensity); ground truth AO itself is chosen by r.AmbientOcclusion.Method, which this does not change.', { settings: P.ppSettings, ...BLEND }, ['settings']),
  R('configure_vignette', 'Set VignetteIntensity to amount: how much the screen edges darken, 0 (off) to 1 on the editor slider, 0.4 by default.', { amount: { type: 'number', description: 'Vignette intensity; 0 is off.' }, ...BLEND }, ['amount']),
  R('configure_chromatic_aberration', 'Set chromatic aberration: amount (SceneFringeIntensity, 0 by default, 5 at the top of the slider) written after settings, which takes other fields by engine name (ChromaticAberrationStartOffset 0-1). Send at least one.', { settings: P.ppSettings, amount: { type: 'number', description: 'Color fringe intensity; written after settings.' }, ...BLEND }),
  R('configure_grain', 'Set film grain: amount (FilmGrainIntensity, 0 by default, 1 at the top of the slider) written after settings, which takes other grain fields by engine name (FilmGrainIntensityShadows, FilmGrainTexelSize, FilmGrainTexture). Send at least one.', { settings: P.ppSettings, amount: { type: 'number', description: 'Film grain intensity; written after settings.' }, ...BLEND }),
  buildRecord({
    id: `${ID}configure_screen_percentage`, action: 'configure_screen_percentage', family: F,
    summary: 'Set screen percentage for rendering (the r.ScreenPercentage console variable; no volume involved).',
    whenToUse: WU, whenNotToUse: ['Default settings are sufficient.'],
    inputProps: { screenPercentage: P.screenPercentage },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'configure_screen_percentage', screenPercentage: 75 },
  }),
  R('create_scene_capture_2d', 'Create a 2D scene capture actor.', { name: P.name, location: P.location, rotation: P.rotation }),
  R('create_scene_capture_cube', 'Create a cube scene capture actor.', { name: P.name, location: P.location, rotation: P.rotation }),
  R('configure_capture_source', 'Configure scene capture source.', { captureSource: { type: 'string', description: 'What the capture renders: FinalColorLDR, SceneColorHDR, SceneDepth, BaseColor, Normal or an SCS_ value. Required.' } }, ['captureSource']),
  R('assign_render_target', 'Assign a render target to a scene capture.', { actorName: P.actorName, renderTargetPath: P.renderTargetPath }),
  R('capture_scene', 'Trigger a scene capture on an actor.', { actorName: P.actorName }),
];
