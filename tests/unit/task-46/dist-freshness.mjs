// @ts-check
// tests/unit/task-46/dist-freshness.mjs
// Task 46 remediation - does the artifact under test match the working tree?
//
// The cross-transport probe spawns `node dist/cli.js`. That is a BUILD, not the
// source, and nothing kept the two in step: run 1 of the gate reported HIGH
// findings F3 and F6 against a dist/ compiled three hours before those fixes
// landed. Both were real of the artifact and false of the code.
//
// So the probe refuses rather than rebuilds. A silent rebuild would leave the
// operator believing they probed what they had staged, and the whole point of
// this file is that nobody should have to infer which bytes were measured.

import { existsSync, readdirSync, statSync } from 'node:fs';
import { join, relative, resolve, sep } from 'node:path';

/** The artifact the stdio driver spawns, resolved like the probe resolves it: from cwd. */
export const BUILD_OUTPUT_ENTRY = 'dist/cli.js';

export const BUILD_INPUT_ROOTS = ['src'];

// tsconfig.json excludes **/*.test.ts and **/*.spec.ts, and dist/ holds zero
// compiled test files. Counting an edited colocated test as a build input would
// report a stale dist for a change that cannot alter what dist/cli.js does.
const NOT_A_BUILD_INPUT = /\.(?:test|spec)\.ts$/u;

const SKIP_DIRECTORIES = new Set(['node_modules', '.git', 'dist', 'coverage']);

/** @param {string} directory @param {(file: string, mtimeMs: number) => void} visit */
function walk(directory, visit) {
  if (!existsSync(directory)) return;
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    if (entry.isDirectory()) {
      if (!SKIP_DIRECTORIES.has(entry.name)) walk(join(directory, entry.name), visit);
      continue;
    }
    if (!entry.isFile()) continue;
    const file = join(directory, entry.name);
    try {
      visit(file, statSync(file).mtimeMs);
    } catch {
      // A file that vanished mid-scan cannot be a build input we are behind on.
    }
  }
}

/**
 * @typedef {{ fresh: boolean, reason: 'FRESH'|'STALE_BUILD'|'MISSING_BUILD', entry: string,
 *   entryMtimeMs: number|null, newestInput: string|null, newestInputMtimeMs: number|null,
 *   staleByMs: number|null }} FreshnessResult
 *
 * @param {string} [projectRoot]
 * @param {{ entry?: string, roots?: readonly string[] }} [options]
 * @returns {FreshnessResult}
 */
export function checkDistFreshness(projectRoot = process.cwd(), options = {}) {
  const entry = options.entry ?? BUILD_OUTPUT_ENTRY;
  const roots = options.roots ?? BUILD_INPUT_ROOTS;

  /** @type {string|null} */
  let newestInput = null;
  let newestInputMtimeMs = -1;
  for (const root of roots) {
    walk(resolve(projectRoot, root), (file, mtimeMs) => {
      if (NOT_A_BUILD_INPUT.test(file) || mtimeMs <= newestInputMtimeMs) return;
      newestInputMtimeMs = mtimeMs;
      newestInput = relative(projectRoot, file).split(sep).join('/');
    });
  }
  const newestFound = newestInputMtimeMs < 0 ? null : newestInputMtimeMs;

  const entryPath = resolve(projectRoot, entry);
  if (!existsSync(entryPath)) {
    return {
      fresh: false, reason: 'MISSING_BUILD', entry, entryMtimeMs: null,
      newestInput, newestInputMtimeMs: newestFound, staleByMs: null,
    };
  }

  const entryMtimeMs = statSync(entryPath).mtimeMs;
  const staleByMs = newestFound === null ? 0 : newestFound - entryMtimeMs;
  return {
    fresh: staleByMs <= 0,
    reason: staleByMs <= 0 ? 'FRESH' : 'STALE_BUILD',
    entry, entryMtimeMs, newestInput, newestInputMtimeMs: newestFound, staleByMs,
  };
}

/** @param {number|null} ms */
const at = (ms) => (ms === null ? 'never' : new Date(ms).toISOString());

/** @param {FreshnessResult} result */
export function distFreshnessMessage(result) {
  if (result.fresh) {
    return `dist is current: ${result.entry} built ${at(result.entryMtimeMs)}, newest input ${result.newestInput ?? 'none'} at ${at(result.newestInputMtimeMs)}.`;
  }
  const lines = [
    `REFUSING TO RUN: ${result.reason}.`,
    `  This probe drives \`node ${result.entry}\`, so it would report the behavior of that BUILD, not of the working tree.`,
  ];
  if (result.reason === 'MISSING_BUILD') {
    lines.push(`  ${result.entry} does not exist.`);
  } else {
    lines.push(`  ${result.entry} built    ${at(result.entryMtimeMs)}`);
    lines.push(`  ${result.newestInput} modified ${at(result.newestInputMtimeMs)} (${Math.round((result.staleByMs ?? 0) / 1000)}s newer)`);
  }
  lines.push('  Run `npm run build`, then re-run this probe.');
  return lines.join('\n');
}

/**
 * @param {string} [projectRoot]
 * @param {{ entry?: string, roots?: readonly string[] }} [options]
 * @returns {FreshnessResult}
 */
export function assertDistFresh(projectRoot = process.cwd(), options = {}) {
  const result = checkDistFreshness(projectRoot, options);
  if (result.fresh) return result;
  const error = new Error(distFreshnessMessage(result));
  error.name = 'StaleBuildRefusal';
  throw error;
}
