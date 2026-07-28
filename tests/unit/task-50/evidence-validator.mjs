// @ts-check
// tests/unit/task-50/evidence-validator.mjs
// Task 50 — the STRICT evidence contract, and the validator that refuses a
// document which cannot be re-checked.
//
// An evidence file is a claim about a moment. Every field it records is either
// re-derivable now or it is decoration, and decoration is how overclaims survive:
// a report that says "verified against the live editor" while naming no pid, no
// build hash and no oracle reading is unfalsifiable, so nobody can catch it being
// wrong. This validator's whole job is to make the document falsifiable and then
// falsify it where it can.
//
// SIX REJECTIONS ARE REQUIRED BY THE PLAN, and each maps to a failure this
// project actually paid for:
//
//   STALE_TREE           the source moved under the evidence. Task 49 recorded
//                        plugin observations of an 18:55 tree while the repo had
//                        advanced to 19:12, and had to say so in notProven.
//   STALE_PACKAGE        the artifact under test is older than its inputs. Two of
//                        Task 46's four "HIGH" divergences were stale-build
//                        artifacts that read as live defects.
//   STALE_PID            a bare pid is not identity; the kernel recycles them, so
//                        a process record without a start time can never be
//                        re-checked, and one whose start time no longer matches
//                        now names somebody else's process.
//   BAD_HASH             a recorded artifact hash that does not match the bytes.
//   MISSING_ORACLE_LINK  a mutation claim with no INDEPENDENT observation behind
//                        it — the Task 49 oracle that asked the subject.
//   MISSING_CLEANUP_LINK an owned fixture with no verified cleanup receipt — the
//                        `cleanupClean: true` that leaked two materials.
//
// A validator that rejected everything would pass all six of those tests, so the
// suite also proves a well-formed document VALIDATES. That positive control is
// the difference between a gate and a wall.

