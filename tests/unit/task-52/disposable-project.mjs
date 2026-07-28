// @ts-check
// tests/unit/task-52/disposable-project.mjs
// Task 52 — the OWNED, disposable workspace a certification run may destroy.
//
// A certification run creates a UE project, compiles a plugin into it, starts an
// editor and then deletes all of it. Every one of those verbs is destructive, and
// the machine it runs on also holds /data/Game/MCPtest, six engine installs and
// whatever else another lane is doing. So ownership is not a convention here; it
// is a check that runs before every removal and refuses anything it did not make.
//
// THE THREE RULES, and what each one is protecting against:
//
//   1. Everything lives under ONE owned root inside /tmp/opencode, stamped with a
//      run id. Nothing outside it is ever written or removed — not the repo, not
//      an engine root, not another run's directory.
//   2. Removal resolves symlinks FIRST. A relative path that stays inside the
//      root by string comparison can still point anywhere once a symlink is in
//      the way, and `rm -rf` follows it.
//   3. Ports are allocated by binding, recorded in the manifest, and re-checked
//      immediately before use. A port that was free when the run started and is
//      busy when the editor launches is a COLLISION to report, not a surprise to
//      debug from an editor log.
//
// The manifest is written before the first artifact exists and removed last, so
// an interrupted run leaves a readable record of exactly what it owned.

import { createServer } from 'node:net';
import { existsSync, mkdirSync, readFileSync, readdirSync, realpathSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { basename, join, resolve, sep } from 'node:path';
import { randomBytes } from 'node:crypto';

/** The one directory tree a run may create and destroy. */
export const OWNED_PARENT = '/tmp/opencode';

/** Ports this wave's other surfaces use. A disposable run never takes them. */
export const RESERVED_PORTS = Object.freeze([3000, 8090, 8091]);

export const OWNERSHIP_REASONS = Object.freeze({
  OWNED: 'OWNED',
  OUTSIDE_OWNED_ROOT: 'OUTSIDE_OWNED_ROOT',
  ESCAPES_VIA_SYMLINK: 'ESCAPES_VIA_SYMLINK',
  NOT_UNDER_OWNED_PARENT: 'NOT_UNDER_OWNED_PARENT',
});

/** @param {string} parent @param {string} path */
function containedBy(parent, path) {
  const base = resolve(parent);
  const target = resolve(path);
  return target === base || target.startsWith(`${base}${sep}`);
}

/**
 * Resolve a path through its deepest EXISTING ancestor, re-appending the segments
 * that do not exist yet. Plain `realpathSync` cannot be used: the owned root does
 * not exist at the moment it is first judged, and a path that does not exist yet
 * still has to be judged before it is created.
 * @param {string} path
 */
function resolveThroughExisting(path) {
  let probe = resolve(path);
  /** @type {string[]} */
  const pending = [];
  while (!existsSync(probe)) {
    const parent = resolve(probe, '..');
    if (parent === probe) return probe;
    pending.unshift(basename(probe));
    probe = parent;
  }
  return join(realpathSync(probe), ...pending);
}

/**
 * Is `path` something THIS run may delete?
 *
 * Resolves the deepest existing ancestor with realpath before comparing, so a
 * symlink planted inside the owned root cannot smuggle a removal out of it. A
 * path that does not exist is judged by its lexical position, which is correct:
 * you cannot delete it anyway, and refusing to reason about it would make the
 * "already clean" case indistinguishable from a violation.
 * @param {{ ownedRoot: string, path: string }} spec
 */
export function judgeOwnership(spec) {
  if (!containedBy(OWNED_PARENT, spec.ownedRoot)) {
    return { owned: false, reason: OWNERSHIP_REASONS.NOT_UNDER_OWNED_PARENT, detail: `${spec.ownedRoot} is not under ${OWNED_PARENT}` };
  }
  if (!containedBy(spec.ownedRoot, spec.path)) {
    return { owned: false, reason: OWNERSHIP_REASONS.OUTSIDE_OWNED_ROOT, detail: `${spec.path} is not under ${spec.ownedRoot}` };
  }
  const realRoot = resolveThroughExisting(spec.ownedRoot);
  const realPath = resolveThroughExisting(spec.path);
  if (!containedBy(realRoot, realPath)) {
    return { owned: false, reason: OWNERSHIP_REASONS.ESCAPES_VIA_SYMLINK, detail: `${spec.path} resolves to ${realPath}, which is outside ${realRoot}` };
  }
  return { owned: true, reason: OWNERSHIP_REASONS.OWNED, detail: null };
}

/**
 * Take a free loopback port by binding one and letting the kernel choose.
 * @param {{ exclude?: readonly number[] }} [spec]
 * @returns {Promise<number>}
 */
export function allocatePort(spec = {}) {
  const exclude = new Set([...RESERVED_PORTS, ...(spec.exclude ?? [])]);
  return new Promise((settle, fail) => {
    const server = createServer();
    server.on('error', fail);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      const port = typeof address === 'object' && address !== null ? address.port : 0;
      server.close(() => {
        if (port === 0 || exclude.has(port)) {
          allocatePort(spec).then(settle, fail);
          return;
        }
        settle(port);
      });
    });
  });
}

