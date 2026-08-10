import { ResponseFactory } from '../../../../utils/responses/response-factory.js';
import { cleanObject } from '../../../../utils/serialization/safe-json.js';

/**
 * Wraps a handler execution with uniform error handling.
 * Executes the async handler; on success returns its value;
 * on throw or rejection, delegates raw error to ResponseFactory.error unchanged.
 */
export async function withHandlerContext<T>(handler: () => Promise<T>): Promise<T> {
  try {
    return await handler();
  } catch (error) {
    return ResponseFactory.error(error) as T;
  }
}

export function createUnknownActionResponse(
  message: string,
  extra?: Record<string, unknown>
): Record<string, unknown> {
  return cleanObject({
    success: false,
    error: 'UNKNOWN_ACTION',
    message,
    ...(extra ?? {})
  });
}