// scripts/lib/generated-files.ts
// Write the committed generated files, or report the ones that drifted. A
// failed or partial write is fixed by rerunning the generator.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname } from 'node:path';

export type GeneratedTarget = readonly [path: string, content: string];

const upToDate = ([path, content]: GeneratedTarget): boolean =>
  existsSync(path) && readFileSync(path, 'utf8') === content;

/** Writes each target whose content changed (unchanged files keep their mtime, so UBT skips them). */
export function writeGeneratedFiles(targets: readonly GeneratedTarget[]): void {
  for (const target of targets) {
    if (upToDate(target)) continue;
    mkdirSync(dirname(target[0]), { recursive: true });
    writeFileSync(target[0], target[1]);
  }
}

/** Logs each missing or stale target with the command that regenerates it; true when any drifted. */
export function reportDrift(targets: readonly GeneratedTarget[], label: string, fix: string): boolean {
  const stale = targets.filter((target) => !upToDate(target));
  for (const [path] of stale) console.error(`[${label}] DRIFT: ${path} is missing or stale. Run ${fix}`);
  return stale.length > 0;
}
