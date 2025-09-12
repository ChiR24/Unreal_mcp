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
 * Custom error class for MCP tools
 */
export class ToolError extends Error {
  constructor(
    public type: ErrorType,
    public toolName: string,
    message: string,
    public originalError?: any
  ) {
    super(message);
    this.name = 'ToolError';
  }
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
    
    log.error(`Tool ${toolName} failed:`, {
      type: errorType,
      message: error.message || error,
      context
    });

    return {
      success: false,
      error: userMessage,
      message: `Failed to execute ${toolName}: ${userMessage}`,
      // Add debug info in development
      ...(process.env.NODE_ENV === 'development' && {
        _debug: {
          errorType,
          originalError: error.message || String(error),
          stack: error.stack,
          context
        }
      })
    };
  }

  /**
   * Create a standardized warning response
   */
  static createWarningResponse(
    message: string,
    result: any,
    toolName: string
  ): BaseToolResponse {
    log.warn(`Tool ${toolName} warning: ${message}`);
    
    return {
      success: true,
      warning: message,
      message: `${toolName} completed with warnings`,
      ...result
    };
  }

  /**
   * Create a standardized success response
   */
  static createSuccessResponse(
    message: string,
    data: any = {}
  ): BaseToolResponse {
    return {
      success: true,
      message,
      ...data
    };
  }

  /**
   * Categorize error by type
   */
  private static categorizeError(error: any): ErrorType {
    if (error instanceof ToolError) {
      return error.type;
    }

    const errorMessage = error.message?.toLowerCase() || String(error).toLowerCase();

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

  /**
   * Wrap async function with error handling
   */
  static async wrapAsync<T extends BaseToolResponse>(
    toolName: string,
    fn: () => Promise<T>,
    context?: any
  ): Promise<T> {
    try {
      const result = await fn();
      
      // Ensure result has success field
      if (typeof result === 'object' && result !== null) {
        if (!('success' in result)) {
          (result as any).success = true;
        }
      }
      
      return result;
    } catch (error) {
      return this.createErrorResponse(error, toolName, context) as T;
    }
  }

  /**
   * Validate required parameters
   */
  static validateParams(
    params: any,
    required: string[],
    toolName: string
  ): void {
    if (!params || typeof params !== 'object') {
      throw new ToolError(
        ErrorType.PARAMETER,
        toolName,
        'Invalid parameters: expected object'
      );
    }

    for (const field of required) {
      if (!(field in params) || params[field] === undefined || params[field] === null) {
        throw new ToolError(
          ErrorType.PARAMETER,
          toolName,
          `Missing required parameter: ${field}`
        );
      }

      // Additional validation for common types
      if (field.includes('Path') || field.includes('Name')) {
        if (typeof params[field] !== 'string' || params[field].trim() === '') {
          throw new ToolError(
            ErrorType.PARAMETER,
            toolName,
            `Invalid ${field}: must be a non-empty string`
          );
        }
      }
    }
  }

  /**
   * Handle Unreal Engine specific errors
   */
  static handleUnrealError(error: any, operation: string): string {
    const errorStr = String(error.message || error).toLowerCase();

    // Common Unreal errors
    if (errorStr.includes('worldcontext')) {
      return `${operation} completed (WorldContext warnings are normal)`;
    }
    
    if (errorStr.includes('does not exist')) {
      return `Asset or object not found for ${operation}`;
    }
    
    if (errorStr.includes('access denied') || errorStr.includes('read-only')) {
      return `Permission denied for ${operation}. Check file permissions.`;
    }
    
    if (errorStr.includes('already exists')) {
      return `Object already exists for ${operation}`;
    }

    return `Unreal Engine error during ${operation}: ${error.message || error}`;
  }

  /**
   * Create operation result with consistent structure
   */
  static createResult<T extends BaseToolResponse>(
    success: boolean,
    message: string,
    data?: Partial<T>
  ): T {
    const result: BaseToolResponse = {
      success,
      message,
      ...(data || {})
    };

    if (!success && !result.error) {
      result.error = message;
    }

    return result as T;
  }
}