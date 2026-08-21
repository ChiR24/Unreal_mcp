import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../../types/automation/automation-responses.js';
import { ResponseFactory } from '../../../../utils/responses/response-factory.js';
import { executeAutomationRequest } from '../../foundation/dispatch/common-handlers.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean } from '../../foundation/arguments/argument-helper.js';
import { validateAnimationPath as validatePath, nonNegativeNumberOrDefault } from './animation-authoring-utils.js';

export async function handleMontageBlendAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {

  case 'set_blend_in': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'blendTime', default: 0.25 },
      { key: 'blendOption', default: 'Linear' },
      { key: 'save', default: true },
    ]);

    const rawAssetPath = extractString(params, 'assetPath');
    const assetPathValidation = validatePath(rawAssetPath, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const blendTime = nonNegativeNumberOrDefault(params['blendTime'], 0.25);
    const blendOption = extractOptionalString(params, 'blendOption') ?? 'Linear';
    const save = extractOptionalBoolean(params, 'save') ?? true;

    const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
      subAction: 'set_blend_in',
      assetPath,
          blendTime,
          blendOption,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set blend in', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Blend in settings updated');
      }

  case 'set_blend_out': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'blendTime', default: 0.25 },
      { key: 'blendOption', default: 'Linear' },
      { key: 'save', default: true },
    ]);

    const rawAssetPath = extractString(params, 'assetPath');
    const assetPathValidation = validatePath(rawAssetPath, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const blendTime = nonNegativeNumberOrDefault(params['blendTime'], 0.25);
    const blendOption = extractOptionalString(params, 'blendOption') ?? 'Linear';
    const save = extractOptionalBoolean(params, 'save') ?? true;

    const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
      subAction: 'set_blend_out',
      assetPath,
          blendTime,
          blendOption,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set blend out', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Blend out settings updated');
      }

  case 'link_sections': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'fromSection', required: true },
      { key: 'toSection', required: true },
      { key: 'save', default: true },
    ]);

    const rawAssetPath = extractString(params, 'assetPath');
    const assetPathValidation = validatePath(rawAssetPath, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const fromSection = extractString(params, 'fromSection');
    const toSection = extractString(params, 'toSection');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
      subAction: 'link_sections',
      assetPath,
          fromSection,
          toSection,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to link sections', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Linked '${fromSection}' to '${toSection}'`);
      }

      // ===== 10.3 Blend Spaces =====

    default:
      return undefined;
  }
}
