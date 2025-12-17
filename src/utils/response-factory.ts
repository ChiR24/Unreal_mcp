import { StandardActionResponse } from '../types/tool-interfaces.js';
import { cleanObject } from './safe-json.js';

export class ResponseFactory {
    /**
     * Create a standard success response
     */
    static success(data: any, message: string = 'Operation successful'): StandardActionResponse {
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
    static error(error: any, defaultMessage: string = 'Operation failed'): StandardActionResponse {
        const errorMessage = error instanceof Error ? error.message : String(error || defaultMessage);

        // Log the full error for debugging (internal logs) but return a clean message to the client
        console.error('[ResponseFactory] Error:', error);

        return {
            success: false,
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
            message: `Validation Error: ${message}`,
            data: null
        };
    }
}
