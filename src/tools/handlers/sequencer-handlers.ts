/**
 * Phase 30: Cinematics & Media - Sequencer Handlers
 * Handles Level Sequences, master sequences, shot tracks, camera cuts,
 * actor binding, tracks/sections, keyframes, and playback control.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, normalizeLocation, normalizeRotation } from './common-handlers.js';

/**
 * Main handler for sequencer tools
 */
export async function handleSequencerTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown> {
  // Normalize location/rotation if present
  const normalizedLocation = normalizeLocation(args.location);
  const normalizedRotation = normalizeRotation(args.rotation as [number, number, number] | { pitch: number; yaw: number; roll: number } | null | undefined);

  // Build the payload for automation request
  const payload: Record<string, unknown> = {
    action: action,
    ...args,
    location: normalizedLocation,
    rotation: normalizedRotation
  };

  // Remove undefined values
  Object.keys(payload).forEach(key => {
    if (payload[key] === undefined) {
      delete payload[key];
    }
  });

  switch (action) {
    // =========================================
    // SEQUENCE CREATION & MANAGEMENT
    // =========================================
    case 'create_master_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for create_master_sequence'
      ));

    case 'add_subsequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_subsequence'
      ));

    case 'remove_subsequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_subsequence'
      ));

    case 'get_subsequences':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_subsequences'
      ));

    // =========================================
    // SHOT TRACKS
    // =========================================
    case 'add_shot_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_shot_track'
      ));

    case 'add_shot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_shot'
      ));

    case 'remove_shot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_shot'
      ));

    case 'get_shots':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_shots'
      ));

    // =========================================
    // CAMERA
    // =========================================
    case 'create_cine_camera_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for create_cine_camera_actor'
      ));

    case 'configure_camera_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for configure_camera_settings'
      ));

    case 'add_camera_cut_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_camera_cut_track'
      ));

    case 'add_camera_cut':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_camera_cut'
      ));

    // =========================================
    // ACTOR BINDING
    // =========================================
    case 'bind_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for bind_actor'
      ));

    case 'unbind_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for unbind_actor'
      ));

    case 'get_bindings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_bindings'
      ));

    // =========================================
    // TRACKS & SECTIONS
    // =========================================
    case 'add_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_track'
      ));

    case 'remove_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_track'
      ));

    case 'add_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_section'
      ));

    case 'remove_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_section'
      ));

    case 'get_tracks':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_tracks'
      ));

    // =========================================
    // KEYFRAMES
    // =========================================
    case 'add_keyframe':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_keyframe'
      ));

    case 'remove_keyframe':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_keyframe'
      ));

    case 'get_keyframes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_keyframes'
      ));

    // =========================================
    // PLAYBACK RANGE & PROPERTIES
    // =========================================
    case 'set_playback_range':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for set_playback_range'
      ));

    case 'get_playback_range':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_playback_range'
      ));

    case 'set_display_rate':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for set_display_rate'
      ));

    case 'get_sequence_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_sequence_info'
      ));

    // =========================================
    // RUNTIME CONTROL
    // =========================================
    case 'play_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for play_sequence'
      ));

    case 'pause_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for pause_sequence'
      ));

    case 'stop_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for stop_sequence'
      ));

    case 'scrub_to_time':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for scrub_to_time'
      ));

    // =========================================
    // UTILITIES
    // =========================================
    case 'list_sequences':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for list_sequences'
      ));

    case 'duplicate_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for duplicate_sequence'
      ));

    case 'delete_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for delete_sequence'
      ));

    case 'export_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for export_sequence'
      ));

    // =========================================
    // DEFAULT - PASS THROUGH
    // =========================================
    default:
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        `Automation bridge not available for sequencer action: ${action}`
      ));
  }
}
