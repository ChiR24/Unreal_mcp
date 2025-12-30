export type LogLevel = 'debug' | 'info' | 'warn' | 'error';

export class Logger {
  private level: LogLevel;

  constructor(private scope: string, level: LogLevel = 'info') {
    const envLevel = (process.env.LOG_LEVEL || process.env.LOGLEVEL || level).toString().toLowerCase();
    this.level = (['debug', 'info', 'warn', 'error'] as LogLevel[]).includes(envLevel as LogLevel)
      ? (envLevel as LogLevel)
      : 'info';
  }

  private shouldLog(level: LogLevel) {
    const order: LogLevel[] = ['debug', 'info', 'warn', 'error'];
    return order.indexOf(level) >= order.indexOf(this.level);
  }

  debug(...args: unknown[]) {
    if (!this.shouldLog('debug')) return;
    // Write to stderr to avoid corrupting MCP stdout stream
    console.error(`[${this.scope}]`, ...args);
  }
  info(...args: unknown[]) {
    if (!this.shouldLog('info')) return;
    // Write to stderr to avoid corrupting MCP stdout stream
    console.error(`[${this.scope}]`, ...args);
  }
  warn(...args: unknown[]) {
    if (!this.shouldLog('warn')) return;
    console.warn(`[${this.scope}]`, ...args);
  }
  error(...args: unknown[]) {
    if (this.shouldLog('error')) console.error(`[${this.scope}]`, ...args);
  }
}
