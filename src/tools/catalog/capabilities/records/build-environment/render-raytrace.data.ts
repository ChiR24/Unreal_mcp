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

// A ray-tracing action sets console variables: its feature's on/off from enabled, the rest from settings keys.
const RT_ENABLED = { ...P.enabled, description: 'Turn the feature on (default true) or off: its console variable is set to 1 or 0 even when only settings are sent.' };
const RT_SETTINGS = (keys: string): JsonObject => ({ ...P.settings, description: `Console values by key, only these (any other key is refused and nothing is set): ${keys}.` });
const RT_TAIL = ' Values are set as a console command would set them (console_command and this call replace each other, last one wins) and last for this editor session, not saved to the project. With hardware ray tracing off for the project (r.RayTracing 0, fixed at startup) they are stored, draw nothing, and the reply warns (rayTracingEnabled false). A variable that did not take is listed under unsupportedCVars with why: missing in this engine version, read-only, or reset by the engine; refused with CVAR_UNAVAILABLE when none took.';

export const RENDER_RAYTRACE_RECORDS: readonly CapabilityRecordSource[] = [
  R('configure_ray_traced_shadows', `Turn ray-traced shadows on or off (r.RayTracing.Shadows) and set their samples per pixel.${RT_TAIL}`,
    { settings: RT_SETTINGS('SamplesPerPixel (whole number 1-64)'), enabled: RT_ENABLED }, [], 'write', { action: 'configure_ray_traced_shadows', settings: { SamplesPerPixel: 4 } }),
  R('configure_ray_traced_gi', `Turn ray-traced global illumination on or off (r.RayTracing.GlobalIllumination) and set its samples and bounces.${RT_TAIL}`,
    { settings: RT_SETTINGS('SamplesPerPixel (whole number 1-64), MaxBounces (whole number 0-16)'), enabled: RT_ENABLED }),
  R('configure_ray_traced_reflections', `Turn ray-traced reflections on or off (r.RayTracing.Reflections) and set their samples and roughness cutoff.${RT_TAIL}`,
    { settings: RT_SETTINGS('SamplesPerPixel (whole number 1-64), MaxRoughness (0-1: rougher surfaces fall back to other reflections)'), enabled: RT_ENABLED }),
  R('configure_ray_traced_ao', `Turn ray-traced ambient occlusion on or off (r.RayTracing.AmbientOcclusion) and set its intensity and radius.${RT_TAIL}`,
    { settings: RT_SETTINGS('Radius (0-10000 world units)'), enabled: RT_ENABLED, intensity: { ...P.intensity, description: 'Occlusion intensity 0-10 (r.RayTracing.AmbientOcclusion.Intensity); out of range is refused.' } }),
  R('configure_path_tracing', `Set the path tracer's samples per pixel and bounces. Its switch r.PathTracing is read-only, fixed at startup by the project's config, so enabled cannot turn it on or off here (the reply lists it under unsupportedCVars); to see the path tracer, set_view_mode PathTracing.${RT_TAIL}`,
    { settings: RT_SETTINGS('SamplesPerPixel (whole number 1-256), MaxBounces (whole number 0-16)'), enabled: RT_ENABLED }),
  R('set_light_channel', 'Turn lighting channels on or off for a light actor.', { actorName: P.actorName, channel: { type: 'integer', description: 'Light channel index.' }, channels: P.channels, enabled: CHANNEL_ENABLED }),
  R('set_actor_light_channel', 'Turn lighting channels on or off for a target actor\'s primitives.', { actorName: P.actorName, targetActor: P.targetActor, channel: { type: 'integer', description: 'Light channel index.' }, channels: P.channels, enabled: CHANNEL_ENABLED }),
  R('configure_lightmass_settings', 'Set the level\'s Lightmass settings (World Settings > Lightmass) from settings, keyed by FLightmassWorldInfoSettings field names such as NumIndirectLightingBounces or IndirectLightingQuality; the next lighting build uses them. A key the struct lacks is listed in unsupportedSettings, a call where none applies is refused naming the fields it comes close to; one undo step, and the level is left unsaved.', { settings: P.settings }),
  R('build_lighting_quality', 'Bake the level\'s static lighting at quality (default Production; Preview, Medium, High, or 0 to 3), as build_lighting does: it answers when the bake ends, rebuilds no reflection captures and saves nothing; a level whose World Settings force no precomputed lighting is refused (PRECOMPUTED_LIGHTING_DISABLED), an unknown quality is UNKNOWN_QUALITY, and a failed build lists its reasons.', { quality: P.quality }),
  R('configure_indirect_lighting_cache', 'Set how every primitive component of actorName samples the indirect lighting cache (the baked light a movable or unbuilt object receives): enabled true is point sampling, false off; settings writes other primitive component fields by engine name. One undo step; componentCount says how many components took it.', { actorName: P.actorName, settings: P.settings, enabled: { ...P.enabled, description: 'Indirect lighting cache on (point sampled) or off for each primitive.' } }, ['actorName'], 'write', { action: 'configure_indirect_lighting_cache', actorName: 'StaticMeshActor_1', enabled: false }),
  R('create_sphere_reflection_capture', 'Create a sphere reflection capture actor.', { ...REFLECTION, settings: CAPTURE_SETTINGS }, [], 'write', { action: 'create_sphere_reflection_capture', name: 'SphereRC_1' }, ['name', 'actorName']),
  R('create_box_reflection_capture', 'Create a box reflection capture actor.', { ...REFLECTION, settings: CAPTURE_SETTINGS }, [], 'write', { action: 'create_box_reflection_capture', name: 'BoxRC_1' }, ['name', 'actorName']),
  R('configure_reflection_capture_resolution', 'Set reflection capture resolution (project-wide CVar; marks the named capture, or every capture, for recapture).', { actorName: P.actorName, resolution: { type: 'integer', description: 'Capture resolution.' } }),
  R('configure_capture_resolution', 'Resize the render target a SceneCapture2D draws into to resolution x resolution (1-4096; the reply reads the size back). It is the render target asset that changes, for every capture drawing into it, in one undo step, and it is left unsaved. A cube capture is UNSUPPORTED_TARGET, a capture with no render target RENDER_TARGET_NOT_ASSIGNED; without actorName the level\'s one scene capture is used.', { actorName: P.actorName, resolution: { type: 'integer', description: 'Width and height in pixels, 1 to 4096.' } }, ['resolution']),
  R('configure_capture_offset', 'Move the point a sphere or box reflection capture captures from by captureOffset (cm from the actor, {x, y, z} or [x, y, z]; a component left out keeps its value), then mark it for recapture. One undo step; the reply reads the offset back. Without actorName the level\'s one reflection capture is used; several is AMBIGUOUS.', { actorName: P.actorName, captureOffset: { ...P.captureOffset, description: 'Offset in cm from the capture actor.' } }, ['captureOffset']),
  R('recapture_scene', 'Recapture a reflection or scene capture.', { actorName: P.actorName }),
  R('create_planar_reflection', 'Create a planar reflection actor.', REFLECTION, [], 'write', { action: 'create_planar_reflection', name: 'PlanarRC_1' }, ['name', 'actorName']),
  R('configure_planar_reflection', 'Configure planar reflection settings.', { actorName: P.actorName, settings: P.settings }),
];
