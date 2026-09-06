import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString } from '../foundation/arguments/argument-helper.js';
import { normalizeAssetPath } from './material-authoring-common.js';

export async function handleMaterialNodeDeleteAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'delete_node': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = normalizeAssetPath(extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ??
                         extractOptionalString(rawArgs, 'functionPath') ?? '');
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.delete_node: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        const nodeId = extractOptionalString(rawArgs, 'nodeId');
        const nodeIdsRaw = Array.isArray(rawArgs.nodeIds) ? rawArgs.nodeIds : undefined;
        const nodeIds = nodeIdsRaw?.filter((id): id is string => typeof id === 'string' && id.trim().length > 0);
        if (nodeIdsRaw && (!nodeIds || nodeIds.length !== nodeIdsRaw.length)) {
          return ResponseFactory.error(
            'manage_material_authoring.delete_node: nodeIds must be an array of non-empty strings',
            'INVALID_NODE_IDS'
          );
        }
        if (!nodeId && (!nodeIds || nodeIds.length === 0)) {
          return ResponseFactory.error(
            'manage_material_authoring.delete_node: provide nodeId or a non-empty nodeIds array',
            'MISSING_NODE_ID'
          );
        }

        const payload: Record<string, unknown> = {
          subAction: 'delete_node',
          assetPath,
        };
        if (nodeId) payload.nodeId = nodeId;
        if (nodeIds && nodeIds.length > 0) payload.nodeIds = nodeIds;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to delete node(s)', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node(s) deleted');
      }

      // Update code, description, or pins on a custom expression node
      case 'update_custom_expression': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'nodeId', required: true },
          { key: 'code' },
          { key: 'description' },
          { key: 'outputType' },
          { key: 'inputs' },
          { key: 'additionalOutputs' },
        ]);
        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const nodeId = extractString(params, 'nodeId');

        const payload: Record<string, unknown> = {
          subAction: 'update_custom_expression',
          assetPath,
          nodeId,
        };
        const code = extractOptionalString(params, 'code');
        const description = extractOptionalString(params, 'description');
        const outputType = extractOptionalString(params, 'outputType');
        const inputs = (params as Record<string, unknown>).inputs;
        const additionalOutputs = (params as Record<string, unknown>).additionalOutputs;
        if (inputs != null && !Array.isArray(inputs)) {
          return ResponseFactory.error('manage_material_authoring.update_custom_expression: inputs must be an array', 'INVALID_INPUTS');
        }
        if (inputs != null && Array.isArray(inputs)) {
          for (let i = 0; i < inputs.length; i++) {
            const item = inputs[i] as Record<string, unknown>;
            if (!item || typeof item !== 'object' || typeof item.name !== 'string' || !item.name.trim()) {
              return ResponseFactory.error(
                `manage_material_authoring.update_custom_expression: inputs[${i}] must be an object with a non-empty string "name"`,
                'INVALID_INPUTS'
              );
            }
          }
        }
        if (additionalOutputs != null && !Array.isArray(additionalOutputs)) {
          return ResponseFactory.error('manage_material_authoring.update_custom_expression: additionalOutputs must be an array', 'INVALID_OUTPUTS');
        }
        if (additionalOutputs != null && Array.isArray(additionalOutputs)) {
          for (let i = 0; i < additionalOutputs.length; i++) {
            const item = additionalOutputs[i] as Record<string, unknown>;
            if (!item || typeof item !== 'object' || typeof item.name !== 'string' || !item.name.trim()) {
              return ResponseFactory.error(
                `manage_material_authoring.update_custom_expression: additionalOutputs[${i}] must be an object with a non-empty string "name"`,
                'INVALID_OUTPUTS'
              );
            }
            if (item.type != null && typeof item.type !== 'string') {
              return ResponseFactory.error(
                `manage_material_authoring.update_custom_expression: additionalOutputs[${i}].type must be a string if provided`,
                'INVALID_OUTPUTS'
              );
            }
          }
        }
        const hasCode = code !== undefined && code !== null;
        const hasDescription = description !== undefined && description !== null;
        const hasOutputType = outputType !== undefined && outputType !== null;
        const hasInputs = inputs !== undefined && inputs !== null;
        const hasAdditionalOutputs = additionalOutputs !== undefined && additionalOutputs !== null;
        if (!hasCode && !hasDescription && !hasOutputType && !hasInputs && !hasAdditionalOutputs) {
          return ResponseFactory.error(
            'manage_material_authoring.update_custom_expression: provide at least one field to update',
            'MISSING_UPDATE_FIELDS'
          );
        }
        if (hasCode) payload.code = code;
        if (hasDescription) payload.description = description;
        if (hasOutputType) payload.outputType = outputType;
        if (hasInputs) payload.inputs = inputs;
        if (hasAdditionalOutputs) payload.additionalOutputs = additionalOutputs;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to update custom expression', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Custom expression updated');
      }


    default:
      return undefined;
  }
}
