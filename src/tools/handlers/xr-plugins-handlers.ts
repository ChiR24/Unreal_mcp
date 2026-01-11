/**
 * Phase 41: XR Plugins (VR/AR/MR) Handlers
 * Handles OpenXR, Meta Quest, SteamVR, Apple ARKit, Google ARCore, Varjo, HoloLens.
 * ~140 actions across 7 XR platform subsystems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for XR Plugins tools
 */
export async function handleXRPluginsTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<HandlerResult> {
  // Build the payload for automation request
  const payload: Record<string, unknown> = {
    action_type: action,
    ...args
  };

  // Remove undefined values
  Object.keys(payload).forEach(key => {
    if (payload[key] === undefined) {
      delete payload[key];
    }
  });

  switch (action) {
    // =========================================
    // OPENXR - Core Runtime (20 actions)
    // =========================================
    case 'get_openxr_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_openxr_info'
      )) as HandlerResult;

    case 'configure_openxr_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_openxr_settings'
      )) as HandlerResult;

    case 'set_tracking_origin':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_tracking_origin'
      )) as HandlerResult;

    case 'get_tracking_origin':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracking_origin'
      )) as HandlerResult;

    case 'create_xr_action_set':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_xr_action_set'
      )) as HandlerResult;

    case 'add_xr_action':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for add_xr_action'
      )) as HandlerResult;

    case 'bind_xr_action':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for bind_xr_action'
      )) as HandlerResult;

    case 'get_xr_action_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_xr_action_state'
      )) as HandlerResult;

    case 'trigger_haptic_feedback':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for trigger_haptic_feedback'
      )) as HandlerResult;

    case 'stop_haptic_feedback':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for stop_haptic_feedback'
      )) as HandlerResult;

    case 'get_hmd_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hmd_pose'
      )) as HandlerResult;

    case 'get_controller_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_controller_pose'
      )) as HandlerResult;

    case 'get_hand_tracking_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hand_tracking_data'
      )) as HandlerResult;

    case 'enable_hand_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_hand_tracking'
      )) as HandlerResult;

    case 'disable_hand_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_hand_tracking'
      )) as HandlerResult;

    case 'get_eye_tracking_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_eye_tracking_data'
      )) as HandlerResult;

    case 'enable_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_eye_tracking'
      )) as HandlerResult;

    case 'get_view_configuration':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_view_configuration'
      )) as HandlerResult;

    case 'set_render_scale':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_render_scale'
      )) as HandlerResult;

    case 'get_supported_extensions':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_supported_extensions'
      )) as HandlerResult;

    // =========================================
    // META QUEST - Oculus Platform (22 actions)
    // =========================================
    case 'get_quest_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_info'
      )) as HandlerResult;

    case 'configure_quest_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_quest_settings'
      )) as HandlerResult;

    case 'enable_passthrough':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_passthrough'
      )) as HandlerResult;

    case 'disable_passthrough':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_passthrough'
      )) as HandlerResult;

    case 'configure_passthrough_style':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_passthrough_style'
      )) as HandlerResult;

    case 'enable_scene_capture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_scene_capture'
      )) as HandlerResult;

    case 'get_scene_anchors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_scene_anchors'
      )) as HandlerResult;

    case 'get_room_layout':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_room_layout'
      )) as HandlerResult;

    case 'enable_quest_hand_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_quest_hand_tracking'
      )) as HandlerResult;

    case 'get_quest_hand_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_hand_pose'
      )) as HandlerResult;

    case 'enable_quest_face_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_quest_face_tracking'
      )) as HandlerResult;

    case 'get_quest_face_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_face_state'
      )) as HandlerResult;

    case 'enable_quest_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_quest_eye_tracking'
      )) as HandlerResult;

    case 'get_quest_eye_gaze':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_eye_gaze'
      )) as HandlerResult;

    case 'enable_quest_body_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_quest_body_tracking'
      )) as HandlerResult;

    case 'get_quest_body_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_body_state'
      )) as HandlerResult;

    case 'create_spatial_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_spatial_anchor'
      )) as HandlerResult;

    case 'save_spatial_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for save_spatial_anchor'
      )) as HandlerResult;

    case 'load_spatial_anchors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for load_spatial_anchors'
      )) as HandlerResult;

    case 'delete_spatial_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for delete_spatial_anchor'
      )) as HandlerResult;

    case 'configure_guardian_bounds':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_guardian_bounds'
      )) as HandlerResult;

    case 'get_guardian_geometry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_guardian_geometry'
      )) as HandlerResult;

    // =========================================
    // STEAMVR - Valve/HTC Platform (18 actions)
    // =========================================
    case 'get_steamvr_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_steamvr_info'
      )) as HandlerResult;

    case 'configure_steamvr_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_steamvr_settings'
      )) as HandlerResult;

    case 'configure_chaperone_bounds':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_chaperone_bounds'
      )) as HandlerResult;

    case 'get_chaperone_geometry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_chaperone_geometry'
      )) as HandlerResult;

    case 'create_steamvr_overlay':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_steamvr_overlay'
      )) as HandlerResult;

    case 'set_overlay_texture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_overlay_texture'
      )) as HandlerResult;

    case 'show_overlay':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for show_overlay'
      )) as HandlerResult;

    case 'hide_overlay':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for hide_overlay'
      )) as HandlerResult;

    case 'destroy_overlay':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for destroy_overlay'
      )) as HandlerResult;

    case 'get_tracked_device_count':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_device_count'
      )) as HandlerResult;

    case 'get_tracked_device_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_device_info'
      )) as HandlerResult;

    case 'get_lighthouse_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_lighthouse_info'
      )) as HandlerResult;

    case 'trigger_steamvr_haptic':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for trigger_steamvr_haptic'
      )) as HandlerResult;

    case 'get_steamvr_action_manifest':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_steamvr_action_manifest'
      )) as HandlerResult;

    case 'set_steamvr_action_manifest':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_steamvr_action_manifest'
      )) as HandlerResult;

    case 'enable_steamvr_skeletal_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_steamvr_skeletal_input'
      )) as HandlerResult;

    case 'get_skeletal_bone_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_skeletal_bone_data'
      )) as HandlerResult;

    case 'configure_steamvr_render':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_steamvr_render'
      )) as HandlerResult;

    // =========================================
    // APPLE ARKIT - iOS AR Platform (22 actions)
    // =========================================
    case 'get_arkit_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arkit_info'
      )) as HandlerResult;

    case 'configure_arkit_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_arkit_session'
      )) as HandlerResult;

    case 'start_arkit_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for start_arkit_session'
      )) as HandlerResult;

    case 'pause_arkit_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for pause_arkit_session'
      )) as HandlerResult;

    case 'configure_world_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_world_tracking'
      )) as HandlerResult;

    case 'get_tracked_planes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_planes'
      )) as HandlerResult;

    case 'get_tracked_images':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_images'
      )) as HandlerResult;

    case 'add_reference_image':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for add_reference_image'
      )) as HandlerResult;

    case 'enable_people_occlusion':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_people_occlusion'
      )) as HandlerResult;

    case 'disable_people_occlusion':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_people_occlusion'
      )) as HandlerResult;

    case 'enable_arkit_face_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_arkit_face_tracking'
      )) as HandlerResult;

    case 'get_arkit_face_blendshapes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arkit_face_blendshapes'
      )) as HandlerResult;

    case 'get_arkit_face_geometry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arkit_face_geometry'
      )) as HandlerResult;

    case 'enable_body_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_body_tracking'
      )) as HandlerResult;

    case 'get_body_skeleton':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_body_skeleton'
      )) as HandlerResult;

    case 'create_arkit_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_arkit_anchor'
      )) as HandlerResult;

    case 'remove_arkit_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for remove_arkit_anchor'
      )) as HandlerResult;

    case 'get_light_estimation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_light_estimation'
      )) as HandlerResult;

    case 'enable_scene_reconstruction':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_scene_reconstruction'
      )) as HandlerResult;

    case 'get_scene_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_scene_mesh'
      )) as HandlerResult;

    case 'perform_raycast':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for perform_raycast'
      )) as HandlerResult;

    case 'get_camera_intrinsics':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_camera_intrinsics'
      )) as HandlerResult;

    // =========================================
    // GOOGLE ARCORE - Android AR Platform (18 actions)
    // =========================================
    case 'get_arcore_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arcore_info'
      )) as HandlerResult;

    case 'configure_arcore_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_arcore_session'
      )) as HandlerResult;

    case 'start_arcore_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for start_arcore_session'
      )) as HandlerResult;

    case 'pause_arcore_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for pause_arcore_session'
      )) as HandlerResult;

    case 'get_arcore_planes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arcore_planes'
      )) as HandlerResult;

    case 'get_arcore_points':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arcore_points'
      )) as HandlerResult;

    case 'create_arcore_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_arcore_anchor'
      )) as HandlerResult;

    case 'remove_arcore_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for remove_arcore_anchor'
      )) as HandlerResult;

    case 'enable_depth_api':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_depth_api'
      )) as HandlerResult;

    case 'get_depth_image':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_depth_image'
      )) as HandlerResult;

    case 'enable_geospatial':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_geospatial'
      )) as HandlerResult;

    case 'get_geospatial_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_geospatial_pose'
      )) as HandlerResult;

    case 'create_geospatial_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_geospatial_anchor'
      )) as HandlerResult;

    case 'resolve_cloud_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for resolve_cloud_anchor'
      )) as HandlerResult;

    case 'host_cloud_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for host_cloud_anchor'
      )) as HandlerResult;

    case 'enable_arcore_augmented_images':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_arcore_augmented_images'
      )) as HandlerResult;

    case 'get_arcore_light_estimate':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arcore_light_estimate'
      )) as HandlerResult;

    case 'perform_arcore_raycast':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for perform_arcore_raycast'
      )) as HandlerResult;

    // =========================================
    // VARJO - High-End VR/XR Platform (16 actions)
    // =========================================
    case 'get_varjo_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_varjo_info'
      )) as HandlerResult;

    case 'configure_varjo_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_varjo_settings'
      )) as HandlerResult;

    case 'enable_varjo_passthrough':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_varjo_passthrough'
      )) as HandlerResult;

    case 'disable_varjo_passthrough':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_varjo_passthrough'
      )) as HandlerResult;

    case 'configure_varjo_depth_test':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_varjo_depth_test'
      )) as HandlerResult;

    case 'enable_varjo_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_varjo_eye_tracking'
      )) as HandlerResult;

    case 'get_varjo_gaze_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_varjo_gaze_data'
      )) as HandlerResult;

    case 'calibrate_varjo_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for calibrate_varjo_eye_tracking'
      )) as HandlerResult;

    case 'enable_foveated_rendering':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_foveated_rendering'
      )) as HandlerResult;

    case 'configure_foveated_rendering':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_foveated_rendering'
      )) as HandlerResult;

    case 'enable_varjo_mixed_reality':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_varjo_mixed_reality'
      )) as HandlerResult;

    case 'configure_varjo_chroma_key':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_varjo_chroma_key'
      )) as HandlerResult;

    case 'get_varjo_camera_intrinsics':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_varjo_camera_intrinsics'
      )) as HandlerResult;

    case 'enable_varjo_depth_estimation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_varjo_depth_estimation'
      )) as HandlerResult;

    case 'get_varjo_environment_cubemap':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_varjo_environment_cubemap'
      )) as HandlerResult;

    case 'configure_varjo_markers':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_varjo_markers'
      )) as HandlerResult;

    // =========================================
    // MICROSOFT HOLOLENS - Mixed Reality Platform (20 actions)
    // =========================================
    case 'get_hololens_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hololens_info'
      )) as HandlerResult;

    case 'configure_hololens_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_hololens_settings'
      )) as HandlerResult;

    case 'enable_spatial_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_spatial_mapping'
      )) as HandlerResult;

    case 'disable_spatial_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_spatial_mapping'
      )) as HandlerResult;

    case 'get_spatial_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_spatial_mesh'
      )) as HandlerResult;

    case 'configure_spatial_mapping_quality':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_spatial_mapping_quality'
      )) as HandlerResult;

    case 'enable_scene_understanding':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_scene_understanding'
      )) as HandlerResult;

    case 'get_scene_objects':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_scene_objects'
      )) as HandlerResult;

    case 'enable_qr_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_qr_tracking'
      )) as HandlerResult;

    case 'get_tracked_qr_codes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_qr_codes'
      )) as HandlerResult;

    case 'create_world_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_world_anchor'
      )) as HandlerResult;

    case 'save_world_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for save_world_anchor'
      )) as HandlerResult;

    case 'load_world_anchors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for load_world_anchors'
      )) as HandlerResult;

    case 'enable_hololens_hand_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_hololens_hand_tracking'
      )) as HandlerResult;

    case 'get_hololens_hand_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hololens_hand_mesh'
      )) as HandlerResult;

    case 'enable_hololens_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_hololens_eye_tracking'
      )) as HandlerResult;

    case 'get_hololens_gaze_ray':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hololens_gaze_ray'
      )) as HandlerResult;

    case 'register_voice_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for register_voice_command'
      )) as HandlerResult;

    case 'unregister_voice_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for unregister_voice_command'
      )) as HandlerResult;

    case 'get_registered_voice_commands':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_registered_voice_commands'
      )) as HandlerResult;

    // =========================================
    // COMMON XR UTILITIES (6 actions)
    // =========================================
    case 'get_xr_system_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_xr_system_info'
      )) as HandlerResult;

    case 'list_xr_devices':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for list_xr_devices'
      )) as HandlerResult;

    case 'set_xr_device_priority':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_xr_device_priority'
      )) as HandlerResult;

    case 'reset_xr_orientation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for reset_xr_orientation'
      )) as HandlerResult;

    case 'configure_xr_spectator':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_xr_spectator'
      )) as HandlerResult;

    case 'get_xr_runtime_name':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_xr_runtime_name'
      )) as HandlerResult;

    default:
      return {
        success: false,
        error: `Unknown manage_xr action: ${action}`,
        hint: 'Check available actions in the tool schema'
      };
  }
}
