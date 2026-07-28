// @ts-check
// tests/unit/task-50/oracle-judgement.mjs
// Task 50 — turning independent observations into a VERDICT about a claim.
//
// The observations in state-oracles.mjs are just readings. This file is where a
// reading is allowed to contradict a response, and it is written around one
// question: IF THE RESPONSE IS LYING, DOES ANYTHING HERE NOTICE?
//
// The claim under judgement is whatever the transport SAID happened. It is input,
// never evidence. A `{ success: true }` that changed nothing must come out of
// `judgeClaim` as FORGED_SUCCESS — a suite that cannot catch a lie is decoration.
//
// FIVE RULES, each paid for by a real failure in this plan:
//
//  1. NO PROOF WITHOUT PRE-STATE. Task 49 run 1: one transport read the other's
//     leftover asset and scored its own failed create as proven. `present` after
//     is meaningless unless `absent` before.
//  2. AN INCONCLUSIVE READING NEVER PASSES. An oracle that could not look is not
//     an oracle that saw nothing.
//  3. THE SETUP MUST BE ASSERTED. Task 46's drain test proved a container emptied
//     that had never been filled. A proof over an empty container is vacuous.
//  4. EVERY POLARITY NEEDS A POSITIVE CONTROL. Without one, an oracle that reports
//     "absent" for everything satisfies every absence assertion in the suite, and
//     the suite is green while detecting nothing.
//  5. A MUTATION CLAIM NEEDS AT LEAST ONE OUT-OF-BAND READING. Corroboration from
//     a second transport is welcome; it is not a substitute, because it shares the
//     plugin and the queue with the mutation it is corroborating.

import { INDEPENDENCE, INDEPENDENCE_RANK } from './state-oracles.mjs';

/** What a response can claim to have done to the world. */
export const EFFECTS = Object.freeze(['created', 'modified', 'deleted', 'unchanged']);

/**
 * The closed set of verdicts. Every one names WHAT went wrong, because "FAIL" in
 * a report costs another live run to interpret.
 */
export const VERDICTS = Object.freeze({
  PROVEN: 'PROVEN',
  /** Claimed success; the world did not change. THE headline detection. */
  FORGED_SUCCESS: 'FORGED_SUCCESS',
  /** Claimed a modification; the digest is byte-identical. */
  FORGED_MODIFICATION: 'FORGED_MODIFICATION',
  /** Claimed a deletion; the thing is still there. */
  FORGED_DELETION: 'FORGED_DELETION',
  /** Claimed failure; the world changed anyway — a silent partial mutation. */
  UNCLAIMED_MUTATION: 'UNCLAIMED_MUTATION',
  /** The fixture already existed, so no reading could attribute it to this call. */
  PRE_STATE_CONTAMINATED: 'PRE_STATE_CONTAMINATED',
  /** An oracle could not look. Never a pass, never a silent negative. */
  UNPROVEN: 'UNPROVEN',
  /** Only weak (same-plugin) corroboration was offered for a mutation. */
  NOT_INDEPENDENT: 'NOT_INDEPENDENT',
  /** The setup this claim depends on did not succeed; judging it would be vacuous. */
  VACUOUS: 'VACUOUS',
  /** Cleanup ran and the thing survived. */
  RESIDUE: 'RESIDUE',
});

/** @typedef {import('./state-oracles.mjs').Observation} Observation */

/**
 * @typedef {{ verdict: string, pass: boolean, reason: string,
 *   claim: Record<string, unknown>, before: Observation|null, after: Observation,
 *   corroboration: Observation[], independence: string }} Judgement
 */

/**
 * @param {readonly Observation[]} observations
 * @returns {string} the STRONGEST independence present, or 'none'
 */
export function strongestIndependence(observations) {
  let best = -1;
  /** @type {string} */
  let name = 'none';
  for (const entry of observations) {
    const rank = INDEPENDENCE_RANK[entry.independence];
    if (rank !== undefined && rank > best) { best = rank; name = entry.independence; }
  }
  return name;
}

