// @ts-check
// scripts/qa/task62-compatibility-matrix.mjs
// Task 62 — the derivation and refusal engine behind the aggregate compatibility
// matrix. No I/O lives here: callers hand it already-parsed documents, so the
// same code path runs over the real evidence and over a forged input set.
//
// THE ONE RULE THIS FILE EXISTS TO ENFORCE is that a row is classified by WHO
// OWNS THE REMEDIATION, never by how the source record phrased its own verdict:
//
//   root ABSENT                      -> BLOCKED_EXTERNAL / root-absent   (operator installs the engine)
//   root PRESENT, editor NOT built   -> BLOCKED_EXTERNAL / root-unbuilt  (operator compiles the editor target)
//   root PRESENT + BUILT, not certified -> FAIL           (OUR defect; NOT a blocker)
//   root PRESENT + BUILT + certified    -> PASS           (cited to the stage table that proves it)
//
// The two BLOCKED_EXTERNAL subclasses have different remediation and are kept
// distinguishable end to end. The third state matters just as much: a minor whose
// engine is present and buildable and whose plugin still will not compile is a
// defect we own, and filing it as BLOCKED_EXTERNAL would move our bug onto the
// operator's plate. Task 60's own record calls itself "BLOCKED"; this engine
// re-derives FAIL from `hasCompiledEditor: true` plus a FAILED stage, because the
// classifier reads facts, not verdict prose.
//
// EVERY REFUSAL IS A REAL REFUSAL. `aggregate()` returns rejections and the CLI
// exits non-zero on them, which is the only thing that makes an accepted run
// meaningful. The forged-input probe in task62-forge-inputs.mjs exists to prove
// each rule can fire.

/** Closed refusal taxonomy. A reason not on this list cannot be reported. */
export const AGGREGATE_REJECTIONS = Object.freeze({
  NO_CITATION: 'NO_CITATION',
  MISSING_STAGE_TABLE: 'MISSING_STAGE_TABLE',
  REUSED_ARTIFACT_HASH: 'REUSED_ARTIFACT_HASH',
  BLOCKED_ROW_CLAIMS_PASS: 'BLOCKED_ROW_CLAIMS_PASS',
  MISSING_MINOR_ROW: 'MISSING_MINOR_ROW',
  INVENTORY_DISAGREEMENT: 'INVENTORY_DISAGREEMENT',
  UNEXPLAINED_DELTA: 'UNEXPLAINED_DELTA',
  PREVIEW_LABEL_LOST: 'PREVIEW_LABEL_LOST',
  INFERRED_STAGE: 'INFERRED_STAGE',
  RECERTIFICATION_NOT_INDEPENDENT: 'RECERTIFICATION_NOT_INDEPENDENT',
});

/** The three states. They never collapse into two. */
export const ROW_STATE = Object.freeze({
  PASS: 'PASS',
  FAIL: 'FAIL',
  BLOCKED_EXTERNAL: 'BLOCKED_EXTERNAL',
});

/** The two blocked subclasses, which do NOT share a remediation. */
export const BLOCKED_SUBCLASS = Object.freeze({
  ROOT_ABSENT: 'root-absent',
  ROOT_UNBUILT: 'root-unbuilt',
});

/** @type {Readonly<Record<string, string>>} */
export const REMEDIATION_OWNER = Object.freeze({
  [BLOCKED_SUBCLASS.ROOT_ABSENT]: 'operator: install the engine at a root this host can see, then build its editor target',
  [BLOCKED_SUBCLASS.ROOT_UNBUILT]: 'operator: finish compiling the editor target (Engine/Binaries/Linux/UnrealEditor-Cmd) for the already-installed root',
  [ROW_STATE.FAIL]: 'us: the engine is installed and its editor is built, so nothing external is missing. Fixing the plugin is our work.',
});

/** One row per UE minor across 5.0-5.9 exclusive. Exactly nine, always. */
export const ADVERTISED_MINORS = Object.freeze(['5.0', '5.1', '5.2', '5.3', '5.4', '5.5', '5.6', '5.7', '5.8']);

/** Stage outcomes the certification orchestrator emits. */
const STAGE_OUTCOMES = Object.freeze(['PASSED', 'FAILED', 'NOT_REACHED', 'NOT_APPLICABLE']);

