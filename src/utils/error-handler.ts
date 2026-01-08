import { Logger } from './logger.js';
import { BaseToolResponse } from '../types/tool-types.js';

const log = new Logger('ErrorHandler');

/**
 * Error types for categorization
 */
export enum ErrorType {
  VALIDATION = 'VALIDATION',
  CONNECTION = 'CONNECTION',
  UNREAL_ENGINE = 'UNREAL_ENGINE',
  PARAMETER = 'PARAMETER',
  EXECUTION = 'EXECUTION',
  TIMEOUT = 'TIMEOUT',
  UNKNOWN = 'UNKNOWN'
}

/**
 * Debug information attached to error responses in development mode
 */
interface ErrorResponseDebug {
  errorType: ErrorType;
  originalError: string;
  stack?: string;
  context?: Record<string, unknown>;
  retriable: boolean;
  scope: string;
}

/**
 * Extended error response with optional debug info
 */
interface ErrorToolResponse extends BaseToolResponse {
  // When present, provides machine-actionable details to help callers (LLMs) repair the request.
  errorDetails?: {
    kind: 'validation' | 'parameter' | 'connection' | 'execution' | 'timeout' | 'unknown';
    toolName: string;
    missing?: string[];
    invalid?: Array<{ path: string; message: string }>;
    expected?: Array<{ path: string; hint: string }>;
  };
  _debug?: ErrorResponseDebug;
}

/**
 * Represents any error object with common properties
 */
interface ErrorLike {
  message?: string;
  code?: string;
  type?: string;
  errorType?: string;
  stack?: string;
  response?: { status?: number };
  details?: unknown;
}

/**
 * Normalize any error type to ErrorLike interface
 */
function normalizeErrorToLike(error: unknown): ErrorLike {
  // Handle ZodError (duck-typing)
  const isZodError = typeof error === 'object' && error !== null && 'issues' in error && Array.isArray((error as any).issues);
  const details = isZodError 
    ? (error as any).issues 
    : (typeof error === 'object' && error !== null && 'details' in error ? (error as any).details : undefined);

  if (error instanceof Error) {
    return {
      message: error.message,
      stack: error.stack,
      code: (error as NodeJS.ErrnoException).code,
      details
    };
  }
  if (typeof error === 'object' && error !== null) {
    const obj = error as Record<string, unknown>;
    return {
      message: typeof obj.message === 'string' ? obj.message : undefined,
      code: typeof obj.code === 'string' ? obj.code : undefined,
      type: typeof obj.type === 'string' ? obj.type : undefined,
      errorType: typeof obj.errorType === 'string' ? obj.errorType : undefined,
      stack: typeof obj.stack === 'string' ? obj.stack : undefined,
      response: typeof obj.response === 'object' && obj.response !== null
        ? {
          status: typeof (obj.response as Record<string, unknown>).status === 'number'
            ? (obj.response as Record<string, unknown>).status as number
            : undefined
        }
        : undefined,
      details: obj.details || details
    };
  }
  return { message: String(error) };
}

/**
 * Consistent error handling for all tools
 */
