// @ts-check
// tests/unit/engine-external-blocker/external-blocker-validator.mjs
// Task 61 — the gate that refuses a blocker which could be read as a pass.
//
// A validator nobody has shown can reject is decoration. This one is written
// against three specific forgeries, each of which would let a missing UE minor
// disappear from the final support claim:
//
//   A PASS FIELD. `{ status: 'BLOCKED_EXTERNAL', ..., pass: true }` validates
//   under any schema that only checks the fields it knows about. Some later
//   aggregator sums `pass` and the blocker becomes a green row. So the scan is
//   INVERTED: it walks the whole record and refuses any key or scalar that reads
//   as success, at any depth, including keys this file has never seen.
//
//   A SYNTHETIC HASH. A hash that was typed rather than computed makes the record
//   unfalsifiable — nothing changes when the host changes. Both digests are
//   therefore RECOMPUTED, from the recorded file list and from the filesystem.
//
//   A WRONG-MINOR PATH. This is the substitution the plan forbids in prose:
//   5.3.2 standing in for 5.4. In a record it looks like a root cited under the
//   wrong minor, or an operator instruction pointing at an engine that already
//   contains something else. Every cited path is re-read and compared to what it
//   actually contains.
//
// AND A POSITIVE CONTROL. A validator that refused everything would pass all
// three rejection tests while gating nothing, so the suite also proves a
// well-formed blocker is ACCEPTED. That is the difference between a gate and a
// wall — the same lesson Task 50 encodes for its own six rejections.
//
// FAIL-CLOSED. An invalid record does not stop being a blocker: the aggregate
// keeps counting it while refusing the document. There is no code path here that
// converts a record into a skip or a pass, and `AGGREGATE_STATUSES` deliberately
// contains no token meaning either.

import { createHash } from 'node:crypto';
import { existsSync, readFileSync } from 'node:fs';

import {
  ABSENCE_REASON, ADVERTISED_RANGE, BLOCKED_TASKS, BLOCKER_SEVERITY, BLOCKER_STATUS,
  DETECTION_OUTCOME, REQUIRED_ENGINE_FILES, digestOfFiles, minorContainedAt,
} from './external-blocker.mjs';

/** Closed refusal taxonomy. A reason not on this list cannot be reported. */
export const BLOCKER_REJECTIONS = Object.freeze({
  MALFORMED: 'MALFORMED',
  MISSING_FIELD: 'MISSING_FIELD',
  UNKNOWN_FIELD: 'UNKNOWN_FIELD',
  WRONG_STATUS: 'WRONG_STATUS',
  /** A `pass`/`ok`/`success`/`verdict` field, or a scalar that reads as one. */
  PASS_FIELD: 'PASS_FIELD',
  /** A `skip`/`waived`/`optional` field — "never a skip" is enforced, not requested. */
  SKIP_FIELD: 'SKIP_FIELD',
  /** A hash that was typed rather than computed. */
  SYNTHETIC_HASH: 'SYNTHETIC_HASH',
  /** A recorded hash that does not match the bytes it names. */
  HASH_MISMATCH: 'HASH_MISMATCH',
  /** A path cited under a minor it does not contain. */
  WRONG_MINOR_PATH: 'WRONG_MINOR_PATH',
  /** The required `Build.version` input does not describe the subject minor. */
  WRONG_MINOR_INPUT: 'WRONG_MINOR_INPUT',
  /** The blocker is refuted: the minor it claims is missing is installed. */
  ROOT_NOW_PRESENT: 'ROOT_NOW_PRESENT',
  NO_DETECTION_COMMAND: 'NO_DETECTION_COMMAND',
  NO_DETECTION_TIME: 'NO_DETECTION_TIME',
  NO_TREE_HASH: 'NO_TREE_HASH',
  NO_OPERATOR_INPUT: 'NO_OPERATOR_INPUT',
  NO_REMEDIATION: 'NO_REMEDIATION',
  NOT_BLOCKING: 'NOT_BLOCKING',
  DUPLICATE_RECORD: 'DUPLICATE_RECORD',
  /** A minor the detector reported missing for which no record was emitted. */
  UNRECORDED_MISSING_MINOR: 'UNRECORDED_MISSING_MINOR',
});

