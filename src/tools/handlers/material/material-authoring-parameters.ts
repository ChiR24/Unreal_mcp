import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean, extractOptionalVector } from '../foundation/arguments/argument-helper.js';

export async function handleMaterialParameterAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'add_texture_sample': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'texturePath', required: true },
          { key: 'parameterName', aliases: ['name'] },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
          { key: 'samplerType', default: 'Color' },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const texturePath = extractString(params, 'texturePath');
        const parameterName = extractOptionalString(params, 'parameterName');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;
        const samplerType = extractOptionalString(params, 'samplerType') ?? 'Color';

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_texture_sample',
          assetPath,
          texturePath,
          parameterName,
          x,
          y,
          samplerType,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add texture sample', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture sample added');
      }

      // Add a texture coordinate (UV) node
      case 'add_texture_coordinate': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'coordinateIndex', default: 0 },
          { key: 'uTiling', default: 1.0 },
          { key: 'vTiling', default: 1.0 },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const coordinateIndex = extractOptionalNumber(params, 'coordinateIndex') ?? 0;
        const uTiling = extractOptionalNumber(params, 'uTiling') ?? 1.0;
        const vTiling = extractOptionalNumber(params, 'vTiling') ?? 1.0;
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_texture_coordinate',
          assetPath,
          coordinateIndex,
          uTiling,
          vTiling,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add texture coordinate', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture coordinate added');
      }

      // Add a scalar parameter expression
      case 'add_scalar_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', default: 0.0 },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalNumber(params, 'defaultValue') ?? 0.0;
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_scalar_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add scalar parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Scalar parameter '${parameterName}' added`);
      }

      // Add a vector parameter expression (color/vector)
      case 'add_vector_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', aliases: ['color'] },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalVector(params, 'defaultValue') ?? { r: 1, g: 1, b: 1, a: 1 };
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_vector_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add vector parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Vector parameter '${parameterName}' added`);
      }

      // Add a static switch parameter for conditional material logic
      case 'add_static_switch_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', default: false },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalBoolean(params, 'defaultValue') ?? false;
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_static_switch_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add static switch', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Static switch '${parameterName}' added`);
      }


    default:
      return undefined;
  }
}
