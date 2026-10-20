// Fold specs for build_environment. Data only; see ../shared/fold.ts.
import { byName, byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';
import { LIGHT_SUMMARY, LIGHT_WHEN_NOT_TO_USE, LIGHT_WHEN_TO_USE } from '../shared/light-text.js';

export const BUILD_ENVIRONMENT_FOLDS: readonly FoldSpec[] = [
  // landscape
  {
    primary: 'create_landscape', selector: 'kind',
    summary: 'Create a new landscape in the world (its current level), or a grass type for one, chosen by kind. landscape spawns a flat Landscape actor labelled name (or landscapeName; an actor already under it is refused, NAME_TAKEN), its corner at location: componentCount {x, y} components (default 8 x 8), or the fewest that cover sizeX and sizeY (vertices like the editor Overall Resolution, or quads). Each component holds sectionsPerComponent (1 or 2) squared sections of quadsPerSection quads (7, 15, 31, 63 by default, 127 or 255). materialPath goes on it (default the engine grid material; one that does not load is refused, ASSET_NOT_FOUND). builtComponents, resolutionX/resolutionY and extentX/extentY (half the built size; a quad is 100 units) are measured from what was built, a build that makes no components fails (LANDSCAPE_BUILD_FAILED), and the level is left unsaved. grass_type saves a landscape grass type asset named name in path (default /Game/Landscape) that grows meshPath at density 1 per 10 square metres, scaled 0.8 to 1.2, randomly turned and aligned to the surface; a grass type already there is kept unchanged (alreadyExists) and another asset there is refused (ASSET_ALREADY_EXISTS).',
    topics: ['create a new landscape'],
    members: { landscape: 'create_landscape', grass_type: 'create_landscape_grass_type' },
  },
  {
    primary: 'sculpt', selector: 'sculptOp',
    summary: 'Sculpt a landscape with a brush, modify its heightmap data directly, or set its material.',
    topics: ['sculpt landscape', 'heightmap', 'landscape material', 'raise or flatten the terrain or ground'],
    members: { sculpt: 'sculpt', heightmap: 'modify_heightmap', material: 'set_landscape_material' },
    aliasMembers: ['sculpt_landscape'],
  },
  {
    primary: 'edit_landscape', selector: 'edit',
    summary: 'Edit a landscape: paint layers, import a heightmap, configure LOD, material or splines, generate LODs.',
    topics: ['paint landscape', 'landscape layer', 'import heightmap', 'landscape lod', 'landscape splines'],
    members: byName(['paint_landscape', 'paint_landscape_layer', 'import_heightmap', 'configure_landscape_lod', 'configure_landscape_material', 'configure_landscape_splines', 'generate_lods']),
  },
  {
    primary: 'create_landscape_asset', selector: 'kind',
    summary: 'Create a landscape asset, chosen by kind. layer_info makes a landscape layer info asset (the layer a landscape material paints) named layerName or name in path (default /Game/Landscape), with hardness, physicalMaterialPath (refused when it does not load) and noWeightBlend; an asset already at that path is refused (ASSET_ALREADY_EXISTS). streaming_proxy places a World Partition streaming proxy linked to the landscape, at location and rotation (default the landscape\'s), reusing a proxy already labelled actorName.',
    members: { layer_info: 'create_landscape_layer_info', streaming_proxy: 'create_landscape_streaming_proxy' },
  },
  // foliage
  {
    primary: 'add_foliage', selector: 'foliageOp',
    // 'scatter' creates or updates the foliage TYPE asset (mesh, density,
    // scale range, culling); it places no instances. Saying it scattered
    // led callers to ask for 120 instances in a 6000uu radius, get a
    // success back, and find an empty level.
    summary: 'Create or update a foliage type asset, place explicit instances/transforms, or paint instances over a brush disc or a box area, dropped onto the ground.',
    topics: ['add foliage', 'paint foliage', 'foliage instances', 'scatter foliage', 'scatter grass', 'fill area with foliage', 'plant trees', 'place bushes', 'add trees, grass or flowers to the level', 'make a forest', 'scatter rocks'],
    members: { scatter: 'add_foliage', instances: 'add_foliage_instances', paint: 'paint_foliage' },
  },
  {
    primary: 'create_foliage_type', selector: 'kind',
    summary: 'Create foliage assets, chosen by kind. type makes a static mesh foliage type named name in path (default /Game/Foliage) that places meshPath, with density (instances per 1000 x 1000 units, default 100), minScale and maxScale (default 1), alignToNormal and randomYaw (default true) and cullDistance. A foliage type already there is updated instead, keeping the settings not sent (created false); another kind of asset under the name is refused (ASSET_ALREADY_EXISTS). procedural places a procedural foliage volume and a spawner asset named <name>_Spawner (refused when it exists) with one foliage type per foliageTypes entry ({meshPath, density}), simulated with seed over bounds; a simulation that places nothing fails.',
    members: { type: 'create_foliage_type', procedural: 'create_procedural_foliage' },
  },
  {
    primary: 'configure_foliage', selector: 'setting',
    summary: 'Change a foliage type asset (foliageTypePath, or foliageType), chosen by setting: mesh (meshPath; previousMesh, placedInstances and instancesShowingNewMesh tell how the placed instances followed), placement (density, minScale, maxScale, alignToNormal, randomYaw), collision (collisionEnabled), culling (cullDistance; the fade start moves in when it was further) or lod (settings: UFoliageType properties by name). Only the fields sent are written, and configuredProperties names them; a call that sets nothing is refused, as is a value a property will not take (CONFIGURATION_FAILED). Saved (SAVE_FAILED otherwise; engine content is never saved).',
    topics: ['foliage mesh', 'foliage placement', 'foliage collision', 'foliage culling', 'foliage lod'],
    members: byTarget('configure_foliage_', ['configure_foliage_mesh', 'configure_foliage_placement', 'configure_foliage_collision', 'configure_foliage_culling', 'configure_foliage_lod']),
  },
  { primary: 'remove_foliage', topics: ['remove trees or grass'], summary: 'Remove foliage instances of a type, all foliage, or only those inside an area box (a pit or a path).', members: ['remove_foliage_instances'] },
  // lighting
  {
    primary: 'create_light', summary: LIGHT_SUMMARY, whenToUse: LIGHT_WHEN_TO_USE, whenNotToUse: LIGHT_WHEN_NOT_TO_USE,
    topics: ['add point light', 'add spot light', 'add a light', 'place a light', 'rect light', 'spotlight', 'street lamp light', 'light intensity', 'light color'],
    members: ['create_dynamic_light', 'spawn_light'],
  },
  {
    primary: 'create_sky_light', selector: 'skyLightOp',
    summary: 'Create a sky light (skyLightOp create, the default), or make sure the edited level has exactly one (ensure_single). create spawns one at location (default 0,0,500) as one undo step, lit by the captured scene or by cubemapPath (refused before anything spawns when that path holds no cube texture), at intensity; recapture captures the sky again now. During Play it spawns into the running game and is gone when play stops. ensure_single spawns a sky light labelled name (default SkyLight) when the level has none and relabels the only one to name; with several it refuses with MULTIPLE_SKY_LIGHTS, naming the one to keep (keep) and the ones to delete with control_actor delete (remove), and deletes nothing itself. It refuses during Play (PIE_ACTIVE), and its reply says spawned and relabelled.',
    members: { create: 'create_sky_light', ensure_single: 'ensure_single_sky_light' },
    aliasMembers: ['spawn_sky_light'],
  },
  {
    primary: 'configure_lighting', selector: 'setting',
    summary: 'Configure scene lighting: shadows, ambient occlusion, exposure, global illumination, volumetric fog.',
    topics: ['shadows', 'ambient occlusion', 'exposure', 'global illumination', 'lumen', 'volumetric fog', 'turn on lumen'],
    members: { shadows: 'configure_shadows', ambient_occlusion: 'set_ambient_occlusion', exposure: 'set_exposure', global_illumination: 'setup_global_illumination', volumetric_fog: 'setup_volumetric_fog' },
  },
  {
    primary: 'create_lighting_setup', selector: 'kind',
    summary: 'Create a lighting-enabled level or a Lightmass importance volume.',
    members: { level: 'create_lighting_enabled_level', lightmass_volume: 'create_lightmass_volume' },
  },
  // render / post process
  {
    primary: 'configure_ray_tracing', selector: 'feature',
    summary: 'Configure a ray-tracing feature: ambient occlusion, global illumination, reflections, shadows, path tracing.',
    topics: ['ray tracing', 'ray traced shadows', 'ray traced reflections', 'path tracing', 'ray traced gi'],
    members: { ao: 'configure_ray_traced_ao', gi: 'configure_ray_traced_gi', reflections: 'configure_ray_traced_reflections', shadows: 'configure_ray_traced_shadows', path_tracing: 'configure_path_tracing' },
  },
  {
    primary: 'configure_post_process', selector: 'setting',
    summary: 'Configure a post-process setting on a volume or camera: bloom, exposure, depth of field, motion blur, tonemapper, color grading, LUT, white balance, vignette, grain, chromatic aberration, lens flare, SSAO/GTAO, SSR, screen percentage, blend.',
    topics: ['post process', 'bloom', 'exposure', 'auto exposure', 'eye adaptation', 'depth of field', 'motion blur', 'tonemapper', 'color grading', 'vignette', 'add bloom', 'screen tint', 'change screen tint', 'add bloom or depth of field'],
    members: byName(['configure_bloom', 'set_bloom_intensity', 'set_bloom_threshold', 'configure_exposure', 'set_exposure_compensation', 'set_exposure_method',
      'set_exposure_min_max', 'configure_dof', 'set_dof_method', 'set_focal_distance', 'set_aperture', 'configure_bokeh', 'configure_motion_blur',
      'set_motion_blur_amount', 'set_motion_blur_max', 'configure_tonemapper', 'set_tonemapper_type', 'set_pp_color_grading', 'set_pp_lut',
      'set_pp_white_balance', 'configure_vignette', 'configure_grain', 'configure_chromatic_aberration', 'configure_lens_flare', 'configure_ssao',
      'configure_gtao', 'configure_ssr_settings', 'configure_lumen_reflection_settings', 'configure_screen_percentage', 'configure_pp_blend']),
  },
  {
    primary: 'create_capture_actor', selector: 'kind',
    summary: 'Place a capture actor labelled name at location and rotation, chosen by kind: scene_capture_2d or scene_capture_cube (renders the view into a render target, assigned with configure_scene_capture setting=render_target), or a sphere_reflection, box_reflection or planar_reflection capture (settings: properties of its capture component). A capture of the same kind already labelled name is kept (alreadyExists); an actor of another class under that label is refused (NAME_TAKEN).',
    topics: ['scene capture', 'reflection capture', 'planar reflection', 'render target capture', 'place a reflection capture', 'add a reflection capture'],
    members: { scene_capture_2d: 'create_scene_capture_2d', scene_capture_cube: 'create_scene_capture_cube', sphere_reflection: 'create_sphere_reflection_capture', box_reflection: 'create_box_reflection_capture', planar_reflection: 'create_planar_reflection' },
  },
  {
    primary: 'configure_scene_capture', selector: 'setting',
    summary: 'Configure a capture actor: render target, capture source, resolution, offset, planar-reflection settings, reflection capture resolution, or trigger a (re)capture.',
    topics: ['render target', 'capture source', 'capture resolution', 'recapture scene', 'reflection capture resolution'],
    members: {
      render_target: 'assign_render_target', source: 'configure_capture_source', resolution: 'configure_capture_resolution', offset: 'configure_capture_offset',
      planar_reflection: 'configure_planar_reflection', reflection_resolution: 'configure_reflection_capture_resolution', capture: 'capture_scene', recapture: 'recapture_scene',
    },
  },
  {
    primary: 'configure_lightmass', selector: 'setting',
    summary: 'Configure static lighting builds: Lightmass settings, build quality, indirect lighting cache.',
    topics: ['lightmass', 'lighting quality', 'indirect lighting cache', 'static lighting'],
    members: { settings: 'configure_lightmass_settings', build_quality: 'build_lighting_quality', indirect_lighting_cache: 'configure_indirect_lighting_cache' },
  },
  {
    primary: 'set_light_channel', selector: 'channelTarget',
    summary: 'Turn lighting channels 0-2 (channels, or one channel) on or off (enabled, default true): on a light, which then lights only primitives sharing a channel with it, or on every primitive of an actor. One undo step; the playing actor while Play In Editor runs.',
    members: { light: 'set_light_channel', actor: 'set_actor_light_channel' },
  },
  // spline
  {
    primary: 'create_spline', selector: 'kind',
    summary: 'Create a spline actor, a spline mesh component, or a typed spline (road, wall, fence, pipe, cable, river).',
    topics: ['spline actor', 'spline mesh', 'road spline', 'wall spline', 'fence', 'pipe', 'cable', 'river spline', 'make a spline', 'add a cable', 'add a fence along a path'],
    members: {
      actor: 'create_spline_actor', mesh_component: 'create_spline_mesh_component', road: 'create_road_spline', wall: 'create_wall_spline',
      fence: 'create_fence_spline', pipe: 'create_pipe_spline', cable: 'create_cable_spline', river: 'create_river_spline',
    },
  },
  {
    primary: 'edit_spline', selector: 'edit',
    summary: 'Edit spline points: add a point, set a point\'s position, rotation, scale or tangents, set the spline type.',
    topics: ['spline point', 'spline tangent', 'spline type'],
    members: { add_point: 'add_spline_point', set_point_position: 'set_spline_point_position', set_point_rotation: 'set_spline_point_rotation', set_point_scale: 'set_spline_point_scale', set_point_tangents: 'set_spline_point_tangents', set_type: 'set_spline_type' },
  },
  {
    primary: 'configure_spline_meshes', selector: 'setting',
    summary: 'Configure meshes along a spline: mesh asset, material, forward axis, spacing, randomization, or scatter meshes along it.',
    topics: ['spline mesh asset', 'spline mesh material', 'mesh spacing', 'scatter along spline', 'mesh randomization', 'space fence posts'],
    members: { mesh_asset: 'set_spline_mesh_asset', material: 'set_spline_mesh_material', axis: 'configure_spline_mesh_axis', spacing: 'configure_mesh_spacing', randomization: 'configure_mesh_randomization', scatter: 'scatter_meshes_along_spline' },
  },
  // atmosphere
  {
    primary: 'configure_atmosphere', selector: 'setting',
    summary: 'Configure the sky and atmosphere: sky atmosphere, sky light, sun position, directional light, height fog, volumetric clouds, time of day, sky/light color curves.',
    topics: ['sky atmosphere', 'sun position', 'height fog', 'volumetric cloud', 'time of day', 'sky light', 'directional light', 'add fog', 'add sky', 'make it night', 'make it foggy', 'make it sunset or sunrise', 'add fog to the scene'],
    members: {
      sky_atmosphere: 'configure_sky_atmosphere', sky_light: 'configure_sky_light', sun_position: 'configure_sun_position', directional_light: 'configure_directional_light_atmosphere',
      height_fog: 'configure_exponential_height_fog', volumetric_cloud: 'configure_volumetric_cloud', time_of_day: 'set_time_of_day',
      sky_color_curve: 'configure_sky_color_curve', light_color_curve: 'configure_light_color_curve',
    },
  },
  {
    primary: 'create_atmosphere_actor', selector: 'kind',
    summary: 'Create the sky lighting rig (kind sky_sphere: a Sky Atmosphere, a sun and a Sky Light), an Exponential Height Fog (fog_volume), or one actor carrying its own sun, sky light and atmosphere at noon (time_of_day_system; nothing animates it, set_time_of_day moves the sun). Each reuses an actor already carrying its label, but one labelled otherwise is not seen, so mixing sky_sphere and time_of_day_system, or a sun of another label, doubles the sun.',
    topics: ['sky sphere', 'fog volume', 'time of day system'],
    members: { sky_sphere: 'create_sky_sphere', fog_volume: 'create_fog_volume', time_of_day_system: 'create_time_of_day_system' },
  },
  // weather
  {
    primary: 'configure_weather', selector: 'setting',
    summary: 'Configure a weather actor: rain, snow or lightning set a Cascade particle system (particleSystemPath) on an Emitter labelled WeatherEmitter, SnowEmitter or LightningEmitter; wind spawns or tunes a WindDirectionalSource (direction as its rotation, speed and other settings by property name). Nothing is simulated beyond what those actors do; not saved.',
    topics: ['rain', 'snow', 'lightning', 'wind', 'weather', 'add snow', 'add rain, snow, wind or lightning'],
    members: { rain: 'configure_rain_particles', snow: 'configure_snow_particles', lightning: 'configure_lightning', wind: 'configure_wind' },
  },
  // water
  {
    primary: 'create_water_body', selector: 'kind',
    summary: 'Create a water body (ocean, lake, river or custom) with the Water plugin: real water that takes the editor\'s default materials and, for an ocean or lake, its default waves, as placing one in the editor does (listed under defaultsApplied).',
    topics: ['water body', 'ocean', 'lake', 'river', 'add a lake or ocean', 'add a river', 'create a river', 'make a river'],
    members: byTarget('create_water_body_', ['create_water_body_ocean', 'create_water_body_lake', 'create_water_body_river', 'create_water_body_custom']),
  },
  {
    primary: 'configure_water', selector: 'setting',
    summary: 'Configure a water body: its Gerstner waves (height, wavelength, steepness, direction), material or collision.',
    topics: ['water waves', 'water material', 'water collision'],
    members: byTarget('configure_water_', ['configure_water_waves', 'configure_water_material', 'configure_water_collision']),
  },
];
