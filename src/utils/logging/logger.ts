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

// One record per call: a control character inside a string (a line break in an upstream's server name) started a line
// that read as a diagnostic of its own. Tabs stay.
export function oneLine(text: string): string {
  return Array.from(text, (c) => {
    const code = c.charCodeAt(0);
    return (code < 0x20 && c !== '\t') || code === 0x7f ? `\\u${code.toString(16).padStart(4, '0')}` : c;
  }).join('');
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
    if (this.isEnabled(level)) console.error(`[${this.scope}]`, ...args.map((arg) => typeof arg === 'string' ? oneLine(arg) : arg));
  }

  debug(...args: unknown[]) { this.write('debug', args); }
  info(...args: unknown[]) { this.write('info', args); }
  warn(...args: unknown[]) { this.write('warn', args); }
  error(...args: unknown[]) { this.write('error', args); }
}
