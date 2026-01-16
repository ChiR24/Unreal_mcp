/**
 * Phase 39: Motion Capture & Live Link Handlers
 * Handles Live Link core, face tracking, and motion capture integrations.
 * 64 actions covering source management, subjects, presets, face capture, and external mocap systems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for Live Link tools
 */
export async function handleLiveLinkTools(
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
    // LIVE LINK CORE - Sources (15 actions)
    // =========================================
    case 'add_livelink_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_livelink_source'
      )) as HandlerResult;

    case 'remove_livelink_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for remove_livelink_source'
      )) as HandlerResult;

    case 'list_livelink_sources':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for list_livelink_sources'
      )) as HandlerResult;

    case 'get_source_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_source_status'
      )) as HandlerResult;

    case 'get_source_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_source_type'
      )) as HandlerResult;

    case 'configure_source_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_source_settings'
      )) as HandlerResult;

    case 'add_messagebus_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_messagebus_source'
      )) as HandlerResult;

    case 'discover_messagebus_sources':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for discover_messagebus_sources'
      )) as HandlerResult;

    case 'remove_all_sources':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for remove_all_sources'
      )) as HandlerResult;

    // =========================================
    // LIVE LINK CORE - Subjects (15 actions)
    // =========================================
    case 'list_livelink_subjects':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for list_livelink_subjects'
      )) as HandlerResult;

    case 'get_subject_role':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_role'
      )) as HandlerResult;

    case 'get_subject_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_state'
      )) as HandlerResult;

    case 'enable_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for enable_subject'
      )) as HandlerResult;

    case 'disable_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for disable_subject'
      )) as HandlerResult;

    case 'pause_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for pause_subject'
      )) as HandlerResult;

    case 'unpause_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for unpause_subject'
      )) as HandlerResult;

    case 'clear_subject_frames':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for clear_subject_frames'
      )) as HandlerResult;

    case 'get_subject_static_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_static_data'
      )) as HandlerResult;

    case 'get_subject_frame_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_frame_data'
      )) as HandlerResult;

    case 'add_virtual_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_virtual_subject'
      )) as HandlerResult;

    case 'remove_virtual_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for remove_virtual_subject'
      )) as HandlerResult;

    case 'configure_subject_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_subject_settings'
      )) as HandlerResult;

    case 'get_subject_frame_times':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subject_frame_times'
      )) as HandlerResult;

    case 'get_subjects_by_role':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_subjects_by_role'
      )) as HandlerResult;

    // =========================================
    // LIVE LINK PRESETS (8 actions)
    // =========================================
    case 'create_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for create_livelink_preset'
      )) as HandlerResult;

    case 'load_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for load_livelink_preset'
      )) as HandlerResult;

    case 'apply_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for apply_livelink_preset'
      )) as HandlerResult;

    case 'add_preset_to_client':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_preset_to_client'
      )) as HandlerResult;

    case 'build_preset_from_client':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for build_preset_from_client'
      )) as HandlerResult;

    case 'save_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for save_livelink_preset'
      )) as HandlerResult;

    case 'get_preset_sources':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_preset_sources'
      )) as HandlerResult;

    case 'get_preset_subjects':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_preset_subjects'
      )) as HandlerResult;

    // =========================================
    // LIVE LINK COMPONENTS (8 actions)
    // =========================================
    case 'add_livelink_controller':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for add_livelink_controller'
      )) as HandlerResult;

    case 'configure_livelink_controller':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_livelink_controller'
      )) as HandlerResult;

    case 'set_controller_subject':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_controller_subject'
      )) as HandlerResult;

    case 'set_controller_role':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_controller_role'
      )) as HandlerResult;

    case 'enable_controller_evaluation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for enable_controller_evaluation'
      )) as HandlerResult;

    case 'disable_controller_evaluation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for disable_controller_evaluation'
      )) as HandlerResult;

    case 'set_controlled_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_controlled_component'
      )) as HandlerResult;

    case 'get_controller_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_controller_info'
      )) as HandlerResult;

    // =========================================
    // LIVE LINK TIMECODE (6 actions)
    // =========================================
    case 'configure_livelink_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_livelink_timecode'
      )) as HandlerResult;

    case 'set_timecode_provider':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_timecode_provider'
      )) as HandlerResult;

    case 'get_livelink_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_livelink_timecode'
      )) as HandlerResult;

    case 'configure_time_sync':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_time_sync'
      )) as HandlerResult;

    case 'set_buffer_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_buffer_settings'
      )) as HandlerResult;

    case 'configure_frame_interpolation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_frame_interpolation'
      )) as HandlerResult;

    // =========================================
    // LIVE LINK FACE (8 actions)
    // =========================================
    case 'configure_face_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_face_source'
      )) as HandlerResult;

    case 'configure_arkit_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_arkit_mapping'
      )) as HandlerResult;

    case 'set_face_neutral_pose':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_face_neutral_pose'
      )) as HandlerResult;

    case 'get_face_blendshapes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_face_blendshapes'
      )) as HandlerResult;

    case 'configure_blendshape_remap':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_blendshape_remap'
      )) as HandlerResult;

    case 'apply_face_to_skeletal_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for apply_face_to_skeletal_mesh'
      )) as HandlerResult;

    case 'configure_face_retargeting':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_face_retargeting'
      )) as HandlerResult;

    case 'get_face_tracking_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_face_tracking_status'
      )) as HandlerResult;

    // =========================================
    // MOTION CAPTURE - SKELETON MAPPING (6 actions)
    // =========================================
    case 'configure_skeleton_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_skeleton_mapping'
      )) as HandlerResult;

    case 'create_retarget_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for create_retarget_asset'
      )) as HandlerResult;

    case 'configure_bone_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_bone_mapping'
      )) as HandlerResult;

    case 'configure_curve_mapping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_curve_mapping'
      )) as HandlerResult;

    case 'apply_mocap_to_character':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for apply_mocap_to_character'
      )) as HandlerResult;

    case 'get_skeleton_mapping_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_skeleton_mapping_info'
      )) as HandlerResult;

    // =========================================
    // UTILITY (4 actions)
    // =========================================
    case 'get_livelink_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_livelink_info'
      )) as HandlerResult;

    case 'list_available_roles':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for list_available_roles'
      )) as HandlerResult;

    case 'list_source_factories':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for list_source_factories'
      )) as HandlerResult;

    case 'force_livelink_tick':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for force_livelink_tick'
      )) as HandlerResult;

    // =========================================
    // WAVE 7.16-7.25 ADDITIONS
    // =========================================
    case 'get_livelink_subjects':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_livelink_subjects'
      )) as HandlerResult;

    case 'configure_livelink_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_livelink_preset'
      )) as HandlerResult;

    case 'record_livelink_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for record_livelink_data'
      )) as HandlerResult;

    case 'get_livelink_frame':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_livelink_frame'
      )) as HandlerResult;

    case 'set_subject_role':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for set_subject_role'
      )) as HandlerResult;

    case 'configure_face_tracking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for configure_face_tracking'
      )) as HandlerResult;

    case 'calibrate_livelink':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for calibrate_livelink'
      )) as HandlerResult;

    case 'get_livelink_statistics':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_livelink',
        payload,
        'Automation bridge not available for get_livelink_statistics'
      )) as HandlerResult;

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown Live Link action: ${action}`
      });
  }
}
