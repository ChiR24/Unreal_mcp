import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import {
  DEFAULT_NIAGARA_EMITTER_NAME,
  ensureDefaultNiagaraAssets,
  ensureDefaultNiagaraAuthoringAssets,
  getLastAddedNiagaraUserParameterName,
  rememberLastAddedNiagaraUserParameterName
} from './effect-handler-state.js';

const EFFECT_GRAPH_ACTIONS = new Set<string>([
  'add_niagara_module',
  'connect_niagara_pins',
  'remove_niagara_node'
]);

const EFFECT_GRAPH_SUB_ACTIONS: Record<string, string> = {
  'add_niagara_module': 'add_module',
  'connect_niagara_pins': 'connect_pins',
  'remove_niagara_node': 'remove_node',
};

const EFFECT_AUTHORING_ACTIONS = new Set<string>([
  'add_emitter_to_system', 'set_emitter_properties',
  'add_spawn_rate_module', 'add_spawn_burst_module', 'add_spawn_per_unit_module',
  'add_initialize_particle_module', 'add_particle_state_module',
  'add_force_module', 'add_velocity_module', 'add_acceleration_module',
  'add_size_module', 'add_color_module',
  'add_sprite_renderer_module', 'add_mesh_renderer_module',
  'add_ribbon_renderer_module', 'add_light_renderer_module',
  'add_collision_module', 'add_kill_particles_module', 'add_camera_offset_module',
  'add_user_parameter', 'set_parameter_value', 'bind_parameter_to_source', 'set_niagara_dynamic_input',
  'add_skeletal_mesh_data_interface', 'add_static_mesh_data_interface',
  'add_spline_data_interface', 'add_audio_spectrum_data_interface',
  'add_collision_query_data_interface',
  'add_event_generator', 'add_event_receiver', 'configure_event_payload',
  'enable_gpu_simulation', 'add_simulation_stage',
  'get_niagara_info', 'validate_niagara_system'
]);

export async function handleEffectGraphAction(
  action: string,
  mutableArgs: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (!EFFECT_GRAPH_ACTIONS.has(action)) return undefined;

  const defaultAssets = await ensureDefaultNiagaraAssets(tools);
  mutableArgs.subAction = EFFECT_GRAPH_SUB_ACTIONS[action] || action;
  if (!mutableArgs.assetPath) {
    mutableArgs.assetPath = defaultAssets.systemPath;
  }
  if (!mutableArgs.systemPath) {
    mutableArgs.systemPath = mutableArgs.assetPath;
  }
  if (action === 'add_niagara_module' && !mutableArgs.modulePath) {
    mutableArgs.modulePath = '/Niagara/Modules/Emitter/EmitterState.EmitterState';
  }
  if (action === 'connect_niagara_pins' && !mutableArgs.fromNode && !mutableArgs.fromPin && !mutableArgs.toNode && !mutableArgs.toPin) {
    mutableArgs.autoConnect = true;
  }
  return executeAutomationRequest(tools, 'manage_niagara_graph', mutableArgs) as Promise<Record<string, unknown>>;
}

export async function handleEffectAuthoringAction(
  action: string,
  mutableArgs: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (!EFFECT_AUTHORING_ACTIONS.has(action)) return undefined;

  const usesImplicitSystem = !mutableArgs.systemPath;
  const defaultAssets = await ensureDefaultNiagaraAuthoringAssets(tools);
  if (!mutableArgs.systemPath) {
    mutableArgs.systemPath = defaultAssets.systemPath;
  }
  if (!mutableArgs.emitterPath) {
    mutableArgs.emitterPath = defaultAssets.emitterPath;
  }
  if (!mutableArgs.emitterName) {
    mutableArgs.emitterName = DEFAULT_NIAGARA_EMITTER_NAME;
  }
  if (!mutableArgs.assetPath) {
    mutableArgs.assetPath = defaultAssets.systemPath;
  }
  if (!mutableArgs.parameterName) {
    mutableArgs.parameterName = (mutableArgs.name as string | undefined) || 'MCPParameter';
  }
  if (action === 'add_user_parameter' && typeof mutableArgs.parameterName === 'string') {
    rememberLastAddedNiagaraUserParameterName(mutableArgs.parameterName);
  }
  if (
    action === 'set_parameter_value' &&
    (!mutableArgs.parameterName || mutableArgs.parameterName === mutableArgs.propertyName)
  ) {
    mutableArgs.parameterName = getLastAddedNiagaraUserParameterName() || 'MCPParameter';
  }
  if (!mutableArgs.parameterType) {
    mutableArgs.parameterType = 'Float';
  }
  if (!mutableArgs.forceType) {
    mutableArgs.forceType = 'gravity';
  }
  if (!mutableArgs.sourceBinding) {
    mutableArgs.sourceBinding = 'Emitter.Age';
  }
  if (!mutableArgs.eventName) {
    mutableArgs.eventName = (mutableArgs.name as string | undefined) || 'MCPEvent';
  }
  if (!mutableArgs.stageName) {
    mutableArgs.stageName = (mutableArgs.name as string | undefined) || 'MCPSimulationStage';
  }
  if (mutableArgs.value === undefined) {
    mutableArgs.value = 1;
  }
  if (usesImplicitSystem && mutableArgs.save === undefined) {
    mutableArgs.save = false;
  }
  return executeAutomationRequest(tools, 'manage_niagara_authoring', mutableArgs) as Promise<Record<string, unknown>>;
}