const SHA256 = /^[0-9a-f]{64}$/u;

/**
 * Parse the shared engine-inventory table that Tasks 56 and 61 both embed.
 *
 * This is the ONE source that speaks about all nine minors at once, and it is
 * re-derived from `Engine/Build/Build.version` on every root by the task that
 * emitted it. Reading presence and runnability from here — rather than from each
 * per-engine record's prose — is what lets a minor with no record of its own
 * (5.1, 5.2, 5.4, 5.6) still get a derived row instead of a hand-written one.
 *
 * `build` and `run` are the last two columns: `run = NO` means no compiled
 * UnrealEditor-Cmd, which is the root-unbuilt blocker.
 * @param {string} table
 * @returns {Map<string, {minor: string, identity: string, root: string, channel: string, build: string, run: string}>}
 */
export function parseInventoryTable(table) {
  /** @type {Map<string, any>} */
  const rows = new Map();
  for (const line of String(table ?? '').split('\n')) {
    const cells = line.split('|').map((cell) => cell.trim());
    // A data line is `| 5.0 | 5.0.3 | /root | channel | yes | NO |` -> 8 cells
    // once the empty edges are counted. The header and the `---` rule are skipped
    // by the version-shaped test on the first cell.
    if (cells.length < 7) continue;
    const minor = cells[1];
    if (!/^\d+\.\d+$/u.test(minor)) continue;
    rows.set(minor, {
      minor,
      identity: cells[2],
      root: cells[3],
      channel: cells[4],
      build: cells[5],
      run: cells[6],
    });
  }
  return rows;
}

/**
 * Normalise the two stage-table shapes the engine lanes emit into one.
 *
 * Tasks 56-58 and 60 record `{ stage, outcome, detail }`; Task 59 records
 * `{ name, ok, detail }`. Returning null rather than an empty array is
 * deliberate: "no stage table" and "a stage table with nothing in it" are both
 * grounds to refuse a PASS or FAIL, and the caller must not be able to confuse
 * either with "certified".
 * @param {any} record
 * @returns {{id: string, outcome: string, detail: string}[]|null}
 */
export function normalizeStages(record) {
  const raw = record?.environment?.stages;
  if (!Array.isArray(raw) || raw.length === 0) return null;
  /** @type {{id: string, outcome: string, detail: string}[]} */
  const stages = [];
  for (const entry of raw) {
    const id = typeof entry?.stage === 'string' ? entry.stage
      : typeof entry?.name === 'string' ? entry.name : null;
    if (id === null) return null;
    const outcome = typeof entry?.outcome === 'string' && STAGE_OUTCOMES.includes(entry.outcome) ? entry.outcome
      : entry?.ok === true ? 'PASSED'
        : entry?.ok === false ? 'FAILED' : null;
    if (outcome === null) return null;
    stages.push({ id, outcome, detail: String(entry?.detail ?? '') });
  }
  return stages;
}

/**
 * A stage table certifies only when every stage was actually reached and none
 * failed. NOT_REACHED is never read as a pass — a stage nobody ran proves
 * nothing in either direction, which is the rule Tasks 56-60 wrote into their own
 * stage details and which this engine refuses to soften.
 * @param {{id: string, outcome: string, detail: string}[]} stages
 */
export function summarizeStages(stages) {
  const tally = { PASSED: 0, FAILED: 0, NOT_REACHED: 0, NOT_APPLICABLE: 0 };
  for (const stage of stages) tally[/** @type {keyof typeof tally} */ (stage.outcome)] += 1;
  const failed = stages.filter((stage) => stage.outcome === 'FAILED');
  return {
    total: stages.length,
    tally,
    certified: tally.FAILED === 0 && tally.NOT_REACHED === 0 && tally.PASSED > 0,
    firstFailed: failed[0] ?? null,
    last: stages[stages.length - 1] ?? null,
  };
}

/**
 * Every artifact digest a record commits to, tagged with where it was read.
 *
 * Reuse is checked ACROSS minors: one binary cannot have been built against two
 * different engines, so a digest appearing under two minors means a result was
 * copied rather than measured. The engine's own `Build.version` digest is
 * included because two rows pointing at one root is the same lie told earlier.
 * @param {any} record
 * @returns {{sha256: string, kind: string, where: string}[]}
 */
