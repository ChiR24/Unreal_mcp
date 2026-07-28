// Task 46 remediation - the staleness hole in the live matrix gate.
//
// The probe drives `node dist/cli.js` for the stdio transport. Nothing made
// dist/ current, so run 1 reported two HIGH findings - STALE_STATE downgraded
// (F3) and options.preview silently ignored (F6) - that were true of a dist/
// built three hours BEFORE those fixes landed and false of the working tree.
// A gate that emits false HIGH findings is worse than no gate: it burns
// reviewer trust and sends people to fix bugs that do not exist.
//
// Two halves are pinned here, because either alone leaves the hole open:
//   1. the freshness check itself must actually distinguish stale / missing /
//      current, and must not cry stale over a file the build never reads;
//   2. the probe must REFUSE - loudly, before it opens either transport and
//      before it writes any report - when the dist/ it is about to spawn does
//      not match the working tree.
//
// Case 6 is the regression test proper: it runs the REAL probe against a
// deliberately staled tree and fails if the probe gets as far as producing a
// result. It asserts the refusal by its message, not merely by a non-zero exit,
// because before the fix the probe also exited non-zero in that tree - for the
// unrelated reason that the registry was missing. A test that accepted any
// failure would have passed against the defect.

import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, rmSync, utimesSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

import { afterAll, describe, expect, it } from 'vitest';

import { BUILD_OUTPUT_ENTRY, checkDistFreshness, distFreshnessMessage } from './dist-freshness.mjs';

const PROBE = resolve(process.cwd(), 'scripts/qa/task46-cross-transport-matrix.mjs');

const created: string[] = [];

/** A throwaway project root shaped like this repo: a src/ tree and a dist/ build. */
function fixture(): string {
  const dir = mkdtempSync(join(tmpdir(), 'task46-freshness-'));
  created.push(dir);
  mkdirSync(join(dir, 'src', 'server', 'gateway'), { recursive: true });
  mkdirSync(join(dir, 'dist'), { recursive: true });
  return dir;
}

/** Absolute control over mtimes; wall-clock ordering is not reliable at this resolution. */
function write(path: string, atSeconds: number): void {
  writeFileSync(path, '// fixture\n');
  utimesSync(path, atSeconds, atSeconds);
}

const EARLY = 1_700_000_000;
const LATE = 1_700_009_999;

afterAll(() => {
  for (const dir of created) rmSync(dir, { recursive: true, force: true });
});

describe('task 46 - dist freshness detection', () => {
  it('reports STALE_BUILD and names the source that outran the build', () => {
    const root = fixture();
    write(join(root, 'dist', 'cli.js'), EARLY);
    write(join(root, 'src', 'server', 'gateway', 'gateway-execute.ts'), LATE);

    const result = checkDistFreshness(root);

    expect(result.fresh).toBe(false);
    expect(result.reason).toBe('STALE_BUILD');
    expect(result.newestInput).toBe('src/server/gateway/gateway-execute.ts');
    expect(result.staleByMs).toBeGreaterThan(0);
  });

  it('reports MISSING_BUILD when the artifact the probe would spawn does not exist', () => {
    const root = fixture();
    write(join(root, 'src', 'index.ts'), EARLY);

    const result = checkDistFreshness(root);

    expect(result.fresh).toBe(false);
    expect(result.reason).toBe('MISSING_BUILD');
    expect(result.entry).toBe(BUILD_OUTPUT_ENTRY);
  });

  it('accepts a build newer than every source it was compiled from', () => {
    const root = fixture();
    write(join(root, 'src', 'server', 'gateway', 'gateway-execute.ts'), EARLY);
    write(join(root, 'dist', 'cli.js'), LATE);

    expect(checkDistFreshness(root)).toMatchObject({ fresh: true, reason: 'FRESH' });
  });

  it('does not call a build stale over a file tsconfig excludes from it', () => {
    // tsconfig.json excludes **/*.test.ts and **/*.spec.ts, and dist/ contains
    // zero compiled test files. Treating an edited colocated test as a build
    // input would report a stale dist for a change that cannot alter what
    // dist/cli.js does - the kind of false alarm that gets a gate bypassed.
    const root = fixture();
    write(join(root, 'src', 'server', 'gateway', 'gateway-execute.ts'), EARLY);
    write(join(root, 'dist', 'cli.js'), LATE);
    write(join(root, 'src', 'server', 'gateway', 'gateway-execute.test.ts'), LATE + 500);

    expect(checkDistFreshness(root).fresh).toBe(true);
  });

  it('explains the refusal with both timestamps, the offending file and the remedy', () => {
    const root = fixture();
    write(join(root, 'dist', 'cli.js'), EARLY);
    write(join(root, 'src', 'index.ts'), LATE);

    const message = distFreshnessMessage(checkDistFreshness(root));

    expect(message).toContain('src/index.ts');
    expect(message).toContain(BUILD_OUTPUT_ENTRY);
    expect(message).toContain('npm run build');
    expect(message).toContain(new Date(EARLY * 1000).toISOString());
    expect(message).toContain(new Date(LATE * 1000).toISOString());
  });
});

describe('task 46 - the probe refuses to run against a stale build', () => {
  it('exits with the staleness refusal and writes no report', () => {
    const root = fixture();
    const out = join(root, 'report.json');
    write(join(root, 'dist', 'cli.js'), EARLY);
    write(join(root, 'src', 'server', 'gateway', 'gateway-execute.ts'), LATE);

    // cwd, not the probe's own location: the probe resolves `dist/cli.js` and
    // its registry relative to cwd, so the freshness check must judge the exact
    // artifact this invocation would have spawned.
    const run = spawnSync(process.execPath, [PROBE, '--out', out], {
      cwd: root,
      encoding: 'utf8',
      timeout: 20_000,
    });

    expect(run.status).not.toBe(0);
    expect(run.stderr).toContain('STALE_BUILD');
    expect(run.stderr).toContain('src/server/gateway/gateway-execute.ts');
    expect(run.stderr).toContain('npm run build');
    // Refusing means refusing before any observation exists to be believed.
    expect(existsSync(out)).toBe(false);
  }, 30_000);
});