/** Top-level keys a record may carry. Anything else is a typo that silently disables a check. */
export const RECORD_KEYS = Object.freeze([
  'recordId', 'status', 'severity', 'blocksTasks', 'blocksClaims', 'subject',
  'detection', 'absence', 'requiredOperatorInput', 'remediation', 'consequence',
]);

/**
 * Key names that would let a reader — or an aggregator — take this record for a
 * success or a waiver. Compared after lowercasing and stripping separators, so
 * `is_ok`, `isOk` and `IS-OK` are all the same key.
 */
export const FORBIDDEN_PASS_KEYS = Object.freeze([
  'pass', 'passed', 'passes', 'passing', 'ispass', 'ok', 'isok', 'okay', 'success',
  'successful', 'succeeded', 'green', 'verdict', 'certified', 'certification',
  'verified', 'satisfied', 'satisfies', 'available', 'supported', 'complete',
  'completed', 'done', 'ready', 'approved', 'healthy', 'allowed', 'acceptable',
]);

/** Key names that would downgrade the blocker to something ignorable. */
export const FORBIDDEN_SKIP_KEYS = Object.freeze([
  'skip', 'skipped', 'skippable', 'waived', 'waiver', 'ignored', 'ignorable',
  'optional', 'nonblocking', 'nonfatal', 'advisory', 'deferred', 'exempt', 'exempted',
]);

/** Scalar values that read as a verdict wherever they appear. Whole-string, case-insensitive. */
export const FORBIDDEN_VALUES = Object.freeze([
  'pass', 'passed', 'passing', 'ok', 'okay', 'success', 'successful', 'succeeded',
  'green', 'skip', 'skipped', 'waived', 'verified', 'certified', 'satisfied',
]);

/** Hashes of strings somebody types when inventing one. Refused even without filesystem access. */
const PLACEHOLDER_DIGESTS = Object.freeze(new Set(
  ['', '0', 'test', 'todo', 'fake', 'synthetic', 'placeholder', 'unknown', 'none']
    .map((seed) => createHash('sha256').update(seed).digest('hex')),
));

/** Real filesystem access. Injectable so every test runs offline and host-independent. */
export const realBlockerIo = Object.freeze({
  /** @param {string} path @returns {string|null} */
  hashOf: (path) => {
    try {
      return createHash('sha256').update(readFileSync(path)).digest('hex');
    } catch {
      return null;
    }
  },
  /** @param {string} buildVersionFile @returns {string|null} */
  minorAt: (buildVersionFile) => minorContainedAt(buildVersionFile),
  /** @param {string} path */
  exists: (path) => existsSync(path),
});

const normalizeKey = (/** @type {string} */ key) => key.toLowerCase().replace(/[^a-z0-9]/gu, '');

/**
 * Is this a hash somebody computed, or one somebody typed?
 *
 * Shape alone cannot prove provenance — that is what recomputation is for — but it
 * catches the inventions that recomputation would only catch when the filesystem
 * is reachable, and it makes the refusal legible in the report.
 * @param {unknown} value
 * @returns {string|null} the reason it is synthetic, or null
 */
export function syntheticHashReason(value) {
  if (typeof value !== 'string') return 'a digest must be a string';
  if (!/^[0-9a-f]{64}$/u.test(value)) return `"${value.slice(0, 24)}" is not 64 lowercase hex characters, so it is not a sha256 of anything`;
  if (new Set(value).size <= 2) return `"${value.slice(0, 16)}..." is a repeated character, which no real sha256 of a source file is`;
  if (PLACEHOLDER_DIGESTS.has(value)) return `"${value.slice(0, 16)}..." is the sha256 of an empty or placeholder string, not of the file it claims to summarise`;
  return null;
}

/**
 * Validate ONE BLOCKED_EXTERNAL record.
 * @param {any} record
 * @param {{ io?: typeof realBlockerIo, skipFilesystem?: boolean, now?: () => Date }} [options]
 * @returns {{ outcome: 'ACCEPTED'|'REJECTED', rejections: {code: string, at: string, detail: string}[], checked: Record<string, number> }}
 */
