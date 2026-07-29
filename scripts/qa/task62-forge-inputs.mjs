// @ts-check
// scripts/qa/task62-forge-inputs.mjs
// Task 62 — build an input set that lies, so the aggregator can be caught either
// refusing it or failing to.
//
// An aggregator that has only ever been run on inputs it accepts proves nothing:
// a function that returns ACCEPTED unconditionally passes that test perfectly.
// The forgery here is the specific lie this task is most at risk of swallowing —
// "all nine UE minors passed" — expressed the four ways it would actually reach
// the aggregate:
//
//   1. a minor with no engine, or with an engine whose editor was never compiled,
//      presenting a fully PASSED stage table                 -> BLOCKED_ROW_CLAIMS_PASS
//   2. a blocker record quietly restatused away from BLOCKED  -> BLOCKED_ROW_CLAIMS_PASS
//   3. one real package and binary digest re-used to dress a
//      second minor's row                                     -> REUSED_ARTIFACT_HASH
//   4. a verdict asserted with the stage table deleted, and a
//      failure asserted with its observable deleted           -> MISSING_STAGE_TABLE / NO_CITATION
//
// NOTHING IS INVENTED FROM SCRATCH. The forgery starts from the real records and
// mutates them, because a forgery built out of empty objects would be refused for
// being malformed rather than for being false, and that would prove the wrong
// thing.

import { copyFileSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

/** The per-engine lanes, and which minor each speaks for in the real input set. */
const ENGINE_TASKS = Object.freeze([56, 57, 58, 59, 60]);
const BLOCKER_TASK = 61;
const SIDECARS = Object.freeze(['task-52/profile-matrix.json', 'task-52/native-gates.json']);

/** @param {string} file */
const readJson = (file) => JSON.parse(readFileSync(file, 'utf8'));
/** @param {string} file @param {unknown} value */
const writeJson = (file, value) => { writeFileSync(file, `${JSON.stringify(value, null, 2)}\n`); };

/** Rewrite every stage as PASSED, preserving the ids so the table still looks real. */
function forgeAllStagesPassed(/** @type {any} */ document) {
  const stages = document?.environment?.stages;
  if (!Array.isArray(stages)) return 0;
  for (const stage of stages) {
    if ('outcome' in stage) stage.outcome = 'PASSED';
    if ('ok' in stage) stage.ok = true;
    stage.detail = `${String(stage.detail ?? '')} [FORGED]`;
  }
  return stages.length;
}

/**
 * Produce the forged input set.
 * @param {{sourceDir: string, targetDir: string, minors: readonly string[]}} spec
 * @returns {{written: string[], mutations: {file: string, mutation: string, expectedRefusal: string}[]}}
 */
export function forgeInputSet(spec) {
  mkdirSync(join(spec.targetDir, 'task-52'), { recursive: true });
  /** @type {string[]} */
  const written = [];
  /** @type {{file: string, mutation: string, expectedRefusal: string}[]} */
  const mutations = [];

  for (const sidecar of SIDECARS) {
    copyFileSync(join(spec.sourceDir, sidecar), join(spec.targetDir, sidecar));
    written.push(sidecar);
  }

  // The one real package and binary digest the forger has to work with. Reusing
  // them is not laziness in the fiction — it is the only way to dress a row for a
  // minor no package was ever built for.
  const real = readJson(join(spec.sourceDir, 'task-59-pure-unreal-mcp-implementation.json'));
  const stolenPackage = real?.environment?.certification?.package?.sha256 ?? null;
  const stolenBinary = real?.environment?.certification?.binaryFreshness?.binarySha256 ?? null;

  for (const task of ENGINE_TASKS) {
    const name = `task-${task}-pure-unreal-mcp-implementation.json`;
    const document = readJson(join(spec.sourceDir, name));
    const version = document?.engine?.versionString ?? document?.engine?.version ?? '';
    const minor = String(version).split('.').slice(0, 2).join('.');

    if (task === 59) {
      // A verdict with the table that decided it deleted.
      delete document.environment.stages;
      document.verdict = 'FORGED: 20/20 certification stages passed';
      mutations.push({ file: name, mutation: `UE ${minor}: /environment/stages deleted while the verdict still claims a full pass`, expectedRefusal: 'MISSING_STAGE_TABLE' });
    } else if (task === 60) {
      // A failure asserted with nothing measurable behind it: the stage table
      // still names a FAILED stage, but the exit code and error count are gone.
      delete document.environment.compatibility;
      document.verdict = 'FORGED: certified on UE 5.8';
      mutations.push({ file: name, mutation: `UE ${minor}: /environment/compatibility deleted, so the FAILED stage has no exit code or error count behind it`, expectedRefusal: 'NO_CITATION' });
    } else {
      const count = forgeAllStagesPassed(document);
      document.verdict = `FORGED: ${count}/${count} certification stages passed for UE ${minor}`;
      mutations.push({ file: name, mutation: `UE ${minor}: all ${count} stages rewritten to PASSED although the host inventory records no compiled editor for this root`, expectedRefusal: 'BLOCKED_ROW_CLAIMS_PASS' });
      if (task === 56 && stolenPackage !== null && stolenBinary !== null) {
        document.environment.certification = {
          package: { sha256: stolenPackage, note: 'FORGED: copied from the 5.7.4 run' },
          binaryFreshness: { binarySha256: stolenBinary, fresh: true, note: 'FORGED: copied from the 5.7.4 run' },
        };
        mutations.push({ file: name, mutation: `UE ${minor}: the 5.7.4 package (${String(stolenPackage).slice(0, 12)}) and binary (${String(stolenBinary).slice(0, 12)}) digests re-used to dress this row`, expectedRefusal: 'REUSED_ARTIFACT_HASH' });
      }
    }
    writeJson(join(spec.targetDir, name), document);
    written.push(name);
  }

  // The four minors with no engine at all: their blocker records are restatused,
  // which is exactly how a blocker degrades into a skip and then into silence.
  const blockerName = `task-${BLOCKER_TASK}-pure-unreal-mcp-implementation.json`;
  const blockers = readJson(join(spec.sourceDir, blockerName));
  const restatused = [];
  for (const record of blockers?.environment?.externalBlockers?.records ?? []) {
    record.status = 'PASS';
    restatused.push(record?.subject?.minorKey);
  }
  blockers.verdict = 'FORGED: every advertised minor passed';
  writeJson(join(spec.targetDir, blockerName), blockers);
  written.push(blockerName);
  mutations.push({ file: blockerName, mutation: `UE ${restatused.filter(Boolean).join(', ')}: blocker records restatused from BLOCKED_EXTERNAL to PASS although no engine is installed for any of them`, expectedRefusal: 'BLOCKED_ROW_CLAIMS_PASS' });

  return { written, mutations };
}
