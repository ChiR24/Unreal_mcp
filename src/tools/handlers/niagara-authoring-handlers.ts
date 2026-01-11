/**
 * Niagara Authoring Handlers (Phase 12)
 *
 * Complete Niagara VFX system authoring including:
 * - Systems & Emitters (create, configure, add emitters)
 * - Module Library (spawn, initialize, force, velocity, size, color, renderers, etc.)
 * - Parameters & Data Interfaces (user parameters, mesh/spline/audio data interfaces)
 * - Events & GPU Simulation
 * - Utility (get info, validate)
 *
 * @module niagara-authoring-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all Niagara authoring actions for the manage_niagara_authoring tool.
 */
export async function handleNiagaraAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  const argsRecord = args as HandlerResult;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<HandlerResult> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_niagara_authoring',
      payload as HandlerArgs,
      `Automation bridge not available for Niagara authoring action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };

  switch (action) {
    // =========================================================================
    // 12.1 Systems & Emitters (4 actions)
    // =========================================================================

    case 'create_niagara_system': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_niagara_system');
    }

    case 'create_niagara_emitter': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_niagara_emitter');
    }

    case 'add_emitter_to_system': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterPath, 'emitterPath', 'Missing required parameter: emitterPath');
      return sendRequest('add_emitter_to_system');
    }

    case 'set_emitter_properties': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('set_emitter_properties');
    }

    // =========================================================================
    // 12.2 Module Library (15 actions)
    // =========================================================================

    case 'add_spawn_rate_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_spawn_rate_module');
    }

    case 'add_spawn_burst_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_spawn_burst_module');
    }

    case 'add_spawn_per_unit_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_spawn_per_unit_module');
    }

    case 'add_initialize_particle_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_initialize_particle_module');
    }

    case 'add_particle_state_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_particle_state_module');
    }

    case 'add_force_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      requireNonEmptyString(argsRecord.forceType, 'forceType', 'Missing required parameter: forceType');
      return sendRequest('add_force_module');
    }

    case 'add_velocity_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_velocity_module');
    }

    case 'add_acceleration_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_acceleration_module');
    }

    case 'add_size_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_size_module');
    }

    case 'add_color_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_color_module');
    }

    case 'add_sprite_renderer_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_sprite_renderer_module');
    }

    case 'add_mesh_renderer_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_mesh_renderer_module');
    }

    case 'add_ribbon_renderer_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_ribbon_renderer_module');
    }

    case 'add_light_renderer_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_light_renderer_module');
    }

    case 'add_collision_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_collision_module');
    }

    case 'add_kill_particles_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_kill_particles_module');
    }

    case 'add_camera_offset_module': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_camera_offset_module');
    }

    // =========================================================================
    // 12.3 Parameters & Data Interfaces (7 actions)
    // =========================================================================

    case 'add_user_parameter': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      requireNonEmptyString(argsRecord.parameterType, 'parameterType', 'Missing required parameter: parameterType');
      return sendRequest('add_user_parameter');
    }

    case 'set_parameter_value': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_parameter_value');
    }

    case 'bind_parameter_to_source': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      requireNonEmptyString(argsRecord.sourceBinding, 'sourceBinding', 'Missing required parameter: sourceBinding');
      return sendRequest('bind_parameter_to_source');
    }

    case 'add_skeletal_mesh_data_interface': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_skeletal_mesh_data_interface');
    }

    case 'add_static_mesh_data_interface': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_static_mesh_data_interface');
    }

    case 'add_spline_data_interface': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_spline_data_interface');
    }

    case 'add_audio_spectrum_data_interface': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_audio_spectrum_data_interface');
    }

    case 'add_collision_query_data_interface': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('add_collision_query_data_interface');
    }

    // =========================================================================
    // 12.4 Events & GPU (5 actions)
    // =========================================================================

    case 'add_event_generator': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      requireNonEmptyString(argsRecord.eventName, 'eventName', 'Missing required parameter: eventName');
      return sendRequest('add_event_generator');
    }

    case 'add_event_receiver': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      requireNonEmptyString(argsRecord.eventName, 'eventName', 'Missing required parameter: eventName');
      return sendRequest('add_event_receiver');
    }

    case 'configure_event_payload': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.eventName, 'eventName', 'Missing required parameter: eventName');
      return sendRequest('configure_event_payload');
    }

    case 'enable_gpu_simulation': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      return sendRequest('enable_gpu_simulation');
    }

    case 'add_simulation_stage': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      requireNonEmptyString(argsRecord.emitterName, 'emitterName', 'Missing required parameter: emitterName');
      requireNonEmptyString(argsRecord.stageName, 'stageName', 'Missing required parameter: stageName');
      return sendRequest('add_simulation_stage');
    }

    // =========================================================================
    // 12.5 Utility (2 actions)
    // =========================================================================

    case 'get_niagara_info': {
      requireNonEmptyString(argsRecord.assetPath, 'assetPath', 'Missing required parameter: assetPath');
      return sendRequest('get_niagara_info');
    }

    case 'validate_niagara_system': {
      requireNonEmptyString(argsRecord.systemPath, 'systemPath', 'Missing required parameter: systemPath');
      return sendRequest('validate_niagara_system');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown Niagara authoring action: ${action}`
      });
  }
}
