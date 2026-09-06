import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { extractOptionalString } from '../foundation/arguments/argument-helper.js';

export async function handleMaterialConnectionAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'connect_nodes':
      case 'connect_material_pins': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ?? '';

        const sourceNodeId = extractOptionalString(rawArgs, 'sourceNodeId') ??
                            extractOptionalString(rawArgs, 'fromNode') ??
                            extractOptionalString(rawArgs, 'nodeId') ?? '';
        let targetNodeId = extractOptionalString(rawArgs, 'targetNodeId') ??
                           extractOptionalString(rawArgs, 'toNode') ?? '';
        const sourcePin = extractOptionalString(rawArgs, 'sourcePin') ??
                         extractOptionalString(rawArgs, 'fromPin') ?? '';
        const targetPin = extractOptionalString(rawArgs, 'targetPin') ??
                          extractOptionalString(rawArgs, 'toPin') ??
                          extractOptionalString(rawArgs, 'inputName') ?? '';

        if (!targetNodeId && targetPin) {
          targetNodeId = 'Main';
        } else if (targetNodeId.toLowerCase() === 'root') {
          targetNodeId = 'Main';
        }

        const effectiveSourceId = sourceNodeId || sourcePin;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'connect_nodes',
          assetPath,
          sourceNodeId: effectiveSourceId,
          sourcePin,
          targetNodeId,
          inputName: targetPin,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to connect nodes', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Nodes connected');
      }

      // Disconnect material expression nodes or break specific pin connections
      case 'disconnect_nodes':
      case 'break_material_connections': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        // Accept both nodeId and pinName as identifiers
        const nodeId = extractOptionalString(rawArgs, 'nodeId') ??
                      extractOptionalString(rawArgs, 'pinName') ?? '';
        const pinName = extractOptionalString(rawArgs, 'pinName');

        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.disconnect_nodes: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('manage_material_authoring.disconnect_nodes: missing required argument nodeId (or pinName)', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'disconnect_nodes',
          assetPath,
          nodeId,
          pinName,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to disconnect nodes', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Nodes disconnected');
      }


    default:
      return undefined;
  }
}