export function collectHashes(record) {
  /** @type {{sha256: string, kind: string, where: string}[]} */
  const found = [];
  const push = (/** @type {any} */ sha, /** @type {string} */ kind, /** @type {string} */ where) => {
    if (typeof sha === 'string' && SHA256.test(sha)) found.push({ sha256: sha, kind, where });
  };
  for (const [index, artifact] of (record?.artifacts ?? []).entries()) {
    push(artifact?.sha256, 'artifact', `/artifacts/${index} (${artifact?.path ?? 'unnamed'})`);
  }
  push(record?.engine?.buildVersionSha256, 'engine-build-version', '/engine/buildVersionSha256');
  const certification = record?.environment?.certification;
  push(certification?.package?.sha256, 'plugin-package', '/environment/certification/package/sha256');
  push(certification?.binaryFreshness?.binarySha256, 'plugin-binary', '/environment/certification/binaryFreshness/binarySha256');
  return found;
}

/**
 * Build the citation a PASS or FAIL row must carry: which file, which stage, and
 * what was actually observed there. A row whose observable cannot be derived
 * returns null and the caller refuses it — an assertion with no reading behind it
 * is the exact shape this whole evidence system exists to reject.
 * @param {{state: string, evidenceFile: string, record: any,
 *   stages: {id: string, outcome: string, detail: string}[]}} spec
 */
export function buildCitation(spec) {
  const summary = summarizeStages(spec.stages);
  if (spec.state === ROW_STATE.PASS) {
    const proving = summary.last;
    if (proving === null) return null;
    const automation = spec.stages.find((stage) => stage.id === 'automation.startedEqualsCompleted');
    const corpus = spec.stages.find((stage) => stage.id === 'drivers.corpusSubset');
    return {
      evidenceFile: spec.evidenceFile,
      stageId: proving.id,
      stagePath: `/environment/stages[${spec.stages.indexOf(proving)}]`,
      observable: `${summary.tally.PASSED}/${summary.total} stages PASSED, 0 FAILED, 0 NOT_REACHED`
        + (automation ? `; ${automation.id}: ${automation.detail}` : '')
        + (corpus ? `; ${corpus.id}: ${corpus.detail}` : ''),
      stageTally: summary.tally,
    };
  }
  if (spec.state === ROW_STATE.FAIL) {
    const failing = summary.firstFailed;
    if (failing === null) return null;
    const compatibility = spec.record?.environment?.compatibility;
    // A FAIL must name a measurable failure, not just "a stage said FAILED".
    const exitCode = compatibility?.exitCode;
    const errors = compatibility?.errorsEmitted;
    if (typeof exitCode !== 'number' && typeof errors !== 'number') return null;
    return {
      evidenceFile: spec.evidenceFile,
      stageId: failing.id,
      stagePath: `/environment/stages[${spec.stages.indexOf(failing)}]`,
      observable: [
        typeof exitCode === 'number' ? `exit code ${exitCode}` : null,
        typeof compatibility?.unrealBuildToolResult === 'string' ? `UnrealBuildTool: ${compatibility.unrealBuildToolResult}` : null,
        typeof errors === 'number' ? `${errors} compiler error(s) emitted${compatibility?.errorsTruncated === true ? ' (floor: clang hit -ferror-limit)' : ''}` : null,
        typeof compatibility?.filesWithErrors === 'number' ? `${compatibility.filesWithErrors} file(s) with errors` : null,
        `stage ${failing.id}: ${failing.detail}`,
      ].filter((part) => part !== null).join('; '),
      stageTally: summary.tally,
    };
  }
  return null;
}

/**
 * Classify ONE minor from the inventory plus whichever record owns it.
 *
 * `record` is null for a minor nobody certified (an absent root has no run to
 * record), and that is a legitimate blocked row rather than a gap.
 * @param {{minor: string, inventory: any, record: any, evidenceFile: string|null,
 *   blockerRecord?: any}} spec
 */
