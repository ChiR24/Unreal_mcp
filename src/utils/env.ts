export function isEnvTrue(envName: string): boolean {
  const raw = String(process.env[envName] ?? '').toLowerCase().trim();
  return raw === '1' || raw === 'true';
}

// DEPRECATED: Python support has been removed
export function allowPythonFallbackFromEnv(): boolean {
  // Always return false - Python fallback is no longer supported
  return false;
}
