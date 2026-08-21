import * as path from 'node:path';

import { getAdditionalPathPrefixes } from '../../../../config.js';
import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import {
  isUrlArgumentKey,
  validateUrlArgument,
} from './handler-url-validation.js';

function hasParentDirectorySegment(value: string): boolean {
  return value.replace(/\\/g, '/').split('/').some(segment => segment === '..');
}

function normalizeKey(key: string): string {
  return key.toLowerCase();
}

// Keys whose value names a real file/directory ON DISK rather than a virtual content path. Getting this list
// right matters in BOTH directions: too narrow and the "filesystem keys stay strict" claim below is false;
// too wide and plugin-mount asset paths get re-blocked, which is the very bug 0060 exists to fix.
//
// `sourcePath` is the trap: it is an on-disk file for manage_asset.import, but a VIRTUAL asset path for
// rename/duplicate/move -- where it is literally declared as an alias of `assetPath`
// (src/tools/handlers/asset/asset-basic-actions.ts) and validated C++-side by SanitizeProjectRelativePath
// against the engine mount table. Listing it unconditionally made the SAME logical call succeed or fail
// depending on which alias the model happened to pick. So the discriminator is the (key, action) pair, not
// the key name.
//
// Deliberately NOT listed: `destinationPath` and `directory`. Both are virtual — destinationPath goes through
// SanitizeProjectRelativePath, and `directory` defaults to /Game and normalises /Content -> /Game.
const ACTION_SCOPED_FS_KEYS: Record<string, ReadonlySet<string>> = {
  sourcepath: new Set(['import', 'import_asset']),
};

function isLocalFilesystemKey(key: string, action = ''): boolean {
  const normalized = normalizeKey(key);
  const scoped = ACTION_SCOPED_FS_KEYS[normalized];
  if (scoped) return scoped.has(action.toLowerCase());
  return normalized === 'filepath' ||
    normalized === 'filepaths' ||
    normalized === 'mediapath' ||
    normalized === 'outputdirectory' ||
    normalized === 'outputpath' ||
    normalized === 'heightmappath' ||
    normalized === 'snapshotpath' ||
    normalized === 'tracepath';
}

function isPathLikeKey(key: string): boolean {
  const normalized = normalizeKey(key);
  return normalized.includes('path') ||
    normalized.endsWith('directory') ||
    normalized.endsWith('directories');
}

// A UE content root is a single leading path segment (letters, digits,
// underscore, hyphen) — e.g. `/Game/`, `/Paper2D/`, `/Pedestrian_System/`.
// Anything carrying a dot, space, or other punctuation is not a mount name.
const CONTENT_MOUNT_ROOT_PATTERN = /^\/[a-z0-9_][a-z0-9_-]*(\/|$)/u;

// Roots that are shaped like a mount name but name a real directory on disk
// (project layout or OS). A content mount never uses these, so they stay
// rejected even for virtual-asset keys — `path` in particular is ambiguous and
// is used as a filesystem path by some actions.
const NON_MOUNT_ROOTS = new Set([
  'saved', 'config', 'binaries', 'intermediate', 'build', 'source', 'content',
  'plugins', 'logs', 'derivedatacache', 'tmp', 'etc', 'usr', 'var', 'bin',
  'dev', 'proc', 'sys', 'home', 'root', 'users', 'windows', 'boot', 'opt',
  // Defence in depth, named in review: '/mnt/c/Windows/...' is the classic WSL shape and would otherwise
  // pass the shape check.
  'mnt', 'media', 'volumes', 'library', 'private', 'programdata', 'appdata',
  'documents', 'srv', 'run', 'lib', 'sbin',
]);

/**
 * Whether `value` is shaped like an Unreal *virtual* content path rooted at a
 * plugin/game-feature mount (`/MyPlugin/Blueprints/BP_Foo`).
 *
 * These are package paths, not filesystem paths: the engine resolves them
 * through its registered mount table and never opens them as files. The
 * authority on which roots exist is the engine itself — the plugin's
 * SanitizeProjectRelativePath() defers to
 * `FPackageName::IsValidLongPackageName`, which covers every plugin content
 * mount. A static allowlist here cannot know those names (they differ per
 * project), so mirroring one made legitimate plugin content unreachable and
 * reported it as a SECURITY_VIOLATION. Unknown roots still fail — as an
 * ordinary NOT_FOUND from the engine, which is the accurate answer.
 */