export function classifyMinor(spec) {
  const inventory = spec.inventory ?? null;
  /** @type {{code: string, at: string, detail: string}[]} */
  const rejections = [];
  const reject = (/** @type {string} */ code, /** @type {string} */ detail) => {
    rejections.push({ code, at: `/matrix/${spec.minor}`, detail });
  };

  if (inventory === null) {
    reject(AGGREGATE_REJECTIONS.MISSING_MINOR_ROW, `UE ${spec.minor} has no row in the engine inventory table, so its presence on this host is unknown and no state may be asserted for it`);
    return { row: null, rejections };
  }

  const rootPresent = inventory.identity !== 'MISSING' && inventory.identity !== '—';
  const editorBuilt = String(inventory.run).toLowerCase() === 'yes';
  const stages = spec.record === null ? null : normalizeStages(spec.record);
  const summary = stages === null ? null : summarizeStages(stages);
  const certified = summary?.certified === true;

  // A record that claims certification for a minor the inventory says is absent
  // or unbuilt is the forgery this gate exists to stop. Refuse before the row is
  // ever written, so a blocked minor can never be counted as a pass.
  if (!rootPresent && certified) {
    reject(AGGREGATE_REJECTIONS.BLOCKED_ROW_CLAIMS_PASS, `UE ${spec.minor} is absent from the host inventory (identity "${inventory.identity}") yet ${spec.evidenceFile} presents a fully PASSED stage table. An absent engine cannot certify anything.`);
  }
  if (rootPresent && !editorBuilt && certified) {
    reject(AGGREGATE_REJECTIONS.BLOCKED_ROW_CLAIMS_PASS, `UE ${spec.minor} is installed but its inventory row records run="${inventory.run}" (no compiled UnrealEditor-Cmd), yet ${spec.evidenceFile} presents a fully PASSED stage table. No editor could have run the corpus.`);
  }
  // A blocker that has been restatused is how a blocker degrades into a skip and
  // then into silence. Only the host changing may retire one, so a record whose
  // status has moved off BLOCKED_EXTERNAL while the inventory still shows nothing
  // to certify against is refused rather than quietly re-derived.
  const blockerStatus = spec.blockerRecord?.status ?? null;
  if (blockerStatus !== null && blockerStatus !== ROW_STATE.BLOCKED_EXTERNAL && !(rootPresent && editorBuilt)) {
    reject(AGGREGATE_REJECTIONS.BLOCKED_ROW_CLAIMS_PASS, `UE ${spec.minor} carries an external-blocker record restatused to "${blockerStatus}", but the host inventory still reports identity "${inventory.identity}" / run "${inventory.run}". A blocker is retired by the host changing, not by the record being relabelled.`);
  }

  /** @type {string} */
  let state;
  /** @type {string|null} */
  let subclass = null;
  if (!rootPresent) {
    state = ROW_STATE.BLOCKED_EXTERNAL;
    subclass = BLOCKED_SUBCLASS.ROOT_ABSENT;
  } else if (!editorBuilt) {
    state = ROW_STATE.BLOCKED_EXTERNAL;
    subclass = BLOCKED_SUBCLASS.ROOT_UNBUILT;
  } else if (certified) {
    state = ROW_STATE.PASS;
  } else {
    state = ROW_STATE.FAIL;
  }

  // PASS and FAIL both stand on a stage table. Without one there is nothing to
  // cite, and an uncitable assertion is refused rather than downgraded.
  if (state !== ROW_STATE.BLOCKED_EXTERNAL && stages === null) {
    reject(AGGREGATE_REJECTIONS.MISSING_STAGE_TABLE, `UE ${spec.minor} resolves to ${state} but ${spec.evidenceFile ?? 'no record'} carries no readable stage table at /environment/stages, so the outcome cannot be re-checked`);
    return { row: null, rejections };
  }

  const citation = state === ROW_STATE.BLOCKED_EXTERNAL || stages === null || spec.evidenceFile === null
    ? null
    : buildCitation({ state, evidenceFile: spec.evidenceFile, record: spec.record, stages });
  if (state !== ROW_STATE.BLOCKED_EXTERNAL && citation === null) {
    reject(AGGREGATE_REJECTIONS.NO_CITATION, `UE ${spec.minor} resolves to ${state} but no citation could be derived: a ${state} row must name its evidence file, the stage id that decided it, and the observable (exit code / error count / stage tally) read there`);
    return { row: null, rejections };
  }

  // A blocked row cites how absence or unbuiltness was DETECTED, which is a
  // different receipt from a stage table and is kept in a different field so the
  // two can never be read as the same kind of proof.
  const detection = spec.blockerRecord?.detection ?? spec.record?.environment?.blocker?.detection ?? null;
  const blockedCitation = state !== ROW_STATE.BLOCKED_EXTERNAL || detection === null ? null : {
    evidenceFile: spec.evidenceFile,
    detectionCommand: detection.reproducibleShellCommand ?? detection.command ?? null,
    detectedAt: detection.detectedAt ?? null,
    identifiedBy: detection.identifiedBy ?? null,
  };
  if (state === ROW_STATE.BLOCKED_EXTERNAL && blockedCitation === null) {
    reject(AGGREGATE_REJECTIONS.NO_CITATION, `UE ${spec.minor} is BLOCKED_EXTERNAL but no detection receipt (reproducible command + timestamp) was found, so the blocker cannot be re-checked and could not be retired by an operator`);
  }

  // A NOT_REACHED stage must survive as NOT_REACHED. Recording its outcome here
  // is how "never attempted" would quietly become "fine".
  const notReached = (stages ?? []).filter((stage) => stage.outcome === 'NOT_REACHED').map((stage) => stage.id);

  return {
    row: {
      minor: spec.minor,
      state,
      subclass,
      remediationOwner: REMEDIATION_OWNER[subclass ?? state] ?? null,
      engineIdentity: inventory.identity,
      engineRoot: inventory.root,
      rootPresent,
      editorBuilt,
      evidenceFile: spec.evidenceFile,
      citation,
      blockedCitation,
      stageTally: summary?.tally ?? null,
      stagesTotal: summary?.total ?? null,
      neverAttemptedStages: notReached,
      previewLabel: null,
      caveats: [],
    },
    rejections,
  };
}

