// tests/unit/gateway-manifest-catalog-size.test.ts
// Catalog size cap: oversized input fails typed before readFileSync.
// Every test induces a REAL failure or proves a REAL contract.

import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import {
  loadPilotCatalogRaw,
  PILOT_CATALOG_MAX_BYTES,
  PilotCatalogError
} from '../../scripts/gateway-manifest/load.js';
import { fileSizeBytes } from '../../scripts/gateway-manifest/path-policy.js';

function makeTempDir(): string {
  return mkdtempSync(join(tmpdir(), 'gw-cat-size-'));
}

describe('gateway-manifest catalog size cap', () => {
  it('rejects a catalog larger than 50 MiB with a typed TOO_LARGE error', () => {
    const dir = makeTempDir();
    try {
      const catalogPath = join(dir, 'huge.json');
      const fiftyMiBPlusOne = PILOT_CATALOG_MAX_BYTES + 1;
      const buf = Buffer.alloc(fiftyMiBPlusOne, 0x20);
      writeFileSync(catalogPath, buf);
      let caught: unknown;
      try {
        loadPilotCatalogRaw(catalogPath);
      } catch (e) {
        caught = e;
      }
      expect(caught).toBeInstanceOf(PilotCatalogError);
      if (caught instanceof PilotCatalogError) {
        expect(caught.code).toBe('PILOT_CATALOG_TOO_LARGE');
        expect(caught.catalogPath).toBe(catalogPath);
      }
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('accepts a small valid catalog under the size cap', () => {
    const dir = makeTempDir();
    try {
      const catalogPath = join(dir, 'small.json');
      writeFileSync(catalogPath, '{"ok": true}');
      const raw = loadPilotCatalogRaw(catalogPath);
      expect(raw).toEqual({ ok: true });
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('reports file size in bytes for an existing regular file', () => {
    const dir = makeTempDir();
    try {
      const filePath = join(dir, 'sized.txt');
      writeFileSync(filePath, '12345');
      expect(fileSizeBytes(filePath)).toBe(5);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('PILOT_CATALOG_MAX_BYTES is exactly 50 MiB', () => {
    expect(PILOT_CATALOG_MAX_BYTES).toBe(50 * 1024 * 1024);
  });
});
