// Fold specs for manage_effect. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_EFFECT_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'activate', selector: 'control',
    summary: 'Activate (restarting from age 0 unless reset is false), deactivate or reset the Niagara component on a placed actor (actorName), or with assetPath the one playing that system, on actorName or anywhere in the level; the running game\'s copy while Play runs. The reply says active, false while its system is still compiling or loading. A miss names what was not found: the actor, its Niagara component, or a component playing assetPath (SYSTEM_NOT_FOUND).',
    topics: ['activate effect', 'deactivate effect', 'stop effect', 'reset effect', 'niagara component', 'restart particle effect', 'reset niagara system'],
    members: { activate: 'activate', deactivate: 'deactivate', reset: 'reset', effect: 'activate_effect' },
  },
  {
    primary: 'create_effect', selector: 'kind',
    summary: 'Create an effect: a Niagara system or emitter, a ribbon, particle trail, impact or environment effect, volumetric fog, or spawn a preset particle/niagara effect.',
    topics: ['niagara system', 'niagara emitter', 'ribbon', 'particle trail', 'impact effect', 'volumetric fog', 'effect asset', 'make an explosion', 'add fire effect', 'add smoke or steam', 'water fountain', 'muzzle flash', 'a campfire, fireflies or falling leaves'],
    members: {
      niagara_system: 'create_niagara_system', niagara_emitter: 'create_niagara_emitter', niagara_ribbon: 'create_niagara_ribbon', particle_trail: 'create_particle_trail',
      impact: 'create_impact_effect', environment: 'create_environment_effect', volumetric_fog: 'create_volumetric_fog', niagara: 'niagara', particle: 'particle',
    },
  },
  {
    primary: 'add_niagara_module', selector: 'moduleKind',
    summary: 'Add a module to a Niagara emitter: any module script by path, or a typed module (spawn rate/burst/per-unit, initialize particle, velocity, acceleration, force, color, size, collision, kill, camera offset, particle state, sprite/mesh/ribbon/light renderer, simulation stage, event generator/receiver). It goes where its dependencies hold (a force before Solve Forces and Velocity).',
    topics: ['niagara module', 'spawn rate', 'sprite renderer', 'mesh renderer', 'ribbon renderer', 'particle velocity', 'particle color', 'simulation stage', 'particles fall with gravity', 'particles collide'],
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
    topics: ['data interface', 'skeletal mesh data interface', 'spline data interface', 'audio spectrum', 'audio reactive particles'],
    members: byTarget('add_', ['add_static_mesh_data_interface', 'add_skeletal_mesh_data_interface', 'add_spline_data_interface', 'add_collision_query_data_interface', 'add_audio_spectrum_data_interface'], '_data_interface'),
  },
  {
    primary: 'edit_niagara_system', selector: 'edit',
    summary: 'Edit a Niagara system: add an emitter (an asset, or a copy of another system\'s), user parameters or parameter bindings, set parameter values or dynamic inputs, emitter properties, GPU simulation, connect pins.',
    topics: ['niagara parameter', 'user parameter', 'emitter properties', 'dynamic input', 'gpu simulation', 'add emitter', 'copy emitter', 'gpu particles', 'particle lifetime', 'static switch', 'ribbon width'],
    members: {
      add_emitter: 'add_emitter_to_system', add_user_parameter: 'add_user_parameter', bind_parameter: 'bind_parameter_to_source', set_parameter_value: 'set_parameter_value',
      set_parameter: 'set_niagara_parameter', set_dynamic_input: 'set_niagara_dynamic_input', set_emitter_properties: 'set_emitter_properties',
      enable_gpu_simulation: 'enable_gpu_simulation', connect_pins: 'connect_niagara_pins',
    },
  },
  // cleanup deletes level actors, so it is destructive and stands alone; clearing debug shapes is a write.
  {
    primary: 'debug_shape', selector: 'debugOp',
    summary: 'Draw a temporary debug shape in the editor viewport (debugOp draw, the default: sphere, box, circle, line, point, coordinate, cylinder, cone, capsule, arrow or plane at location, for duration seconds, default 5), or clear every debug shape drawn (clear). Nothing is spawned or saved, and nothing shows in Play In Editor or a packaged game.',
    members: { draw: 'debug_shape', clear: 'clear_debug_shapes' },
  },
  {
    primary: 'get_niagara_info', selector: 'info',
    summary: 'Read a Niagara system: each emitter (enabled, CPU or GPU, stack modules, current module input values, an input that reads a parameter or a dynamic input as {linkedTo}, the static switches such as Ribbon Width Mode that gate them, and its renderers with the object path inspect.set_property writes and their material) and its user parameters with their default values; emitterName and moduleNames narrow it to one emitter and the named modules, and actorName reads one placed effect\'s own values and which of them it overrides. An emitter asset gives its name and CPU or GPU target. info=validate lists errors and warnings, each naming the module, the problem and the fixes the editor offers.',
    topics: ['emitter list', 'stack modules', 'user parameters', 'static switches'],
    members: { info: 'get_niagara_info', validate: 'validate_niagara_system' },
  },
];
