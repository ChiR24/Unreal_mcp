// Environment helpers — centralized parsing for boolean env flags used across the server.
export function allowPythonFallbackFromEnv(): boolean {
  const raw = String(process.env.MCP_ALLOW_PYTHON_FALLBACKS ?? '').toLowerCase().trim();
  return raw === '1' || raw === 'true';
}

export function isEnvTrue(envName: string): boolean {
  const raw = String(process.env[envName] ?? '').toLowerCase().trim();
  return raw === '1' || raw === 'true';
}
