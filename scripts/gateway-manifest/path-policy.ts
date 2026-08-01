// scripts/gateway-manifest/path-policy.ts
// Typed path policy for pilot OUTPUT and catalog INPUT.
//
// OUTPUT policy: pilot artifacts may only be written under `<repo>/.omo` or
// the OS temp directory. Repo source/plugin/root dirs and other external
// locations are rejected with a typed PilotPathError. This prevents an
// env-controlled path (MCP_PILOT_OUTPUT_DIR) from overwriting production or
// repo source paths.
//
// INPUT policy: the pilot catalog may only be read from under the repo root
// or the OS temp directory (future task fixtures). Other locations and
// symlink escapes are rejected. The default missing-catalog typed failure in
// load.ts is preserved.
//
// Physical containment is validated using the nearest existing ancestor's
// realpath so a symlink under an allowed root cannot escape to an external
// target.

import { lstatSync, realpathSync, statSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { resolve as resolvePath, sep } from 'node:path';

export type PilotPathErrorDetail =
  | { readonly code: 'PILOT_OUTPUT_DIR_FORBIDDEN'; readonly requested: string; readonly message: string }
  | { readonly code: 'PILOT_OUTPUT_DIR_SYMLINK_ESCAPE'; readonly requested: string; readonly realpath: string; readonly message: string }
  | { readonly code: 'PILOT_CATALOG_PATH_FORBIDDEN'; readonly requested: string; readonly message: string }
  | { readonly code: 'PILOT_CATALOG_PATH_SYMLINK_ESCAPE'; readonly requested: string; readonly realpath: string; readonly message: string };

export class PilotPathError extends Error {
  readonly code: PilotPathErrorDetail['code'];
  readonly detail: PilotPathErrorDetail;
  constructor(detail: PilotPathErrorDetail) {
    super(detail.message);
    this.name = 'PilotPathError';
    this.code = detail.code;
    this.detail = detail;
  }
}

/**
 * Repository production roots that pilot OUTPUT must never touch. Resolved
 * against the repo root before comparison.
 */
const FORBIDDEN_OUTPUT_ROOTS = (repoRoot: string): readonly string[] => [
  repoRoot,
  resolvePath(repoRoot, 'src'),
  resolvePath(repoRoot, 'plugins'),
  resolvePath(repoRoot, 'plugins/McpAutomationBridge'),
  resolvePath(repoRoot, 'plugins/McpAutomationBridge/Source'),
  resolvePath(repoRoot, 'tests'),
  resolvePath(repoRoot, 'scripts'),
  resolvePath(repoRoot, 'docs'),
];

/**
 * Allowed scratch roots for pilot OUTPUT: `<repo>/.omo` and the OS temp dir.
 */
const allowedOutputRoots = (repoRoot: string): readonly string[] => [
  resolvePath(repoRoot, '.omo'),
  resolvePath(tmpdir()),
];

function isWithin(candidate: string, root: string): boolean {
  const prefix = root.endsWith(sep) ? root : root + sep;
  return candidate === root || candidate.startsWith(prefix);
}

function isWithinAny(candidate: string, roots: readonly string[]): boolean {
  return roots.some((root) => isWithin(candidate, root));
}

/**
 * Resolve the nearest existing ancestor of `path` and return its realpath.
 * Used to validate physical containment without requiring the target path to
 * exist yet. Throws PilotPathError if an ancestor's realpath escapes the
 * allowed roots.
 */
function physicalAncestorRealpath(
  path: string,
  allowedRoots: readonly string[],
  errorEscape: (requested: string, real: string) => PilotPathError,
  errorForbidden: (requested: string) => PilotPathError,
): string {
  // Walk up from the target path until an existing ancestor is found, then
  // resolve its realpath. This catches symlinked directories under an allowed
  // root that point outside.
  let current = path;
  const visited = new Set<string>();
  for (;;) {
    if (visited.has(current)) {
      throw errorForbidden(path);
    }
    visited.add(current);
    try {
      lstatSync(current);
    } catch {
      const parent = resolvePath(current, '..');
      if (parent === current) {
        throw errorForbidden(path);
      }
      current = parent;
      continue;
    }
    const real = realpathSync(current);
    if (!isWithinAny(real, allowedRoots)) {
      throw errorEscape(path, real);
    }
    return real;
  }
}


/**
 * Physical containment: a symlinked ancestor under an allowed root must not
 * resolve outside it.
 *
 * The output-dir and catalog-path validators wrapped the identical
 * `physicalAncestorRealpath` call in the identical pair of inline error
 * factories, differing only in two codes and the prose. A new containment rule
 * had to be added twice, and the four codes were a 2x2 table maintained by hand.
 * The message text is reproduced exactly, so no caller sees a changed error.
 */
function guardPhysicalContainment(
  requested: string,
  allowed: readonly string[],
  spec: {
    readonly subject: string;
    readonly escapeCode: 'PILOT_OUTPUT_DIR_SYMLINK_ESCAPE' | 'PILOT_CATALOG_PATH_SYMLINK_ESCAPE';
    readonly forbiddenCode: 'PILOT_OUTPUT_DIR_FORBIDDEN' | 'PILOT_CATALOG_PATH_FORBIDDEN';
    readonly escapeRoots: string;
    readonly forbiddenRoots: string;
  },
): void {
  physicalAncestorRealpath(
    requested,
    allowed,
    (req, real) =>
      new PilotPathError({
        code: spec.escapeCode,
        requested: req,
        realpath: real,
        message:
          `${spec.subject} ${req} resolves (via symlink) to ${real}, which is ` +
          `outside ${spec.escapeRoots}. Symlink escape is not permitted.`,
      }),
    (req) =>
      new PilotPathError({
        code: spec.forbiddenCode,
        requested: req,
        message: `${spec.subject} ${req} could not be validated against ${spec.forbiddenRoots}.`,
      }),
  );
}

/**
 * Validate and resolve the pilot OUTPUT directory. The directory does not
 * need to exist; its nearest existing ancestor is validated for physical
 * containment. Resolves relative env paths against `repoRoot`.
 */
export function resolvePilotOutputDir(envPath: string | undefined, repoRoot: string): string {
  if (envPath === undefined || envPath.length === 0) {
    return resolvePath(repoRoot, '.omo/pilot-manifest');
  }
  const requested = resolvePath(repoRoot, envPath);
  const allowed = allowedOutputRoots(repoRoot);

  // Reject if the requested path is inside a forbidden production root.
  const forbidden = FORBIDDEN_OUTPUT_ROOTS(repoRoot);
  if (isWithinAny(requested, forbidden) && !isWithinAny(requested, allowed)) {
    throw new PilotPathError({
      code: 'PILOT_OUTPUT_DIR_FORBIDDEN',
      requested,
      message:
        `Pilot output dir ${requested} is inside a forbidden production/repo root. ` +
        'Allowed scratch roots: <repo>/.omo or the OS temp directory. ' +
        'Set MCP_PILOT_OUTPUT_DIR to a path under one of those roots.',
    });
  }

  // Reject if the requested path is outside all allowed scratch roots.
  if (!isWithinAny(requested, allowed)) {
    throw new PilotPathError({
      code: 'PILOT_OUTPUT_DIR_FORBIDDEN',
      requested,
      message:
        `Pilot output dir ${requested} is outside the allowed scratch roots. ` +
        'Allowed scratch roots: <repo>/.omo or the OS temp directory. ' +
        'Set MCP_PILOT_OUTPUT_DIR to a path under one of those roots.',
    });
  }

  // Physical containment: a symlinked ancestor under an allowed root must not
  // resolve outside.
  guardPhysicalContainment(requested, allowed, {
    subject: 'Pilot output dir',
    escapeCode: 'PILOT_OUTPUT_DIR_SYMLINK_ESCAPE',
    forbiddenCode: 'PILOT_OUTPUT_DIR_FORBIDDEN',
    escapeRoots: 'the allowed scratch roots',
    forbiddenRoots: 'the allowed scratch roots',
  });

  return requested;
}

/**
 * Validate the pilot catalog INPUT path. Allowed under the repo root or the OS
 * temp directory. Rejects other locations and symlink escapes. The path does
 * not need to exist; its nearest existing ancestor is validated. Resolves
 * relative env paths against `repoRoot`.
 */
export function validatePilotCatalogPath(path: string, repoRoot: string): string {
  const requested = resolvePath(repoRoot, path);
  const allowed = [repoRoot, resolvePath(tmpdir())];

  if (!isWithinAny(requested, allowed)) {
    throw new PilotPathError({
      code: 'PILOT_CATALOG_PATH_FORBIDDEN',
      requested,
      message:
        `Pilot catalog path ${requested} is outside the repo root and the OS temp directory. ` +
        'Set MCP_PILOT_CATALOG_PATH to a path under one of those roots.',
    });
  }

  guardPhysicalContainment(requested, allowed, {
    subject: 'Pilot catalog path',
    escapeCode: 'PILOT_CATALOG_PATH_SYMLINK_ESCAPE',
    forbiddenCode: 'PILOT_CATALOG_PATH_FORBIDDEN',
    escapeRoots: 'the repo root or the OS temp directory',
    forbiddenRoots: 'the allowed roots',
  });

  return requested;
}

/**
 * Stat helper for callers that need to check file size before reading.
 * Returns the file size in bytes. Throws PilotPathError if the path is not a
 * regular file (stat follows symlinks; symlink escape is already rejected by
 * validatePilotCatalogPath before this is called).
 */
export function fileSizeBytes(filePath: string): number {
  const stats = statSync(filePath);
  if (!stats.isFile()) {
    throw new PilotPathError({
      code: 'PILOT_CATALOG_PATH_FORBIDDEN',
      requested: filePath,
      message: `Pilot catalog path ${filePath} is not a regular file.`,
    });
  }
  return stats.size;
}
