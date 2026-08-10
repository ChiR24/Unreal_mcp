import {
  HOST_PATH_PATTERN,
  MAX_BOUNDED_BYTES,
  UE_CONTENT_ROOTS,
  isTraversalPath,
  isUnderContentRoot,
  utf8ByteLength,
} from '../utils/paths/content-path-policy.js';

// src/resources/resource-errors.ts
// Task 31: typed errors, byte budget, path guards, and redaction shared by the
// version-aware read-only resource providers. Every failure path (traversal,
// unknown, unavailable, oversize, malformed) converges on `ResourceError` so a
// failed read is never mistaken for a success payload, and no host filesystem
// path or secret ever crosses the boundary.

/** Stable, typed error codes for the resource surface. */
export const RESOURCE_ERROR_CODES = {
  INVALID_URI: 'RESOURCE_INVALID_URI',
  NOT_FOUND: 'RESOURCE_NOT_FOUND',
  UNAVAILABLE: 'RESOURCE_UNAVAILABLE',
  TOO_LARGE: 'RESOURCE_TOO_LARGE',
  TRAVERSAL: 'RESOURCE_TRAVERSAL_REJECTED',
} as const;

export type ResourceErrorCode = (typeof RESOURCE_ERROR_CODES)[keyof typeof RESOURCE_ERROR_CODES];

/** A typed, non-mutating resource failure carrying the code and the URI. */
export class ResourceError extends Error {
  readonly code: ResourceErrorCode;
  readonly uri: string;

  constructor(code: ResourceErrorCode, uri: string, message: string) {
    super(message);
    this.name = 'ResourceError';
    this.code = code;
    this.uri = uri;
  }
}

/** Maximum serialized byte size for a single bounded resource read (64 KiB). */
export const MAX_RESOURCE_BYTES = MAX_BOUNDED_BYTES;

/** Reject a serialized payload that exceeds the bounded read budget. */
export function enforceByteBudget(uri: string, text: string): void {
  const bytes = utf8ByteLength(text);
  if (bytes > MAX_RESOURCE_BYTES) {
    throw new ResourceError(
      RESOURCE_ERROR_CODES.TOO_LARGE,
      uri,
      `Resource payload is ${bytes} bytes, exceeding the ${MAX_RESOURCE_BYTES} byte budget`,
    );
  }
}

// Re-exported: callers already import UE_CONTENT_ROOTS from this module, and the
// list itself now lives in the shared content-path policy alongside the
// host-path pattern and the root predicate, so the resource, prompt and
// completion surfaces cannot drift apart again.
export { UE_CONTENT_ROOTS };

/**
 * Decode and normalize a template path parameter into a safe UE content handle.
 * Rejects directory traversal, host filesystem paths, control characters, and
 * any path that does not resolve under a known UE mount root.
 */
export function normalizeContentPath(uri: string, rawPath: string): string {
  let decoded: string;
  try {
    decoded = decodeURIComponent(rawPath);
  } catch {
    throw new ResourceError(RESOURCE_ERROR_CODES.INVALID_URI, uri, 'Malformed percent-encoding in resource path');
  }

  if (decoded.length === 0) {
    throw new ResourceError(RESOURCE_ERROR_CODES.INVALID_URI, uri, 'Empty resource path');
  }
  // eslint-disable-next-line no-control-regex
  if (/[\u0000-\u001f\u007f]/u.test(decoded)) {
    throw new ResourceError(RESOURCE_ERROR_CODES.INVALID_URI, uri, 'Control characters in resource path');
  }
  if (HOST_PATH_PATTERN.test(decoded)) {
    throw new ResourceError(RESOURCE_ERROR_CODES.TRAVERSAL, uri, 'Host filesystem paths are not addressable');
  }

  let normalized = decoded.replace(/\/+/gu, '/');
  if (normalized.toLowerCase().startsWith('/content')) {
    normalized = '/Game' + normalized.slice('/Content'.length);
  }
  if (!normalized.startsWith('/')) {
    normalized = '/' + normalized;
  }
  if (isTraversalPath(normalized)) {
    throw new ResourceError(RESOURCE_ERROR_CODES.TRAVERSAL, uri, 'Path traversal segments are not permitted');
  }
  if (normalized.length > 1) {
    normalized = normalized.replace(/\/$/u, '');
  }

  if (!isUnderContentRoot(normalized)) {
    throw new ResourceError(
      RESOURCE_ERROR_CODES.INVALID_URI,
      uri,
      `Path must resolve under a UE content root (${UE_CONTENT_ROOTS.join(', ')})`,
    );
  }
  return normalized;
}

/**
 * Reduce a raw project path (possibly an absolute host `.uproject` path) to just
 * the project name. Never returns a directory; if the result would still look
 * like a path, it is dropped so no host layout leaks.
 */
export function redactProjectName(raw: string | undefined): string | undefined {
  if (raw === undefined) {
    return undefined;
  }
  const trimmed = raw.trim();
  if (trimmed.length === 0) {
    return undefined;
  }
  const base = trimmed.split(/[\\/]/u).pop() ?? '';
  const withoutExt = base.replace(/\.uproject$/iu, '');
  if (withoutExt.length === 0 || /[\\/:]/u.test(withoutExt)) {
    return undefined;
  }
  return withoutExt;
}
