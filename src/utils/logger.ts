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

  debug(...args: any[]) {
    if (!this.shouldLog('debug')) return;
    if (typeof console.debug === 'function') {
      console.debug(`[${this.scope}]`, ...args);
    } else {
      console.log(`[${this.scope}]`, ...args);
    }
  }
  info(...args: any[]) {
    if (!this.shouldLog('info')) return;
    if (typeof console.info === 'function') {
      console.info(`[${this.scope}]`, ...args);
    } else {
      console.log(`[${this.scope}]`, ...args);
    }
  }
  warn(...args: any[]) {
    if (!this.shouldLog('warn')) return;
    if (typeof console.warn === 'function') {
      console.warn(`[${this.scope}]`, ...args);
    } else {
      console.log(`[${this.scope}]`, ...args);
    }
  }
  error(...args: any[]) {
    if (this.shouldLog('error')) console.error(`[${this.scope}]`, ...args);
  }
}
