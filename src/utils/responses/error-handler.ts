import { Logger } from '../logging/logger.js';
import type { BaseToolResponse } from '../../types/tools/tool-base-types.js';
import { categorizeError, getUserFriendlyMessage, isRetriableError, normalizeErrorToLike } from './error-classification.js';
import type { ErrorType as ErrorTypeName } from './error-classification.js';

export { ErrorType } from './error-classification.js';

const log = new Logger('ErrorHandler');

interface ErrorResponseDebug {
  errorType: ErrorTypeName;
  originalError: string;
  stack?: string;
  context?: Record<string, unknown>;
  retriable: boolean;
  scope: string;
}

interface ErrorToolResponse extends BaseToolResponse {
  _debug?: ErrorResponseDebug;
}

export class ErrorHandler {
  static createErrorResponse(
    error: unknown,
    toolName: string,
    context?: Record<string, unknown>
  ): ErrorToolResponse {
    const errorObj = normalizeErrorToLike(error);
    const errorType = categorizeError(errorObj);
    const userMessage = getUserFriendlyMessage(errorType, errorObj);
    const retriable = isRetriableError(errorObj);
    const scope = typeof context?.scope === 'string' && context.scope.trim()
      ? context.scope
      : `tool-call/${toolName}`;
    const errorMessage = errorObj.message || String(error);
    const errorStack = errorObj.stack;

    log.error(`Tool ${toolName} failed:`, {
      type: errorType,
      message: errorMessage,
      retriable,
      scope,
      context
    });

    const response: ErrorToolResponse = {
      success: false,
      error: userMessage,
      message: `Failed to execute ${toolName}: ${userMessage}`,
      retriable,
      scope
    };

    if (process.env.NODE_ENV === 'development') {
      response._debug = {
        errorType,
        originalError: errorMessage,
        stack: errorStack,
        context,
        retriable,
        scope
      };
    }

    return response;
  }

  static async retryWithBackoff<T>(
    fn: () => Promise<T>,
    options: {
      maxRetries?: number;
      initialDelay?: number;
      maxDelay?: number;
      backoffMultiplier?: number;
      shouldRetry?: (error: unknown) => boolean;
    } = {}
  ): Promise<T> {
    const maxRetries = options.maxRetries ?? 3;
    const initialDelay = options.initialDelay ?? 1000;
    const maxDelay = options.maxDelay ?? 10000;
    const multiplier = options.backoffMultiplier ?? 2;
    const shouldRetry = options.shouldRetry ?? ((err: unknown) => isRetriableError(err));

    let delay = initialDelay;

    for (let attempt = 0; attempt <= maxRetries; attempt++) {
      try {
        return await fn();
      } catch (error) {
        if (attempt === maxRetries || !shouldRetry(error)) {
          throw error;
        }

        await new Promise(resolve => setTimeout(resolve, delay));
        delay = Math.min(delay * multiplier, maxDelay);
      }
    }
    throw new Error('Max retries exceeded');
  }
}
