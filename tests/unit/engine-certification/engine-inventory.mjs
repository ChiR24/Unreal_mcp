// @ts-check
// tests/unit/engine-certification/engine-inventory.mjs
// Task 52 — the census of engines that are really on this machine.
//
// The acceptance criterion names five engines and four gaps, and the tempting
// shortcut is a script that produces that list. This one produces whatever is
// installed and states the gaps as gaps, because the census exists precisely to
// stop a certification report claiming a minor nobody ran.
//
// THREE DISTINCTIONS THIS FILE REFUSES TO COLLAPSE:
//
//   installed vs buildable vs runnable. On this host, four of six roots have no
//   compiled `UnrealEditor-Cmd`. They can package a plugin (RunUAT is present) and
//   they cannot host a certification. Reporting them as "available 5.3" would put
//   a green row under a minor where no editor ever started.
//
//   one minor, two roots. 5.0.3 sits at two paths here. Picking whichever the
//   directory listing yielded first would let two runs of the same command
//   certify different trees under the same heading, so the tie is broken by a
//   stated rule and BOTH roots are named in the report.
//
//   absent vs unreadable. A root whose Build.version cannot be read is not a
//   missing minor — it is a root that failed identification, and it is listed as
//   such rather than being quietly dropped or, worse, filed under the version its
//   folder name claims.

import { readdirSync, statSync } from 'node:fs';
import { join } from 'node:path';

import { minorKeyOf, readEngineIdentity, realIo } from './engine-identity.mjs';

/** The minors this project claims to support (`.uplugin` description: "UE 5.0-5.8 Preview"). */
export const EXPECTED_MINORS = Object.freeze(['5.0', '5.1', '5.2', '5.3', '5.4', '5.5', '5.6', '5.7', '5.8']);

/** Why a requested minor cannot host a certification run. */
export const RESOLVE_REASONS = Object.freeze({
  OK: 'OK',
  MINOR_NOT_INSTALLED: 'MINOR_NOT_INSTALLED',
  NO_COMPILED_EDITOR: 'NO_COMPILED_EDITOR',
  NO_BUILD_TOOLCHAIN: 'NO_BUILD_TOOLCHAIN',
});

/**
 * Candidate engine roots under a search directory. Directory names are used ONLY
 * to enumerate candidates — every one is then identified by reading the engine,
 * and a directory that holds no `Build.version` is simply not a candidate.
 * @param {{ searchDirs?: readonly string[] }} [spec]
 */
export function discoverEngineRoots(spec = {}) {
  const dirs = spec.searchDirs ?? ['/data'];
  /** @type {string[]} */
  const roots = [];
  for (const dir of dirs) {
    /** @type {string[]} */
    let entries = [];
    try {
      entries = readdirSync(dir);
    } catch {
      continue;
    }
    for (const entry of entries) {
      const candidate = join(dir, entry);
      try {
        if (!statSync(candidate).isDirectory()) continue;
        statSync(join(candidate, 'Engine/Build/Build.version'));
        roots.push(candidate);
      } catch {
        // Not an engine root. Nothing to report: this is enumeration, not a verdict.
      }
    }
  }
  return roots.sort();
}

/**
 * Break a tie between two roots holding the same minor.
 *
 * Runnable first (an editor you can launch beats one you cannot), then the higher
 * patch, then the path. The last clause is arbitrary and that is the point: it
 * makes the choice STABLE, so the same command certifies the same tree twice.
 * @param {any} left @param {any} right
 */
function preferRoot(left, right) {
  if (left.runnable !== right.runnable) return left.runnable ? -1 : 1;
  if (left.version.patch !== right.version.patch) return right.version.patch - left.version.patch;
  return left.root < right.root ? -1 : left.root > right.root ? 1 : 0;
}

/**
 * @param {{ roots?: readonly string[], expectedMinors?: readonly string[],
 *   io?: typeof realIo, searchDirs?: readonly string[] }} [spec]
 */
