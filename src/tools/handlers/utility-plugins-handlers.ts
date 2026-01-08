/**
 * Phase 43: Utility Plugins Handlers
 * Handles Python Scripting, Editor Scripting, Modeling Tools, Common UI, Paper2D, Procedural Mesh, Variant Manager.
 * ~100 actions across 7 utility plugin subsystems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for Utility Plugins tools
 */
export async function handleUtilityPluginsTools(
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
    // PYTHON SCRIPTING (15 actions)
    // =========================================
    case 'execute_python_script':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_python_script'
      ));

    case 'execute_python_file':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_python_file'
      ));

    case 'execute_python_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_python_command'
      ));

    case 'configure_python_paths':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_python_paths'
      ));

    case 'add_python_path':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_python_path'
      ));

    case 'remove_python_path':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_python_path'
      ));

    case 'get_python_paths':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_python_paths'
      ));

    case 'create_python_editor_utility':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_python_editor_utility'
      ));

    case 'run_startup_scripts':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for run_startup_scripts'
      ));

    case 'get_python_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_python_output'
      ));

    case 'clear_python_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for clear_python_output'
      ));

    case 'is_python_available':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for is_python_available'
      ));

    case 'get_python_version':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_python_version'
      ));

    case 'reload_python_module':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for reload_python_module'
      ));

    case 'get_python_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_python_info'
      ));

    // =========================================
    // EDITOR SCRIPTING (12 actions)
    // =========================================
    case 'create_editor_utility_widget':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_editor_utility_widget'
      ));

    case 'create_editor_utility_blueprint':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_editor_utility_blueprint'
      ));

    case 'add_menu_entry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_menu_entry'
      ));

    case 'remove_menu_entry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_menu_entry'
      ));

    case 'add_toolbar_button':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_toolbar_button'
      ));

    case 'remove_toolbar_button':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_toolbar_button'
      ));

    case 'register_editor_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for register_editor_command'
      ));

    case 'unregister_editor_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for unregister_editor_command'
      ));

    case 'execute_editor_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_editor_command'
      ));

    case 'create_blutility_action':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_blutility_action'
      ));

    case 'run_editor_utility':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for run_editor_utility'
      ));

    case 'get_editor_scripting_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_editor_scripting_info'
      ));

    // =========================================
    // MODELING TOOLS (18 actions)
    // =========================================
    case 'activate_modeling_tool':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for activate_modeling_tool'
      ));

    case 'deactivate_modeling_tool':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for deactivate_modeling_tool'
      ));

    case 'get_active_tool':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_active_tool'
      ));

    case 'select_mesh_elements':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for select_mesh_elements'
      ));

    case 'clear_mesh_selection':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for clear_mesh_selection'
      ));

    case 'get_mesh_selection':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_mesh_selection'
      ));

    case 'set_sculpt_brush':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_sculpt_brush'
      ));

    case 'configure_sculpt_brush':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_sculpt_brush'
      ));

    case 'execute_sculpt_stroke':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_sculpt_stroke'
      ));

    case 'apply_mesh_operation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for apply_mesh_operation'
      ));

    case 'undo_mesh_operation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for undo_mesh_operation'
      ));

    case 'accept_tool_result':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for accept_tool_result'
      ));

    case 'cancel_tool':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for cancel_tool'
      ));

    case 'set_tool_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_tool_property'
      ));

    case 'get_tool_properties':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_tool_properties'
      ));

    case 'list_available_tools':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for list_available_tools'
      ));

    case 'enter_modeling_mode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for enter_modeling_mode'
      ));

    case 'get_modeling_tools_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_modeling_tools_info'
      ));

    // =========================================
    // COMMON UI (10 actions)
    // =========================================
    case 'configure_ui_input_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_ui_input_config'
      ));

    case 'create_common_activatable_widget':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_common_activatable_widget'
      ));

    case 'configure_navigation_rules':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_navigation_rules'
      ));

    case 'set_input_action_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_input_action_data'
      ));

    case 'get_ui_input_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_ui_input_config'
      ));

    case 'register_common_input_metadata':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for register_common_input_metadata'
      ));

    case 'configure_gamepad_navigation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_gamepad_navigation'
      ));

    case 'set_default_focus_widget':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_default_focus_widget'
      ));

    case 'configure_analog_cursor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_analog_cursor'
      ));

    case 'get_common_ui_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_common_ui_info'
      ));

    // =========================================
    // PAPER2D (12 actions)
    // =========================================
    case 'create_sprite':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_sprite'
      ));

    case 'create_flipbook':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_flipbook'
      ));

    case 'add_flipbook_keyframe':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_flipbook_keyframe'
      ));

    case 'create_tile_map':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_tile_map'
      ));

    case 'create_tile_set':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_tile_set'
      ));

    case 'set_tile_map_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_tile_map_layer'
      ));

    case 'spawn_paper_sprite_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for spawn_paper_sprite_actor'
      ));

    case 'spawn_paper_flipbook_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for spawn_paper_flipbook_actor'
      ));

    case 'configure_sprite_collision':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_sprite_collision'
      ));

    case 'configure_sprite_material':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_sprite_material'
      ));

    case 'get_sprite_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_sprite_info'
      ));

    case 'get_paper2d_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_paper2d_info'
      ));

    // =========================================
    // PROCEDURAL MESH (15 actions)
    // =========================================
    case 'create_procedural_mesh_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_procedural_mesh_component'
      ));

    case 'create_mesh_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_mesh_section'
      ));

    case 'update_mesh_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for update_mesh_section'
      ));

    case 'clear_mesh_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for clear_mesh_section'
      ));

    case 'clear_all_mesh_sections':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for clear_all_mesh_sections'
      ));

    case 'set_mesh_section_visible':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_section_visible'
      ));

    case 'set_mesh_collision':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_collision'
      ));

    case 'set_mesh_vertices':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_vertices'
      ));

    case 'set_mesh_triangles':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_triangles'
      ));

    case 'set_mesh_normals':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_normals'
      ));

    case 'set_mesh_uvs':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_uvs'
      ));

    case 'set_mesh_colors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_colors'
      ));

    case 'set_mesh_tangents':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_tangents'
      ));

    case 'convert_procedural_to_static_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for convert_procedural_to_static_mesh'
      ));

    case 'get_procedural_mesh_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_procedural_mesh_info'
      ));

    // =========================================
    // VARIANT MANAGER (15 actions)
    // =========================================
    case 'create_level_variant_sets':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_level_variant_sets'
      ));

    case 'create_variant_set':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_variant_set'
      ));

    case 'delete_variant_set':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for delete_variant_set'
      ));

    case 'add_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_variant'
      ));

    case 'remove_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_variant'
      ));

    case 'duplicate_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for duplicate_variant'
      ));

    case 'activate_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for activate_variant'
      ));

    case 'deactivate_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for deactivate_variant'
      ));

    case 'get_active_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_active_variant'
      ));

    case 'add_actor_binding':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_actor_binding'
      ));

    case 'remove_actor_binding':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_actor_binding'
      ));

    case 'capture_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for capture_property'
      ));

    case 'configure_variant_dependency':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_variant_dependency'
      ));

    case 'export_variant_configuration':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for export_variant_configuration'
      ));

    case 'get_variant_manager_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_variant_manager_info'
      ));

    // =========================================
    // UTILITIES (3 actions)
    // =========================================
    case 'get_utility_plugins_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_utility_plugins_info'
      ));

    case 'list_utility_plugins':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for list_utility_plugins'
      ));

    case 'get_plugin_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_plugin_status'
      ));

    default:
      return {
        success: false,
        error: `Unknown manage_utility_plugins action: ${action}`,
        hint: 'Check available actions in the tool schema'
      };
  }
}
