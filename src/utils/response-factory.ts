import type { AutomationErrorDetail } from '../types/automation-responses.js';
import type { StandardActionResponse } from '../types/tool-interfaces.js';
import { cleanObject } from './safe-json.js';

/** Error response with custom code and optional extra fields */
export interface ErrorResponse {
  success: false;
  isError: true;
  error: string;
  message: string;
  [key: string]: unknown;
}

function isAutomationErrorDetail(value: unknown): value is AutomationErrorDetail {
    return !(value instanceof Error)
        && value !== null
        && typeof value === 'object'
        && !Array.isArray(value)
        && typeof (value as Record<string, unknown>).message === 'string';
}

export class ResponseFactory {
    /**
     * Create a standard success response
     */
    static success(data: unknown, message: string = 'Operation successful'): StandardActionResponse {
        return {
            success: true,
            message,
            data: cleanObject(data)
        };
    }

    /**
     * Create a standard error response
     * @param error The error object or message
     * @param defaultMessage Fallback message if error is not an Error object
     */
    static error(error: unknown, defaultMessage: string = 'Operation failed'): StandardActionResponse {
        if (isAutomationErrorDetail(error)) {
            const detail = cleanObject(error);
            return {
                success: false,
                isError: true,
                error: detail,
                message: detail.message,
                data: null
            };
        }

        const errorMessage = error instanceof Error ? error.message : String(error || defaultMessage);

        return {
            success: false,
            isError: true,  // CRITICAL FIX: Set isError: true so test runner recognizes this as an error
            message: errorMessage,
            data: null
        };
    }

    /**
     * Create a validation error response
     */
    static validationError(message: string): StandardActionResponse {
        return {
            success: false,
            isError: true,  // CRITICAL FIX: Set isError: true so test runner recognizes this as an error
            message: `Validation Error: ${message}`,
            data: null
        };
    }

    /**
     * Create an error response with a specific error code.
     * Use this for business logic errors that need specific codes like 'SECURITY_VIOLATION', 'NOT_FOUND', etc.
     *
     * @param code Error code (e.g., 'SECURITY_VIOLATION', 'NOT_FOUND', 'INVALID_ARGUMENT')
     * @param message Human-readable error message
     * @param extraFields Optional additional fields to include in the response
     */
    static errorWithCode(code: string, message: string, extraFields?: Record<string, unknown>): ErrorResponse {
        return cleanObject({
            success: false,
            isError: true,
            error: code,
            message,
            ...extraFields
        }) as ErrorResponse;
    }
}
