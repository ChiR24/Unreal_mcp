import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

export { isRecord };

export function findAutomationFailure(response: unknown): Record<string, unknown> | null {
  if (!isRecord(response)) return null;
  if (response.success === false || response.isError === true) return response;

  const resultFailure = findAutomationFailure(response.result);
  if (resultFailure) return resultFailure;

  const dataFailure = findAutomationFailure(response.data);
  if (dataFailure) return dataFailure;

  return null;
}

function getStringField(source: Record<string, unknown>, key: string): string | undefined {
  const value = source[key];
  return typeof value === 'string' && value.length > 0 ? value : undefined;
}

export function automationFailureResponse(
  response: Record<string, unknown>,
  failure: Record<string, unknown>,
  fallbackMessage: string,
  extraFields: Record<string, unknown>
): Record<string, unknown> {
  const error = getStringField(failure, 'error') ?? getStringField(failure, 'errorCode') ?? 'OPERATION_FAILED';
  const message = getStringField(failure, 'message') ?? fallbackMessage;
  return cleanObject({
    success: false,
    isError: true,
    error,
    message,
    ...extraFields,
    data: response
  });
}

export function failedOperationResponse(
  response: { readonly error?: string; readonly message?: string },
  fallbackError: string,
  fallbackMessage: string,
  extraFields: Record<string, unknown>
): Record<string, unknown> {
  return cleanObject({
    success: false,
    error: typeof response.error === 'string' ? response.error.toUpperCase() : fallbackError,
    message: typeof response.message === 'string' ? response.message : fallbackMessage,
    ...extraFields,
    data: response
  });
}

export function assetSuccessResponse(
  response: unknown,
  successMessage: string,
  fallbackError: string = 'OPERATION_FAILED',
  fallbackMessage: string = 'Asset operation failed',
  extraFields: Record<string, unknown> = {}
): Record<string, unknown> {
  if (isRecord(response) && response.success === false) {
    return failedOperationResponse(response, fallbackError, fallbackMessage, extraFields);
  }
  return ResponseFactory.success(response, successMessage);
}
