// scripts/gateway-manifest/write.ts
// Transactional multi-target writer and drift checker.
//
// writeManifestTargets: stages ALL temp files first, then renames to finals.
//   A staging failure deletes all staged temps and throws BEFORE any final
//   rename, so all final files remain unchanged. Temp files use a random
//   transaction suffix (randomUUID), exclusive creation (wx), and mode 0o600.
//   Duplicate final target paths and existing final symlinks are rejected.
//
// checkManifestDrift: reports missing AND stale files as drift with actionable
//   diagnostics. NEVER writes, NEVER throws on missing files.

import { randomUUID } from 'node:crypto';
import { chmodSync, existsSync, lstatSync, mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from 'node:fs';
import { dirname } from 'node:path';

export type ManifestTarget = readonly [path: string, content: string];

export interface ManifestDriftEntry {
  readonly path: string;
  readonly kind: 'missing' | 'stale';
  readonly message: string;
}

export interface ManifestDriftResult {
  readonly drift: boolean;
  readonly entries: readonly ManifestDriftEntry[];
}

export class ManifestWriteError extends Error {
  readonly code: 'DUPLICATE_TARGET' | 'SYMLINK_TARGET';
  readonly targetPath: string;

  constructor(code: ManifestWriteError['code'], targetPath: string, message: string) {
    super(message);
    this.name = 'ManifestWriteError';
    this.code = code;
    this.targetPath = targetPath;
  }
}

/**
 * Check mode: compare expected content against existing files.
 * Reports missing and stale files as drift. NEVER writes or modifies any file.
 * NEVER throws on missing files - reports them as drift entries.
 */
export function checkManifestDrift(targets: readonly ManifestTarget[]): ManifestDriftResult {
  const entries: ManifestDriftEntry[] = [];
  for (const [path, content] of targets) {
    if (!existsSync(path)) {
      entries.push({
        path,
        kind: 'missing',
        message: `[gateway-manifest] DRIFT: ${path} is missing. Run scripts/generate-gateway-manifest.ts`
      });
      continue;
    }
    const existing = readFileSync(path, 'utf8');
    if (existing !== content) {
      entries.push({
        path,
        kind: 'stale',
        message: `[gateway-manifest] DRIFT: ${path} is stale. Run scripts/generate-gateway-manifest.ts`
      });
    }
  }
  for (const entry of entries) console.error(entry.message);
  return { drift: entries.length > 0, entries };
}

const FINAL_FILE_MODE = 0o644;
const TEMP_FILE_MODE = 0o600;

/**
 * Generate mode: atomically write all targets as a transaction.
 * 1. Reject duplicate final target paths.
 * 2. Reject existing final symlinks (POSIX rename replaces a symlink rather
 *    than following its target, but explicitly reject to surface the attack).
 * 3. Stage ALL temp files first (write content with exclusive wx + mode 0o600
 *    and a random transaction suffix).
 * 4. On ANY staging failure: delete all staged temps and throw before any
 *    final rename. All final files remain unchanged.
 * 5. After all temps are staged, rename each to its final path. Final files
 *    retain mode 0o644 (default; not made executable).
 */
export function writeManifestTargets(targets: readonly ManifestTarget[]): void {
  const seenFinals = new Set<string>();
  for (const [path] of targets) {
    if (seenFinals.has(path)) {
      throw new ManifestWriteError(
        'DUPLICATE_TARGET',
        path,
        `[gateway-manifest] duplicate final target path: ${path}`
      );
    }
    seenFinals.add(path);
    if (existsSync(path) && lstatSync(path).isSymbolicLink()) {
      throw new ManifestWriteError(
        'SYMLINK_TARGET',
        path,
        `[gateway-manifest] refusing to overwrite a symlink final target: ${path}`
      );
    }
  }

  const staged: string[] = [];
  try {
    for (const [path, content] of targets) {
      const tmp = `${path}.tmp-${randomUUID()}`;
      const dir = dirname(tmp);
      if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
      writeFileSync(tmp, content, { flag: 'wx', mode: TEMP_FILE_MODE });
      staged.push(tmp);
    }
  } catch (error) {
    for (const tmp of staged) rmSync(tmp, { force: true });
    throw error;
  }

  for (const [index, [path]] of targets.entries()) {
    renameSync(staged[index], path);
    chmodSync(path, FINAL_FILE_MODE);
  }
}
