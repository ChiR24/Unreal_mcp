// Fold specs for manage_effect. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_EFFECT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'activate', selector: 'control',
    summary: 'Activate, deactivate or reset a Niagara component on an actor, or activate an effect by asset or system name.',
    topics: ['activate effect', 'deactivate effect', 'reset effect', 'niagara component'],
    members: { activate: 'activate', deactivate: 'deactivate', reset: 'reset', effect: 'activate_effect' },
  },
  {
    primary: 'create_effect', selector: 'kind',
    summary: 'Create an effect: a Niagara system or emitter, a ribbon, particle trail, impact or environment effect, volumetric fog, or spawn a preset particle/niagara effect.',
    topics: ['niagara system', 'niagara emitter', 'ribbon', 'particle trail', 'impact effect', 'volumetric fog', 'effect asset'],
    members: {
      niagara_system: 'create_niagara_system', niagara_emitter: 'create_niagara_emitter', niagara_ribbon: 'create_niagara_ribbon', particle_trail: 'create_particle_trail',
      impact: 'create_impact_effect', environment: 'create_environment_effect', volumetric_fog: 'create_volumetric_fog', niagara: 'niagara', particle: 'particle',
    },
  },
  {
    primary: 'add_niagara_module', selector: 'moduleKind',
    summary: 'Add a module to a Niagara emitter: any module script by path, or a typed module (spawn rate/burst/per-unit, initialize particle, velocity, acceleration, force, color, size, collision, kill, camera offset, particle state, sprite/mesh/ribbon/light renderer, simulation stage, event generator/receiver).',
    topics: ['niagara module', 'spawn rate', 'sprite renderer', 'mesh renderer', 'ribbon renderer', 'particle velocity', 'particle color', 'simulation stage'],
    members: {
      module: 'add_niagara_module',
      ...byTarget('add_', ['add_spawn_rate_module', 'add_spawn_burst_module', 'add_spawn_per_unit_module', 'add_initialize_particle_module', 'add_velocity_module',
        'add_acceleration_module', 'add_force_module', 'add_color_module', 'add_size_module', 'add_collision_module', 'add_kill_particles_module',
        'add_camera_offset_module', 'add_particle_state_module', 'add_sprite_renderer_module', 'add_mesh_renderer_module', 'add_ribbon_renderer_module',
        'add_light_renderer_module'], '_module'),
      simulation_stage: 'add_simulation_stage', event_generator: 'add_event_generator', event_receiver: 'add_event_receiver',
    },
  },
  {
    primary: 'add_niagara_data_interface', selector: 'interfaceKind',
    summary: 'Add a data interface to a Niagara emitter: static mesh, skeletal mesh, spline, collision query, audio spectrum.',
    topics: ['data interface', 'skeletal mesh data interface', 'spline data interface', 'audio spectrum'],
    members: byTarget('add_', ['add_static_mesh_data_interface', 'add_skeletal_mesh_data_interface', 'add_spline_data_interface', 'add_collision_query_data_interface', 'add_audio_spectrum_data_interface'], '_data_interface'),
  },
  {
    primary: 'edit_niagara_system', selector: 'edit',
    summary: 'Edit a Niagara system: add an emitter, user parameters or parameter bindings, set parameter values or dynamic inputs, emitter properties, event payloads, GPU simulation, connect pins.',
    topics: ['niagara parameter', 'user parameter', 'emitter properties', 'dynamic input', 'gpu simulation', 'add emitter'],
    members: {
      add_emitter: 'add_emitter_to_system', add_user_parameter: 'add_user_parameter', bind_parameter: 'bind_parameter_to_source', set_parameter_value: 'set_parameter_value',
      set_parameter: 'set_niagara_parameter', set_dynamic_input: 'set_niagara_dynamic_input', set_emitter_properties: 'set_emitter_properties',
      configure_event_payload: 'configure_event_payload', enable_gpu_simulation: 'enable_gpu_simulation', connect_pins: 'connect_niagara_pins',
    },
  },
  {
    primary: 'cleanup', selector: 'cleanupTarget',
    summary: 'Clean up spawned effects, or clear debug shapes.',
    members: { effects: 'cleanup', debug_shapes: 'clear_debug_shapes' },
  },
  {
    primary: 'get_niagara_info', selector: 'info',
    summary: 'Read a Niagara asset\'s information, or validate a system.',
    members: { info: 'get_niagara_info', validate: 'validate_niagara_system' },
  },
];
