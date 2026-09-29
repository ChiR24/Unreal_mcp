import * as path from 'node:path';

import {
  getConfiguredContentRoots,
  getEditorContentRoots,
} from '../../../../utils/paths/path-security.js';

function hasParentDirectorySegment(value: string): boolean {
  return value.replace(/\\/g, '/').split('/').some(segment => segment === '..');
}

function normalizeKey(key: string): string {
  return key.toLowerCase();
}

/**
 * The action the plugin will run: `subAction` when it is a non-empty string,
 * otherwise `action`. The same rule as the plugin's pre-queue gate
 * (McpConnectionManagerAuthority.cpp) and bridge-request-dispatcher.ts, so a
 * key is classified for the action that actually executes.
 */
function effectiveAction(args: Record<string, unknown>): string {
  if (typeof args.subAction === 'string' && args.subAction !== '') {
    return args.subAction.toLowerCase();
  }
  return typeof args.action === 'string' ? args.action.toLowerCase() : '';
}

// Keys whose value the plugin opens as a FILE ON DISK: each reaches
// McpResolveProjectFilePath (or, for a .t3d exportPath, the T3D exporter).
// A file never lives under a content mount, so these keys keep the configured
// roots plus /Saved and /tmp, and never get the editor-reported mounts.
const FILE_KEYS: ReadonlySet<string> = new Set([
  'filepath',
  'filepaths',
  'mediapath',
  'outputdirectory',
  'outputpath',
  'heightmappath', // import_heightmap (EnvironmentHandlersLandscapeHeightmap.cpp)
  'externalpath', // export_metahuman dna (MetaHumanHandlersExport.cpp)
]);

// Keys that are a file only for some actions, and a content path otherwise.
const FILE_KEYS_BY_ACTION: Readonly<Record<string, ReadonlySet<string>>> = {
  // import: AssetWorkflowImportDuplicate.cpp; import_struct: AssetWorkflowStructsImport.cpp.
  // duplicate/move/rename/import_level read sourcePath as a package path.
  sourcepath: new Set(['import', 'import_struct']),
  // The screenshot folder (McpScreenshotResample.cpp) and export_heightmap's output file.
  path: new Set(['screenshot', 'take_screenshot', 'export_heightmap']),
};

function isLocalFilesystemKey(key: string, action: string, value: string): boolean {
  const normalized = normalizeKey(key);
  if (normalized === 'exportpath') {
    // export_level writes a file only for a .t3d exportPath and copies the level
    // package otherwise (LevelHandlersExport.cpp); inspect's export always writes a file.
    return action !== 'export_level' || value.toLowerCase().endsWith('.t3d');
  }
  const scoped = FILE_KEYS_BY_ACTION[normalized];
  if (scoped) {
    return scoped.has(action);
  }
  return FILE_KEYS.has(normalized);
}

function isPathLikeKey(key: string): boolean {
  const normalized = normalizeKey(key);
  return normalized.includes('path') ||
    normalized.endsWith('directory') ||
    normalized.endsWith('directories');
}

function isUnderRoot(value: string, root: string): boolean {
  return value === root || value.startsWith(`${root}/`);
}

function isAllowedAbsolutePath(key: string, value: string, args: Record<string, unknown>): boolean {
  // A leading pair of separators in any mix (`//host/share`, `/\host\share`) is a
  // UNC path on Windows, not a content address. The normalization below would
  // collapse it to `/host/share`, so it is refused on the raw value. The semantic
  // path layer already rejects a double slash the same way.
  if (/^[\\/]{2}/u.test(value)) {
    return false;
  }

  // Normalize (collapse `.`, `..`, repeated slashes) so the root check sees the
  // canonical form, matching the C++ side's FPaths::CollapseRelativeDirectories.
  // The raw lowercased value is also accepted for an already-canonical path, while
  // `/Game/../Engine/foo` (which normalizes to `/Engine/foo`) still fails the
  // hasParentDirectorySegment check in validateStringSecurity.
  const slashed = value.replace(/\\/g, '/');
  const normalizedForRootCheck = path.posix.normalize(slashed).toLowerCase();
  const lowerValue = value.toLowerCase();
  const action = effectiveAction(args);
  const normalizedKey = normalizeKey(key);
  const isSnapshotPath =
    (normalizedKey === 'path' || normalizedKey === 'outputpath') &&
    (action === 'export_snapshot' || action === 'import_snapshot');
  const isFileKey = isLocalFilesystemKey(key, action, value);

  // The configured roots (UE_CONTENT_ROOTS + MCP_ADDITIONAL_PATH_PREFIXES) apply to
  // every key. /tmp is the canonical POSIX temp location for a file key; the plugin
  // re-checks containment (McpResolveProjectFilePath) for every file it opens.
  const staticRoots = [
    ...getConfiguredContentRoots().map(root => root.toLowerCase()),
    ...(isSnapshotPath || isFileKey ? ['/saved'] : []),
    ...(isFileKey ? ['/tmp'] : []),
  ];
  if (staticRoots.some(root => isUnderRoot(normalizedForRootCheck, root) || isUnderRoot(lowerValue, root))) {
    return true;
  }

  // A content path may also sit under a mount the connected editor reported.
  // Only a value that normalization leaves unchanged is matched against those:
  // a real package path never contains `/./` or `//`, and `/./Name/...` must not
  // reach a mount by way of normalization.
  if (isFileKey || isSnapshotPath || path.posix.normalize(slashed) !== slashed) {
    return false;
  }
  const lowerSlashed = slashed.toLowerCase();
  return getEditorContentRoots().some(root => isUnderRoot(lowerSlashed, root.toLowerCase()));
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
    if (isLocalFilesystemKey(key, effectiveAction(args), value)) {
      return `Security violation: '${key}' uses unauthorized absolute path. A file path must be under /Game/, /Engine/, /Script/, /Temp/, /Niagara/, /Saved/ or /tmp/, or a prefix listed in MCP_ADDITIONAL_PATH_PREFIXES.`;
    }
    return `Security violation: '${key}' uses unauthorized absolute path. Only /Game/, /Engine/, /Script/, /Temp/, /Niagara/, the content mounts the connected editor reports, and MCP_ADDITIONAL_PATH_PREFIXES are allowed; a path may not start with two separators.`;
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
