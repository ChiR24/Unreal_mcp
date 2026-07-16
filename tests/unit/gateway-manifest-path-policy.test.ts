// tests/unit/gateway-manifest-path-policy.test.ts
// Pilot OUTPUT and catalog INPUT path policy: env-controlled paths cannot
// overwrite production/repo source paths or escape approved scratch areas.
// Symlink targets are rejected. Every test induces a REAL failure or proves a
// REAL contract - no false positives.
//
// Test repo roots are created OUTSIDE os.tmpdir() so the forbidden-root
// rejection can be exercised (a repo root under tmpdir is trivially within an
// allowed scratch root and would not trigger the forbidden check).

import { mkdirSync, mkdtempSync, rmSync, symlinkSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import {
  PilotPathError,
  type PilotPathErrorDetail,
  resolvePilotOutputDir,
  validatePilotCatalogPath
} from '../../scripts/gateway-manifest/path-policy.js';

const STAGING_DIR = resolve(process.cwd(), '.omo');

function makeNonTempRepo(): string {
  mkdirSync(STAGING_DIR, { recursive: true });
  return mkdtempSync(join(STAGING_DIR, 'repo-'));
}

function expectPathErrorCode(fn: () => void, code: PilotPathErrorDetail['code']): void {
  let caught: unknown;
  try {
    fn();
  } catch (e) {
    caught = e;
  }
  expect(caught).toBeInstanceOf(PilotPathError);
  if (caught instanceof PilotPathError) {
    expect(caught.code).toBe(code);
  }
}

describe('gateway-manifest pilot OUTPUT path policy', () => {
  it('accepts the default .omo/pilot-manifest output dir (no env)', () => {
    const root = makeNonTempRepo();
    try {
      mkdirSync(join(root, '.omo'), { recursive: true });
      const dir = resolvePilotOutputDir(undefined, root);
      expect(dir).toBe(resolve(root, '.omo/pilot-manifest'));
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('accepts an empty env string as default', () => {
    const root = makeNonTempRepo();
    try {
      mkdirSync(join(root, '.omo'), { recursive: true });
      const dir = resolvePilotOutputDir('', root);
      expect(dir).toBe(resolve(root, '.omo/pilot-manifest'));
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('accepts a path under <repo>/.omo', () => {
    const root = makeNonTempRepo();
    try {
      mkdirSync(join(root, '.omo'), { recursive: true });
      const dir = resolvePilotOutputDir('.omo/custom-pilot', root);
      expect(dir).toBe(resolve(root, '.omo/custom-pilot'));
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('accepts a path under the OS temp directory', () => {
    const root = makeNonTempRepo();
    try {
      const tempSub = join(tmpdir(), 'gw-pilot-accepted');
      const dir = resolvePilotOutputDir(tempSub, root);
      expect(dir).toBe(resolve(tempSub));
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('accepts a relative env path resolved against repo root under .omo', () => {
    const root = makeNonTempRepo();
    try {
      mkdirSync(join(root, '.omo'), { recursive: true });
      const dir = resolvePilotOutputDir('.omo/sub/pilot', root);
      expect(dir).toBe(resolve(root, '.omo/sub/pilot'));
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('rejects the repo root as output dir', () => {
    const root = makeNonTempRepo();
    try {
      expectPathErrorCode(() => resolvePilotOutputDir(root, root), 'PILOT_OUTPUT_DIR_FORBIDDEN');
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('rejects a production source path (src/)', () => {
    const root = makeNonTempRepo();
    try {
      expectPathErrorCode(() => resolvePilotOutputDir('src', root), 'PILOT_OUTPUT_DIR_FORBIDDEN');
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('rejects a plugin source path (plugins/)', () => {
    const root = makeNonTempRepo();
    try {
      expect(() => resolvePilotOutputDir('plugins', root)).toThrow(PilotPathError);
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('rejects an external path outside repo and temp', () => {
    const root = makeNonTempRepo();
    try {
      expectPathErrorCode(() => resolvePilotOutputDir('/etc/pilot-output', root), 'PILOT_OUTPUT_DIR_FORBIDDEN');
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('rejects a symlink under .omo that escapes to an external dir', () => {
    const root = makeNonTempRepo();
    try {
      mkdirSync(join(root, '.omo'), { recursive: true });
      const linkPath = join(root, '.omo', 'escape-link');
      symlinkSync('/etc', linkPath);
      expectPathErrorCode(() => resolvePilotOutputDir('.omo/escape-link/sub', root), 'PILOT_OUTPUT_DIR_SYMLINK_ESCAPE');
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('rejects a symlink under temp that escapes to an external dir', () => {
    const root = makeNonTempRepo();
    const tempLink = join(tmpdir(), `gw-temp-escape-${process.pid}-${Date.now()}`);
    try {
      symlinkSync('/etc', tempLink);
      expectPathErrorCode(() => resolvePilotOutputDir(tempLink, root), 'PILOT_OUTPUT_DIR_SYMLINK_ESCAPE');
    } finally {
      rmSync(root, { recursive: true, force: true });
      rmSync(tempLink, { force: true });
    }
  });
});

describe('gateway-manifest catalog INPUT path policy', () => {
  it('accepts a catalog path under the repo root', () => {
    const root = makeNonTempRepo();
    try {
      const validated = validatePilotCatalogPath('src/tools/catalog.json', root);
      expect(validated).toBe(resolve(root, 'src/tools/catalog.json'));
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('accepts a catalog path under the OS temp directory', () => {
    const root = makeNonTempRepo();
    try {
      const tempCatalog = join(tmpdir(), 'gw-catalog-accepted.json');
      const validated = validatePilotCatalogPath(tempCatalog, root);
      expect(validated).toBe(resolve(tempCatalog));
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('rejects an external catalog path outside repo and temp', () => {
    const root = makeNonTempRepo();
    try {
      expectPathErrorCode(() => validatePilotCatalogPath('/etc/passwd', root), 'PILOT_CATALOG_PATH_FORBIDDEN');
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });

  it('rejects a catalog path that is a symlink escaping to an external dir', () => {
    const root = makeNonTempRepo();
    try {
      mkdirSync(join(root, 'src'), { recursive: true });
      const linkPath = join(root, 'src', 'catalog-link.json');
      symlinkSync('/etc/passwd', linkPath);
      expectPathErrorCode(() => validatePilotCatalogPath('src/catalog-link.json', root), 'PILOT_CATALOG_PATH_SYMLINK_ESCAPE');
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  });
});
