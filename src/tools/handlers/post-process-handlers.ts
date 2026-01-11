/**
 * Phase 29: Advanced Lighting & Rendering - Post-Process Handlers
 * Handles post-process volumes, bloom, DOF, motion blur, color grading,
 * reflection captures, ray tracing, scene captures, and light channels.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest, normalizeLocation, normalizeRotation } from './common-handlers.js';

/**
 * Main handler for post-process tools
 */
export async function handlePostProcessTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<HandlerResult> {
  // Normalize location/rotation if present
  const normalizedLocation = normalizeLocation(args.location);
  const normalizedRotation = normalizeRotation(args.rotation as [number, number, number] | { pitch: number; yaw: number; roll: number } | null | undefined);
  const captureOffsetNormalized = normalizeLocation(args.captureOffset);

  // Build the payload for automation request
  const payload: Record<string, unknown> = {
    action_type: action,
    ...args,
    location: normalizedLocation,
    rotation: normalizedRotation,
    captureOffset: captureOffsetNormalized
  };

  // Remove undefined values
  Object.keys(payload).forEach(key => {
    if (payload[key] === undefined) {
      delete payload[key];
    }
  });

  switch (action) {
    // =========================================
    // POST-PROCESS VOLUME CORE
    // =========================================
    case 'create_post_process_volume':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for create_post_process_volume'
      )) as HandlerResult;

    case 'configure_pp_blend':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_pp_blend'
      )) as HandlerResult;

    case 'configure_pp_priority':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_pp_priority'
      )) as HandlerResult;

    case 'get_post_process_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for get_post_process_settings'
      )) as HandlerResult;

    // =========================================
    // VISUAL EFFECTS (Bloom, DOF, Motion Blur)
    // =========================================
    case 'configure_bloom':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_bloom'
      )) as HandlerResult;

    case 'configure_dof':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_dof'
      )) as HandlerResult;

    case 'configure_motion_blur':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_motion_blur'
      )) as HandlerResult;

    // =========================================
    // COLOR & LENS EFFECTS
    // =========================================
    case 'configure_color_grading':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_color_grading'
      )) as HandlerResult;

    case 'configure_white_balance':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_white_balance'
      )) as HandlerResult;

    case 'configure_vignette':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_vignette'
      )) as HandlerResult;

    case 'configure_chromatic_aberration':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_chromatic_aberration'
      )) as HandlerResult;

    case 'configure_film_grain':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_film_grain'
      )) as HandlerResult;

    case 'configure_lens_flares':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_lens_flares'
      )) as HandlerResult;

    // =========================================
    // REFLECTION CAPTURES
    // =========================================
    case 'create_sphere_reflection_capture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for create_sphere_reflection_capture'
      )) as HandlerResult;

    case 'create_box_reflection_capture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for create_box_reflection_capture'
      )) as HandlerResult;

    case 'create_planar_reflection':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for create_planar_reflection'
      )) as HandlerResult;

    case 'recapture_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for recapture_scene'
      )) as HandlerResult;

    // =========================================
    // RAY TRACING
    // =========================================
    case 'configure_ray_traced_shadows':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_ray_traced_shadows'
      )) as HandlerResult;

    case 'configure_ray_traced_gi':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_ray_traced_gi'
      )) as HandlerResult;

    case 'configure_ray_traced_reflections':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_ray_traced_reflections'
      )) as HandlerResult;

    case 'configure_ray_traced_ao':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_ray_traced_ao'
      )) as HandlerResult;

    case 'configure_path_tracing':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_path_tracing'
      )) as HandlerResult;

    // =========================================
    // SCENE CAPTURES
    // =========================================
    case 'create_scene_capture_2d':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for create_scene_capture_2d'
      )) as HandlerResult;

    case 'create_scene_capture_cube':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for create_scene_capture_cube'
      )) as HandlerResult;

    case 'capture_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for capture_scene'
      )) as HandlerResult;

    // =========================================
    // LIGHT CHANNELS
    // =========================================
    case 'set_light_channel':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for set_light_channel'
      )) as HandlerResult;

    case 'set_actor_light_channel':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for set_actor_light_channel'
      )) as HandlerResult;

    // =========================================
    // LIGHTMASS SETTINGS
    // =========================================
    case 'configure_lightmass_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_lightmass_settings'
      )) as HandlerResult;

    case 'build_lighting_quality':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for build_lighting_quality'
      )) as HandlerResult;

    case 'configure_indirect_lighting_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_indirect_lighting_cache'
      )) as HandlerResult;

    case 'configure_volumetric_lightmap':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_post_process',
        payload,
        'Automation bridge not available for configure_volumetric_lightmap'
      )) as HandlerResult;

    // =========================================
    // DEFAULT / UNKNOWN ACTION
    // =========================================
    default:
      return {
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown post-process action: ${action}. Valid actions: create_post_process_volume, configure_pp_blend, configure_pp_priority, get_post_process_settings, configure_bloom, configure_dof, configure_motion_blur, configure_color_grading, configure_white_balance, configure_vignette, configure_chromatic_aberration, configure_film_grain, configure_lens_flares, create_sphere_reflection_capture, create_box_reflection_capture, create_planar_reflection, recapture_scene, configure_ray_traced_shadows, configure_ray_traced_gi, configure_ray_traced_reflections, configure_ray_traced_ao, configure_path_tracing, create_scene_capture_2d, create_scene_capture_cube, capture_scene, set_light_channel, set_actor_light_channel, configure_lightmass_settings, build_lighting_quality, configure_indirect_lighting_cache, configure_volumetric_lightmap`
      };
  }
}
