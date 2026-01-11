/**
 * Phase 44: Physics & Destruction Plugins Handlers
 * Handles Chaos Destruction, Chaos Vehicles, Chaos Cloth, and Chaos Flesh.
 * ~80 actions across 4 physics plugin subsystems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for Physics & Destruction Plugins tools
 */
export async function handlePhysicsDestructionTools(
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
    // CHAOS DESTRUCTION (29 actions)
    // =========================================
    case 'create_geometry_collection':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_geometry_collection'
      )) as HandlerResult;

    case 'fracture_uniform':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for fracture_uniform'
      )) as HandlerResult;

    case 'fracture_clustered':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for fracture_clustered'
      )) as HandlerResult;

    case 'fracture_radial':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for fracture_radial'
      )) as HandlerResult;

    case 'fracture_slice':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for fracture_slice'
      )) as HandlerResult;

    case 'fracture_brick':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for fracture_brick'
      )) as HandlerResult;

    case 'flatten_fracture':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for flatten_fracture'
      )) as HandlerResult;

    case 'set_geometry_collection_materials':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_geometry_collection_materials'
      )) as HandlerResult;

    case 'set_damage_thresholds':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_damage_thresholds'
      )) as HandlerResult;

    case 'set_cluster_connection_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cluster_connection_type'
      )) as HandlerResult;

    case 'set_collision_particles_fraction':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_collision_particles_fraction'
      )) as HandlerResult;

    case 'set_remove_on_break':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_remove_on_break'
      )) as HandlerResult;

    case 'create_field_system_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_field_system_actor'
      )) as HandlerResult;

    case 'add_transient_field':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_transient_field'
      )) as HandlerResult;

    case 'add_persistent_field':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_persistent_field'
      )) as HandlerResult;

    case 'add_construction_field':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_construction_field'
      )) as HandlerResult;

    case 'add_field_radial_falloff':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_field_radial_falloff'
      )) as HandlerResult;

    case 'add_field_radial_vector':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_field_radial_vector'
      )) as HandlerResult;

    case 'add_field_uniform_vector':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_field_uniform_vector'
      )) as HandlerResult;

    case 'add_field_noise':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_field_noise'
      )) as HandlerResult;

    case 'add_field_strain':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_field_strain'
      )) as HandlerResult;

    case 'create_anchor_field':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_anchor_field'
      )) as HandlerResult;

    case 'set_dynamic_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_dynamic_state'
      )) as HandlerResult;

    case 'enable_clustering':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for enable_clustering'
      )) as HandlerResult;

    case 'get_geometry_collection_stats':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for get_geometry_collection_stats'
      )) as HandlerResult;

    case 'create_geometry_collection_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_geometry_collection_cache'
      )) as HandlerResult;

    case 'record_geometry_collection_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for record_geometry_collection_cache'
      )) as HandlerResult;

    case 'apply_cache_to_collection':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for apply_cache_to_collection'
      )) as HandlerResult;

    case 'remove_geometry_collection_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for remove_geometry_collection_cache'
      )) as HandlerResult;

    // =========================================
    // CHAOS VEHICLES (19 actions)
    // =========================================
    case 'create_wheeled_vehicle_bp':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_wheeled_vehicle_bp'
      )) as HandlerResult;

    case 'add_vehicle_wheel':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for add_vehicle_wheel'
      )) as HandlerResult;

    case 'remove_wheel_from_vehicle':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for remove_wheel_from_vehicle'
      )) as HandlerResult;

    case 'configure_engine_setup':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for configure_engine_setup'
      )) as HandlerResult;

    case 'configure_transmission_setup':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for configure_transmission_setup'
      )) as HandlerResult;

    case 'configure_steering_setup':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for configure_steering_setup'
      )) as HandlerResult;

    case 'configure_differential_setup':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for configure_differential_setup'
      )) as HandlerResult;

    case 'configure_suspension_setup':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for configure_suspension_setup'
      )) as HandlerResult;

    case 'configure_brake_setup':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for configure_brake_setup'
      )) as HandlerResult;

    case 'set_vehicle_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_vehicle_mesh'
      )) as HandlerResult;

    case 'set_wheel_class':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_wheel_class'
      )) as HandlerResult;

    case 'set_wheel_offset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_wheel_offset'
      )) as HandlerResult;

    case 'set_wheel_radius':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_wheel_radius'
      )) as HandlerResult;

    case 'set_vehicle_mass':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_vehicle_mass'
      )) as HandlerResult;

    case 'set_drag_coefficient':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_drag_coefficient'
      )) as HandlerResult;

    case 'set_center_of_mass':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_center_of_mass'
      )) as HandlerResult;

    case 'create_vehicle_animation_instance':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_vehicle_animation_instance'
      )) as HandlerResult;

    case 'set_vehicle_animation_bp':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_vehicle_animation_bp'
      )) as HandlerResult;

    case 'get_vehicle_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for get_vehicle_config'
      )) as HandlerResult;

    // =========================================
    // CHAOS CLOTH (15 actions)
    // =========================================
    case 'create_chaos_cloth_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_chaos_cloth_config'
      )) as HandlerResult;

    case 'create_chaos_cloth_shared_sim_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_chaos_cloth_shared_sim_config'
      )) as HandlerResult;

    case 'apply_cloth_to_skeletal_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for apply_cloth_to_skeletal_mesh'
      )) as HandlerResult;

    case 'remove_cloth_from_skeletal_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for remove_cloth_from_skeletal_mesh'
      )) as HandlerResult;

    case 'set_cloth_mass_properties':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_mass_properties'
      )) as HandlerResult;

    case 'set_cloth_gravity':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_gravity'
      )) as HandlerResult;

    case 'set_cloth_damping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_damping'
      )) as HandlerResult;

    case 'set_cloth_collision_properties':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_collision_properties'
      )) as HandlerResult;

    case 'set_cloth_stiffness':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_stiffness'
      )) as HandlerResult;

    case 'set_cloth_tether_stiffness':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_tether_stiffness'
      )) as HandlerResult;

    case 'set_cloth_aerodynamics':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_aerodynamics'
      )) as HandlerResult;

    case 'set_cloth_anim_drive':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_anim_drive'
      )) as HandlerResult;

    case 'set_cloth_long_range_attachment':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_cloth_long_range_attachment'
      )) as HandlerResult;

    case 'get_cloth_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for get_cloth_config'
      )) as HandlerResult;

    case 'get_cloth_stats':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for get_cloth_stats'
      )) as HandlerResult;

    // =========================================
    // CHAOS FLESH (13 actions)
    // =========================================
    case 'create_flesh_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_flesh_asset'
      )) as HandlerResult;

    case 'create_flesh_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_flesh_component'
      )) as HandlerResult;

    case 'set_flesh_simulation_properties':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_flesh_simulation_properties'
      )) as HandlerResult;

    case 'set_flesh_stiffness':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_flesh_stiffness'
      )) as HandlerResult;

    case 'set_flesh_damping':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_flesh_damping'
      )) as HandlerResult;

    case 'set_flesh_incompressibility':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_flesh_incompressibility'
      )) as HandlerResult;

    case 'set_flesh_inflation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_flesh_inflation'
      )) as HandlerResult;

    case 'set_flesh_solver_iterations':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_flesh_solver_iterations'
      )) as HandlerResult;

    case 'bind_flesh_to_skeleton':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for bind_flesh_to_skeleton'
      )) as HandlerResult;

    case 'set_flesh_rest_state':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for set_flesh_rest_state'
      )) as HandlerResult;

    case 'create_flesh_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for create_flesh_cache'
      )) as HandlerResult;

    case 'record_flesh_simulation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for record_flesh_simulation'
      )) as HandlerResult;

    case 'get_flesh_asset_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for get_flesh_asset_info'
      )) as HandlerResult;

    // =========================================
    // UTILITY (4 actions)
    // =========================================
    case 'get_physics_destruction_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for get_physics_destruction_info'
      )) as HandlerResult;

    case 'list_geometry_collections':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for list_geometry_collections'
      )) as HandlerResult;

    case 'list_chaos_vehicles':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for list_chaos_vehicles'
      )) as HandlerResult;

    case 'get_chaos_plugin_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_physics_destruction',
        payload,
        'Automation bridge not available for get_chaos_plugin_status'
      )) as HandlerResult;

    default:
      return {
        success: false,
        error: `Unknown physics/destruction action: ${action}`
      };
  }
}
