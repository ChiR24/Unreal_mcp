/**
 * Phase 41: XR Plugins (VR/AR/MR) Handlers
 * Handles OpenXR, Meta Quest, SteamVR, Apple ARKit, Google ARCore, Varjo, HoloLens.
 * ~140 actions across 7 XR platform subsystems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for XR Plugins tools
 */
export async function handleXRPluginsTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown> {
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
      ));

    case 'configure_openxr_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_openxr_settings'
      ));

    case 'set_tracking_origin':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_tracking_origin'
      ));

    case 'get_tracking_origin':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracking_origin'
      ));

    case 'create_xr_action_set':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_xr_action_set'
      ));

    case 'add_xr_action':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for add_xr_action'
      ));

    case 'bind_xr_action':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for bind_xr_action'
      ));

    case 'get_xr_action_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_xr_action_state'
      ));

    case 'trigger_haptic_feedback':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for trigger_haptic_feedback'
      ));

    case 'stop_haptic_feedback':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for stop_haptic_feedback'
      ));

    case 'get_hmd_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hmd_pose'
      ));

    case 'get_controller_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_controller_pose'
      ));

    case 'get_hand_tracking_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hand_tracking_data'
      ));

    case 'enable_hand_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_hand_tracking'
      ));

    case 'disable_hand_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_hand_tracking'
      ));

    case 'get_eye_tracking_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_eye_tracking_data'
      ));

    case 'enable_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_eye_tracking'
      ));

    case 'get_view_configuration':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_view_configuration'
      ));

    case 'set_render_scale':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_render_scale'
      ));

    case 'get_supported_extensions':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_supported_extensions'
      ));

    // =========================================
    // META QUEST - Oculus Platform (22 actions)
    // =========================================
    case 'get_quest_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_info'
      ));

    case 'configure_quest_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_quest_settings'
      ));

    case 'enable_passthrough':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_passthrough'
      ));

    case 'disable_passthrough':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_passthrough'
      ));

    case 'configure_passthrough_style':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_passthrough_style'
      ));

    case 'enable_scene_capture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_scene_capture'
      ));

    case 'get_scene_anchors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_scene_anchors'
      ));

    case 'get_room_layout':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_room_layout'
      ));

    case 'enable_quest_hand_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_quest_hand_tracking'
      ));

    case 'get_quest_hand_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_hand_pose'
      ));

    case 'enable_quest_face_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_quest_face_tracking'
      ));

    case 'get_quest_face_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_face_state'
      ));

    case 'enable_quest_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_quest_eye_tracking'
      ));

    case 'get_quest_eye_gaze':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_eye_gaze'
      ));

    case 'enable_quest_body_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_quest_body_tracking'
      ));

    case 'get_quest_body_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_quest_body_state'
      ));

    case 'create_spatial_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_spatial_anchor'
      ));

    case 'save_spatial_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for save_spatial_anchor'
      ));

    case 'load_spatial_anchors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for load_spatial_anchors'
      ));

    case 'delete_spatial_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for delete_spatial_anchor'
      ));

    case 'configure_guardian_bounds':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_guardian_bounds'
      ));

    case 'get_guardian_geometry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_guardian_geometry'
      ));

    // =========================================
    // STEAMVR - Valve/HTC Platform (18 actions)
    // =========================================
    case 'get_steamvr_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_steamvr_info'
      ));

    case 'configure_steamvr_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_steamvr_settings'
      ));

    case 'configure_chaperone_bounds':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_chaperone_bounds'
      ));

    case 'get_chaperone_geometry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_chaperone_geometry'
      ));

    case 'create_steamvr_overlay':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_steamvr_overlay'
      ));

    case 'set_overlay_texture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_overlay_texture'
      ));

    case 'show_overlay':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for show_overlay'
      ));

    case 'hide_overlay':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for hide_overlay'
      ));

    case 'destroy_overlay':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for destroy_overlay'
      ));

    case 'get_tracked_device_count':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_device_count'
      ));

    case 'get_tracked_device_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_device_info'
      ));

    case 'get_lighthouse_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_lighthouse_info'
      ));

    case 'trigger_steamvr_haptic':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for trigger_steamvr_haptic'
      ));

    case 'get_steamvr_action_manifest':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_steamvr_action_manifest'
      ));

    case 'set_steamvr_action_manifest':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_steamvr_action_manifest'
      ));

    case 'enable_steamvr_skeletal_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_steamvr_skeletal_input'
      ));

    case 'get_skeletal_bone_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_skeletal_bone_data'
      ));

    case 'configure_steamvr_render':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_steamvr_render'
      ));

    // =========================================
    // APPLE ARKIT - iOS AR Platform (22 actions)
    // =========================================
    case 'get_arkit_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arkit_info'
      ));

    case 'configure_arkit_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_arkit_session'
      ));

    case 'start_arkit_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for start_arkit_session'
      ));

    case 'pause_arkit_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for pause_arkit_session'
      ));

    case 'configure_world_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_world_tracking'
      ));

    case 'get_tracked_planes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_planes'
      ));

    case 'get_tracked_images':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_images'
      ));

    case 'add_reference_image':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for add_reference_image'
      ));

    case 'enable_people_occlusion':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_people_occlusion'
      ));

    case 'disable_people_occlusion':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_people_occlusion'
      ));

    case 'enable_arkit_face_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_arkit_face_tracking'
      ));

    case 'get_arkit_face_blendshapes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arkit_face_blendshapes'
      ));

    case 'get_arkit_face_geometry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arkit_face_geometry'
      ));

    case 'enable_body_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_body_tracking'
      ));

    case 'get_body_skeleton':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_body_skeleton'
      ));

    case 'create_arkit_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_arkit_anchor'
      ));

    case 'remove_arkit_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for remove_arkit_anchor'
      ));

    case 'get_light_estimation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_light_estimation'
      ));

    case 'enable_scene_reconstruction':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_scene_reconstruction'
      ));

    case 'get_scene_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_scene_mesh'
      ));

    case 'perform_raycast':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for perform_raycast'
      ));

    case 'get_camera_intrinsics':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_camera_intrinsics'
      ));

    // =========================================
    // GOOGLE ARCORE - Android AR Platform (18 actions)
    // =========================================
    case 'get_arcore_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arcore_info'
      ));

    case 'configure_arcore_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_arcore_session'
      ));

    case 'start_arcore_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for start_arcore_session'
      ));

    case 'pause_arcore_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for pause_arcore_session'
      ));

    case 'get_arcore_planes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arcore_planes'
      ));

    case 'get_arcore_points':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arcore_points'
      ));

    case 'create_arcore_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_arcore_anchor'
      ));

    case 'remove_arcore_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for remove_arcore_anchor'
      ));

    case 'enable_depth_api':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_depth_api'
      ));

    case 'get_depth_image':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_depth_image'
      ));

    case 'enable_geospatial':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_geospatial'
      ));

    case 'get_geospatial_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_geospatial_pose'
      ));

    case 'create_geospatial_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_geospatial_anchor'
      ));

    case 'resolve_cloud_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for resolve_cloud_anchor'
      ));

    case 'host_cloud_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for host_cloud_anchor'
      ));

    case 'enable_arcore_augmented_images':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_arcore_augmented_images'
      ));

    case 'get_arcore_light_estimate':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_arcore_light_estimate'
      ));

    case 'perform_arcore_raycast':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for perform_arcore_raycast'
      ));

    // =========================================
    // VARJO - High-End VR/XR Platform (16 actions)
    // =========================================
    case 'get_varjo_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_varjo_info'
      ));

    case 'configure_varjo_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_varjo_settings'
      ));

    case 'enable_varjo_passthrough':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_varjo_passthrough'
      ));

    case 'disable_varjo_passthrough':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_varjo_passthrough'
      ));

    case 'configure_varjo_depth_test':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_varjo_depth_test'
      ));

    case 'enable_varjo_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_varjo_eye_tracking'
      ));

    case 'get_varjo_gaze_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_varjo_gaze_data'
      ));

    case 'calibrate_varjo_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for calibrate_varjo_eye_tracking'
      ));

    case 'enable_foveated_rendering':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_foveated_rendering'
      ));

    case 'configure_foveated_rendering':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_foveated_rendering'
      ));

    case 'enable_varjo_mixed_reality':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_varjo_mixed_reality'
      ));

    case 'configure_varjo_chroma_key':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_varjo_chroma_key'
      ));

    case 'get_varjo_camera_intrinsics':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_varjo_camera_intrinsics'
      ));

    case 'enable_varjo_depth_estimation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_varjo_depth_estimation'
      ));

    case 'get_varjo_environment_cubemap':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_varjo_environment_cubemap'
      ));

    case 'configure_varjo_markers':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_varjo_markers'
      ));

    // =========================================
    // MICROSOFT HOLOLENS - Mixed Reality Platform (20 actions)
    // =========================================
    case 'get_hololens_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hololens_info'
      ));

    case 'configure_hololens_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_hololens_settings'
      ));

    case 'enable_spatial_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_spatial_mapping'
      ));

    case 'disable_spatial_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for disable_spatial_mapping'
      ));

    case 'get_spatial_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_spatial_mesh'
      ));

    case 'configure_spatial_mapping_quality':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_spatial_mapping_quality'
      ));

    case 'enable_scene_understanding':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_scene_understanding'
      ));

    case 'get_scene_objects':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_scene_objects'
      ));

    case 'enable_qr_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_qr_tracking'
      ));

    case 'get_tracked_qr_codes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_tracked_qr_codes'
      ));

    case 'create_world_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for create_world_anchor'
      ));

    case 'save_world_anchor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for save_world_anchor'
      ));

    case 'load_world_anchors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for load_world_anchors'
      ));

    case 'enable_hololens_hand_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_hololens_hand_tracking'
      ));

    case 'get_hololens_hand_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hololens_hand_mesh'
      ));

    case 'enable_hololens_eye_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for enable_hololens_eye_tracking'
      ));

    case 'get_hololens_gaze_ray':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_hololens_gaze_ray'
      ));

    case 'register_voice_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for register_voice_command'
      ));

    case 'unregister_voice_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for unregister_voice_command'
      ));

    case 'get_registered_voice_commands':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_registered_voice_commands'
      ));

    // =========================================
    // COMMON XR UTILITIES (6 actions)
    // =========================================
    case 'get_xr_system_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_xr_system_info'
      ));

    case 'list_xr_devices':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for list_xr_devices'
      ));

    case 'set_xr_device_priority':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for set_xr_device_priority'
      ));

    case 'reset_xr_orientation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for reset_xr_orientation'
      ));

    case 'configure_xr_spectator':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for configure_xr_spectator'
      ));

    case 'get_xr_runtime_name':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_xr',
        payload,
        'Automation bridge not available for get_xr_runtime_name'
      ));

    default:
      throw new Error(`Unknown manage_xr action: ${action}`);
  }
}
