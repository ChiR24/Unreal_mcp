// @ts-check
// tests/unit/evidence-oracles/fixture-namespace.mjs
// Task 50 — OWNED namespaces, provable ownership, and cleanup that is verified
// rather than asserted.
//
// THE RULE THIS FILE ENFORCES: NEVER DELETE ANYTHING YOU DID NOT CREATE.
//
// The workspace this suite runs against is somebody's real project. It already
// holds BP_Probe_1784051174, GapProofBP, SweepTestBP, QAIdemProbe and dozens of
// other artifacts from earlier lanes, plus two empty `task49-*` folders. A probe
// that "cleans up /Game/MCPTest" destroys all of it. Task 49 found six unrelated
// `node dist/cli.js` processes on this host and correctly left them alone; the
// same care has to be structural for content, not a habit.
//
// So ownership is a PREDICATE, checked before every removal, and it is not
// "the path starts with my prefix" — that string test is defeated by `..`, by a
// symlink, and by a sibling whose name merely begins the same way
// (`<scratch>/run-1` vs `<scratch>/run-11`). It resolves real paths and demands
// strict containment.
//
// THE MANIFEST is the interruption-recovery mechanism. A run that is killed
// between "created the fixture" and "removed it" leaves no in-memory ledger, so
// the ledger is written to disk at allocation time. A later run can read it,
// confirm the owning process is dead, and reclaim EXACTLY what that manifest
// names — never a wildcard sweep.

import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, realpathSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { randomBytes } from 'node:crypto';
import { tmpdir } from 'node:os';
import { join, relative, resolve, sep } from 'node:path';

import { observeProcess, observeTree, walkFiles } from './state-oracles.mjs';

/** The one content root this suite is allowed to touch, under a per-run child. */
export const CONTENT_ROOT = '/Game/MCPTest';

/**
 * The one host directory this suite is allowed to touch, under a per-run child.
 *
 * Rooted at the OS temp dir rather than a hardcoded `/tmp`, so it follows TMPDIR
 * and names a real place on Windows. It stays a STABLE, well-known parent on
 * purpose: interruption recovery is a LATER process reading an earlier run's
 * manifest, so a per-process scratch root would silently delete that capability.
 * Unpredictability belongs to the per-run child underneath it, and `open()` is
 * where that is enforced.
 */
export const TEMP_ROOT = join(realpathSync(tmpdir()), 'opencode', 'task-50');

export const MANIFEST_FILE = 'ownership.json';

/**
 * Allocate a run id.
 *
 * Time-ordered so manifests sort, random-suffixed so two runs started in the same
 * second cannot collide — a namespace collision is the "one transport read the
 * other's leftover" bug wearing a different hat.
 * @param {{ now?: () => Date, random?: (bytes: number) => Buffer }} [options]
 */
export function allocateRunId(options = {}) {
  const now = (options.now ?? (() => new Date()))();
  const random = (options.random ?? randomBytes)(4).toString('hex');
  const stamp = now.toISOString().replace(/[-:]/gu, '').replace(/\.\d+Z$/u, 'Z');
  return `t50-${stamp}-${random}`;
}

/**
 * Strict containment: is `candidate` a real path strictly inside real `root`?
 *
 * Both sides are resolved through realpath where they exist, so a symlink cannot
 * smuggle a target out of the owned tree. Equality with the root is NOT
 * containment for a delete check — removing the root itself is a separate,
 * explicit operation.
 * @param {string} root @param {string} candidate
 * @returns {{ owned: boolean, reason: string }}
 */
export function isStrictlyInside(root, candidate) {
  const resolveReal = (/** @type {string} */ path) => {
    const absolute = resolve(path);
    try {
      return realpathSync(absolute);
    } catch {
      // Does not exist yet: resolve the deepest existing ancestor so a symlinked
      // PARENT is still caught, then re-attach the remainder.
      let head = absolute;
      /** @type {string[]} */
      const tail = [];
      while (head !== resolve(head, '..') && !existsSync(head)) {
        tail.unshift(head.slice(head.lastIndexOf(sep) + 1));
        head = resolve(head, '..');
      }
      try {
        return join(realpathSync(head), ...tail);
      } catch {
        return absolute;
      }
    }
  };
  const realRoot = resolveReal(root);
  const realCandidate = resolveReal(candidate);
  if (realCandidate === realRoot) return { owned: false, reason: 'IS_THE_ROOT_ITSELF' };
  const rel = relative(realRoot, realCandidate);
  if (rel.length === 0) return { owned: false, reason: 'IS_THE_ROOT_ITSELF' };
  if (rel.startsWith('..') || resolve(realRoot, rel) !== realCandidate) {
    return { owned: false, reason: 'OUTSIDE_OWNED_ROOT' };
  }
  return { owned: true, reason: 'INSIDE_OWNED_ROOT' };
}

