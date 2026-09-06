import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean } from '../foundation/arguments/argument-helper.js';
import { normalizeAssetPath } from './material-authoring-common.js';

export async function handleMaterialInfoAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'compile_material': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'compile_material',
          assetPath,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to compile material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material compiled');
      }

      // Retrieve material metadata, expressions, and property overview
      case 'get_material_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'filter' },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const filter = extractOptionalString(params, 'filter');

        const payload: Record<string, unknown> = {
          subAction: 'get_material_info',
          assetPath,
        };
        if (filter) payload.filter = filter;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get material info', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material info retrieved');
      }

      // ===== 8.5 Graph Query & Mutation =====

      // Search for nodes by type or name
      case 'find_node': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'nodeType' },
          { key: 'name' },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const nodeType = extractOptionalString(params, 'nodeType');
        const name = extractOptionalString(params, 'name');

        if (!nodeType && !name) {
          return ResponseFactory.error('manage_material_authoring.find_node: requires at least one of nodeType or name', 'MISSING_SEARCH_CRITERIA');
        }

        const payload: Record<string, unknown> = {
          subAction: 'find_node',
          assetPath,
        };
        if (nodeType) payload.nodeType = nodeType;
        if (name) payload.name = name;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to find node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node search complete');
      }

      // Get input/output connections for a node
      case 'get_node_connections': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'nodeId', required: true },
          { key: 'direction' },
          { key: 'depth', default: 1 },
          { key: 'upstream' },
          { key: 'downstream' },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const nodeId = extractString(params, 'nodeId');

        const payload: Record<string, unknown> = {
          subAction: 'get_node_connections',
          assetPath,
          nodeId,
        };
        const direction = extractOptionalString(params, 'direction');
        const depth = extractOptionalNumber(params, 'depth');
        const upstream = extractOptionalBoolean(params, 'upstream');
        const downstream = extractOptionalBoolean(params, 'downstream');
        if (direction) payload.direction = direction;
        if (depth !== undefined && depth !== null) payload.depth = depth;
        if (upstream !== undefined && upstream !== null) payload.upstream = upstream;
        if (downstream !== undefined && downstream !== null) payload.downstream = downstream;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get node connections', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node connections retrieved');
      }

      // Get editable properties of a node
      case 'get_node_properties': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'nodeId', required: true },
        ]);
        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const nodeId = extractString(params, 'nodeId');

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'get_node_properties',
          assetPath,
          nodeId,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get node properties', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node properties retrieved');
      }

      // Set a static switch parameter value on a material
      case 'set_static_switch_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'value', required: true },
          { key: 'save', default: true },
        ]);
        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const parameterName = extractString(params, 'parameterName');
        const value = extractOptionalBoolean(params, 'value');
        if (value === undefined) {
          return ResponseFactory.error(
            'manage_material_authoring.set_static_switch_parameter_value: value must be a boolean',
            'INVALID_VALUE'
          );
        }
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_static_switch_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set static switch parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Static switch parameter set');
      }


    default:
      return undefined;
  }
}
