import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { AutomationResponse } from '../../../../types/automation/automation-responses.js';
import { ResponseFactory } from '../../../../utils/responses/response-factory.js';
import { executeAutomationRequest } from '../../foundation/dispatch/common-handlers.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean } from '../../foundation/arguments/argument-helper.js';
import { validateAnimationPath as validatePath } from './animation-authoring-utils.js';

export async function handleIkRetargetingAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {

  case 'create_pose_library': {
    const params = normalizeArgs(args, [
      { key: 'name', required: true },
      { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
      { key: 'skeletonPath', required: true },
      { key: 'save', default: true },
    ]);

    const name = extractString(params, 'name');
    const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
    const rawSkeletonPath = extractString(params, 'skeletonPath');
    const skeletonPathValidation = validatePath(rawSkeletonPath, 'skeletonPath');
    if (!skeletonPathValidation.valid) {
      return skeletonPathValidation.error;
    }
    const skeletonPath = skeletonPathValidation.sanitized;
    const save = extractOptionalBoolean(params, 'save') ?? true;

    const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
      subAction: 'create_pose_library',
      name,
      path,
      skeletonPath,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create pose library', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Pose library '${name}' created`);
      }

      // ===== 10.6 Retargeting =====
  case 'create_ik_rig': {
    const params = normalizeArgs(args, [
      { key: 'name', required: true },
      { key: 'path', aliases: ['directory'], default: '/Game/Retargeting' },
      { key: 'skeletalMeshPath', required: false },
      { key: 'skeletonPath', required: false },
      { key: 'save', default: true },
    ]);

    const name = extractString(params, 'name');
    const path = extractOptionalString(params, 'path') ?? '/Game/Retargeting';
    const skeletalMeshPath = extractOptionalString(params, 'skeletalMeshPath');
    const skeletonPath = extractOptionalString(params, 'skeletonPath');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
      subAction: 'create_ik_rig',
      name,
      path,
      skeletalMeshPath,
      skeletonPath,
      save,
    })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create IK rig', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `IK Rig '${name}' created`);
      }

      case 'add_ik_chain': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: false },
          { key: 'chainName', required: true },
          { key: 'startBone', required: true },
          { key: 'endBone', required: true },
          { key: 'goal' },
          { key: 'save', default: true },
        ]);

  const rawAssetPath = extractOptionalString(params, 'assetPath');
  const assetPath = rawAssetPath ? validatePath(rawAssetPath, 'assetPath') : undefined;
  if (assetPath && !assetPath.valid) {
    return assetPath.error;
  }
  const chainName = extractString(params, 'chainName');
  const startBone = extractString(params, 'startBone');
  const endBone = extractString(params, 'endBone');
  const goal = extractOptionalString(params, 'goal');
  const save = extractOptionalBoolean(params, 'save') ?? true;

  const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
    subAction: 'add_ik_chain',
    assetPath: assetPath?.sanitized,
          chainName,
          startBone,
          endBone,
          goal,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add IK chain', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `IK chain '${chainName}' added`);
      }

      case 'create_ik_retargeter': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Retargeting' },
          { key: 'sourceIKRigPath', required: true },
          { key: 'targetIKRigPath', required: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Retargeting';
        const sourceIKRigPath = extractString(params, 'sourceIKRigPath');
        const targetIKRigPath = extractString(params, 'targetIKRigPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_ik_retargeter',
          name,
          path,
          sourceIKRigPath,
          targetIKRigPath,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create IK retargeter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `IK Retargeter '${name}' created`);
      }

      case 'set_retarget_chain_mapping': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: false },
          { key: 'sourceChain', required: true },
          { key: 'targetChain', required: true },
          { key: 'save', default: true },
        ]);

  const rawAssetPath = extractOptionalString(params, 'assetPath');
  const assetPath = rawAssetPath ? validatePath(rawAssetPath, 'assetPath') : undefined;
  if (assetPath && !assetPath.valid) {
    return assetPath.error;
  }
  const sourceChain = extractString(params, 'sourceChain');
  const targetChain = extractString(params, 'targetChain');
  const save = extractOptionalBoolean(params, 'save') ?? true;

  const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
    subAction: 'set_retarget_chain_mapping',
    assetPath: assetPath?.sanitized,
          sourceChain,
          targetChain,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set retarget chain mapping', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Chain mapping '${sourceChain}' -> '${targetChain}' set`);
      }


    default:
      return undefined;
  }
}
