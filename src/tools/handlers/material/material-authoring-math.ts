import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber } from '../foundation/arguments/argument-helper.js';

export async function handleMaterialMathAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'add_math_node': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'operation', required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
          { key: 'constA', aliases: ['valueA'] },
          { key: 'constB', aliases: ['valueB'] },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const operation = extractString(params, 'operation');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;
        const constA = extractOptionalNumber(params, 'constA');
        const constB = extractOptionalNumber(params, 'constB');

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_math_node',
          assetPath,
          operation,
          x,
          y,
          constA,
          constB,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add math node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Math node '${operation}' added`);
      }

      // Add utility expression nodes (world position, normals, UV animation, noise, etc.)
      case 'add_world_position':
      case 'add_vertex_normal':
      case 'add_pixel_depth':
      case 'add_fresnel':
      case 'add_reflection_vector':
      case 'add_panner':
      case 'add_rotator':
      case 'add_noise':
      case 'add_voronoi': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: action,
          assetPath,
          x,
          y,
          ...args, // Pass through any additional params
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} node added`);
      }

      // Add conditional nodes (if/switch) for branching material logic
      case 'add_if':
      case 'add_switch': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: action,
          assetPath,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} node added`);
      }

      // Add a custom HLSL expression node with configurable inputs/outputs
      case 'add_custom_expression': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'code', aliases: ['hlsl'], required: true },
          { key: 'outputType', default: 'Float1' },
          { key: 'description' },
          { key: 'inputs' },
          { key: 'additionalOutputs' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const code = extractString(params, 'code');
        const outputType = extractOptionalString(params, 'outputType') ?? 'Float1';
        const description = extractOptionalString(params, 'description');
        const inputs = (params as Record<string, unknown>).inputs;
        const additionalOutputs = (params as Record<string, unknown>).additionalOutputs;
        if (inputs != null && !Array.isArray(inputs)) {
          return ResponseFactory.error('manage_material_authoring.add_custom_expression: inputs must be an array', 'INVALID_INPUTS');
        }
        if (inputs != null && Array.isArray(inputs)) {
          for (let i = 0; i < inputs.length; i++) {
            const item = inputs[i] as Record<string, unknown>;
            if (!item || typeof item !== 'object' || typeof item.name !== 'string' || !item.name.trim()) {
              return ResponseFactory.error(
                `manage_material_authoring.add_custom_expression: inputs[${i}] must be an object with a non-empty string "name"`,
                'INVALID_INPUTS'
              );
            }
          }
        }
        if (additionalOutputs != null && !Array.isArray(additionalOutputs)) {
          return ResponseFactory.error('manage_material_authoring.add_custom_expression: additionalOutputs must be an array', 'INVALID_OUTPUTS');
        }
        if (additionalOutputs != null && Array.isArray(additionalOutputs)) {
          for (let i = 0; i < additionalOutputs.length; i++) {
            const item = additionalOutputs[i] as Record<string, unknown>;
            if (!item || typeof item !== 'object' || typeof item.name !== 'string' || !item.name.trim()) {
              return ResponseFactory.error(
                `manage_material_authoring.add_custom_expression: additionalOutputs[${i}] must be an object with a non-empty string "name"`,
                'INVALID_OUTPUTS'
              );
            }
            if (item.type != null && typeof item.type !== 'string') {
              return ResponseFactory.error(
                `manage_material_authoring.add_custom_expression: additionalOutputs[${i}].type must be a string if provided`,
                'INVALID_OUTPUTS'
              );
            }
          }
        }
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const payload: Record<string, unknown> = {
          subAction: 'add_custom_expression',
          assetPath,
          code,
          outputType,
          description,
          x,
          y,
        };
        if (inputs != null) {
          payload.inputs = inputs;
        }
        if (additionalOutputs != null) {
          payload.additionalOutputs = additionalOutputs;
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add custom expression', res.errorCode);
        }
        const response = ResponseFactory.success(res, res.message ?? 'Custom HLSL expression added');
        const result = res.result;
        if (result && typeof result === 'object' && !Array.isArray(result)) {
          const nodeId = (result as Record<string, unknown>).nodeId;
          if (typeof nodeId === 'string') response.nodeId = nodeId;
        }
        return response;
      }


    default:
      return undefined;
  }
}
