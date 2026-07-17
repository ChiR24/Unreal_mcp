/**
 * Render shard 1: ray tracing, lightmass, reflection captures (18 actions).
 *
 * Grounded in consolidated-handler-registration.ts: RENDER_ACTIONS dispatch
 * through executeAutomationRequest(tools, 'manage_render', { subAction: action }).
 * Native RenderHandlers.cpp and EnvironmentHandlersReflectionSettings.cpp call
 * MarkPackageDirty() - packages are marked dirty but NOT saved immediately
 * (deferred persistence). The caller must save separately.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'render';
const WU = ['Render or lighting quality settings must be configured.'];
const ID = 'build_environment.';
const R = (action: string, summary: string, inputProps: Record<string, unknown>, required: string[],
  effect: 'read' | 'write' = 'write'): CapabilityRecordSource => buildRecord({
  id: ID + action, action, family: F, summary, whenToUse: WU,
  whenNotToUse: ['Default rendering settings are sufficient.'],
  inputProps: { action: P.action, ...inputProps }, required,
  effect, behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
  exampleInput: { action }, exampleOutput: { success: true, message: summary },
});

export const RENDER_RAYTRACE_RECORDS: readonly CapabilityRecordSource[] = [
  R('configure_ray_traced_shadows', 'Configure ray-traced shadow settings.', { settings: P.settings, enabled: P.enabled }, ['action']),
  R('configure_ray_traced_gi', 'Configure ray-traced global illumination.', { settings: P.settings, enabled: P.enabled }, ['action']),
  R('configure_ray_traced_reflections', 'Configure ray-traced reflections.', { settings: P.settings, enabled: P.enabled }, ['action']),
  R('configure_ray_traced_ao', 'Configure ray-traced ambient occlusion.', { settings: P.settings, enabled: P.enabled }, ['action']),
  R('configure_path_tracing', 'Configure path tracing settings.', { settings: P.settings, enabled: P.enabled }, ['action']),
  R('set_light_channel', 'Set the light channel on a light actor.', { actorName: P.actorName, channel: { type: 'integer', description: 'Light channel index.' } }, ['action']),
  R('set_actor_light_channel', 'Set the light channel on a target actor.', { actorName: P.actorName, targetActor: P.targetActor, channel: { type: 'integer', description: 'Light channel index.' } }, ['action']),
  R('configure_lightmass_settings', 'Configure Lightmass global settings.', { settings: P.settings }, ['action']),
  R('build_lighting_quality', 'Build lighting at a specific quality.', { quality: P.quality, settings: P.settings }, ['action']),
  R('configure_indirect_lighting_cache', 'Configure indirect lighting cache.', { settings: P.settings, enabled: P.enabled }, ['action']),
  R('create_sphere_reflection_capture', 'Create a sphere reflection capture actor.', { name: P.name, location: P.location }, ['action']),
  R('create_box_reflection_capture', 'Create a box reflection capture actor.', { name: P.name, location: P.location }, ['action']),
  R('configure_reflection_capture_resolution', 'Set reflection capture resolution.', { resolution: { type: 'integer', description: 'Capture resolution.' } }, ['action']),
  R('configure_capture_resolution', 'Set scene capture resolution.', { resolution: { type: 'integer', description: 'Capture resolution.' } }, ['action']),
  R('configure_capture_offset', 'Set scene capture offset.', { captureOffset: P.captureOffset }, ['action']),
  R('recapture_scene', 'Recapture a reflection or scene capture.', { actorName: P.actorName }, ['action']),
  R('create_planar_reflection', 'Create a planar reflection actor.', { name: P.name, location: P.location, rotation: P.rotation }, ['action']),
  R('configure_planar_reflection', 'Configure planar reflection settings.', { actorName: P.actorName, settings: P.settings }, ['action']),
];
