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
 * Top-level names that are SHAPED like a content mount but name a real directory on disk. A plugin or
 * game-feature mount never uses one of these, so they stay rejected even where a virtual asset path is
 * expected -- `path` in particular is ambiguous and some actions use it as a filesystem path.
 *
 * Deliberately a SUPERSET of the POSIX roots HOST_PATH_PATTERN names: it adds the project-layout
 * directories (Saved, Config, Binaries, ...) and the Windows / macOS / WSL roots HOST_PATH_PATTERN does
 * not cover. Narrowing this to HOST_PATH_PATTERN alone would let `/Windows/...`, `/ProgramData/...` or
 * `/Saved/...` pass the mount check.
 */
export const NON_CONTENT_MOUNT_ROOTS: ReadonlySet<string> = new Set([
  // project layout
  'saved', 'config', 'binaries', 'intermediate', 'build', 'source', 'content',
  'plugins', 'logs', 'derivedatacache',
  // host OS roots (POSIX, Windows, macOS, and the WSL `/mnt/c/...` shape)
  'tmp', 'etc', 'usr', 'var', 'bin', 'dev', 'proc', 'sys', 'home', 'root', 'users',
  'windows', 'boot', 'opt', 'mnt', 'media', 'volumes', 'library', 'private',
  'programdata', 'appdata', 'documents', 'srv', 'run', 'lib', 'sbin',
]);

/** A content root is one leading segment of letters, digits, underscore or hyphen: /Game/, /Paper2D/. */
const CONTENT_MOUNT_ROOT_PATTERN = /^\/[a-z0-9_][a-z0-9_-]*(\/|$)/u;

/**
 * Whether `normalizedLowerValue` (posix-normalized, lowercased) is SHAPED like an Unreal virtual content
 * path rooted at a plugin or game-feature mount, e.g. `/myplugin/blueprints/bp_foo`.
 *
 * Shape only. Those are package paths the engine resolves through its registered mount table and never
 * opens as files; the authority on which mounts exist is the plugin side (SanitizeProjectRelativePath ->
 * FPackageName::IsValidLongPackageName). A static allowlist here cannot know per-project mount names, so
 * an unknown mount should fail as an honest NOT_FOUND from the engine, not as a SECURITY_VIOLATION.
 *
 * HOST_PATH_PATTERN is consulted first so this check and every other path surface agree on what a host
 * path is; NON_CONTENT_MOUNT_ROOTS then covers the roots HOST_PATH_PATTERN does not name.
 */
export function isContentMountShapedPath(normalizedLowerValue: string): boolean {
  if (HOST_PATH_PATTERN.test(normalizedLowerValue)) {
    return false;
  }
  if (!CONTENT_MOUNT_ROOT_PATTERN.test(normalizedLowerValue)) {
    return false;
  }
  const root = normalizedLowerValue.split('/')[1] ?? '';
  return !NON_CONTENT_MOUNT_ROOTS.has(root);
}

/**
 * Percent-encoded traversal, single- and double-encoded.
 *
 * Kept as a named export because the asset handlers and the tests refer to it,
 * but it is NOT what makes `isTraversalPath` encoding-aware: a spelling list
 * only ever catches the spellings on it, and `%2e.`, `.%2e` and `..%2f` are
 * not on it. `isTraversalPath` decodes instead.
 */
export const ENCODED_TRAVERSAL_PATTERN = /%2e%2e|%252e/iu;

/**
 * How many decode rounds `isTraversalPath` will run before giving up.
 *
 * Decoding to a fixed point is what catches mixed encodings; a bound is what
 * keeps a pathological input (`%25` repeated, each round peeling one layer)
 * from turning the check into a long loop. Four rounds covers every real
 * double-encoding; anything still shrinking after that is hostile by
 * construction and is treated as traversal.
 */
const MAX_TRAVERSAL_DECODE_ROUNDS = 4;

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
 *
 * Percent-encoded traversal counts as traversal. Three of the four callers used
 * to miss it: the prompt and completion surfaces test the raw argument and so
 * saw `%2e%2e` as an ordinary name, and the resource reader's single decode
 * turns `%252e%252e` into `%2e%2e` rather than into `..`. Folding the check in
 * here fixes all three at the one place they share, instead of at each caller.
 *
 * It decodes to a fixed point rather than matching known spellings, because a
 * spelling list catches only what is on it: `%2e%2e` and `%252e` were caught,
 * while `/Game/%2e./S`, `/Game/.%2e/S` and `/Game/..%2fS` all decode to
 * `/Game/../S` and all passed. Each round peels one encoding layer; the loop
 * stops when decoding stops changing the string, when it throws on malformed
 * input (which is itself refused), or at MAX_TRAVERSAL_DECODE_ROUNDS.
 */
export function isTraversalPath(value: string): boolean {
  let current = value;
  for (let round = 0; round <= MAX_TRAVERSAL_DECODE_ROUNDS; round += 1) {
    if (current.split(/[\\/]/u).includes('..')) return true;
    if (round === MAX_TRAVERSAL_DECODE_ROUNDS) {
      // Still decoding to something new after the bound: refuse rather than
      // return a verdict derived from a prefix of the decoding.
      return current !== value;
    }
    let decoded: string;
    try {
      decoded = decodeURIComponent(current);
    } catch {
      // Malformed percent-encoding never addresses a real asset, and letting it
      // through would leave the downstream decoder to resolve it unchecked.
      return true;
    }
    if (decoded === current) return false;
    current = decoded;
  }
  return false;
}

/** True when the value sits at or under one of the UE content roots. */
export function isUnderContentRoot(value: string): boolean {
  return UE_CONTENT_ROOTS.some((root) => value === root || value.startsWith(`${root}/`));
}

/** UTF-8 byte length, for the bounded-payload guards. */
export function utf8ByteLength(text: string): number {
  return Buffer.byteLength(text, 'utf8');
}