/**
 * Judge one claim against a pre/post observation pair.
 *
 * `claim` is the response's own story: `{ outcome: 'success'|'error', effect }`.
 * Nothing in it is trusted; it only decides WHICH contradiction we are looking
 * for. The observations decide the verdict.
 *
 * @param {{
 *   claim: { outcome: 'success'|'error', effect: string, target?: string },
 *   before: Observation|null,
 *   after: Observation,
 *   corroboration?: readonly Observation[],
 *   setupOk?: boolean,
 *   requireOutOfBand?: boolean,
 * }} spec
 * @returns {Judgement}
 */
export function judgeClaim(spec) {
  const corroboration = [...(spec.corroboration ?? [])];
  const all = [spec.after, ...corroboration, ...(spec.before === null ? [] : [spec.before])];
  const independence = strongestIndependence(all);
  /** @param {string} verdict @param {string} reason */
  const out = (verdict, reason) => ({
    verdict, pass: verdict === VERDICTS.PROVEN, reason,
    claim: { ...spec.claim }, before: spec.before, after: spec.after,
    corroboration, independence,
  });

  // RULE 3 — an unasserted setup makes everything downstream meaningless.
  if (spec.setupOk === false) {
    return out(VERDICTS.VACUOUS, 'the setup for this claim did not succeed; any verdict about the outcome would be a proof over an empty container');
  }

  // RULE 2 — could the oracle look at all?
  if (!spec.after.conclusive || spec.after.present === null) {
    return out(VERDICTS.UNPROVEN, `the post-state oracle (${spec.after.mechanism}) produced no reading; an oracle that could not look is not an oracle that saw nothing`);
  }

  // RULE 5 — a state CHANGE must be witnessed by something outside the stack that
  // performed it. Reads (`unchanged`) may be corroborated by weaker mechanisms.
  const changing = spec.claim.effect !== 'unchanged';
  if (changing && (spec.requireOutOfBand ?? true) && independence !== INDEPENDENCE.OUT_OF_BAND) {
    return out(VERDICTS.NOT_INDEPENDENT, `strongest available independence was "${independence}"; a mutation claim needs at least one out-of-band reading, because a same-plugin read shares every failure mode with the mutation`);
  }

  // RULE 1 — pre-state.
  if (changing && spec.before === null) {
    return out(VERDICTS.UNPROVEN, 'no pre-state reading was taken; "present" afterwards cannot distinguish this call from a leftover');
  }
  if (spec.before !== null && (!spec.before.conclusive || spec.before.present === null)) {
    return out(VERDICTS.UNPROVEN, `the pre-state oracle (${spec.before.mechanism}) produced no reading`);
  }

  const before = spec.before;
  const wasThere = before?.present === true;
  const isThere = spec.after.present === true;
  const digestChanged = before === null || before.digest === null || spec.after.digest === null
    ? null
    : before.digest !== spec.after.digest;

  if (spec.claim.effect === 'created') {
    if (wasThere) {
      return out(VERDICTS.PRE_STATE_CONTAMINATED, `"${spec.after.target}" already existed before the call; a "present" reading could not distinguish this call from a leftover, and an "absent" reading would be judging a fixture nobody made`);
    }
    if (spec.claim.outcome === 'success') {
      return isThere
        ? out(VERDICTS.PROVEN, `claimed creation of "${spec.after.target}" is confirmed by ${spec.after.mechanism}, which read it absent before and present after`)
        : out(VERDICTS.FORGED_SUCCESS, `the response claimed success but ${spec.after.mechanism} finds "${spec.after.target}" still absent; the world did not change`);
    }
    return isThere
      ? out(VERDICTS.UNCLAIMED_MUTATION, `the response reported an error but "${spec.after.target}" exists; a refused call left state behind`)
      : out(VERDICTS.PROVEN, `claimed failure is confirmed: ${spec.after.mechanism} finds "${spec.after.target}" absent, so nothing was created`);
  }

  if (spec.claim.effect === 'modified') {
    if (!wasThere) {
      return out(VERDICTS.VACUOUS, `"${spec.after.target}" did not exist before the call, so "modified" has nothing to be true of`);
    }
    if (digestChanged === null) {
      return out(VERDICTS.UNPROVEN, `${spec.after.mechanism} yielded no digest on one side, so a modification cannot be distinguished from a no-op`);
    }
    if (spec.claim.outcome === 'success') {
      return digestChanged
        ? out(VERDICTS.PROVEN, `claimed modification is confirmed: ${spec.after.mechanism} digest moved ${String(before?.digest).slice(0, 12)} -> ${String(spec.after.digest).slice(0, 12)}`)
        : out(VERDICTS.FORGED_MODIFICATION, `the response claimed a modification but the ${spec.after.mechanism} digest is byte-identical; nothing changed`);
    }
    return digestChanged
      ? out(VERDICTS.UNCLAIMED_MUTATION, `the response reported an error but the ${spec.after.mechanism} digest moved; a refused call mutated state`)
      : out(VERDICTS.PROVEN, 'claimed failure is confirmed: the digest is unchanged');
  }

  if (spec.claim.effect === 'deleted') {
    if (!wasThere) {
      return out(VERDICTS.VACUOUS, `"${spec.after.target}" was already absent before the call, so a deletion proves nothing`);
    }
    if (spec.claim.outcome === 'success') {
      return isThere
        ? out(VERDICTS.FORGED_DELETION, `the response claimed the deletion succeeded but ${spec.after.mechanism} still finds "${spec.after.target}"`)
        : out(VERDICTS.PROVEN, `claimed deletion is confirmed: ${spec.after.mechanism} read it present before and absent after`);
    }
    return isThere
      ? out(VERDICTS.PROVEN, 'claimed failure is confirmed: the target survives, as a refused deletion should leave it')
      : out(VERDICTS.UNCLAIMED_MUTATION, `the response reported an error but "${spec.after.target}" is gone; a refused deletion destroyed state`);
  }

  // effect === 'unchanged' — a read. The falsifiable claim is that it changed nothing.
  if (digestChanged === true) {
    return out(VERDICTS.UNCLAIMED_MUTATION, `a read-only claim moved the ${spec.after.mechanism} digest`);
  }
  if (wasThere !== isThere) {
    return out(VERDICTS.UNCLAIMED_MUTATION, `a read-only claim changed presence from ${wasThere} to ${isThere}`);
  }
  return out(VERDICTS.PROVEN, `read-only claim confirmed: ${spec.after.mechanism} shows no change`);
}

