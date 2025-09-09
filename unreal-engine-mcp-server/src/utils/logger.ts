export type LogLevel = 'debug' | 'info' | 'warn' | 'error';

export class Logger {
  constructor(private scope: string, private level: LogLevel = 'info') {}

  private shouldLog(level: LogLevel) {
    const order: LogLevel[] = ['debug', 'info', 'warn', 'error'];
    return order.indexOf(level) >= order.indexOf(this.level);
  }

  debug(...args: any[]) {
    if (this.shouldLog('debug')) console.debug(`[${this.scope}]`, ...args);
  }
  info(...args: any[]) {
    if (this.shouldLog('info')) console.info(`[${this.scope}]`, ...args);
  }
  warn(...args: any[]) {
    if (this.shouldLog('warn')) console.warn(`[${this.scope}]`, ...args);
  }
  error(...args: any[]) {
    if (this.shouldLog('error')) console.error(`[${this.scope}]`, ...args);
  }
}
