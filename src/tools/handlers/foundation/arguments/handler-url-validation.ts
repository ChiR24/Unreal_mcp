export function isUrlArgumentKey(key: string): boolean {
  const normalized = key.toLowerCase();
  return normalized === 'url' ||
    normalized === 'urls' ||
    normalized === 'streamurl';
}

export function validateUrlArgument(
  key: string,
  value: string,
): string | undefined {
  const trimmed = value.trim();
  if (trimmed === '') {
    // Empty URL is treated as "no value provided" rather than an error.
    // Callers should treat undefined as "no opinion" and skip the validation
    // path; any downstream handler that requires a URL will surface its own
    // "missing required" error before reaching the bridge.
    return undefined;
  }

  // Network media URLs are unconditionally disabled because redirect
  // destinations and resolved addresses cannot be pinned by the media
  // backend.  All HTTP/HTTPS URLs — including loopback — are rejected
  // to prevent server-side request forgery and redirect-chain attacks.
  // Use filePath/mediaPath with an allowed filesystem root instead.
  let parsed: URL;
  try {
    parsed = new URL(trimmed);
  } catch (error) {
    if (error instanceof TypeError) {
      return 'Security violation: \'' + key + '\' is not a valid absolute URL. ' +
        'Network media URLs are disabled — use filePath or mediaPath with an allowed filesystem root instead.';
    }
    throw error;
  }
  const scheme = parsed.protocol.slice(0, -1).toLowerCase();
  if (scheme === 'file') {
    return `Security violation: '${key}' uses a file URL. Use filePath/mediaPath with an allowed filesystem root instead.`;
  }
  return `Security violation: '${key}' uses a network media URL, but network media URLs are disabled because redirect destinations and resolved addresses cannot be pinned by the media backend. Use filePath or mediaPath with an allowed filesystem root instead.`;
}