/**
 * A minor certified twice must have been certified twice. Two runs sharing a
 * package or binary digest are one run reported as two, and the later record's
 * independence claim is then false.
 * @param {{minor: string, record: any, evidenceFile: string}} spec
 */
export function checkRecertificationIndependence(spec) {
  const freshness = spec.record?.environment?.freshnessVsTask52;
  if (freshness === null || typeof freshness !== 'object') return null;
  const prior = freshness.priorRun ?? {};
  const current = freshness.thisRun ?? {};
  const samePackage = typeof prior.packageSha256 === 'string' && prior.packageSha256 === current.packageSha256;
  const sameBinary = typeof prior.binarySha256 === 'string' && prior.binarySha256 === current.binarySha256;
  return {
    minor: spec.minor,
    evidenceFile: spec.evidenceFile,
    priorRun: { source: prior.source ?? null, packageSha256: prior.packageSha256 ?? null, binarySha256: prior.binarySha256 ?? null, editorPid: prior.editorPid ?? null },
    thisRun: { source: current.source ?? null, packageSha256: current.packageSha256 ?? null, binarySha256: current.binarySha256 ?? null, editorPid: current.editorPid ?? null },
    packagesDiffer: !samePackage,
    binariesDiffer: !sameBinary,
    independent: !samePackage && !sameBinary,
  };
}

/**
 * Re-derive the offline profile matrix from the generated capability registry.
 *
 * This is the "expected deltas are explained by generated records" gate. Nothing
 * is taken on the matrix's word: each row's available/filtered counts and every
 * gate tally are recomputed from `availability.unreal.min/max`,
 * `availability.requiredPlugins` and `availability.editorStates`, and a row that
 * does not reproduce is reported as an UNEXPLAINED delta rather than narrated.
 *
 * Gate tallies count OCCURRENCES (a record needing two absent plugins is two
 * PLUGIN_NOT_ENABLED occurrences); `available`/`filtered` count DISTINCT records.
 * Both are reproduced, so a mismatch in either is caught.
 * @param {{matrix: any, registry: any}} spec
 */
