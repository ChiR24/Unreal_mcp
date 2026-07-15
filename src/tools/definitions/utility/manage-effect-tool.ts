import { commonSchemas } from '../../catalog/tool-definition-utils.js';
import type { ToolDefinition } from '../shared/tool-definition.js';

export const manageEffectToolDefinition: ToolDefinition = {
    name: 'manage_effect',
    category: 'gameplay',
    description: 'Niagara particle systems, VFX, debug shapes, and GPU simulations. Create systems, emitters, modules, and control particle effects.',
    inputSchema: {
      type: 'object',
      properties: {
        action: {
          type: 'string',
          enum: [
            'particle', 'niagara', 'debug_shape', 'spawn_niagara', 'create_dynamic_light',
            'create_niagara_system', 'create_niagara_emitter', 'create_volumetric_fog',
            'create_particle_trail', 'create_environment_effect', 'create_impact_effect',
            'create_niagara_ribbon', 'activate', 'activate_effect', 'deactivate', 'reset',
            'advance_simulation', 'add_niagara_module', 'connect_niagara_pins',
            'remove_niagara_node', 'set_niagara_parameter', 'clear_debug_shapes', 'cleanup',
            'list_debug_shapes', 'add_emitter_to_system', 'set_emitter_properties',
            'add_spawn_rate_module', 'add_spawn_burst_module', 'add_spawn_per_unit_module',
            'add_initialize_particle_module', 'add_particle_state_module', 'add_force_module',
            'add_velocity_module', 'add_acceleration_module', 'add_size_module', 'add_color_module',
            'add_sprite_renderer_module', 'add_mesh_renderer_module', 'add_ribbon_renderer_module',
            'add_light_renderer_module', 'add_collision_module', 'add_kill_particles_module',
            'add_camera_offset_module', 'add_user_parameter', 'set_parameter_value',
            'bind_parameter_to_source', 'set_niagara_dynamic_input', 'add_skeletal_mesh_data_interface',
            'add_static_mesh_data_interface', 'add_spline_data_interface', 'add_audio_spectrum_data_interface',
            'add_collision_query_data_interface', 'add_event_generator', 'add_event_receiver',
            'configure_event_payload', 'enable_gpu_simulation', 'add_simulation_stage',
            'get_niagara_info', 'validate_niagara_system'
          ],
          description: 'Effect/Niagara action to perform.'
        },
        // Common parameters
        name: commonSchemas.name,
        path: commonSchemas.directoryPathForCreation,
        savePath: commonSchemas.savePath,
        assetPath: commonSchemas.assetPath,
        timeoutMs: commonSchemas.numberProp,
        save: commonSchemas.booleanProp,
        // System/Emitter parameters
        system: commonSchemas.assetPath,
        systemPath: commonSchemas.assetPath,
        systemName: commonSchemas.stringProp,
        emitter: commonSchemas.stringProp,
        emitterName: commonSchemas.stringProp,
        emitterPath: commonSchemas.assetPath,
        emitterProperties: commonSchemas.objectProp,
        // Placement and lifecycle
        location: commonSchemas.location,
        actorName: commonSchemas.actorName,
        attachToActor: commonSchemas.actorName,
        reset: commonSchemas.booleanProp,
        filter: commonSchemas.stringProp,
        deltaTime: commonSchemas.numberProp,
        steps: commonSchemas.integerProp,
        // Debug/particle/fog/light parameters
        preset: commonSchemas.stringProp,
        shape: commonSchemas.stringProp,
        shapeType: commonSchemas.stringProp,
        radius: commonSchemas.numberProp,
        color: {
          oneOf: [
            { type: 'array', items: commonSchemas.numberProp },
            {
              type: 'object',
              properties: {
                r: commonSchemas.numberProp,
                g: commonSchemas.numberProp,
                b: commonSchemas.numberProp,
                a: commonSchemas.numberProp
              }
            }
          ],
          description: 'RGBA color as an array [r,g,b,a] or object {r,g,b,a} depending on action.'
        },
        duration: commonSchemas.numberProp,
        lightType: commonSchemas.stringProp,
        intensity: commonSchemas.numberProp,
        density: commonSchemas.numberProp,
        scattering: commonSchemas.numberProp,
        extinction: commonSchemas.numberProp,
        // Niagara graph operations
        modulePath: commonSchemas.assetPath,
        scriptType: commonSchemas.stringProp,
        autoConnect: commonSchemas.booleanProp,
        nodeId: commonSchemas.nodeId,
        targetNodeId: commonSchemas.nodeId,
        inputName: commonSchemas.stringProp,
        dynamicInputScriptPath: commonSchemas.assetPath,
        replaceExisting: commonSchemas.booleanProp,
        // Niagara parameters
        parameterName: commonSchemas.parameterName,
        parameterType: commonSchemas.stringProp,
        parameterValue: commonSchemas.value,
        value: commonSchemas.value,
        sourceBinding: commonSchemas.stringProp,
        // Emitter/module authoring
        spawnRate: commonSchemas.numberProp,
        burstCount: commonSchemas.numberProp,
        burstTime: commonSchemas.numberProp,
        spawnPerUnit: commonSchemas.numberProp,
        lifetime: commonSchemas.numberProp,
        mass: commonSchemas.numberProp,
        forceType: commonSchemas.stringProp,
        forceStrength: commonSchemas.numberProp,
        forceVector: commonSchemas.location,
        velocity: commonSchemas.location,
        velocityMode: commonSchemas.stringProp,
        acceleration: commonSchemas.location,
        sizeMode: commonSchemas.stringProp,
        uniformSize: commonSchemas.numberProp,
        colorMode: commonSchemas.stringProp,
        materialPath: commonSchemas.materialPath,
        alignment: commonSchemas.stringProp,
        facingMode: commonSchemas.stringProp,
        meshPath: commonSchemas.meshPath,
        lightRadius: commonSchemas.numberProp,
        collisionMode: commonSchemas.stringProp,
        restitution: commonSchemas.numberProp,
        friction: commonSchemas.numberProp,
        dieOnCollision: commonSchemas.booleanProp,
        killCondition: commonSchemas.stringProp,
        cameraOffset: commonSchemas.numberProp,
        // Events, GPU, and simulation stages
        eventName: commonSchemas.eventName,
        eventType: commonSchemas.stringProp,
        spawnOnEvent: commonSchemas.booleanProp,
        eventSpawnCount: commonSchemas.numberProp,
        eventPayload: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              name: commonSchemas.name,
              type: commonSchemas.stringProp
            }
          },
          description: 'Niagara event payload attributes to expose.'
        },
        fixedBoundsEnabled: commonSchemas.booleanProp,
        deterministicEnabled: commonSchemas.booleanProp,
        stageName: commonSchemas.stringProp,
        stageIterationSource: commonSchemas.stringProp
      },
      required: ['action']
    },
    outputSchema: {
      type: 'object',
      properties: {
        ...commonSchemas.outputBase,
        systemPath: commonSchemas.assetPath,
        emitterName: commonSchemas.stringProp,
        dynamicInputNodeId: commonSchemas.nodeId,
        shapes: commonSchemas.arrayOfObjects,
        niagaraInfo: commonSchemas.objectProp,
        validationResult: commonSchemas.objectProp
      }
    }
};
