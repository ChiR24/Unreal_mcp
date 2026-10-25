/**
 * Lighting family records (15 actions).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';
import { str } from '../shared/schema-props.js';
import {
  LIGHT_AIM_TEXT, LIGHT_CLASS_TEXT, LIGHT_COLOR_WITH_PROPERTIES_TEXT, LIGHT_INTENSITY_TEXT, LIGHT_LOCATION_TEXT, LIGHT_PROPERTIES_TEXT,
  LIGHT_SUMMARY, LIGHT_TYPE_TEXT, LIGHT_WHEN_NOT_TO_USE, LIGHT_WHEN_TO_USE,
} from '../shared/light-text.js';

const F = 'lighting';
const WU = ['A light actor or lighting setting must be created or configured.'];

export const LIGHTING_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_light', action: 'create_light', family: F,
    summary: LIGHT_SUMMARY,
    whenToUse: LIGHT_WHEN_TO_USE, whenNotToUse: LIGHT_WHEN_NOT_TO_USE,
    inputProps: {
      lightType: { ...P.lightType, description: LIGHT_TYPE_TEXT },
      lightClass: { ...P.lightClass, description: LIGHT_CLASS_TEXT },
      name: P.name,
      location: { ...P.location, description: LIGHT_LOCATION_TEXT },
      rotation: { ...P.rotation, description: `Rotation {pitch, yaw, roll} in degrees. ${LIGHT_AIM_TEXT}` },
      intensity: { ...P.intensity, description: LIGHT_INTENSITY_TEXT },
      color: { ...P.color, description: `Light color {r, g, b, a}. ${LIGHT_COLOR_WITH_PROPERTIES_TEXT}` },
      properties: { ...P.properties, description: LIGHT_PROPERTIES_TEXT },
    },
    requiredOneOf: ['lightClass', 'lightType'],
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: {
      action: 'create_light', lightType: 'Spot', name: 'StreetLamp', location: { x: 0, y: 0, z: 400 },
      rotation: { pitch: -90, yaw: 0, roll: 0 }, intensity: 5000, color: { r: 1, g: 0.7, b: 0.42 },
      properties: { castShadows: true, outerConeAngle: 50, attenuationRadius: 1200 },
    },
    exampleOutput: {
      success: true, message: 'Light spawned',
      details: {
        actorName: 'StreetLamp', actorLabel: 'StreetLamp', objectName: 'SpotLight_0',
        applied: ['intensity', 'color', 'castShadows', 'outerConeAngle', 'attenuationRadius'],
      },
    },
  }),
  buildRecord({
    id: 'build_environment.create_sky_light', action: 'create_sky_light', family: F,
    summary: 'Create a sky light actor, capturing the scene or lighting from a cubemap.',
    whenToUse: WU, whenNotToUse: ['A sky light already exists.'],
    inputProps: { name: P.name, location: P.location, rotation: P.rotation, intensity: P.intensity,
      cubemapPath: { ...P.cubemapPath, description: 'Cube texture to light from (sets the specified-cubemap source); a path that does not load fails before anything spawns.' },
      recapture: P.recapture },
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_sky_light', name: 'SkyLight_1' },
  }),
  buildRecord({
    id: 'build_environment.ensure_single_sky_light', action: 'ensure_single_sky_light', family: F,
    summary: 'Make sure the edited level has exactly one sky light: spawn one when it has none, relabel the only one; with several, refuse and name the ones to delete.',
    whenToUse: ['A level must end up lit by exactly one sky light.', 'Before adding a sky light, to reuse the one the level has.'],
    whenNotToUse: ['Several sky lights are intentionally present.', 'The extra sky lights must go: delete them with control_actor delete (actorNames), which this never does.'],
    inputProps: { name: { ...P.name, description: 'Label the sky light must carry (default SkyLight): a single sky light with another label is relabelled, and when there is none one is spawned under it. With several, the one so labelled is the one the refusal says to keep.' }, recapture: P.recapture },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'ensure_single_sky_light' },
  }),
  buildRecord({
    id: 'build_environment.create_lightmass_volume', action: 'create_lightmass_volume', family: F,
    summary: 'Place a Lightmass Importance Volume in the editor level: a box size cm across (default 1000 on each axis) at location, labelled name (a label already in the level is refused), in one undo step. Lightmass spends its photons inside it, so baked lighting outside is coarser; the reply gives the size it encloses. manage_level_structure create_volume (volumeClass LightmassImportanceVolume) places the same volume.',
    whenToUse: ['A lightmass importance volume is needed for baking.'],
    whenNotToUse: ['Dynamic lighting is used exclusively.'],
    inputProps: { name: P.name, location: P.location, size: P.size },
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'create_lightmass_volume', name: 'LIV_1' },
  }),
  buildRecord({
    id: 'build_environment.create_lighting_enabled_level', action: 'create_lighting_enabled_level', family: F,
    summary: 'Create a new level at path (levelName is appended when path is a folder) lit by a sun (a directional light driving the sky), a SkyAtmosphere and a sky light capturing that sky in real time, save it and open it in place of the open level; actors lists what it holds. Refused during Play (PIE_ACTIVE) and while open levels have unsaved changes (DIRTY_PACKAGES, listed in unsavedPackages); a level already at the path is neither replaced nor opened (alreadyExisted).',
    whenToUse: ['A new level with lighting setup is needed.'],
    whenNotToUse: ['An existing level should be modified.'],
    inputProps: { name: P.name, path: P.path, levelName: str('Level name; appended when path is a folder.') },
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'create_lighting_enabled_level', name: 'LightingLevel' },
  }),
  buildRecord({
    id: 'build_environment.setup_global_illumination', action: 'setup_global_illumination', family: F,
    summary: 'Switch the dynamic global illumination method (method: LumenGI, which also turns on Lumen reflections, ScreenSpace, None or Lightmass for baked lighting only; RayTraced only before UE 5.4) through r.DynamicGlobalIlluminationMethod, set as a console command would set it and read back (GI_METHOD_NOT_APPLIED names what held it). That lasts for this editor session; saveToProject true also writes it to Project Settings > Rendering (DefaultEngine.ini), so the project and its packaged game keep it (savedToProject). quality sets the GI scalability level for the session (sg.GlobalIlluminationQuality; one that did not take is listed under notSet), indirectLightingIntensity a post-process volume\'s override (actorName names a post-process volume, else the level\'s one unbound volume is used (several are AMBIGUOUS; with none, an unbound one is spawned)), bounces the level\'s Lightmass bounces.',
    whenToUse: ['GI must be enabled or tuned.'],
    whenNotToUse: ['GI is not needed for the scene.'],
    inputProps: {
      method: {
        type: 'string',
        enum: ['LumenGI', 'ScreenSpace', 'None', 'RayTraced', 'Lightmass'],
        description: 'Global illumination method. Matches the handler-enforced value set.',
      },
      quality: {
        type: 'string',
        enum: ['Low', 'Medium', 'High', 'Epic'],
        description: 'Global illumination scalability level (sg.GlobalIlluminationQuality).',
      },
      indirectLightingIntensity: P.indirectLightingIntensity, bounces: P.bounces,
      saveToProject: { type: 'boolean', description: 'Also write the method to Project Settings > Rendering (DefaultEngine.ini), so it survives an editor restart and reaches the packaged game (default false: this session only).' },
    },
    required: ['method'],
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'setup_global_illumination', method: 'LumenGI' },
  }),
  buildRecord({
    id: 'build_environment.configure_shadows', action: 'configure_shadows', family: F,
    summary: 'Set shadows. Engine-wide values go to console variables for this editor session only (not saved to the project): shadowQuality Low, Medium, High, Epic or Cinematic (sg.ShadowQuality), shadowDistance (r.Shadow.DistanceScale), contactShadows, rayTracedShadows and virtualShadowMaps, each set as a console command would set it and read back: one that did not take is listed under notSet with why (read-only, or missing in this engine version), and a call where none took is refused. With actorName, that light\'s own castShadows, shadowBias, shadowSlopeBias and shadowResolutionScale change, saved with the level. settings may carry the same keys nested; a value there wins.',
    whenToUse: ['Shadow quality or method must be tuned.'],
    whenNotToUse: ['Shadows are not needed.'],
    inputProps: {
      settings: { ...P.settings, description: 'The same shadow keys as the top level, nested; a value here wins.' },
      actorName: { ...P.actorName, description: 'Light whose own shadow settings (castShadows, shadowBias, shadowSlopeBias, shadowResolutionScale) change.' },
      shadowQuality: P.shadowQuality, shadowDistance: P.shadowDistance, contactShadows: P.contactShadows,
      rayTracedShadows: P.rayTracedShadows, virtualShadowMaps: P.virtualShadowMaps,
      castShadows: P.castShadows, shadowBias: P.shadowBias, shadowSlopeBias: P.shadowSlopeBias, shadowResolutionScale: P.shadowResolutionScale,
    },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_shadows', actorName: 'Light_1', shadowQuality: 'High' },
  }),
  buildRecord({
    id: 'build_environment.set_exposure', action: 'set_exposure', family: F,
    summary: 'Set auto-exposure on a post-process volume: actorName names a post-process volume, else the level\'s one unbound volume is used (several are AMBIGUOUS; with none, an unbound one is spawned). method (Manual, AutoExposureHistogram or AutoExposureBasic), compensationValue (the exposure bias), minBrightness and maxBrightness each turn on their override; a call that sets none is NO_SETTING_SUPPLIED. The reply lists appliedSettings; one undo step, a spawned volume included.',
    whenToUse: ['Exposure must be adjusted.'],
    whenNotToUse: ['Default exposure is sufficient.'],
    inputProps: {
      actorName: P.actorName,
      method: {
        type: 'string',
        enum: ['Manual', 'AutoExposureHistogram', 'AutoExposureBasic'],
        description: 'Auto-exposure method written on the post-process volume.',
      },
      compensationValue: P.compensationValue, minBrightness: P.minBrightness, maxBrightness: P.maxBrightness,
    },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'set_exposure', compensationValue: 1.0 },
  }),
  buildRecord({
    id: 'build_environment.set_ambient_occlusion', action: 'set_ambient_occlusion', family: F,
    summary: 'Set ambient occlusion on a post-process volume: actorName names a post-process volume, else the level\'s one unbound volume is used (several are AMBIGUOUS; with none, an unbound one is spawned). intensity, radius and quality (Low 25, Medium 50 or High 100 percent) each turn on their override; enabled true sets intensity 0.5 and false sets 0, unless intensity is sent too. A call that sets none is NO_SETTING_SUPPLIED. The reply gives intensity, radius and qualityPercent; one undo step, a spawned volume included.',
    whenToUse: ['AO intensity or method must be tuned.'],
    whenNotToUse: ['AO is not needed.'],
    inputProps: {
      actorName: P.actorName, enabled: P.enabled,
      intensity: { type: 'number', description: 'Ambient occlusion intensity.' },
      radius: { type: 'number', description: 'Ambient occlusion radius in world units.' },
      quality: {
        type: 'string',
        enum: ['Low', 'Medium', 'High'],
        description: 'Ambient occlusion quality on the post-process volume (25, 50 or 100 of 100).',
      },
    },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'instant', resources: 'low',
    exampleInput: { action: 'set_ambient_occlusion', intensity: 0.5 },
  }),
  buildRecord({
    id: 'build_environment.setup_volumetric_fog', action: 'setup_volumetric_fog', family: F,
    summary: 'Turn volumetric fog on or off (enabled) and set how far it reaches (viewDistance) on the level\'s first Exponential Height Fog, spawning one at the origin when there is none. What is left out keeps its value; a call that sets neither is NO_SETTING_SUPPLIED. The reply reads both back; one undo step.',
    whenToUse: ['Volumetric fog must be enabled or tuned.'],
    whenNotToUse: ['Standard exponential height fog is sufficient.'],
    inputProps: { enabled: P.enabled, viewDistance: P.viewDistance },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'setup_volumetric_fog', enabled: true, viewDistance: 6000 },
  }),
  buildRecord({
    id: 'build_environment.build_lighting', action: 'build_lighting', family: F,
    topics: ['bake lighting', 'lightmass', 'build lights', 'rebuild lighting', 'bake lightmaps'],
    summary: 'Build the level\'s static lighting (Lightmass) at quality (default Production; Preview, Medium, High). Reflection captures are not rebuilt, a level whose World Settings force no precomputed lighting is refused (PRECOMPUTED_LIGHTING_DISABLED), an unknown quality is UNKNOWN_QUALITY, and a failed build lists its reasons (no static lights, no Lightmass importance volume). Can take minutes; nothing is saved.',
    whenToUse: ['Static lighting must be built.'],
    whenNotToUse: ['Dynamic lighting is used exclusively.'],
    inputProps: { quality: P.quality },
    effect: 'write', behavior: { longRunning: true, idempotency: 'idempotent' },
    latency: 'long-running', resources: 'high',
    exampleInput: { action: 'build_lighting', quality: 'Preview' },
  }),
  buildRecord({
    id: 'build_environment.list_light_types', action: 'list_light_types', family: F,
    topics: ['kinds available', 'light classes', 'class names'],
    summary: 'List the light classes known to the editor: DirectionalLight, PointLight, SpotLight, RectLight and any other loaded light class, with a count.',
    whenToUse: ['Available light types must be enumerated.'],
    whenNotToUse: ['A specific light type is already known.'],
    inputProps: {},
    outputProps: {
      types: { type: 'array', items: str('Light type.'), description: 'Available light types.' },
      count: { type: 'number', description: 'Number of available light types.' },
    },
    outputRequired: ['types'],
    effect: 'read', latency: 'instant', resources: 'low',
    exampleInput: { action: 'list_light_types' },
    exampleOutput: { success: true, types: ['PointLight', 'SpotLight', 'DirectionalLight'], count: 3 },
  }),
];