/**
 * Is this port free RIGHT NOW? Called again immediately before the editor starts,
 * because "it was free two minutes ago" is not a fact about the present.
 * @param {number} port
 * @returns {Promise<{ free: boolean, detail: string }>}
 */
export function probePortFree(port) {
  return new Promise((settle) => {
    const server = createServer();
    server.on('error', (error) => settle({ free: false, detail: String(/** @type {any} */ (error).code ?? error) }));
    server.listen(port, '127.0.0.1', () => {
      server.close(() => settle({ free: true, detail: 'bound and released' }));
    });
  });
}

/** @param {string} root */
function walk(root) {
  /** @type {string[]} */
  const found = [];
  /** @param {string} dir */
  const visit = (dir) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      found.push(path);
      if (entry.isDirectory() && !entry.isSymbolicLink()) visit(path);
    }
  };
  if (existsSync(root)) visit(root);
  return found;
}

/**
 * One disposable workspace, owned end to end.
 */
export class DisposableWorkspace {
  /** @param {{ runId?: string, parent?: string, purpose?: string }} [spec] */
  constructor(spec = {}) {
    this.runId = spec.runId ?? `task52-${Date.now().toString(36)}-${randomBytes(3).toString('hex')}`;
    this.parent = spec.parent ?? OWNED_PARENT;
    this.root = join(this.parent, this.runId);
    this.purpose = spec.purpose ?? 'task-52 disposable UE certification';
    this.manifestPath = join(this.root, 'OWNED-BY-TASK-52.json');
    /** @type {Record<string, number>} */
    this.ports = {};
    this.opened = false;
  }

  /** Create the root and stamp it, BEFORE anything else is written into it. */
  open() {
    const guard = judgeOwnership({ ownedRoot: this.root, path: this.root });
    if (!guard.owned) throw new Error(`refusing to open a workspace outside ${OWNED_PARENT}: ${guard.detail}`);
    mkdirSync(this.root, { recursive: true });
    this.opened = true;
    this.writeManifest();
    return this.manifest();
  }

  manifest() {
    return {
      runId: this.runId, root: this.root, purpose: this.purpose,
      ownerPid: process.pid, openedAt: new Date().toISOString(), ports: { ...this.ports },
    };
  }

  writeManifest() {
    writeFileSync(this.manifestPath, `${JSON.stringify(this.manifest(), null, 2)}\n`);
  }

  /** @param {string} relative */
  path(relative) {
    const target = join(this.root, relative);
    const guard = judgeOwnership({ ownedRoot: this.root, path: target });
    if (!guard.owned) throw new Error(`refusing to address ${target}: ${guard.detail}`);
    return target;
  }

  /** @param {string} relative */
  dir(relative) {
    const target = this.path(relative);
    mkdirSync(target, { recursive: true });
    return target;
  }

  /**
   * Claim named ports for this run and record them in the manifest.
   * @param {readonly string[]} names
   */
  async claimPorts(names) {
    /** @type {number[]} */
    const taken = [];
    for (const name of names) {
      const port = await allocatePort({ exclude: taken });
      taken.push(port);
      this.ports[name] = port;
    }
    if (this.opened) this.writeManifest();
    return { ...this.ports };
  }

  /** Re-check every claimed port at the moment of use. */
  async verifyPortsStillFree() {
    /** @type {Array<{ name: string, port: number, free: boolean, detail: string }>} */
    const rows = [];
    for (const [name, port] of Object.entries(this.ports)) {
      const probe = await probePortFree(port);
      rows.push({ name, port, ...probe });
    }
    return { rows, collided: rows.filter((row) => !row.free) };
  }

