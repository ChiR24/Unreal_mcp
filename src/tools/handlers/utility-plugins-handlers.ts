/**
 * Phase 43: Utility Plugins Handlers
 * Handles Python Scripting, Editor Scripting, Modeling Tools, Common UI, Paper2D, Procedural Mesh, Variant Manager.
 * ~100 actions across 7 utility plugin subsystems.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for Utility Plugins tools
 */
export async function handleUtilityPluginsTools(
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
    // PYTHON SCRIPTING (15 actions)
    // =========================================
    case 'execute_python_script':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_python_script'
      )) as HandlerResult;

    case 'execute_python_file':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_python_file'
      )) as HandlerResult;

    case 'execute_python_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_python_command'
      )) as HandlerResult;

    case 'configure_python_paths':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_python_paths'
      )) as HandlerResult;

    case 'add_python_path':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_python_path'
      )) as HandlerResult;

    case 'remove_python_path':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_python_path'
      )) as HandlerResult;

    case 'get_python_paths':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_python_paths'
      )) as HandlerResult;

    case 'create_python_editor_utility':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_python_editor_utility'
      )) as HandlerResult;

    case 'run_startup_scripts':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for run_startup_scripts'
      )) as HandlerResult;

    case 'get_python_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_python_output'
      )) as HandlerResult;

    case 'clear_python_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for clear_python_output'
      )) as HandlerResult;

    case 'is_python_available':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for is_python_available'
      )) as HandlerResult;

    case 'get_python_version':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_python_version'
      )) as HandlerResult;

    case 'reload_python_module':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for reload_python_module'
      )) as HandlerResult;

    case 'get_python_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_python_info'
      )) as HandlerResult;

    // =========================================
    // EDITOR SCRIPTING (12 actions)
    // =========================================
    case 'create_editor_utility_widget':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_editor_utility_widget'
      )) as HandlerResult;

    case 'create_editor_utility_blueprint':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_editor_utility_blueprint'
      )) as HandlerResult;

    case 'add_menu_entry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_menu_entry'
      )) as HandlerResult;

    case 'remove_menu_entry':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_menu_entry'
      )) as HandlerResult;

    case 'add_toolbar_button':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_toolbar_button'
      )) as HandlerResult;

    case 'remove_toolbar_button':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_toolbar_button'
      )) as HandlerResult;

    case 'register_editor_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for register_editor_command'
      )) as HandlerResult;

    case 'unregister_editor_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for unregister_editor_command'
      )) as HandlerResult;

    case 'execute_editor_command':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_editor_command'
      )) as HandlerResult;

    case 'create_blutility_action':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_blutility_action'
      )) as HandlerResult;

    case 'run_editor_utility':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for run_editor_utility'
      )) as HandlerResult;

    case 'get_editor_scripting_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_editor_scripting_info'
      )) as HandlerResult;

    // =========================================
    // MODELING TOOLS (18 actions)
    // =========================================
    case 'activate_modeling_tool':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for activate_modeling_tool'
      )) as HandlerResult;

    case 'deactivate_modeling_tool':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for deactivate_modeling_tool'
      )) as HandlerResult;

    case 'get_active_tool':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_active_tool'
      )) as HandlerResult;

    case 'select_mesh_elements':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for select_mesh_elements'
      )) as HandlerResult;

    case 'clear_mesh_selection':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for clear_mesh_selection'
      )) as HandlerResult;

    case 'get_mesh_selection':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_mesh_selection'
      )) as HandlerResult;

    case 'set_sculpt_brush':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_sculpt_brush'
      )) as HandlerResult;

    case 'configure_sculpt_brush':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_sculpt_brush'
      )) as HandlerResult;

    case 'execute_sculpt_stroke':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for execute_sculpt_stroke'
      )) as HandlerResult;

    case 'apply_mesh_operation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for apply_mesh_operation'
      )) as HandlerResult;

    case 'undo_mesh_operation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for undo_mesh_operation'
      )) as HandlerResult;

    case 'accept_tool_result':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for accept_tool_result'
      )) as HandlerResult;

    case 'cancel_tool':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for cancel_tool'
      )) as HandlerResult;

    case 'set_tool_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_tool_property'
      )) as HandlerResult;

    case 'get_tool_properties':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_tool_properties'
      )) as HandlerResult;

    case 'list_available_tools':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for list_available_tools'
      )) as HandlerResult;

    case 'enter_modeling_mode':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for enter_modeling_mode'
      )) as HandlerResult;

    case 'get_modeling_tools_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_modeling_tools_info'
      )) as HandlerResult;

    // =========================================
    // COMMON UI (10 actions)
    // =========================================
    case 'configure_ui_input_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_ui_input_config'
      )) as HandlerResult;

    case 'create_common_activatable_widget':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_common_activatable_widget'
      )) as HandlerResult;

    case 'configure_navigation_rules':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_navigation_rules'
      )) as HandlerResult;

    case 'set_input_action_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_input_action_data'
      )) as HandlerResult;

    case 'get_ui_input_config':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_ui_input_config'
      )) as HandlerResult;

    case 'register_common_input_metadata':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for register_common_input_metadata'
      )) as HandlerResult;

    case 'configure_gamepad_navigation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_gamepad_navigation'
      )) as HandlerResult;

    case 'set_default_focus_widget':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_default_focus_widget'
      )) as HandlerResult;

    case 'configure_analog_cursor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_analog_cursor'
      )) as HandlerResult;

    case 'get_common_ui_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_common_ui_info'
      )) as HandlerResult;

    // =========================================
    // PAPER2D (12 actions)
    // =========================================
    case 'create_sprite':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_sprite'
      )) as HandlerResult;

    case 'create_flipbook':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_flipbook'
      )) as HandlerResult;

    case 'add_flipbook_keyframe':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_flipbook_keyframe'
      )) as HandlerResult;

    case 'create_tile_map':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_tile_map'
      )) as HandlerResult;

    case 'create_tile_set':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_tile_set'
      )) as HandlerResult;

    case 'set_tile_map_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_tile_map_layer'
      )) as HandlerResult;

    case 'spawn_paper_sprite_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for spawn_paper_sprite_actor'
      )) as HandlerResult;

    case 'spawn_paper_flipbook_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for spawn_paper_flipbook_actor'
      )) as HandlerResult;

    case 'configure_sprite_collision':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_sprite_collision'
      )) as HandlerResult;

    case 'configure_sprite_material':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_sprite_material'
      )) as HandlerResult;

    case 'get_sprite_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_sprite_info'
      )) as HandlerResult;

    case 'get_paper2d_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_paper2d_info'
      )) as HandlerResult;

    // =========================================
    // PROCEDURAL MESH (15 actions)
    // =========================================
    case 'create_procedural_mesh_component':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_procedural_mesh_component'
      )) as HandlerResult;

    case 'create_mesh_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_mesh_section'
      )) as HandlerResult;

    case 'update_mesh_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for update_mesh_section'
      )) as HandlerResult;

    case 'clear_mesh_section':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for clear_mesh_section'
      )) as HandlerResult;

    case 'clear_all_mesh_sections':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for clear_all_mesh_sections'
      )) as HandlerResult;

    case 'set_mesh_section_visible':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_section_visible'
      )) as HandlerResult;

    case 'set_mesh_collision':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_collision'
      )) as HandlerResult;

    case 'set_mesh_vertices':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_vertices'
      )) as HandlerResult;

    case 'set_mesh_triangles':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_triangles'
      )) as HandlerResult;

    case 'set_mesh_normals':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_normals'
      )) as HandlerResult;

    case 'set_mesh_uvs':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_uvs'
      )) as HandlerResult;

    case 'set_mesh_colors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_colors'
      )) as HandlerResult;

    case 'set_mesh_tangents':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for set_mesh_tangents'
      )) as HandlerResult;

    case 'convert_procedural_to_static_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for convert_procedural_to_static_mesh'
      )) as HandlerResult;

    case 'get_procedural_mesh_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_procedural_mesh_info'
      )) as HandlerResult;

    // =========================================
    // VARIANT MANAGER (15 actions)
    // =========================================
    case 'create_level_variant_sets':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_level_variant_sets'
      )) as HandlerResult;

    case 'create_variant_set':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for create_variant_set'
      )) as HandlerResult;

    case 'delete_variant_set':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for delete_variant_set'
      )) as HandlerResult;

    case 'add_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_variant'
      )) as HandlerResult;

    case 'remove_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_variant'
      )) as HandlerResult;

    case 'duplicate_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for duplicate_variant'
      )) as HandlerResult;

    case 'activate_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for activate_variant'
      )) as HandlerResult;

    case 'deactivate_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for deactivate_variant'
      )) as HandlerResult;

    case 'get_active_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_active_variant'
      )) as HandlerResult;

    case 'add_actor_binding':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for add_actor_binding'
      )) as HandlerResult;

    case 'remove_actor_binding':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for remove_actor_binding'
      )) as HandlerResult;

    case 'capture_property':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for capture_property'
      )) as HandlerResult;

    case 'configure_variant_dependency':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for configure_variant_dependency'
      )) as HandlerResult;

    case 'export_variant_configuration':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for export_variant_configuration'
      )) as HandlerResult;

    case 'get_variant_manager_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_variant_manager_info'
      )) as HandlerResult;

    // =========================================
    // UTILITIES (3 actions)
    // =========================================
    case 'get_utility_plugins_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_utility_plugins_info'
      )) as HandlerResult;

    case 'list_utility_plugins':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for list_utility_plugins'
      )) as HandlerResult;

    case 'get_plugin_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_utility_plugins',
        payload,
        'Automation bridge not available for get_plugin_status'
      )) as HandlerResult;

    default:
      return {
        success: false,
        error: `Unknown manage_utility_plugins action: ${action}`,
        hint: 'Check available actions in the tool schema'
      };
  }
}
