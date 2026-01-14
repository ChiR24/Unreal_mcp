export type LogLevel = 'debug' | 'info' | 'warn' | 'error';
export type LogFormat = 'text' | 'json';

interface JsonLogEntry {
  timestamp: string;
  level: LogLevel;
  scope: string;
  message: string;
  requestId?: string;
  data?: unknown;
}

/* eslint-disable no-console */
export class Logger {
  private level: LogLevel;
  private format: LogFormat;
  private requestId?: string;

  constructor(private scope: string, level: LogLevel = 'info') {
    const envLevel = (process.env.LOG_LEVEL || process.env.LOGLEVEL || level).toString().toLowerCase();
    this.level = (['debug', 'info', 'warn', 'error'] as LogLevel[]).includes(envLevel as LogLevel)
      ? (envLevel as LogLevel)
      : 'info';
    
    const envFormat = (process.env.LOG_FORMAT || 'text').toString().toLowerCase();
    this.format = envFormat === 'json' ? 'json' : 'text';
  }

  /**
   * Set a request ID for correlation. All subsequent log messages will include this ID.
   */
  setRequestId(requestId: string | undefined): void {
    this.requestId = requestId;
  }

  /**
   * Create a child logger with a specific request ID for correlation.
   */
  withRequestId(requestId: string): Logger {
    const child = new Logger(this.scope, this.level);
    child.format = this.format;
    child.requestId = requestId;
    return child;
  }

  private shouldLog(level: LogLevel) {
    const order: LogLevel[] = ['debug', 'info', 'warn', 'error'];
    return order.indexOf(level) >= order.indexOf(this.level);
  }

  isDebugEnabled(): boolean {
    return this.shouldLog('debug');
  }

  private formatMessage(level: LogLevel, args: unknown[]): string {
    if (this.format === 'json') {
      const entry: JsonLogEntry = {
        timestamp: new Date().toISOString(),
        level,
        scope: this.scope,
        message: args.map(a => typeof a === 'string' ? a : JSON.stringify(a)).join(' '),
        requestId: this.requestId
      };
      // If there's additional data beyond the message string, include it
      if (args.length > 1 && typeof args[0] === 'string') {
        entry.message = args[0];
        entry.data = args.slice(1);
      }
      return JSON.stringify(entry);
    }
    // Text format
    const prefix = this.requestId ? `[${this.scope}] [${this.requestId}]` : `[${this.scope}]`;
    return `${prefix} ${args.map(a => typeof a === 'string' ? a : JSON.stringify(a)).join(' ')}`;
  }

  debug(...args: unknown[]) {
    if (!this.shouldLog('debug')) return;
    // Write to stderr to avoid corrupting MCP stdout stream
    console.error(this.formatMessage('debug', args));
  }
  info(...args: unknown[]) {
    if (!this.shouldLog('info')) return;
    // Write to stderr to avoid corrupting MCP stdout stream
    console.error(this.formatMessage('info', args));
  }
  warn(...args: unknown[]) {
    if (!this.shouldLog('warn')) return;
    console.warn(this.formatMessage('warn', args));
  }
  error(...args: unknown[]) {
    if (this.shouldLog('error')) console.error(this.formatMessage('error', args));
  }
}
