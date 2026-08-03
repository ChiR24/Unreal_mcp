// tests/unit/evidence-oracles/fixture-namespace.test.ts
// Task 50 — OWNERSHIP, DOUBLE CLEANUP, INTERRUPTION RECOVERY.
//
// The project this suite runs against is somebody's working content. It already
// holds BP_Probe_1784051174, GapProofBP, SweepTestBP, QAIdemProbe and two empty
// `task49-*` folders from an earlier lane. "Clean up /Game/MCPTest" destroys all
// of that, and Task 49 found six unrelated `node dist/cli.js` processes on this
// host that were correctly left alone. These tests exist to make that restraint
// structural rather than a habit somebody has to remember at 2am.
//
// Every case uses REAL directories under the owned /tmp/opencode/task-50 root,
// including the symlink and traversal cases — a mocked path module would happily
// agree with a containment check that a real filesystem defeats.

import { existsSync, mkdirSync, readFileSync, rmSync, symlinkSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';

import {
  CONTENT_ROOT,
  FixtureNamespace,
  MANIFEST_FILE,
  allocateRunId,
  findInterruptedRuns,
  isInsideGamePath,
  isStrictlyInside,
  reclaimInterruptedRun,
  residualContent,
} from './fixture-namespace.mjs';

const SANDBOX = join('/tmp/opencode/task-50', `fixture-spec-${process.pid}`);
const TEMP_PARENT = join(SANDBOX, 'temp');
const PROJECT = join(SANDBOX, 'Project');

function makeNamespace(runId = 'run-alpha'): FixtureNamespace {
  return new FixtureNamespace({ runId, projectRoot: PROJECT, tempRoot: TEMP_PARENT });
}

beforeEach(() => {
  mkdirSync(TEMP_PARENT, { recursive: true });
  mkdirSync(join(PROJECT, 'Content'), { recursive: true });
});
afterEach(() => { rmSync(SANDBOX, { recursive: true, force: true }); });

describe('Task 50 — run ids cannot collide', () => {
  it('is time-ordered and random-suffixed', () => {
    const at = new Date('2026-07-27T18:30:00.000Z');
    const id = allocateRunId({ now: () => at, random: () => Buffer.from([0xde, 0xad, 0xbe, 0xef]) });
    expect(id).toBe('t50-20260727T183000Z-deadbeef');
  });

  it('two ids minted in the SAME instant still differ — a namespace collision is how one run reads another\'s leftover as proof', () => {
    const at = new Date('2026-07-27T18:30:00.000Z');
    const ids = new Set(Array.from({ length: 200 }, () => allocateRunId({ now: () => at })));
    expect(ids.size).toBe(200);
  });
});

describe('Task 50 — containment is a real-path check, not a prefix match', () => {
  it('accepts a genuine child', () => {
    expect(isStrictlyInside(TEMP_PARENT, join(TEMP_PARENT, 'run-a/file.txt')).owned).toBe(true);
  });

  it('REFUSES the root itself — removing the shared root is never "cleaning up my run"', () => {
    expect(isStrictlyInside(TEMP_PARENT, TEMP_PARENT)).toMatchObject({ owned: false, reason: 'IS_THE_ROOT_ITSELF' });
  });

  it('REFUSES a sibling whose name merely starts the same way', () => {
    expect(isStrictlyInside(join(TEMP_PARENT, 'run-1'), join(TEMP_PARENT, 'run-11')).owned).toBe(false);
  });

  it('REFUSES a `..` escape that a prefix test would accept', () => {
    expect(isStrictlyInside(join(TEMP_PARENT, 'run-a'), join(TEMP_PARENT, 'run-a/../../elsewhere')).owned).toBe(false);
  });

  it('REFUSES a symlink pointing out of the owned tree, even though its path is inside', () => {
    const owned = join(TEMP_PARENT, 'run-sym');
    const outside = join(SANDBOX, 'not-mine');
    mkdirSync(owned, { recursive: true });
    mkdirSync(outside, { recursive: true });
    writeFileSync(join(outside, 'precious.uasset'), 'someone else work', { mode: 0o600 });
    symlinkSync(outside, join(owned, 'escape'));
    const verdict = isStrictlyInside(owned, join(owned, 'escape'));
    expect(verdict.owned).toBe(false);
    expect(existsSync(join(outside, 'precious.uasset'))).toBe(true);
  });

  it('the same rules apply to /Game object paths, which have no filesystem to realpath', () => {
    expect(isInsideGamePath('/Game/MCPTest/run-1', '/Game/MCPTest/run-1/M_A').owned).toBe(true);
    expect(isInsideGamePath('/Game/MCPTest/run-1', '/Game/MCPTest/run-11/M_A').owned).toBe(false);
    expect(isInsideGamePath('/Game/MCPTest/run-1', '/Game/MCPTest/run-1')).toMatchObject({ owned: false, reason: 'IS_THE_ROOT_ITSELF' });
    expect(isInsideGamePath('/Game/MCPTest/run-1', '/Game/MCPTest/run-1/../other/M_A')).toMatchObject({ owned: false, reason: 'PATH_TRAVERSAL' });
    expect(isInsideGamePath('/Game/MCPTest/run-1', '/Game/Existing/BP_Probe_1784051174').owned).toBe(false);
  });
});

describe('Task 50 — a fixture that cannot be declared cannot be deleted', () => {
  it('declares owned content and owned files', () => {
    const namespace = makeNamespace();
    namespace.open();
    expect(namespace.gameRoot).toBe(`${CONTENT_ROOT}/run-alpha`);
    namespace.declare('content', `${namespace.gameRoot}/M_Mine`);
    namespace.declare('file', join(namespace.tempRoot, 'scratch.json'));
    expect(namespace.declared).toHaveLength(2);
  });

  it('THROWS on a content path belonging to the shared project, naming what it does own', () => {
    const namespace = makeNamespace();
    namespace.open();
    expect(() => namespace.declare('content', '/Game/MCPTest/BP_Test'))
      .toThrowError(/REFUSING to declare unowned content .*OUTSIDE_OWNED_ROOT/u);
  });

  it('THROWS on a host path outside the owned temp namespace', () => {
    const namespace = makeNamespace();
    namespace.open();
    expect(() => namespace.declare('file', '/tmp/opencode/big_1.json')).toThrowError(/UNOWNED|REFUSING/u);
  });

  it('removeOwnedFile REFUSES an unowned path and leaves it on disk', () => {
    const namespace = makeNamespace();
    namespace.open();
    const foreign = join(SANDBOX, 'foreign.json');
    writeFileSync(foreign, 'not mine', { mode: 0o600 });
    const receipt = namespace.removeOwnedFile(foreign);
    expect(receipt.removed).toBe(false);
    expect(receipt.reason).toContain('REFUSED');
    expect(existsSync(foreign)).toBe(true);
  });

  it('removeOwnedFile removes what this run really made', () => {
    const namespace = makeNamespace();
    namespace.open();
    const mine = join(namespace.tempRoot, 'mine.json');
    writeFileSync(mine, '{}', { mode: 0o600 });
    expect(namespace.removeOwnedFile(mine).removed).toBe(true);
    expect(existsSync(mine)).toBe(false);
  });
});

describe('Task 50 — the baseline is taken BEFORE anything is created', () => {
  it('records the pre-run digest of the content namespace at open()', () => {
    const namespace = makeNamespace();
    const manifest = namespace.open();
    expect(manifest.baselineDigest).toMatch(/^[0-9a-f]{64}$/u);
    expect(manifest.baselineFileCount).toBe(0);
  });

  it('a namespace that ALREADY holds content records that fact rather than assuming empty', () => {
    const namespace = makeNamespace('run-dirty');
    mkdirSync(namespace.diskRoot, { recursive: true });
    writeFileSync(join(namespace.diskRoot, 'Leftover.uasset'), 'from an interrupted run', { mode: 0o600 });
    const manifest = namespace.open();
    expect(manifest.baselineFileCount).toBe(1);
    expect(residualContent(namespace).map((entry) => entry.objectPath)).toEqual([`${namespace.gameRoot}/Leftover`]);
  });
});

describe('Task 50 — cleanup is VERIFIED, and double cleanup is a no-op', () => {
  it('reports contentRestored:false when a fixture survives — regardless of what any delete said', () => {
    const namespace = makeNamespace();
    namespace.open();
    mkdirSync(namespace.diskRoot, { recursive: true });
    writeFileSync(join(namespace.diskRoot, 'M_Leaked.uasset'), 'still here', { mode: 0o600 });
    const receipt = namespace.close();
    expect(receipt.contentRestored).toBe(false);
    expect(receipt.residualContentFiles).toBe(1);
  });

  it('POSITIVE CONTROL: reports contentRestored:true when the namespace really came back to baseline', () => {
    const namespace = makeNamespace();
    namespace.open();
    mkdirSync(namespace.diskRoot, { recursive: true });
    const made = join(namespace.diskRoot, 'M_Temp.uasset');
    writeFileSync(made, 'transient', { mode: 0o600 });
    rmSync(made);
    const receipt = namespace.close();
    expect(receipt.contentRestored).toBe(true);
    expect(receipt.residualContentFiles).toBe(0);
  });

  it('removes the now-EMPTY content directory too — a folder per run is residue that accumulates', () => {
    const namespace = makeNamespace();
    namespace.open();
    mkdirSync(namespace.diskRoot, { recursive: true });
    const made = join(namespace.diskRoot, 'M_Temp.uasset');
    writeFileSync(made, 'transient', { mode: 0o600 });
    rmSync(made);
    const receipt = namespace.close();
    expect(receipt.emptyDirectoriesRemoved).toBe(1);
    expect(receipt.contentDirectoryRemains).toBe(false);
    expect(existsSync(join(PROJECT, 'Content'))).toBe(true);
  });

  it('does NOT remove the content directory while any file remains in it', () => {
    const namespace = makeNamespace();
    namespace.open();
    mkdirSync(namespace.diskRoot, { recursive: true });
    writeFileSync(join(namespace.diskRoot, 'M_Leaked.uasset'), 'survivor', { mode: 0o600 });
    const receipt = namespace.close();
    expect(receipt.emptyDirectoriesRemoved).toBe(0);
    expect(receipt.contentDirectoryRemains).toBe(true);
    expect(receipt.contentRestored).toBe(false);
  });

  it('releases the owned temp namespace and says so from an independent existence check', () => {
    const namespace = makeNamespace();
    namespace.open();
    writeFileSync(join(namespace.tempRoot, 'scratch.json'), '{}', { mode: 0o600 });
    const receipt = namespace.close();
    expect(receipt.tempReleased).toBe(true);
    expect(existsSync(namespace.tempRoot)).toBe(false);
  });

  it('DOUBLE CLEANUP is idempotent: the second close removes nothing and still reports honestly', () => {
    const namespace = makeNamespace();
    namespace.open();
    writeFileSync(join(namespace.tempRoot, 'scratch.json'), '{}', { mode: 0o600 });
    const first = namespace.close();
    const second = namespace.close();
    expect(first.tempReleased).toBe(true);
    expect(second.tempReleased).toBe(true);
    expect(second.fileReceipts).toHaveLength(0);
    expect(second.contentRestored).toBe(true);
  });

  it('double cleanup cannot delete whatever LATER occupies the released path', () => {
    const namespace = makeNamespace();
    namespace.open();
    namespace.close();
    // Somebody else takes the name after we released it. The manifest is the
    // ownership token and it is gone, so this directory is no longer ours.
    mkdirSync(namespace.tempRoot, { recursive: true });
    writeFileSync(join(namespace.tempRoot, 'someone-elses.json'), 'not ours', { mode: 0o600 });
    const second = namespace.close();
    expect(existsSync(join(namespace.tempRoot, 'someone-elses.json'))).toBe(true);
    expect(second.alreadyClosed).toBe(true);
    expect(second.fileReceipts).toHaveLength(0);
    // Honest on both counts: we DID release ours, and something is there now.
    expect(second.tempReleased).toBe(true);
    expect(second.tempPathOccupied).toBe(true);
  });
});

describe('Task 50 — interruption recovery reclaims ONLY provably dead runs', () => {
  it('finds an abandoned namespace whose owner is gone', () => {
    const namespace = new FixtureNamespace({ runId: 'run-dead', projectRoot: PROJECT, tempRoot: TEMP_PARENT, pid: 999_999 });
    namespace.open();
    writeFileSync(join(namespace.tempRoot, 'half-written.json'), '{"interrupted":true}', { mode: 0o600 });
    const scan = findInterruptedRuns({ root: TEMP_PARENT });
    expect(scan.reclaimable.map((entry) => entry.runId)).toEqual(['run-dead']);
    expect(scan.active).toHaveLength(0);
  });

  it('LEAVES ALONE a namespace whose owning process is still alive — that is another lane\'s run', () => {
    const namespace = new FixtureNamespace({ runId: 'run-live', projectRoot: PROJECT, tempRoot: TEMP_PARENT, pid: process.pid });
    namespace.open();
    const scan = findInterruptedRuns({ root: TEMP_PARENT, selfPid: -1 });
    expect(scan.active.map((entry) => entry.runId)).toEqual(['run-live']);
    expect(scan.reclaimable).toHaveLength(0);
  });

  it('a RECYCLED pid does not protect an abandoned run: the start time must match too', () => {
    const namespace = new FixtureNamespace({ runId: 'run-recycled', projectRoot: PROJECT, tempRoot: TEMP_PARENT, pid: process.pid });
    namespace.open();
    // Rewrite the manifest as an OLDER process that happened to hold this pid.
    const manifest = JSON.parse(readFileSync(join(namespace.tempRoot, MANIFEST_FILE), 'utf8'));
    manifest.pidStartTicks = Number(manifest.pidStartTicks) - 1;
    writeFileSync(join(namespace.tempRoot, MANIFEST_FILE), JSON.stringify(manifest), { mode: 0o600 });
    const scan = findInterruptedRuns({ root: TEMP_PARENT, selfPid: -1 });
    expect(scan.reclaimable.map((entry) => entry.runId)).toEqual(['run-recycled']);
  });

  it('a directory with NO manifest is never touched — it is not ours to reason about', () => {
    const stranger = join(TEMP_PARENT, 'someone-elses-work');
    mkdirSync(stranger, { recursive: true });
    writeFileSync(join(stranger, 'data.json'), 'precious', { mode: 0o600 });
    const scan = findInterruptedRuns({ root: TEMP_PARENT });
    expect(scan.reclaimable).toHaveLength(0);
    expect(scan.unreadable).toContain(stranger);
    expect(existsSync(join(stranger, 'data.json'))).toBe(true);
  });

  it('a manifest from a DIFFERENT suite is not reclaimable', () => {
    const foreign = join(TEMP_PARENT, 'other-suite');
    mkdirSync(foreign, { recursive: true });
    writeFileSync(join(foreign, MANIFEST_FILE), JSON.stringify({ suite: 'task-51', runId: 'x', pid: 999_999 }), { mode: 0o600 });
    expect(findInterruptedRuns({ root: TEMP_PARENT }).reclaimable).toHaveLength(0);
  });

  it('reclaim removes exactly the tempRoot the manifest names', () => {
    const namespace = new FixtureNamespace({ runId: 'run-reclaim', projectRoot: PROJECT, tempRoot: TEMP_PARENT, pid: 999_998 });
    namespace.open();
    writeFileSync(join(namespace.tempRoot, 'stale.json'), '{}', { mode: 0o600 });
    const [manifest] = findInterruptedRuns({ root: TEMP_PARENT }).reclaimable;
    const receipt = reclaimInterruptedRun(manifest, { root: TEMP_PARENT });
    expect(receipt).toMatchObject({ reclaimed: true, runId: 'run-reclaim' });
    expect(existsSync(namespace.tempRoot)).toBe(false);
    expect(existsSync(TEMP_PARENT)).toBe(true);
  });

  it('reclaim REFUSES a manifest that points outside the shared root — a tampered manifest is not authority', () => {
    const outside = join(SANDBOX, 'critical');
    mkdirSync(outside, { recursive: true });
    writeFileSync(join(outside, 'keep.txt'), 'keep', { mode: 0o600 });
    const receipt = reclaimInterruptedRun(
      { runId: 'evil', pid: 1, pidStartTicks: null, tempRoot: outside, diskRoot: '', gameRoot: '', openedAt: '', manifestPath: '' },
      { root: TEMP_PARENT },
    );
    expect(receipt.reclaimed).toBe(false);
    expect(receipt.reason).toContain('REFUSED');
    expect(existsSync(join(outside, 'keep.txt'))).toBe(true);
  });

  it('a run that was interrupted mid-fixture can be resumed: the manifest still names its content root', () => {
    const namespace = new FixtureNamespace({ runId: 'run-resume', projectRoot: PROJECT, tempRoot: TEMP_PARENT, pid: 999_997 });
    namespace.open();
    mkdirSync(namespace.diskRoot, { recursive: true });
    writeFileSync(join(namespace.diskRoot, 'M_Orphan.uasset'), 'made before the kill', { mode: 0o600 });
    const [manifest] = findInterruptedRuns({ root: TEMP_PARENT }).reclaimable;
    // The recovery path knows WHICH content to ask the editor to delete, and it
    // is exactly what that manifest declared — never a wildcard sweep.
    expect(manifest.gameRoot).toBe('/Game/MCPTest/run-resume');
    expect(residualContent(namespace).map((entry) => entry.objectPath)).toEqual(['/Game/MCPTest/run-resume/M_Orphan']);
  });
});
