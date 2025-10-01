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
 * Consistent error handling for all tools
 */
export class ErrorHandler {
  /**
   * Create a standardized error response
   */
  static createErrorResponse(
    error: any,
    toolName: string,
    context?: any
  ): BaseToolResponse {
    const errorType = this.categorizeError(error);
    const userMessage = this.getUserFriendlyMessage(errorType, error);
    const retriable = this.isRetriable(error);
    const scope = context?.scope || `tool-call/${toolName}`;
    
    log.error(`Tool ${toolName} failed:`, {
      type: errorType,
      message: error.message || error,
      retriable,
      scope,
      context
    });

    return {
      success: false,
      error: userMessage,
      message: `Failed to execute ${toolName}: ${userMessage}`,
      retriable: retriable as any,
      scope: scope as any,
      // Add debug info in development
      ...(process.env.NODE_ENV === 'development' && {
        _debug: {
          errorType,
          originalError: error.message || String(error),
          stack: error.stack,
          context,
          retriable,
          scope
        }
      })
    } as any;
  }

  /**
   * Categorize error by type
   */
  private static categorizeError(error: any): ErrorType {
    const explicitType = (error?.type || error?.errorType || '').toString().toUpperCase();
    if (explicitType && Object.values(ErrorType).includes(explicitType as ErrorType)) {
      return explicitType as ErrorType;
    }

    const errorMessage = error?.message?.toLowerCase() || String(error).toLowerCase();

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
      errorMessage.includes('remote control') ||
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
   */
  private static getUserFriendlyMessage(type: ErrorType, error: any): string {
    const originalMessage = error.message || String(error);

    switch (type) {
      case ErrorType.CONNECTION:
        return 'Failed to connect to Unreal Engine. Please ensure Remote Control is enabled and the engine is running.';
      
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

  /** Determine if an error is likely retriable */
  private static isRetriable(error: any): boolean {
    try {
      const code = (error?.code || '').toString().toUpperCase();
      const msg = (error?.message || String(error) || '').toLowerCase();
      const status = Number((error?.response?.status));
      if (['ECONNRESET','ECONNREFUSED','ETIMEDOUT','EPIPE'].includes(code)) return true;
      if (/timeout|timed out|network|connection|closed|unavailable|busy|temporar/.test(msg)) return true;
      if (!isNaN(status) && (status === 429 || (status >= 500 && status < 600))) return true;
    } catch {}
    return false;
  }

  /**
   * Retry an async operation with exponential backoff
   * Best practice from TypeScript async programming patterns
   * @param operation - Async operation to retry
   * @param options - Retry configuration
   * @returns Result of the operation
   */
  static async retryWithBackoff<T>(
    operation: () => Promise<T>,
    options: {
      maxRetries?: number;
      initialDelay?: number;
      maxDelay?: number;
      backoffMultiplier?: number;
      shouldRetry?: (error: unknown) => boolean;
    } = {}
  ): Promise<T> {
    const {
      maxRetries = 3,
      initialDelay = 100,
      maxDelay = 10000,
      backoffMultiplier = 2,
      shouldRetry = (error) => this.isRetriable(error)
    } = options;

    let lastError: unknown;
    let delay = initialDelay;

    for (let attempt = 0; attempt <= maxRetries; attempt++) {
      try {
        return await operation();
      } catch (error) {
        lastError = error;

        if (attempt === maxRetries || !shouldRetry(error)) {
          throw error;
        }

        log.debug(`Retry attempt ${attempt + 1}/${maxRetries} after ${delay}ms`);
        await new Promise(resolve => setTimeout(resolve, delay));

        delay = Math.min(delay * backoffMultiplier, maxDelay);
      }
    }

    throw lastError;
  }

  /**
   * Add timeout to any promise
   * @param promise - Promise to add timeout to
   * @param timeoutMs - Timeout in milliseconds
   * @param errorMessage - Custom error message for timeout
   * @returns Promise that rejects on timeout
   */
  static async withTimeout<T>(
    promise: Promise<T>,
    timeoutMs: number,
    errorMessage = 'Operation timed out'
  ): Promise<T> {
    let timeoutHandle: NodeJS.Timeout | undefined;

    const timeoutPromise = new Promise<never>((_, reject) => {
      timeoutHandle = setTimeout(() => {
        reject(new Error(errorMessage));
      }, timeoutMs);
    });

    try {
      return await Promise.race([promise, timeoutPromise]);
    } finally {
      if (timeoutHandle !== undefined) {
        clearTimeout(timeoutHandle);
      }
    }
  }

  /**
   * Execute multiple operations with Promise.allSettled for better error handling
   * Returns detailed results for each operation, including failures
   * @param operations - Array of async operations to execute
   * @returns Object with successful and failed operations separated
   */
  static async batchExecute<T>(
    operations: Array<() => Promise<T>>
  ): Promise<{
    successful: Array<{ index: number; value: T }>;
    failed: Array<{ index: number; reason: unknown }>;
    successCount: number;
    failureCount: number;
  }> {
    const results = await Promise.allSettled(operations.map(op => op()));

    const successful: Array<{ index: number; value: T }> = [];
    const failed: Array<{ index: number; reason: unknown }> = [];

    results.forEach((result, index) => {
      if (result.status === 'fulfilled') {
        successful.push({ index, value: result.value });
      } else {
        failed.push({ index, reason: result.reason });
      }
    });

    return {
      successful,
      failed,
      successCount: successful.length,
      failureCount: failed.length
    };
  }
}