/**
 * Judge whether cleanup actually restored the pre-run state.
 *
 * Deliberately NOT "did the delete call return success". Task 49 believed a
 * delete response and reported `cleanupClean: true` over two leaked materials.
 * The only accepted proof is the same out-of-band digest, read again.
 * @param {{ baseline: Observation, afterCleanup: Observation, owned: string }} spec
 * @returns {{ verdict: string, pass: boolean, reason: string, baseline: Observation, afterCleanup: Observation, owned: string }}
 */
export function judgeCleanup(spec) {
  const base = { baseline: spec.baseline, afterCleanup: spec.afterCleanup, owned: spec.owned };
  if (!spec.afterCleanup.conclusive) {
    return { ...base, verdict: VERDICTS.UNPROVEN, pass: false, reason: `the post-cleanup oracle (${spec.afterCleanup.mechanism}) produced no reading, so residue cannot be excluded` };
  }
  if (spec.baseline.digest !== null && spec.afterCleanup.digest !== null) {
    const restored = spec.baseline.digest === spec.afterCleanup.digest;
    return restored
      ? { ...base, verdict: VERDICTS.PROVEN, pass: true, reason: `cleanup restored the pre-run state exactly: ${spec.afterCleanup.mechanism} digest matches the baseline` }
      : { ...base, verdict: VERDICTS.RESIDUE, pass: false, reason: `cleanup did NOT restore "${spec.owned}": ${spec.afterCleanup.mechanism} digest ${String(spec.afterCleanup.digest).slice(0, 12)} != baseline ${String(spec.baseline.digest).slice(0, 12)}` };
  }
  return spec.afterCleanup.present === false
    ? { ...base, verdict: VERDICTS.PROVEN, pass: true, reason: `cleanup verified: ${spec.afterCleanup.mechanism} finds "${spec.owned}" absent` }
    : { ...base, verdict: VERDICTS.RESIDUE, pass: false, reason: `residue: ${spec.afterCleanup.mechanism} still finds "${spec.owned}" after cleanup` };
}

