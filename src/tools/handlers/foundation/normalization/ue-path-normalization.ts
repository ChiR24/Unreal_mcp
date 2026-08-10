import { getAdditionalPathPrefixes } from '../../../../config.js';

/**
 * Map the `/Content` (or bare `Content/`) alias onto `/Game`, and nothing else.
 *
 * Split out so callers that must NOT root-anchor can still route the alias
 * through one implementation — the repo rule is that `/Content` is mapped in
 * exactly one place. Boundary-aware: `/ContentOther` is a different folder and
 * is left alone.
 */
export function mapContentAlias(rawValue: string): string {
  const normalized = rawValue.replace(/\\/g, '/');
  if (normalized.startsWith('/Content/') || normalized === '/Content') {
    return `/Game${normalized.slice('/Content'.length)}`;
  }
  if (normalized.startsWith('Content/')) {
    return `/Game/${normalized.slice('Content/'.length)}`;
  }
  return normalized;
}

/**
 * Canonicalize ONE UE content path: map the `/Content` alias onto `/Game` and
 * root-anchor a bare path. Exported (it was a closure inside
 * `normalizePathFields`) so every caller routes through this one implementation
 * rather than re-deriving the alias. Honors `MCP_ADDITIONAL_PATH_PREFIXES`.
 */
export function normalizeUePathValue(rawValue: string): string {
  const rootAliases = [
    'Game',
    'Engine',
    'Script',
    'Temp',
    'Niagara',
    ...getAdditionalPathPrefixes().map(prefix => prefix.replace(/^\//, '').replace(/\/$/, ''))
  ];
  let normalized = mapContentAlias(rawValue);
  if (rootAliases.some(root => normalized.startsWith(`${root}/`))) {
    normalized = `/${normalized}`;
  }
  if (!normalized.startsWith('/')) {
    normalized = `/Game/${normalized}`;
  }
  return normalized;
}

export function normalizePathFields(
  args: Record<string, unknown>,
  pathFields: readonly string[]
): Record<string, unknown> {
  const result = { ...args };

  for (const field of pathFields) {
    const value = result[field];
    if (Array.isArray(value)) {
      result[field] = value.map(entry =>
        typeof entry === 'string' && entry.length > 0 ? normalizeUePathValue(entry) : entry
      );
      continue;
    }

    if (typeof value === 'object' && value !== null) {
      result[field] = Object.fromEntries(
        Object.entries(value).map(([key, entry]) => [
          key,
          typeof entry === 'string' && entry.length > 0
            ? normalizeUePathValue(entry)
            : entry
        ])
      );
      continue;
    }

    if (typeof value !== 'string' || value.length === 0) {
      continue;
    }

    result[field] = normalizeUePathValue(value);
  }

  return result;
}