function looksLikeContentMountRoot(normalizedValue: string): boolean {
  if (!CONTENT_MOUNT_ROOT_PATTERN.test(normalizedValue)) {
    return false;
  }
  const root = normalizedValue.split('/')[1] ?? '';
  return !NON_MOUNT_ROOTS.has(root);
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
  const localRoots = isLocalFilesystemKey(key, action) ? ['/tmp'] : [];
  const allowedRoots = ['/game', '/engine', '/script', '/temp', '/niagara',
    ...(isSnapshotPath || isLocalFilesystemKey(key, action) ? ['/saved'] : []),
    ...localRoots,
    ...additional.map(prefix => prefix.replace(/\/$/, '').toLowerCase())];

  const matchesStaticRoot = allowedRoots.some(root => {
    const candidate = normalizedForRootCheck.startsWith(`${root}/`) ||
      normalizedForRootCheck === root;
    // Also accept the raw (non-normalized) value if the lowercased form
    // matches. This preserves the original behavior for already-canonical
    // paths while still rejecting e.g. `/Game/../Engine/foo` (which
    // normalizes to `/Engine/foo`, not under /Game).
    const rawMatches = lowerValue === root || lowerValue.startsWith(`${root}/`);
    return candidate || rawMatches;
  });

  if (matchesStaticRoot) {
    return true;
  }

  // Virtual asset paths defer to the engine's mount table (see looksLikeContentMountRoot). Keys that name a
  // real file on disk (isLocalFilesystemKey) and snapshot paths keep the strict allowlist, because those
  // values *are* used to open files. Note this layer is now a SHAPE check for asset paths -- the actual
  // containment guarantee lives in the plugin, which is where it belongs.
  if (!isLocalFilesystemKey(key, action) && !isSnapshotPath) {
    return looksLikeContentMountRoot(normalizedForRootCheck);
  }

  return false;
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

  if (isUrlArgumentKey(key)) {
    return validateUrlArgument(key, value);
  }

  if (isPathLikeKey(key) && value.startsWith('/') && !isAllowedAbsolutePath(key, value, args)) {
    const actionForKey = typeof args.action === 'string' ? args.action : '';
    if (isLocalFilesystemKey(key, actionForKey)) {
      return `Security violation: '${key}' uses unauthorized absolute path. Only /Game/, /Engine/, /Script/, /Temp/, /Saved/, /tmp/, /Niagara/ paths are allowed by default. Set MCP_ADDITIONAL_PATH_PREFIXES to whitelist custom plugin content mount points.`;
    }
    return `Security violation: '${key}' is not a valid Unreal content path. Expected a mounted content root such as /Game/..., /Engine/..., /Script/..., or a plugin mount like /MyPlugin/... (got '${value}').`;
  }

  return undefined;
}

export function ensureArgsPresent(args: unknown): asserts args is Record<string, unknown> {
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

export function validateSecurityPatterns(args: Record<string, unknown>): string | undefined {
  for (const [key, value] of Object.entries(args)) {
    const error = validateValue(args, key, value, 0);
    if (error) {
      return error;
    }
  }
  return undefined;
}

export function validateArgsSecurity(args: HandlerArgs): void {
  ensureArgsPresent(args);
  const securityError = validateSecurityPatterns(args);
  if (securityError) {
    throw new Error(securityError);
  }
}

export function requireAction(args: HandlerArgs): string {
  ensureArgsPresent(args);
  const action = args.action;
  if (typeof action !== 'string' || action.trim() === '') {
    throw new Error('Missing required parameter: action');
  }
  return action;
}

export function requireNonEmptyString(value: unknown, field: string, message?: string): string {
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error(message ?? `Invalid ${field}: must be a non-empty string`);
  }
  return value;
}

export function requireAssetName(value: unknown, field: string, message?: string): string {
  const strValue = requireNonEmptyString(value, field, message);

  if (strValue.includes('/') || strValue.includes('\\')) {
    throw new Error(message ?? `Invalid ${field}: '${strValue}' appears to be a path, not an asset name. Asset names should not contain '/' or '\\' characters. If you meant to specify a path, use the appropriate path parameter instead.`);
  }

  return strValue;
}

export function validateExpectedParams(
  args: Record<string, unknown>,
  allowedParams: string[],
  context: string = 'handler'
): void {
  const alwaysAllowed = ['action', 'subAction', 'timeoutMs'];
  const allAllowed = new Set([...alwaysAllowed, ...allowedParams]);
  const unknownParams = Object.keys(args).filter(key => !allAllowed.has(key));

  if (unknownParams.length > 0) {
    throw new Error(
      `Invalid parameters for ${context}: unknown parameters [${unknownParams.join(', ')}]. ` +
      `Allowed: [${allowedParams.join(', ')}]`
    );
  }
}

export function validateRequiredParams(
  args: Record<string, unknown>,
  requiredParams: string[],
  context: string = 'handler'
): void {
  const missingParams = requiredParams.filter(param => {
    const value = args[param];
    return value === undefined || value === null ||
           (typeof value === 'string' && value.trim() === '');
  });

  if (missingParams.length > 0) {
    throw new Error(
      `Missing required parameters for ${context}: [${missingParams.join(', ')}]`
    );
  }
}
