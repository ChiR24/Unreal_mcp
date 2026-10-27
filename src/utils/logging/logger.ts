export type LogLevel = 'debug' | 'info' | 'warn' | 'error';

const LOG_LEVEL_ORDER: Record<LogLevel, number> = {
  debug: 0,
  info: 1,
  warn: 2,
  error: 3
};

function normalizeLogLevel(value: unknown, fallback: LogLevel): LogLevel {
  const normalized = typeof value === 'string' ? value.trim().toLowerCase() : '';
  return Object.hasOwn(LOG_LEVEL_ORDER, normalized) ? normalized as LogLevel : fallback;
}

// Every level writes to stderr: stdout carries the MCP JSON-RPC stream.
export class Logger {
  private level: LogLevel;

  constructor(private scope: string, level: LogLevel = 'info') {
    this.level = normalizeLogLevel(process.env.LOG_LEVEL ?? process.env.LOGLEVEL, level);
  }

  isEnabled(level: LogLevel): boolean {
    return LOG_LEVEL_ORDER[level] >= LOG_LEVEL_ORDER[this.level];
  }

  private write(level: LogLevel, args: unknown[]) {
    if (this.isEnabled(level)) console.error(`[${this.scope}]`, ...args);
  }

  debug(...args: unknown[]) { this.write('debug', args); }
  info(...args: unknown[]) { this.write('info', args); }
  warn(...args: unknown[]) { this.write('warn', args); }
  error(...args: unknown[]) { this.write('error', args); }
}
