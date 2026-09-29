/**
 * Validation and sanitization utilities for Unreal Engine assets
 */

import { getContentRoots, getContentRootsRevision } from '../paths/path-security.js';

/**
 * Maximum asset name length
 */
const MAX_ASSET_NAME_LENGTH = 64;

/**
 * Invalid characters for Unreal Engine asset names
 * Note: Dashes are allowed in Unreal asset names
 */
// eslint-disable-next-line no-useless-escape
const INVALID_CHARS = /[@#%$&*()+=\[\]{}<>?|\\;:'"`,~!\s]/g;

/**
 * Reserved keywords that shouldn't be used as names
 */
const RESERVED_KEYWORDS = new Set([
  'none', 'null', 'undefined', 'true', 'false',
  'class', 'struct', 'enum', 'interface',
  'default', 'transient', 'native'
]);

let cachedAssetRoots: Set<string> | undefined;
let cachedRootByLowerCase: Map<string, string> | undefined;
let cachedRootsRevision = -1;

// Rebuilt whenever the content roots change (a connected editor reported new
// mounts), so a plugin mount is recognised as a root instead of being treated as
// a folder under /Game.
function getAssetRoots(): Set<string> {
  const revision = getContentRootsRevision();
  if (!cachedAssetRoots || cachedRootsRevision !== revision) {
    cachedAssetRoots = new Set(getContentRoots().map(root => root.replace(/^\//, '')));
    cachedRootByLowerCase = undefined;
    cachedRootsRevision = revision;
  }
  return cachedAssetRoots;
}

/**
 * The declared spelling of a mount root, matched case-insensitively.
 *
 * UE mount roots are case-insensitive, and the strict `sanitizePath` helper has
 * always matched them that way. Matching case-SENSITIVELY here meant `/game/Foo`
 * and `/engine/Bar` were not recognised as roots at all, so they were prefixed
 * as if they were bare folder names: `/Game/game/Foo` and `/Game/engine/Bar` —
 * silently relocating the asset, in `/engine`'s case into a different mount.
 */
function canonicalAssetRoot(segment: string): string | undefined {
  const roots = getAssetRoots();
  if (!cachedRootByLowerCase) {
    cachedRootByLowerCase = new Map([...roots].map(root => [root.toLowerCase(), root]));
  }
  return cachedRootByLowerCase.get(segment.toLowerCase());
}

/**
 * Sanitize an asset name for Unreal Engine
 * @param name The name to sanitize
 * @returns Sanitized name
 */
export function sanitizeAssetName(name: string): string {
  if (!name || typeof name !== 'string') {
    return 'Asset';
  }

  let sanitized = name.trim();

  // Replace invalid characters with underscores
  sanitized = sanitized.replace(INVALID_CHARS, '_');

  sanitized = sanitized.replace(/_+/g, '_');

  sanitized = sanitized.replace(/^_+|_+$/g, '');

  // If name is empty after sanitization, use default
  if (!sanitized) {
    return 'Asset';
  }

  // If name is a reserved keyword, append underscore
  if (RESERVED_KEYWORDS.has(sanitized.toLowerCase())) {
    sanitized = `${sanitized}_Asset`;
  }

  // Ensure name starts with a letter
  if (!/^[A-Za-z]/.test(sanitized)) {
    sanitized = `Asset_${sanitized}`;
  }

  // Truncate overly long names to reduce risk of hitting path length limits
  if (sanitized.length > MAX_ASSET_NAME_LENGTH) {
    sanitized = sanitized.slice(0, MAX_ASSET_NAME_LENGTH);
  }

  return sanitized;
}

/**
 * Normalize and sanitize an Unreal asset path.
 *
 * Unlike the strict path-security helper, this function accepts partial paths,
 * defaults empty input to /Game, prefixes unknown roots with /Game, and
 * sanitizes individual path segments. A known root is one of the static roots,
 * a configured extra, or a mount the connected editor reported.
 * @param path The path to sanitize
 * @returns Sanitized path
 */
export function normalizeAndSanitizeAssetPath(path: string): string {
  if (!path || typeof path !== 'string') {
    return '/Game';
  }

  path = path.replace(/\\/g, '/');

  // Normalize double slashes (prevents engine crash from paths like /Game//Test)
  while (path.includes('//')) {
    path = path.replace(/\/\//g, '/');
  }

  // Ensure path starts with /
  if (!path.startsWith('/')) {
    path = `/${path}`;
  }

  let segments = path.split('/').filter(s => s.length > 0);

  // Block path traversal attempts
  if (segments.some(s => s === '..' || s === '.')) {
    throw new Error('Path traversal (..) is not allowed');
  }

  if (segments.length === 0) {
    return '/Game';
  }

  // Ensure the first segment is a valid root (Game, Engine, Script, Temp, Niagara,
  // configured extras, or a mount the connected editor reported), matched
  // case-insensitively and rewritten to its declared spelling so the rest of
  // the pipeline sees one canonical form.
  const ROOTS = getAssetRoots();
  const declaredRoot = canonicalAssetRoot(segments[0]);
  segments = declaredRoot === undefined
    ? ['Game', ...segments]
    : [declaredRoot, ...segments.slice(1)];

  const sanitizedSegments = segments.map(segment => {
    // Don't sanitize root folders
    if (ROOTS.has(segment)) {
      return segment;
    }
    // A leading underscore is valid in UE package/folder paths (e.g. /Game/_Scratch).
    // sanitizeAssetName strips leading underscores — correct for a bare asset NAME,
    // but for a path SEGMENT it silently relocated assets to the wrong folder
    // (/Game/_Scratch -> /Game/Scratch), diverging from manage_blueprint. Preserve it.
    const cleaned = sanitizeAssetName(segment);
    return /^_/.test(segment) && !cleaned.startsWith('_') ? `_${cleaned}` : cleaned;
  });

  // Reconstruct path
  return '/' + sanitizedSegments.join('/');
}
