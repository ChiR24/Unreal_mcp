import type { AutomationRequestResponse } from './unreal-bridge-types.js';
import { isRecord } from './utils/validation/type-guards.js';

export { isRecord };

export function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export function isStringArray(value: unknown): value is string[] {
  return Array.isArray(value) && value.every(item => typeof item === 'string');
}

export function resultRecord(response: AutomationRequestResponse): Record<string, unknown> | undefined {
  return isRecord(response.result) ? { ...response.result } : undefined;
}

export function responsePayloadRecord(response: unknown): Record<string, unknown> {
  if (isRecord(response) && isRecord(response.result)) {
    return response.result;
  }
  return isRecord(response) ? response : {};
}
