/**
 * Render shard 1: ray tracing, lightmass, reflection captures (18 actions).
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'render';
const WU = ['Render or lighting quality settings must be configured.'];
const ID = 'build_environment.';
const CHANNEL_ENABLED = { ...P.enabled, description: 'Turn the channels on (default true) or off.' };
// A reflection capture is labelled by name (actorName is accepted too), like the scene captures.
const REFLECTION: JsonObject = {
  name: { ...P.name, description: 'Label of the new capture actor; an actor with it already is reported, not duplicated.' },
  actorName: P.actorName, location: P.location, rotation: P.rotation,
};
// Sphere and box captures carry a reflection capture component; a planar reflection does not.
const CAPTURE_SETTINGS = { ...P.settings, description: 'Reflection capture component properties by name, e.g. {"Brightness": 1.5}.' };
const R = (action: string, summary: string, inputProps: JsonObject, required: string[] = [],
  effect: 'read' | 'write' = 'write', exampleInput: JsonObject = { action }, requiredOneOf?: string[]): CapabilityRecordSource => buildRecord({
  id: ID + action, action, family: F, summary, whenToUse: WU,
  whenNotToUse: ['Default rendering settings are sufficient.'],
  inputProps, required, ...(requiredOneOf === undefined ? {} : { requiredOneOf }),
  effect, behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
  exampleInput,
});

export const RENDER_RAYTRACE_RECORDS: readonly CapabilityRecordSource[] = [
  R('configure_ray_traced_shadows', 'Configure ray-traced shadow settings.', { settings: P.settings, enabled: P.enabled }),
  R('configure_ray_traced_gi', 'Configure ray-traced global illumination.', { settings: P.settings, enabled: P.enabled }),
  R('configure_ray_traced_reflections', 'Configure ray-traced reflections.', { settings: P.settings, enabled: P.enabled }),
  R('configure_ray_traced_ao', 'Configure ray-traced ambient occlusion.', { settings: P.settings, enabled: P.enabled, intensity: { ...P.intensity, description: 'Ray-traced ambient occlusion intensity (r.RayTracing.AmbientOcclusion.Intensity).' } }),
  R('configure_path_tracing', 'Configure path tracing settings.', { settings: P.settings, enabled: P.enabled }),
  R('set_light_channel', 'Turn lighting channels on or off for a light actor.', { actorName: P.actorName, channel: { type: 'integer', description: 'Light channel index.' }, channels: P.channels, enabled: CHANNEL_ENABLED }),
  R('set_actor_light_channel', 'Turn lighting channels on or off for a target actor\'s primitives.', { actorName: P.actorName, targetActor: P.targetActor, channel: { type: 'integer', description: 'Light channel index.' }, channels: P.channels, enabled: CHANNEL_ENABLED }),
  R('configure_lightmass_settings', 'Configure Lightmass global settings.', { settings: P.settings }),
  R('build_lighting_quality', 'Build lighting at a specific quality.', { quality: P.quality }),
  R('configure_indirect_lighting_cache', 'Configure indirect lighting cache on an actor\'s primitive components.', { actorName: P.actorName, settings: P.settings, enabled: { ...P.enabled, description: 'Indirect lighting cache on (point sampled) or off for each primitive.' } }, ['actorName'], 'write', { action: 'configure_indirect_lighting_cache', actorName: 'StaticMeshActor_1', enabled: false }),
  R('create_sphere_reflection_capture', 'Create a sphere reflection capture actor.', { ...REFLECTION, settings: CAPTURE_SETTINGS }, [], 'write', { action: 'create_sphere_reflection_capture', name: 'SphereRC_1' }, ['name', 'actorName']),
  R('create_box_reflection_capture', 'Create a box reflection capture actor.', { ...REFLECTION, settings: CAPTURE_SETTINGS }, [], 'write', { action: 'create_box_reflection_capture', name: 'BoxRC_1' }, ['name', 'actorName']),
  R('configure_reflection_capture_resolution', 'Set reflection capture resolution (project-wide CVar; marks the named capture, or every capture, for recapture).', { actorName: P.actorName, resolution: { type: 'integer', description: 'Capture resolution.' } }),
  R('configure_capture_resolution', 'Set scene capture resolution.', { actorName: P.actorName, resolution: { type: 'integer', description: 'Capture resolution.' } }),
  R('configure_capture_offset', 'Set scene capture offset.', { actorName: P.actorName, captureOffset: P.captureOffset }),
  R('recapture_scene', 'Recapture a reflection or scene capture.', { actorName: P.actorName }),
  R('create_planar_reflection', 'Create a planar reflection actor.', REFLECTION, [], 'write', { action: 'create_planar_reflection', name: 'PlanarRC_1' }, ['name', 'actorName']),
  R('configure_planar_reflection', 'Configure planar reflection settings.', { actorName: P.actorName, settings: P.settings }),
];
