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

export const RENDER_SCREEN_RECORDS: readonly CapabilityRecordSource[] = [
  R('set_exposure_compensation', 'Set exposure compensation value.', { compensationValue: P.compensationValue }),
  R('set_exposure_min_max', 'Set exposure min and max brightness.', { minBrightness: P.minBrightness, maxBrightness: P.maxBrightness }),
  R('configure_ssao', 'Configure screen-space ambient occlusion.', { settings: P.settings }),
  R('configure_gtao', 'Configure ground-truth ambient occlusion.', { settings: P.settings }),
  R('configure_vignette', 'Configure vignette settings.', { amount: P.amount }),
  R('configure_chromatic_aberration', 'Configure chromatic aberration.', { amount: P.amount }),
  R('configure_grain', 'Configure film grain settings.', { amount: P.amount }),
  R('configure_screen_percentage', 'Set screen percentage for rendering.', { screenPercentage: P.screenPercentage }),
  R('create_scene_capture_2d', 'Create a 2D scene capture actor.', { name: P.name, location: P.location, rotation: P.rotation }),
  R('create_scene_capture_cube', 'Create a cube scene capture actor.', { name: P.name, location: P.location }),
  R('configure_capture_source', 'Configure scene capture source.', { captureSource: { type: 'string', description: 'Capture source string.' } }),
  R('assign_render_target', 'Assign a render target to a scene capture.', { actorName: P.actorName, renderTargetPath: P.renderTargetPath }),
  R('capture_scene', 'Trigger a scene capture on an actor.', { actorName: P.actorName }),
];
