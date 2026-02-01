#!/usr/bin/env node
import path from 'node:path';
import { pathToFileURL } from 'node:url';
/**
 * World Building Tools Integration Tests
 * 
 * Tools: manage_lighting (61), build_environment (58), manage_volumes (41)
 * Total Actions: 160
 * Test Cases: ~320 (2x coverage per action)
 * 
 * Usage:
 *   node tests/category-tests/world-tools.test.mjs
 */

// ============================================================================
// TEST ASSET AUTO-CREATION
// ============================================================================
// This test suite automatically creates required assets during test execution.
// 
// SETUP tests (marked with 'SETUP:' prefix) create assets programmatically:
// - Line 311: SETUP: Create TestFoliage asset (uses build_environment/add_foliage_type)
// - Line 552: SETUP: Create SplineMeshBP blueprint (uses manage_asset/create_blueprint)
//
// These setup tests run before dependent tests and use the 'success|already exists'
// expectation pattern, making them idempotent (safe to run multiple times).
//
// If SETUP tests fail, subsequent tests that depend on those assets will fail
// with clear error messages indicating the asset creation failure.
// ============================================================================

import { runToolTests } from '../test-runner.mjs';

const TEST_FOLDER = '/Game/WorldToolsTest';

// ============================================================================
// MANAGE_LIGHTING (61 actions) - ~122 test cases
// ============================================================================
const manageLightingTests = [
  // === Light Creation (8 actions) ===
  // spawn_light
  { scenario: 'Lighting: spawn_light point success', toolName: 'manage_lighting', arguments: { action: 'spawn_light', name: 'TestPoint_01', lightType: 'Point', location: { x: 0, y: 0, z: 200 }, intensity: 5000 }, expected: 'success' },
  { scenario: 'Lighting: spawn_light invalid type', toolName: 'manage_lighting', arguments: { action: 'spawn_light', name: 'TestLight_Invalid', lightType: 'InvalidType', location: { x: 0, y: 0, z: 200 } }, expected: 'error|invalid' },
  
  // create_light
  { scenario: 'Lighting: create_light directional', toolName: 'manage_lighting', arguments: { action: 'create_light', name: 'DirLight_Test', lightType: 'Directional', location: { x: 0, y: 0, z: 1000 } }, expected: 'success' },
  { scenario: 'Lighting: create_light spot with params', toolName: 'manage_lighting', arguments: { action: 'create_light', name: 'SpotLight_Test', lightType: 'Spot', location: { x: 500, y: 0, z: 300 }, intensity: 8000, innerCone: 22.5, outerCone: 45 }, expected: 'success' },
  
  // spawn_sky_light
  { scenario: 'Lighting: spawn_sky_light success', toolName: 'manage_lighting', arguments: { action: 'spawn_sky_light', name: 'SkyLight_Test', location: { x: 0, y: 0, z: 500 } }, expected: 'success' },
  { scenario: 'Lighting: spawn_sky_light with cubemap', toolName: 'manage_lighting', arguments: { action: 'spawn_sky_light', name: 'SkyLight_Cubemap', location: { x: 0, y: 0, z: 500 }, sourceType: 'SpecifiedCubemap', cubemapPath: '/Engine/EngineMaterials/DefaultDiffuse' }, expected: 'success|not found' },
  
  // create_sky_light
  { scenario: 'Lighting: create_sky_light success', toolName: 'manage_lighting', arguments: { action: 'create_sky_light', name: 'SkyLight_Created', location: { x: 0, y: 0, z: 600 } }, expected: 'success' },
  { scenario: 'Lighting: create_sky_light recapture', toolName: 'manage_lighting', arguments: { action: 'create_sky_light', name: 'SkyLight_Recapture', location: { x: 100, y: 0, z: 600 }, recapture: true }, expected: 'success' },
  
  // ensure_single_sky_light
  { scenario: 'Lighting: ensure_single_sky_light success', toolName: 'manage_lighting', arguments: { action: 'ensure_single_sky_light' }, expected: 'success' },
  { scenario: 'Lighting: ensure_single_sky_light with location', toolName: 'manage_lighting', arguments: { action: 'ensure_single_sky_light', location: { x: 0, y: 0, z: 1000 } }, expected: 'success' },
  
  // create_lightmass_volume
  { scenario: 'Lighting: create_lightmass_volume success', toolName: 'manage_lighting', arguments: { action: 'create_lightmass_volume', name: 'LightmassVol_Test', location: { x: 0, y: 0, z: 0 }, extent: { x: 5000, y: 5000, z: 2000 } }, expected: 'success' },
  { scenario: 'Lighting: create_lightmass_volume no extent', toolName: 'manage_lighting', arguments: { action: 'create_lightmass_volume', name: 'LightmassVol_Default' }, expected: 'success' },
  
  // create_lighting_enabled_level
  { scenario: 'Lighting: create_lighting_enabled_level success', toolName: 'manage_lighting', arguments: { action: 'create_lighting_enabled_level', levelName: 'LightingTest_Level' }, expected: 'success|already exists' },
  { scenario: 'Lighting: create_lighting_enabled_level with template', toolName: 'manage_lighting', arguments: { action: 'create_lighting_enabled_level', levelName: 'LightingTest_Template', useTemplate: true }, expected: 'success|already exists' },
  
  // create_dynamic_light
  { scenario: 'Lighting: create_dynamic_light point', toolName: 'manage_lighting', arguments: { action: 'create_dynamic_light', name: 'DynamicPoint_Test', lightType: 'Point', location: { x: 200, y: 200, z: 200 } }, expected: 'success' },
  { scenario: 'Lighting: create_dynamic_light rect', toolName: 'manage_lighting', arguments: { action: 'create_dynamic_light', name: 'DynamicRect_Test', lightType: 'Rect', location: { x: 0, y: 500, z: 200 }, width: 100, height: 50 }, expected: 'success' },

  // === Global Illumination & Shadows (6 actions) ===
  // setup_global_illumination
  { scenario: 'Lighting: setup_global_illumination lumen', toolName: 'manage_lighting', arguments: { action: 'setup_global_illumination', method: 'LumenGI' }, expected: 'success' },
  { scenario: 'Lighting: setup_global_illumination lightmass', toolName: 'manage_lighting', arguments: { action: 'setup_global_illumination', method: 'Lightmass', bounces: 3 }, expected: 'success' },
  
  // configure_shadows
  { scenario: 'Lighting: configure_shadows success', toolName: 'manage_lighting', arguments: { action: 'configure_shadows', cascadedShadows: true, shadowDistance: 5000 }, expected: 'success' },
  { scenario: 'Lighting: configure_shadows ray traced', toolName: 'manage_lighting', arguments: { action: 'configure_shadows', rayTracedShadows: true, contactShadows: true }, expected: 'success|not supported' },
  
  // set_exposure
  { scenario: 'Lighting: set_exposure values', toolName: 'manage_lighting', arguments: { action: 'set_exposure', compensationValue: 1.0, minBrightness: 0.5, maxBrightness: 4.0 }, expected: 'success' },
  { scenario: 'Lighting: set_exposure auto', toolName: 'manage_lighting', arguments: { action: 'set_exposure', compensationValue: 0 }, expected: 'success' },
  
  // set_ambient_occlusion
  { scenario: 'Lighting: set_ambient_occlusion enable', toolName: 'manage_lighting', arguments: { action: 'set_ambient_occlusion', enabled: true }, expected: 'success' },
  { scenario: 'Lighting: set_ambient_occlusion disable', toolName: 'manage_lighting', arguments: { action: 'set_ambient_occlusion', enabled: false }, expected: 'success' },
  
  // setup_volumetric_fog
  { scenario: 'Lighting: setup_volumetric_fog success', toolName: 'manage_lighting', arguments: { action: 'setup_volumetric_fog', enabled: true, density: 0.5 }, expected: 'success' },
  { scenario: 'Lighting: setup_volumetric_fog with height', toolName: 'manage_lighting', arguments: { action: 'setup_volumetric_fog', enabled: true, fogHeight: 1000, scatteringIntensity: 0.8 }, expected: 'success' },
  
  // build_lighting
  { scenario: 'Lighting: build_lighting preview', toolName: 'manage_lighting', arguments: { action: 'build_lighting', quality: 'Preview' }, expected: 'success|skipped' },
  { scenario: 'Lighting: build_lighting production', toolName: 'manage_lighting', arguments: { action: 'build_lighting', quality: 'Production', buildReflectionCaptures: true }, expected: 'success|skipped' },
  
  // list_light_types
  { scenario: 'Lighting: list_light_types success', toolName: 'manage_lighting', arguments: { action: 'list_light_types' }, expected: 'success' },
  { scenario: 'Lighting: list_light_types query', toolName: 'manage_lighting', arguments: { action: 'list_light_types' }, expected: 'Directional|Point|Spot|Rect' },

  // === Post-Process (10 actions) ===
  // create_post_process_volume
  { scenario: 'Lighting: create_post_process_volume infinite', toolName: 'manage_lighting', arguments: { action: 'create_post_process_volume', volumeName: 'PPV_Global', infinite: true }, expected: 'success' },
  { scenario: 'Lighting: create_post_process_volume bounded', toolName: 'manage_lighting', arguments: { action: 'create_post_process_volume', volumeName: 'PPV_Local', location: { x: 0, y: 0, z: 200 }, extent: { x: 500, y: 500, z: 500 }, infinite: false }, expected: 'success' },
  
  // configure_pp_blend
  { scenario: 'Lighting: configure_pp_blend success', toolName: 'manage_lighting', arguments: { action: 'configure_pp_blend', volumeName: 'PPV_Global', blendRadius: 100, blendWeight: 1.0 }, expected: 'success|not found' },
  { scenario: 'Lighting: configure_pp_blend not found', toolName: 'manage_lighting', arguments: { action: 'configure_pp_blend', volumeName: 'NonExistent_PPV', blendRadius: 50 }, expected: 'not found|error' },
  
  // configure_pp_priority
  { scenario: 'Lighting: configure_pp_priority success', toolName: 'manage_lighting', arguments: { action: 'configure_pp_priority', volumeName: 'PPV_Global', priority: 10 }, expected: 'success|not found' },
  { scenario: 'Lighting: configure_pp_priority high', toolName: 'manage_lighting', arguments: { action: 'configure_pp_priority', volumeName: 'PPV_Local', priority: 100 }, expected: 'success|not found' },
  
  // get_post_process_settings
  { scenario: 'Lighting: get_post_process_settings success', toolName: 'manage_lighting', arguments: { action: 'get_post_process_settings', volumeName: 'PPV_Global' }, expected: 'success|not found' },
  { scenario: 'Lighting: get_post_process_settings not found', toolName: 'manage_lighting', arguments: { action: 'get_post_process_settings', volumeName: 'NonExistent_PPV' }, expected: 'not found|error' },
  
  // configure_bloom
  { scenario: 'Lighting: configure_bloom success', toolName: 'manage_lighting', arguments: { action: 'configure_bloom', volumeName: 'PPV_Global', bloomIntensity: 1.5, bloomThreshold: 1.0 }, expected: 'success' },
  { scenario: 'Lighting: configure_bloom disabled', toolName: 'manage_lighting', arguments: { action: 'configure_bloom', volumeName: 'PPV_Global', bloomIntensity: 0 }, expected: 'success' },
  
  // configure_dof
  { scenario: 'Lighting: configure_dof success', toolName: 'manage_lighting', arguments: { action: 'configure_dof', volumeName: 'PPV_Global', focalDistance: 1000, focalRegion: 500 }, expected: 'success' },
  { scenario: 'Lighting: configure_dof cinematic', toolName: 'manage_lighting', arguments: { action: 'configure_dof', volumeName: 'PPV_Global', focalDistance: 500, focalRegion: 100 }, expected: 'success' },
  
  // configure_motion_blur
  { scenario: 'Lighting: configure_motion_blur enable', toolName: 'manage_lighting', arguments: { action: 'configure_motion_blur', volumeName: 'PPV_Global', motionBlurAmount: 0.5 }, expected: 'success' },
  { scenario: 'Lighting: configure_motion_blur disable', toolName: 'manage_lighting', arguments: { action: 'configure_motion_blur', volumeName: 'PPV_Global', motionBlurAmount: 0 }, expected: 'success' },
  
  // configure_color_grading
  { scenario: 'Lighting: configure_color_grading success', toolName: 'manage_lighting', arguments: { action: 'configure_color_grading', volumeName: 'PPV_Global', globalSaturation: { r: 1.0, g: 1.0, b: 1.0 } }, expected: 'success' },
  { scenario: 'Lighting: configure_color_grading desaturated', toolName: 'manage_lighting', arguments: { action: 'configure_color_grading', volumeName: 'PPV_Global', globalSaturation: { r: 0.5, g: 0.5, b: 0.5 } }, expected: 'success' },
  
  // configure_white_balance
  { scenario: 'Lighting: configure_white_balance warm', toolName: 'manage_lighting', arguments: { action: 'configure_white_balance', volumeName: 'PPV_Global', whiteTemp: 7000 }, expected: 'success' },
  { scenario: 'Lighting: configure_white_balance cool', toolName: 'manage_lighting', arguments: { action: 'configure_white_balance', volumeName: 'PPV_Global', whiteTemp: 4000 }, expected: 'success' },
  
  // configure_vignette
  { scenario: 'Lighting: configure_vignette success', toolName: 'manage_lighting', arguments: { action: 'configure_vignette', volumeName: 'PPV_Global', vignetteIntensity: 0.5 }, expected: 'success' },
  { scenario: 'Lighting: configure_vignette disabled', toolName: 'manage_lighting', arguments: { action: 'configure_vignette', volumeName: 'PPV_Global', vignetteIntensity: 0 }, expected: 'success' },

  // === Post-Process Effects (4 actions) ===
  // configure_chromatic_aberration
  { scenario: 'Lighting: configure_chromatic_aberration enable', toolName: 'manage_lighting', arguments: { action: 'configure_chromatic_aberration', chromaticAberrationIntensity: 1.0 }, expected: 'success' },
  { scenario: 'Lighting: configure_chromatic_aberration disable', toolName: 'manage_lighting', arguments: { action: 'configure_chromatic_aberration', chromaticAberrationIntensity: 0 }, expected: 'success' },
  
  // configure_film_grain
  { scenario: 'Lighting: configure_film_grain enable', toolName: 'manage_lighting', arguments: { action: 'configure_film_grain', filmGrainIntensity: 0.5 }, expected: 'success' },
  { scenario: 'Lighting: configure_film_grain cinematic', toolName: 'manage_lighting', arguments: { action: 'configure_film_grain', filmGrainIntensity: 0.3 }, expected: 'success' },
  
  // configure_lens_flares
  { scenario: 'Lighting: configure_lens_flares enable', toolName: 'manage_lighting', arguments: { action: 'configure_lens_flares', enabled: true }, expected: 'success' },
  { scenario: 'Lighting: configure_lens_flares with intensity', toolName: 'manage_lighting', arguments: { action: 'configure_lens_flares', enabled: true, intensity: 1.5 }, expected: 'success' },

  // === Reflection Captures (4 actions) ===
  // create_sphere_reflection_capture
  { scenario: 'Lighting: create_sphere_reflection_capture success', toolName: 'manage_lighting', arguments: { action: 'create_sphere_reflection_capture', name: 'SphereCapture_Test', location: { x: 0, y: 0, z: 200 }, influenceRadius: 1000 }, expected: 'success' },
  { scenario: 'Lighting: create_sphere_reflection_capture small', toolName: 'manage_lighting', arguments: { action: 'create_sphere_reflection_capture', name: 'SphereCapture_Small', location: { x: 500, y: 0, z: 200 }, influenceRadius: 200 }, expected: 'success' },
  
  // create_box_reflection_capture
  { scenario: 'Lighting: create_box_reflection_capture success', toolName: 'manage_lighting', arguments: { action: 'create_box_reflection_capture', name: 'BoxCapture_Test', location: { x: 0, y: 500, z: 200 }, boxExtent: { x: 500, y: 500, z: 300 } }, expected: 'success' },
  { scenario: 'Lighting: create_box_reflection_capture room', toolName: 'manage_lighting', arguments: { action: 'create_box_reflection_capture', name: 'BoxCapture_Room', location: { x: 0, y: 0, z: 150 }, boxExtent: { x: 300, y: 300, z: 150 } }, expected: 'success' },
  
  // create_planar_reflection
  { scenario: 'Lighting: create_planar_reflection success', toolName: 'manage_lighting', arguments: { action: 'create_planar_reflection', name: 'PlanarRefl_Test', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'Lighting: create_planar_reflection water', toolName: 'manage_lighting', arguments: { action: 'create_planar_reflection', name: 'PlanarRefl_Water', location: { x: 0, y: 0, z: -10 }, screenPercentage: 50 }, expected: 'success' },
  
  // recapture_scene
  { scenario: 'Lighting: recapture_scene success', toolName: 'manage_lighting', arguments: { action: 'recapture_scene' }, expected: 'success' },
  { scenario: 'Lighting: recapture_scene all', toolName: 'manage_lighting', arguments: { action: 'recapture_scene' }, expected: 'success|recaptured' },

  // === Ray Tracing (5 actions) ===
  // configure_ray_traced_shadows
  { scenario: 'Lighting: configure_ray_traced_shadows enable', toolName: 'manage_lighting', arguments: { action: 'configure_ray_traced_shadows', rayTracedShadowsEnabled: true }, expected: 'success|not supported' },
  { scenario: 'Lighting: configure_ray_traced_shadows disable', toolName: 'manage_lighting', arguments: { action: 'configure_ray_traced_shadows', rayTracedShadowsEnabled: false }, expected: 'success' },
  
  // configure_ray_traced_gi
  { scenario: 'Lighting: configure_ray_traced_gi enable', toolName: 'manage_lighting', arguments: { action: 'configure_ray_traced_gi', rayTracedGIEnabled: true }, expected: 'success|not supported' },
  { scenario: 'Lighting: configure_ray_traced_gi disable', toolName: 'manage_lighting', arguments: { action: 'configure_ray_traced_gi', rayTracedGIEnabled: false }, expected: 'success' },
  
  // configure_ray_traced_reflections
  { scenario: 'Lighting: configure_ray_traced_reflections enable', toolName: 'manage_lighting', arguments: { action: 'configure_ray_traced_reflections', rayTracedReflectionsEnabled: true }, expected: 'success|not supported' },
  { scenario: 'Lighting: configure_ray_traced_reflections disable', toolName: 'manage_lighting', arguments: { action: 'configure_ray_traced_reflections', rayTracedReflectionsEnabled: false }, expected: 'success' },
  
  // configure_ray_traced_ao
  { scenario: 'Lighting: configure_ray_traced_ao enable', toolName: 'manage_lighting', arguments: { action: 'configure_ray_traced_ao', rayTracedAOEnabled: true }, expected: 'success|not supported' },
  { scenario: 'Lighting: configure_ray_traced_ao disable', toolName: 'manage_lighting', arguments: { action: 'configure_ray_traced_ao', rayTracedAOEnabled: false }, expected: 'success' },
  
  // configure_path_tracing
  { scenario: 'Lighting: configure_path_tracing enable', toolName: 'manage_lighting', arguments: { action: 'configure_path_tracing', pathTracingEnabled: true }, expected: 'success|not supported' },
  { scenario: 'Lighting: configure_path_tracing disable', toolName: 'manage_lighting', arguments: { action: 'configure_path_tracing', pathTracingEnabled: false }, expected: 'success' },

  // === Scene Capture (3 actions) ===
  // create_scene_capture_2d
  { scenario: 'Lighting: create_scene_capture_2d success', toolName: 'manage_lighting', arguments: { action: 'create_scene_capture_2d', name: 'SceneCapture2D_Test', location: { x: 0, y: 0, z: 300 }, fov: 90, captureResolution: 512 }, expected: 'success' },
  { scenario: 'Lighting: create_scene_capture_2d high res', toolName: 'manage_lighting', arguments: { action: 'create_scene_capture_2d', name: 'SceneCapture2D_HiRes', location: { x: 0, y: 0, z: 300 }, captureResolution: 2048 }, expected: 'success' },
  
  // create_scene_capture_cube
  { scenario: 'Lighting: create_scene_capture_cube success', toolName: 'manage_lighting', arguments: { action: 'create_scene_capture_cube', name: 'SceneCaptureCube_Test', location: { x: 0, y: 0, z: 200 } }, expected: 'success' },
  { scenario: 'Lighting: create_scene_capture_cube high res', toolName: 'manage_lighting', arguments: { action: 'create_scene_capture_cube', name: 'SceneCaptureCube_HiRes', location: { x: 0, y: 0, z: 200 }, captureResolution: 1024 }, expected: 'success' },
  
  // capture_scene
  { scenario: 'Lighting: capture_scene 2d', toolName: 'manage_lighting', arguments: { action: 'capture_scene', actorName: 'SceneCapture2D_Test' }, expected: 'success|not found' },
  { scenario: 'Lighting: capture_scene not found', toolName: 'manage_lighting', arguments: { action: 'capture_scene', actorName: 'NonExistent_Capture' }, expected: 'not found|error' },

  // === Light Channels (2 actions) ===
  // set_light_channel
  { scenario: 'Lighting: set_light_channel success', toolName: 'manage_lighting', arguments: { action: 'set_light_channel', lightActorName: 'DirLight_Test', channel0: true, channel1: false, channel2: false }, expected: 'success|not found' },
  { scenario: 'Lighting: set_light_channel multi', toolName: 'manage_lighting', arguments: { action: 'set_light_channel', lightActorName: 'SpotLight_Test', channel0: true, channel1: true, channel2: false }, expected: 'success|not found' },
  
  // set_actor_light_channel
  { scenario: 'Lighting: set_actor_light_channel success', toolName: 'manage_lighting', arguments: { action: 'set_actor_light_channel', actorName: 'TestActor', channel0: true, channel1: false, channel2: false }, expected: 'success|not found' },
  { scenario: 'Lighting: set_actor_light_channel not found', toolName: 'manage_lighting', arguments: { action: 'set_actor_light_channel', actorName: 'NonExistent_Actor', channel0: true, channel1: true, channel2: true }, expected: 'not found|error' },

  // === Lightmass Settings (4 actions) ===
  // configure_lightmass_settings
  { scenario: 'Lighting: configure_lightmass_settings success', toolName: 'manage_lighting', arguments: { action: 'configure_lightmass_settings', numIndirectBounces: 3, indirectLightingQuality: 1.0 }, expected: 'success' },
  { scenario: 'Lighting: configure_lightmass_settings high quality', toolName: 'manage_lighting', arguments: { action: 'configure_lightmass_settings', numIndirectBounces: 5, indirectLightingQuality: 2.0 }, expected: 'success' },
  
  // build_lighting_quality
  { scenario: 'Lighting: build_lighting_quality preview', toolName: 'manage_lighting', arguments: { action: 'build_lighting_quality', quality: 'Preview' }, expected: 'success|skipped' },
  { scenario: 'Lighting: build_lighting_quality medium', toolName: 'manage_lighting', arguments: { action: 'build_lighting_quality', quality: 'Medium' }, expected: 'success|skipped' },
  
  // configure_indirect_lighting_cache
  { scenario: 'Lighting: configure_indirect_lighting_cache success', toolName: 'manage_lighting', arguments: { action: 'configure_indirect_lighting_cache', enabled: true }, expected: 'success' },
  { scenario: 'Lighting: configure_indirect_lighting_cache disable', toolName: 'manage_lighting', arguments: { action: 'configure_indirect_lighting_cache', enabled: false }, expected: 'success' },
  
  // configure_volumetric_lightmap
  { scenario: 'Lighting: configure_volumetric_lightmap enable', toolName: 'manage_lighting', arguments: { action: 'configure_volumetric_lightmap', volumetricLightmapEnabled: true }, expected: 'success' },
  { scenario: 'Lighting: configure_volumetric_lightmap disable', toolName: 'manage_lighting', arguments: { action: 'configure_volumetric_lightmap', volumetricLightmapEnabled: false }, expected: 'success' },

  // === Lumen (5 actions) ===
  // configure_lumen_gi
  { scenario: 'Lighting: configure_lumen_gi success', toolName: 'manage_lighting', arguments: { action: 'configure_lumen_gi', lumenQuality: 1.0 }, expected: 'success' },
  { scenario: 'Lighting: configure_lumen_gi high quality', toolName: 'manage_lighting', arguments: { action: 'configure_lumen_gi', lumenQuality: 2.0, lumenDetailTrace: true }, expected: 'success' },
  
  // set_lumen_reflections
  { scenario: 'Lighting: set_lumen_reflections success', toolName: 'manage_lighting', arguments: { action: 'set_lumen_reflections', lumenReflectionQuality: 1.0 }, expected: 'success' },
  { scenario: 'Lighting: set_lumen_reflections high', toolName: 'manage_lighting', arguments: { action: 'set_lumen_reflections', lumenReflectionQuality: 2.0, lumenUpdateSpeed: 0.5 }, expected: 'success' },
  
  // tune_lumen_performance
  { scenario: 'Lighting: tune_lumen_performance success', toolName: 'manage_lighting', arguments: { action: 'tune_lumen_performance', lumenFinalGatherQuality: 1.0 }, expected: 'success' },
  { scenario: 'Lighting: tune_lumen_performance fast', toolName: 'manage_lighting', arguments: { action: 'tune_lumen_performance', lumenFinalGatherQuality: 0.5 }, expected: 'success' },
  
  // create_lumen_volume
  { scenario: 'Lighting: create_lumen_volume success', toolName: 'manage_lighting', arguments: { action: 'create_lumen_volume', name: 'LumenVol_Test', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  { scenario: 'Lighting: create_lumen_volume with extent', toolName: 'manage_lighting', arguments: { action: 'create_lumen_volume', name: 'LumenVol_Large', location: { x: 0, y: 0, z: 0 }, extent: { x: 5000, y: 5000, z: 2000 } }, expected: 'success' },
  
  // set_virtual_shadow_maps
  { scenario: 'Lighting: set_virtual_shadow_maps enable', toolName: 'manage_lighting', arguments: { action: 'set_virtual_shadow_maps', virtualShadowMapResolution: 2048 }, expected: 'success' },
  { scenario: 'Lighting: set_virtual_shadow_maps quality', toolName: 'manage_lighting', arguments: { action: 'set_virtual_shadow_maps', virtualShadowMapResolution: 4096, virtualShadowMapQuality: 2 }, expected: 'success' },

  // === MegaLights & Advanced (10 actions) ===
  // configure_megalights_scene
  { scenario: 'Lighting: configure_megalights_scene enable', toolName: 'manage_lighting', arguments: { action: 'configure_megalights_scene', megalightsEnabled: true }, expected: 'success|not supported' },
  { scenario: 'Lighting: configure_megalights_scene with budget', toolName: 'manage_lighting', arguments: { action: 'configure_megalights_scene', megalightsEnabled: true, megalightsBudget: 128 }, expected: 'success|not supported' },
  
  // get_megalights_budget
  { scenario: 'Lighting: get_megalights_budget success', toolName: 'manage_lighting', arguments: { action: 'get_megalights_budget' }, expected: 'success|not supported' },
  { scenario: 'Lighting: get_megalights_budget query', toolName: 'manage_lighting', arguments: { action: 'get_megalights_budget' }, expected: 'success|budget' },
  
  // optimize_lights_for_megalights
  { scenario: 'Lighting: optimize_lights_for_megalights success', toolName: 'manage_lighting', arguments: { action: 'optimize_lights_for_megalights' }, expected: 'success|not supported' },
  { scenario: 'Lighting: optimize_lights_for_megalights query', toolName: 'manage_lighting', arguments: { action: 'optimize_lights_for_megalights' }, expected: 'success|optimized' },
  
  // configure_gi_settings
  { scenario: 'Lighting: configure_gi_settings lumen', toolName: 'manage_lighting', arguments: { action: 'configure_gi_settings', giMethod: 'lumen' }, expected: 'success' },
  { scenario: 'Lighting: configure_gi_settings screenspace', toolName: 'manage_lighting', arguments: { action: 'configure_gi_settings', giMethod: 'screenspace' }, expected: 'success' },
  
  // bake_lighting_preview
  { scenario: 'Lighting: bake_lighting_preview success', toolName: 'manage_lighting', arguments: { action: 'bake_lighting_preview', previewBake: true }, expected: 'success|skipped' },
  { scenario: 'Lighting: bake_lighting_preview full', toolName: 'manage_lighting', arguments: { action: 'bake_lighting_preview', previewBake: false }, expected: 'success|skipped' },
  
  // get_light_complexity
  { scenario: 'Lighting: get_light_complexity success', toolName: 'manage_lighting', arguments: { action: 'get_light_complexity' }, expected: 'success' },
  { scenario: 'Lighting: get_light_complexity query', toolName: 'manage_lighting', arguments: { action: 'get_light_complexity' }, expected: 'success|complexity' },
  
  // configure_volumetric_fog
  { scenario: 'Lighting: configure_volumetric_fog success', toolName: 'manage_lighting', arguments: { action: 'configure_volumetric_fog', density: 0.02, fogInscatteringColor: { r: 0.5, g: 0.6, b: 0.8 } }, expected: 'success' },
  { scenario: 'Lighting: configure_volumetric_fog with params', toolName: 'manage_lighting', arguments: { action: 'configure_volumetric_fog', density: 0.05, fogExtinctionScale: 1.0, fogViewDistance: 6000 }, expected: 'success' },
  
  // create_light_batch
  { scenario: 'Lighting: create_light_batch success', toolName: 'manage_lighting', arguments: { action: 'create_light_batch', lights: [{ name: 'BatchLight_1', lightType: 'Point', location: { x: 0, y: 0, z: 200 } }, { name: 'BatchLight_2', lightType: 'Point', location: { x: 500, y: 0, z: 200 } }] }, expected: 'success' },
  { scenario: 'Lighting: create_light_batch empty', toolName: 'manage_lighting', arguments: { action: 'create_light_batch', lights: [] }, expected: 'success|empty' },
  
  // configure_shadow_settings
  { scenario: 'Lighting: configure_shadow_settings success', toolName: 'manage_lighting', arguments: { action: 'configure_shadow_settings', shadowBias: 0.5, shadowResolution: 2048 }, expected: 'success' },
  { scenario: 'Lighting: configure_shadow_settings cascades', toolName: 'manage_lighting', arguments: { action: 'configure_shadow_settings', dynamicShadowCascades: 4, shadowSlopeBias: 0.5 }, expected: 'success' },
  
  // validate_lighting_setup
  { scenario: 'Lighting: validate_lighting_setup success', toolName: 'manage_lighting', arguments: { action: 'validate_lighting_setup' }, expected: 'success' },
  { scenario: 'Lighting: validate_lighting_setup full', toolName: 'manage_lighting', arguments: { action: 'validate_lighting_setup', validatePerformance: true, validateOverlap: true, validateShadows: true }, expected: 'success' },
];

// ============================================================================
// BUILD_ENVIRONMENT (58 actions) - ~116 test cases
// ============================================================================
const buildEnvironmentTests = [
  // === SETUP: Create Required Test Assets ===
  // Create TestFoliage asset for foliage tests
  { scenario: 'SETUP: Create TestFoliage asset', toolName: 'build_environment', arguments: { action: 'add_foliage_type', name: 'TestFoliage', meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|already exists' },
  
  // === Landscape (20 actions) ===
  // create_landscape
  { scenario: 'Environment: create_landscape success', toolName: 'build_environment', arguments: { action: 'create_landscape', name: 'Landscape_Test', location: { x: 0, y: 0, z: 0 } }, expected: 'success|already exists' },
  { scenario: 'Environment: create_landscape with size', toolName: 'build_environment', arguments: { action: 'create_landscape', name: 'Landscape_Large', location: { x: 0, y: 0, z: 0 }, sizeX: 8129, sizeY: 8129 }, expected: 'success|already exists' },
  
  // sculpt
  { scenario: 'Environment: sculpt success', toolName: 'build_environment', arguments: { action: 'sculpt', landscapeName: 'Landscape_Test', location: { x: 500, y: 500, z: 0 }, radius: 500, strength: 0.5 }, expected: 'success|not found' },
  { scenario: 'Environment: sculpt not found', toolName: 'build_environment', arguments: { action: 'sculpt', landscapeName: 'NonExistent_Landscape', location: { x: 0, y: 0, z: 0 }, radius: 100 }, expected: 'not found|error' },
  
  // sculpt_landscape
  { scenario: 'Environment: sculpt_landscape success', toolName: 'build_environment', arguments: { action: 'sculpt_landscape', landscapeName: 'Landscape_Test', location: { x: 1000, y: 1000, z: 0 }, radius: 300, strength: 0.8 }, expected: 'success|not found' },
  { scenario: 'Environment: sculpt_landscape with tool', toolName: 'build_environment', arguments: { action: 'sculpt_landscape', landscapeName: 'Landscape_Test', location: { x: 500, y: 500, z: 0 }, tool: 'Smooth', radius: 500 }, expected: 'success|not found' },
  
  // add_foliage
  { scenario: 'Environment: add_foliage success', toolName: 'build_environment', arguments: { action: 'add_foliage', foliageType: 'TestFoliage', meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success' },
  { scenario: 'Environment: add_foliage with settings', toolName: 'build_environment', arguments: { action: 'add_foliage', foliageType: 'TreeFoliage', meshPath: '/Engine/BasicShapes/Cylinder', density: 0.5, minScale: 0.8, maxScale: 1.2 }, expected: 'success' },
  
  // paint_foliage
  { scenario: 'Environment: paint_foliage success', toolName: 'build_environment', arguments: { action: 'paint_foliage', foliageType: 'TestFoliage', location: { x: 0, y: 0, z: 0 }, radius: 500, density: 0.5 }, expected: 'success|not found' },
  { scenario: 'Environment: paint_foliage not found', toolName: 'build_environment', arguments: { action: 'paint_foliage', foliageType: 'NonExistent_Foliage', location: { x: 0, y: 0, z: 0 }, radius: 100 }, expected: 'not found|error' },
  
  // create_procedural_terrain
  { scenario: 'Environment: create_procedural_terrain success', toolName: 'build_environment', arguments: { action: 'create_procedural_terrain', name: 'ProceduralTerrain_Test', seed: 12345 }, expected: 'success' },
  { scenario: 'Environment: create_procedural_terrain with size', toolName: 'build_environment', arguments: { action: 'create_procedural_terrain', name: 'ProceduralTerrain_Large', seed: 54321, sizeX: 4096, sizeY: 4096 }, expected: 'success' },
  
  // create_procedural_foliage
  { scenario: 'Environment: create_procedural_foliage success', toolName: 'build_environment', arguments: { action: 'create_procedural_foliage', name: 'ProcFoliage_Test' }, expected: 'success' },
  { scenario: 'Environment: create_procedural_foliage with types', toolName: 'build_environment', arguments: { action: 'create_procedural_foliage', name: 'ProcFoliage_Forest', foliageTypes: [{ meshPath: '/Engine/BasicShapes/Cylinder' }] }, expected: 'success' },
  
  // add_foliage_instances
  { scenario: 'Environment: add_foliage_instances success', toolName: 'build_environment', arguments: { action: 'add_foliage_instances', foliageType: 'TestFoliage', locations: [{ x: 0, y: 0, z: 0 }, { x: 100, y: 0, z: 0 }] }, expected: 'success|not found' },
  { scenario: 'Environment: add_foliage_instances with transforms', toolName: 'build_environment', arguments: { action: 'add_foliage_instances', foliageType: 'TestFoliage', transforms: [{ location: { x: 0, y: 0, z: 0 }, rotation: { pitch: 0, yaw: 45, roll: 0 }, scale: { x: 1, y: 1, z: 1 } }] }, expected: 'success|not found' },
  
  // get_foliage_instances
  { scenario: 'Environment: get_foliage_instances success', toolName: 'build_environment', arguments: { action: 'get_foliage_instances', foliageType: 'TestFoliage' }, expected: 'success|not found' },
  { scenario: 'Environment: get_foliage_instances with bounds', toolName: 'build_environment', arguments: { action: 'get_foliage_instances', foliageType: 'TestFoliage', bounds: { min: { x: -1000, y: -1000, z: -100 }, max: { x: 1000, y: 1000, z: 1000 } } }, expected: 'success|not found' },
  
  // remove_foliage
  { scenario: 'Environment: remove_foliage success', toolName: 'build_environment', arguments: { action: 'remove_foliage', foliageType: 'TestFoliage', location: { x: 0, y: 0, z: 0 }, radius: 500 }, expected: 'success|not found' },
  { scenario: 'Environment: remove_foliage all', toolName: 'build_environment', arguments: { action: 'remove_foliage', foliageType: 'TestFoliage' }, expected: 'success|not found' },
  
  // paint_landscape
  { scenario: 'Environment: paint_landscape success', toolName: 'build_environment', arguments: { action: 'paint_landscape', landscapeName: 'Landscape_Test', layerName: 'Grass', location: { x: 500, y: 500, z: 0 }, radius: 300 }, expected: 'success|not found' },
  { scenario: 'Environment: paint_landscape erase', toolName: 'build_environment', arguments: { action: 'paint_landscape', landscapeName: 'Landscape_Test', layerName: 'Grass', location: { x: 500, y: 500, z: 0 }, radius: 300, eraseMode: true }, expected: 'success|not found' },
  
  // paint_landscape_layer
  { scenario: 'Environment: paint_landscape_layer success', toolName: 'build_environment', arguments: { action: 'paint_landscape_layer', landscapeName: 'Landscape_Test', layerName: 'Rock', location: { x: 1000, y: 1000, z: 0 }, brushSize: 500 }, expected: 'success|not found' },
  { scenario: 'Environment: paint_landscape_layer with strength', toolName: 'build_environment', arguments: { action: 'paint_landscape_layer', landscapeName: 'Landscape_Test', layerName: 'Dirt', location: { x: 0, y: 1000, z: 0 }, brushSize: 300, strength: 0.7 }, expected: 'success|not found' },
  
  // modify_heightmap
  { scenario: 'Environment: modify_heightmap success', toolName: 'build_environment', arguments: { action: 'modify_heightmap', landscapeName: 'Landscape_Test', heightData: [100, 150, 200], minX: 0, minY: 0, maxX: 2, maxY: 0 }, expected: 'success|not found' },
  { scenario: 'Environment: modify_heightmap with normals', toolName: 'build_environment', arguments: { action: 'modify_heightmap', landscapeName: 'Landscape_Test', heightData: [100, 150, 200, 250], minX: 0, minY: 0, maxX: 1, maxY: 1, updateNormals: true }, expected: 'success|not found' },
  
  // set_landscape_material
  { scenario: 'Environment: set_landscape_material success', toolName: 'build_environment', arguments: { action: 'set_landscape_material', landscapeName: 'Landscape_Test', materialPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },
  { scenario: 'Environment: set_landscape_material not found', toolName: 'build_environment', arguments: { action: 'set_landscape_material', landscapeName: 'NonExistent_Landscape', materialPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'not found|error' },
  
  // create_landscape_grass_type
  { scenario: 'Environment: create_landscape_grass_type success', toolName: 'build_environment', arguments: { action: 'create_landscape_grass_type', name: 'GrassType_Test', path: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Environment: create_landscape_grass_type with mesh', toolName: 'build_environment', arguments: { action: 'create_landscape_grass_type', name: 'GrassType_Custom', path: TEST_FOLDER, meshPath: '/Engine/BasicShapes/Plane' }, expected: 'success' },
  
  // generate_lods
  { scenario: 'Environment: generate_lods success', toolName: 'build_environment', arguments: { action: 'generate_lods', landscapeName: 'Landscape_Test' }, expected: 'success|not found' },
  { scenario: 'Environment: generate_lods not found', toolName: 'build_environment', arguments: { action: 'generate_lods', landscapeName: 'NonExistent_Landscape' }, expected: 'not found|error' },
  
  // bake_lightmap
  { scenario: 'Environment: bake_lightmap success', toolName: 'build_environment', arguments: { action: 'bake_lightmap', landscapeName: 'Landscape_Test' }, expected: 'success|not found|skipped' },
  { scenario: 'Environment: bake_lightmap not found', toolName: 'build_environment', arguments: { action: 'bake_lightmap', landscapeName: 'NonExistent_Landscape' }, expected: 'not found|error' },
  
  // export_snapshot
  { scenario: 'Environment: export_snapshot success', toolName: 'build_environment', arguments: { action: 'export_snapshot', landscapeName: 'Landscape_Test', path: 'C:/Temp', filename: 'landscape_export' }, expected: 'success|not found' },
  { scenario: 'Environment: export_snapshot not found', toolName: 'build_environment', arguments: { action: 'export_snapshot', landscapeName: 'NonExistent_Landscape', path: 'C:/Temp' }, expected: 'not found|error' },
  
  // import_snapshot
  { scenario: 'Environment: import_snapshot success', toolName: 'build_environment', arguments: { action: 'import_snapshot', landscapeName: 'Landscape_Test', path: 'C:/Temp/landscape_export.png' }, expected: 'success|not found' },
  { scenario: 'Environment: import_snapshot file not found', toolName: 'build_environment', arguments: { action: 'import_snapshot', landscapeName: 'Landscape_Test', path: 'C:/Temp/nonexistent.png' }, expected: 'not found|error' },
  
  // delete
  { scenario: 'Environment: delete landscape', toolName: 'build_environment', arguments: { action: 'delete', name: 'Landscape_ToDelete' }, expected: 'success|not found' },
  { scenario: 'Environment: delete not found', toolName: 'build_environment', arguments: { action: 'delete', name: 'NonExistent_Object' }, expected: 'not found|error' },

  // === Sky & Atmosphere (6 actions) ===
  // create_sky_sphere
  { scenario: 'Environment: create_sky_sphere success', toolName: 'build_environment', arguments: { action: 'create_sky_sphere', name: 'SkySphere_Test' }, expected: 'success' },
  { scenario: 'Environment: create_sky_sphere with location', toolName: 'build_environment', arguments: { action: 'create_sky_sphere', name: 'SkySphere_Custom', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  
  // create_sky_atmosphere
  { scenario: 'Environment: create_sky_atmosphere success', toolName: 'build_environment', arguments: { action: 'create_sky_atmosphere', name: 'SkyAtmo_Test' }, expected: 'success' },
  { scenario: 'Environment: create_sky_atmosphere with location', toolName: 'build_environment', arguments: { action: 'create_sky_atmosphere', name: 'SkyAtmo_Custom', location: { x: 0, y: 0, z: 0 } }, expected: 'success' },
  
  // configure_sky_atmosphere
  { scenario: 'Environment: configure_sky_atmosphere success', toolName: 'build_environment', arguments: { action: 'configure_sky_atmosphere', mieAnisotropy: 0.8, rayleighScatteringScale: 1.0 }, expected: 'success' },
  { scenario: 'Environment: configure_sky_atmosphere sunset', toolName: 'build_environment', arguments: { action: 'configure_sky_atmosphere', mieAnisotropy: 0.6, mieScatteringScale: 0.03, atmosphereHeight: 60 }, expected: 'success' },

  // === Fog (4 actions) ===
  // create_fog_volume
  { scenario: 'Environment: create_fog_volume success', toolName: 'build_environment', arguments: { action: 'create_fog_volume', name: 'FogVolume_Test', location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
  { scenario: 'Environment: create_fog_volume with extent', toolName: 'build_environment', arguments: { action: 'create_fog_volume', name: 'FogVolume_Large', location: { x: 0, y: 0, z: 0 }, extent: { x: 5000, y: 5000, z: 500 } }, expected: 'success' },
  
  // create_exponential_height_fog
  { scenario: 'Environment: create_exponential_height_fog success', toolName: 'build_environment', arguments: { action: 'create_exponential_height_fog', name: 'ExpFog_Test' }, expected: 'success' },
  { scenario: 'Environment: create_exponential_height_fog with density', toolName: 'build_environment', arguments: { action: 'create_exponential_height_fog', name: 'ExpFog_Dense', fogDensity: 0.05 }, expected: 'success' },
  
  // configure_exponential_height_fog
  { scenario: 'Environment: configure_exponential_height_fog success', toolName: 'build_environment', arguments: { action: 'configure_exponential_height_fog', fogDensity: 0.02, fogHeightFalloff: 0.2 }, expected: 'success' },
  { scenario: 'Environment: configure_exponential_height_fog volumetric', toolName: 'build_environment', arguments: { action: 'configure_exponential_height_fog', fogDensity: 0.03, volumetricFog: true, volumetricFogDistance: 6000 }, expected: 'success' },

  // === Clouds (2 actions) ===
  // create_volumetric_cloud
  { scenario: 'Environment: create_volumetric_cloud success', toolName: 'build_environment', arguments: { action: 'create_volumetric_cloud', name: 'VolumetricCloud_Test' }, expected: 'success' },
  { scenario: 'Environment: create_volumetric_cloud with settings', toolName: 'build_environment', arguments: { action: 'create_volumetric_cloud', name: 'VolumetricCloud_Custom', layerBottomAltitude: 5, layerHeight: 10 }, expected: 'success' },
  
  // configure_volumetric_cloud
  { scenario: 'Environment: configure_volumetric_cloud success', toolName: 'build_environment', arguments: { action: 'configure_volumetric_cloud', layerBottomAltitude: 5, layerHeight: 10 }, expected: 'success' },
  { scenario: 'Environment: configure_volumetric_cloud dramatic', toolName: 'build_environment', arguments: { action: 'configure_volumetric_cloud', layerBottomAltitude: 3, layerHeight: 15, tracingMaxDistance: 50 }, expected: 'success' },

  // === Time of Day (1 action) ===
  // set_time_of_day
  { scenario: 'Environment: set_time_of_day noon', toolName: 'build_environment', arguments: { action: 'set_time_of_day', hour: 12 }, expected: 'success' },
  { scenario: 'Environment: set_time_of_day sunset', toolName: 'build_environment', arguments: { action: 'set_time_of_day', time: 18.5 }, expected: 'success' },

  // === Water (14 actions) ===
  // create_water_body_ocean
  { scenario: 'Environment: create_water_body_ocean success', toolName: 'build_environment', arguments: { action: 'create_water_body_ocean', name: 'Ocean_Test', location: { x: 0, y: 0, z: -100 } }, expected: 'success|plugin not enabled' },
  { scenario: 'Environment: create_water_body_ocean large', toolName: 'build_environment', arguments: { action: 'create_water_body_ocean', name: 'Ocean_Large', location: { x: 0, y: 0, z: -200 } }, expected: 'success|plugin not enabled' },
  
  // create_water_body_lake
  { scenario: 'Environment: create_water_body_lake success', toolName: 'build_environment', arguments: { action: 'create_water_body_lake', name: 'Lake_Test', location: { x: 1000, y: 0, z: 0 } }, expected: 'success|plugin not enabled' },
  { scenario: 'Environment: create_water_body_lake small', toolName: 'build_environment', arguments: { action: 'create_water_body_lake', name: 'Lake_Small', location: { x: 2000, y: 0, z: 50 } }, expected: 'success|plugin not enabled' },
  
  // create_water_body_river
  { scenario: 'Environment: create_water_body_river success', toolName: 'build_environment', arguments: { action: 'create_water_body_river', name: 'River_Test', location: { x: -1000, y: 0, z: 0 } }, expected: 'success|plugin not enabled' },
  { scenario: 'Environment: create_water_body_river spline', toolName: 'build_environment', arguments: { action: 'create_water_body_river', name: 'River_Long', location: { x: -2000, y: 0, z: 100 } }, expected: 'success|plugin not enabled' },
  
  // configure_water_body
  { scenario: 'Environment: configure_water_body success', toolName: 'build_environment', arguments: { action: 'configure_water_body', name: 'Ocean_Test' }, expected: 'success|not found' },
  { scenario: 'Environment: configure_water_body not found', toolName: 'build_environment', arguments: { action: 'configure_water_body', name: 'NonExistent_Water' }, expected: 'not found|error' },
  
  // configure_water_waves
  { scenario: 'Environment: configure_water_waves success', toolName: 'build_environment', arguments: { action: 'configure_water_waves', name: 'Ocean_Test' }, expected: 'success|not found' },
  { scenario: 'Environment: configure_water_waves calm', toolName: 'build_environment', arguments: { action: 'configure_water_waves', name: 'Lake_Test' }, expected: 'success|not found' },
  
  // get_water_body_info
  { scenario: 'Environment: get_water_body_info success', toolName: 'build_environment', arguments: { action: 'get_water_body_info', name: 'Ocean_Test' }, expected: 'success|not found' },
  { scenario: 'Environment: get_water_body_info not found', toolName: 'build_environment', arguments: { action: 'get_water_body_info', name: 'NonExistent_Water' }, expected: 'not found|error' },
  
  // list_water_bodies
  { scenario: 'Environment: list_water_bodies success', toolName: 'build_environment', arguments: { action: 'list_water_bodies' }, expected: 'success' },
  { scenario: 'Environment: list_water_bodies query', toolName: 'build_environment', arguments: { action: 'list_water_bodies' }, expected: 'success|bodies' },
  
  // set_river_depth
  { scenario: 'Environment: set_river_depth success', toolName: 'build_environment', arguments: { action: 'set_river_depth', name: 'River_Test', splineKey: 0, depth: 100 }, expected: 'success|not found' },
  { scenario: 'Environment: set_river_depth not found', toolName: 'build_environment', arguments: { action: 'set_river_depth', name: 'NonExistent_River', splineKey: 0, depth: 100 }, expected: 'not found|error' },
  
  // set_ocean_extent
  { scenario: 'Environment: set_ocean_extent success', toolName: 'build_environment', arguments: { action: 'set_ocean_extent', name: 'Ocean_Test', extent: 5000 }, expected: 'success|not found' },
  { scenario: 'Environment: set_ocean_extent not found', toolName: 'build_environment', arguments: { action: 'set_ocean_extent', name: 'NonExistent_Ocean', extent: 5000 }, expected: 'not found|error' },
  
  // set_water_static_mesh
  { scenario: 'Environment: set_water_static_mesh success', toolName: 'build_environment', arguments: { action: 'set_water_static_mesh', name: 'Lake_Test', meshPath: '/Engine/BasicShapes/Plane' }, expected: 'success|not found' },
  { scenario: 'Environment: set_water_static_mesh not found', toolName: 'build_environment', arguments: { action: 'set_water_static_mesh', name: 'NonExistent_Water', meshPath: '/Engine/BasicShapes/Plane' }, expected: 'not found|error' },
  
  // set_river_transitions
  { scenario: 'Environment: set_river_transitions success', toolName: 'build_environment', arguments: { action: 'set_river_transitions', name: 'River_Test', lakeTransitionMaterial: '/Engine/EngineMaterials/DefaultMaterial', oceanTransitionMaterial: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },
  { scenario: 'Environment: set_river_transitions not found', toolName: 'build_environment', arguments: { action: 'set_river_transitions', name: 'NonExistent_River', lakeTransitionMaterial: '/Engine/EngineMaterials/DefaultMaterial', oceanTransitionMaterial: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'not found|error' },
  
  // set_water_zone
  { scenario: 'Environment: set_water_zone success', toolName: 'build_environment', arguments: { action: 'set_water_zone', name: 'Ocean_Test', waterZonePath: '/Game/WaterZones/OceanZone' }, expected: 'success|not found' },
  { scenario: 'Environment: set_water_zone not found', toolName: 'build_environment', arguments: { action: 'set_water_zone', name: 'NonExistent_Water', waterZonePath: '/Game/WaterZones/OceanZone' }, expected: 'not found|error' },
  
  // get_water_surface_info
  { scenario: 'Environment: get_water_surface_info success', toolName: 'build_environment', arguments: { action: 'get_water_surface_info', name: 'Ocean_Test', location: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Environment: get_water_surface_info not found', toolName: 'build_environment', arguments: { action: 'get_water_surface_info', name: 'NonExistent_Water', location: { x: 0, y: 0, z: 0 } }, expected: 'not found|error' },
  
  // get_wave_info
  { scenario: 'Environment: get_wave_info success', toolName: 'build_environment', arguments: { action: 'get_wave_info', name: 'Ocean_Test' }, expected: 'success|not found' },
  { scenario: 'Environment: get_wave_info not found', toolName: 'build_environment', arguments: { action: 'get_wave_info', name: 'NonExistent_Ocean' }, expected: 'not found|error' },

  // === Weather (8 actions) ===
  // configure_wind
  { scenario: 'Environment: configure_wind success', toolName: 'build_environment', arguments: { action: 'configure_wind' }, expected: 'success' },
  { scenario: 'Environment: configure_wind strong', toolName: 'build_environment', arguments: { action: 'configure_wind', strength: 500 }, expected: 'success' },
  
  // create_weather_system
  { scenario: 'Environment: create_weather_system success', toolName: 'build_environment', arguments: { action: 'create_weather_system', name: 'Weather_Test' }, expected: 'success' },
  { scenario: 'Environment: create_weather_system with location', toolName: 'build_environment', arguments: { action: 'create_weather_system', name: 'Weather_Custom', location: { x: 0, y: 0, z: 1000 } }, expected: 'success' },
  
  // configure_rain_particles
  { scenario: 'Environment: configure_rain_particles success', toolName: 'build_environment', arguments: { action: 'configure_rain_particles' }, expected: 'success' },
  { scenario: 'Environment: configure_rain_particles heavy', toolName: 'build_environment', arguments: { action: 'configure_rain_particles', density: 0.8 }, expected: 'success' },
  
  // configure_snow_particles
  { scenario: 'Environment: configure_snow_particles success', toolName: 'build_environment', arguments: { action: 'configure_snow_particles' }, expected: 'success' },
  { scenario: 'Environment: configure_snow_particles blizzard', toolName: 'build_environment', arguments: { action: 'configure_snow_particles', density: 0.9 }, expected: 'success' },
  
  // configure_lightning
  { scenario: 'Environment: configure_lightning success', toolName: 'build_environment', arguments: { action: 'configure_lightning' }, expected: 'success' },
  { scenario: 'Environment: configure_lightning intense', toolName: 'build_environment', arguments: { action: 'configure_lightning', intensity: 2.0 }, expected: 'success' },
  
  // configure_weather_preset
  { scenario: 'Environment: configure_weather_preset clear', toolName: 'build_environment', arguments: { action: 'configure_weather_preset', name: 'Clear' }, expected: 'success' },
  { scenario: 'Environment: configure_weather_preset storm', toolName: 'build_environment', arguments: { action: 'configure_weather_preset', name: 'Storm' }, expected: 'success|not found' },
  
  // query_water_bodies
  { scenario: 'Environment: query_water_bodies success', toolName: 'build_environment', arguments: { action: 'query_water_bodies' }, expected: 'success' },
  { scenario: 'Environment: query_water_bodies filtered', toolName: 'build_environment', arguments: { action: 'query_water_bodies' }, expected: 'success|bodies' },
  
  // configure_ocean_waves
  { scenario: 'Environment: configure_ocean_waves success', toolName: 'build_environment', arguments: { action: 'configure_ocean_waves', name: 'Ocean_Test' }, expected: 'success|not found' },
  { scenario: 'Environment: configure_ocean_waves calm', toolName: 'build_environment', arguments: { action: 'configure_ocean_waves', name: 'Ocean_Test' }, expected: 'success|not found' },

  // === Wave 5 Environment Actions (8 actions) ===
  // create_landscape_spline
  { scenario: 'Environment: create_landscape_spline success', toolName: 'build_environment', arguments: { action: 'create_landscape_spline', name: 'LandscapeSpline_Test', landscapeName: 'Landscape_Test' }, expected: 'success|not found' },
  { scenario: 'Environment: create_landscape_spline road', toolName: 'build_environment', arguments: { action: 'create_landscape_spline', name: 'LandscapeSpline_Road', landscapeName: 'Landscape_Test' }, expected: 'success|not found' },
  
  // configure_foliage_density
  { scenario: 'Environment: configure_foliage_density success', toolName: 'build_environment', arguments: { action: 'configure_foliage_density', foliageType: 'TestFoliage', density: 0.5 }, expected: 'success|not found' },
  { scenario: 'Environment: configure_foliage_density sparse', toolName: 'build_environment', arguments: { action: 'configure_foliage_density', foliageType: 'TestFoliage', density: 0.1 }, expected: 'success|not found' },
  
  // batch_paint_foliage
  { scenario: 'Environment: batch_paint_foliage success', toolName: 'build_environment', arguments: { action: 'batch_paint_foliage', foliageTypePath: '/Game/Foliage/TestFoliage', foliageTypes: ['TestFoliage'], locations: [{ x: 0, y: 0, z: 0 }] }, expected: 'success|not found' },
  { scenario: 'Environment: batch_paint_foliage multi', toolName: 'build_environment', arguments: { action: 'batch_paint_foliage', foliageTypePath: '/Game/Foliage/TestFoliage', foliageTypes: ['TestFoliage', 'TreeFoliage'], locations: [{ x: 100, y: 100, z: 0 }, { x: 200, y: 200, z: 0 }] }, expected: 'success|not found' },
  
  // configure_wind_directional
  { scenario: 'Environment: configure_wind_directional success', toolName: 'build_environment', arguments: { action: 'configure_wind_directional' }, expected: 'success' },
  { scenario: 'Environment: configure_wind_directional strong', toolName: 'build_environment', arguments: { action: 'configure_wind_directional', strength: 1.0 }, expected: 'success' },
  
  // get_terrain_height_at
  { scenario: 'Environment: get_terrain_height_at success', toolName: 'build_environment', arguments: { action: 'get_terrain_height_at', location: { x: 500, y: 500, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Environment: get_terrain_height_at origin', toolName: 'build_environment', arguments: { action: 'get_terrain_height_at', location: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_VOLUMES (41 actions) - ~82 test cases
// ============================================================================
const manageVolumesTests = [
  // === SETUP: Create Required Test Assets ===
  // Create SplineMeshBP blueprint for spline mesh component tests
  { scenario: 'SETUP: Create SplineMeshBP blueprint', toolName: 'manage_asset', arguments: { action: 'create_blueprint', path: '/Game/Blueprints/SplineMeshBP', parentClass: 'Actor', name: 'SplineMeshBP' }, expected: 'success|already exists' },
  
  // === Trigger Volumes (4 actions) ===
  // create_trigger_volume
  { scenario: 'Volumes: create_trigger_volume success', toolName: 'manage_volumes', arguments: { action: 'create_trigger_volume', volumeName: 'TriggerVol_Test', location: { x: 0, y: 0, z: 100 }, extent: { x: 200, y: 200, z: 200 } }, expected: 'success' },
  { scenario: 'Volumes: create_trigger_volume large', toolName: 'manage_volumes', arguments: { action: 'create_trigger_volume', volumeName: 'TriggerVol_Large', location: { x: 500, y: 0, z: 100 }, extent: { x: 500, y: 500, z: 500 } }, expected: 'success' },
  
  // create_trigger_box
  { scenario: 'Volumes: create_trigger_box success', toolName: 'manage_volumes', arguments: { action: 'create_trigger_box', volumeName: 'TriggerBox_Test', location: { x: 0, y: 0, z: 100 }, boxExtent: { x: 200, y: 200, z: 200 } }, expected: 'success' },
  { scenario: 'Volumes: create_trigger_box small', toolName: 'manage_volumes', arguments: { action: 'create_trigger_box', volumeName: 'TriggerBox_Small', location: { x: 100, y: 0, z: 50 }, boxExtent: { x: 50, y: 50, z: 50 } }, expected: 'success' },
  
  // create_trigger_sphere
  { scenario: 'Volumes: create_trigger_sphere success', toolName: 'manage_volumes', arguments: { action: 'create_trigger_sphere', volumeName: 'TriggerSphere_Test', location: { x: 500, y: 0, z: 100 }, sphereRadius: 200 }, expected: 'success' },
  { scenario: 'Volumes: create_trigger_sphere large', toolName: 'manage_volumes', arguments: { action: 'create_trigger_sphere', volumeName: 'TriggerSphere_Large', location: { x: 500, y: 500, z: 100 }, sphereRadius: 500 }, expected: 'success' },
  
  // create_trigger_capsule
  { scenario: 'Volumes: create_trigger_capsule success', toolName: 'manage_volumes', arguments: { action: 'create_trigger_capsule', volumeName: 'TriggerCapsule_Test', location: { x: 1000, y: 0, z: 100 }, capsuleRadius: 50, capsuleHalfHeight: 100 }, expected: 'success' },
  { scenario: 'Volumes: create_trigger_capsule tall', toolName: 'manage_volumes', arguments: { action: 'create_trigger_capsule', volumeName: 'TriggerCapsule_Tall', location: { x: 1000, y: 500, z: 100 }, capsuleRadius: 30, capsuleHalfHeight: 200 }, expected: 'success' },

  // === Blocking Volumes (4 actions) ===
  // create_blocking_volume
  { scenario: 'Volumes: create_blocking_volume success', toolName: 'manage_volumes', arguments: { action: 'create_blocking_volume', volumeName: 'BlockingVol_Test', location: { x: 0, y: 500, z: 100 }, extent: { x: 300, y: 300, z: 300 } }, expected: 'success' },
  { scenario: 'Volumes: create_blocking_volume wall', toolName: 'manage_volumes', arguments: { action: 'create_blocking_volume', volumeName: 'BlockingVol_Wall', location: { x: 0, y: 1000, z: 100 }, extent: { x: 500, y: 50, z: 200 } }, expected: 'success' },
  
  // create_kill_z_volume
  { scenario: 'Volumes: create_kill_z_volume success', toolName: 'manage_volumes', arguments: { action: 'create_kill_z_volume', volumeName: 'KillZVol_Test', location: { x: 0, y: -500, z: -100 }, extent: { x: 5000, y: 5000, z: 100 } }, expected: 'success' },
  { scenario: 'Volumes: create_kill_z_volume deep', toolName: 'manage_volumes', arguments: { action: 'create_kill_z_volume', volumeName: 'KillZVol_Deep', location: { x: 0, y: 0, z: -500 }, extent: { x: 10000, y: 10000, z: 50 } }, expected: 'success' },
  
  // create_pain_causing_volume
  { scenario: 'Volumes: create_pain_causing_volume success', toolName: 'manage_volumes', arguments: { action: 'create_pain_causing_volume', volumeName: 'PainVol_Test', location: { x: 1000, y: 500, z: 100 }, extent: { x: 200, y: 200, z: 200 }, damagePerSec: 10 }, expected: 'success' },
  { scenario: 'Volumes: create_pain_causing_volume lethal', toolName: 'manage_volumes', arguments: { action: 'create_pain_causing_volume', volumeName: 'PainVol_Lethal', location: { x: 1000, y: 1000, z: 100 }, extent: { x: 100, y: 100, z: 100 }, damagePerSec: 100, bPainCausing: true }, expected: 'success' },
  
  // create_physics_volume
  { scenario: 'Volumes: create_physics_volume success', toolName: 'manage_volumes', arguments: { action: 'create_physics_volume', volumeName: 'PhysicsVol_Test', location: { x: 500, y: 500, z: 100 }, extent: { x: 300, y: 300, z: 300 } }, expected: 'success' },
  { scenario: 'Volumes: create_physics_volume water', toolName: 'manage_volumes', arguments: { action: 'create_physics_volume', volumeName: 'PhysicsVol_Water', location: { x: 500, y: 1000, z: 0 }, extent: { x: 500, y: 500, z: 200 }, bWaterVolume: true, fluidFriction: 0.5 }, expected: 'success' },

  // === Audio Volumes (2 actions) ===
  // create_audio_volume
  { scenario: 'Volumes: create_audio_volume success', toolName: 'manage_volumes', arguments: { action: 'create_audio_volume', volumeName: 'AudioVol_Test', location: { x: -500, y: 0, z: 100 }, extent: { x: 300, y: 300, z: 300 }, bEnabled: true }, expected: 'success' },
  { scenario: 'Volumes: create_audio_volume disabled', toolName: 'manage_volumes', arguments: { action: 'create_audio_volume', volumeName: 'AudioVol_Disabled', location: { x: -500, y: 500, z: 100 }, extent: { x: 200, y: 200, z: 200 }, bEnabled: false }, expected: 'success' },
  
  // create_reverb_volume
  { scenario: 'Volumes: create_reverb_volume success', toolName: 'manage_volumes', arguments: { action: 'create_reverb_volume', volumeName: 'ReverbVol_Test', location: { x: -500, y: 500, z: 100 }, extent: { x: 400, y: 400, z: 300 } }, expected: 'success' },
  { scenario: 'Volumes: create_reverb_volume cave', toolName: 'manage_volumes', arguments: { action: 'create_reverb_volume', volumeName: 'ReverbVol_Cave', location: { x: -1000, y: 500, z: 100 }, extent: { x: 500, y: 500, z: 200 }, reverbVolume: 0.8 }, expected: 'success' },

  // === Optimization Volumes (3 actions) ===
  // create_cull_distance_volume
  { scenario: 'Volumes: create_cull_distance_volume success', toolName: 'manage_volumes', arguments: { action: 'create_cull_distance_volume', volumeName: 'CullVol_Test', location: { x: 0, y: 0, z: 0 }, extent: { x: 10000, y: 10000, z: 5000 } }, expected: 'success' },
  { scenario: 'Volumes: create_cull_distance_volume with distances', toolName: 'manage_volumes', arguments: { action: 'create_cull_distance_volume', volumeName: 'CullVol_Custom', location: { x: 0, y: 0, z: 0 }, cullDistances: [{ size: 100, cullDistance: 5000 }, { size: 500, cullDistance: 10000 }] }, expected: 'success' },
  
  // create_precomputed_visibility_volume
  { scenario: 'Volumes: create_precomputed_visibility_volume success', toolName: 'manage_volumes', arguments: { action: 'create_precomputed_visibility_volume', volumeName: 'PrecompVisVol_Test', location: { x: 0, y: 0, z: 0 }, extent: { x: 5000, y: 5000, z: 2000 } }, expected: 'success' },
  { scenario: 'Volumes: create_precomputed_visibility_volume small', toolName: 'manage_volumes', arguments: { action: 'create_precomputed_visibility_volume', volumeName: 'PrecompVisVol_Small', location: { x: 1000, y: 1000, z: 0 }, extent: { x: 1000, y: 1000, z: 500 } }, expected: 'success' },
  
  // create_lightmass_importance_volume
  { scenario: 'Volumes: create_lightmass_importance_volume success', toolName: 'manage_volumes', arguments: { action: 'create_lightmass_importance_volume', volumeName: 'LightmassVol_Test', location: { x: 0, y: 0, z: 0 }, extent: { x: 10000, y: 10000, z: 5000 } }, expected: 'success' },
  { scenario: 'Volumes: create_lightmass_importance_volume focused', toolName: 'manage_volumes', arguments: { action: 'create_lightmass_importance_volume', volumeName: 'LightmassVol_Focused', location: { x: 0, y: 0, z: 100 }, extent: { x: 2000, y: 2000, z: 1000 } }, expected: 'success' },

  // === Navigation Volumes (3 actions) ===
  // create_nav_mesh_bounds_volume
  { scenario: 'Volumes: create_nav_mesh_bounds_volume success', toolName: 'manage_volumes', arguments: { action: 'create_nav_mesh_bounds_volume', volumeName: 'NavBounds_Test', location: { x: 0, y: 0, z: 0 }, extent: { x: 5000, y: 5000, z: 1000 } }, expected: 'success' },
  { scenario: 'Volumes: create_nav_mesh_bounds_volume large', toolName: 'manage_volumes', arguments: { action: 'create_nav_mesh_bounds_volume', volumeName: 'NavBounds_Large', location: { x: 0, y: 0, z: 0 }, extent: { x: 20000, y: 20000, z: 2000 } }, expected: 'success' },
  
  // create_nav_modifier_volume
  { scenario: 'Volumes: create_nav_modifier_volume success', toolName: 'manage_volumes', arguments: { action: 'create_nav_modifier_volume', volumeName: 'NavMod_Test', location: { x: 1000, y: 1000, z: 100 }, extent: { x: 500, y: 500, z: 300 } }, expected: 'success' },
  { scenario: 'Volumes: create_nav_modifier_volume dynamic', toolName: 'manage_volumes', arguments: { action: 'create_nav_modifier_volume', volumeName: 'NavMod_Dynamic', location: { x: 2000, y: 2000, z: 100 }, extent: { x: 300, y: 300, z: 200 }, bDynamicModifier: true }, expected: 'success' },
  
  // create_camera_blocking_volume
  { scenario: 'Volumes: create_camera_blocking_volume success', toolName: 'manage_volumes', arguments: { action: 'create_camera_blocking_volume', volumeName: 'CameraBlock_Test', location: { x: 0, y: 0, z: 200 }, extent: { x: 100, y: 100, z: 100 } }, expected: 'success' },
  { scenario: 'Volumes: create_camera_blocking_volume wall', toolName: 'manage_volumes', arguments: { action: 'create_camera_blocking_volume', volumeName: 'CameraBlock_Wall', location: { x: -500, y: 0, z: 150 }, extent: { x: 10, y: 500, z: 150 } }, expected: 'success' },

  // === Volume Properties (3 actions) ===
  // set_volume_extent
  { scenario: 'Volumes: set_volume_extent success', toolName: 'manage_volumes', arguments: { action: 'set_volume_extent', volumeName: 'TriggerBox_Test', extent: { x: 300, y: 300, z: 300 } }, expected: 'success|not found' },
  { scenario: 'Volumes: set_volume_extent not found', toolName: 'manage_volumes', arguments: { action: 'set_volume_extent', volumeName: 'NonExistent_Volume', extent: { x: 100, y: 100, z: 100 } }, expected: 'not found|error' },
  
  // set_volume_properties
  { scenario: 'Volumes: set_volume_properties success', toolName: 'manage_volumes', arguments: { action: 'set_volume_properties', volumeName: 'AudioVol_Test', bEnabled: true }, expected: 'success|not found' },
  { scenario: 'Volumes: set_volume_properties physics', toolName: 'manage_volumes', arguments: { action: 'set_volume_properties', volumeName: 'PhysicsVol_Test', terminalVelocity: 1000, fluidFriction: 0.3 }, expected: 'success|not found' },
  
  // get_volumes_info
  { scenario: 'Volumes: get_volumes_info all', toolName: 'manage_volumes', arguments: { action: 'get_volumes_info' }, expected: 'success' },
  { scenario: 'Volumes: get_volumes_info trigger', toolName: 'manage_volumes', arguments: { action: 'get_volumes_info', volumeType: 'Trigger' }, expected: 'success' },

  // === Splines (18 actions) ===
  // create_spline_actor
  { scenario: 'Volumes: create_spline_actor success', toolName: 'manage_volumes', arguments: { action: 'create_spline_actor', actorName: 'Spline_Test', location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
  { scenario: 'Volumes: create_spline_actor closed', toolName: 'manage_volumes', arguments: { action: 'create_spline_actor', actorName: 'Spline_Closed', location: { x: 500, y: 0, z: 100 }, bClosedLoop: true }, expected: 'success' },
  
  // add_spline_point
  { scenario: 'Volumes: add_spline_point success', toolName: 'manage_volumes', arguments: { action: 'add_spline_point', actorName: 'Spline_Test', position: { x: 500, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Volumes: add_spline_point with tangent', toolName: 'manage_volumes', arguments: { action: 'add_spline_point', actorName: 'Spline_Test', position: { x: 1000, y: 500, z: 150 }, tangent: { x: 100, y: 100, z: 0 } }, expected: 'success|not found' },
  
  // remove_spline_point
  { scenario: 'Volumes: remove_spline_point success', toolName: 'manage_volumes', arguments: { action: 'remove_spline_point', actorName: 'Spline_Test', pointIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Volumes: remove_spline_point not found', toolName: 'manage_volumes', arguments: { action: 'remove_spline_point', actorName: 'NonExistent_Spline', pointIndex: 0 }, expected: 'not found|error' },
  
  // set_spline_point_position
  { scenario: 'Volumes: set_spline_point_position success', toolName: 'manage_volumes', arguments: { action: 'set_spline_point_position', actorName: 'Spline_Test', pointIndex: 0, position: { x: 100, y: 100, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Volumes: set_spline_point_position world', toolName: 'manage_volumes', arguments: { action: 'set_spline_point_position', actorName: 'Spline_Test', pointIndex: 1, position: { x: 600, y: 200, z: 120 }, coordinateSpace: 'World' }, expected: 'success|not found' },
  
  // set_spline_point_tangents
  { scenario: 'Volumes: set_spline_point_tangents success', toolName: 'manage_volumes', arguments: { action: 'set_spline_point_tangents', actorName: 'Spline_Test', pointIndex: 0, arriveTangent: { x: 100, y: 0, z: 0 }, leaveTangent: { x: 100, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Volumes: set_spline_point_tangents not found', toolName: 'manage_volumes', arguments: { action: 'set_spline_point_tangents', actorName: 'NonExistent_Spline', pointIndex: 0, arriveTangent: { x: 0, y: 0, z: 0 }, leaveTangent: { x: 0, y: 0, z: 0 } }, expected: 'not found|error' },
  
  // set_spline_point_rotation
  { scenario: 'Volumes: set_spline_point_rotation success', toolName: 'manage_volumes', arguments: { action: 'set_spline_point_rotation', actorName: 'Spline_Test', pointIndex: 0, rotation: { pitch: 0, yaw: 45, roll: 0 } }, expected: 'success|not found' },
  { scenario: 'Volumes: set_spline_point_rotation not found', toolName: 'manage_volumes', arguments: { action: 'set_spline_point_rotation', actorName: 'NonExistent_Spline', pointIndex: 0, rotation: { pitch: 0, yaw: 0, roll: 0 } }, expected: 'not found|error' },
  
  // set_spline_point_scale
  { scenario: 'Volumes: set_spline_point_scale success', toolName: 'manage_volumes', arguments: { action: 'set_spline_point_scale', actorName: 'Spline_Test', pointIndex: 0, scale: { x: 1.5, y: 1.5, z: 1.5 } }, expected: 'success|not found' },
  { scenario: 'Volumes: set_spline_point_scale not found', toolName: 'manage_volumes', arguments: { action: 'set_spline_point_scale', actorName: 'NonExistent_Spline', pointIndex: 0, scale: { x: 1, y: 1, z: 1 } }, expected: 'not found|error' },
  
  // set_spline_type
  { scenario: 'Volumes: set_spline_type curve', toolName: 'manage_volumes', arguments: { action: 'set_spline_type', actorName: 'Spline_Test', splineType: 'Curve' }, expected: 'success|not found' },
  { scenario: 'Volumes: set_spline_type linear', toolName: 'manage_volumes', arguments: { action: 'set_spline_type', actorName: 'Spline_Test', splineType: 'Linear' }, expected: 'success|not found' },
  
  // create_spline_mesh_component
  { scenario: 'Volumes: create_spline_mesh_component success', toolName: 'manage_volumes', arguments: { action: 'create_spline_mesh_component', actorName: 'SplineMesh_Test', meshPath: '/Engine/BasicShapes/Cylinder', blueprintPath: '/Game/Blueprints/SplineMeshBP' }, expected: 'success' },
  { scenario: 'Volumes: create_spline_mesh_component with axis', toolName: 'manage_volumes', arguments: { action: 'create_spline_mesh_component', actorName: 'SplineMesh_Custom', meshPath: '/Engine/BasicShapes/Cube', forwardAxis: 'X', blueprintPath: '/Game/Blueprints/SplineMeshBP' }, expected: 'success' },
  
  // set_spline_mesh_asset
  { scenario: 'Volumes: set_spline_mesh_asset success', toolName: 'manage_volumes', arguments: { action: 'set_spline_mesh_asset', actorName: 'SplineMesh_Test', meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success|not found' },
  { scenario: 'Volumes: set_spline_mesh_asset not found', toolName: 'manage_volumes', arguments: { action: 'set_spline_mesh_asset', actorName: 'NonExistent_SplineMesh', meshPath: '/Engine/BasicShapes/Sphere' }, expected: 'not found|error' },
  
  // configure_spline_mesh_axis
  { scenario: 'Volumes: configure_spline_mesh_axis X', toolName: 'manage_volumes', arguments: { action: 'configure_spline_mesh_axis', actorName: 'SplineMesh_Test', forwardAxis: 'X' }, expected: 'success|not found' },
  { scenario: 'Volumes: configure_spline_mesh_axis Z', toolName: 'manage_volumes', arguments: { action: 'configure_spline_mesh_axis', actorName: 'SplineMesh_Test', forwardAxis: 'Z' }, expected: 'success|not found' },
  
  // set_spline_mesh_material
  { scenario: 'Volumes: set_spline_mesh_material success', toolName: 'manage_volumes', arguments: { action: 'set_spline_mesh_material', actorName: 'SplineMesh_Test', materialPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },
  { scenario: 'Volumes: set_spline_mesh_material not found', toolName: 'manage_volumes', arguments: { action: 'set_spline_mesh_material', actorName: 'NonExistent_SplineMesh', materialPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'not found|error' },
  
  // scatter_meshes_along_spline
  { scenario: 'Volumes: scatter_meshes_along_spline success', toolName: 'manage_volumes', arguments: { action: 'scatter_meshes_along_spline', actorName: 'Spline_Test', meshPath: '/Engine/BasicShapes/Sphere', spacing: 100 }, expected: 'success|not found' },
  { scenario: 'Volumes: scatter_meshes_along_spline dense', toolName: 'manage_volumes', arguments: { action: 'scatter_meshes_along_spline', actorName: 'Spline_Test', meshPath: '/Engine/BasicShapes/Cube', spacing: 50 }, expected: 'success|not found' },
  
  // configure_mesh_spacing
  { scenario: 'Volumes: configure_mesh_spacing success', toolName: 'manage_volumes', arguments: { action: 'configure_mesh_spacing', actorName: 'Spline_Test', spacing: 150 }, expected: 'success|not found' },
  { scenario: 'Volumes: configure_mesh_spacing tight', toolName: 'manage_volumes', arguments: { action: 'configure_mesh_spacing', actorName: 'Spline_Test', spacing: 25 }, expected: 'success|not found' },
  
  // configure_mesh_randomization
  { scenario: 'Volumes: configure_mesh_randomization success', toolName: 'manage_volumes', arguments: { action: 'configure_mesh_randomization', actorName: 'Spline_Test' }, expected: 'success|not found' },
  { scenario: 'Volumes: configure_mesh_randomization not found', toolName: 'manage_volumes', arguments: { action: 'configure_mesh_randomization', actorName: 'NonExistent_Spline' }, expected: 'not found|error' },

  // === Spline Templates (6 actions) ===
  // create_road_spline
  { scenario: 'Volumes: create_road_spline success', toolName: 'manage_volumes', arguments: { action: 'create_road_spline', actorName: 'RoadSpline_Test', location: { x: 0, y: 0, z: 0 }, width: 400 }, expected: 'success' },
  { scenario: 'Volumes: create_road_spline narrow', toolName: 'manage_volumes', arguments: { action: 'create_road_spline', actorName: 'RoadSpline_Narrow', location: { x: 1000, y: 0, z: 0 }, width: 200 }, expected: 'success' },
  
  // create_river_spline
  { scenario: 'Volumes: create_river_spline success', toolName: 'manage_volumes', arguments: { action: 'create_river_spline', actorName: 'RiverSpline_Test', location: { x: -1000, y: 0, z: 0 }, width: 300 }, expected: 'success' },
  { scenario: 'Volumes: create_river_spline wide', toolName: 'manage_volumes', arguments: { action: 'create_river_spline', actorName: 'RiverSpline_Wide', location: { x: -2000, y: 0, z: 0 }, width: 600 }, expected: 'success' },
  
  // create_fence_spline
  { scenario: 'Volumes: create_fence_spline success', toolName: 'manage_volumes', arguments: { action: 'create_fence_spline', actorName: 'FenceSpline_Test', location: { x: 0, y: 1000, z: 0 } }, expected: 'success' },
  { scenario: 'Volumes: create_fence_spline with mesh', toolName: 'manage_volumes', arguments: { action: 'create_fence_spline', actorName: 'FenceSpline_Custom', location: { x: 0, y: 2000, z: 0 }, meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success' },
  
  // create_wall_spline
  { scenario: 'Volumes: create_wall_spline success', toolName: 'manage_volumes', arguments: { action: 'create_wall_spline', actorName: 'WallSpline_Test', location: { x: 0, y: -1000, z: 0 } }, expected: 'success' },
  { scenario: 'Volumes: create_wall_spline thick', toolName: 'manage_volumes', arguments: { action: 'create_wall_spline', actorName: 'WallSpline_Thick', location: { x: 0, y: -2000, z: 0 }, width: 100 }, expected: 'success' },
  
  // create_cable_spline
  { scenario: 'Volumes: create_cable_spline success', toolName: 'manage_volumes', arguments: { action: 'create_cable_spline', actorName: 'CableSpline_Test', location: { x: 2000, y: 0, z: 200 } }, expected: 'success' },
  { scenario: 'Volumes: create_cable_spline droopy', toolName: 'manage_volumes', arguments: { action: 'create_cable_spline', actorName: 'CableSpline_Droopy', location: { x: 2000, y: 500, z: 300 } }, expected: 'success' },
  
  // create_pipe_spline
  { scenario: 'Volumes: create_pipe_spline success', toolName: 'manage_volumes', arguments: { action: 'create_pipe_spline', actorName: 'PipeSpline_Test', location: { x: -2000, y: 0, z: 100 } }, expected: 'success' },
  { scenario: 'Volumes: create_pipe_spline industrial', toolName: 'manage_volumes', arguments: { action: 'create_pipe_spline', actorName: 'PipeSpline_Industrial', location: { x: -2000, y: 500, z: 200 }, width: 50 }, expected: 'success' },

  // === Spline Info (1 action) ===
  // get_splines_info
  { scenario: 'Volumes: get_splines_info all', toolName: 'manage_volumes', arguments: { action: 'get_splines_info' }, expected: 'success' },
  { scenario: 'Volumes: get_splines_info query', toolName: 'manage_volumes', arguments: { action: 'get_splines_info' }, expected: 'success|splines' },
];

// ============================================================================
// COMBINED TEST CASES
// ============================================================================
export const worldToolsTests = [
  ...manageLightingTests,
  ...buildEnvironmentTests,
  ...manageVolumesTests,
];

// Run tests
const main = async () => {
  await runToolTests('world-tools', worldToolsTests);
};

if (import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  main();
}

