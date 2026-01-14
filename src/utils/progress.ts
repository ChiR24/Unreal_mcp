import type { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { Logger } from './logger.js';

/**
 * Progress notification level for MCP logging
 */
export type ProgressLevel = 'debug' | 'info' | 'notice' | 'warning' | 'error';

/**
 * Options for progress notifications
 */
export interface ProgressOptions {
  /** Unique identifier for the progress stream (e.g., request ID) */
  progressToken?: string;
  /** Log level for the notification */
  level?: ProgressLevel;
  /** Additional data to include in the notification */
  data?: Record<string, unknown>;
}

/**
 * Progress notification helper that uses MCP SDK's server.sendLoggingMessage()
 * or server.notification() for progress updates.
 */
export class ProgressHelper {
  private log = new Logger('ProgressHelper');
  
  constructor(private server: Server) {}

  /**
   * Send a progress notification to the MCP client.
   * Uses notifications/message for logging messages per MCP spec.
   * 
   * @param message - The progress message to send
   * @param options - Optional configuration for the notification
   */
  async sendProgress(message: string, options: ProgressOptions = {}): Promise<void> {
    const { progressToken, level = 'info', data } = options;
    
    try {
      // Use MCP logging notification pattern
      await this.server.notification({
        method: 'notifications/message',
        params: {
          level,
          logger: progressToken ?? 'progress',
          data: {
            message,
            timestamp: new Date().toISOString(),
            ...data
          }
        }
      });
    } catch (err) {
      // Log locally if notification fails (client may not support it)
      this.log.debug(`Progress notification failed (client may not support): ${message}`, err);
    }
  }

  /**
   * Send a progress update with percentage completion.
   * 
   * @param message - The progress message
   * @param current - Current progress value
   * @param total - Total progress value (for percentage calculation)
   * @param options - Optional configuration
   */
  async sendProgressWithPercent(
    message: string,
    current: number,
    total: number,
    options: ProgressOptions = {}
  ): Promise<void> {
    const percent = total > 0 ? Math.round((current / total) * 100) : 0;
    await this.sendProgress(`[${percent}%] ${message}`, {
      ...options,
      data: {
        ...options.data,
        current,
        total,
        percent
      }
    });
  }

  /**
   * Create a scoped progress sender for a specific operation.
   * Useful for tracking progress across multiple steps.
   * 
   * @param operationName - Name of the operation for context
   * @param progressToken - Optional token for correlation
   */
  createScopedProgress(operationName: string, progressToken?: string): ScopedProgress {
    return new ScopedProgress(this, operationName, progressToken);
  }
}

/**
 * Scoped progress helper for tracking multi-step operations.
 */
export class ScopedProgress {
  private stepIndex = 0;

  constructor(
    private helper: ProgressHelper,
    private operationName: string,
    private progressToken?: string
  ) {}

  /**
   * Report the start of the operation.
   */
  async start(message?: string): Promise<void> {
    await this.helper.sendProgress(
      message ?? `Starting ${this.operationName}`,
      { progressToken: this.progressToken, level: 'info' }
    );
  }

  /**
   * Report a step in the operation.
   */
  async step(message: string): Promise<void> {
    this.stepIndex++;
    await this.helper.sendProgress(
      `[Step ${this.stepIndex}] ${message}`,
      { progressToken: this.progressToken, level: 'info' }
    );
  }

  /**
   * Report completion of the operation.
   */
  async complete(message?: string): Promise<void> {
    await this.helper.sendProgress(
      message ?? `Completed ${this.operationName}`,
      { progressToken: this.progressToken, level: 'info' }
    );
  }

  /**
   * Report an error in the operation.
   */
  async error(message: string): Promise<void> {
    await this.helper.sendProgress(
      `Error in ${this.operationName}: ${message}`,
      { progressToken: this.progressToken, level: 'error' }
    );
  }
}

/**
 * Factory function to create a progress helper from an MCP server instance.
 */
export function createProgressHelper(server: Server): ProgressHelper {
  return new ProgressHelper(server);
}