export function validateBlockerRecord(record, options = {}) {
  const io = options.io ?? realBlockerIo;
  const now = (options.now ?? (() => new Date()))();
  /** @type {{code: string, at: string, detail: string}[]} */
  const rejections = [];
  /** @param {string} code @param {string} at @param {string} detail */
  const reject = (code, at, detail) => { rejections.push({ code, at, detail }); };
  /** @type {Record<string, number>} */
  const checked = { keys: 0, hashes: 0, citedPaths: 0 };
  const done = () => ({
    outcome: /** @type {'ACCEPTED'|'REJECTED'} */ (rejections.length === 0 ? 'ACCEPTED' : 'REJECTED'),
    rejections,
    checked,
  });

  if (record === null || typeof record !== 'object' || Array.isArray(record)) {
    reject(BLOCKER_REJECTIONS.MALFORMED, '/', 'a blocker record must be a JSON object');
    return done();
  }

  // ── the inverted scan: no pass, no skip, anywhere, at any depth ─────────────
  /** @param {unknown} node @param {string} at @param {number} depth */
  const scan = (node, at, depth) => {
    if (depth > 12 || node === null) return;
    if (typeof node === 'string') {
      if (FORBIDDEN_VALUES.includes(node.trim().toLowerCase())) {
        reject(BLOCKER_REJECTIONS.PASS_FIELD, at, `the value "${node}" reads as a verdict; a blocker must carry no token that an aggregator could sum as a success`);
      }
      return;
    }
    if (Array.isArray(node)) {
      node.forEach((entry, index) => { scan(entry, `${at}/${index}`, depth + 1); });
      return;
    }
    if (typeof node !== 'object') return;
    for (const [key, value] of Object.entries(node)) {
      checked.keys += 1;
      const normalized = normalizeKey(key);
      if (FORBIDDEN_PASS_KEYS.includes(normalized)) {
        reject(BLOCKER_REJECTIONS.PASS_FIELD, `${at}/${key}`, `"${key}" is a pass/verdict field. A BLOCKED_EXTERNAL record must be structurally incapable of being read as success: it is never a pass, never a fail, and never a fabricated result.`);
      }
      if (FORBIDDEN_SKIP_KEYS.includes(normalized)) {
        reject(BLOCKER_REJECTIONS.SKIP_FIELD, `${at}/${key}`, `"${key}" would downgrade this blocker to something ignorable. A missing advertised minor is not optional and cannot be waived inside its own record.`);
      }
      scan(value, `${at}/${key}`, depth + 1);
    }
  };
  scan(record, '', 0);

  // ── shape ──────────────────────────────────────────────────────────────────
  for (const key of Object.keys(record)) {
    if (!RECORD_KEYS.includes(key)) reject(BLOCKER_REJECTIONS.UNKNOWN_FIELD, `/${key}`, `"${key}" is not part of the blocker contract; an unrecognised key is usually a typo that silently disables the check it was meant to feed`);
  }
  for (const key of RECORD_KEYS) {
    if (record[key] === undefined) reject(BLOCKER_REJECTIONS.MISSING_FIELD, `/${key}`, `required section "${key}" is absent`);
  }
  if (rejections.some((entry) => entry.code === BLOCKER_REJECTIONS.MISSING_FIELD)) return done();

  if (record.status !== BLOCKER_STATUS) {
    reject(BLOCKER_REJECTIONS.WRONG_STATUS, '/status', `status must be exactly "${BLOCKER_STATUS}"; "${String(record.status)}" is a different claim`);
  }
  if (record.severity !== BLOCKER_SEVERITY) {
    reject(BLOCKER_REJECTIONS.NOT_BLOCKING, '/severity', `severity must be exactly "${BLOCKER_SEVERITY}"; anything softer lets an advertised minor go uncertified without blocking the claim that covers it`);
  }
  if (!Array.isArray(record.blocksTasks) || record.blocksTasks.length === 0) {
    reject(BLOCKER_REJECTIONS.NOT_BLOCKING, '/blocksTasks', `a blocker that blocks nothing is a note; this record must name the tasks it stops (${BLOCKED_TASKS.join(', ')})`);
  }
  if (!Array.isArray(record.blocksClaims) || record.blocksClaims.length === 0) {
    reject(BLOCKER_REJECTIONS.NOT_BLOCKING, '/blocksClaims', 'the record must name the public claims it withholds, or nothing downstream knows what to stop asserting');
  }

  const minorKey = record.subject?.minorKey;
  if (typeof minorKey !== 'string' || !/^\d+\.\d+$/u.test(minorKey)) {
    reject(BLOCKER_REJECTIONS.MALFORMED, '/subject/minorKey', 'the subject must name a `<major>.<minor>` key');
    return done();
  }
  if (record.subject?.advertisedRange !== ADVERTISED_RANGE) {
    reject(BLOCKER_REJECTIONS.MALFORMED, '/subject/advertisedRange', `the advertised range must be recorded as "${ADVERTISED_RANGE}"; narrowing it here would make the gap disappear instead of reporting it`);
  }

  // ── detection: command, time, tree hash ────────────────────────────────────
  const detection = record.detection;
  if (typeof detection?.command !== 'string' || detection.command.trim() === '') {
    reject(BLOCKER_REJECTIONS.NO_DETECTION_COMMAND, '/detection/command', 'the record must name the command that proved the absence, or nobody can re-run it');
  }
  if (typeof detection?.reproducibleShellCommand !== 'string' || detection.reproducibleShellCommand.trim() === '') {
    reject(BLOCKER_REJECTIONS.NO_DETECTION_COMMAND, '/detection/reproducibleShellCommand', 'a second, tool-independent command must be given so the absence can be confirmed without trusting this repository');
  }
  if (typeof detection?.commandExitCode !== 'number' || detection.commandExitCode === 0) {
    reject(BLOCKER_REJECTIONS.NOT_BLOCKING, '/detection/commandExitCode', 'the detector must have exited non-zero: exit 0 means nothing was missing, so a blocker carrying it is describing a different host');
  }
  if (detection?.outcome !== DETECTION_OUTCOME) {
    reject(BLOCKER_REJECTIONS.MALFORMED, '/detection/outcome', `detection outcome must be "${DETECTION_OUTCOME}" — an oracle that could not look is not an oracle that saw nothing`);
  }
  const detectedAt = detection?.detectedAt;
  const detectedMs = typeof detectedAt === 'string' ? Date.parse(detectedAt) : Number.NaN;
  if (!Number.isFinite(detectedMs) || typeof detectedAt !== 'string' || new Date(detectedMs).toISOString() !== detectedAt) {
    reject(BLOCKER_REJECTIONS.NO_DETECTION_TIME, '/detection/detectedAt', 'the detection time must be a full ISO-8601 instant; a record that cannot say WHEN it looked cannot be re-checked against a host that has since changed');
  } else if (detectedMs > now.getTime() + 60_000) {
    reject(BLOCKER_REJECTIONS.NO_DETECTION_TIME, '/detection/detectedAt', `the detection time ${detectedAt} is in the future, so it was written rather than observed`);
  }

  /** @param {any} entries @param {string|undefined} digest @param {string} at */
  const checkDigest = (entries, digest, at) => {
    if (!Array.isArray(entries) || entries.length === 0) {
      reject(BLOCKER_REJECTIONS.NO_TREE_HASH, at, 'no files were recorded, so the digest summarises nothing and staleness is undetectable');
      return;
    }
    const synthetic = syntheticHashReason(digest);
    if (synthetic !== null) {
      reject(BLOCKER_REJECTIONS.SYNTHETIC_HASH, `${at}/digest`, synthetic);
      return;
    }
    checked.hashes += 1;
    const recomputed = digestOfFiles(entries);
    if (recomputed !== digest) {
      reject(BLOCKER_REJECTIONS.HASH_MISMATCH, `${at}/digest`, `the recorded digest does not summarise the recorded file list (${String(digest).slice(0, 12)} vs ${recomputed.slice(0, 12)}); it was not computed from these files`);
    }
    for (const [index, entry] of entries.entries()) {
      const reason = syntheticHashReason(entry?.sha256);
      if (reason !== null) {
        reject(BLOCKER_REJECTIONS.SYNTHETIC_HASH, `${at}/${index}`, `${String(entry?.path)}: ${reason}`);
        continue;
      }
      checked.hashes += 1;
      if (options.skipFilesystem === true) continue;
      const actual = io.hashOf(entry.path);
      if (actual === null) {
        reject(BLOCKER_REJECTIONS.HASH_MISMATCH, `${at}/${index}`, `"${entry.path}" cannot be read now, so its recorded hash cannot be confirmed`);
      } else if (actual !== entry.sha256) {
        reject(BLOCKER_REJECTIONS.HASH_MISMATCH, `${at}/${index}`, `"${entry.path}" hashes ${actual.slice(0, 12)} now but the record says ${String(entry.sha256).slice(0, 12)}`);
      }
    }
  };

  checkDigest(detection?.detectorTree?.files, detection?.detectorTree?.sourceDigest, '/detection/detectorTree/files');

  const roots = detection?.hostEngineTree?.roots;
  checkDigest(
    Array.isArray(roots)
      ? roots.filter((entry) => typeof entry?.buildVersionSha256 === 'string')
        .map((entry) => ({ path: entry.buildVersionFile, sha256: entry.buildVersionSha256 }))
      : roots,
    detection?.hostEngineTree?.digest,
    '/detection/hostEngineTree/roots',
  );

  // ── wrong-minor paths ──────────────────────────────────────────────────────
  if (Array.isArray(roots) && options.skipFilesystem !== true) {
    for (const [index, entry] of roots.entries()) {
      if (typeof entry?.buildVersionFile !== 'string') continue;
      checked.citedPaths += 1;
      const contained = io.minorAt(entry.buildVersionFile);
      if (contained === null) continue;
      if (typeof entry.minorKey === 'string' && entry.minorKey !== contained) {
        reject(BLOCKER_REJECTIONS.WRONG_MINOR_PATH, `/detection/hostEngineTree/roots/${index}`, `"${entry.root}" is filed as minor ${entry.minorKey} but ${REQUIRED_ENGINE_FILES.buildVersion} there says ${contained}. A version read from anywhere other than the engine's own file is not an identity.`);
      }
      if (contained === minorKey) {
        reject(BLOCKER_REJECTIONS.ROOT_NOW_PRESENT, `/detection/hostEngineTree/roots/${index}`, `"${entry.root}" contains ${contained}, which is the very minor this record calls absent. The blocker is refuted and must be re-derived, not re-published.`);
      }
    }
  }
  if (Array.isArray(record.absence?.rootsContainingSubjectMinor) && record.absence.rootsContainingSubjectMinor.length > 0) {
    reject(BLOCKER_REJECTIONS.ROOT_NOW_PRESENT, '/absence/rootsContainingSubjectMinor', `the record names ${record.absence.rootsContainingSubjectMinor.length} root(s) containing ${minorKey} while claiming it is absent`);
  }
  if (record.absence?.reason !== ABSENCE_REASON) {
    reject(BLOCKER_REJECTIONS.MALFORMED, '/absence/reason', `the absence reason must be "${ABSENCE_REASON}"; a root that failed identification is a different problem and must not be filed as a missing minor`);
  }
  const substitutes = record.absence?.notSubstitutableBy;
  if (!Array.isArray(substitutes) || substitutes.length === 0) {
    reject(BLOCKER_REJECTIONS.MISSING_FIELD, '/absence/notSubstitutableBy', 'the record must name the installed minors it refuses to substitute, or the next reader will reach for the nearest one');
  } else {
    for (const [index, entry] of substitutes.entries()) {
      if (entry?.minorKey === minorKey) {
        reject(BLOCKER_REJECTIONS.WRONG_MINOR_PATH, `/absence/notSubstitutableBy/${index}`, `"${entry.root}" is listed as a non-substitute for ${minorKey} while being filed as ${minorKey} itself`);
      }
    }
  }

  // ── the required operator input ────────────────────────────────────────────
  const input = record.requiredOperatorInput;
  if (input === null || typeof input !== 'object') {
    reject(BLOCKER_REJECTIONS.NO_OPERATOR_INPUT, '/requiredOperatorInput', 'the record must state exactly what an operator has to supply');
    return done();
  }
  if (input.requiredFile !== REQUIRED_ENGINE_FILES.buildVersion) {
    reject(BLOCKER_REJECTIONS.NO_OPERATOR_INPUT, '/requiredOperatorInput/requiredFile', `the required input must be ${REQUIRED_ENGINE_FILES.buildVersion} — the only file the detector reads as identity`);
  }
  const fields = input.requiredFields;
  const expectedMinor = Number(minorKey.split('.')[1]);
  if (fields?.MajorVersion !== 5 || fields?.MinorVersion !== expectedMinor) {
    reject(BLOCKER_REJECTIONS.WRONG_MINOR_INPUT, '/requiredOperatorInput/requiredFields', `the required Build.version content must declare MajorVersion 5 and MinorVersion ${expectedMinor} for a ${minorKey} record; it declares ${String(fields?.MajorVersion)}.${String(fields?.MinorVersion)}, which would be satisfied by the wrong engine`);
  }
  if (fields !== null && typeof fields === 'object' && fields.PatchVersion !== null) {
    reject(BLOCKER_REJECTIONS.WRONG_MINOR_INPUT, '/requiredOperatorInput/requiredFields/PatchVersion', 'PatchVersion must be null (unconstrained): naming a patch would invent a release, and any patch of the minor satisfies detection');
  }
  if (typeof input.acceptanceCommand !== 'string' || input.acceptanceCommand.trim() === '') {
    reject(BLOCKER_REJECTIONS.NO_OPERATOR_INPUT, '/requiredOperatorInput/acceptanceCommand', 'the record must name the command that will show the input has been supplied, or nobody can tell when the blocker is cleared');
  }
  if (!Array.isArray(input.alsoRequired) || input.alsoRequired.length === 0) {
    reject(BLOCKER_REJECTIONS.NO_OPERATOR_INPUT, '/requiredOperatorInput/alsoRequired', 'a hand-written Build.version would parse; the record must also demand the Version.h agreement, RunUAT and the editor binary that make the root usable');
  }
  if (typeof input.engineRoot !== 'string' || input.engineRoot.trim() === '') {
    reject(BLOCKER_REJECTIONS.NO_OPERATOR_INPUT, '/requiredOperatorInput/engineRoot', 'the record must name the engine root that has to exist');
  } else if (options.skipFilesystem !== true) {
    checked.citedPaths += 1;
    const contained = io.minorAt(`${input.engineRoot}/${REQUIRED_ENGINE_FILES.buildVersion}`);
    if (contained === minorKey) {
      reject(BLOCKER_REJECTIONS.ROOT_NOW_PRESENT, '/requiredOperatorInput/engineRoot', `"${input.engineRoot}" already contains ${minorKey}; the required input exists and this record no longer describes the host`);
    } else if (contained !== null) {
      reject(BLOCKER_REJECTIONS.WRONG_MINOR_PATH, '/requiredOperatorInput/engineRoot', `"${input.engineRoot}" already contains ${contained}, not ${minorKey}. Pointing the remediation at another minor's root is the substitution this record exists to refuse.`);
    }
  }
  if (typeof input.structuralTemplateFrom === 'string' && options.skipFilesystem !== true) {
    checked.citedPaths += 1;
    const templateMinor = io.minorAt(input.structuralTemplateFrom);
    if (templateMinor === minorKey) {
      reject(BLOCKER_REJECTIONS.WRONG_MINOR_PATH, '/requiredOperatorInput/structuralTemplateFrom', `the structural template "${input.structuralTemplateFrom}" contains ${minorKey} itself, so it is presented as a shape while being evidence of presence`);
    }
    const templateHash = io.hashOf(input.structuralTemplateFrom);
    if (typeof input.structuralTemplateSha256 === 'string' && templateHash !== null && templateHash !== input.structuralTemplateSha256) {
      reject(BLOCKER_REJECTIONS.HASH_MISMATCH, '/requiredOperatorInput/structuralTemplateSha256', `the template hashes ${templateHash.slice(0, 12)} now but the record says ${input.structuralTemplateSha256.slice(0, 12)}`);
    }
  }

  // ── remediation ────────────────────────────────────────────────────────────
  if (!Array.isArray(record.remediation?.steps) || record.remediation.steps.length === 0) {
    reject(BLOCKER_REJECTIONS.NO_REMEDIATION, '/remediation/steps', 'the remediation must be an instruction a human can execute, not a vague "install UE"');
  }
  if (typeof record.consequence !== 'string' || record.consequence.trim() === '') {
    reject(BLOCKER_REJECTIONS.NO_REMEDIATION, '/consequence', 'the record must state what stays unclaimable while it stands');
  }

  return done();
}