import { existsSync, readFileSync, statSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';

import { INDEPENDENCE } from './state-oracles.mjs';
import { observeProcess } from './state-oracles.mjs';
import { secretValues } from '../task-49/live-resource-ledger.mjs';

/** Closed refusal taxonomy. A reason not on this list cannot be reported. */
export const REJECTIONS = Object.freeze({
  STALE_TREE: 'STALE_TREE',
  STALE_PACKAGE: 'STALE_PACKAGE',
  STALE_PID: 'STALE_PID',
  BAD_HASH: 'BAD_HASH',
  MISSING_ORACLE_LINK: 'MISSING_ORACLE_LINK',
  MISSING_CLEANUP_LINK: 'MISSING_CLEANUP_LINK',
  DEPENDENT_ORACLE: 'DEPENDENT_ORACLE',
  UNVERIFIED_CLEANUP: 'UNVERIFIED_CLEANUP',
  NO_POSITIVE_CONTROL: 'NO_POSITIVE_CONTROL',
  MOCK_EVIDENCE: 'MOCK_EVIDENCE',
  SECRET_LEAK: 'SECRET_LEAK',
  MISSING_FIELD: 'MISSING_FIELD',
  UNKNOWN_FIELD: 'UNKNOWN_FIELD',
  MALFORMED: 'MALFORMED',
});

/** Top-level keys the document may carry. Anything else is UNKNOWN_FIELD: a typo
 * in a key name silently disables the check that key feeds. */
export const DOCUMENT_KEYS = Object.freeze([
  'task', 'title', 'plan', 'kind', 'generatedAt', 'verdict',
  'environment', 'tree', 'artifacts', 'engine', 'clients',
  'commands', 'transcripts', 'observations', 'claims', 'cleanup',
  'positiveControls', 'notProven', 'notes',
]);

const REQUIRED_KEYS = Object.freeze([
  'task', 'title', 'plan', 'generatedAt', 'environment', 'tree',
  'artifacts', 'engine', 'clients', 'commands', 'transcripts',
  'observations', 'claims', 'cleanup', 'positiveControls',
]);

/** @param {string} file */
function fileSha256(file) {
  return createHash('sha256').update(readFileSync(file)).digest('hex');
}

/**
 * The digest of a set of source files, computed identically at aggregation time
 * and at validation time. `<sha>  <relative path>` lines, sorted, hashed — the
 * same shape `sha256sum` emits, so a human can reproduce it with one command.
 * @param {readonly {path: string, sha256: string}[]} entries
 */
export function treeDigestOf(entries) {
  const lines = [...entries].map((entry) => `${entry.sha256}  ${entry.path}`).sort();
  return createHash('sha256').update(lines.join('\n')).digest('hex');
}

/**
 * @typedef {{ code: string, at: string, detail: string }} Rejection
 */

/**
 * Validate an evidence document.
 *
 * `projectRoot` is where relative artifact/tree paths resolve. `now` and
 * `procRoot` exist so the tests can drive staleness deterministically instead of
 * sleeping, which is how a timing check ends up flaky and then ends up deleted.
 * @param {any} document
 * @param {{ projectRoot?: string, procRoot?: string, env?: NodeJS.ProcessEnv,
 *   skipFilesystem?: boolean }} [options]
 * @returns {{ valid: boolean, rejections: Rejection[], checked: Record<string, number> }}
 */
export function validateEvidence(document, options = {}) {
  const projectRoot = options.projectRoot ?? process.cwd();
  /** @type {Rejection[]} */
  const rejections = [];
  /** @param {string} code @param {string} at @param {string} detail */
  const reject = (code, at, detail) => { rejections.push({ code, at, detail }); };
  /** @type {Record<string, number>} */
  const checked = { artifacts: 0, processes: 0, observations: 0, claims: 0, cleanup: 0, treeFiles: 0 };

  if (document === null || typeof document !== 'object' || Array.isArray(document)) {
    return { valid: false, rejections: [{ code: REJECTIONS.MALFORMED, at: '/', detail: 'evidence must be a JSON object' }], checked };
  }

  for (const key of Object.keys(document)) {
    if (!DOCUMENT_KEYS.includes(key)) reject(REJECTIONS.UNKNOWN_FIELD, `/${key}`, `"${key}" is not part of the evidence contract; an unrecognised key is usually a typo that silently disables the check it was meant to feed`);
  }
  for (const key of REQUIRED_KEYS) {
    if (document[key] === undefined) reject(REJECTIONS.MISSING_FIELD, `/${key}`, `required section "${key}" is absent`);
  }
  if (rejections.some((entry) => entry.code === REJECTIONS.MISSING_FIELD)) {
    return { valid: false, rejections, checked };
  }

  // ── mock mode is never evidence ────────────────────────────────────────────
  if (document.environment?.mockUnrealConnection === true) {
    reject(REJECTIONS.MOCK_EVIDENCE, '/environment/mockUnrealConnection', 'a mocked run is not live evidence and must never be recorded as one');
  }

  // ── secrets ────────────────────────────────────────────────────────────────
  const secrets = secretValues(options.env ?? process.env);
  if (secrets.length > 0) {
    const serialized = JSON.stringify(document);
    for (const secret of secrets) {
      if (serialized.includes(secret)) reject(REJECTIONS.SECRET_LEAK, '/', 'a capability token value appears verbatim in the document');
    }
  }

  // ── STALE_TREE ─────────────────────────────────────────────────────────────
  const tree = document.tree;
  if (!Array.isArray(tree?.files) || typeof tree?.sourceDigest !== 'string') {
    reject(REJECTIONS.MALFORMED, '/tree', 'tree must record { files: [{path, sha256}], sourceDigest } so the digest can be recomputed');
  } else {
    checked.treeFiles = tree.files.length;
    if (tree.files.length === 0) {
      reject(REJECTIONS.STALE_TREE, '/tree/files', 'no source files were recorded, so "which tree was this?" is unanswerable and staleness is undetectable');
    }
    const recomputedClaim = treeDigestOf(tree.files);
    if (recomputedClaim !== tree.sourceDigest) {
      reject(REJECTIONS.MALFORMED, '/tree/sourceDigest', `the recorded digest does not summarise the recorded file list (${tree.sourceDigest.slice(0, 12)} vs ${recomputedClaim.slice(0, 12)})`);
    } else if (options.skipFilesystem !== true) {
      /** @type {string[]} */
      const moved = [];
      for (const entry of tree.files) {
        const file = resolve(projectRoot, entry.path);
        if (!existsSync(file)) { moved.push(`${entry.path} (deleted)`); continue; }
        if (fileSha256(file) !== entry.sha256) moved.push(entry.path);
      }
      if (moved.length > 0) {
        reject(REJECTIONS.STALE_TREE, '/tree/files', `${moved.length} recorded source file(s) no longer match the tree: ${moved.slice(0, 5).join(', ')}${moved.length > 5 ? ', ...' : ''}. Observations of the old tree cannot be reported as observations of this one.`);
      }
    }
  }

  // ── STALE_PACKAGE + BAD_HASH ───────────────────────────────────────────────
  if (!Array.isArray(document.artifacts) || document.artifacts.length === 0) {
    reject(REJECTIONS.MISSING_FIELD, '/artifacts', 'at least one built artifact must be named; evidence that does not say which bytes ran cannot be re-checked');
  } else {
    for (const [index, artifact] of document.artifacts.entries()) {
      checked.artifacts += 1;
      const at = `/artifacts/${index}`;
      if (typeof artifact?.path !== 'string' || typeof artifact?.sha256 !== 'string') {
        reject(REJECTIONS.MALFORMED, at, 'an artifact must record { path, sha256, builtAtMs, inputsNewestAtMs }');
        continue;
      }
      if (typeof artifact.builtAtMs !== 'number' || typeof artifact.inputsNewestAtMs !== 'number') {
        reject(REJECTIONS.STALE_PACKAGE, at, `"${artifact.path}" records no build/input timestamps, so it is impossible to tell whether the artifact under test was behind its sources`);
        continue;
      }
      if (artifact.inputsNewestAtMs > artifact.builtAtMs) {
        reject(REJECTIONS.STALE_PACKAGE, at, `"${artifact.path}" was built ${new Date(artifact.builtAtMs).toISOString()} but its newest input (${artifact.inputsNewest ?? 'unknown'}) is ${new Date(artifact.inputsNewestAtMs).toISOString()} — ${Math.round((artifact.inputsNewestAtMs - artifact.builtAtMs) / 1000)}s newer. This measured the BUILD, not the tree.`);
      }
      if (options.skipFilesystem === true) continue;
      const file = resolve(projectRoot, artifact.path);
      if (!existsSync(file)) {
        reject(REJECTIONS.BAD_HASH, at, `"${artifact.path}" no longer exists, so its recorded hash cannot be confirmed`);
        continue;
      }
      const actual = fileSha256(file);
      if (actual !== artifact.sha256) {
        reject(REJECTIONS.BAD_HASH, at, `"${artifact.path}" hashes ${actual.slice(0, 12)} now but the evidence records ${String(artifact.sha256).slice(0, 12)}`);
      }
    }
  }

  // ── STALE_PID ──────────────────────────────────────────────────────────────
  const processes = Array.isArray(document.environment?.processes) ? document.environment.processes : null;
  if (processes === null) {
    reject(REJECTIONS.MISSING_FIELD, '/environment/processes', 'the processes this run drove or spawned must be recorded');
  } else {
    for (const [index, entry] of processes.entries()) {
      checked.processes += 1;
      const at = `/environment/processes/${index}`;
      if (typeof entry?.pid !== 'number') {
        reject(REJECTIONS.MALFORMED, at, 'a process record needs a numeric pid');
        continue;
      }
      if (typeof entry.startTicks !== 'number') {
        // A pid with no start time is unfalsifiable the moment the process exits.
        reject(REJECTIONS.STALE_PID, at, `pid ${entry.pid} is recorded without a start time; the kernel recycles pids, so this record can never be re-checked`);
        continue;
      }
      const live = observeProcess({ pid: entry.pid, procRoot: options.procRoot });
      const stillRunning = live.present === true || live.detail.zombie === true;
      if (stillRunning && Number(live.detail.startTicks) !== entry.startTicks) {
        reject(REJECTIONS.STALE_PID, at, `pid ${entry.pid} is alive but started at ${String(live.detail.startTicks)} ticks, not the recorded ${entry.startTicks}; this record now names a different process`);
      }
    }
  }

  // ── observations index ─────────────────────────────────────────────────────
  /** @type {Map<string, any>} */
  const observations = new Map();
  if (!Array.isArray(document.observations)) {
    reject(REJECTIONS.MALFORMED, '/observations', 'observations must be an array');
  } else {
    for (const [index, entry] of document.observations.entries()) {
      checked.observations += 1;
      if (typeof entry?.id !== 'string') {
        reject(REJECTIONS.MALFORMED, `/observations/${index}`, 'every observation needs an id a claim can reference');
        continue;
      }
      if (typeof entry.mechanism !== 'string' || typeof entry.independence !== 'string') {
        reject(REJECTIONS.MALFORMED, `/observations/${index}`, 'every observation must state the mechanism it read and how independent that mechanism is');
        continue;
      }
      observations.set(entry.id, entry);
    }
  }

  /** @type {Map<string, any>} */
  const cleanups = new Map();
  if (!Array.isArray(document.cleanup)) {
    reject(REJECTIONS.MALFORMED, '/cleanup', 'cleanup must be an array of receipts');
  } else {
    for (const [index, entry] of document.cleanup.entries()) {
      checked.cleanup += 1;
      if (typeof entry?.id !== 'string' || typeof entry.owned !== 'string') {
        reject(REJECTIONS.MALFORMED, `/cleanup/${index}`, 'a cleanup receipt needs { id, owned, verifiedBy, pass }');
        continue;
      }
      if (typeof entry.verifiedBy !== 'string' || !observations.has(entry.verifiedBy)) {
        reject(REJECTIONS.UNVERIFIED_CLEANUP, `/cleanup/${index}`, `cleanup "${entry.id}" cites no post-cleanup observation; a delete response is the claim, not the proof — this is exactly the shape that reported cleanupClean:true over two leaked materials`);
        continue;
      }
      cleanups.set(entry.id, entry);
    }
  }

  // ── MISSING_ORACLE_LINK + MISSING_CLEANUP_LINK ─────────────────────────────
  if (!Array.isArray(document.claims)) {
    reject(REJECTIONS.MALFORMED, '/claims', 'claims must be an array');
  } else {
    for (const [index, claim] of document.claims.entries()) {
      checked.claims += 1;
      const at = `/claims/${index}`;
      if (typeof claim?.id !== 'string' || typeof claim.effect !== 'string') {
        reject(REJECTIONS.MALFORMED, at, 'a claim needs { id, effect, outcome, oracleRefs }');
        continue;
      }
      const mutating = claim.effect !== 'unchanged';
      const refs = Array.isArray(claim.oracleRefs) ? claim.oracleRefs : [];
      /** @type {any[]} */
      const resolved = refs.map((/** @type {string} */ ref) => observations.get(ref)).filter((/** @type {any} */ entry) => entry !== undefined);
      if (refs.length === 0 || resolved.length !== refs.length) {
        reject(REJECTIONS.MISSING_ORACLE_LINK, at, `claim "${claim.id}" ${refs.length === 0 ? 'cites no oracle observation' : 'cites an observation id that does not exist'}; a response is not proof of itself`);
        continue;
      }
      if (mutating && !resolved.some((entry) => entry.independence === INDEPENDENCE.OUT_OF_BAND)) {
        reject(REJECTIONS.DEPENDENT_ORACLE, at, `claim "${claim.id}" is a mutation proven only by ${resolved.map((entry) => entry.independence).join(', ')} readings; those share the plugin and the queue with the mutation, so they can be wrong for the same reason`);
      }
      if (mutating && !resolved.some((entry) => entry.phase === 'pre')) {
        reject(REJECTIONS.MISSING_ORACLE_LINK, at, `claim "${claim.id}" cites no PRE-state observation; "present afterwards" cannot distinguish this call from a leftover`);
      }
      // Cleanup is demanded by the OBSERVATIONS, not by the claim's own label.
      // "effect: created" plus "outcome: error" describes a refusal that made
      // nothing, and there is no fixture to remove; demanding a receipt for it
      // would train people to attach empty receipts. The trigger is therefore the
      // post-state reading: if something IS there, you must prove you removed it.
      // That is also strictly harder to evade than the label was — a claim cannot
      // relabel its way out of a leak the oracle can see.
      const owns = (claim.effect === 'created' || claim.effect === 'modified')
        && resolved.some((entry) => entry.phase === 'post' && entry.present === true);
      if (owns) {
        if (typeof claim.cleanupRef !== 'string' || !cleanups.has(claim.cleanupRef)) {
          reject(REJECTIONS.MISSING_CLEANUP_LINK, at, `claim "${claim.id}" created or modified owned state but cites no verified cleanup receipt; a fixture nobody proved removed changes the result of the NEXT run`);
        } else if (cleanups.get(claim.cleanupRef).pass !== true) {
          reject(REJECTIONS.UNVERIFIED_CLEANUP, at, `claim "${claim.id}" cites cleanup "${claim.cleanupRef}", which did not pass: ${String(cleanups.get(claim.cleanupRef).reason ?? 'no reason recorded')}`);
        }
      }
    }
  }

  // ── positive controls ──────────────────────────────────────────────────────
  const controls = document.positiveControls;
  if (controls?.ok !== true || !Array.isArray(controls?.mechanisms) || controls.mechanisms.length === 0) {
    reject(REJECTIONS.NO_POSITIVE_CONTROL, '/positiveControls', 'no mechanism was watched reporting BOTH a present and an absent reading; without that, a permanently blind oracle satisfies every absence assertion in the document');
  }

  return { valid: rejections.length === 0, rejections, checked };
}

/** Human-readable refusal, for a CLI that must tell an operator what to do. @param {ReturnType<typeof validateEvidence>} result */
export function describeRejections(result) {
  if (result.valid) return 'evidence VALID: every recorded hash, build, pid, oracle link and cleanup link re-checked.';
  const lines = ['evidence REJECTED:'];
  for (const entry of result.rejections) lines.push(`  ${entry.code} at ${entry.at}\n    ${entry.detail}`);
  return lines.join('\n');
}

/**
 * Snapshot the files whose identity the evidence depends on.
 * @param {{ projectRoot: string, files: readonly string[] }} spec
 */
export function snapshotTree(spec) {
  const entries = spec.files
    .filter((file) => existsSync(resolve(spec.projectRoot, file)))
    .map((file) => ({ path: file, sha256: fileSha256(resolve(spec.projectRoot, file)) }));
  return { files: entries, sourceDigest: treeDigestOf(entries) };
}

/**
 * Describe a built artifact and, crucially, how far behind its inputs it is.
 * @param {{ projectRoot: string, path: string, inputsNewest?: string|null, inputsNewestAtMs?: number|null }} spec
 */
export function snapshotArtifact(spec) {
  const file = resolve(spec.projectRoot, spec.path);
  const exists = existsSync(file);
  return {
    path: spec.path,
    sha256: exists ? fileSha256(file) : null,
    builtAtMs: exists ? statSync(file).mtimeMs : null,
    inputsNewest: spec.inputsNewest ?? null,
    inputsNewestAtMs: spec.inputsNewestAtMs ?? null,
  };
}
