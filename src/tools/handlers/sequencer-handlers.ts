/**
 * Phase 30: Cinematics & Media - Sequencer Handlers
 * Handles Level Sequences, master sequences, shot tracks, camera cuts,
 * actor binding, tracks/sections, keyframes, and playback control.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest, normalizeLocation, normalizeRotation } from './common-handlers.js';

/**
 * Main handler for sequencer tools
 */
export async function handleSequencerTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<HandlerResult> {
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
      )) as HandlerResult;

    case 'add_subsequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_subsequence'
      )) as HandlerResult;

    case 'remove_subsequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_subsequence'
      )) as HandlerResult;

    case 'get_subsequences':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_subsequences'
      )) as HandlerResult;

    // =========================================
    // SHOT TRACKS
    // =========================================
    case 'add_shot_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_shot_track'
      )) as HandlerResult;

    case 'add_shot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_shot'
      )) as HandlerResult;

    case 'remove_shot':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_shot'
      )) as HandlerResult;

    case 'get_shots':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_shots'
      )) as HandlerResult;

    // =========================================
    // CAMERA
    // =========================================
    case 'create_cine_camera_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for create_cine_camera_actor'
      )) as HandlerResult;

    case 'configure_camera_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for configure_camera_settings'
      )) as HandlerResult;

    case 'add_camera_cut_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_camera_cut_track'
      )) as HandlerResult;

    case 'add_camera_cut':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_camera_cut'
      )) as HandlerResult;

    // =========================================
    // ACTOR BINDING
    // =========================================
    case 'bind_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for bind_actor'
      )) as HandlerResult;

    case 'unbind_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for unbind_actor'
      )) as HandlerResult;

    case 'get_bindings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_bindings'
      )) as HandlerResult;

    // =========================================
    // TRACKS & SECTIONS
    // =========================================
    case 'add_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_track'
      )) as HandlerResult;

    case 'remove_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_track'
      )) as HandlerResult;

    case 'add_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_section'
      )) as HandlerResult;

    case 'remove_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_section'
      )) as HandlerResult;

    case 'get_tracks':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_tracks'
      )) as HandlerResult;

    // =========================================
    // KEYFRAMES
    // =========================================
    case 'add_keyframe':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for add_keyframe'
      )) as HandlerResult;

    case 'remove_keyframe':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for remove_keyframe'
      )) as HandlerResult;

    case 'get_keyframes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_keyframes'
      )) as HandlerResult;

    // =========================================
    // PLAYBACK RANGE & PROPERTIES
    // =========================================
    case 'set_playback_range':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for set_playback_range'
      )) as HandlerResult;

    case 'get_playback_range':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_playback_range'
      )) as HandlerResult;

    case 'set_display_rate':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for set_display_rate'
      )) as HandlerResult;

    case 'get_sequence_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for get_sequence_info'
      )) as HandlerResult;

    // =========================================
    // RUNTIME CONTROL
    // =========================================
    case 'play_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for play_sequence'
      )) as HandlerResult;

    case 'pause_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for pause_sequence'
      )) as HandlerResult;

    case 'stop_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for stop_sequence'
      )) as HandlerResult;

    case 'scrub_to_time':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for scrub_to_time'
      )) as HandlerResult;

    // =========================================
    // UTILITIES
    // =========================================
    case 'list_sequences':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for list_sequences'
      )) as HandlerResult;

    case 'duplicate_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for duplicate_sequence'
      )) as HandlerResult;

    case 'delete_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for delete_sequence'
      )) as HandlerResult;

    case 'export_sequence':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        'Automation bridge not available for export_sequence'
      )) as HandlerResult;

    // =========================================
    // DEFAULT - PASS THROUGH
    // =========================================
    default:
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_sequencer',
        payload,
        `Automation bridge not available for sequencer action: ${action}`
      )) as HandlerResult;
  }
}
