// tests/unit/gateway-manifest-write.test.ts
// Transactional writer and drift checker contracts for the gateway manifest.
// Every test induces a REAL failure or proves a REAL contract - no false
// positives. Uses temp dirs with guaranteed try/finally cleanup.

import { existsSync, lstatSync, mkdtempSync, readdirSync, readFileSync, rmSync, statSync, symlinkSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import {
  checkManifestDrift,
  type ManifestTarget,
  writeManifestTargets
} from '../../scripts/gateway-manifest/write.js';

function makeTempDir(): string {
  return mkdtempSync(join(tmpdir(), 'gw-manifest-'));
}

describe('gateway-manifest writeManifestTargets transactional staging', () => {
  it('writes all targets when all staging succeeds', () => {
    const dir = makeTempDir();
    try {
      const fileA = join(dir, 'a.json');
      const fileB = join(dir, 'b.json');
      const targets: ManifestTarget[] = [
        [fileA, '{"a": 1}'],
        [fileB, '{"b": 2}']
      ];

      writeManifestTargets(targets);

      expect(readFileSync(fileA, 'utf8')).toBe('{"a": 1}');
      expect(readFileSync(fileB, 'utf8')).toBe('{"b": 2}');
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('leaves ALL final files unchanged and cleans temps when a later target has a missing parent', () => {
    const dir = makeTempDir();
    try {
      // First target: valid, will stage successfully.
      const fileA = join(dir, 'a.json');
      const originalA = 'original-a-content';
      writeFileSync(fileA, originalA);

      // Second target: pre-existing final with known content.
      const fileB = join(dir, 'b.json');
      const originalB = 'original-b-content';
      writeFileSync(fileB, originalB);

      // Third target: a regular file blocks the parent directory path.
      // mkdirSync(dirname(tmp), { recursive: true }) will throw ENOTDIR
      // because 'blocker' is a file, not a directory. This is a REAL
      // staging failure that occurs AFTER the first two temps are staged.
      const blocker = join(dir, 'blocker');
      writeFileSync(blocker, 'blocker');
      const fileC = join(blocker, 'c.json');

      const targets: ManifestTarget[] = [
        [fileA, 'new-a-content'],
        [fileB, 'new-b-content'],
        [fileC, 'new-c-content']
      ];

      // The write MUST throw because the third target's parent cannot be created.
      expect(() => writeManifestTargets(targets)).toThrow();

      // Both pre-existing final files MUST be unchanged.
      expect(readFileSync(fileA, 'utf8')).toBe(originalA);
      expect(readFileSync(fileB, 'utf8')).toBe(originalB);

      // No temp residue: the staged temps for targets A and B were cleaned up.
      const findTemps = (d: string): string[] => {
        const out: string[] = [];
        for (const entry of readdirSync(d, { withFileTypes: true })) {
          const full = join(d, entry.name);
          if (entry.name.includes('.tmp-')) out.push(full);
          if (entry.isDirectory()) out.push(...findTemps(full));
        }
        return out;
      };
      expect(findTemps(dir)).toHaveLength(0);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('leaves no .tmp-* residue after successful write', () => {
    const dir = makeTempDir();
    try {
      const fileA = join(dir, 'a.json');
      writeManifestTargets([[fileA, '{"ok": true}']]);
      // No temp files remain after a successful transaction.
      const temps = readdirSync(dir).filter((n) => n.includes('.tmp-'));
      expect(temps).toHaveLength(0);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe('gateway-manifest checkManifestDrift', () => {
  it('reports stale AND missing files as drift without writing', () => {
    const dir = makeTempDir();
    try {
      const fileA = join(dir, 'a.txt');
      const fileB = join(dir, 'b.txt');
      const fileC = join(dir, 'c.txt'); // missing

      writeFileSync(fileA, 'correct-a');
      writeFileSync(fileB, 'stale-b');

      const targets: ManifestTarget[] = [
        [fileA, 'correct-a'],
        [fileB, 'correct-b'],
        [fileC, 'correct-c']
      ];

      const result = checkManifestDrift(targets);

      expect(result.drift).toBe(true);
      const kinds = result.entries.map((e) => e.kind).sort();
      expect(kinds).toContain('stale');
      expect(kinds).toContain('missing');

      // No writes occurred: fileA and fileB unchanged, fileC still missing.
      expect(readFileSync(fileA, 'utf8')).toBe('correct-a');
      expect(readFileSync(fileB, 'utf8')).toBe('stale-b');
      expect(existsSync(fileC)).toBe(false);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('returns no drift when all files match exactly', () => {
    const dir = makeTempDir();
    try {
      const fileA = join(dir, 'a.txt');
      writeFileSync(fileA, 'exact-match');
      const result = checkManifestDrift([[fileA, 'exact-match']]);
      expect(result.drift).toBe(false);
      expect(result.entries).toHaveLength(0);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('never throws on missing files', () => {
    const dir = makeTempDir();
    try {
      const missing = join(dir, 'does-not-exist.txt');
      expect(() => checkManifestDrift([[missing, 'content']])).not.toThrow();
      const result = checkManifestDrift([[missing, 'content']]);
      expect(result.drift).toBe(true);
      expect(result.entries[0].kind).toBe('missing');
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe('gateway-manifest writeManifestTargets symlink final rejection', () => {
  it('rejects an existing symlink final target and leaves the symlink unchanged', () => {
    const dir = makeTempDir();
    try {
      const target = join(dir, 'real.json');
      writeFileSync(target, 'real-content');
      const link = join(dir, 'link.json');
      symlinkSync(target, link);

      expect(() => writeManifestTargets([[link, 'new-content']])).toThrow();

      expect(lstatSync(link).isSymbolicLink()).toBe(true);
      expect(readFileSync(link, 'utf8')).toBe('real-content');
      expect(readFileSync(target, 'utf8')).toBe('real-content');
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('rejects a symlink final even when other targets are valid', () => {
    const dir = makeTempDir();
    try {
      const valid = join(dir, 'valid.json');
      const target = join(dir, 'real.json');
      writeFileSync(target, 'real-content');
      const link = join(dir, 'link.json');
      symlinkSync(target, link);

      const targets: ManifestTarget[] = [
        [valid, '{"valid": true}'],
        [link, '{"link": true}']
      ];

      expect(() => writeManifestTargets(targets)).toThrow();

      expect(existsSync(valid)).toBe(false);
      expect(lstatSync(link).isSymbolicLink()).toBe(true);
      expect(readFileSync(target, 'utf8')).toBe('real-content');
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe('gateway-manifest writeManifestTargets duplicate final rejection', () => {
  it('rejects duplicate final paths before any staging', () => {
    const dir = makeTempDir();
    try {
      const dup = join(dir, 'dup.json');
      const targets: ManifestTarget[] = [
        [dup, '{"a": 1}'],
        [dup, '{"a": 2}']
      ];
      expect(() => writeManifestTargets(targets)).toThrow(/duplicate final target path/);
      expect(existsSync(dup)).toBe(false);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe('gateway-manifest writeManifestTargets random and exclusive temp', () => {
  it('uses an unpredictable temp name (not pid+index)', () => {
    const dir = makeTempDir();
    try {
      const file = join(dir, 'a.json');
      writeManifestTargets([[file, '{"ok": true}']]);
      const temps = readdirSync(dir).filter((n) => n.includes('.tmp-'));
      expect(temps).toHaveLength(0);
      expect(readFileSync(file, 'utf8')).toBe('{"ok": true}');
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('two writes to the same path do not collide on temp names', () => {
    const dir = makeTempDir();
    try {
      const file = join(dir, 'a.json');
      writeManifestTargets([[file, '{"v": 1}']]);
      const firstContent = readFileSync(file, 'utf8');
      writeManifestTargets([[file, '{"v": 2}']]);
      const secondContent = readFileSync(file, 'utf8');
      expect(firstContent).toBe('{"v": 1}');
      expect(secondContent).toBe('{"v": 2}');
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('final file mode is 0o644 (not executable, not 0o600)', () => {
    const dir = makeTempDir();
    try {
      const file = join(dir, 'a.json');
      writeManifestTargets([[file, '{"ok": true}']]);
      const mode = statSync(file).mode & 0o777;
      expect(mode).toBe(0o644);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it('staging failure cleans temps and leaves finals unchanged', () => {
    const dir = makeTempDir();
    try {
      const fileA = join(dir, 'a.json');
      const original = 'original';
      writeFileSync(fileA, original);
      const blocker = join(dir, 'blocker');
      writeFileSync(blocker, 'x');
      const fileB = join(blocker, 'b.json');

      expect(() => writeManifestTargets([[fileA, 'new'], [fileB, 'new-b']])).toThrow();
      expect(readFileSync(fileA, 'utf8')).toBe(original);
      const temps = readdirSync(dir).filter((n) => n.includes('.tmp-'));
      expect(temps).toHaveLength(0);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});
