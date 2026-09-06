import { z } from 'zod';

import { sanitizePath } from '../../../../utils/paths/path-security.js';
import { SemanticBoundaryError } from './errors.js';

// Wire-boundary Unreal path types. These are deliberately distinct from the
// permissive runtime helpers elsewhere: a path here carries a security-validated,
// /Content->/Game-normalized branded string so a raw asset string can never be
// confused with a class or object reference at the boundary. The brand is proven
// by Zod parse (never by a cast), and security rejections surface as typed
// `SemanticBoundaryError` variants rather than bare generic errors.
//
// SAFE-PARSE CONTRACT: The exported schemas (AssetPathSchema/ObjectPathSchema/
// ClassPathSchema) use `.superRefine` + `ctx.addIssue` (never a throwing
// transform) so `.safeParse()` on invalid input returns `{ success: false }`
// instead of throwing. The `parse*` helpers call the same canonical sanitizer
// directly, which throws a typed `SemanticBoundaryError` with a precise code
// (PATH_TRAVERSAL / INVALID_PATH_ROOT). Direct `.parse()` on a schema throws a
// ZodError (acceptable ZodError semantics); `.safeParse()` never throws.

const ALLOWED_ROOTS = ['/Game', '/Engine', '/Script', '/Temp', '/Niagara'] as const;

// Case-insensitive /Content -> /Game mount normalization, applied exactly once
// at the boundary (a /Game result can never re-trigger it). Matches the
// case-insensitive root policy of the shared sanitizePath helper.
function normalizeContentToGame(raw: string): string {
  let normalized = raw.replace(/\\/g, '/');
  if (/^\/?content\//i.test(normalized)) {
    const idx = normalized.toLowerCase().indexOf('content/');
    normalized = `/Game/${normalized.slice(idx + 'content/'.length)}`;
  }
  return normalized;
}

// Asset-path root/traversal gate: throws a typed error (instead of a generic
// one) for an invalid root or directory traversal before sanitizePath is reached.
function assertValidRootAndNoTraversal(normalized: string): void {
  if (normalized.includes('..')) {
    throw new SemanticBoundaryError({
      kind: 'path',
      code: 'PATH_TRAVERSAL',
      message: 'Invalid path: directory traversal (..) is not allowed',
      input: normalized
    });
  }
  const dot = normalized.indexOf('.');
  const colon = normalized.indexOf(':');
  const end = Math.min(dot === -1 ? Infinity : dot, colon === -1 ? Infinity : colon);
  const prefix = end === Infinity ? normalized : normalized.slice(0, end);
  const isAllowed = ALLOWED_ROOTS.some(
    (root) => prefix === root || prefix.startsWith(`${root}/`)
  );
  if (!isAllowed) {
    throw new SemanticBoundaryError({
      kind: 'path',
      code: 'INVALID_PATH_ROOT',
      message: `Invalid path: must start with one of [${ALLOWED_ROOTS.join(', ')}]`,
      input: normalized
    });
  }
}

// Route the path PREFIX (the part before the first '.'/'::' suffix) through the
// shared sanitizePath policy (root allow-list, traversal, double-slash, illegal
// Windows/control chars). sanitizePath throws a plain Error, so we re-wrap it as
// a typed SemanticBoundaryError to keep the boundary's error algebra uniform.
function sanitizePrefixViaShared(prefix: string): string {
  try {
    return sanitizePath(prefix);
  } catch (err) {
    throw new SemanticBoundaryError({
      kind: 'path',
      code: 'INVALID_PATH_ROOT',
      message: err instanceof Error ? err.message : 'Invalid path',
      input: prefix
    });
  }
}

// Object/Class paths may carry a `.Subobject` / `:Property` / `::` suffix that the
// shared sanitizePath illegal-char set would reject. We split the suffix off (at the
// first '.' or ':' - ':' covers both ':Property' and '::Member' forms), run the
// shared policy over the prefix, then re-validate the suffix for illegal characters
// (allowing only the '.'/'::' separators) so object/class references get the same
// character-level hardening as asset paths.
//
// DOUBLE-SLASH POLICY: object/class paths STRICTLY REJECT '//' as INVALID_PATH_ROOT.
// This diverges from asset paths, which run through the shared sanitizePath helper
// that NORMALIZES '//' (to prevent engine crashes on asset load). The divergence is
// intentional: object/class references are subobject pointers where a collapsed
// double-slash would silently change the addressed subobject, whereas asset paths
// are mount-relative where normalization is the safer choice.
function sanitizeObjectOrClassPath(input: unknown): string {
  if (typeof input !== 'string' || input.length === 0) {
    throw new SemanticBoundaryError({
      kind: 'validation',
      code: 'VALIDATION_ERROR',
      message: 'Path must be a non-empty string'
    });
  }
  const normalized = normalizeContentToGame(input);
  if (normalized.includes('//')) {
    throw new SemanticBoundaryError({
      kind: 'path',
      code: 'INVALID_PATH_ROOT',
      message: 'Invalid path: double slash (//) is not allowed',
      input: normalized
    });
  }
  if (normalized.includes('..')) {
    throw new SemanticBoundaryError({
      kind: 'path',
      code: 'PATH_TRAVERSAL',
      message: 'Invalid path: directory traversal (..) is not allowed',
      input: normalized
    });
  }
  const dot = normalized.indexOf('.');
  const colon = normalized.indexOf(':');
  const end = Math.min(dot === -1 ? Infinity : dot, colon === -1 ? Infinity : colon);
  const prefix = end === Infinity ? normalized : normalized.slice(0, end);
  const suffix = end === Infinity ? '' : normalized.slice(end);
  const sanitizedPrefix = sanitizePrefixViaShared(prefix);
  if (suffixHasIllegalChars(suffix)) {
    throw new SemanticBoundaryError({
      kind: 'path',
      code: 'INVALID_PATH_ROOT',
      message: 'Invalid path: contains illegal characters',
      input: normalized
    });
  }
  return `${canonicalizeRoot(sanitizedPrefix)}${suffix}`;
}

