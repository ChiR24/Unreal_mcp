// @ts-check
// tests/unit/task-51/fuzz-shrink.mjs
// Task 51 — deterministic minimization, and the replay artifact a failure must
// carry to be worth reporting.
//
// AN UNMINIMIZED FUZZ FAILURE IS A LIABILITY. A 60-character string full of zero-
// width joiners and lone surrogates "fails" for some reason nobody will determine,
// so the finding is filed, not fixed, and the seed is eventually deleted to make
// CI green. Minimization converts that into a two-character input whose cause is
// obvious in one reading.
//
// THE LOOP IS DETERMINISTIC, NOT RANDOM. ddmin-style: fixed candidate order, fixed
// pass order, a hard budget on evaluations, and no PRNG anywhere. Two runs on the
// same failing input produce byte-identical minima, so the artifact recorded in
// evidence is the artifact the next person reproduces.
//
// THE PREDICATE MUST STAY THE SAME FAILURE. A shrinker that only asks "does it
// still fail?" happily walks a null-pointer crash into an unrelated validation
// error and reports a minimum that proves nothing. So `stillFails` is asked for a
// TAG, and only a candidate that reproduces the SAME tag is accepted.

/** Hard ceiling on predicate evaluations per shrink. A shrink that outruns this is
 * reported at its best-so-far rather than allowed to become the slow test. */
export const DEFAULT_SHRINK_BUDGET = 400;

/**
 * @typedef {(candidate: string) => string|null} FailureProbe
 *   returns the failure TAG the candidate reproduces, or null if it does not fail
 */

/**
 * Candidate reductions of a string, cheapest and most aggressive first.
 * Order is fixed and total: no set iteration, no PRNG, no locale-dependent sort.
 * @param {string} input @returns {string[]}
 */
export function stringCandidates(input) {
  /** @type {string[]} */
  const out = [];
  /** @param {string} value */
  const push = (value) => { if (value !== input && !out.includes(value)) out.push(value); };

  if (input.length === 0) return out;
  // 1. Halving from both ends — the big wins.
  for (let chunk = Math.ceil(input.length / 2); chunk >= 1; chunk = Math.floor(chunk / 2)) {
    for (let start = 0; start + chunk <= input.length; start += chunk) {
      push(input.slice(0, start) + input.slice(start + chunk));
    }
    if (chunk === 1) break;
  }
  // 2. Trim one unit from each end.
  push(input.slice(1));
  push(input.slice(0, -1));
  // 3. Character simplification: replace exotic code points with 'a', then with ''.
  //    This is what turns "quÍ​t" into "quit" instead of leaving a mystery.
  for (let index = 0; index < input.length; index += 1) {
    const code = /** @type {number} */ (input.codePointAt(index));
    if (code > 0x7f) {
      push(`${input.slice(0, index)}a${input.slice(index + 1)}`);
      push(input.slice(0, index) + input.slice(index + 1));
    }
  }
  return out;
}

/**
 * Minimize `input` while it keeps reproducing the SAME failure tag.
 * @param {string} input @param {FailureProbe} probe
 * @param {{ budget?: number }} [options]
 * @returns {{ minimal: string, tag: string|null, evaluations: number, passes: number, budgetExhausted: boolean }}
 */
export function shrinkString(input, probe, options = {}) {
  const budget = options.budget ?? DEFAULT_SHRINK_BUDGET;
  const originalTag = probe(input);
  let evaluations = 1;
  if (originalTag === null) {
    return { minimal: input, tag: null, evaluations, passes: 0, budgetExhausted: false };
  }

  let best = input;
  let passes = 0;
  let improved = true;
  while (improved && evaluations < budget) {
    improved = false;
    passes += 1;
    for (const candidate of stringCandidates(best)) {
      if (evaluations >= budget) break;
      evaluations += 1;
      if (probe(candidate) === originalTag) {
        best = candidate;
        improved = true;
        break; // restart from the new, smaller base — keeps the walk monotone
      }
    }
  }
  return { minimal: best, tag: originalTag, evaluations, passes, budgetExhausted: evaluations >= budget };
}

/**
 * Minimize a record by dropping optional keys one at a time, in a FIXED key order.
 * Used for envelopes (consent / revision / idempotency siblings) where the question
 * is "which field is actually required to reproduce this?".
 * @param {Record<string, unknown>} input
 * @param {(candidate: Record<string, unknown>) => string|null} probe
 * @param {{ required?: readonly string[], budget?: number }} [options]
 */
export function shrinkRecord(input, probe, options = {}) {
  const required = new Set(options.required ?? []);
  const budget = options.budget ?? DEFAULT_SHRINK_BUDGET;
  const originalTag = probe(input);
  let evaluations = 1;
  if (originalTag === null) return { minimal: input, tag: null, evaluations, dropped: [] };

  let best = { ...input };
  /** @type {string[]} */
  const dropped = [];
  for (const key of Object.keys(input).sort()) {
    if (required.has(key) || evaluations >= budget) continue;
    const candidate = { ...best };
    delete candidate[key];
    evaluations += 1;
    if (probe(candidate) === originalTag) {
      best = candidate;
      dropped.push(key);
    }
  }
  return { minimal: best, tag: originalTag, evaluations, dropped };
}

/**
 * The record a failure must produce to count as reported.
 *
 * Everything here is either a value someone can paste back in or a coordinate they
 * can re-derive it from. `replay` is a literal command line, because a finding whose
 * reproduction has to be reconstructed from prose is a finding that gets closed as
 * "could not reproduce".
 * @param {{ suite: string, property: string, seed: number|string, stream: string,
 *   index: number, tag: string, original: unknown, minimal: unknown,
 *   evaluations: number, detail?: Record<string, unknown> }} spec
 */
export function replayArtifact(spec) {
  return {
    suite: spec.suite,
    property: spec.property,
    seed: spec.seed,
    stream: spec.stream,
    index: spec.index,
    tag: spec.tag,
    original: spec.original,
    minimal: spec.minimal,
    shrinkEvaluations: spec.evaluations,
    detail: spec.detail ?? {},
    replay: `npx vitest run tests/unit/task-51/${spec.suite}.test.ts -t ${JSON.stringify(spec.property)}`,
    reproduce: {
      seed: spec.seed,
      stream: spec.stream,
      index: spec.index,
      note: 'streamFor(seed, stream) then draw `index` cases; the minimal value above reproduces the tag directly.',
    },
  };
}
