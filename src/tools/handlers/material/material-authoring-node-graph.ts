import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean } from '../foundation/arguments/argument-helper.js';
import { normalizeAssetPath } from './material-authoring-common.js';

export async function handleMaterialNodeGraphAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'get_node_chain': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'startNodeId', required: true },
          { key: 'endNodeId' },
          { key: 'endPin' },
        ]);
        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const startNodeId = extractString(params, 'startNodeId');
        const endNodeId = extractOptionalString(params, 'endNodeId');
        const endPin = extractOptionalString(params, 'endPin');
        if (!endNodeId && !endPin) {
          return ResponseFactory.error(
            'manage_material_authoring.get_node_chain: provide endNodeId or endPin',
            'INVALID_ARGUMENT'
          );
        }

        const payload: Record<string, unknown> = {
          subAction: 'get_node_chain',
          assetPath,
          startNodeId,
        };
        if (endNodeId) payload.endNodeId = endNodeId;
        if (endPin) payload.endPin = endPin;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to trace node chain', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node chain traced');
      }

      // Get the connected subgraph or find orphan nodes
      case 'get_connected_subgraph': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = normalizeAssetPath(extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ??
                         extractOptionalString(rawArgs, 'functionPath') ?? '');
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.get_connected_subgraph: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        const nodeId = extractOptionalString(rawArgs, 'nodeId');
        const orphansOnly = extractOptionalBoolean(rawArgs, 'orphansOnly') ?? false;
        if (!nodeId && !orphansOnly) {
          return ResponseFactory.error(
            'manage_material_authoring.get_connected_subgraph: provide nodeId or set orphansOnly=true',
            'MISSING_NODE_ID'
          );
        }

        const payload: Record<string, unknown> = {
          subAction: 'get_connected_subgraph',
          assetPath,
          orphansOnly,
        };
        if (nodeId) payload.nodeId = nodeId;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get connected subgraph', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Connected subgraph retrieved');
      }


    default:
      return undefined;
  }
}
