import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean } from '../../foundation/arguments/argument-helper.js';
import { sendAnimationAuthoringRequest, validateRequiredPath } from './animation-authoring-utils.js';

export async function handleAimOffsetAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {

      case 'create_aim_offset': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
          { key: 'skeletonPath', required: false },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
        const skeletonPath = extractOptionalString(params, 'skeletonPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        return await sendAnimationAuthoringRequest(tools, {
          subAction: 'create_aim_offset',
          name,
          path,
          skeletonPath,
          save,
        }, 'Failed to create aim offset', `Aim Offset '${name}' created`);
      }

  case 'add_aim_offset_sample': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', required: true },
      { key: 'animationPath', required: true },
      { key: 'yaw', required: true },
      { key: 'pitch', required: true },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const animationPathValidation = validateRequiredPath(params, 'animationPath');
    if (!animationPathValidation.valid) {
      return animationPathValidation.error;
    }
    const animationPath = animationPathValidation.sanitized;
    const yaw = extractOptionalNumber(params, 'yaw') ?? 0;
    const pitch = extractOptionalNumber(params, 'pitch') ?? 0;
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_aim_offset_sample',
      assetPath,
      animationPath,
      yaw,
      pitch,
      save,
    }, 'Failed to add aim offset sample', 'Aim offset sample added');
  }

    default:
      return undefined;
  }
}
