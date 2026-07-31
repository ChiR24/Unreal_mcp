#!/usr/bin/env node
// Task 61 — prove, per advertised UE minor, whether this host can host a run.
//
// The product advertises UE 5.0-5.8. This command answers, for each of those nine
// minors, whether an engine containing it is really on this machine — and answers
// it from `Engine/Build/Build.version`, the file the engine itself writes. It
// never reads a directory name: `/data/UnrealEngine` carries no version in its
// name and contains 5.7.4, so a name-based probe would be right by luck here and
// silently wrong the first time a root is renamed.
//
// THE EXIT CODE IS THE POINT. A blocker that exits 0 is indistinguishable from a
// gate that passed — the exact shape Task 54 shipped when it wired `npm audit`
// into CI at position 15 and never ran it. So:
//
//   0  every requested minor is installed, buildable AND runnable
//   3  at least one requested minor is NOT INSTALLED   <- emits blocker records
//   4  every requested minor is installed, but one cannot build or launch
//   2  usage error
//
// Exit 3 is the honest result on this host today, and a BLOCKED_EXTERNAL record
// is refused if it claims to have been produced by a detector that exited 0.
//
// This command is READ-ONLY. It stats and reads files under the search dirs. It
// downloads nothing, installs nothing, launches no editor and starts no build —
// a concurrent lane owns UBT, editors and ports.
//
// Run: node scripts/qa/detect-missing-engines.mjs [--search-dir /data]...
//                                                        [--minor 5.1]... [--json]

import { EXPECTED_MINORS, RESOLVE_REASONS, buildEngineInventory, formatInventoryTable } from '../../tests/unit/engine-certification/engine-inventory.mjs';

const EXIT = Object.freeze({ SATISFIED: 0, USAGE: 2, MINOR_MISSING: 3, MINOR_UNUSABLE: 4 });

/** Every `--flag VALUE`, in the order given. @param {string} name */
const argsAll = (name) => process.argv.reduce((found, token, index) => (
  token === name && process.argv[index + 1] !== undefined ? [...found, process.argv[index + 1]] : found
), /** @type {string[]} */ ([]));

const out = (/** @type {string} */ line) => { process.stdout.write(`${line}\n`); };

const searchDirs = argsAll('--search-dir');
const requested = argsAll('--minor');
const asJson = process.argv.includes('--json');

for (const minor of requested) {
  if (!/^\d+\.\d+$/u.test(minor)) {
    process.stderr.write(`--minor expects a <major>.<minor> key such as 5.1; got "${minor}"\n`);
    process.exit(EXIT.USAGE);
  }
}

const dirs = searchDirs.length > 0 ? searchDirs : ['/data'];
const minors = requested.length > 0 ? requested : [...EXPECTED_MINORS];
const detectedAt = new Date().toISOString();
const inventory = buildEngineInventory({ searchDirs: dirs });

const results = minors.map((minorKey) => {
  const resolved = inventory.resolve(minorKey);
  return {
    minorKey,
    reason: resolved.reason,
    root: resolved.root,
    versionString: resolved.identity === null ? null : resolved.identity.versionString,
    buildVersionFile: resolved.identity === null ? null : resolved.identity.sources.buildVersion.file,
    buildVersionSha256: resolved.identity === null ? null : resolved.identity.sources.buildVersion.sha256,
    detail: resolved.detail,
  };
});

const missing = results.filter((entry) => entry.reason === RESOLVE_REASONS.MINOR_NOT_INSTALLED);
const unusable = results.filter((entry) => entry.reason !== RESOLVE_REASONS.MINOR_NOT_INSTALLED && entry.reason !== RESOLVE_REASONS.OK);
const exitCode = missing.length > 0
  ? EXIT.MINOR_MISSING
  : (unusable.length > 0 ? EXIT.MINOR_UNUSABLE : EXIT.SATISFIED);

if (asJson) {
  out(JSON.stringify({
    detectedAt,
    searchDirs: dirs,
    requestedMinors: minors,
    scannedRoots: inventory.scannedRoots,
    results,
    missingMinors: missing.map((entry) => entry.minorKey),
    unusableMinors: unusable.map((entry) => entry.minorKey),
    exitCode,
    identifiedBy: 'Engine/Build/Build.version, corroborated by Version.h; a directory name is never an input',
  }, null, 2));
} else {
  out(`UE inventory at ${detectedAt}`);
  out(`search dirs: ${dirs.join(', ')} — ${inventory.scannedRoots} engine root(s) scanned`);
  out('identity read from Engine/Build/Build.version, corroborated by Version.h; a directory name is never an input');
  out('');
  out(formatInventoryTable(inventory));
  out('');
  out(`requested minors (${minors.length}):`);
  for (const entry of results) {
    if (entry.reason === RESOLVE_REASONS.OK) {
      out(`  ${entry.minorKey}  PRESENT  ${entry.versionString} at ${entry.root}`);
      continue;
    }
    out(`  ${entry.minorKey}  ABSENT   ${entry.reason}: ${entry.detail}`);
  }
  out('');
  if (missing.length > 0) {
    out(`${missing.length} advertised minor(s) are NOT INSTALLED: ${missing.map((entry) => entry.minorKey).join(', ')}`);
    out('No neighbouring minor may be substituted: certification is per-minor and binaries are never reused across minors.');
  }
  if (unusable.length > 0) {
    out(`${unusable.length} installed minor(s) cannot host a run: ${unusable.map((entry) => `${entry.minorKey} (${entry.reason})`).join(', ')}`);
  }
  out(`exit ${exitCode}`);
}

process.exit(exitCode);
