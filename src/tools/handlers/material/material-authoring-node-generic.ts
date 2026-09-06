import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { extractOptionalString, extractOptionalNumber, extractOptionalBoolean } from '../foundation/arguments/argument-helper.js';
import { handleMaterialInfoAction } from './material-authoring-info.js';

export async function handleMaterialNodeGenericAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'add_material_node': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        const nodeType = extractOptionalString(rawArgs, 'nodeType') ??
                        extractOptionalString(rawArgs, 'type') ?? '';
        const x = extractOptionalNumber(rawArgs, 'x') ?? extractOptionalNumber(rawArgs, 'posX') ?? 0;
        const y = extractOptionalNumber(rawArgs, 'y') ?? extractOptionalNumber(rawArgs, 'posY') ?? 0;

        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.add_material_node: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeType) {
          return ResponseFactory.error('manage_material_authoring.add_material_node: missing required argument nodeType', 'MISSING_NODE_TYPE');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_material_node',
          assetPath,
          nodeType,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add material node', res.errorCode);
        }
        const response = ResponseFactory.success(res, res.message ?? `Material node '${nodeType}' added`);
        const result = res.result;
        if (result && typeof result === 'object' && !Array.isArray(result)) {
          const nodeId = (result as Record<string, unknown>).nodeId;
          if (typeof nodeId === 'string') response.nodeId = nodeId;
        }
        return response;
      }

      // Alias connect_material_pins -> connect_nodes is handled via fallthrough at the connect_nodes case above

      // Alias: rebuild_material -> compile_material
      case 'rebuild_material':
        return await handleMaterialInfoAction('compile_material', args, tools);

      // Generic parameter setter
      case 'set_material_parameter': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ??
                         extractOptionalString(rawArgs, 'instancePath') ?? '';
        const parameterName = extractOptionalString(rawArgs, 'parameterName') ?? '';
        const parameterType = extractOptionalString(rawArgs, 'parameterType') ?? 'scalar';
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;
        const value = rawArgs.value;

        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.set_material_parameter: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!parameterName) {
          return ResponseFactory.error('manage_material_authoring.set_material_parameter: missing required argument parameterName', 'MISSING_PARAMETER_NAME');
        }
        if (value === undefined) {
          return ResponseFactory.error('manage_material_authoring.set_material_parameter: missing required argument value', 'MISSING_VALUE');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_material_parameter',
          assetPath,
          parameterName,
          value,
          parameterType,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Parameter '${parameterName}' set`);
      }

      // Get node details
      case 'get_material_node_details': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        const nodeId = extractOptionalString(rawArgs, 'nodeId') ?? '';

        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.get_material_node_details: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('manage_material_authoring.get_material_node_details: missing required argument nodeId', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'get_material_node_details',
          assetPath,
          nodeId,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get node details', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node details retrieved');
      }

      // Remove material node
      case 'remove_material_node': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        const nodeId = extractOptionalString(rawArgs, 'nodeId') ?? '';

        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.remove_material_node: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('manage_material_authoring.remove_material_node: missing required argument nodeId', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'remove_material_node',
          assetPath,
          nodeId,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to remove node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material node removed');
      }


    default:
      return undefined;
  }
}
