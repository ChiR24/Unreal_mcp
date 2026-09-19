import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../types/automation/automation-responses.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean } from '../foundation/arguments/argument-helper.js';
import { normalizeAssetPath, parseMaterialPath } from './material-authoring-common.js';

export async function handleMaterialCreationAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'create_material': {
        // Check if materialPath is provided (full path like /Game/MCPTest/M_Test)
        const rawArgs = args as Record<string, unknown>;
        const materialPath = extractOptionalString(rawArgs, 'materialPath') ??
                            extractOptionalString(rawArgs, 'material_path') ??
                            extractOptionalString(rawArgs, 'assetPath');

        let name: string;
        let path: string;

        if (materialPath) {
          // Normalize and parse full path into name and directory
          const parsed = parseMaterialPath(normalizeAssetPath(materialPath));
          if (!parsed || !parsed.name || !parsed.path) {
            return ResponseFactory.error('manage_material_authoring.create_material: invalid materialPath format', 'INVALID_ARGUMENT');
          }
          name = parsed.name;
          path = parsed.path;
        } else {
          // Use normalizeArgs for individual name/path
          const params = normalizeArgs(args, [
            { key: 'name', required: true },
            { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          ]);
          name = extractString(params, 'name');
          path = normalizeAssetPath(extractOptionalString(params, 'path') ?? '/Game/Materials');
        }

        const materialDomain = extractOptionalString(rawArgs, 'materialDomain') ??
                              extractOptionalString(rawArgs, 'domain') ?? 'Surface';
        const blendMode = extractOptionalString(rawArgs, 'blendMode') ?? 'Opaque';
        const shadingModel = extractOptionalString(rawArgs, 'shadingModel') ?? 'DefaultLit';
        const twoSided = extractOptionalBoolean(rawArgs, 'twoSided') ?? false;
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'create_material',
          name,
          path,
          materialDomain,
          blendMode,
          shadingModel,
          twoSided,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material '${name}' created`);
      }

      // Set the blend mode on a material (Opaque, Translucent, Masked, etc.)
      case 'set_blend_mode': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'instancePath'], required: true },
          { key: 'blendMode', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const blendMode = extractString(params, 'blendMode');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_blend_mode',
          assetPath,
          blendMode,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set blend mode', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Blend mode set to ${blendMode}`);
      }

      // Set the shading model (DefaultLit, Unlit, Subsurface, etc.)
      case 'set_shading_model': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'instancePath'], required: true },
          { key: 'shadingModel', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const shadingModel = extractString(params, 'shadingModel');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_shading_model',
          assetPath,
          shadingModel,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set shading model', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Shading model set to ${shadingModel}`);
      }

      // Set the material domain (Surface, DeferredDecal, PostProcess, etc.)
      case 'set_material_domain': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'domain', aliases: ['materialDomain'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const domain = extractString(params, 'domain');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_material_domain',
          assetPath,
          materialDomain: domain,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set material domain', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material domain set to ${domain}`);
      }

      case 'set_two_sided': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'twoSided', aliases: ['enabled'], default: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const twoSided = extractOptionalBoolean(params, 'twoSided') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_two_sided',
          assetPath,
          twoSided,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set two-sided', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Two-sided set to ${twoSided}`);
      }

      // Set cast shadows property
      case 'set_cast_shadows': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'castShadows', aliases: ['enabled'], default: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const castShadows = extractOptionalBoolean(params, 'castShadows') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_cast_shadows',
          assetPath,
          castShadows,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set cast shadows', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Cast shadows set to ${castShadows}`);
      }

    default:
      return undefined;
  }
}
