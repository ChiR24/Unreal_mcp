import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean } from '../../foundation/arguments/argument-helper.js';
import { nonNegativeNumberOrDefault, sendAnimationAuthoringRequest, validateRequiredPath } from './animation-authoring-utils.js';

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

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const blendTime = nonNegativeNumberOrDefault(params['blendTime'], 0.25);
    const blendOption = extractOptionalString(params, 'blendOption') ?? 'Linear';
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'set_blend_in',
      assetPath,
      blendTime,
      blendOption,
      save,
    }, 'Failed to set blend in', 'Blend in settings updated');
  }

  case 'set_blend_out': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'blendTime', default: 0.25 },
      { key: 'blendOption', default: 'Linear' },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const blendTime = nonNegativeNumberOrDefault(params['blendTime'], 0.25);
    const blendOption = extractOptionalString(params, 'blendOption') ?? 'Linear';
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'set_blend_out',
      assetPath,
      blendTime,
      blendOption,
      save,
    }, 'Failed to set blend out', 'Blend out settings updated');
  }

  case 'link_sections': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'fromSection', required: true },
      { key: 'toSection', required: true },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const fromSection = extractString(params, 'fromSection');
    const toSection = extractString(params, 'toSection');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'link_sections',
      assetPath,
      fromSection,
      toSection,
      save,
    }, 'Failed to link sections', `Linked '${fromSection}' to '${toSection}'`);
  }

    default:
      return undefined;
  }
}