  /**
   * Remove the workspace and PROVE it is gone by listing the tree afterwards.
   * Never touches anything outside the owned root, and says so with a receipt.
   */
  close() {
    const before = walk(this.root).length;
    const guard = judgeOwnership({ ownedRoot: this.root, path: this.root });
    if (!guard.owned) {
      return { removed: false, reason: guard.reason, detail: guard.detail, entriesBefore: before, residual: walk(this.root) };
    }
    try {
      rmSync(this.root, { recursive: true, force: true });
    } catch (error) {
      return { removed: false, reason: 'REMOVE_FAILED', detail: String(error), entriesBefore: before, residual: walk(this.root) };
    }
    const residual = walk(this.root);
    return {
      removed: residual.length === 0 && !existsSync(this.root),
      reason: residual.length === 0 ? 'REMOVED' : 'RESIDUE',
      detail: residual.length === 0 ? `${before} entries removed; ${this.root} no longer exists` : `${residual.length} entries remain`,
      entriesBefore: before, residual,
    };
  }
}

/**
 * Every live pid whose command line names `needle`, plus every descendant of
 * those pids, read straight out of /proc.
 *
 * The descendant walk matters and the needle alone is not enough: a packaging
 * run names its workspace, but the UAT, UBT and clang processes it spawns name
 * only their own scratch directory. Reclaiming the parent and leaving the tree
 * would free the directory while a compile kept running against it.
 * @param {{ needle: string, procRoot?: string }} spec
 */
export function processTreeNaming(spec) {
  const procRoot = spec.procRoot ?? '/proc';
  /** @type {Map<number, number>} */
  const parents = new Map();
  /** @type {Map<number, string>} */
  const commands = new Map();
  for (const entry of readdirSync(procRoot)) {
    if (!/^\d+$/u.test(entry)) continue;
    const pid = Number(entry);
    try {
      commands.set(pid, readFileSync(join(procRoot, entry, 'cmdline'), 'utf8').split('\0').join(' ').trim());
      const stat = readFileSync(join(procRoot, entry, 'stat'), 'utf8');
      parents.set(pid, Number(stat.slice(stat.lastIndexOf(')') + 2).split(' ')[1]));
    } catch {
      // The process exited mid-scan; it is by definition not still holding anything.
    }
  }
  const seeds = [...commands.entries()].filter(([, cmd]) => cmd.includes(spec.needle)).map(([pid]) => pid);
  /** @type {Set<number>} */
  const owned = new Set(seeds);
  let grew = true;
  while (grew) {
    grew = false;
    for (const [pid, ppid] of parents) {
      if (!owned.has(pid) && owned.has(ppid)) {
        owned.add(pid);
        grew = true;
      }
    }
  }
  return [...owned].sort((a, b) => b - a).map((pid) => ({ pid, command: (commands.get(pid) ?? '').slice(0, 200) }));
}

/**
 * Processes still running against a task-52 workspace that no longer exists.
 *
 * This is the residue a crashed orchestrator leaves that a directory scan cannot
 * see: the workspace is gone, so `surveyOwnedParent` reports nothing, while an
 * editor keeps running against the deleted project — holding its ports and its
 * memory indefinitely. The path is proof of ownership, so nothing else can match.
 * @param {{ parent?: string, procRoot?: string }} [spec]
 */
export function findOrphanedProcesses(spec = {}) {
  const parent = spec.parent ?? OWNED_PARENT;
  const naming = processTreeNaming({ needle: `${parent}/task52-`, procRoot: spec.procRoot });
  return naming
    .map((entry) => {
      const quoted = parent.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&');
      const match = new RegExp(`${quoted}/task52-[A-Za-z0-9-]+`, 'u').exec(entry.command);
      return { ...entry, workspace: match === null ? null : match[0] };
    })
    .filter((entry) => entry.workspace !== null && !existsSync(entry.workspace) && entry.pid !== process.pid);
}

/**
 * Stop processes whose owned workspace has already been removed.
 * @param {{ parent?: string, signal?: (pid: number, signal: string) => void, waitMs?: number }} [spec]
 */
