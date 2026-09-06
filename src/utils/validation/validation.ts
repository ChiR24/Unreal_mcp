/**
 * Validation and sanitization utilities for Unreal Engine assets
 */

import { getAdditionalPathPrefixes } from '../../config.js';

/**
 * Maximum asset name length
 */
const MAX_ASSET_NAME_LENGTH = 64;

/**
 * Invalid characters for Unreal Engine asset names
 * Note: Dashes are allowed in Unreal asset names
 * Includes SQL injection pattern protection (semicolons, quotes, double-dashes)
 */
// eslint-disable-next-line no-useless-escape
const INVALID_CHARS = /[@#%$&*()+=\[\]{}<>?|\\;:'"`,~!\s]/g;

/**
 * SQL injection patterns to reject in asset names
 * These patterns could be dangerous if passed to database queries or eval contexts
 */
const SQL_INJECTION_PATTERNS = /('|";|--|\bDROP\b|\bDELETE\b|\bINSERT\b|\bUPDATE\b|\bEXEC\b|\bEXECUTE\b)/i;
const SQL_INJECTION_REPLACE_PATTERNS = /('|";|--|\bDROP\b|\bDELETE\b|\bINSERT\b|\bUPDATE\b|\bEXEC\b|\bEXECUTE\b)/gi;

/**
 * Reserved keywords that shouldn't be used as names
 */
const RESERVED_KEYWORDS = new Set([
  'none', 'null', 'undefined', 'true', 'false',
  'class', 'struct', 'enum', 'interface',
  'default', 'transient', 'native'
]);

const DEFAULT_ASSET_ROOTS = ['Game', 'Engine', 'Script', 'Temp', 'Niagara'];
let cachedAssetRoots: Set<string> | undefined;

function getAssetRoots(): Set<string> {
  if (!cachedAssetRoots) {
    const additionalRoots = getAdditionalPathPrefixes()
      .map(p => p.replace(/^\//, '').replace(/\/$/, ''));
    cachedAssetRoots = new Set([...DEFAULT_ASSET_ROOTS, ...additionalRoots]);
  }
  return cachedAssetRoots;
}

/**
 * Sanitize a command argument to prevent injection attacks
 * @param arg The argument to sanitize
 * @returns Sanitized argument safe for command execution
 */
export function sanitizeCommandArgument(arg: string): string {
  if (!arg || typeof arg !== 'string') {
    return '';
  }

  let sanitized = arg.trim();

  // Remove null bytes and control characters

  // eslint-disable-next-line no-control-regex
  sanitized = sanitized.replace(/[\x00-\x1F\x7F]/g, '');

  // SECURITY: Replace semicolons with underscores to prevent command injection
  // Semicolons can be used to chain commands (e.g., "MyLevel;Quit" would execute "Quit")
  sanitized = sanitized.replace(/;/g, '_');

  // Escape backslashes and quotes for command safety
  sanitized = sanitized.replace(/\\/g, '\\\\').replace(/"/g, '\\"');

  // Remove newlines and carriage returns that could allow command injection
  sanitized = sanitized.replace(/[\r\n]/g, ' ');

  return sanitized;
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

  // Check for SQL injection patterns and reject early
  if (SQL_INJECTION_PATTERNS.test(sanitized)) {
    // Replace dangerous patterns with underscores instead of throwing
    sanitized = sanitized.replace(SQL_INJECTION_REPLACE_PATTERNS, '_');
  }

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
 * sanitizes individual path segments.
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

  // Ensure the first segment is a valid root (Game, Engine, Script, Temp, Niagara, or configured extras)
  const ROOTS = getAssetRoots();
  if (!ROOTS.has(segments[0])) {
    segments = ['Game', ...segments];
  }

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
