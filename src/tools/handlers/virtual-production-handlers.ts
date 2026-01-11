/**
 * Phase 40: Virtual Production Plugins Handlers
 * Handles nDisplay, Composure, OCIO, Remote Control, DMX, OSC, MIDI, Timecode.
 * ~150 actions across 8 virtual production subsystems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for Virtual Production tools
 */
export async function handleVirtualProductionTools(
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
    // nDISPLAY - Cluster Configuration (10 actions)
    // =========================================
    case 'create_ndisplay_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_ndisplay_config'
      )) as HandlerResult;

    case 'add_cluster_node':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_cluster_node'
      )) as HandlerResult;

    case 'remove_cluster_node':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for remove_cluster_node'
      )) as HandlerResult;

    case 'add_viewport':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_viewport'
      )) as HandlerResult;

    case 'remove_viewport':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for remove_viewport'
      )) as HandlerResult;

    case 'set_viewport_camera':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_viewport_camera'
      )) as HandlerResult;

    case 'configure_viewport_region':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_viewport_region'
      )) as HandlerResult;

    case 'set_projection_policy':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_projection_policy'
      )) as HandlerResult;

    case 'configure_warp_blend':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_warp_blend'
      )) as HandlerResult;

    case 'list_cluster_nodes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_cluster_nodes'
      )) as HandlerResult;

    // =========================================
    // nDISPLAY - LED Wall / ICVFX (10 actions)
    // =========================================
    case 'create_led_wall':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_led_wall'
      )) as HandlerResult;

    case 'configure_led_wall_size':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_led_wall_size'
      )) as HandlerResult;

    case 'configure_icvfx_camera':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_icvfx_camera'
      )) as HandlerResult;

    case 'add_icvfx_camera':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_icvfx_camera'
      )) as HandlerResult;

    case 'remove_icvfx_camera':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for remove_icvfx_camera'
      )) as HandlerResult;

    case 'configure_inner_frustum':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_inner_frustum'
      )) as HandlerResult;

    case 'configure_outer_viewport':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_outer_viewport'
      )) as HandlerResult;

    case 'set_chromakey_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_chromakey_settings'
      )) as HandlerResult;

    case 'configure_light_cards':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_light_cards'
      )) as HandlerResult;

    case 'set_stage_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_stage_settings'
      )) as HandlerResult;

    // =========================================
    // nDISPLAY - Sync & Genlock (5 actions)
    // =========================================
    case 'set_sync_policy':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_sync_policy'
      )) as HandlerResult;

    case 'configure_genlock':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_genlock'
      )) as HandlerResult;

    case 'set_primary_node':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_primary_node'
      )) as HandlerResult;

    case 'configure_network_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_network_settings'
      )) as HandlerResult;

    case 'get_ndisplay_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_ndisplay_info'
      )) as HandlerResult;

    // =========================================
    // COMPOSURE - Elements & Layers (12 actions)
    // =========================================
    case 'create_composure_element':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_composure_element'
      )) as HandlerResult;

    case 'delete_composure_element':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for delete_composure_element'
      )) as HandlerResult;

    case 'add_composure_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_composure_layer'
      )) as HandlerResult;

    case 'remove_composure_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for remove_composure_layer'
      )) as HandlerResult;

    case 'attach_child_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for attach_child_layer'
      )) as HandlerResult;

    case 'detach_child_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for detach_child_layer'
      )) as HandlerResult;

    case 'add_input_pass':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_input_pass'
      )) as HandlerResult;

    case 'add_transform_pass':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_transform_pass'
      )) as HandlerResult;

    case 'add_output_pass':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_output_pass'
      )) as HandlerResult;

    case 'configure_chroma_keyer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_chroma_keyer'
      )) as HandlerResult;

    case 'bind_render_target':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_render_target'
      )) as HandlerResult;

    case 'get_composure_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_composure_info'
      )) as HandlerResult;

    // =========================================
    // OCIO - OpenColorIO (10 actions)
    // =========================================
    case 'create_ocio_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_ocio_config'
      )) as HandlerResult;

    case 'load_ocio_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for load_ocio_config'
      )) as HandlerResult;

    case 'get_ocio_colorspaces':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_ocio_colorspaces'
      )) as HandlerResult;

    case 'get_ocio_displays':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_ocio_displays'
      )) as HandlerResult;

    case 'set_display_view':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_display_view'
      )) as HandlerResult;

    case 'add_colorspace_transform':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_colorspace_transform'
      )) as HandlerResult;

    case 'apply_ocio_look':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for apply_ocio_look'
      )) as HandlerResult;

    case 'configure_viewport_ocio':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_viewport_ocio'
      )) as HandlerResult;

    case 'set_ocio_working_colorspace':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_ocio_working_colorspace'
      )) as HandlerResult;

    case 'get_ocio_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_ocio_info'
      )) as HandlerResult;

    // =========================================
    // REMOTE CONTROL - Presets & Properties (15 actions)
    // =========================================
    case 'create_remote_control_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_remote_control_preset'
      )) as HandlerResult;

    case 'load_remote_control_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for load_remote_control_preset'
      )) as HandlerResult;

    case 'expose_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for expose_property'
      )) as HandlerResult;

    case 'unexpose_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for unexpose_property'
      )) as HandlerResult;

    case 'expose_function':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for expose_function'
      )) as HandlerResult;

    case 'create_controller':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_controller'
      )) as HandlerResult;

    case 'bind_controller':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_controller'
      )) as HandlerResult;

    case 'get_exposed_properties':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_exposed_properties'
      )) as HandlerResult;

    case 'set_exposed_property_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_exposed_property_value'
      )) as HandlerResult;

    case 'get_exposed_property_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_exposed_property_value'
      )) as HandlerResult;

    case 'start_web_server':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for start_web_server'
      )) as HandlerResult;

    case 'stop_web_server':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for stop_web_server'
      )) as HandlerResult;

    case 'get_web_server_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_web_server_status'
      )) as HandlerResult;

    case 'create_layout_group':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_layout_group'
      )) as HandlerResult;

    case 'get_remote_control_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_remote_control_info'
      )) as HandlerResult;

    // =========================================
    // DMX - Library & Fixtures (20 actions)
    // =========================================
    case 'create_dmx_library':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_dmx_library'
      )) as HandlerResult;

    case 'import_gdtf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for import_gdtf'
      )) as HandlerResult;

    case 'create_fixture_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_fixture_type'
      )) as HandlerResult;

    case 'add_fixture_mode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_fixture_mode'
      )) as HandlerResult;

    case 'add_fixture_function':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_fixture_function'
      )) as HandlerResult;

    case 'create_fixture_patch':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_fixture_patch'
      )) as HandlerResult;

    case 'assign_fixture_to_universe':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for assign_fixture_to_universe'
      )) as HandlerResult;

    case 'configure_dmx_port':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_dmx_port'
      )) as HandlerResult;

    case 'create_artnet_port':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_artnet_port'
      )) as HandlerResult;

    case 'create_sacn_port':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_sacn_port'
      )) as HandlerResult;

    case 'send_dmx':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_dmx'
      )) as HandlerResult;

    case 'receive_dmx':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for receive_dmx'
      )) as HandlerResult;

    case 'set_fixture_channel_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_fixture_channel_value'
      )) as HandlerResult;

    case 'get_fixture_channel_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_fixture_channel_value'
      )) as HandlerResult;

    case 'add_dmx_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_dmx_component'
      )) as HandlerResult;

    case 'configure_dmx_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_dmx_component'
      )) as HandlerResult;

    case 'list_dmx_universes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_dmx_universes'
      )) as HandlerResult;

    case 'list_dmx_fixtures':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_dmx_fixtures'
      )) as HandlerResult;

    case 'create_dmx_sequencer_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_dmx_sequencer_track'
      )) as HandlerResult;

    case 'get_dmx_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_dmx_info'
      )) as HandlerResult;

    // =========================================
    // OSC - Open Sound Control (12 actions)
    // =========================================
    case 'create_osc_server':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_osc_server'
      )) as HandlerResult;

    case 'stop_osc_server':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for stop_osc_server'
      )) as HandlerResult;

    case 'create_osc_client':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_osc_client'
      )) as HandlerResult;

    case 'send_osc_message':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_osc_message'
      )) as HandlerResult;

    case 'send_osc_bundle':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_osc_bundle'
      )) as HandlerResult;

    case 'bind_osc_address':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_osc_address'
      )) as HandlerResult;

    case 'unbind_osc_address':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for unbind_osc_address'
      )) as HandlerResult;

    case 'bind_osc_to_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_osc_to_property'
      )) as HandlerResult;

    case 'list_osc_servers':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_osc_servers'
      )) as HandlerResult;

    case 'list_osc_clients':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_osc_clients'
      )) as HandlerResult;

    case 'configure_osc_dispatcher':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_osc_dispatcher'
      )) as HandlerResult;

    case 'get_osc_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_osc_info'
      )) as HandlerResult;

    // =========================================
    // MIDI - Device Integration (15 actions)
    // =========================================
    case 'list_midi_devices':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_midi_devices'
      )) as HandlerResult;

    case 'open_midi_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for open_midi_input'
      )) as HandlerResult;

    case 'close_midi_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for close_midi_input'
      )) as HandlerResult;

    case 'open_midi_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for open_midi_output'
      )) as HandlerResult;

    case 'close_midi_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for close_midi_output'
      )) as HandlerResult;

    case 'send_midi_note_on':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_note_on'
      )) as HandlerResult;

    case 'send_midi_note_off':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_note_off'
      )) as HandlerResult;

    case 'send_midi_cc':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_cc'
      )) as HandlerResult;

    case 'send_midi_pitch_bend':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_pitch_bend'
      )) as HandlerResult;

    case 'send_midi_program_change':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_program_change'
      )) as HandlerResult;

    case 'bind_midi_to_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_midi_to_property'
      )) as HandlerResult;

    case 'unbind_midi':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for unbind_midi'
      )) as HandlerResult;

    case 'configure_midi_learn':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_midi_learn'
      )) as HandlerResult;

    case 'add_midi_device_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_midi_device_component'
      )) as HandlerResult;

    case 'get_midi_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_midi_info'
      )) as HandlerResult;

    // =========================================
    // TIMECODE - Providers & Genlock (18 actions)
    // =========================================
    case 'create_timecode_provider':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_timecode_provider'
      )) as HandlerResult;

    case 'set_timecode_provider':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_timecode_provider'
      )) as HandlerResult;

    case 'get_current_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_current_timecode'
      )) as HandlerResult;

    case 'set_frame_rate':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_frame_rate'
      )) as HandlerResult;

    case 'configure_ltc_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_ltc_timecode'
      )) as HandlerResult;

    case 'configure_aja_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_aja_timecode'
      )) as HandlerResult;

    case 'configure_blackmagic_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_blackmagic_timecode'
      )) as HandlerResult;

    case 'configure_system_time_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_system_time_timecode'
      )) as HandlerResult;

    case 'enable_timecode_genlock':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for enable_timecode_genlock'
      )) as HandlerResult;

    case 'disable_timecode_genlock':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for disable_timecode_genlock'
      )) as HandlerResult;

    case 'set_custom_timestep':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_custom_timestep'
      )) as HandlerResult;

    case 'configure_genlock_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_genlock_source'
      )) as HandlerResult;

    case 'get_timecode_provider_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_timecode_provider_status'
      )) as HandlerResult;

    case 'synchronize_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for synchronize_timecode'
      )) as HandlerResult;

    case 'create_timecode_synchronizer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_timecode_synchronizer'
      )) as HandlerResult;

    case 'add_timecode_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_timecode_source'
      )) as HandlerResult;

    case 'list_timecode_providers':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_timecode_providers'
      )) as HandlerResult;

    case 'get_timecode_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_timecode_info'
      )) as HandlerResult;

    // =========================================
    // UTILITY (3 actions)
    // =========================================
    case 'get_virtual_production_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_virtual_production_info'
      )) as HandlerResult;

    case 'list_active_vp_sessions':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_active_vp_sessions'
      )) as HandlerResult;

    case 'reset_vp_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for reset_vp_state'
      )) as HandlerResult;

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown virtual production action: ${action}. Available categories: nDisplay, Composure, OCIO, Remote Control, DMX, OSC, MIDI, Timecode.`
      });
  }
}
