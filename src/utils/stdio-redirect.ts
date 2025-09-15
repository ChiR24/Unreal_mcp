export function routeStdoutLogsToStderr(): void {
  // Enable by default. Allow opt-out with LOG_TO_STDERR=false or JSON_STDOUT_MODE=false
  const flagRaw = String(process.env.LOG_TO_STDERR ?? process.env.JSON_STDOUT_MODE ?? 'true').toLowerCase();
  const enabled = !(flagRaw === 'false' || flagRaw === '0' || flagRaw === 'off' || flagRaw === 'no');
  if (!enabled) return;

  try {
    const toErr = console.error.bind(console) as (...args: any[]) => void;
    // Route common stdout channels to stderr to keep stdout JSON-only for MCP
    console.log = toErr as any;
    console.info = toErr as any;
    console.debug = toErr as any;
    // Be explicit with trace as well
    console.trace = toErr as any;
  } catch {
    // If overriding fails for any reason, just continue silently.
  }
}
