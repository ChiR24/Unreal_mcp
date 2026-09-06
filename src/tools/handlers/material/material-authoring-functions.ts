import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean } from '../foundation/arguments/argument-helper.js';
import { normalizeAssetPath } from './material-authoring-common.js';

export async function handleMaterialFunctionAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'create_material_function': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials/Functions' },
          { key: 'description' },
          { key: 'exposeToLibrary', default: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials/Functions';
        const description = extractOptionalString(params, 'description');
        const exposeToLibrary = extractOptionalBoolean(params, 'exposeToLibrary') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'create_material_function',
          name,
          path,
          description,
          exposeToLibrary,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material function', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material function '${name}' created`);
      }

      // Add an input or output pin to a material function
      case 'add_function_input':
      case 'add_function_output': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['functionPath'], required: true },
          { key: 'inputName', aliases: ['name', 'outputName'], required: true },
          { key: 'inputType', aliases: ['type', 'outputType'], default: 'Float3' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const inputName = extractString(params, 'inputName');
        const inputType = extractOptionalString(params, 'inputType') ?? 'Float3';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: action,
          assetPath,
          inputName,
          inputType,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add function ${action === 'add_function_input' ? 'input' : 'output'}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Function ${action === 'add_function_input' ? 'input' : 'output'} '${inputName}' added`);
      }

      // Insert a material function call node into a material graph
      case 'use_material_function': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'functionPath', required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const functionPath = normalizeAssetPath(extractString(params, 'functionPath'));
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'use_material_function',
          assetPath,
          functionPath,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to use material function', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material function added');
      }

      // ===== 8.4 Material Instances =====
      case 'get_material_function_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['functionPath', 'materialFunctionPath'], required: true },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'get_material_function_info',
          assetPath,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get material function info', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material function info retrieved');
      }

      // ===== 8.6 Aliases and Additional Actions =====


    default:
      return undefined;
  }
}