export function explainProfileMatrix(spec) {
  const records = spec.registry?.records;
  const optional = spec.matrix?.optionalPlugins;
  /** @type {{code: string, at: string, detail: string}[]} */
  const rejections = [];
  if (!Array.isArray(records) || !Array.isArray(optional)) {
    rejections.push({ code: AGGREGATE_REJECTIONS.UNEXPLAINED_DELTA, at: '/profileMatrix', detail: 'the generated canonical registry or the matrix optional-plugin list is unreadable, so no capability delta can be explained from a generated record' });
    return { rows: [], rejections, recordCount: null };
  }

  const compare = (/** @type {number[]} */ engine, /** @type {any} */ bound) => (engine[0] - bound.major) || (engine[1] - bound.minor) || (engine[2] - bound.patch);

  /** @type {any[]} */
  const rows = [];
  for (const row of spec.matrix.rows ?? []) {
    const engine = String(row.engine).split('.').map(Number);
    // The matrix records how many plugins the profile enabled, not which. The two
    // profiles it actually contains are "all optional" and "none"; any other count
    // leaves the enabled set underdetermined and must not be guessed.
    const enabled = row.pluginCount === optional.length ? optional : row.pluginCount === 0 ? [] : null;
    if (enabled === null) {
      rejections.push({ code: AGGREGATE_REJECTIONS.UNEXPLAINED_DELTA, at: `/profileMatrix/${row.profile}`, detail: `profile "${row.profile}" enables ${row.pluginCount} of ${optional.length} optional plugins; the enabled set is underdetermined, so its counts cannot be re-derived from the generated records` });
      continue;
    }
    let available = 0;
    /** @type {Record<string, number>} */
    const byGate = {};
    /** @type {string[]} */
    const belowMin = [];
    /** @type {string[]} */
    const aboveMax = [];
    const bump = (/** @type {string} */ gate) => { byGate[gate] = (byGate[gate] ?? 0) + 1; };
    for (const record of records) {
      const unreal = record?.availability?.unreal ?? {};
      let filtered = false;
      if (unreal.min && compare(engine, unreal.min) < 0) { bump('ENGINE_BELOW_MIN'); belowMin.push(record.id); filtered = true; }
      if (unreal.max && compare(engine, unreal.max) > 0) { bump('ENGINE_ABOVE_MAX'); aboveMax.push(record.id); filtered = true; }
      for (const plugin of record?.availability?.requiredPlugins ?? []) {
        if (!enabled.includes(plugin)) { bump('PLUGIN_NOT_ENABLED'); filtered = true; }
      }
      if (!(record?.availability?.editorStates ?? []).includes(row.editorState)) { bump('EDITOR_STATE_UNSUPPORTED'); filtered = true; }
      if (!filtered) available += 1;
    }
    const filteredCount = records.length - available;
    const matches = available === row.available
      && filteredCount === row.filtered
      && JSON.stringify(sortKeys(byGate)) === JSON.stringify(sortKeys(row.byGate ?? {}));
    rows.push({
      profile: row.profile,
      engine: row.engine,
      transport: row.transport,
      editorState: row.editorState,
      pluginCount: row.pluginCount,
      recorded: { available: row.available, filtered: row.filtered, byGate: sortKeys(row.byGate ?? {}) },
      rederived: { available, filtered: filteredCount, byGate: sortKeys(byGate) },
      reproduces: matches,
      explainedBy: {
        ENGINE_BELOW_MIN: belowMin,
        ENGINE_ABOVE_MAX: aboveMax,
      },
    });
    if (!matches) {
      rejections.push({ code: AGGREGATE_REJECTIONS.UNEXPLAINED_DELTA, at: `/profileMatrix/${row.profile}`, detail: `profile "${row.profile}" records available=${row.available}/filtered=${row.filtered} ${JSON.stringify(row.byGate)} but the generated registry re-derives available=${available}/filtered=${filteredCount} ${JSON.stringify(sortKeys(byGate))}. The delta is not explained by any generated availability field.` });
    }
  }
  return { rows, rejections, recordCount: records.length };
}

/** @param {Record<string, number>} value */
function sortKeys(value) {
  /** @type {Record<string, number>} */
  const out = {};
  for (const key of Object.keys(value).sort()) out[key] = value[key];
  return out;
}

/**
 * The native preprocessor gate census must account for every condition on every
 * engine. A row whose three buckets do not sum to the declared total has lost a
 * condition somewhere, and a lost condition is an unexplained capability delta.
 * @param {any} gates
 */
