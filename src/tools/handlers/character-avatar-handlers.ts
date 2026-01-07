/**
 * Character & Avatar Plugin Handlers (Phase 36)
 *
 * Complete avatar and character customization with:
 * - MetaHuman: Import, spawn, face/skin/hair customization, LOD config
 * - Groom/Hair: Asset creation, import, simulation, styling
 * - Mutable (Customizable): Object/Instance creation, parameters, baking
 * - Ready Player Me: URL/GLB loading, avatar application
 *
 * @module character-avatar-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all character avatar actions for the manage_character_avatar tool.
 */
export async function handleCharacterAvatarTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<Record<string, unknown>> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_character_avatar',
      payload as HandlerArgs,
      `Automation bridge not available for character avatar action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // =========================================================================
    // 36.1 MetaHuman Actions (18 actions)
    // =========================================================================

    case 'import_metahuman': {
      requireNonEmptyString(argsRecord.sourcePath, 'sourcePath', 'Missing required parameter: sourcePath');
      return sendRequest('import_metahuman');
    }

    case 'spawn_metahuman_actor': {
      requireNonEmptyString(argsRecord.metahumanPath, 'metahumanPath', 'Missing required parameter: metahumanPath');
      return sendRequest('spawn_metahuman_actor');
    }

    case 'get_metahuman_component': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('get_metahuman_component');
    }

    case 'set_body_type': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_body_type');
    }

    case 'set_face_parameter': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_face_parameter');
    }

    case 'set_skin_tone': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_skin_tone');
    }

    case 'set_hair_style': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_hair_style');
    }

    case 'set_eye_color': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_eye_color');
    }

    case 'configure_metahuman_lod': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('configure_metahuman_lod');
    }

    case 'enable_body_correctives': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('enable_body_correctives');
    }

    case 'enable_neck_correctives': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('enable_neck_correctives');
    }

    case 'set_quality_level': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_quality_level');
    }

    case 'configure_face_rig': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('configure_face_rig');
    }

    case 'set_body_part': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      requireNonEmptyString(argsRecord.bodyPartType, 'bodyPartType', 'Missing required parameter: bodyPartType');
      return sendRequest('set_body_part');
    }

    case 'get_metahuman_info': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('get_metahuman_info');
    }

    case 'list_available_presets': {
      return sendRequest('list_available_presets');
    }

    case 'apply_preset': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      requireNonEmptyString(argsRecord.presetName, 'presetName', 'Missing required parameter: presetName');
      return sendRequest('apply_preset');
    }

    case 'export_metahuman_settings': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('export_metahuman_settings');
    }

    // =========================================================================
    // 36.2 Groom/Hair Actions (14 actions)
    // =========================================================================

    case 'create_groom_asset': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_groom_asset');
    }

    case 'import_groom': {
      requireNonEmptyString(argsRecord.sourcePath, 'sourcePath', 'Missing required parameter: sourcePath');
      return sendRequest('import_groom');
    }

    case 'create_groom_binding': {
      requireNonEmptyString(argsRecord.groomAssetPath, 'groomAssetPath', 'Missing required parameter: groomAssetPath');
      requireNonEmptyString(argsRecord.skeletalMeshPath, 'skeletalMeshPath', 'Missing required parameter: skeletalMeshPath');
      return sendRequest('create_groom_binding');
    }

    case 'spawn_groom_actor': {
      requireNonEmptyString(argsRecord.groomAssetPath, 'groomAssetPath', 'Missing required parameter: groomAssetPath');
      return sendRequest('spawn_groom_actor');
    }

    case 'attach_groom_to_skeletal_mesh': {
      requireNonEmptyString(argsRecord.groomAssetPath, 'groomAssetPath', 'Missing required parameter: groomAssetPath');
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('attach_groom_to_skeletal_mesh');
    }

    case 'configure_hair_simulation': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('configure_hair_simulation');
    }

    case 'set_hair_width': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_hair_width');
    }

    case 'set_hair_root_scale': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_hair_root_scale');
    }

    case 'set_hair_tip_scale': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_hair_tip_scale');
    }

    case 'set_hair_color': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_hair_color');
    }

    case 'configure_hair_physics': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('configure_hair_physics');
    }

    case 'configure_hair_rendering': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('configure_hair_rendering');
    }

    case 'enable_hair_simulation': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('enable_hair_simulation');
    }

    case 'get_groom_info': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('get_groom_info');
    }

    // =========================================================================
    // 36.3 Mutable/Customizable Actions (16 actions)
    // =========================================================================

    case 'create_customizable_object': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_customizable_object');
    }

    case 'compile_customizable_object': {
      requireNonEmptyString(argsRecord.objectPath, 'objectPath', 'Missing required parameter: objectPath');
      return sendRequest('compile_customizable_object');
    }

    case 'create_customizable_instance': {
      requireNonEmptyString(argsRecord.objectPath, 'objectPath', 'Missing required parameter: objectPath');
      return sendRequest('create_customizable_instance');
    }

    case 'set_bool_parameter': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_bool_parameter');
    }

    case 'set_int_parameter': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_int_parameter');
    }

    case 'set_float_parameter': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_float_parameter');
    }

    case 'set_color_parameter': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_color_parameter');
    }

    case 'set_vector_parameter': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_vector_parameter');
    }

    case 'set_texture_parameter': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_texture_parameter');
    }

    case 'set_transform_parameter': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_transform_parameter');
    }

    case 'set_projector_parameter': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      requireNonEmptyString(argsRecord.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return sendRequest('set_projector_parameter');
    }

    case 'update_skeletal_mesh': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      return sendRequest('update_skeletal_mesh');
    }

    case 'bake_customizable_instance': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      return sendRequest('bake_customizable_instance');
    }

    case 'get_parameter_info': {
      requireNonEmptyString(argsRecord.objectPath, 'objectPath', 'Missing required parameter: objectPath');
      return sendRequest('get_parameter_info');
    }

    case 'get_instance_info': {
      requireNonEmptyString(argsRecord.instancePath, 'instancePath', 'Missing required parameter: instancePath');
      return sendRequest('get_instance_info');
    }

    case 'spawn_customizable_actor': {
      requireNonEmptyString(argsRecord.objectPath, 'objectPath', 'Missing required parameter: objectPath');
      return sendRequest('spawn_customizable_actor');
    }

    // =========================================================================
    // 36.4 Ready Player Me Actions (12 actions)
    // =========================================================================

    case 'load_avatar_from_url': {
      requireNonEmptyString(argsRecord.avatarUrl, 'avatarUrl', 'Missing required parameter: avatarUrl');
      return sendRequest('load_avatar_from_url');
    }

    case 'load_avatar_from_glb': {
      requireNonEmptyString(argsRecord.glbPath, 'glbPath', 'Missing required parameter: glbPath');
      return sendRequest('load_avatar_from_glb');
    }

    case 'create_rpm_actor': {
      requireNonEmptyString(argsRecord.avatarUrl, 'avatarUrl', 'Missing required parameter: avatarUrl');
      return sendRequest('create_rpm_actor');
    }

    case 'apply_avatar_to_character': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('apply_avatar_to_character');
    }

    case 'configure_rpm_materials': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('configure_rpm_materials');
    }

    case 'set_rpm_outfit': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      return sendRequest('set_rpm_outfit');
    }

    case 'get_avatar_metadata': {
      requireNonEmptyString(argsRecord.avatarUrl, 'avatarUrl', 'Missing required parameter: avatarUrl');
      return sendRequest('get_avatar_metadata');
    }

    case 'cache_avatar': {
      requireNonEmptyString(argsRecord.avatarUrl, 'avatarUrl', 'Missing required parameter: avatarUrl');
      return sendRequest('cache_avatar');
    }

    case 'clear_avatar_cache': {
      return sendRequest('clear_avatar_cache');
    }

    case 'create_rpm_animation_blueprint': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      return sendRequest('create_rpm_animation_blueprint');
    }

    case 'retarget_rpm_animation': {
      requireNonEmptyString(argsRecord.sourceAnimationPath, 'sourceAnimationPath', 'Missing required parameter: sourceAnimationPath');
      return sendRequest('retarget_rpm_animation');
    }

    case 'get_rpm_info': {
      return sendRequest('get_rpm_info');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown character avatar action: ${action}`
      });
  }
}