export class ErrorHandler {
  /**
   * Create a standardized error response
   * @param error - The error object (can be Error, string, object with message, or unknown)
   * @param toolName - Name of the tool that failed
   * @param context - Optional additional context for debugging
   */
  static createErrorResponse(
    error: unknown,
    toolName: string,
    context?: Record<string, unknown>
  ): ErrorToolResponse {
    const errorObj = normalizeErrorToLike(error);
    const errorType = this.categorizeError(errorObj);
    const userMessage = this.getUserFriendlyMessage(errorType, errorObj);
    const retriable = this.isRetriable(errorObj);
    const scope = (context?.scope as string) || `tool-call/${toolName}`;
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

    // Attach LLM-actionable details when possible.
    try {
      const kindFromType: NonNullable<ErrorToolResponse['errorDetails']>['kind'] =
        errorType === ErrorType.VALIDATION ? 'validation'
          : errorType === ErrorType.PARAMETER ? 'parameter'
            : errorType === ErrorType.CONNECTION ? 'connection'
              : errorType === ErrorType.TIMEOUT ? 'timeout'
                : errorType === ErrorType.EXECUTION ? 'execution'
                  : 'unknown';

      const missing: string[] = [];
      const invalid: Array<{ path: string; message: string }> = [];
      const expected: Array<{ path: string; hint: string }> = [];

      // Heuristics: parse common "Missing required parameter: X" patterns.
      if (typeof errorMessage === 'string') {
        const missingMatch = errorMessage.match(/Missing required parameter:\s*([A-Za-z0-9_\-\.]+)/);
        if (missingMatch?.[1]) missing.push(missingMatch[1]);
      }

      // Duck-type Zod issues if provided (shape: [{ path, message, ... }]).
      const details = errorObj.details;
      if (Array.isArray(details)) {
        for (const item of details) {
          const it = item as { path?: unknown; message?: unknown };
          const msg = typeof it.message === 'string' ? it.message : undefined;
          const p = Array.isArray(it.path)
            ? it.path.map(seg => String(seg)).join('.')
            : typeof it.path === 'string'
              ? it.path
              : '';
          if (msg) {
            invalid.push({ path: p || 'root', message: msg });
          }
        }
      }

      // Provide some generic, high-confidence hints only for relevant tools.
      if (toolName && (toolName.includes('asset') || toolName.includes('level') || toolName.includes('manage_') || toolName.includes('control_'))) {
        expected.push({ path: '*/Path', hint: "Unreal paths should generally look like '/Game/Folder/Asset'" });
        expected.push({ path: 'action', hint: 'Most tools require an action enum string such as "list", "create", "delete".' });
      }

      if (missing.length > 0 || invalid.length > 0 || expected.length > 0) {
        response.errorDetails = {
          kind: kindFromType,
          toolName,
          missing: missing.length > 0 ? missing : undefined,
          invalid: invalid.length > 0 ? invalid : undefined,
          expected: expected.length > 0 ? expected : undefined
        };
      }
    } catch {
      // best-effort only
    }

    // Add debug info in development
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

  /**
   * Categorize error by type
   * @param error - The error to categorize
   */
  private static categorizeError(error: ErrorLike | Error | string): ErrorType {
    const errorObj = typeof error === 'object' ? error as ErrorLike : null;
    const explicitType = (errorObj?.type || errorObj?.errorType || '').toString().toUpperCase();
    if (explicitType && Object.values(ErrorType).includes(explicitType as ErrorType)) {
      return explicitType as ErrorType;
    }

    const errorMessage = (errorObj?.message || String(error)).toLowerCase();

    // Connection errors
    if (
      errorMessage.includes('econnrefused') ||
      errorMessage.includes('timeout') ||
      errorMessage.includes('connection') ||
      errorMessage.includes('network')
    ) {
      return ErrorType.CONNECTION;
    }

    // Validation errors
    if (
      errorMessage.includes('invalid') ||
      errorMessage.includes('required') ||
      errorMessage.includes('must be') ||
      errorMessage.includes('validation')
    ) {
      return ErrorType.VALIDATION;
    }

    // Unreal Engine specific errors
    if (
      errorMessage.includes('unreal') ||
      errorMessage.includes('connection failed') ||
      errorMessage.includes('blueprint') ||
      errorMessage.includes('actor') ||
      errorMessage.includes('asset')
    ) {
      return ErrorType.UNREAL_ENGINE;
    }

    // Parameter errors
    if (
      errorMessage.includes('parameter') ||
      errorMessage.includes('argument') ||
      errorMessage.includes('missing')
    ) {
      return ErrorType.PARAMETER;
    }

    // Timeout errors
    if (errorMessage.includes('timeout')) {
      return ErrorType.TIMEOUT;
    }

    return ErrorType.UNKNOWN;
  }

  /**
   * Get user-friendly error message
   * @param type - The categorized error type
   * @param error - The original error
   */
  private static getUserFriendlyMessage(type: ErrorType, error: ErrorLike | Error | string): string {
    const originalMessage = (typeof error === 'object' && error !== null && 'message' in error)
      ? (error as { message?: string }).message || String(error)
      : String(error);

    switch (type) {
      case ErrorType.CONNECTION:
        return 'Failed to connect to Unreal Engine. Please ensure the Automation Bridge plugin is active and the editor is running.';

      case ErrorType.VALIDATION:
        return `Invalid input: ${originalMessage}`;

      case ErrorType.UNREAL_ENGINE:
        return `Unreal Engine error: ${originalMessage}`;

      case ErrorType.PARAMETER:
        return `Invalid parameters: ${originalMessage}`;

      case ErrorType.TIMEOUT:
        return 'Operation timed out. Unreal Engine may be busy or unresponsive.';

      case ErrorType.EXECUTION:
        return `Execution failed: ${originalMessage}`;

      default:
        return originalMessage;
    }
  }

  /**
   * Determine if an error is likely retriable
   * @param error - The error to check
   */
  private static isRetriable(error: ErrorLike | Error | string): boolean {
    try {
      const errorObj = typeof error === 'object' ? error as ErrorLike : null;
      const code = (errorObj?.code || '').toString().toUpperCase();
      const msg = (errorObj?.message || String(error) || '').toLowerCase();
      const status = Number(errorObj?.response?.status);
      if (['ECONNRESET', 'ECONNREFUSED', 'ETIMEDOUT', 'EPIPE'].includes(code)) return true;
      if (/timeout|timed out|network|connection|closed|unavailable|busy|temporar/.test(msg)) return true;
      if (!isNaN(status) && (status === 429 || (status >= 500 && status < 600))) return true;
    } catch (err) {
      // Error checking retriability is uncommon; log at debug level
      log.debug('isRetriable check failed', err instanceof Error ? err.message : String(err));
    }
    return false;
  }

  /**
   * Retry a function with exponential backoff
   * @param fn - The async function to retry
   * @param options - Retry configuration options
   */
  static async retryWithBackoff<T>(
    fn: () => Promise<T>,
    options: {
      maxRetries?: number;
      initialDelay?: number;
      maxDelay?: number;
      backoffMultiplier?: number;
      shouldRetry?: (error: ErrorLike | Error | unknown) => boolean;
    } = {}
  ): Promise<T> {
    const maxRetries = options.maxRetries ?? 3;
    const initialDelay = options.initialDelay ?? 1000;
    const maxDelay = options.maxDelay ?? 10000;
    const multiplier = options.backoffMultiplier ?? 2;
    const shouldRetry = options.shouldRetry ?? ((err: unknown) => this.isRetriable(err as ErrorLike));

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
