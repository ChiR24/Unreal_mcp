/**
 * Phase 37: Asset & Content Plugins Handlers
 * Handles Interchange, USD, Alembic, glTF, Datasmith, SpeedTree, Quixel/Fab, Houdini Engine, Substance.
 * ~150 actions across 9 plugin categories.
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for asset & content plugins tools
 */
export async function handleAssetPluginsTools(
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
    // INTERCHANGE FRAMEWORK (18 actions)
    // =========================================
    case 'create_interchange_pipeline':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_interchange_pipeline'
      ));

    case 'configure_interchange_pipeline':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_interchange_pipeline'
      ));

    case 'import_with_interchange':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_with_interchange'
      ));

    case 'import_fbx_with_interchange':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_fbx_with_interchange'
      ));

    case 'import_obj_with_interchange':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_obj_with_interchange'
      ));

    case 'export_with_interchange':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_with_interchange'
      ));

    case 'set_interchange_translator':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_interchange_translator'
      ));

    case 'get_interchange_translators':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_interchange_translators'
      ));

    case 'configure_import_asset_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_import_asset_type'
      ));

    case 'set_interchange_result_container':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_interchange_result_container'
      ));

    case 'get_interchange_import_result':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_interchange_import_result'
      ));

    case 'cancel_interchange_import':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for cancel_interchange_import'
      ));

    case 'create_interchange_source_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_interchange_source_data'
      ));

    case 'set_interchange_pipeline_stack':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_interchange_pipeline_stack'
      ));

    case 'configure_static_mesh_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_static_mesh_settings'
      ));

    case 'configure_skeletal_mesh_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_skeletal_mesh_settings'
      ));

    case 'configure_animation_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_animation_settings'
      ));

    case 'configure_material_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_material_settings'
      ));

    // =========================================
    // USD (Universal Scene Description) (24 actions)
    // =========================================
    case 'create_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_usd_stage'
      ));

    case 'open_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for open_usd_stage'
      ));

    case 'close_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for close_usd_stage'
      ));

    case 'get_usd_stage_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_stage_info'
      ));

    case 'create_usd_prim':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_usd_prim'
      ));

    case 'get_usd_prim':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_prim'
      ));

    case 'set_usd_prim_attribute':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_usd_prim_attribute'
      ));

    case 'get_usd_prim_attribute':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_prim_attribute'
      ));

    case 'add_usd_reference':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for add_usd_reference'
      ));

    case 'add_usd_payload':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for add_usd_payload'
      ));

    case 'set_usd_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_usd_variant'
      ));

    case 'create_usd_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_usd_layer'
      ));

    case 'set_edit_target_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_edit_target_layer'
      ));

    case 'save_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for save_usd_stage'
      ));

    case 'export_actor_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_actor_to_usd'
      ));

    case 'export_level_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_level_to_usd'
      ));

    case 'export_static_mesh_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_static_mesh_to_usd'
      ));

    case 'export_skeletal_mesh_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_skeletal_mesh_to_usd'
      ));

    case 'export_material_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_material_to_usd'
      ));

    case 'export_animation_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_animation_to_usd'
      ));

    case 'enable_usd_live_edit':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for enable_usd_live_edit'
      ));

    case 'spawn_usd_stage_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for spawn_usd_stage_actor'
      ));

    case 'configure_usd_asset_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_usd_asset_cache'
      ));

    case 'get_usd_prim_children':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_prim_children'
      ));

    // =========================================
    // ALEMBIC (15 actions)
    // =========================================
    case 'import_alembic_file':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_file'
      ));

    case 'import_alembic_static_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_static_mesh'
      ));

    case 'import_alembic_skeletal_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_skeletal_mesh'
      ));

    case 'import_alembic_geometry_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_geometry_cache'
      ));

    case 'import_alembic_groom':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_groom'
      ));

    case 'configure_alembic_import_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_alembic_import_settings'
      ));

    case 'set_alembic_sampling_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_alembic_sampling_settings'
      ));

    case 'set_alembic_compression_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_alembic_compression_type'
      ));

    case 'set_alembic_normal_generation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_alembic_normal_generation'
      ));

    case 'reimport_alembic_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for reimport_alembic_asset'
      ));

    case 'get_alembic_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_alembic_info'
      ));

    case 'create_geometry_cache_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_geometry_cache_track'
      ));

    case 'play_geometry_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for play_geometry_cache'
      ));

    case 'set_geometry_cache_time':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_geometry_cache_time'
      ));

    case 'export_to_alembic':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_to_alembic'
      ));

    // =========================================
    // glTF (16 actions)
    // =========================================
    case 'import_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_gltf'
      ));

    case 'import_glb':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_glb'
      ));

    case 'import_gltf_static_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_gltf_static_mesh'
      ));

    case 'import_gltf_skeletal_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_gltf_skeletal_mesh'
      ));

    case 'export_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_to_gltf'
      ));

    case 'export_to_glb':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_to_glb'
      ));

    case 'export_level_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_level_to_gltf'
      ));

    case 'export_actor_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_actor_to_gltf'
      ));

    case 'configure_gltf_export_options':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_gltf_export_options'
      ));

    case 'set_gltf_export_scale':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_gltf_export_scale'
      ));

    case 'set_gltf_texture_format':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_gltf_texture_format'
      ));

    case 'set_draco_compression':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_draco_compression'
      ));

    case 'export_material_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_material_to_gltf'
      ));

    case 'export_animation_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_animation_to_gltf'
      ));

    case 'get_gltf_export_messages':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_gltf_export_messages'
      ));

    case 'configure_gltf_material_baking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_gltf_material_baking'
      ));

    // =========================================
    // DATASMITH (18 actions)
    // =========================================
    case 'import_datasmith_file':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_file'
      ));

    case 'import_datasmith_cad':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_cad'
      ));

    case 'import_datasmith_revit':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_revit'
      ));

    case 'import_datasmith_sketchup':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_sketchup'
      ));

    case 'import_datasmith_3dsmax':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_3dsmax'
      ));

    case 'import_datasmith_rhino':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_rhino'
      ));

    case 'import_datasmith_solidworks':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_solidworks'
      ));

    case 'import_datasmith_archicad':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_archicad'
      ));

    case 'configure_datasmith_import_options':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_datasmith_import_options'
      ));

    case 'set_datasmith_tessellation_quality':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_datasmith_tessellation_quality'
      ));

    case 'reimport_datasmith_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for reimport_datasmith_scene'
      ));

    case 'get_datasmith_scene_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_datasmith_scene_info'
      ));

    case 'update_datasmith_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for update_datasmith_scene'
      ));

    case 'configure_datasmith_lightmap':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_datasmith_lightmap'
      ));

    case 'create_datasmith_runtime_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_datasmith_runtime_actor'
      ));

    case 'configure_datasmith_materials':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_datasmith_materials'
      ));

    case 'export_datasmith_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_datasmith_scene'
      ));

    case 'sync_datasmith_changes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for sync_datasmith_changes'
      ));

    // =========================================
    // SPEEDTREE (12 actions)
    // =========================================
    case 'import_speedtree_model':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_speedtree_model'
      ));

    case 'import_speedtree_9':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_speedtree_9'
      ));

    case 'import_speedtree_atlas':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_speedtree_atlas'
      ));

    case 'configure_speedtree_wind':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_speedtree_wind'
      ));

    case 'set_speedtree_wind_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_speedtree_wind_type'
      ));

    case 'set_speedtree_wind_speed':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_speedtree_wind_speed'
      ));

    case 'configure_speedtree_lod':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_speedtree_lod'
      ));

    case 'set_speedtree_lod_distances':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_speedtree_lod_distances'
      ));

    case 'set_speedtree_lod_transition':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_speedtree_lod_transition'
      ));

    case 'create_speedtree_material':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_speedtree_material'
      ));

    case 'configure_speedtree_collision':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_speedtree_collision'
      ));

    case 'get_speedtree_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_speedtree_info'
      ));

    // =========================================
    // QUIXEL/FAB BRIDGE (12 actions)
    // =========================================
    case 'connect_to_bridge':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for connect_to_bridge'
      ));

    case 'disconnect_bridge':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for disconnect_bridge'
      ));

    case 'get_bridge_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_bridge_status'
      ));

    case 'import_megascan_surface':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_surface'
      ));

    case 'import_megascan_3d_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_3d_asset'
      ));

    case 'import_megascan_3d_plant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_3d_plant'
      ));

    case 'import_megascan_decal':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_decal'
      ));

    case 'import_megascan_atlas':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_atlas'
      ));

    case 'import_megascan_brush':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_brush'
      ));

    case 'search_fab_assets':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for search_fab_assets'
      ));

    case 'download_fab_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for download_fab_asset'
      ));

    case 'configure_megascan_import_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_megascan_import_settings'
      ));

    // =========================================
    // HOUDINI ENGINE (22 actions)
    // =========================================
    case 'import_hda':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_hda'
      ));

    case 'instantiate_hda':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for instantiate_hda'
      ));

    case 'spawn_hda_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for spawn_hda_actor'
      ));

    case 'get_hda_parameters':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_hda_parameters'
      ));

    case 'set_hda_float_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_float_parameter'
      ));

    case 'set_hda_int_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_int_parameter'
      ));

    case 'set_hda_bool_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_bool_parameter'
      ));

    case 'set_hda_string_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_string_parameter'
      ));

    case 'set_hda_color_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_color_parameter'
      ));

    case 'set_hda_vector_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_vector_parameter'
      ));

    case 'set_hda_ramp_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_ramp_parameter'
      ));

    case 'set_hda_multi_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_multi_parameter'
      ));

    case 'cook_hda':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for cook_hda'
      ));

    case 'bake_hda_to_actors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for bake_hda_to_actors'
      ));

    case 'bake_hda_to_blueprint':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for bake_hda_to_blueprint'
      ));

    case 'configure_hda_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_hda_input'
      ));

    case 'set_hda_world_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_world_input'
      ));

    case 'set_hda_geometry_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_geometry_input'
      ));

    case 'set_hda_curve_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_curve_input'
      ));

    case 'get_hda_outputs':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_hda_outputs'
      ));

    case 'get_hda_cook_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_hda_cook_status'
      ));

    case 'connect_to_houdini_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for connect_to_houdini_session'
      ));

    // =========================================
    // SUBSTANCE (20 actions)
    // =========================================
    case 'import_sbsar_file':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_sbsar_file'
      ));

    case 'create_substance_instance':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_substance_instance'
      ));

    case 'get_substance_parameters':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_substance_parameters'
      ));

    case 'set_substance_float_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_float_parameter'
      ));

    case 'set_substance_int_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_int_parameter'
      ));

    case 'set_substance_bool_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_bool_parameter'
      ));

    case 'set_substance_color_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_color_parameter'
      ));

    case 'set_substance_string_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_string_parameter'
      ));

    case 'set_substance_image_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_image_input'
      ));

    case 'render_substance_textures':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for render_substance_textures'
      ));

    case 'get_substance_outputs':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_substance_outputs'
      ));

    case 'create_material_from_substance':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_material_from_substance'
      ));

    case 'apply_substance_to_material':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for apply_substance_to_material'
      ));

    case 'configure_substance_output_size':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_substance_output_size'
      ));

    case 'randomize_substance_seed':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for randomize_substance_seed'
      ));

    case 'export_substance_textures':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_substance_textures'
      ));

    case 'reimport_sbsar':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for reimport_sbsar'
      ));

    case 'get_substance_graph_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_substance_graph_info'
      ));

    case 'set_substance_output_format':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_output_format'
      ));

    case 'batch_render_substances':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for batch_render_substances'
      ));

    // =========================================
    // UTILITY
    // =========================================
    case 'get_asset_plugins_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_asset_plugins_info'
      ));

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown asset plugins action: ${action}. Available categories: Interchange, USD, Alembic, glTF, Datasmith, SpeedTree, Quixel/Fab, Houdini Engine, Substance.`
      });
  }
}
