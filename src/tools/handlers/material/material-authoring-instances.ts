import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean, extractOptionalObject } from '../foundation/arguments/argument-helper.js';
import { normalizeAssetPath, parseMaterialPath } from './material-authoring-common.js';
import { toFiniteNumber } from '../../../utils/validation/normalize.js';

export async function handleMaterialInstanceAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'create_material_instance': {
        // Support both old format (name+path+parentMaterial) and new format (instancePath+parentMaterialPath)
        const rawArgs = args as Record<string, unknown>;
        const instancePath = extractOptionalString(rawArgs, 'instancePath') ??
                            extractOptionalString(rawArgs, 'instance_path') ??
                            extractOptionalString(rawArgs, 'materialPath');
        const parentMaterialPath = extractOptionalString(rawArgs, 'parentMaterialPath') ??
                                  extractOptionalString(rawArgs, 'parent_material_path') ??
                                  extractOptionalString(rawArgs, 'parentMaterial') ??
                                  extractOptionalString(rawArgs, 'parent');

        let name: string;
        let path: string;
        let parentMaterial: string;

        if (instancePath) {
          // Normalize and parse full path into name and directory
          const parsed = parseMaterialPath(normalizeAssetPath(instancePath));
          if (!parsed || !parsed.name || !parsed.path) {
            return ResponseFactory.error('manage_material_authoring.create_material_instance: invalid instancePath format', 'INVALID_ARGUMENT');
          }
          name = parsed.name;
          path = parsed.path;
          parentMaterial = parentMaterialPath ?? '';
        } else {
          // Use normalizeArgs for individual name/path
          const params = normalizeArgs(args, [
            { key: 'name', required: true },
            { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
            { key: 'parentMaterial', aliases: ['parent'], required: true },
          ]);
          name = extractString(params, 'name');
          path = normalizeAssetPath(extractOptionalString(params, 'path') ?? '/Game/Materials');
          parentMaterial = extractString(params, 'parentMaterial');
        }

        if (!parentMaterial) {
          return ResponseFactory.error('manage_material_authoring.create_material_instance: parentMaterialPath or parent is required', 'MISSING_PARENT');
        }

        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'create_material_instance',
          name,
          path,
          parentMaterial,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material instance', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material instance '${name}' created`);
      }

      // Set a scalar parameter override on a material instance
      case 'set_scalar_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'value', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        // 'value' can arrive as a numeric string (the MCP boundary stringifies scalars);
        // extractOptionalNumber rejected non-number types, so the old `?? 0` silently zeroed
        // every value (0.3 -> 0). Use the codebase's toFiniteNumber, which accepts a number or
        // a numeric string and rejects blank/boolean/array/non-parseable inputs (Number() would
        // coerce '', false, [] to 0). A bad value surfaces as an error, not a silent 0.
        const rawValue = (params as Record<string, unknown>)['value'];
        const value = toFiniteNumber(rawValue);
        if (value === undefined) {
          return ResponseFactory.error(
            `manage_material_authoring.set_scalar_parameter_value: 'value' must be a finite number, got ${JSON.stringify(rawValue)}`,
            'INVALID_ARGUMENT');
        }
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_scalar_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set scalar parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Scalar parameter '${parameterName}' set to ${value}`);
      }

      // Set a vector/color parameter override on a material instance
      case 'set_vector_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'value', aliases: ['color'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const value = extractOptionalObject(params, 'value') ?? { r: 1, g: 1, b: 1, a: 1 };
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_vector_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set vector parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Vector parameter '${parameterName}' set`);
      }

      // Set a texture parameter override on a material instance
      case 'set_texture_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'texturePath', aliases: ['value'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const texturePath = extractString(params, 'texturePath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_texture_parameter_value',
          assetPath,
          parameterName,
          texturePath,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set texture parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Texture parameter '${parameterName}' set`);
      }

      // ===== 8.5 Specialized Materials =====
      // Create a specialized material (landscape, decal, or post-process)

    default:
      return undefined;
  }
}