export function explainNativeGates(gates) {
  /** @type {{code: string, at: string, detail: string}[]} */
  const rejections = [];
  const total = gates?.totalConditions;
  /** @type {any[]} */
  const rows = [];
  for (const row of gates?.rows ?? []) {
    const sum = (row.compiledCount ?? 0) + (row.excludedCount ?? 0) + (row.undecidedCount ?? 0);
    const balances = sum === total;
    rows.push({ engine: row.engine, compiled: row.compiledCount, excluded: row.excludedCount, undecided: row.undecidedCount, sum, balances });
    if (!balances) {
      rejections.push({ code: AGGREGATE_REJECTIONS.UNEXPLAINED_DELTA, at: `/nativeGates/${row.engine}`, detail: `native gate census for ${row.engine} sums to ${sum} but the file declares ${total} total conditions; ${Math.abs(total - sum)} condition(s) are unaccounted for` });
    }
  }
  return { rows, rejections, totalConditions: total, distinctConditions: gates?.distinctConditions ?? null };
}

/**
 * Assemble the matrix and refuse it where it cannot be defended.
 *
 * @param {{inventoryTable: string, records: Record<string, {minor: string, file: string, document: any}>,
 *   blockerRecords: Record<string, any>, matrix: any, registry: any, gates: any}} inputs
 */
