/**
 * Phase 40: Virtual Production Plugins Handlers
 * Handles nDisplay, Composure, OCIO, Remote Control, DMX, OSC, MIDI, Timecode.
 * ~150 actions across 8 virtual production subsystems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for Virtual Production tools
 */
export async function handleVirtualProductionTools(
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
    // nDISPLAY - Cluster Configuration (10 actions)
    // =========================================
    case 'create_ndisplay_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_ndisplay_config'
      ));

    case 'add_cluster_node':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_cluster_node'
      ));

    case 'remove_cluster_node':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for remove_cluster_node'
      ));

    case 'add_viewport':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_viewport'
      ));

    case 'remove_viewport':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for remove_viewport'
      ));

    case 'set_viewport_camera':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_viewport_camera'
      ));

    case 'configure_viewport_region':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_viewport_region'
      ));

    case 'set_projection_policy':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_projection_policy'
      ));

    case 'configure_warp_blend':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_warp_blend'
      ));

    case 'list_cluster_nodes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_cluster_nodes'
      ));

    // =========================================
    // nDISPLAY - LED Wall / ICVFX (10 actions)
    // =========================================
    case 'create_led_wall':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_led_wall'
      ));

    case 'configure_led_wall_size':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_led_wall_size'
      ));

    case 'configure_icvfx_camera':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_icvfx_camera'
      ));

    case 'add_icvfx_camera':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_icvfx_camera'
      ));

    case 'remove_icvfx_camera':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for remove_icvfx_camera'
      ));

    case 'configure_inner_frustum':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_inner_frustum'
      ));

    case 'configure_outer_viewport':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_outer_viewport'
      ));

    case 'set_chromakey_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_chromakey_settings'
      ));

    case 'configure_light_cards':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_light_cards'
      ));

    case 'set_stage_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_stage_settings'
      ));

    // =========================================
    // nDISPLAY - Sync & Genlock (5 actions)
    // =========================================
    case 'set_sync_policy':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_sync_policy'
      ));

    case 'configure_genlock':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_genlock'
      ));

    case 'set_primary_node':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_primary_node'
      ));

    case 'configure_network_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_network_settings'
      ));

    case 'get_ndisplay_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_ndisplay_info'
      ));

    // =========================================
    // COMPOSURE - Elements & Layers (12 actions)
    // =========================================
    case 'create_composure_element':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_composure_element'
      ));

    case 'delete_composure_element':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for delete_composure_element'
      ));

    case 'add_composure_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_composure_layer'
      ));

    case 'remove_composure_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for remove_composure_layer'
      ));

    case 'attach_child_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for attach_child_layer'
      ));

    case 'detach_child_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for detach_child_layer'
      ));

    case 'add_input_pass':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_input_pass'
      ));

    case 'add_transform_pass':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_transform_pass'
      ));

    case 'add_output_pass':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_output_pass'
      ));

    case 'configure_chroma_keyer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_chroma_keyer'
      ));

    case 'bind_render_target':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_render_target'
      ));

    case 'get_composure_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_composure_info'
      ));

    // =========================================
    // OCIO - OpenColorIO (10 actions)
    // =========================================
    case 'create_ocio_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_ocio_config'
      ));

    case 'load_ocio_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for load_ocio_config'
      ));

    case 'get_ocio_colorspaces':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_ocio_colorspaces'
      ));

    case 'get_ocio_displays':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_ocio_displays'
      ));

    case 'set_display_view':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_display_view'
      ));

    case 'add_colorspace_transform':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_colorspace_transform'
      ));

    case 'apply_ocio_look':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for apply_ocio_look'
      ));

    case 'configure_viewport_ocio':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_viewport_ocio'
      ));

    case 'set_ocio_working_colorspace':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_ocio_working_colorspace'
      ));

    case 'get_ocio_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_ocio_info'
      ));

    // =========================================
    // REMOTE CONTROL - Presets & Properties (15 actions)
    // =========================================
    case 'create_remote_control_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_remote_control_preset'
      ));

    case 'load_remote_control_preset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for load_remote_control_preset'
      ));

    case 'expose_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for expose_property'
      ));

    case 'unexpose_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for unexpose_property'
      ));

    case 'expose_function':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for expose_function'
      ));

    case 'create_controller':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_controller'
      ));

    case 'bind_controller':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_controller'
      ));

    case 'get_exposed_properties':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_exposed_properties'
      ));

    case 'set_exposed_property_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_exposed_property_value'
      ));

    case 'get_exposed_property_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_exposed_property_value'
      ));

    case 'start_web_server':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for start_web_server'
      ));

    case 'stop_web_server':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for stop_web_server'
      ));

    case 'get_web_server_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_web_server_status'
      ));

    case 'create_layout_group':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_layout_group'
      ));

    case 'get_remote_control_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_remote_control_info'
      ));

    // =========================================
    // DMX - Library & Fixtures (20 actions)
    // =========================================
    case 'create_dmx_library':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_dmx_library'
      ));

    case 'import_gdtf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for import_gdtf'
      ));

    case 'create_fixture_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_fixture_type'
      ));

    case 'add_fixture_mode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_fixture_mode'
      ));

    case 'add_fixture_function':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_fixture_function'
      ));

    case 'create_fixture_patch':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_fixture_patch'
      ));

    case 'assign_fixture_to_universe':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for assign_fixture_to_universe'
      ));

    case 'configure_dmx_port':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_dmx_port'
      ));

    case 'create_artnet_port':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_artnet_port'
      ));

    case 'create_sacn_port':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_sacn_port'
      ));

    case 'send_dmx':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_dmx'
      ));

    case 'receive_dmx':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for receive_dmx'
      ));

    case 'set_fixture_channel_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_fixture_channel_value'
      ));

    case 'get_fixture_channel_value':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_fixture_channel_value'
      ));

    case 'add_dmx_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_dmx_component'
      ));

    case 'configure_dmx_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_dmx_component'
      ));

    case 'list_dmx_universes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_dmx_universes'
      ));

    case 'list_dmx_fixtures':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_dmx_fixtures'
      ));

    case 'create_dmx_sequencer_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_dmx_sequencer_track'
      ));

    case 'get_dmx_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_dmx_info'
      ));

    // =========================================
    // OSC - Open Sound Control (12 actions)
    // =========================================
    case 'create_osc_server':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_osc_server'
      ));

    case 'stop_osc_server':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for stop_osc_server'
      ));

    case 'create_osc_client':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_osc_client'
      ));

    case 'send_osc_message':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_osc_message'
      ));

    case 'send_osc_bundle':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_osc_bundle'
      ));

    case 'bind_osc_address':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_osc_address'
      ));

    case 'unbind_osc_address':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for unbind_osc_address'
      ));

    case 'bind_osc_to_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_osc_to_property'
      ));

    case 'list_osc_servers':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_osc_servers'
      ));

    case 'list_osc_clients':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_osc_clients'
      ));

    case 'configure_osc_dispatcher':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_osc_dispatcher'
      ));

    case 'get_osc_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_osc_info'
      ));

    // =========================================
    // MIDI - Device Integration (15 actions)
    // =========================================
    case 'list_midi_devices':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_midi_devices'
      ));

    case 'open_midi_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for open_midi_input'
      ));

    case 'close_midi_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for close_midi_input'
      ));

    case 'open_midi_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for open_midi_output'
      ));

    case 'close_midi_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for close_midi_output'
      ));

    case 'send_midi_note_on':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_note_on'
      ));

    case 'send_midi_note_off':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_note_off'
      ));

    case 'send_midi_cc':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_cc'
      ));

    case 'send_midi_pitch_bend':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_pitch_bend'
      ));

    case 'send_midi_program_change':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for send_midi_program_change'
      ));

    case 'bind_midi_to_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for bind_midi_to_property'
      ));

    case 'unbind_midi':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for unbind_midi'
      ));

    case 'configure_midi_learn':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_midi_learn'
      ));

    case 'add_midi_device_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_midi_device_component'
      ));

    case 'get_midi_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_midi_info'
      ));

    // =========================================
    // TIMECODE - Providers & Genlock (18 actions)
    // =========================================
    case 'create_timecode_provider':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_timecode_provider'
      ));

    case 'set_timecode_provider':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_timecode_provider'
      ));

    case 'get_current_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_current_timecode'
      ));

    case 'set_frame_rate':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_frame_rate'
      ));

    case 'configure_ltc_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_ltc_timecode'
      ));

    case 'configure_aja_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_aja_timecode'
      ));

    case 'configure_blackmagic_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_blackmagic_timecode'
      ));

    case 'configure_system_time_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_system_time_timecode'
      ));

    case 'enable_timecode_genlock':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for enable_timecode_genlock'
      ));

    case 'disable_timecode_genlock':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for disable_timecode_genlock'
      ));

    case 'set_custom_timestep':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for set_custom_timestep'
      ));

    case 'configure_genlock_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for configure_genlock_source'
      ));

    case 'get_timecode_provider_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_timecode_provider_status'
      ));

    case 'synchronize_timecode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for synchronize_timecode'
      ));

    case 'create_timecode_synchronizer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for create_timecode_synchronizer'
      ));

    case 'add_timecode_source':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for add_timecode_source'
      ));

    case 'list_timecode_providers':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_timecode_providers'
      ));

    case 'get_timecode_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_timecode_info'
      ));

    // =========================================
    // UTILITY (3 actions)
    // =========================================
    case 'get_virtual_production_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for get_virtual_production_info'
      ));

    case 'list_active_vp_sessions':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for list_active_vp_sessions'
      ));

    case 'reset_vp_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_virtual_production',
        payload,
        'Automation bridge not available for reset_vp_state'
      ));

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown virtual production action: ${action}. Available categories: nDisplay, Composure, OCIO, Remote Control, DMX, OSC, MIDI, Timecode.`
      });
  }
}