export async function sweepOrphanedProcesses(spec = {}) {
  const signal = spec.signal ?? ((pid, sig) => process.kill(pid, sig));
  const doomed = findOrphanedProcesses({ parent: spec.parent });
  for (const entry of doomed) {
    try {
      signal(entry.pid, 'SIGTERM');
    } catch { /* already gone */ }
  }
  if (doomed.length > 0) await new Promise((settle) => { setTimeout(settle, spec.waitMs ?? 10_000); });
  for (const entry of findOrphanedProcesses({ parent: spec.parent })) {
    try {
      signal(entry.pid, 'SIGKILL');
    } catch { /* already gone */ }
  }
  if (doomed.length > 0) await new Promise((settle) => { setTimeout(settle, 2000); });
  const remaining = findOrphanedProcesses({ parent: spec.parent });
  return { stopped: doomed, remaining, clean: remaining.length === 0 };
}

/**
 * Reclaim a run this lane abandoned: stop its processes, then remove its tree.
 *
 * REFUSES unless the manifest's owner process is really gone. A run whose owner
 * is alive is somebody's work in progress, and "it looked stale" is exactly the
 * reasoning that ends with a colleague's build torn down mid-flight.
 * @param {{ root: string, signal?: (pid: number, signal: string) => void, waitMs?: number }} spec
 */
export async function reclaimOrphanedRun(spec) {
  const signal = spec.signal ?? ((pid, sig) => process.kill(pid, sig));
  const manifestPath = join(spec.root, 'OWNED-BY-TASK-52.json');
  if (!existsSync(manifestPath)) {
    return { reclaimed: false, reason: 'NOT_A_TASK52_RUN', root: spec.root, stopped: [], removal: null };
  }
  const manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
  let ownerAlive = false;
  try {
    process.kill(Number(manifest.ownerPid), 0);
    ownerAlive = true;
  } catch {
    ownerAlive = false;
  }
  if (ownerAlive) {
    return { reclaimed: false, reason: 'OWNER_STILL_ALIVE', root: spec.root, ownerPid: manifest.ownerPid, stopped: [], removal: null };
  }
  const guard = judgeOwnership({ ownedRoot: spec.root, path: spec.root });
  if (!guard.owned) {
    return { reclaimed: false, reason: guard.reason, root: spec.root, stopped: [], removal: null };
  }

  const doomed = processTreeNaming({ needle: spec.root });
  for (const entry of doomed) {
    try {
      signal(entry.pid, 'SIGTERM');
    } catch { /* already gone */ }
  }
  await new Promise((settle) => { setTimeout(settle, spec.waitMs ?? 8000); });
  const survivors = processTreeNaming({ needle: spec.root });
  for (const entry of survivors) {
    try {
      signal(entry.pid, 'SIGKILL');
    } catch { /* already gone */ }
  }
  await new Promise((settle) => { setTimeout(settle, 2000); });

  const workspace = new DisposableWorkspace({ runId: manifest.runId });
  const removal = workspace.close();
  return {
    reclaimed: removal.removed && processTreeNaming({ needle: spec.root }).length === 0,
    reason: removal.removed ? 'RECLAIMED' : removal.reason,
    root: spec.root,
    ownerPid: manifest.ownerPid,
    stopped: doomed,
    stillRunningAfter: processTreeNaming({ needle: spec.root }),
    removal,
  };
}

/**
 * Other runs' leftovers under the owned parent, classified but NEVER removed
 * here: a directory whose owner process is still alive belongs to that run.
 * @param {{ parent?: string }} [spec]
 */
export function surveyOwnedParent(spec = {}) {
  const parent = spec.parent ?? OWNED_PARENT;
  /** @type {Array<Record<string, unknown>>} */
  const runs = [];
  if (!existsSync(parent)) return { parent, runs, scanned: 0 };
  for (const entry of readdirSync(parent)) {
    const candidate = join(parent, entry);
    const manifest = join(candidate, 'OWNED-BY-TASK-52.json');
    try {
      if (!statSync(candidate).isDirectory() || !existsSync(manifest)) continue;
      const parsed = JSON.parse(readFileSync(manifest, 'utf8'));
      let ownerAlive = false;
      try {
        process.kill(Number(parsed.ownerPid), 0);
        ownerAlive = true;
      } catch {
        ownerAlive = false;
      }
      runs.push({ root: candidate, runId: parsed.runId, ownerPid: parsed.ownerPid, ownerAlive, openedAt: parsed.openedAt });
    } catch {
      // Unreadable or not ours. Recorded nowhere, removed never.
    }
  }
  return { parent, runs, scanned: runs.length };
}