export function aggregate(inputs) {
  /** @type {{code: string, at: string, detail: string}[]} */
  const rejections = [];
  const inventory = parseInventoryTable(inputs.inventoryTable);

  /** @type {any[]} */
  const rows = [];
  for (const minor of ADVERTISED_MINORS) {
    const owner = inputs.records[minor] ?? null;
    const result = classifyMinor({
      minor,
      inventory: inventory.get(minor) ?? null,
      record: owner?.document ?? null,
      evidenceFile: owner?.file ?? (inputs.blockerRecords[minor] ? inputs.blockerRecords[minor].sourceFile ?? null : null),
      blockerRecord: inputs.blockerRecords[minor] ?? null,
    });
    rejections.push(...result.rejections);
    if (result.row !== null) rows.push(result.row);
  }

  if (rows.length + rejections.filter((entry) => entry.code === AGGREGATE_REJECTIONS.MISSING_MINOR_ROW).length !== ADVERTISED_MINORS.length) {
    rejections.push({ code: AGGREGATE_REJECTIONS.MISSING_MINOR_ROW, at: '/matrix', detail: `the matrix must carry exactly ${ADVERTISED_MINORS.length} rows (one per UE minor 5.0-5.8); ${rows.length} were derived` });
  }

  // ── the preview label must survive ─────────────────────────────────────────
  // Sourced from the generated records, not from the engine record's prose: the
  // catalogue's advertised ceiling literally carries channel "preview". If the
  // row for that ceiling reads as a stable release, the whole support statement
  // is wrong, so the label is a refusal and not a note.
  const ceiling = inputs.registry?.records?.[0]?.availability?.unreal?.max ?? null;
  if (ceiling !== null && ceiling.channel === 'preview') {
    const ceilingMinor = `${ceiling.major}.${ceiling.minor}`;
    const row = rows.find((entry) => entry.minor === ceilingMinor);
    if (row !== undefined) {
      const labelling = inputs.records[ceilingMinor]?.document?.engine?.channelLabelling ?? null;
      const folderLabel = labelling?.folderLabel ?? null;
      row.previewLabel = `Preview ${ceiling.preview ?? 1}`;
      row.previewEvidence = {
        registryMax: `${ceiling.major}.${ceiling.minor}.${ceiling.patch} channel "${ceiling.channel}" preview ${ceiling.preview ?? 1}`,
        engineFolderLabel: folderLabel,
        engineTag: labelling?.engineTag ?? null,
        folderLabelContradicted: labelling?.folderLabelContradicted ?? null,
        notClaimed: labelling?.notClaimed ?? [],
      };
      if (typeof row.previewLabel !== 'string' || row.previewLabel.length === 0) {
        rejections.push({ code: AGGREGATE_REJECTIONS.PREVIEW_LABEL_LOST, at: `/matrix/${ceilingMinor}`, detail: `the generated records advertise a maximum of ${ceilingMinor} on the "preview" channel, but the ${ceilingMinor} row carries no preview label and would read as a stable release` });
      }
    } else {
      rejections.push({ code: AGGREGATE_REJECTIONS.PREVIEW_LABEL_LOST, at: '/matrix', detail: `the generated records advertise a preview-channel maximum at ${ceilingMinor} but no row exists for that minor to carry the label` });
    }
  }

  // ── reused artifacts ───────────────────────────────────────────────────────
  /** @type {Map<string, {sha256: string, sightings: any[]}>} */
  const digests = new Map();
  for (const [minor, owner] of Object.entries(inputs.records)) {
    for (const hit of collectHashes(owner.document)) {
      const bucket = digests.get(hit.sha256) ?? { sha256: hit.sha256, sightings: [] };
      bucket.sightings.push({ minor, kind: hit.kind, where: hit.where, file: owner.file });
      digests.set(hit.sha256, bucket);
    }
  }
  /** @type {any[]} */
  const reused = [];
  for (const bucket of digests.values()) {
    const minors = [...new Set(bucket.sightings.map((sighting) => sighting.minor))];
    if (minors.length > 1) {
      reused.push({ sha256: bucket.sha256, minors, sightings: bucket.sightings });
      rejections.push({ code: AGGREGATE_REJECTIONS.REUSED_ARTIFACT_HASH, at: '/artifactDigests', detail: `digest ${bucket.sha256.slice(0, 16)} is claimed by more than one UE minor (${minors.join(', ')}). One artifact cannot have been built against two engines, so at least one of those rows was copied rather than measured.` });
    }
  }

  // ── a re-certified minor must have re-certified ────────────────────────────
  /** @type {any[]} */
  const recertifications = [];
  for (const [minor, owner] of Object.entries(inputs.records)) {
    const check = checkRecertificationIndependence({ minor, record: owner.document, evidenceFile: owner.file });
    if (check === null) continue;
    recertifications.push(check);
    if (!check.independent) {
      rejections.push({ code: AGGREGATE_REJECTIONS.RECERTIFICATION_NOT_INDEPENDENT, at: `/matrix/${minor}`, detail: `UE ${minor} claims two certification runs but they share ${check.packagesDiffer ? '' : 'a package digest'}${!check.packagesDiffer && !check.binariesDiffer ? ' and ' : ''}${check.binariesDiffer ? '' : 'a binary digest'}; that is one run reported as two` });
    }
  }

  const profileMatrix = explainProfileMatrix({ matrix: inputs.matrix, registry: inputs.registry });
  rejections.push(...profileMatrix.rejections);
  const nativeGates = explainNativeGates(inputs.gates);
  rejections.push(...nativeGates.rejections);

  const counts = {
    PASS: rows.filter((row) => row.state === ROW_STATE.PASS).length,
    FAIL: rows.filter((row) => row.state === ROW_STATE.FAIL).length,
    BLOCKED_EXTERNAL: rows.filter((row) => row.state === ROW_STATE.BLOCKED_EXTERNAL).length,
  };

  return {
    rows,
    counts,
    // The subclass split is carried alongside the blocked count so a reader can
    // never collapse two different remediations into one number.
    blockedBySubclass: {
      [BLOCKED_SUBCLASS.ROOT_ABSENT]: rows.filter((row) => row.subclass === BLOCKED_SUBCLASS.ROOT_ABSENT).length,
      [BLOCKED_SUBCLASS.ROOT_UNBUILT]: rows.filter((row) => row.subclass === BLOCKED_SUBCLASS.ROOT_UNBUILT).length,
    },
    reusedDigests: reused,
    recertifications,
    profileMatrix,
    nativeGates,
    rejections,
    outcome: rejections.length === 0 ? 'ACCEPTED' : 'REFUSED',
  };
}

/** Human-readable refusal, so an operator is told what to do. @param {ReturnType<typeof aggregate>} result */
export function describeAggregate(result) {
  if (result.outcome === 'ACCEPTED') {
    return `aggregate ACCEPTED: ${result.rows.length} rows — ${result.counts.PASS} PASS, ${result.counts.FAIL} FAIL, ${result.counts.BLOCKED_EXTERNAL} BLOCKED_EXTERNAL.`;
  }
  const lines = [`aggregate REFUSED (${result.rejections.length} rejection(s)):`];
  for (const entry of result.rejections) lines.push(`  ${entry.code} at ${entry.at}\n    ${entry.detail}`);
  return lines.join('\n');
}
