/**
 * Enhanced Error Types for Unreal MCP
 * Defines structured error codes and a strongly-typed Error class
 */

/**
 * Standardized error codes for machine-readable error handling
 */
export enum McpErrorCode {
  // Connection & Bridge
  UE_NOT_CONNECTED = 'UE_NOT_CONNECTED',
  BRIDGE_TIMEOUT = 'BRIDGE_TIMEOUT',
  BRIDGE_ERROR = 'BRIDGE_ERROR',
  CONNECTION_REFUSED = 'CONNECTION_REFUSED',

  // Validation
  INVALID_PARAMS = 'INVALID_PARAMS',
  INVALID_ASSET_PATH = 'INVALID_ASSET_PATH',
  MISSING_REQUIRED_PARAM = 'MISSING_REQUIRED_PARAM',

  // Unreal Engine Resources
  ASSET_NOT_FOUND = 'ASSET_NOT_FOUND',
  ACTOR_NOT_FOUND = 'ACTOR_NOT_FOUND',
  ASSET_ALREADY_EXISTS = 'ASSET_ALREADY_EXISTS',
  INVALID_ASSET_TYPE = 'INVALID_ASSET_TYPE',
  LEVEL_NOT_LOADED = 'LEVEL_NOT_LOADED',
  EDITOR_NOT_AVAILABLE = 'EDITOR_NOT_AVAILABLE',

  // Execution
  EXECUTION_FAILED = 'EXECUTION_FAILED',
  INTERNAL_ERROR = 'INTERNAL_ERROR',
  NOT_IMPLEMENTED = 'NOT_IMPLEMENTED',

  // Plugin/Feature Availability
  PLUGIN_UNAVAILABLE = 'PLUGIN_UNAVAILABLE',
  REQUIRES_UE_VERSION = 'REQUIRES_UE_VERSION',
  
  // System
  UNKNOWN_ERROR = 'UNKNOWN_ERROR'
}

/**
 * Options for constructing an McpError
 */
export interface McpErrorOptions {
  retryable?: boolean;
  recoverable?: boolean;
  suggestedFixes?: string[];
  details?: Record<string, unknown>;
  originalError?: unknown;
}

/**
 * Enhanced Error class with structured metadata
 */
export class McpError extends Error {
  public readonly code: McpErrorCode;
  public readonly retryable: boolean;
  public readonly recoverable: boolean;
  public readonly suggestedFixes: string[];
  public readonly details?: Record<string, unknown>;
  public readonly originalError?: unknown;

  constructor(code: McpErrorCode, message: string, options: McpErrorOptions = {}) {
    super(message);
    this.name = 'McpError';
    this.code = code;
    this.retryable = options.retryable ?? false;
    this.recoverable = options.recoverable ?? false;
    this.suggestedFixes = options.suggestedFixes ?? [];
    this.details = options.details;
    this.originalError = options.originalError;

    // Restore prototype chain for proper instanceof checks
    Object.setPrototypeOf(this, McpError.prototype);
  }

  /**
   * Convert to a plain object for JSON serialization
   */
  toJSON() {
    return {
      name: this.name,
      code: this.code,
      message: this.message,
      retryable: this.retryable,
      recoverable: this.recoverable,
      suggestedFixes: this.suggestedFixes,
      details: this.details
    };
  }
}

/**
 * Response indicating a required plugin is not available
 */
export interface PluginUnavailableResponse {
  success: false;
  error: 'PLUGIN_UNAVAILABLE';
  plugin: string;
}

/**
 * Response indicating a feature requires a newer UE version
 */
export interface FeatureUnavailableResponse {
  success: false;
  error: 'REQUIRES_UE_VERSION';
  feature: string;
  minVersion: string;
}

/**
 * Union type for all MCP error responses
 */
export type McpErrorResponse = PluginUnavailableResponse | FeatureUnavailableResponse;
