// src/utils/paths/content-path-policy.ts
// The shared vocabulary for "is this string a safe UE content path / a secret
// argument name", used by the resource, prompt and completion surfaces.
//
// These lived as three near-copies and had already drifted three ways:
//   * resource-errors carried the `u` flag on the host-path regex, prompt-errors
//     did not;
//   * completion-slots additionally rejected `bin|opt|usr`, the other two did not;
//   * the content-root list was spelled twice under two names;
//   * the asset handlers kept a fourth list that caught /proc and /sys and the
//     percent-encoded forms, but only the `c:` drive letter.

// Drift in a REJECTION rule is a security problem — the same input was refused
// on one surface and accepted on another. This module is the union of what the
// three enforced, so consolidating tightens rather than loosens every caller.
//
// Only the vocabulary and the predicates live here. Each surface keeps its own
// throwing wrapper, because each raises its own typed error.

/** UE content mount roots a normalized object/asset handle may reference. */
export const UE_CONTENT_ROOTS = ['/Game', '/Engine', '/Script', '/Temp', '/Niagara'] as const;

/**
 * A host filesystem path, which never belongs in a UE content address.
 * Union of the four prior copies: Windows drive letters, backslashes, `~`, and
 * the common POSIX system roots. `\b` keeps `/binaries` from matching `/bin`.
 */
export const HOST_PATH_PATTERN =
  /^[a-zA-Z]:[\\/]|\\|^~|^\/(?:home|users|etc|proc|sys|var|root|tmp|bin|opt|usr)\b/iu;

/**
 * Percent-encoded traversal, single- and double-encoded. Only surfaces that
 * check a value BEFORE decoding it need this: the resource reader decodes
 * first, so by the time it runs HOST_PATH_PATTERN these forms cannot appear.
 * The asset handlers check raw arguments, so for them this is the live guard.
 */
export const ENCODED_TRAVERSAL_PATTERN = /%2e%2e|%252e/iu;

/** An argument name that names a credential. */
export const SECRET_NAME_PATTERN =
  /(token|secret|password|passwd|api[_-]?key|apikey|credential|private[_-]?key|privatekey|bearer|auth)/u;

/** Maximum serialized byte size for one bounded read or rendered body (64 KiB). */
export const MAX_BOUNDED_BYTES = 65536;

/**
 * True when any segment is `..`, i.e. the value tries to escape its root.
 *
 * Splits on BOTH separators. Two of the three prior copies split on `/` only;
 * this takes the widest (completion-slots') behaviour, so consolidating rejects
 * strictly more than any caller did before. Callers that also run
 * HOST_PATH_PATTERN reject a backslash outright before reaching this.
 */
export function isTraversalPath(value: string): boolean {
  return value.split(/[\\/]/u).includes('..');
}

/** True when the value sits at or under one of the UE content roots. */
export function isUnderContentRoot(value: string): boolean {
  return UE_CONTENT_ROOTS.some((root) => value === root || value.startsWith(`${root}/`));
}

/** UTF-8 byte length, for the bounded-payload guards. */
export function utf8ByteLength(text: string): number {
  return Buffer.byteLength(text, 'utf8');
}
