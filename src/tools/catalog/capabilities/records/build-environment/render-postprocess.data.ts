/**
 * Render shard 2: post-process, exposure, screen effects (22 actions).
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'render';
const WU = ['Post-process, exposure, or screen effect settings must be configured.'];
const ID = 'build_environment.';
const R = (action: string, summary: string, inputProps: JsonObject, required: string[] = []): CapabilityRecordSource => buildRecord({
  id: ID + action, action, family: F, summary, whenToUse: WU,
  whenNotToUse: ['Default post-process settings are sufficient.'],
  // Every post-process action resolves a PostProcessVolume; actorName lets the
  // caller pick one explicitly when the resolver reports AMBIGUOUS. Each also
  // applies the volume's infiniteUnbound and blendWeight when they are passed.
  inputProps: { actorName: P.actorName, blendWeight: P.blendWeight, infiniteUnbound: P.infiniteUnbound, ...inputProps }, required,
  effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
  exampleInput: { action },
});

export const RENDER_POSTPROCESS_RECORDS: readonly CapabilityRecordSource[] = [
  R('configure_ssr_settings', 'Configure screen-space reflections.', { settings: P.ppSettings }),
  R('configure_lumen_reflection_settings', 'Configure Lumen reflection settings.', { settings: P.ppSettings }),
  R('configure_pp_blend', 'Configure a post-process volume\'s blend weight, unbound flag and whether it is enabled.', { enabled: { ...P.enabled, description: 'Whether the volume is enabled.' } }),
  R('set_pp_white_balance', 'Set post-process white balance.', { settings: P.ppSettings }),
  R('set_pp_color_grading', 'Set post-process color grading.', { settings: P.ppSettings }),
  R('set_pp_lut', 'Set post-process LUT texture.', { lutPath: P.lutPath }),
  R('configure_tonemapper', 'Configure tonemapper settings.', { settings: P.ppSettings }),
  R('set_tonemapper_type', 'Set tonemapper type.', { method: P.method }),
  R('configure_bloom', 'Configure bloom settings.', { settings: P.ppSettings, amount: { ...P.amount, description: 'Bloom intensity.' }, threshold: { ...P.threshold, description: 'Bloom threshold (-1 = everything blooms).' } }),
  R('set_bloom_intensity', 'Set bloom intensity.', { amount: P.amount }),
  R('set_bloom_threshold', 'Set bloom threshold.', { threshold: P.threshold }),
  R('configure_lens_flare', 'Configure lens flare settings.', { settings: P.ppSettings, enabled: { ...P.enabled, description: 'false zeroes the lens flare intensity.' } }),
  R('configure_dof', 'Configure depth of field.', { settings: P.ppSettings }),
  R('set_dof_method', 'Set depth of field method.', { method: P.method }),
  R('set_focal_distance', 'Set focal distance for DOF.', { distance: P.distance }),
  R('set_aperture', 'Set camera aperture (f-stop).', { aperture: P.aperture }),
  R('configure_bokeh', 'Configure bokeh settings.', { settings: P.ppSettings }),
  R('configure_motion_blur', 'Configure motion blur settings.', { settings: P.ppSettings }),
  R('set_motion_blur_amount', 'Set motion blur amount.', { amount: P.amount }),
  R('set_motion_blur_max', 'Set motion blur maximum.', { amount: P.amount }),
  R('configure_exposure', 'Configure exposure settings.', {
    settings: P.ppSettings, compensationValue: P.compensationValue, minBrightness: P.minBrightness, maxBrightness: P.maxBrightness,
    method: { ...P.method, description: 'Auto-exposure method: Manual, Histogram or Basic.' },
  }),
  R('set_exposure_method', 'Set exposure method.', { method: P.method }),
];
