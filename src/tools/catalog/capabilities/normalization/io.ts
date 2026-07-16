/**
 * Atomic file IO for the inventory artifact, plus stable serialization.
 */
import { existsSync, readFileSync, renameSync, writeFileSync } from 'node:fs';
import type { NormalizationInventory } from './types.js';
import { validateInventoryData } from './validate.js';

/** Deterministic 2-space serialization (no trailing whitespace variance). */
export function stableStringify(artifact: NormalizationInventory): string {
  return `${JSON.stringify(artifact, null, 2)}\n`;
}

export function artifactExists(path: string): boolean {
  return existsSync(path);
}

/** Read and fully validate a committed artifact from disk. */
export function readArtifact(path: string): NormalizationInventory {
  const text = readFileSync(path, 'utf8');
  return validateInventoryData(JSON.parse(text));
}

/** Write atomically (temp file + rename) so a crash never leaves a partial artifact. */
export function writeArtifactAtomic(
  path: string,
  artifact: NormalizationInventory,
): void {
  const tmp = `${path}.tmp`;
  writeFileSync(tmp, stableStringify(artifact), 'utf8');
  renameSync(tmp, path);
}
