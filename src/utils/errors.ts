/**
 * Enhanced error types for better error handling and recovery
 */

export enum ErrorCode {
  // Connection errors
  CONNECTION_FAILED = 'CONNECTION_FAILED',
  CONNECTION_TIMEOUT = 'CONNECTION_TIMEOUT',
  CONNECTION_REFUSED = 'CONNECTION_REFUSED',
  
  // API errors
  API_ERROR = 'API_ERROR',
  INVALID_RESPONSE = 'INVALID_RESPONSE',
  RATE_LIMITED = 'RATE_LIMITED',
  
  // Validation errors
  VALIDATION_ERROR = 'VALIDATION_ERROR',
  INVALID_PARAMETERS = 'INVALID_PARAMETERS',
  MISSING_REQUIRED_FIELD = 'MISSING_REQUIRED_FIELD',
  
  // Resource errors
  RESOURCE_NOT_FOUND = 'RESOURCE_NOT_FOUND',
  RESOURCE_LOCKED = 'RESOURCE_LOCKED',
  RESOURCE_UNAVAILABLE = 'RESOURCE_UNAVAILABLE',
  
  // Permission errors
  UNAUTHORIZED = 'UNAUTHORIZED',
  FORBIDDEN = 'FORBIDDEN',
  
  // System errors
  INTERNAL_ERROR = 'INTERNAL_ERROR',
  CIRCUIT_BREAKER_OPEN = 'CIRCUIT_BREAKER_OPEN',
  SERVICE_UNAVAILABLE = 'SERVICE_UNAVAILABLE'
}

export interface ErrorMetadata {
  code: ErrorCode;
  statusCode?: number;
  retriable: boolean;
  context?: Record<string, any>;
  timestamp: Date;
  correlationId?: string;
}

/**
 * Base application error with metadata
 */
export class AppError extends Error {
  public readonly metadata: ErrorMetadata;

  constructor(message: string, metadata: Partial<ErrorMetadata> = {}) {
    super(message);
    this.name = this.constructor.name;
    this.metadata = {
      code: metadata.code || ErrorCode.INTERNAL_ERROR,
      retriable: metadata.retriable || false,
      timestamp: new Date(),
      ...metadata
    };
    Error.captureStackTrace(this, this.constructor);
  }

  toJSON() {
    return {
      name: this.name,
      message: this.message,
      ...this.metadata,
      stack: this.stack
    };
  }
}

/**
 * Connection-related errors
 */
export class ConnectionError extends AppError {
  constructor(message: string, metadata: Partial<ErrorMetadata> = {}) {
    super(message, {
      code: ErrorCode.CONNECTION_FAILED,
      retriable: true,
      statusCode: 503,
      ...metadata
    });
  }
}

/**
 * API-related errors
 */
export class ApiError extends AppError {
  constructor(message: string, statusCode: number, metadata: Partial<ErrorMetadata> = {}) {
    super(message, {
      code: ErrorCode.API_ERROR,
      statusCode,
      retriable: statusCode >= 500 || statusCode === 429,
      ...metadata
    });
  }
}

/**
 * Validation errors
 */
export class ValidationError extends AppError {
  constructor(message: string, metadata: Partial<ErrorMetadata> = {}) {
    super(message, {
      code: ErrorCode.VALIDATION_ERROR,
      statusCode: 400,
      retriable: false,
      ...metadata
    });
  }
}

/**
 * Resource errors
 */
export class ResourceError extends AppError {
  constructor(message: string, code: ErrorCode, metadata: Partial<ErrorMetadata> = {}) {
    super(message, {
      code,
      statusCode: code === ErrorCode.RESOURCE_NOT_FOUND ? 404 : 409,
      retriable: code === ErrorCode.RESOURCE_LOCKED,
      ...metadata
    });
  }
}

/**
 * Circuit Breaker implementation for fault tolerance
 */
export enum CircuitState {
  CLOSED = 'CLOSED',
  OPEN = 'OPEN',
  HALF_OPEN = 'HALF_OPEN'
}

interface CircuitBreakerOptions {
  threshold: number;
  timeout: number;
  resetTimeout: number;
  onStateChange?: (oldState: CircuitState, newState: CircuitState) => void;
}

