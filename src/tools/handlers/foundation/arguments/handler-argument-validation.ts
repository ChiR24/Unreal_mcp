import * as path from 'node:path';

import { getAdditionalPathPrefixes } from '../../../../config.js';
import { UE_CONTENT_ROOTS } from '../../../../utils/paths/content-path-policy.js';

function hasParentDirectorySegment(value: string): boolean {
  return value.replace(/\\/g, '/').split('/').some(segment => segment === '..');
}

function normalizeKey(key: string): string {
  return key.toLowerCase();
}

function isLocalFilesystemKey(key: string): boolean {
  const normalized = normalizeKey(key);
  return normalized === 'filepath' ||
    normalized === 'filepaths' ||
    normalized === 'mediapath' ||
    normalized === 'outputdirectory' ||
    normalized === 'outputpath';
}

function isPathLikeKey(key: string): boolean {
  const normalized = normalizeKey(key);
  return normalized.includes('path') ||
    normalized.endsWith('directory') ||
    normalized.endsWith('directories');
}

function isAllowedAbsolutePath(key: string, value: string, args: Record<string, unknown>): boolean {
  // C2 fix: normalize the path (collapse `.`, `..`, repeated slashes) so the
  // root check sees the canonical form. The hasParentDirectorySegment check
  // in validateStringSecurity already catches `..` segments, but
  // path.posix.normalize() also handles `./`, double slashes, and other
  // normalization edge cases. This matches the C++ side's
  // FPaths::CollapseRelativeDirectories call.
  const lowerValue = value.toLowerCase();
  const normalizedForRootCheck = path.posix
    .normalize(value.replace(/\\/g, '/'))
    .toLowerCase();
  const additional = getAdditionalPathPrefixes();
  const action = typeof args.action === 'string' ? args.action.toLowerCase() : '';
  const normalizedKey = normalizeKey(key);
  const isSnapshotPath =
    (normalizedKey === 'path' || normalizedKey === 'outputpath') &&
    (action === 'export_snapshot' || action === 'import_snapshot');
  // /tmp is allowed for filesystem-output keys (filepath, filepaths, mediapath,
  // outputdirectory, outputpath) regardless of action. The intent is that
  // these keys describe files on disk, and /tmp is the canonical temp location
  // on POSIX hosts. The C++ side (McpSequencePathSecurity::ValidateLocalPath)
  // is stricter: it only allows /Saved/ and /Content/. Callers that need
  // /tmp/ for render output must therefore use it through the filesystem
  // surface, not the asset surface.
  const localRoots = isLocalFilesystemKey(key) ? ['/tmp'] : [];
  // Derived from content-path-policy's UE_CONTENT_ROOTS so this gate and every
  // other path surface can never disagree about which roots are content roots.
  const allowedRoots = [...UE_CONTENT_ROOTS.map(root => root.toLowerCase()),
    ...(isSnapshotPath || isLocalFilesystemKey(key) ? ['/saved'] : []),
    ...localRoots,
    ...additional.map(prefix => prefix.replace(/\/$/, '').toLowerCase())];

  return allowedRoots.some(root => {
    const candidate = normalizedForRootCheck.startsWith(`${root}/`) ||
      normalizedForRootCheck === root;
    // Also accept the raw (non-normalized) value if the lowercased form
    // matches. This preserves the original behavior for already-canonical
    // paths while still rejecting e.g. `/Game/../Engine/foo` (which
    // normalizes to `/Engine/foo`, not under /Game).
    const rawMatches = lowerValue === root || lowerValue.startsWith(`${root}/`);
    return candidate || rawMatches;
  });
}

function validateStringSecurity(
  args: Record<string, unknown>,
  key: string,
  value: string
): string | undefined {
  const blockedPathPatterns = [
    '/etc/',
    '\\Windows\\',
    '\\Program Files',
  ];
  const lowerValue = value.toLowerCase();

  if (hasParentDirectorySegment(value)) {
    return `Security violation: '${key}' contains blocked path pattern. Path traversal is not allowed.`;
  }

  for (const pattern of blockedPathPatterns) {
    if (value.includes(pattern) || lowerValue.includes(pattern.toLowerCase())) {
      return `Security violation: '${key}' contains blocked path pattern. Path traversal is not allowed.`;
    }
  }

  if (isPathLikeKey(key) && value.startsWith('/') && !isAllowedAbsolutePath(key, value, args)) {
    const savedNote = isLocalFilesystemKey(key) ? ', /Saved/, /tmp/' : '';
    return `Security violation: '${key}' uses unauthorized absolute path. Only /Game/, /Engine/, /Script/, /Temp/${savedNote}, /Niagara/ paths are allowed by default. Set MCP_ADDITIONAL_PATH_PREFIXES to whitelist custom plugin content mount points.`;
  }

  return undefined;
}

function ensureArgsPresent(args: unknown): asserts args is Record<string, unknown> {
  if (args === null || args === undefined) {
    throw new Error('Invalid arguments: null or undefined');
  }
}

// Maximum recursion depth for nested-object validation. Beyond this depth,
// the value is treated as opaque (defense against pathological nesting that
// could blow the stack or hide deep payloads). Matches the value used by
// the log-redaction depth cap.
const MAX_VALIDATION_DEPTH = 12;

// Depth-limited recursive validator. Handles the same cases as the previous
// loop (top-level strings, arrays of strings, objects of strings) but also
// recurses into nested objects and arrays-of-objects so that deeply nested
// payloads like { a: { b: { path: '/etc/passwd' } } } are inspected.
function validateValue(
  args: Record<string, unknown>,
  key: string,
  value: unknown,
  depth: number
): string | undefined {
  if (typeof value === 'string') {
    return validateStringSecurity(args, key, value);
  }
  if (Array.isArray(value)) {
    if (depth >= MAX_VALIDATION_DEPTH) {
      return undefined;
    }
    for (const entry of value) {
      if (typeof entry === 'string') {
        const error = validateStringSecurity(args, key, entry);
        if (error) {
          return error;
        }
        continue;
      }
      if (entry !== null && typeof entry === 'object') {
        for (const [entryKey, entryValue] of Object.entries(entry)) {
          const error = validateValue(args, entryKey, entryValue, depth + 1);
          if (error) {
            return error;
          }
        }
      }
    }
    return undefined;
  }
  if (value !== null && typeof value === 'object') {
    if (depth >= MAX_VALIDATION_DEPTH) {
      return undefined;
    }
    for (const [entryKey, entry] of Object.entries(value)) {
      const error = validateValue(args, entryKey, entry, depth + 1);
      if (error) {
        return error;
      }
    }
    return undefined;
  }
  return undefined;
}

function validateSecurityPatterns(args: Record<string, unknown>): string | undefined {
  for (const [key, value] of Object.entries(args)) {
    const error = validateValue(args, key, value, 0);
    if (error) {
      return error;
    }
  }
  return undefined;
}

export function validateArgsSecurity(args: Record<string, unknown>): void {
  ensureArgsPresent(args);
  const securityError = validateSecurityPatterns(args);
  if (securityError) {
    throw new Error(securityError);
  }
}
