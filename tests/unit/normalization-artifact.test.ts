/**
 * Focused unit tests for the Task 5 normalization inventory — determinism of
 * generation and byte-stability of the committed artifact.
 *
 * These tests verify that regenerated inventory output is deterministic across
 * runs and that the committed artifact on disk matches the regenerated output
 * byte-for-byte and validates against the schema.
 */

import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import {
  generateInventory,
} from '../../src/tools/catalog/capabilities/normalization/generate.js';
import {
  readArtifact,
  stableStringify,
} from '../../src/tools/catalog/capabilities/normalization/io.js';

const here = dirname(fileURLToPath(import.meta.url));
const ARTIFACT_PATH = resolve(
  here,
  '../../src/tools/catalog/capabilities/normalization-inventory.json',
);

describe('determinism and committed artifact', () => {
  it('generates byte-identical output across two runs', () => {
    const a = stableStringify(generateInventory());
    const b = stableStringify(generateInventory());
    expect(a).toBe(b);
  });

  it('matches the committed artifact byte-for-byte', () => {
    const committed = readFileSync(ARTIFACT_PATH, 'utf8');
    const regenerated = stableStringify(generateInventory());
    expect(regenerated).toBe(committed);
  });

  it('validates the committed artifact on disk', () => {
    const inv = readArtifact(ARTIFACT_PATH);
    expect(inv.occurrences.length).toBe(1335);
    expect(inv.schemaVersion).toBe('task5.normalization.v1');
  });
});
