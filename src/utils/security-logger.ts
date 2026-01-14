/**
 * Centralized security audit logging for incident response.
 * All security events go through this class for consistent formatting.
 */
import { Logger } from './logger.js';

const securityLog = new Logger('Security', 'warn');

/**
 * Fields that must never appear in security logs
 */
const SENSITIVE_FIELDS = ['password', 'token', 'apiKey', 'secret', 'credential', 'key', 'auth'];

export class SecurityLogger {
  /**
   * Generic audit log for security events.
   * Automatically filters sensitive fields from details.
   */
  static auditLog(event: string, details: Record<string, unknown>): void {
    const safeDetails: Record<string, unknown> = {};
    for (const [key, value] of Object.entries(details)) {
      const lowerKey = key.toLowerCase();
      const isSensitive = SENSITIVE_FIELDS.some(field => lowerKey.includes(field));
      if (!isSensitive) {
        safeDetails[key] = value;
      }
    }
    securityLog.warn(`[AUDIT] ${event}`, safeDetails);
  }

  /**
   * Log path traversal attempt (directory traversal attack)
   */
  static pathTraversalAttempt(path: string, source?: string): void {
    this.auditLog('PATH_TRAVERSAL_ATTEMPT', { path, source });
  }

  /**
   * Log rate limit exceeded event
   */
  static rateLimitExceeded(identifier: string, endpoint?: string): void {
    this.auditLog('RATE_LIMIT_EXCEEDED', { identifier, endpoint });
  }

  /**
   * Log authentication failure
   */
  static authFailure(identifier: string, reason?: string): void {
    this.auditLog('AUTH_FAILURE', { identifier, reason });
  }
}
