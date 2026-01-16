/**
 * Phase 37: Asset & Content Plugins Handlers
 * Handles Interchange, USD, Alembic, glTF, Datasmith, SpeedTree, Quixel/Fab, Houdini Engine, Substance.
 * ~150 actions across 9 plugin categories.
 */

import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

/**
 * Main handler for asset & content plugins tools
 */
export async function handleAssetPluginsTools(
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
    // INTERCHANGE FRAMEWORK (18 actions)
    // =========================================
    case 'create_interchange_pipeline':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_interchange_pipeline'
      )) as HandlerResult;

    case 'configure_interchange_pipeline':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_interchange_pipeline'
      )) as HandlerResult;

    case 'import_with_interchange':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_with_interchange'
      )) as HandlerResult;

    case 'import_fbx_with_interchange':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_fbx_with_interchange'
      )) as HandlerResult;

    case 'import_obj_with_interchange':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_obj_with_interchange'
      )) as HandlerResult;

    case 'export_with_interchange':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_with_interchange'
      )) as HandlerResult;

    case 'set_interchange_translator':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_interchange_translator'
      )) as HandlerResult;

    case 'get_interchange_translators':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_interchange_translators'
      )) as HandlerResult;

    case 'configure_import_asset_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_import_asset_type'
      )) as HandlerResult;

    case 'set_interchange_result_container':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_interchange_result_container'
      )) as HandlerResult;

    case 'get_interchange_import_result':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_interchange_import_result'
      )) as HandlerResult;

    case 'cancel_interchange_import':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for cancel_interchange_import'
      )) as HandlerResult;

    case 'create_interchange_source_data':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_interchange_source_data'
      )) as HandlerResult;

    case 'set_interchange_pipeline_stack':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_interchange_pipeline_stack'
      )) as HandlerResult;

    case 'configure_static_mesh_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_static_mesh_settings'
      )) as HandlerResult;

    case 'configure_skeletal_mesh_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_skeletal_mesh_settings'
      )) as HandlerResult;

    case 'configure_animation_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_animation_settings'
      )) as HandlerResult;

    case 'configure_material_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_material_settings'
      )) as HandlerResult;

    // =========================================
    // USD (Universal Scene Description) (24 actions)
    // =========================================
    case 'create_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_usd_stage'
      )) as HandlerResult;

    case 'open_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for open_usd_stage'
      )) as HandlerResult;

    case 'close_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for close_usd_stage'
      )) as HandlerResult;

    case 'get_usd_stage_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_stage_info'
      )) as HandlerResult;

    case 'create_usd_prim':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_usd_prim'
      )) as HandlerResult;

    case 'get_usd_prim':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_prim'
      )) as HandlerResult;

    case 'set_usd_prim_attribute':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_usd_prim_attribute'
      )) as HandlerResult;

    case 'get_usd_prim_attribute':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_prim_attribute'
      )) as HandlerResult;

    case 'add_usd_reference':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for add_usd_reference'
      )) as HandlerResult;

    case 'add_usd_payload':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for add_usd_payload'
      )) as HandlerResult;

    case 'set_usd_variant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_usd_variant'
      )) as HandlerResult;

    case 'create_usd_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_usd_layer'
      )) as HandlerResult;

    case 'set_edit_target_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_edit_target_layer'
      )) as HandlerResult;

    case 'save_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for save_usd_stage'
      )) as HandlerResult;

    case 'export_actor_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_actor_to_usd'
      )) as HandlerResult;

    case 'export_level_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_level_to_usd'
      )) as HandlerResult;

    case 'export_static_mesh_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_static_mesh_to_usd'
      )) as HandlerResult;

    case 'export_skeletal_mesh_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_skeletal_mesh_to_usd'
      )) as HandlerResult;

    case 'export_material_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_material_to_usd'
      )) as HandlerResult;

    case 'export_animation_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_animation_to_usd'
      )) as HandlerResult;

    case 'enable_usd_live_edit':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for enable_usd_live_edit'
      )) as HandlerResult;

    case 'spawn_usd_stage_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for spawn_usd_stage_actor'
      )) as HandlerResult;

    case 'configure_usd_asset_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_usd_asset_cache'
      )) as HandlerResult;

    case 'get_usd_prim_children':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_prim_children'
      )) as HandlerResult;

    case 'import_usd_stage':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_usd_stage'
      )) as HandlerResult;

    case 'export_to_usd':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_to_usd'
      )) as HandlerResult;

    case 'get_usd_prims':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_usd_prims'
      )) as HandlerResult;

    case 'configure_usd_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_usd_settings'
      )) as HandlerResult;

    case 'link_usd_layer':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for link_usd_layer'
      )) as HandlerResult;

    // =========================================
    // ALEMBIC (15 actions)
    // =========================================
    case 'import_alembic_file':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_file'
      )) as HandlerResult;

    case 'import_alembic_static_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_static_mesh'
      )) as HandlerResult;

    case 'import_alembic_skeletal_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_skeletal_mesh'
      )) as HandlerResult;

    case 'import_alembic_geometry_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_geometry_cache'
      )) as HandlerResult;

    case 'import_alembic_groom':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic_groom'
      )) as HandlerResult;

    case 'configure_alembic_import_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_alembic_import_settings'
      )) as HandlerResult;

    case 'set_alembic_sampling_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_alembic_sampling_settings'
      )) as HandlerResult;

    case 'set_alembic_compression_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_alembic_compression_type'
      )) as HandlerResult;

    case 'set_alembic_normal_generation':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_alembic_normal_generation'
      )) as HandlerResult;

    case 'reimport_alembic_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for reimport_alembic_asset'
      )) as HandlerResult;

    case 'get_alembic_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_alembic_info'
      )) as HandlerResult;

    case 'create_geometry_cache_track':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_geometry_cache_track'
      )) as HandlerResult;

    case 'play_geometry_cache':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for play_geometry_cache'
      )) as HandlerResult;

    case 'set_geometry_cache_time':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_geometry_cache_time'
      )) as HandlerResult;

    case 'export_to_alembic':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_to_alembic'
      )) as HandlerResult;

    case 'import_alembic':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_alembic'
      )) as HandlerResult;

    case 'configure_alembic_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_alembic_settings'
      )) as HandlerResult;

    case 'get_alembic_cache_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_alembic_cache_info'
      )) as HandlerResult;

    // =========================================
    // glTF (16 actions)
    // =========================================
    case 'import_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_gltf'
      )) as HandlerResult;

    case 'import_glb':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_glb'
      )) as HandlerResult;

    case 'import_gltf_static_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_gltf_static_mesh'
      )) as HandlerResult;

    case 'import_gltf_skeletal_mesh':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_gltf_skeletal_mesh'
      )) as HandlerResult;

    case 'export_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_to_gltf'
      )) as HandlerResult;

    case 'export_to_glb':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_to_glb'
      )) as HandlerResult;

    case 'export_level_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_level_to_gltf'
      )) as HandlerResult;

    case 'export_actor_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_actor_to_gltf'
      )) as HandlerResult;

    case 'configure_gltf_export_options':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_gltf_export_options'
      )) as HandlerResult;

    case 'set_gltf_export_scale':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_gltf_export_scale'
      )) as HandlerResult;

    case 'set_gltf_texture_format':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_gltf_texture_format'
      )) as HandlerResult;

    case 'set_draco_compression':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_draco_compression'
      )) as HandlerResult;

    case 'export_material_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_material_to_gltf'
      )) as HandlerResult;

    case 'export_animation_to_gltf':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_animation_to_gltf'
      )) as HandlerResult;

    case 'get_gltf_export_messages':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_gltf_export_messages'
      )) as HandlerResult;

    case 'configure_gltf_material_baking':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_gltf_material_baking'
      )) as HandlerResult;

    case 'configure_gltf_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_gltf_settings'
      )) as HandlerResult;

    // =========================================
    // DATASMITH (18 actions)
    // =========================================
    case 'import_datasmith_file':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_file'
      )) as HandlerResult;

    case 'import_datasmith_cad':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_cad'
      )) as HandlerResult;

    case 'import_datasmith_revit':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_revit'
      )) as HandlerResult;

    case 'import_datasmith_sketchup':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_sketchup'
      )) as HandlerResult;

    case 'import_datasmith_3dsmax':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_3dsmax'
      )) as HandlerResult;

    case 'import_datasmith_rhino':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_rhino'
      )) as HandlerResult;

    case 'import_datasmith_solidworks':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_solidworks'
      )) as HandlerResult;

    case 'import_datasmith_archicad':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith_archicad'
      )) as HandlerResult;

    case 'configure_datasmith_import_options':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_datasmith_import_options'
      )) as HandlerResult;

    case 'set_datasmith_tessellation_quality':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_datasmith_tessellation_quality'
      )) as HandlerResult;

    case 'reimport_datasmith_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for reimport_datasmith_scene'
      )) as HandlerResult;

    case 'get_datasmith_scene_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_datasmith_scene_info'
      )) as HandlerResult;

    case 'update_datasmith_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for update_datasmith_scene'
      )) as HandlerResult;

    case 'configure_datasmith_lightmap':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_datasmith_lightmap'
      )) as HandlerResult;

    case 'create_datasmith_runtime_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_datasmith_runtime_actor'
      )) as HandlerResult;

    case 'configure_datasmith_materials':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_datasmith_materials'
      )) as HandlerResult;

    case 'export_datasmith_scene':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_datasmith_scene'
      )) as HandlerResult;

    case 'sync_datasmith_changes':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for sync_datasmith_changes'
      )) as HandlerResult;

    case 'import_datasmith':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_datasmith'
      )) as HandlerResult;

    case 'configure_datasmith_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_datasmith_settings'
      )) as HandlerResult;

    // =========================================
    // SPEEDTREE (12 actions)
    // =========================================
    case 'import_speedtree_model':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_speedtree_model'
      )) as HandlerResult;

    case 'import_speedtree_9':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_speedtree_9'
      )) as HandlerResult;

    case 'import_speedtree_atlas':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_speedtree_atlas'
      )) as HandlerResult;

    case 'configure_speedtree_wind':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_speedtree_wind'
      )) as HandlerResult;

    case 'set_speedtree_wind_type':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_speedtree_wind_type'
      )) as HandlerResult;

    case 'set_speedtree_wind_speed':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_speedtree_wind_speed'
      )) as HandlerResult;

    case 'configure_speedtree_lod':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_speedtree_lod'
      )) as HandlerResult;

    case 'set_speedtree_lod_distances':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_speedtree_lod_distances'
      )) as HandlerResult;

    case 'set_speedtree_lod_transition':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_speedtree_lod_transition'
      )) as HandlerResult;

    case 'create_speedtree_material':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_speedtree_material'
      )) as HandlerResult;

    case 'configure_speedtree_collision':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_speedtree_collision'
      )) as HandlerResult;

    case 'get_speedtree_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_speedtree_info'
      )) as HandlerResult;

    // =========================================
    // QUIXEL/FAB BRIDGE (12 actions)
    // =========================================
    case 'connect_to_bridge':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for connect_to_bridge'
      )) as HandlerResult;

    case 'disconnect_bridge':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for disconnect_bridge'
      )) as HandlerResult;

    case 'get_bridge_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_bridge_status'
      )) as HandlerResult;

    case 'import_megascan_surface':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_surface'
      )) as HandlerResult;

    case 'import_megascan_3d_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_3d_asset'
      )) as HandlerResult;

    case 'import_megascan_3d_plant':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_3d_plant'
      )) as HandlerResult;

    case 'import_megascan_decal':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_decal'
      )) as HandlerResult;

    case 'import_megascan_atlas':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_atlas'
      )) as HandlerResult;

    case 'import_megascan_brush':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_megascan_brush'
      )) as HandlerResult;

    case 'search_fab_assets':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for search_fab_assets'
      )) as HandlerResult;

    case 'download_fab_asset':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for download_fab_asset'
      )) as HandlerResult;

    case 'configure_megascan_import_settings':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_megascan_import_settings'
      )) as HandlerResult;

    // =========================================
    // HOUDINI ENGINE (22 actions)
    // =========================================
    case 'import_hda':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_hda'
      )) as HandlerResult;

    case 'bake_hda_output':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for bake_hda_output'
      )) as HandlerResult;

    case 'instantiate_hda':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for instantiate_hda'
      )) as HandlerResult;

    case 'spawn_hda_actor':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for spawn_hda_actor'
      )) as HandlerResult;

    case 'get_hda_parameters':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_hda_parameters'
      )) as HandlerResult;

    case 'set_hda_float_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_float_parameter'
      )) as HandlerResult;

    case 'set_hda_int_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_int_parameter'
      )) as HandlerResult;

    case 'set_hda_bool_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_bool_parameter'
      )) as HandlerResult;

    case 'set_hda_string_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_string_parameter'
      )) as HandlerResult;

    case 'set_hda_color_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_color_parameter'
      )) as HandlerResult;

    case 'set_hda_vector_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_vector_parameter'
      )) as HandlerResult;

    case 'set_hda_ramp_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_ramp_parameter'
      )) as HandlerResult;

    case 'set_hda_multi_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_multi_parameter'
      )) as HandlerResult;

    case 'cook_hda':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for cook_hda'
      )) as HandlerResult;

    case 'bake_hda_to_actors':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for bake_hda_to_actors'
      )) as HandlerResult;

    case 'bake_hda_to_blueprint':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for bake_hda_to_blueprint'
      )) as HandlerResult;

    case 'configure_hda_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_hda_input'
      )) as HandlerResult;

    case 'set_hda_world_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_world_input'
      )) as HandlerResult;

    case 'set_hda_geometry_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_geometry_input'
      )) as HandlerResult;

    case 'set_hda_curve_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_hda_curve_input'
      )) as HandlerResult;

    case 'get_hda_outputs':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_hda_outputs'
      )) as HandlerResult;

    case 'get_hda_cook_status':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_hda_cook_status'
      )) as HandlerResult;

    case 'connect_to_houdini_session':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for connect_to_houdini_session'
      )) as HandlerResult;

    // =========================================
    // SUBSTANCE (20 actions)
    // =========================================
    case 'import_sbsar_file':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for import_sbsar_file'
      )) as HandlerResult;

    case 'create_substance_instance':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_substance_instance'
      )) as HandlerResult;

    case 'get_substance_parameters':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_substance_parameters'
      )) as HandlerResult;

    case 'set_substance_float_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_float_parameter'
      )) as HandlerResult;

    case 'set_substance_int_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_int_parameter'
      )) as HandlerResult;

    case 'set_substance_bool_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_bool_parameter'
      )) as HandlerResult;

    case 'set_substance_color_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_color_parameter'
      )) as HandlerResult;

    case 'set_substance_string_parameter':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_string_parameter'
      )) as HandlerResult;

    case 'set_substance_image_input':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_image_input'
      )) as HandlerResult;

    case 'render_substance_textures':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for render_substance_textures'
      )) as HandlerResult;

    case 'get_substance_outputs':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_substance_outputs'
      )) as HandlerResult;

    case 'create_material_from_substance':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for create_material_from_substance'
      )) as HandlerResult;

    case 'apply_substance_to_material':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for apply_substance_to_material'
      )) as HandlerResult;

    case 'configure_substance_output_size':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for configure_substance_output_size'
      )) as HandlerResult;

    case 'randomize_substance_seed':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for randomize_substance_seed'
      )) as HandlerResult;

    case 'export_substance_textures':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for export_substance_textures'
      )) as HandlerResult;

    case 'reimport_sbsar':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for reimport_sbsar'
      )) as HandlerResult;

    case 'get_substance_graph_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_substance_graph_info'
      )) as HandlerResult;

    case 'set_substance_output_format':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for set_substance_output_format'
      )) as HandlerResult;

    case 'batch_render_substances':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for batch_render_substances'
      )) as HandlerResult;

    // =========================================
    // UTILITY
    // =========================================
    case 'get_asset_plugins_info':
      return cleanObject(await executeAutomationRequest(
        tools,
        'manage_asset_plugins',
        payload,
        'Automation bridge not available for get_asset_plugins_info'
      )) as HandlerResult;

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown asset plugins action: ${action}. Available categories: Interchange, USD, Alembic, glTF, Datasmith, SpeedTree, Quixel/Fab, Houdini Engine, Substance.`
      });
  }
}