export class CircuitBreaker {
  private state: CircuitState = CircuitState.CLOSED;
  private failures = 0;
  private successCount = 0;
  private lastFailureTime?: Date;
  private readonly options: CircuitBreakerOptions;

  constructor(options: Partial<CircuitBreakerOptions> = {}) {
    this.options = {
      threshold: options.threshold || 5,
      timeout: options.timeout || 60000, // 1 minute
      resetTimeout: options.resetTimeout || 30000, // 30 seconds
      onStateChange: options.onStateChange
    };
  }

  /**
   * Execute function with circuit breaker protection
   */
  async execute<T>(fn: () => Promise<T>): Promise<T> {
    // Check circuit state
    if (this.state === CircuitState.OPEN) {
      if (this.shouldAttemptReset()) {
        this.transitionTo(CircuitState.HALF_OPEN);
      } else {
        throw new AppError('Circuit breaker is open', {
          code: ErrorCode.CIRCUIT_BREAKER_OPEN,
          retriable: true,
          context: {
            failures: this.failures,
            lastFailure: this.lastFailureTime
          }
        });
      }
    }

    try {
      const result = await fn();
      this.onSuccess();
      return result;
    } catch (error) {
      this.onFailure();
      throw error;
    }
  }

  private onSuccess(): void {
    this.failures = 0;
    if (this.state === CircuitState.HALF_OPEN) {
      this.successCount++;
      if (this.successCount >= 3) {
        this.transitionTo(CircuitState.CLOSED);
      }
    }
  }

  private onFailure(): void {
    this.failures++;
    this.lastFailureTime = new Date();
    this.successCount = 0;

    if (this.failures >= this.options.threshold) {
      this.transitionTo(CircuitState.OPEN);
    }
  }

  private shouldAttemptReset(): boolean {
    return (
      this.lastFailureTime !== undefined &&
      Date.now() - this.lastFailureTime.getTime() >= this.options.resetTimeout
    );
  }

  private transitionTo(newState: CircuitState): void {
    const oldState = this.state;
    this.state = newState;
    
    if (newState === CircuitState.CLOSED) {
      this.failures = 0;
      this.successCount = 0;
    }

    if (this.options.onStateChange && oldState !== newState) {
      this.options.onStateChange(oldState, newState);
    }
  }

  getState(): CircuitState {
    return this.state;
  }

  getMetrics() {
    return {
      state: this.state,
      failures: this.failures,
      successCount: this.successCount,
      lastFailureTime: this.lastFailureTime
    };
  }
}

/**
 * Error recovery strategies
 */
export class ErrorRecovery {
  private static circuitBreakers = new Map<string, CircuitBreaker>();

  /**
   * Get or create circuit breaker for a service
   */
  static getCircuitBreaker(service: string, options?: Partial<CircuitBreakerOptions>): CircuitBreaker {
    if (!this.circuitBreakers.has(service)) {
      this.circuitBreakers.set(service, new CircuitBreaker(options));
    }
    const breaker = this.circuitBreakers.get(service);
    if (!breaker) {
      throw new Error(`Circuit breaker for service ${service} could not be created`);
    }
    return breaker;
  }

  /**
   * Wrap function with error recovery
   */
  static async withRecovery<T>(
    fn: () => Promise<T>,
    options: {
      service: string;
      fallback?: () => T | Promise<T>;
      onError?: (error: Error) => void;
    }
  ): Promise<T> {
    const breaker = this.getCircuitBreaker(options.service);

    try {
      return await breaker.execute(fn);
    } catch (error) {
      if (options.onError) {
        options.onError(error as Error);
      }

      // Try fallback if available
      if (options.fallback) {
        return await options.fallback();
      }

      throw error;
    }
  }

  /**
   * Check if error is retriable
   */
  static isRetriable(error: Error): boolean {
    if (error instanceof AppError) {
      return error.metadata.retriable;
    }

    // Check for network errors
    const message = error.message.toLowerCase();
    return (
      message.includes('timeout') ||
      message.includes('network') ||
      message.includes('econnrefused') ||
      message.includes('econnreset')
    );
  }
}