import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools, StandardActionResponse } from '../../../../types/tools/tool-interfaces.js';
import { ResponseFactory } from '../../../../utils/responses/response-factory.js';
import { sanitizePath } from '../../../../utils/paths/path-security.js';
import { executeAutomationRequest } from '../../foundation/dispatch/common-handlers.js';
import { extractOptionalString, extractString } from '../../foundation/arguments/argument-helper.js';
import { isRecord } from '../../../../utils/validation/type-guards.js';

export type AnimationAuthoringResult = Record<string, unknown>;

type PathValidation =
  | { readonly valid: true; readonly sanitized: string }
  | { readonly valid: false; readonly error: StandardActionResponse };

type AutomationFailure = {
  readonly success: false;
  readonly error?: unknown;
  readonly errorCode?: string;
};

export const ANIMATION_AUTHORING_PATH_PARAMS = [
  'path',
  'savePath',
  'skeletonPath',
  'skeletalMeshPath',
  'sourceSkeleton',
  'targetSkeleton',
  'assetPath',
  'animationPath',
  'blueprintPath',
  'retargeterPath',
  'meshPath',
  'montagePath',
  'animSequencePath',
  'animPath',
  'animAssetPath',
  'animMontagePath',
  'blendSpacePath',
  'rigPath',
] as const;

function isAutomationFailure(value: unknown): value is AutomationFailure {
  if (!isRecord(value)) {
    return false;
  }

  return value['success'] === false;
}

function responseMessage(value: unknown): string | undefined {
  if (!isRecord(value)) {
    return undefined;
  }

  const message = value['message'];
  return typeof message === 'string' ? message : undefined;
}

export function validateAnimationPath(path: string, fieldName: string): PathValidation {
  try {
    return { valid: true, sanitized: sanitizePath(path) };
  } catch (error) {
    const message = error instanceof Error
      ? error.message
      : `Invalid ${fieldName}: path traversal or illegal characters detected`;
    return {
      valid: false,
      error: ResponseFactory.error(message, 'SECURITY_VIOLATION'),
    };
  }
}

export function validateRequiredPath(params: Record<string, unknown>, key: string): PathValidation {
  return validateAnimationPath(extractString(params, key), key);
}

export function validateOptionalPath(
  params: Record<string, unknown>,
  key: string
): PathValidation | undefined {
  const value = extractOptionalString(params, key);
  return value === undefined ? undefined : validateAnimationPath(value, key);
}

export function finiteNumber(value: unknown): number | undefined {
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

export function finiteNumberOrDefault(value: unknown, fallback: number): number {
  return finiteNumber(value) ?? fallback;
}

export function positiveNumberOrDefault(value: unknown, fallback: number): number {
  const parsed = finiteNumber(value);
  return parsed !== undefined && parsed > 0 ? parsed : fallback;
}

export function positiveIntegerOrDefault(value: unknown, fallback: number): number {
  return Math.floor(positiveNumberOrDefault(value, fallback));
}

export function optionalPositiveNumber(value: unknown): number | undefined {
  const parsed = finiteNumber(value);
  return parsed !== undefined && parsed > 0 ? parsed : undefined;
}

export function optionalPositiveInteger(value: unknown): number | undefined {
  const parsed = optionalPositiveNumber(value);
  return parsed === undefined ? undefined : Math.floor(parsed);
}

export function nonNegativeNumberOrDefault(value: unknown, fallback: number): number {
  const parsed = finiteNumber(value);
  return parsed !== undefined && parsed >= 0 ? parsed : fallback;
}

export function optionalNonNegativeNumber(value: unknown): number | undefined {
  const parsed = finiteNumber(value);
  return parsed !== undefined && parsed >= 0 ? parsed : undefined;
}

export function nonNegativeIntegerOrDefault(value: unknown, fallback: number): number {
  return Math.floor(nonNegativeNumberOrDefault(value, fallback));
}

export async function sendAnimationAuthoringRequest(
  tools: ITools,
  payload: HandlerArgs,
  failureMessage: string,
  successMessage: string
): Promise<AnimationAuthoringResult> {
  const result = await executeAutomationRequest(tools, 'manage_animation_authoring', payload);
  if (isAutomationFailure(result)) {
    return ResponseFactory.error(result.error ?? failureMessage, result.errorCode);
  }

  return ResponseFactory.success(result, responseMessage(result) ?? successMessage);
}