/**
 * The aggregate statuses. There is deliberately no member meaning "pass": this
 * module can report that blockers exist, or that none were recorded, and the
 * second is NOT evidence that a minor is present — that is Tasks 56-60's job.
 */
export const AGGREGATE_STATUSES = Object.freeze({
  BLOCKED_EXTERNAL: 'BLOCKED_EXTERNAL',
  NO_BLOCKERS_RECORDED: 'NO_BLOCKERS_RECORDED',
});

/**
 * Aggregate a set of blocker records.
 *
 * FAIL-CLOSED BY CONSTRUCTION: `blockerCount` is the number of records, computed
 * before any validation, so a record that fails the schema still blocks. There is
 * no branch that moves a record into `skippedCount` or `passedCount`; both are
 * literal zeroes, present only so a consumer that reads them finds a zero rather
 * than an absent key it might default to something friendlier.
 * @param {{ records: readonly any[], detectedMissingMinors?: readonly string[],
 *   io?: typeof realBlockerIo, skipFilesystem?: boolean, now?: () => Date }} spec
 */
export function aggregateExternalBlockers(spec) {
  const records = [...spec.records];
  /** @type {{code: string, at: string, detail: string}[]} */
  const rejections = [];
  const perRecord = records.map((record, index) => {
    const result = validateBlockerRecord(record, { io: spec.io, skipFilesystem: spec.skipFilesystem, now: spec.now });
    for (const entry of result.rejections) rejections.push({ ...entry, at: `/records/${index}${entry.at}` });
    return { recordId: record?.recordId ?? `<record ${index}>`, minorKey: record?.subject?.minorKey ?? null, outcome: result.outcome, rejections: result.rejections };
  });

  const seen = new Map();
  for (const [index, entry] of perRecord.entries()) {
    if (entry.minorKey === null) continue;
    if (seen.has(entry.minorKey)) {
      rejections.push({ code: BLOCKER_REJECTIONS.DUPLICATE_RECORD, at: `/records/${index}`, detail: `${entry.minorKey} already has a record (${seen.get(entry.minorKey)}); two records for one minor let a later reader satisfy the gap with whichever one is weaker` });
    }
    seen.set(entry.minorKey, entry.recordId);
  }
  for (const minor of spec.detectedMissingMinors ?? []) {
    if (!seen.has(minor)) {
      rejections.push({ code: BLOCKER_REJECTIONS.UNRECORDED_MISSING_MINOR, at: '/records', detail: `the detector reported ${minor} missing but no record was emitted for it; an unrecorded gap is exactly the shape that reads as coverage` });
    }
  }

  const blockedMinors = [...new Set(perRecord.map((entry) => entry.minorKey).filter((entry) => entry !== null))].sort();
  return {
    status: records.length > 0 ? AGGREGATE_STATUSES.BLOCKED_EXTERNAL : AGGREGATE_STATUSES.NO_BLOCKERS_RECORDED,
    outcome: /** @type {'ACCEPTED'|'REJECTED'} */ (rejections.length === 0 ? 'ACCEPTED' : 'REJECTED'),
    blockerCount: records.length,
    skippedCount: 0,
    passedCount: 0,
    blockedMinors,
    perRecord,
    rejections,
    treatment: `each of the ${records.length} record(s) counts as a BLOCKER. This module has no code path that turns one into a skip or a pass, and an invalid record still blocks — it is refused as evidence without ceasing to be a gap.`,
  };
}

/** Human-readable refusal, for a CLI that must tell an operator what to do.
 * @param {{outcome: string, rejections: readonly {code: string, at: string, detail: string}[]}} result */
export function describeBlockerRejections(result) {
  if (result.outcome === 'ACCEPTED') return 'blocker records ACCEPTED: every digest recomputed, every cited path re-read, no pass/skip field present.';
  const lines = ['blocker records REJECTED:'];
  for (const entry of result.rejections) lines.push(`  ${entry.code} at ${entry.at}\n    ${entry.detail}`);
  return lines.join('\n');
}