/**
 * RULE 4, enforced over a whole suite rather than trusted per case.
 *
 * For each (kind, mechanism) an oracle was used with, we require it to have been
 * WATCHED reporting `true` at least once and `false` at least once. A mechanism
 * that only ever answered one way is either blind or stuck, and every assertion
 * that relied on it is unearned.
 * @param {readonly Observation[]} observations
 * @returns {{ ok: boolean, mechanisms: Array<{ mechanism: string, kind: string, sawPresent: boolean, sawAbsent: boolean, inconclusive: number }>, missing: string[] }}
 */
export function auditPositiveControls(observations) {
  /** @type {Map<string, { mechanism: string, kind: string, sawPresent: boolean, sawAbsent: boolean, inconclusive: number }>} */
  const byMechanism = new Map();
  for (const entry of observations) {
    const key = `${entry.kind}|${entry.mechanism}`;
    const row = byMechanism.get(key) ?? { mechanism: entry.mechanism, kind: entry.kind, sawPresent: false, sawAbsent: false, inconclusive: 0 };
    if (entry.present === true) row.sawPresent = true;
    else if (entry.present === false) row.sawAbsent = true;
    else row.inconclusive += 1;
    byMechanism.set(key, row);
  }
  const rows = [...byMechanism.values()];
  const missing = rows.filter((row) => !row.sawPresent || !row.sawAbsent)
    .map((row) => `${row.kind}/${row.mechanism} never saw ${row.sawPresent ? 'an absent' : 'a present'} reading`);
  return { ok: missing.length === 0, mechanisms: rows, missing };
}

/**
 * Assert a setup step really succeeded before anything is judged against it.
 *
 * Takes the SETUP's own observation, not its response: "the create call returned
 * 200" is the claim, "the package is on disk" is the fact.
 * @param {{ label: string, observation: Observation, expectPresent?: boolean }} spec
 */
export function assertSetupObserved(spec) {
  const expect = spec.expectPresent ?? true;
  if (!spec.observation.conclusive || spec.observation.present === null) {
    return { ok: false, label: spec.label, reason: `setup "${spec.label}" could not be observed via ${spec.observation.mechanism}; every later assertion would be vacuous` };
  }
  if (spec.observation.present !== expect) {
    return { ok: false, label: spec.label, reason: `setup "${spec.label}" was expected ${expect ? 'present' : 'absent'} but ${spec.observation.mechanism} read ${spec.observation.present ? 'present' : 'absent'}` };
  }
  return { ok: true, label: spec.label, reason: `setup "${spec.label}" observed ${expect ? 'present' : 'absent'} via ${spec.observation.mechanism}` };
}

/**
 * A forged response, for the self-tests and for the live suite's own lie-detector
 * probe. Named rather than hand-rolled at each call site so "what a forgery looks
 * like" is one definition every test agrees on.
 * @param {{ target: string, effect?: string, message?: string }} spec
 */
export function forgedSuccessClaim(spec) {
  return {
    outcome: /** @type {'success'} */ ('success'),
    effect: spec.effect ?? 'created',
    target: spec.target,
    forged: true,
    responseWouldSay: spec.message ?? `Created ${spec.target}`,
  };
}
