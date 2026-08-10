/**
 * Render shard 2: post-process, exposure, screen effects (22 actions).
 *
 * Grounded in consolidated-handler-registration.ts: RENDER_ACTIONS dispatch
 * through executeAutomationRequest(tools, 'manage_render', { subAction: action }).
 * Native handlers call MarkPackageDirty() (deferred persistence).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'render';
const WU = ['Post-process, exposure, or screen effect settings must be configured.'];
const ID = 'build_environment.';
const R = (action: string, summary: string, inputProps: Record<string, unknown>, required: string[] = ['action']): CapabilityRecordSource => buildRecord({
  id: ID + action, action, family: F, summary, whenToUse: WU,
  whenNotToUse: ['Default post-process settings are sufficient.'],
  inputProps: { action: P.action, ...inputProps }, required,
  effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
  exampleInput: { action }, exampleOutput: { success: true, message: summary },
});

export const RENDER_POSTPROCESS_RECORDS: readonly CapabilityRecordSource[] = [
  R('configure_ssr_settings', 'Configure screen-space reflections.', { settings: P.settings }),
  R('configure_lumen_reflection_settings', 'Configure Lumen reflection settings.', { settings: P.settings }),
  R('configure_pp_blend', 'Configure post-process blend weight.', { blendWeight: P.blendWeight, infiniteUnbound: P.infiniteUnbound }),
  R('set_pp_white_balance', 'Set post-process white balance.', { settings: P.settings }),
  R('set_pp_color_grading', 'Set post-process color grading.', { settings: P.settings }),
  R('set_pp_lut', 'Set post-process LUT texture.', { lutPath: P.lutPath }),
  R('configure_tonemapper', 'Configure tonemapper settings.', { settings: P.settings }),
  R('set_tonemapper_type', 'Set tonemapper type.', { method: P.method }),
  R('configure_bloom', 'Configure bloom settings.', { settings: P.settings }),
  R('set_bloom_intensity', 'Set bloom intensity.', { amount: P.amount }),
  R('set_bloom_threshold', 'Set bloom threshold.', { threshold: P.threshold }),
  R('configure_lens_flare', 'Configure lens flare settings.', { settings: P.settings }),
  R('configure_dof', 'Configure depth of field.', { settings: P.settings }),
  R('set_dof_method', 'Set depth of field method.', { method: P.method }),
  R('set_focal_distance', 'Set focal distance for DOF.', { distance: P.distance }),
  R('set_aperture', 'Set camera aperture (f-stop).', { aperture: P.aperture }),
  R('configure_bokeh', 'Configure bokeh settings.', { settings: P.settings }),
  R('configure_motion_blur', 'Configure motion blur settings.', { settings: P.settings }),
  R('set_motion_blur_amount', 'Set motion blur amount.', { amount: P.amount }),
  R('set_motion_blur_max', 'Set motion blur maximum.', { amount: P.amount }),
  R('configure_exposure', 'Configure exposure settings.', { settings: P.settings, compensationValue: P.compensationValue }),
  R('set_exposure_method', 'Set exposure method.', { method: P.method }),
];