/**
 * The same containment rule for Unreal object paths, which have no filesystem to
 * realpath. Segment-wise so `/Game/MCPTest/run-1` never owns `/Game/MCPTest/run-11`.
 * @param {string} root @param {string} candidate
 */
export function isInsideGamePath(root, candidate) {
  const normalize = (/** @type {string} */ path) => String(path).replace(/\/+$/u, '').split('/').filter((part) => part.length > 0);
  const rootParts = normalize(root);
  const parts = normalize(candidate);
  if (parts.some((part) => part === '..')) return { owned: false, reason: 'PATH_TRAVERSAL' };
  if (parts.length <= rootParts.length) {
    return { owned: false, reason: parts.join('/') === rootParts.join('/') ? 'IS_THE_ROOT_ITSELF' : 'OUTSIDE_OWNED_ROOT' };
  }
  const inside = rootParts.every((part, index) => parts[index] === part);
  return inside ? { owned: true, reason: 'INSIDE_OWNED_ROOT' } : { owned: false, reason: 'OUTSIDE_OWNED_ROOT' };
}

/**
 * One run's owned namespaces plus the ledger that proves what it made.
 */
export class FixtureNamespace {
  /**
   * The scratch directory THIS instance created. Only `open()` assigns it, and
   * only from `mkdtempSync`, so a re-open reuses the run's own tree instead of
   * stranding it.
   * @type {string|null}
   */
  #createdTempRoot = null;

  /**
   * The manifest exactly as first written. A re-open returns this rather than
   * re-stamping, so the recorded baseline can never be restated.
   * @type {Record<string, unknown>|null}
   */
  #manifest = null;

