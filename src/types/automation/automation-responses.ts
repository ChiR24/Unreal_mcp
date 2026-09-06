/**
 * Common type definitions for automation bridge responses
 * Used to provide type safety for sendAutomationRequest calls
 */

export interface AutomationErrorDetail {
    message: string;
    code?: string;
    [key: string]: unknown;
}

/**
 * Base response structure from the Automation Bridge
 * Most responses follow this pattern with optional additional fields
 */
export interface AutomationResponse {
    success: boolean;
    message?: string;
    error?: string | AutomationErrorDetail;
    /** Optional error code for business logic errors (e.g., 'SECURITY_VIOLATION', 'NOT_FOUND') */
    errorCode?: string;
    result?: unknown;
    assetPath?: string;
    // Common additional fields
    warnings?: string[];
    details?: unknown;
    data?: unknown;
    [key: string]: unknown;
}
