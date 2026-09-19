import { cleanObject } from '../../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { AnimationArgs, ComponentInfo } from '../../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../../foundation/dispatch/common-handlers.js';
import { sanitizePath } from '../../../../utils/paths/path-security.js';
// One list for the whole animation domain. The runtime copy used to repeat 12 of
// these 18 names, so a path field added for authoring went unchecked at runtime.
import { ANIMATION_AUTHORING_PATH_PARAMS } from '../authoring/animation-authoring-utils.js';

export interface SkeletalMeshComponentInfo extends ComponentInfo {
  type?: string;
  className?: string;
  skeletalMesh?: string;
  path?: string;
}

export interface ResultPayload {
  error?: string;
  message?: string;
  [key: string]: unknown;
}

interface ComponentsResponse {
  success?: boolean;
  components?: ComponentInfo[];
  [key: string]: unknown;
}

function isComponentsResponse(value: unknown): value is ComponentsResponse {
  return typeof value === 'object' &&
    value !== null &&
    (!('components' in value) || Array.isArray(value.components));
}

function isSkeletalMeshComponent(component: ComponentInfo): component is SkeletalMeshComponentInfo {
  return component.type === 'SkeletalMeshComponent' || component.className === 'SkeletalMeshComponent';
}

export function securityViolation(message: string): Record<string, unknown> {
  return cleanObject({
    success: false,
    error: 'SECURITY_VIOLATION',
    message
  });
}

export function validateAnimationPathInputs(args: Record<string, unknown>): Record<string, unknown> | undefined {
  for (const param of ANIMATION_AUTHORING_PATH_PARAMS) {
    const value = args[param];
    if (value && typeof value === 'string') {
      try {
        sanitizePath(value);
      } catch (e) {
        return securityViolation(e instanceof Error ? e.message : `Invalid ${param}: path traversal or illegal characters detected`);
      }
    }
  }

  const artifacts = args.artifacts;
  if (Array.isArray(artifacts)) {
    for (const artifact of artifacts) {
      if (typeof artifact === 'string') {
        try {
          sanitizePath(artifact);
        } catch (e) {
          return securityViolation(e instanceof Error ? e.message : 'Invalid path in artifacts: path traversal or illegal characters detected');
        }
      }
    }
  }

  return undefined;
}

export async function findSkeletalMeshComponent(tools: ITools, actorName: string): Promise<SkeletalMeshComponentInfo | undefined> {
  let response: unknown;
  try {
    response = await executeAutomationRequest(tools, 'control_actor', { action: 'get_components', actorName });
  } catch (error) {
    if (!(error instanceof Error)) {
      throw error;
    }
    return undefined;
  }

  if (!isComponentsResponse(response) || !Array.isArray(response.components)) {
    return undefined;
  }

  return response.components.find(isSkeletalMeshComponent);
}

export function applyBlendSpaceAxisAliases(mutableArgs: AnimationArgs & Record<string, unknown>, argsTyped: AnimationArgs): void {
  if (argsTyped.horizontalAxis) {
    mutableArgs.minX = argsTyped.horizontalAxis.minValue;
    mutableArgs.maxX = argsTyped.horizontalAxis.maxValue;
  }
  if (argsTyped.verticalAxis) {
    mutableArgs.minY = argsTyped.verticalAxis.minValue;
    mutableArgs.maxY = argsTyped.verticalAxis.maxValue;
  }
}

export function sanitizeAnimationPath(rawPath: unknown, fallback: string): string {
  return sanitizePath(String(rawPath || fallback));
}

export function optionalArray(value: unknown): unknown[] | undefined {
  return Array.isArray(value) ? value : undefined;
}
