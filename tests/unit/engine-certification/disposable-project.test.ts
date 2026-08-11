// tests/unit/engine-certification/disposable-project.test.ts
// Task 52 — the ownership guard is the only thing standing between a
// certification run and somebody else's work.
//
// This run compiles, launches and then DELETES a tree, on a machine that also
// holds /data/Game/MCPtest, six engine installs and whatever the other lanes are
// doing. The guard therefore gets adversarial tests, not smoke tests: the cases
// below are the ways a path that looks owned turns out not to be.

import { afterEach, describe, expect, it } from 'vitest';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:net';
import { tmpdir } from 'node:os';
import { join, resolve, sep } from 'node:path';

import {
  DisposableWorkspace,
  MANIFEST_FILE,
  OWNERSHIP_REASONS,
  OWNED_PARENT,
  RESERVED_PORTS,
  allocatePort,
  findOrphanedProcesses,
  judgeOwnership,
  probePortFree,
  surveyOwnedParent,
} from './disposable-project.mjs';

/** A throwaway /proc, so the orphan scan is tested without spawning anything.
 *  /proc is Linux-shaped, so cmdlines carry forward slashes on every host, which
 *  is why the module spells OWNED_PARENT with forward slashes and matches on
 *  `${OWNED_PARENT}/task52-` rather than a join-built path. */
function fakeProc(entries: { pid: number; cmdline: string; ppid?: number }[]) {
  const root = mkdtempSync(join(tmpdir(), 'task52-proc-'));
  for (const entry of entries) {
    const dir = join(root, String(entry.pid));
    mkdirSync(dir, { recursive: true });
    writeFileSync(join(dir, 'cmdline'), entry.cmdline.split(' ').join('\0'));
    writeFileSync(join(dir, 'stat'), `${entry.pid} (proc) S ${entry.ppid ?? 1} 0 0 0 -1 0 0 0 0 0 0 0`);
  }
  return root;
}

/** Every workspace this file opens, so a failing assertion cannot leak one. */
const opened: DisposableWorkspace[] = [];
const workspace = (runId?: string) => {
  const created = new DisposableWorkspace(runId === undefined ? {} : { runId });
  opened.push(created);
  return created;
};

afterEach(() => {
  while (opened.length > 0) opened.pop()?.close();
});

describe('judgeOwnership', () => {
  const ownedRoot = join(OWNED_PARENT, 'task52-unit-guard');

  it('accepts a path inside the owned root', () => {
    expect(judgeOwnership({ ownedRoot, path: join(ownedRoot, 'proj/Content') }).owned).toBe(true);
  });

  it('refuses a sibling directory that merely shares a name prefix', () => {
    const verdict = judgeOwnership({ ownedRoot, path: `${ownedRoot}-other/proj` });
    expect(verdict.owned).toBe(false);
    expect(verdict.reason).toBe(OWNERSHIP_REASONS.OUTSIDE_OWNED_ROOT);
  });

  it('refuses the reference project, however the path is spelled', () => {
    expect(judgeOwnership({ ownedRoot, path: '/data/Game/MCPtest' }).owned).toBe(false);
    expect(judgeOwnership({ ownedRoot, path: join(ownedRoot, '../../../data/Game/MCPtest') }).owned).toBe(false);
    expect(judgeOwnership({ ownedRoot, path: '/data/UnrealEngine' }).owned).toBe(false);
  });

  it('refuses a root that is not under the owned parent at all', () => {
    const verdict = judgeOwnership({ ownedRoot: '/data/Game/MCPtest', path: '/data/Game/MCPtest/Content' });
    expect(verdict.owned).toBe(false);
    expect(verdict.reason).toBe(OWNERSHIP_REASONS.NOT_UNDER_OWNED_PARENT);
  });

  it.runIf(process.platform !== 'win32')('refuses a path that stays inside by STRING but escapes through a symlink', () => {
    const root = join(OWNED_PARENT, 'task52-unit-symlink');
    const outside = join(OWNED_PARENT, 'task52-unit-symlink-target');
    mkdirSync(root, { recursive: true });
    mkdirSync(outside, { recursive: true });
    const bridge = join(root, 'escape');
    if (!existsSync(bridge)) symlinkSync(outside, bridge);
    try {
      const verdict = judgeOwnership({ ownedRoot: root, path: join(bridge, 'Content') });
      expect(verdict.owned).toBe(false);
      expect(verdict.reason).toBe(OWNERSHIP_REASONS.ESCAPES_VIA_SYMLINK);
    } finally {
      rmSync(root, { recursive: true, force: true });
      rmSync(outside, { recursive: true, force: true });
    }
  });
});