  /**
   * @param {{ runId?: string, projectRoot: string, contentRoot?: string, tempRoot?: string,
   *   pid?: number, now?: () => Date }} spec
   */
  constructor(spec) {
    this.runId = spec.runId ?? allocateRunId();
    this.projectRoot = resolve(spec.projectRoot);
    this.now = spec.now ?? (() => new Date());
    this.pid = spec.pid ?? process.pid;
    /** The owned Unreal content namespace: `/Game/MCPTest/<run-id>`. */
    this.gameRoot = `${spec.contentRoot ?? CONTENT_ROOT}/${this.runId}`;
    /** Where that namespace lands on disk. */
    this.diskRoot = join(this.projectRoot, 'Content', ...this.gameRoot.replace(/^\/Game\//u, '').split('/'));
    /** The stable parent this suite's runs share, so recovery can enumerate them. */
    this.tempParent = resolve(spec.tempRoot ?? TEMP_ROOT);
    /**
     * The owned host scratch namespace. PROVISIONAL until `open()`, which replaces
     * it with the directory mkdtemp actually created. `runId` remains the logical
     * identity of the run — the manifest, not the directory name, is what recovery
     * reads — so the content namespace above is unaffected.
     */
    this.tempRoot = join(this.tempParent, this.runId);
    /** @type {Array<{ kind: string, path: string, declaredAt: string }>} */
    this.declared = [];
    /** @type {import('./state-oracles.mjs').Observation|null} */
    this.baseline = null;
    /** Set only when THIS run removed its own temp root. @type {string|null} */
    this.tempReleasedAt = null;
    this.manifestPath = join(this.tempRoot, MANIFEST_FILE);
  }

  /**
   * Create the host scratch dir, write the ownership manifest, and take the
   * BASELINE reading of the content namespace.
   *
   * The baseline is not decoration: it is what "restored" is later compared to,
   * and taking it BEFORE anything is created is the pre-state discipline Task 49
   * learned by scoring a leftover as proof.
   */
  open() {
    mkdirSync(this.tempParent, { recursive: true, mode: 0o700 });
    // The shared parent lives in a world-writable temp dir, so prove it is a real
    // directory rather than a symlink somebody planted BEFORE writing under it.
    // `isStrictlyInside` cannot see this: a symlinked parent resolves the root and
    // its children through the same link, so containment still agrees.
    const realParent = realpathSync(this.tempParent);
    if (realParent !== this.tempParent) {
      const error = new Error(`REFUSING to open ${this.tempParent}: it resolves to ${realParent}, which is not where it claims to be.`);
      error.name = 'UnownedFixture';
      throw error;
    }
    // IDEMPOTENT, and deliberately INERT on a second call. Re-running the
    // observation below would fold anything created since the first open into the
    // PRE-state, and `close()` compares against that recorded baseline — so a
    // fixture that survived cleanup would come back reported as `contentRestored`.
    // That is Task 49's leak exactly: a leftover scored as proof. The directory is
    // reused rather than stranded, and the manifest is written once.
    if (this.#createdTempRoot !== null) return this.#manifest;

    // The parent is shared and predictable by design (recovery has to find it);
    // the run's own directory must not be. mkdtemp creates it exclusively at mode
    // 0700, so it is provably ours the instant it exists and no earlier process
    // could have parked a directory on the name we were going to use.
    const tempRoot = mkdtempSync(join(this.tempParent, `${this.runId}-`));
    this.#createdTempRoot = tempRoot;
    this.tempRoot = tempRoot;
    this.manifestPath = join(tempRoot, MANIFEST_FILE);
    this.baseline = observeTree({ root: this.diskRoot, kind: 'namespace' });
    const manifest = {
      runId: this.runId,
      suite: 'task-50',
      pid: this.pid,
      // The pid alone is not identity: the kernel recycles pids, so a recovery
      // pass that trusted a bare pid could reclaim a LIVE run's namespace.
      pidStartTicks: observeProcess({ pid: this.pid }).detail.startTicks ?? null,
      openedAt: this.now().toISOString(),
      gameRoot: this.gameRoot,
      diskRoot: this.diskRoot,
      tempRoot,
      baselineDigest: this.baseline.digest,
      baselineFileCount: this.baseline.detail.fileCount ?? 0,
    };
    this.#manifest = manifest;
    writeFileSync(join(tempRoot, MANIFEST_FILE), `${JSON.stringify(manifest, null, 2)}\n`, { mode: 0o600 });
    return manifest;
  }

  /**
   * Register something this run created. Refuses anything outside the owned
   * namespaces — a fixture that cannot be declared cannot later be deleted,
   * which is exactly the protection we want.
   * @param {'content'|'file'} kind @param {string} path
   */
  declare(kind, path) {
    const verdict = kind === 'content'
      ? isInsideGamePath(this.gameRoot, path)
      : isStrictlyInside(this.tempRoot, path);
    if (!verdict.owned) {
      const error = new Error(`REFUSING to declare unowned ${kind} "${path}": ${verdict.reason}. `
        + `This run owns only ${kind === 'content' ? this.gameRoot : this.tempRoot}.`);
      error.name = 'UnownedFixture';
      throw error;
    }
    this.declared.push({ kind, path, declaredAt: this.now().toISOString() });
    return this;
  }

  /**
   * Ownership check used before ANY removal, content or file.
   * @param {'content'|'file'} kind @param {string} path
   */
  owns(kind, path) {
    return kind === 'content' ? isInsideGamePath(this.gameRoot, path) : isStrictlyInside(this.tempRoot, path);
  }

  /**
   * Remove a host file/dir, but only after proving ownership.
   * @param {string} path
   * @returns {{ removed: boolean, path: string, reason: string }}
   */
  removeOwnedFile(path) {
    const verdict = isStrictlyInside(this.tempRoot, path);
    if (!verdict.owned) return { removed: false, path, reason: `REFUSED: ${verdict.reason}` };
    if (!existsSync(path)) return { removed: false, path, reason: 'ALREADY_ABSENT' };
    rmSync(path, { recursive: true, force: true });
    return { removed: true, path, reason: 'REMOVED' };
  }

  /**
   * Close the run: release the host scratch namespace and report, from an
   * INDEPENDENT re-read, whether the content namespace was restored.
   *
   * Content is NOT deleted from disk here. The editor owns those packages while
   * it is running, and ripping `.uasset` files out from under a live editor is a
   * different hazard from the one this suite is measuring. Content removal is the
   * caller's job through the editor; this method's job is to say honestly whether
   * it worked.
   *
   * IDEMPOTENT, and idempotent in the way that matters: THE MANIFEST IS THE
   * OWNERSHIP TOKEN. Once it is gone this run no longer owns the path, so a second
   * close touches nothing. Without that rule a re-entered cleanup would happily
   * delete whatever occupies the path NEXT — a run destroying a stranger's
   * directory because it once had the same name is precisely the "never delete
   * unowned content" violation this file exists to prevent.
   * @param {{ removeTemp?: boolean }} [options]
   */
  close(options = {}) {
    const removeTemp = options.removeTemp ?? true;
    const afterCleanup = observeTree({ root: this.diskRoot, kind: 'namespace' });
    /** @type {Array<{ removed: boolean, path: string, reason: string }>} */
    const fileReceipts = [];
    const stillOurs = existsSync(this.manifestPath);
    if (removeTemp && stillOurs) {
      // The manifest goes LAST, so an abort mid-close still leaves a recoverable
      // record and the next pass still recognises the directory as ours.
      for (const entry of readdirSync(this.tempRoot)) {
        if (entry === MANIFEST_FILE) continue;
        fileReceipts.push(this.removeOwnedFile(join(this.tempRoot, entry)));
      }
      fileReceipts.push(this.removeOwnedFile(this.manifestPath));
      if (readdirSync(this.tempRoot).length === 0) {
        rmSync(this.tempRoot, { recursive: true });
        this.tempReleasedAt = this.now().toISOString();
      }
      // A non-empty directory here holds something we did not create. Leaving it
      // is the correct outcome; residualTemp records it.
    } else if (removeTemp && this.tempReleasedAt === null && !existsSync(this.tempRoot)) {
      // Never opened, or released by an earlier pass of this same run.
      this.tempReleasedAt = this.now().toISOString();
    }
    // The content namespace's own DIRECTORY is residue too. Deleting an asset
    // through the editor removes the package but leaves the folder, which is why
    // two empty `task49-*` directories still sit in this project. A file-count
    // digest calls that "restored"; a reader looking at the Content tree calls it
    // litter that accumulates one folder per run. Removed only when it holds no
    // files at all, and only inside this run's own uniquely-named namespace.
    let emptyDirectoriesRemoved = 0;
    if (Number(afterCleanup.detail.fileCount ?? 0) === 0 && existsSync(this.diskRoot)) {
      const contentParent = join(this.projectRoot, 'Content');
      if (isStrictlyInside(contentParent, this.diskRoot).owned && walkFiles(this.diskRoot).length === 0) {
        rmSync(this.diskRoot, { recursive: true });
        emptyDirectoriesRemoved = 1;
      }
    }
    const residualTemp = existsSync(this.tempRoot) ? observeTree({ root: this.tempRoot, kind: 'tempdir' }) : null;
    return {
      emptyDirectoriesRemoved,
      contentDirectoryRemains: existsSync(this.diskRoot),
      runId: this.runId,
      gameRoot: this.gameRoot,
      diskRoot: this.diskRoot,
      tempRoot: this.tempRoot,
      closedAt: this.now().toISOString(),
      alreadyClosed: !stillOurs,
      baseline: this.baseline,
      afterCleanup,
      // Restored means the digest came back to the baseline — not that a delete
      // call answered success. Task 49 believed a delete response and leaked.
      contentRestored: this.baseline !== null && this.baseline.digest === afterCleanup.digest,
      residualContentFiles: Number(afterCleanup.detail.fileCount ?? 0) - Number(this.baseline?.detail.fileCount ?? 0),
      fileReceipts,
      // THIS run released it. Not "the path happens to be free now", which would
      // read as success when a stranger's directory sits there instead.
      tempReleased: this.tempReleasedAt !== null,
      tempReleasedAt: this.tempReleasedAt,
      tempPathOccupied: existsSync(this.tempRoot),
      residualTemp,
      declared: this.declared,
    };
  }
}

/**
 * @typedef {{ runId: string, pid: number, pidStartTicks: number|null, tempRoot: string,
 *   diskRoot: string, gameRoot: string, openedAt: string, manifestPath: string }} RecoverableManifest
 */

/**
 * Find namespaces abandoned by an INTERRUPTED run.
 *
 * Reclaimable requires the owning process to be provably gone. A pid that is
 * still alive AND whose start time matches the manifest is a live run and is
 * left completely alone — reclaiming it would be this suite doing to another lane
 * exactly what the plan forbids.
 * @param {{ root?: string, procRoot?: string, selfPid?: number }} [options]
 * @returns {{ scanned: number, reclaimable: RecoverableManifest[], active: RecoverableManifest[], unreadable: string[] }}
 */
export function findInterruptedRuns(options = {}) {
  const root = options.root ?? TEMP_ROOT;
  const selfPid = options.selfPid ?? process.pid;
  /** @type {RecoverableManifest[]} */
  const reclaimable = [];
  /** @type {RecoverableManifest[]} */
  const active = [];
  /** @type {string[]} */
  const unreadable = [];
  if (!existsSync(root)) return { scanned: 0, reclaimable, active, unreadable };

  let scanned = 0;
  for (const entry of readdirSync(root, { withFileTypes: true })) {
    if (!entry.isDirectory()) continue;
    const manifestPath = join(root, entry.name, MANIFEST_FILE);
    if (!existsSync(manifestPath)) {
      // A directory with no manifest is not ours to reason about, let alone remove.
      unreadable.push(join(root, entry.name));
      continue;
    }
    scanned += 1;
    /** @type {any} */
    let manifest;
    try {
      manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
    } catch {
      unreadable.push(manifestPath);
      continue;
    }
    if (manifest?.suite !== 'task-50' || typeof manifest.runId !== 'string') {
      unreadable.push(manifestPath);
      continue;
    }
    /** @type {RecoverableManifest} */
    const record = { ...manifest, manifestPath };
    if (manifest.pid === selfPid) { active.push(record); continue; }
    const alive = observeProcess({ pid: Number(manifest.pid), procRoot: options.procRoot });
    const sameProcess = alive.present === true
      && (manifest.pidStartTicks === null || Number(alive.detail.startTicks) === Number(manifest.pidStartTicks));
    if (sameProcess) active.push(record);
    else reclaimable.push(record);
  }
  return { scanned, reclaimable, active, unreadable };
}

/**
 * Reclaim ONE interrupted run's host scratch namespace.
 *
 * Only the tempRoot the manifest itself names, only when that path is strictly
 * inside the shared root, and only after `findInterruptedRuns` proved the owner
 * is gone. No globs, no "anything older than", no sweep.
 * @param {RecoverableManifest} manifest @param {{ root?: string }} [options]
 */
export function reclaimInterruptedRun(manifest, options = {}) {
  const root = options.root ?? TEMP_ROOT;
  const verdict = isStrictlyInside(root, manifest.tempRoot);
  if (!verdict.owned) {
    return { reclaimed: false, runId: manifest.runId, path: manifest.tempRoot, reason: `REFUSED: ${verdict.reason}` };
  }
  const before = observeTree({ root: manifest.tempRoot, kind: 'tempdir' });
  rmSync(manifest.tempRoot, { recursive: true, force: true });
  const after = observeTree({ root: manifest.tempRoot, kind: 'tempdir' });
  return {
    reclaimed: after.present === false,
    runId: manifest.runId,
    path: manifest.tempRoot,
    reason: after.present === false ? 'RECLAIMED' : 'STILL_PRESENT_AFTER_REMOVAL',
    filesBefore: before.detail.fileCount ?? 0,
  };
}

/**
 * Content the run created that is STILL on disk, expressed as `/Game/...` paths a
 * caller can hand back to the editor to delete.
 *
 * This is the residue detector the live suite uses after cleanup. It reads the
 * filesystem, so a delete that answered `success` while leaving the package
 * behind — Task 49's exact leak — shows up here regardless of what any response
 * said.
 * @param {FixtureNamespace} namespace
 * @returns {Array<{ objectPath: string, file: string, bytes: number }>}
 */
export function residualContent(namespace) {
  if (!existsSync(namespace.diskRoot)) return [];
  /** @type {Array<{ objectPath: string, file: string, bytes: number }>} */
  const found = [];
  /** @param {string} directory */
  const visit = (directory) => {
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
      const path = join(directory, entry.name);
      if (entry.isDirectory()) { visit(path); continue; }
      if (!entry.isFile()) continue;
      const rel = relative(namespace.diskRoot, path).split(sep).join('/').replace(/\.(?:uasset|umap)$/u, '');
      found.push({ objectPath: `${namespace.gameRoot}/${rel}`, file: path, bytes: statSync(path).size });
    }
  };
  visit(namespace.diskRoot);
  return found.sort((a, b) => a.objectPath.localeCompare(b.objectPath));
}
