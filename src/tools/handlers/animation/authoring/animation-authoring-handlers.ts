import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import { createUnknownActionResponse } from '../../foundation/dispatch/handler-error-context.js';
import { ResponseFactory } from '../../../../utils/responses/response-factory.js';
import { handleAnimationBlueprintAction } from './animation-authoring-blueprints.js';
import { handleBlendSpaceAction } from './animation-authoring-blend-spaces.js';
import { handleAnimationMontageAction } from './animation-authoring-montages.js';
import { handleRigRetargetingAction } from './animation-authoring-rig-retargeting.js';
import { handleAnimationSequenceEventAction } from './animation-authoring-sequence-events.js';
import { handleAnimationSequenceSettingAction } from './animation-authoring-sequence-settings.js';
import { handleAnimationSequenceAction } from './animation-authoring-sequences.js';
import {
  ANIMATION_AUTHORING_PATH_PARAMS,
  type AnimationAuthoringResult,
  sendAnimationAuthoringRequest,
  validateAnimationPath,
  validateRequiredPath,
} from './animation-authoring-utils.js';
import { normalizeArgs } from '../../foundation/arguments/argument-helper.js';

async function handleAnimationInfoAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<AnimationAuthoringResult | undefined> {
  if (action !== 'get_animation_info') {
    return undefined;
  }

  const params = normalizeArgs(args, [{ key: 'assetPath', required: true }]);
  const assetPathValidation = validateRequiredPath(params, 'assetPath');
  if (!assetPathValidation.valid) {
    return assetPathValidation.error;
  }

  return await sendAnimationAuthoringRequest(
    tools,
    {
      subAction: 'get_animation_info',
      assetPath: assetPathValidation.sanitized,
    },
    'Failed to get animation info',
    'Animation info retrieved'
  );
}

function validatePathArguments(args: HandlerArgs): AnimationAuthoringResult | undefined {
  for (const param of ANIMATION_AUTHORING_PATH_PARAMS) {
    const value = args[param];
    if (typeof value === 'string') {
      const pathValidation = validateAnimationPath(value, param);
      if (!pathValidation.valid) {
        return pathValidation.error;
      }
    }
  }

  return undefined;
}

export async function handleAnimationAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<AnimationAuthoringResult> {
  try {
    const pathError = validatePathArguments(args);
    if (pathError !== undefined) {
      return pathError;
    }

    const handlers = [
      handleAnimationSequenceAction,
      handleAnimationSequenceEventAction,
      handleAnimationSequenceSettingAction,
      handleAnimationMontageAction,
      handleBlendSpaceAction,
      handleAnimationBlueprintAction,
      handleRigRetargetingAction,
      handleAnimationInfoAction,
    ];

    for (const handler of handlers) {
      const result = await handler(action, args, tools);
      if (result !== undefined) {
        return result;
      }
    }

    return createUnknownActionResponse(action, `Unknown animation authoring action: ${action}`);
  } catch (error) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Animation authoring operation failed: ${err.message}`, 'ANIMATION_AUTHORING_ERROR');
  }
}
