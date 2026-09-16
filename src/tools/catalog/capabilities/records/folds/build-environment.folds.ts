// Fold specs for build_environment. Data only; see ../shared/fold.ts.
import { byName, byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const BUILD_ENVIRONMENT_FOLDS: readonly FoldSpec[] = [
  // landscape
  {
    primary: 'create_landscape', selector: 'kind',
    summary: 'Create a landscape actor, or a landscape grass type asset.',
    members: { landscape: 'create_landscape', grass_type: 'create_landscape_grass_type' },
  },
  {
    primary: 'sculpt', selector: 'sculptOp',
    summary: 'Sculpt a landscape with a brush, modify its heightmap data directly, or set its material.',
    topics: ['sculpt landscape', 'heightmap', 'landscape material'],
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
    summary: 'Create a landscape layer info asset or a landscape streaming proxy.',
    members: { layer_info: 'create_landscape_layer_info', streaming_proxy: 'create_landscape_streaming_proxy' },
  },
  // foliage
  {
    primary: 'add_foliage', selector: 'foliageOp',
    // 'scatter' creates or updates the foliage TYPE asset (mesh, density,
    // scale range, culling); it places no instances. Saying it scattered
    // led callers to ask for 120 instances in a 6000uu radius, get a
    // success back, and find an empty level.
    summary: 'Create or update a foliage type asset, place explicit instances/transforms, or paint instances across a brush radius.',
    topics: ['add foliage', 'paint foliage', 'foliage instances', 'scatter foliage'],
    members: { scatter: 'add_foliage', instances: 'add_foliage_instances', paint: 'paint_foliage' },
  },
  {
    primary: 'create_foliage_type', selector: 'kind',
    summary: 'Create a foliage type asset or a procedural foliage spawner.',
    members: { type: 'create_foliage_type', procedural: 'create_procedural_foliage' },
  },
  {
    primary: 'configure_foliage', selector: 'setting',
    summary: 'Configure a foliage type: mesh, placement, collision, culling, LOD.',
    topics: ['foliage mesh', 'foliage placement', 'foliage collision', 'foliage culling', 'foliage lod'],
    members: byTarget('configure_foliage_', ['configure_foliage_mesh', 'configure_foliage_placement', 'configure_foliage_collision', 'configure_foliage_culling', 'configure_foliage_lod']),
  },
  { primary: 'remove_foliage', summary: 'Remove foliage instances of a type, or all foliage.', members: ['remove_foliage_instances'] },
  // lighting
  { primary: 'create_light', summary: 'Create a light actor of a given type.', members: ['create_dynamic_light', 'spawn_light'] },
  {
    primary: 'create_sky_light', selector: 'skyLightOp',
    summary: 'Create a sky light, or ensure the level has exactly one.',
    members: { create: 'create_sky_light', ensure_single: 'ensure_single_sky_light' },
    aliasMembers: ['spawn_sky_light'],
  },
  {
    primary: 'configure_lighting', selector: 'setting',
    summary: 'Configure scene lighting: shadows, ambient occlusion, exposure, global illumination, volumetric fog.',
    topics: ['shadows', 'ambient occlusion', 'exposure', 'global illumination', 'lumen', 'volumetric fog'],
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
    topics: ['post process', 'bloom', 'exposure', 'depth of field', 'motion blur', 'tonemapper', 'color grading', 'vignette'],
    members: byName(['configure_bloom', 'set_bloom_intensity', 'set_bloom_threshold', 'configure_exposure', 'set_exposure_compensation', 'set_exposure_method',
      'set_exposure_min_max', 'configure_dof', 'set_dof_method', 'set_focal_distance', 'set_aperture', 'configure_bokeh', 'configure_motion_blur',
      'set_motion_blur_amount', 'set_motion_blur_max', 'configure_tonemapper', 'set_tonemapper_type', 'set_pp_color_grading', 'set_pp_lut',
      'set_pp_white_balance', 'configure_vignette', 'configure_grain', 'configure_chromatic_aberration', 'configure_lens_flare', 'configure_ssao',
      'configure_gtao', 'configure_ssr_settings', 'configure_lumen_reflection_settings', 'configure_screen_percentage', 'configure_pp_blend']),
  },
  {
    primary: 'create_capture_actor', selector: 'kind',
    summary: 'Create a scene capture (2D or cube) or a reflection capture (sphere, box or planar).',
    topics: ['scene capture', 'reflection capture', 'planar reflection', 'render target capture'],
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
    summary: 'Set lighting channels on a light or on an actor\'s primitives.',
    members: { light: 'set_light_channel', actor: 'set_actor_light_channel' },
  },
  // spline
  {
    primary: 'create_spline', selector: 'kind',
    summary: 'Create a spline actor, a spline mesh component, or a typed spline (road, wall, fence, pipe, cable, river).',
    topics: ['spline actor', 'spline mesh', 'road spline', 'wall spline', 'fence', 'pipe', 'cable', 'river spline'],
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
    topics: ['spline mesh asset', 'spline mesh material', 'mesh spacing', 'scatter along spline', 'mesh randomization'],
    members: { mesh_asset: 'set_spline_mesh_asset', material: 'set_spline_mesh_material', axis: 'configure_spline_mesh_axis', spacing: 'configure_mesh_spacing', randomization: 'configure_mesh_randomization', scatter: 'scatter_meshes_along_spline' },
  },
  // atmosphere
  {
    primary: 'configure_atmosphere', selector: 'setting',
    summary: 'Configure the sky and atmosphere: sky atmosphere, sky light, sun position, directional light, height fog, volumetric clouds, time of day, sky/light color curves.',
    topics: ['sky atmosphere', 'sun position', 'height fog', 'volumetric cloud', 'time of day', 'sky light', 'directional light'],
    members: {
      sky_atmosphere: 'configure_sky_atmosphere', sky_light: 'configure_sky_light', sun_position: 'configure_sun_position', directional_light: 'configure_directional_light_atmosphere',
      height_fog: 'configure_exponential_height_fog', volumetric_cloud: 'configure_volumetric_cloud', time_of_day: 'set_time_of_day',
      sky_color_curve: 'configure_sky_color_curve', light_color_curve: 'configure_light_color_curve',
    },
  },
  {
    primary: 'create_atmosphere_actor', selector: 'kind',
    summary: 'Create a sky sphere, a fog volume, or a time-of-day system.',
    topics: ['sky sphere', 'fog volume', 'time of day system'],
    members: { sky_sphere: 'create_sky_sphere', fog_volume: 'create_fog_volume', time_of_day_system: 'create_time_of_day_system' },
  },
  // weather
  {
    primary: 'configure_weather', selector: 'setting',
    summary: 'Configure weather effects: rain, snow, lightning, wind.',
    topics: ['rain', 'snow', 'lightning', 'wind', 'weather'],
    members: { rain: 'configure_rain_particles', snow: 'configure_snow_particles', lightning: 'configure_lightning', wind: 'configure_wind' },
  },
  // water
  {
    primary: 'create_water_body', selector: 'kind',
    summary: 'Create a water body: ocean, lake, river or custom.',
    topics: ['water body', 'ocean', 'lake', 'river'],
    members: byTarget('create_water_body_', ['create_water_body_ocean', 'create_water_body_lake', 'create_water_body_river', 'create_water_body_custom']),
  },
  {
    primary: 'configure_water', selector: 'setting',
    summary: 'Configure water: waves, material, collision.',
    topics: ['water waves', 'water material', 'water collision'],
    members: byTarget('configure_water_', ['configure_water_waves', 'configure_water_material', 'configure_water_collision']),
  },
];
