import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean } from '../foundation/arguments/argument-helper.js';

export async function handleMaterialSpecializedAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'create_landscape_material':
      case 'create_decal_material':
      case 'create_post_process_material': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: action,
          name,
          path,
          save,
          ...args, // Pass through extra params like layers for landscape
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action.replace(/_/g, ' ')} created`);
      }

      // Add a landscape layer (paint layer) to a landscape material
      case 'add_landscape_layer': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'layerName', required: true },
          { key: 'blendType', default: 'LB_WeightBlend' },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const layerName = extractString(params, 'layerName');
        const blendType = extractOptionalString(params, 'blendType') ?? 'LB_WeightBlend';

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_landscape_layer',
          assetPath,
          layerName,
          blendType,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add landscape layer', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Landscape layer '${layerName}' added`);
      }

      // Configure the layer blend node with layer weight assignments
      case 'configure_layer_blend': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'layers', required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const layers = params.layers;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'configure_layer_blend',
          assetPath,
          layers,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to configure layer blend', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Layer blend configured');
      }


    default:
      return undefined;
  }
}