export function buildEngineInventory(spec = {}) {
  const io = spec.io ?? realIo;
  const expectedMinors = spec.expectedMinors ?? EXPECTED_MINORS;
  const roots = spec.roots ?? discoverEngineRoots({ searchDirs: spec.searchDirs });
  const identities = [...roots].sort().map((root) => readEngineIdentity({ root, io }));

  /** @type {Map<string, any[]>} */
  const byMinor = new Map();
  /** @type {Array<{ root: string, reason: string, detail: string|null }>} */
  const unusable = [];
  /** @type {Array<{ root: string, kind: string, claimed: string|null, contained: string|null }>} */
  const folderNameContradictions = [];

  for (const identity of identities) {
    if (!identity.usable) {
      unusable.push({ root: identity.root, reason: identity.reason, detail: identity.detail });
      continue;
    }
    const key = /** @type {string} */ (identity.minorKey);
    byMinor.set(key, [...(byMinor.get(key) ?? []), identity]);
    if (identity.folderName.agrees === false) {
      folderNameContradictions.push({
        root: identity.root, kind: 'VERSION',
        claimed: identity.folderName.claim === null ? null : `${identity.folderName.claim.major}.${identity.folderName.claim.minor}.${identity.folderName.claim.patch}`,
        contained: identity.versionString,
      });
    }
    if (identity.channel.folderLabelContradicted) {
      folderNameContradictions.push({
        root: identity.root, kind: 'CHANNEL_LABEL',
        claimed: identity.channel.folderLabel, contained: identity.channel.tag,
      });
    }
  }

  const available = [...byMinor.entries()]
    .sort(([left], [right]) => (left < right ? -1 : 1))
    .map(([minorKey, group]) => {
      const ordered = [...group].sort(preferRoot);
      const best = ordered[0];
      return {
        minorKey,
        versionString: best.versionString,
        preferredRoot: best.root,
        roots: group.map((entry) => entry.root).sort(),
        channel: best.channel.value,
        channelProvenBy: best.channel.provenBy,
        buildable: ordered.some((entry) => entry.buildable),
        runnable: ordered.some((entry) => entry.runnable),
        identity: best,
      };
    });

  const installedMinors = new Set(available.map((entry) => entry.minorKey));
  const missing = expectedMinors.filter((minor) => !installedMinors.has(minor));
  const duplicates = available
    .filter((entry) => entry.roots.length > 1)
    .map((entry) => ({ minorKey: entry.minorKey, roots: entry.roots, preferredRoot: entry.preferredRoot }));

  /**
   * Resolve a requested minor to the root that CONTAINS it.
   * @param {string} minorKey
   */
  const resolve = (minorKey) => {
    const entry = available.find((candidate) => candidate.minorKey === minorKey);
    if (entry === undefined) {
      return {
        ok: false, reason: RESOLVE_REASONS.MINOR_NOT_INSTALLED, root: null, identity: null,
        detail: `no engine root on this machine reports ${minorKey} in ${'Engine/Build/Build.version'}`,
      };
    }
    if (!entry.buildable) {
      return { ok: false, reason: RESOLVE_REASONS.NO_BUILD_TOOLCHAIN, root: entry.preferredRoot, identity: entry.identity, detail: 'RunUAT is absent, so the plugin cannot be packaged for this root' };
    }
    if (!entry.runnable) {
      return {
        ok: false, reason: RESOLVE_REASONS.NO_COMPILED_EDITOR, root: entry.preferredRoot, identity: entry.identity,
        detail: `${entry.preferredRoot} has no compiled UnrealEditor-Cmd; a plugin can be built here but no editor can be launched, so nothing can be certified`,
      };
    }
    return { ok: true, reason: RESOLVE_REASONS.OK, root: entry.preferredRoot, identity: entry.identity, detail: null };
  };

  return {
    scannedRoots: identities.length,
    expectedMinors: [...expectedMinors],
    identities,
    available,
    certifiable: available.filter((entry) => entry.runnable && entry.buildable),
    missing,
    unusable,
    duplicates,
    folderNameContradictions,
    resolve,
  };
}

/** @param {ReturnType<typeof buildEngineInventory>} inventory */
export function formatInventoryTable(inventory) {
  const lines = [
    '| minor | identity | root | channel | build | run |',
    '|-------|----------|------|---------|-------|-----|',
  ];
  for (const minor of inventory.expectedMinors) {
    const entry = inventory.available.find((candidate) => candidate.minorKey === minor);
    if (entry === undefined) {
      lines.push(`| ${minor} | MISSING | — | — | — | — |`);
      continue;
    }
    const channel = entry.channel === null ? `unproven` : `${entry.channel} (${entry.channelProvenBy})`;
    const roots = entry.roots.length > 1 ? `${entry.preferredRoot} (+${entry.roots.length - 1} more)` : entry.preferredRoot;
    lines.push(`| ${minor} | ${entry.versionString} | ${roots} | ${channel} | ${entry.buildable ? 'yes' : 'no'} | ${entry.runnable ? 'yes' : 'NO'} |`);
  }
  for (const entry of inventory.unusable) {
    lines.push(`| ?.? | UNUSABLE (${entry.reason}) | ${entry.root} | — | — | — |`);
  }
  return lines.join('\n');
}
