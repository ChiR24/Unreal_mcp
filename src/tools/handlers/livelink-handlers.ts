/**
 * Phase 39: Motion Capture & Live Link Handlers
 * Handles Live Link core, face tracking, and motion capture integrations.
 * ~70 actions covering source management, subjects, presets, face capture, and external mocap systems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for Live Link tools
 */
export async function handleLiveLinkTools(
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
    // LIVE LINK CORE - Sources (15 actions)
    // =========================================
    case 'add_livelink_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_livelink_source'
      ));

    case 'remove_livelink_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for remove_livelink_source'
      ));

    case 'list_livelink_sources':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for list_livelink_sources'
      ));

    case 'get_source_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_source_status'
      ));

    case 'get_source_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_source_type'
      ));

    case 'configure_source_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_source_settings'
      ));

    case 'add_messagebus_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_messagebus_source'
      ));

    case 'discover_messagebus_sources':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for discover_messagebus_sources'
      ));

    case 'remove_all_sources':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for remove_all_sources'
      ));

    // =========================================
    // LIVE LINK CORE - Subjects (15 actions)
    // =========================================
    case 'list_livelink_subjects':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for list_livelink_subjects'
      ));

    case 'get_subject_role':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_role'
      ));

    case 'get_subject_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_state'
      ));

    case 'enable_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for enable_subject'
      ));

    case 'disable_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for disable_subject'
      ));

    case 'pause_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for pause_subject'
      ));

    case 'unpause_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for unpause_subject'
      ));

    case 'clear_subject_frames':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for clear_subject_frames'
      ));

    case 'get_subject_static_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_static_data'
      ));

    case 'get_subject_frame_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_frame_data'
      ));

    case 'add_virtual_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_virtual_subject'
      ));

    case 'remove_virtual_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for remove_virtual_subject'
      ));

    case 'configure_subject_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_subject_settings'
      ));

    case 'get_subject_frame_times':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_frame_times'
      ));

    case 'get_subjects_by_role':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subjects_by_role'
      ));

    // =========================================
    // LIVE LINK PRESETS (8 actions)
    // =========================================
    case 'create_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for create_livelink_preset'
      ));

    case 'load_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for load_livelink_preset'
      ));

    case 'apply_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for apply_livelink_preset'
      ));

    case 'add_preset_to_client':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_preset_to_client'
      ));

    case 'build_preset_from_client':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for build_preset_from_client'
      ));

    case 'save_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for save_livelink_preset'
      ));

    case 'get_preset_sources':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_preset_sources'
      ));

    case 'get_preset_subjects':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_preset_subjects'
      ));

    // =========================================
    // LIVE LINK COMPONENTS (8 actions)
    // =========================================
    case 'add_livelink_controller':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_livelink_controller'
      ));

    case 'configure_livelink_controller':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_livelink_controller'
      ));

    case 'set_controller_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_controller_subject'
      ));

    case 'set_controller_role':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_controller_role'
      ));

    case 'enable_controller_evaluation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for enable_controller_evaluation'
      ));

    case 'disable_controller_evaluation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for disable_controller_evaluation'
      ));

    case 'set_controlled_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_controlled_component'
      ));

    case 'get_controller_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_controller_info'
      ));

    // =========================================
    // LIVE LINK TIMECODE (6 actions)
    // =========================================
    case 'configure_livelink_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_livelink_timecode'
      ));

    case 'set_timecode_provider':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_timecode_provider'
      ));

    case 'get_livelink_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_livelink_timecode'
      ));

    case 'configure_time_sync':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_time_sync'
      ));

    case 'set_buffer_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_buffer_settings'
      ));

    case 'configure_frame_interpolation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_frame_interpolation'
      ));

    // =========================================
    // LIVE LINK FACE (8 actions)
    // =========================================
    case 'configure_face_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_face_source'
      ));

    case 'configure_arkit_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_arkit_mapping'
      ));

    case 'set_face_neutral_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_face_neutral_pose'
      ));

    case 'get_face_blendshapes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_face_blendshapes'
      ));

    case 'configure_blendshape_remap':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_blendshape_remap'
      ));

    case 'apply_face_to_skeletal_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for apply_face_to_skeletal_mesh'
      ));

    case 'configure_face_retargeting':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_face_retargeting'
      ));

    case 'get_face_tracking_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_face_tracking_status'
      ));

    // =========================================
    // MOTION CAPTURE - SKELETON MAPPING (6 actions)
    // =========================================
    case 'configure_skeleton_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_skeleton_mapping'
      ));

    case 'create_retarget_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for create_retarget_asset'
      ));

    case 'configure_bone_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_bone_mapping'
      ));

    case 'configure_curve_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_curve_mapping'
      ));

    case 'apply_mocap_to_character':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for apply_mocap_to_character'
      ));

    case 'get_skeleton_mapping_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_skeleton_mapping_info'
      ));

    // =========================================
    // UTILITY (4 actions)
    // =========================================
    case 'get_livelink_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_livelink_info'
      ));

    case 'list_available_roles':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for list_available_roles'
      ));

    case 'list_source_factories':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for list_source_factories'
      ));

    case 'force_livelink_tick':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for force_livelink_tick'
      ));

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown Live Link action: ${action}`
      });
  }
}