// Canonicalize the leading root to its declared casing (e.g. /game/Foo -> /Game/Foo)
// so object/class references share the case-insensitive root policy of sanitizePath.
function canonicalizeRoot(prefix: string): string {
  const lower = prefix.toLowerCase();
  for (const root of ALLOWED_ROOTS) {
    const rootLower = root.toLowerCase();
    if (lower === rootLower || lower.startsWith(`${rootLower}/`)) {
      return `${root}${prefix.slice(root.length)}`;
    }
  }
  return prefix;
}

// Illegal characters for object/class suffixes: Windows reserved chars plus
// control characters. The '.' (subobject) and '::' (property) separators are
// allowed, so they are intentionally excluded from this check.
function suffixHasIllegalChars(value: string): boolean {
  for (let i = 0; i < value.length; i++) {
    const code = value.charCodeAt(i);
    if (code <= 0x1f || code === 0x7f) return true;
    const ch = value[i];
    if (ch === '<' || ch === '>' || ch === '"' || ch === '|' || ch === '?' || ch === '*') return true;
  }
  return false;
}

// Core sanitization for asset paths (the single canonical sanitizer shared by the
// schema superRefine and parseAssetPath). Throws SemanticBoundaryError on rejection.
// Wraps shared sanitizePath failures (plain Error) as typed domain errors.
function sanitizeAssetPathCore(raw: string): string {
  const normalized = normalizeContentToGame(raw);
  assertValidRootAndNoTraversal(normalized);
  try {
    return sanitizePath(normalized);
  } catch (err) {
    throw new SemanticBoundaryError({
      kind: 'path',
      code: 'INVALID_PATH_ROOT',
      message: err instanceof Error ? err.message : 'Invalid path',
      input: normalized
    });
  }
}

// Convert a sanitizer's typed SemanticBoundaryError into a Zod issue so the
// exported schemas' `.safeParse()` returns `{ success: false }` instead of
// throwing. Direct `.parse()` throws a ZodError (acceptable); `.safeParse()`
// never throws.
function reportSanitizerFailure(err: unknown, ctx: z.RefinementCtx): never {
  const message =
    err instanceof SemanticBoundaryError
      ? err.semanticError.message
      : err instanceof Error
        ? err.message
        : 'Invalid path';
  ctx.addIssue({ code: 'custom', message, fatal: true });
  return z.NEVER;
}

// Exported schemas route untrusted values through the same canonical sanitization
// path as the parse* helpers, so a direct `AssetPathSchema.parse(unsafe)` cannot
// mint an unsafe branded string. The superRefine catches sanitizer throws and
// converts them to ctx.addIssue so safeParse() returns { success: false }; the
// transform runs the sanitizer again on validated input to normalize (idempotent:
// a sanitized /Game path passes through unchanged on re-parse).
export const AssetPathSchema = z
  .string()
  .superRefine((raw, ctx) => {
    try {
      sanitizeAssetPathCore(raw);
    } catch (err) {
      return reportSanitizerFailure(err, ctx);
    }
  })
  .transform((raw): string => sanitizeAssetPathCore(raw))
  .brand<'AssetPath'>();
export type AssetPath = z.infer<typeof AssetPathSchema>;

export const ObjectPathSchema = z
  .string()
  .superRefine((raw, ctx) => {
    try {
      sanitizeObjectOrClassPath(raw);
    } catch (err) {
      return reportSanitizerFailure(err, ctx);
    }
  })
  .transform((raw): string => sanitizeObjectOrClassPath(raw))
  .brand<'ObjectPath'>();
export type ObjectPath = z.infer<typeof ObjectPathSchema>;

export const ClassPathSchema = z
  .string()
  .superRefine((raw, ctx) => {
    try {
      sanitizeObjectOrClassPath(raw);
    } catch (err) {
      return reportSanitizerFailure(err, ctx);
    }
  })
  .transform((raw): string => sanitizeObjectOrClassPath(raw))
  .brand<'ClassPath'>();
export type ClassPath = z.infer<typeof ClassPathSchema>;

// parse* helpers call the canonical sanitizer directly so they throw a typed
// SemanticBoundaryError (with a precise code) rather than a ZodError, then mint
// the brand by parsing the already-sanitized value through the schema (the
// superRefine passes, the transform is idempotent, the brand is minted).
export function parseAssetPath(input: unknown): AssetPath {
  const sanitized = sanitizeAssetPathCore(input as string);
  return AssetPathSchema.parse(sanitized);
}

export function parseObjectPath(input: unknown): ObjectPath {
  const sanitized = sanitizeObjectOrClassPath(input);
  return ObjectPathSchema.parse(sanitized);
}

export function parseClassPath(input: unknown): ClassPath {
  const sanitized = sanitizeObjectOrClassPath(input);
  return ClassPathSchema.parse(sanitized);
}

