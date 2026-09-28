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
  R('set_exposure_compensation', 'Set exposure compensation value.', { compensationValue: P.compensationValue, ...BLEND }),
  R('set_exposure_min_max', 'Set exposure min and max brightness; a bound left out keeps its value.', { minBrightness: P.minBrightness, maxBrightness: P.maxBrightness, ...BLEND }),
  R('configure_ssao', 'Configure screen-space ambient occlusion.', { settings: P.settings, amount: { ...P.amount, description: 'Ambient occlusion intensity; written after settings.' }, ...BLEND }),
  R('configure_gtao', 'Configure ground-truth ambient occlusion.', { settings: P.settings, ...BLEND }),
  R('configure_vignette', 'Configure vignette settings.', { amount: P.amount, ...BLEND }),
  R('configure_chromatic_aberration', 'Configure chromatic aberration.', { settings: P.settings, amount: { ...P.amount, description: 'Scene fringe intensity; written after settings.' }, ...BLEND }),
  R('configure_grain', 'Configure film grain settings.', { settings: P.settings, amount: { ...P.amount, description: 'Film grain intensity; written after settings.' }, ...BLEND }),
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
