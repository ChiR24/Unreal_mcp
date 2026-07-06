import { getAdditionalPathPrefixes } from '../../../../config.js';

export function normalizePathFields(
  args: Record<string, unknown>,
  pathFields: readonly string[]
): Record<string, unknown> {
  const result = { ...args };
  const rootAliases = [
    'Game',
    'Engine',
    'Script',
    'Temp',
    'Niagara',
    ...getAdditionalPathPrefixes().map(prefix => prefix.replace(/^\//, '').replace(/\/$/, ''))
  ];

  const normalizePathValue = (rawValue: string): string => {
    let normalized = rawValue.replace(/\\/g, '/');
    if (normalized.startsWith('/Content/')) {
      normalized = `/Game/${normalized.slice('/Content/'.length)}`;
    } else if (normalized.startsWith('Content/')) {
      normalized = `/Game/${normalized.slice('Content/'.length)}`;
    } else if (rootAliases.some(root => normalized.startsWith(`${root}/`))) {
      normalized = `/${normalized}`;
    }
    if (!normalized.startsWith('/')) {
      normalized = `/Game/${normalized}`;
    }
    return normalized;
  };

  for (const field of pathFields) {
    const value = result[field];
    if (Array.isArray(value)) {
      result[field] = value.map(entry =>
        typeof entry === 'string' && entry.length > 0 ? normalizePathValue(entry) : entry
      );
      continue;
    }

    if (typeof value === 'object' && value !== null) {
      result[field] = Object.fromEntries(
        Object.entries(value).map(([key, entry]) => [
          key,
          typeof entry === 'string' && entry.length > 0
            ? normalizePathValue(entry)
            : entry
        ])
      );
      continue;
    }

    if (typeof value !== 'string' || value.length === 0) {
      continue;
    }

    result[field] = normalizePathValue(value);
  }

  return result;
}