describe('port allocation', () => {
  it('never hands out a port this wave has already dedicated to another surface', async () => {
    const ports = await Promise.all([allocatePort(), allocatePort(), allocatePort()]);
    for (const port of ports) expect(RESERVED_PORTS).not.toContain(port);
  });

  it('claims distinct named ports and records them in the manifest', async () => {
    const space = workspace();
    space.open();
    const ports = await space.claimPorts(['native', 'wsPrimary', 'wsSecondary']);
    expect(new Set(Object.values(ports)).size).toBe(3);
    const manifest = JSON.parse(String(readFileSync(space.manifestPath, 'utf8')));
    expect(manifest.ports).toEqual(ports);
  });

  it('reports a port that became busy after it was claimed, instead of failing later', async () => {
    const space = workspace();
    space.open();
    await space.claimPorts(['native']);
    const port = space.ports.native as number;
    expect((await space.verifyPortsStillFree()).collided).toHaveLength(0);

    const squatter = createServer();
    await new Promise<void>((settle) => squatter.listen(port, '127.0.0.1', settle));
    try {
      const collision = await space.verifyPortsStillFree();
      expect(collision.collided.map((row: { name: string }) => row.name)).toEqual(['native']);
      expect(await probePortFree(port)).toEqual({ free: false, detail: 'EADDRINUSE' });
    } finally {
      await new Promise<void>((settle) => squatter.close(() => settle()));
    }
  });
});

describe('DisposableWorkspace', () => {
  it('stamps its root before anything else is written there', () => {
    const space = workspace();
    space.open();
    expect(existsSync(space.manifestPath)).toBe(true);
    expect(resolve(space.root).startsWith(resolve(OWNED_PARENT) + sep)).toBe(true);
  });

  it('refuses to address a path outside itself', () => {
    const space = workspace();
    space.open();
    expect(() => space.path('../../../data/Game/MCPtest')).toThrow(/refusing to address/u);
  });

  it('removes everything it made and PROVES the tree is gone', () => {
    const space = workspace();
    space.open();
    writeFileSync(join(space.dir('proj/Content'), 'thing.uasset'), 'x', { mode: 0o600 });
    const receipt = space.close();
    expect(receipt.removed).toBe(true);
    expect(receipt.entriesBefore).toBeGreaterThan(2);
    expect(receipt.residual).toEqual([]);
    expect(existsSync(space.root)).toBe(false);
  });

  it('is idempotent: closing an already-closed workspace is still a clean receipt', () => {
    const space = workspace();
    space.open();
    space.close();
    const again = space.close();
    expect(again.removed).toBe(true);
    expect(again.entriesBefore).toBe(0);
  });
});

describe('findOrphanedProcesses', () => {
  it('ignores a process whose workspace still exists', () => {
    const space = workspace();
    space.open();
    const seen = findOrphanedProcesses({ procRoot: fakeProc([{ pid: 4242, cmdline: `/bin/UnrealEditor-Cmd ${space.root.replaceAll('\\', '/')}/project/X.uproject` }]) });
    expect(seen).toEqual([]);
  });

  it('finds a process still running against a workspace that was already removed', () => {
    const gone = `${OWNED_PARENT}/task52-already-removed`;
    const seen = findOrphanedProcesses({ procRoot: fakeProc([{ pid: 4242, cmdline: `/bin/UnrealEditor-Cmd ${gone}/project/X.uproject` }]) });
    expect(seen.map((entry: { pid: number }) => entry.pid)).toEqual([4242]);
    expect(seen[0]?.workspace).toBe(gone);
  });

  it('ignores processes that name no task-52 workspace at all', () => {
    const seen = findOrphanedProcesses({
      procRoot: fakeProc([{ pid: 4243, cmdline: '/bin/UnrealEditor-Cmd /data/Game/MCPtest/MCPtest.uproject' }]),
    });
    expect(seen).toEqual([]);
  });
});

describe('surveyOwnedParent', () => {
  it('sees this run as live and never proposes removing it', () => {
    const space = workspace();
    space.open();
    const survey = surveyOwnedParent();
    const mine = survey.runs.find((entry) => entry.runId === space.runId);
    expect(mine?.ownerAlive).toBe(true);
    expect(mine?.ownerPid).toBe(process.pid);
  });

  it('classifies an abandoned run without touching it', () => {
    // Surveys a parent this test made, not the shared one: planting a fake
    // abandoned run in the real owned parent would leave a manifest for whatever
    // else is running on this host to classify — and reclaim.
    const parent = mkdtempSync(join(tmpdir(), 'task52-survey-'));
    const orphan = join(parent, 'task52-unit-orphan');
    mkdirSync(orphan, { recursive: true });
    writeFileSync(join(orphan, MANIFEST_FILE),
      JSON.stringify({ runId: 'task52-unit-orphan', ownerPid: 2 ** 22, openedAt: new Date().toISOString() }), { mode: 0o600 });
    try {
      const found = surveyOwnedParent({ parent }).runs.find((entry) => entry.runId === 'task52-unit-orphan');
      expect(found?.ownerAlive).toBe(false);
      expect(existsSync(orphan)).toBe(true);
    } finally {
      rmSync(parent, { recursive: true, force: true });
    }
  });
});
