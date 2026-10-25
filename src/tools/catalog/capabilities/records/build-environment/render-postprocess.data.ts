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
  R('configure_ssr_settings', 'Set screen-space reflections on the volume from settings, by engine field name: ScreenSpaceReflectionIntensity and ScreenSpaceReflectionQuality (0-100; 100 and 50 by default) and ScreenSpaceReflectionMaxRoughness (0-1).', { settings: P.ppSettings }, ['settings']),
  R('configure_lumen_reflection_settings', 'Set Lumen reflections on the volume from settings, by engine field name, such as LumenReflectionQuality and LumenMaxRoughnessToTraceReflections (0-1); they show only where the project renders reflections with Lumen.', { settings: P.ppSettings }, ['settings']),
  R('configure_pp_blend', 'Set the volume\'s own fields: enabled, blendWeight (0-1, how much of the volume applies) and infiniteUnbound (true: the whole level, false: only inside its bounds). Send at least one.', { enabled: { ...P.enabled, description: 'Whether the volume applies at all.' } }),
  R('set_pp_white_balance', 'Set white balance on the volume from settings, by engine field name: WhiteTemp (Kelvin; 6500, the default, is neutral, lower is warmer) and WhiteTint (-1 to 1, green to magenta).', { settings: P.ppSettings }, ['settings']),
  R('set_pp_color_grading', 'Set color grading on the volume from settings, by engine field name: ColorSaturation, ColorContrast, ColorGamma and ColorGain as {x, y, z, w} (1 is neutral; w scales all three), ColorOffset (0 neutral), each also in a Shadows, Midtones or Highlights form (ColorSaturationShadows), and ColorGradingIntensity (0-1).', { settings: P.ppSettings }, ['settings']),
  R('set_pp_lut', 'Grade the volume with a color lookup table: lutPath is a texture in the 256x16 strip LUT format, which becomes ColorGradingLUT with its override on (ColorGradingIntensity sets how strongly it applies). A path that loads no texture is ASSET_NOT_FOUND.', { lutPath: P.lutPath }, ['lutPath']),
  R('configure_tonemapper', 'Set the filmic tonemapper\'s curve on a post-process volume from settings, by FPostProcessSettings field: FilmSlope, FilmToe, FilmShoulder, FilmBlackClip, FilmWhiteClip, ToneCurveAmount, ExpandGamut (each turns its override on).', { settings: P.ppSettings }),
  R('set_tonemapper_type', 'Choose the tonemapper look on a post-process volume: method Filmic (default: the engine\'s filmic tone curve, ToneCurveAmount and ExpandGamut 1) or Linear (the curve off, both 0); anything else is refused, nothing changed.', { method: { ...P.method, description: 'Filmic (default) or Linear.' } }),
  R('configure_bloom', 'Set bloom: amount (BloomIntensity) and threshold (BloomThreshold), written after settings, which takes any other bloom field by engine name (BloomSizeScale, BloomMethod). Send at least one.', { settings: P.ppSettings, amount: { type: 'number', description: 'Bloom intensity, 0 or more (0.675 by default; 8 is the top of the editor slider).' }, threshold: { type: 'number', description: 'Brightness a pixel needs to bloom: -1 (the default) blooms everything, 0 or more only brighter pixels.' } }),
  R('set_bloom_intensity', 'Set BloomIntensity to amount: 0 or more, 0.675 by default, 8 at the top of the editor slider; 0 turns bloom off.', { amount: { type: 'number', description: 'Bloom intensity, 0 or more.' } }, ['amount']),
  R('set_bloom_threshold', 'Set BloomThreshold to threshold, the brightness a pixel needs to bloom: -1 (the default) blooms everything, 0 or more only brighter pixels.', { threshold: { type: 'number', description: '-1 or more.' } }, ['threshold']),
  R('configure_lens_flare', 'Set lens flares from settings, by engine field name (LensFlareIntensity 0-16, 1 by default; LensFlareThreshold; LensFlareBokehSize; LensFlareTint). enabled false sets LensFlareIntensity 0; enabled true puts it back to 1 unless settings gives it.', { settings: P.ppSettings, enabled: { ...P.enabled, description: 'false sets LensFlareIntensity 0; true sets it back to 1 unless settings gives it.' } }),
  R('configure_dof', 'Set depth of field from settings, by engine field name: DepthOfFieldFocalDistance (cm to the sharp plane; at 0, the default, there is no depth of field), DepthOfFieldFstop (1-32; lower blurs more), DepthOfFieldSensorWidth (mm), DepthOfFieldDepthBlurRadius and the rest of the Lens|Depth of Field fields.', { settings: P.ppSettings }, ['settings']),
  R('set_dof_method', 'Choose the depth of field the volume renders with method: UE 5 desktop and console renderers always use the cinematic one, so this picks what mobile renders, Gaussian (bMobileHQGaussian on: the high quality Gaussian blur) or Cinematic (off: mobile\'s own). CircleDOF and BokehDOF are taken as Cinematic, GaussianDOF as Gaussian.', { method: { type: 'string', description: 'Cinematic (also CircleDOF, BokehDOF) or Gaussian (also GaussianDOF).' } }, ['method']),
  R('set_focal_distance', 'Set DepthOfFieldFocalDistance, the distance in cm from the camera to the sharp plane, 0 or more; depth of field shows only while it is above 0 (0 is the default).', { distance: { type: 'number', description: 'Focal distance in cm, 0 or more.' } }, ['distance']),
  R('set_aperture', 'Set DepthOfFieldFstop to aperture, 1 to 32 (4 by default): a lower f-stop gives a shallower, blurrier depth of field. Shows once a focal distance is set.', { aperture: { type: 'number', description: 'f-stop, 1 to 32.' } }, ['aperture']),
  R('configure_bokeh', 'Set the shape of out-of-focus highlights from settings, by engine field name: DepthOfFieldBladeCount (aperture blades), DepthOfFieldMinFstop, DepthOfFieldSensorWidth (mm), DepthOfFieldSqueezeFactor (anamorphic).', { settings: P.ppSettings }, ['settings']),
  R('configure_motion_blur', 'Set motion blur from settings, by engine field name: MotionBlurAmount (0-1, 0.5 by default; 0 turns it off), MotionBlurMax (0-100, percent of the screen width, 5 by default), MotionBlurTargetFPS, MotionBlurPerObjectSize.', { settings: P.ppSettings }, ['settings']),
  R('set_motion_blur_amount', 'Set MotionBlurAmount to amount, 0 to 1 (0.5 by default); 0 turns motion blur off.', { amount: { type: 'number', description: '0 to 1.' } }, ['amount']),
  R('set_motion_blur_max', 'Set MotionBlurMax to amount: the longest blur streak, 0 to 100 percent of the screen width (5 by default).', { amount: { type: 'number', description: '0 to 100, percent of the screen width.' } }, ['amount']),
  R('configure_exposure', 'Set exposure: method (Manual, Histogram, the default, or Basic), compensationValue (AutoExposureBias, in stops), minBrightness and maxBrightness (the auto exposure range), then settings for any other exposure field (AutoExposureSpeedUp, AutoExposureSpeedDown). The range is in EV100 when the project extends the default luminance range, as UE 5 projects do, and in luminance otherwise. Send at least one.', {
    settings: P.ppSettings, compensationValue: { type: 'number', description: 'Exposure bias in stops (+1 is twice as bright).' }, minBrightness: { type: 'number', description: 'Lowest auto exposure (EV100 with the extended luminance range).' }, maxBrightness: { type: 'number', description: 'Highest auto exposure (EV100 with the extended luminance range).' },
    method: { ...P.method, description: 'Auto-exposure method: Manual (fixed; camera settings), Histogram (the default) or Basic.' },
  }),
  R('set_exposure_method', 'Set AutoExposureMethod: Manual (no adaptation: exposure from compensation and camera settings), Histogram (the default, adapting from the scene\'s brightness) or Basic (a cheaper adaptation). Anything else is INVALID_SETTING.', { method: { type: 'string', description: 'Manual, Histogram or Basic.' } }, ['method']),
];
