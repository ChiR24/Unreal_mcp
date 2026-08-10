import { cleanObject } from '../../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { AnimationArgs, AutomationResponse, HandlerArgs } from '../../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../../foundation/dispatch/common-handlers.js';
import {
  findSkeletalMeshComponent,
  ResultPayload,
  sanitizeAnimationPath,
  securityViolation
} from './animation-handler-utils.js';

export async function tryHandleSpecialAnimationAction(
  animAction: string,
  args: HandlerArgs,
  argsTyped: AnimationArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (animAction === 'create_animation_blueprint' || animAction === 'create_anim_blueprint' || animAction === 'create_animation_bp') {
    return await handleCreateAnimationBlueprint(args, argsTyped, tools);
  }
  if (animAction === 'play_anim_montage' || animAction === 'play_montage') {
    return await handlePlayMontage(args, argsTyped, tools);
  }
  if (animAction === 'setup_ragdoll') {
    return await handleSetupRagdoll(argsTyped, tools);
  }
  if (animAction === 'activate_ragdoll') {
    return await handleActivateRagdoll(argsTyped, tools);
  }
  return undefined;
}

async function handleCreateAnimationBlueprint(args: HandlerArgs, argsTyped: AnimationArgs, tools: ITools): Promise<Record<string, unknown>> {
  const name = argsTyped.name ?? argsTyped.blueprintName;
  const skeletonPath = argsTyped.skeletonPath ?? argsTyped.targetSkeleton;
  let meshPath = argsTyped.meshPath;
  let savePath: string;

  try {
    savePath = sanitizeAnimationPath(argsTyped.savePath ?? argsTyped.path, '/Game/Animations');
  } catch (e) {
    return securityViolation(e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected');
  }

  if (!skeletonPath && argsTyped.actorName) {
    const meshComp = await findSkeletalMeshComponent(tools, argsTyped.actorName);
    if (meshComp) {
      if (!meshPath && meshComp.path) meshPath = meshComp.path;
      if (!meshPath && meshComp.skeletalMesh) meshPath = meshComp.skeletalMesh;
    }
  }

  const payload = {
    ...args,
    name,
    skeletonPath,
    meshPath,
    savePath
  };

  return await executeAutomationRequest(
    tools,
    'create_animation_blueprint',
    payload,
    'Automation bridge not available for animation blueprint creation'
  ) as Record<string, unknown>;
}

async function handlePlayMontage(args: HandlerArgs, argsTyped: AnimationArgs, tools: ITools): Promise<Record<string, unknown>> {
  const resp = await executeAutomationRequest(
    tools,
    'play_anim_montage',
    args,
    'Automation bridge not available for montage playback'
  ) as AutomationResponse;
  const result = (resp?.result ?? resp ?? {}) as ResultPayload;
  const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
  const message = typeof result.message === 'string' ? result.message : '';
  const msgLower = message.toLowerCase();

  if (msgLower.includes('actor not found') || msgLower.includes('no animation played') || errorCode === 'ACTOR_NOT_FOUND') {
    return cleanObject({
      success: false,
      error: 'ACTOR_NOT_FOUND',
      message: message || 'Actor not found; no animation played',
      actorName: argsTyped.actorName
    });
  }

  if (
    errorCode === 'INVALID_ARGUMENT' &&
    msgLower.includes('actorname required') &&
    typeof argsTyped.playRate === 'number' &&
    argsTyped.playRate === 0
  ) {
    return cleanObject({
      success: true,
      noOp: true,
      message: 'Montage playback skipped: playRate 0 with missing actorName treated as no-op.'
    });
  }

  return cleanObject(resp);
}

async function handleSetupRagdoll(argsTyped: AnimationArgs, tools: ITools): Promise<Record<string, unknown>> {
  const mutableArgs = { ...argsTyped } as AnimationArgs & Record<string, unknown>;

  if (argsTyped.actorName && !argsTyped.meshPath && !argsTyped.skeletonPath) {
    const meshComp = await findSkeletalMeshComponent(tools, argsTyped.actorName);
    if (meshComp && meshComp.path) {
      mutableArgs.meshPath = meshComp.path;
    }
  }

  const resp = await executeAutomationRequest(tools, 'setup_ragdoll', mutableArgs, 'Automation bridge not available for ragdoll setup') as AutomationResponse;
  const result = (resp?.result ?? resp ?? {}) as ResultPayload;
  const message = typeof result.message === 'string' ? result.message : '';
  const msgLower = message.toLowerCase();

  if (msgLower.includes('actor not found') || msgLower.includes('no ragdoll applied')) {
    return cleanObject({
      success: false,
      error: 'ACTOR_NOT_FOUND',
      message: message || 'Actor not found; no ragdoll applied',
      actorName: argsTyped.actorName
    });
  }

  return cleanObject(resp);
}

async function handleActivateRagdoll(argsTyped: AnimationArgs, tools: ITools): Promise<Record<string, unknown>> {
  const mutableArgs = { ...argsTyped } as AnimationArgs & Record<string, unknown>;

  if (argsTyped.actorName && !argsTyped.meshPath && !argsTyped.skeletonPath) {
    const meshComp = await findSkeletalMeshComponent(tools, argsTyped.actorName);
    if (meshComp && meshComp.path) {
      mutableArgs.meshPath = meshComp.path;
    }
  }

  // Task 21: route to the distinct native activate_ragdoll action, NOT
  // setup_ragdoll, so the activation toggle is actually reachable.
  const resp = await executeAutomationRequest(tools, 'activate_ragdoll', mutableArgs, 'Automation bridge not available for ragdoll activation') as AutomationResponse;
  const result = (resp?.result ?? resp ?? {}) as ResultPayload;
  const message = typeof result.message === 'string' ? result.message : '';
  const msgLower = message.toLowerCase();

  if (msgLower.includes('actor not found') || msgLower.includes('no ragdoll applied')) {
    return cleanObject({
      success: false,
      error: 'ACTOR_NOT_FOUND',
      message: message || 'Actor not found; no ragdoll activation',
      actorName: argsTyped.actorName
    });
  }

  return cleanObject(resp);
}
