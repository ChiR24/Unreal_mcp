// @ts-check
// tests/unit/adversarial/editor-gates.mjs
// Task 51 — the two EDITOR-SIDE gates, and the vacuity rules that make them
// capable of failing.
//
// These two claims sat BLOCKED with the observable EDITOR_OWNED_BY_ANOTHER_LANE
// because the lane that produced them was not allowed to start an editor. Nothing
// about the ARITHMETIC was blocked, so the arithmetic lives here, is driven
// offline by editor-gates.test.ts, and the live orchestrator only supplies
// readings. A gate whose scoring is only ever exercised by a 20-minute editor run
// is a gate nobody can check — which is exactly how D4 survived review once.
//
// WHY THE SCORING IS NOT A COMPARISON.
//
// Task 51 shipped the same defect twice. The first time, a single instantaneous
// post-warm-up RSS sample landed on a transient peak, so `final - baseline` came
// out negative and `<= 32 MiB` was true for every possible input, including a real
// leak. The settled-trough fix removed that for ONE process. At 32-session scale
// it came straight back (D4): thirty-two children each carrying 1/32 of the
// traffic decay faster than they grow, so all 32 retained deltas were negative
// (worst -28.61 MiB, mean -109.58 MiB) and the gate STILL could not fail.
//
// The editor is a third instance of the same trap and the most dangerous one,
// because a multi-gigabyte process whose RSS wanders by tens of MiB for reasons
// that have nothing to do with MCP will happily produce a comfortable negative
// number. So both gates here refuse to report PASS on a reading that could not
// have produced FAIL:
//
//   RSS       — if the baseline was itself a peak, the verdict is
//               INVALID_VACUOUS_BASELINE, never PASS.
//   RESIDUALS — if the positive control never moved the counter, the counter is
//               blind and the verdict is INVALID_BLIND_COUNTER, never PASS.
//
// A named invalid verdict is a worse-looking result than a green check and a
// better one than a number nobody can trust.

import { summariseRss } from './load-harness.mjs';

/** The plan's editor ceiling. Task 51's Node gate is 32 MiB; the editor's is 64. */
export const EDITOR_RETAINED_LIMIT_BYTES = 64 * 1024 * 1024;

/** Bytes -> MiB, 2dp, for human-readable detail strings. @param {number|null} bytes */
export const mib = (bytes) => (typeof bytes === 'number' ? `${(bytes / 1048576).toFixed(2)} MiB` : 'n/a');

/**
 * Score the editor retained-RSS gate from three settled troughs.
 *
 * `baseline`, `mid` and `final` must all be the MINIMUM of a settle window, never
 * an instantaneous read, or the comparison is not like-for-like. The caller proves
 * that by passing the whole sample object through; `samples` reaching 0 means the
 * pid vanished mid-measurement and is reported as UNMEASURED rather than as 0.
 *
 * @param {{ baseline: {min:number|null,max:number|null,samples:number},
 *           mid: {min:number|null,max:number|null,samples:number},
 *           final: {min:number|null,max:number|null,samples:number},
 *           limitBytes?: number }} spec
 */
export function judgeEditorRss(spec) {
  const limitBytes = spec.limitBytes ?? EDITOR_RETAINED_LIMIT_BYTES;
  const { baseline, mid, final } = spec;
  if (baseline.min === null || final.min === null || baseline.samples === 0 || final.samples === 0) {
    return {
      verdict: 'UNMEASURED', ok: false, retainedBytes: null, secondHalfGrowthBytes: null,
      peakOverBaselineBytes: null, baselineWasAPeak: null, limitBytes,
      detail: 'the editor pid stopped answering /proc during the measurement, so no delta exists to score',
    };
  }
  const retainedBytes = final.min - baseline.min;
  const secondHalfGrowthBytes = mid.min === null ? null : final.min - mid.min;
  const peakOverBaselineBytes = final.max === null ? null : final.max - baseline.min;

  // Reuse Task 51's own reducer so the editor and the 32-session load are scored
  // by ONE implementation of the vacuity rule. A second copy would be free to
  // drift, and the shape it guards against is precisely the one that drifted.
  const summary = summariseRss([{ retainedBytes, secondHalfGrowthBytes, peakOverBaselineBytes }]);

  if (summary.baselineWasAPeak) {
    return {
      verdict: 'INVALID_VACUOUS_BASELINE', ok: false, retainedBytes, secondHalfGrowthBytes,
      peakOverBaselineBytes, baselineWasAPeak: true, limitBytes,
      detail: `the post-warm-up baseline (${mib(baseline.min)}) was never exceeded afterwards `
        + `(peak over baseline ${mib(peakOverBaselineBytes)}), so retained ${mib(retainedBytes)} is negative by `
        + 'construction and <= 64 MiB could not have failed. This is D4 in the editor, and it is NOT a pass.',
    };
  }
  const ok = retainedBytes <= limitBytes;
  return {
    verdict: ok ? 'PASS' : 'FAIL', ok, retainedBytes, secondHalfGrowthBytes,
    peakOverBaselineBytes, baselineWasAPeak: false, limitBytes,
    detail: `retained ${mib(retainedBytes)} vs limit ${mib(limitBytes)}; second-half growth `
      + `${mib(secondHalfGrowthBytes)}; the workload moved RSS ${mib(peakOverBaselineBytes)} above baseline, `
      + 'so a leak of gate size would have been visible',
  };
}

/**
 * Has the editor's start-up transient finished decaying?
 *
 * THE REASON THIS EXISTS. A 30-request warm-up is nothing to a multi-gigabyte
 * editor. Two live runs took their post-warm-up trough while the process was
 * still shedding its asset-registry gather — one baseline of 2677 MiB decayed to
 * 873 MiB DURING the measured phase — so the retained delta came out at
 * -1798 MiB and `<= 64 MiB` could not have failed. That is D4 again, a third
 * time, and the fix is not a bigger tolerance: it is refusing to take a baseline
 * until consecutive troughs stop falling.
 *
 * Returns whether the readings converged, so a run that never reaches steady
 * state reports that condition instead of quietly baselining on a slope.
 * @param {Array<{min:number|null}>} troughs consecutive settled troughs, oldest first
 * @param {{ window?: number, tolerance?: number }} [options]
 */
export function isSteadyState(troughs, options = {}) {
  const window = options.window ?? 3;
  const tolerance = options.tolerance ?? 0.01;
  const values = troughs.map((entry) => entry.min).filter((value) => typeof value === 'number');
  if (values.length < window) return { steady: false, reason: 'NOT_ENOUGH_SAMPLES', spread: null };
  const recent = /** @type {number[]} */ (values.slice(-window));
  const high = Math.max(...recent);
  const low = Math.min(...recent);
  const spread = high === 0 ? 0 : (high - low) / high;
  // Falling still counts as unsettled even inside the tolerance band, because a
  // slow monotonic decay is exactly what produces a vacuous baseline.
  const falling = recent[recent.length - 1] < recent[0] && spread > tolerance / 2;
  return {
    steady: spread <= tolerance && !falling,
    reason: spread <= tolerance && !falling ? 'STEADY' : (falling ? 'STILL_DECAYING' : 'STILL_MOVING'),
    spread,
  };
}

/**
 * Pull the total live UObject count out of an `obj list` response.
 *
 * UE has printed this summary in more than one shape across 5.0-5.8, so every
 * form this recognises is listed and the one that matched is REPORTED. A parser
 * that silently picks a number is how a counter starts reading something else;
 * the caller records `matchedPattern` alongside the value so the reading can be
 * re-derived from the raw text kept in evidence.
 * @param {string} text
 */
export function parseObjectCount(text) {
  if (typeof text !== 'string' || text.length === 0) {
    return { count: null, matchedPattern: null, reason: 'EMPTY_RESPONSE' };
  }
  const patterns = [
    // UE5 `obj list` summary: "123456 Objects (Total: 1.234M / Max: ...)"
    { name: 'objects-total', re: /(\d[\d,]*)\s+Objects?\s*\((?:Total|Max)/giu },
    // Older/alternate: "Total: 123456 objects"
    { name: 'total-objects', re: /Total:\s*(\d[\d,]*)\s+objects?/giu },
    // `obj list class=X` per-class tail: "123 Objects" on its own line
    { name: 'objects-bare', re: /^\s*(\d[\d,]*)\s+Objects?\s*$/gimu },
  ];
  for (const { name, re } of patterns) {
    // A .memreport carries `obj list` output in SEVERAL sections, so this line
    // appears many times — a live run found it 8 times and the last one was a
    // zero-count group, which is why "take the last" read the census as 0 objects
    // and the gate (correctly) scored INVALID_BLIND_COUNTER instead of passing.
    // The grand total is necessarily the LARGEST of them. This is still only a
    // heuristic, so it is never trusted on its own: the positive control has to
    // move whichever number this picks, or judgeResidualObjects refuses it.
    const found = [...text.matchAll(re)]
      .map((match) => Number(match[1].replace(/,/gu, '')))
      .filter((value) => Number.isFinite(value));
    if (found.length > 0) {
      return {
        count: Math.max(...found), matchedPattern: name, occurrences: found.length,
        allCounts: [...found].sort((left, right) => right - left).slice(0, 8), reason: 'OK',
      };
    }
  }
  return { count: null, matchedPattern: null, occurrences: 0, reason: 'NO_KNOWN_SUMMARY_LINE' };
}

/**
 * Score the residual-UObject gate.
 *
 * The positive control is not decoration. `final - baseline == 0` is the expected
 * reading for a healthy editor AND the reading a counter that is stuck, blind or
 * parsed off the wrong line produces every single time. So the control creates
 * real objects first and the gate refuses to score anything unless the counter was
 * seen to RISE for them and FALL again when they were destroyed. Without both
 * movements the instrument has not been shown to work and the verdict is
 * INVALID_BLIND_COUNTER.
 *
 * @param {{ baselineCount:number|null, controlPeakCount:number|null,
 *           controlReturnCount:number|null, finalCount:number|null,
 *           createdObjects:number, toleranceObjects?: number }} spec
 */
export function judgeResidualObjects(spec) {
  const tolerance = spec.toleranceObjects ?? 0;
  const { baselineCount, controlPeakCount, controlReturnCount, finalCount, createdObjects } = spec;
  const readings = { baselineCount, controlPeakCount, controlReturnCount, finalCount };
  if ([baselineCount, controlPeakCount, controlReturnCount, finalCount].some((value) => typeof value !== 'number')) {
    return {
      verdict: 'UNMEASURED', ok: false, residualObjects: null, controlRoseBy: null,
      controlReturnedBy: null, readings, toleranceObjects: tolerance,
      detail: 'at least one object-count reading could not be parsed from the editor, so there is no delta to score',
    };
  }
  const controlRoseBy = /** @type {number} */ (controlPeakCount) - /** @type {number} */ (baselineCount);
  const controlReturnedBy = /** @type {number} */ (controlPeakCount) - /** @type {number} */ (controlReturnCount);
  if (controlRoseBy < createdObjects) {
    return {
      verdict: 'INVALID_BLIND_COUNTER', ok: false, residualObjects: null, controlRoseBy, controlReturnedBy,
      readings, toleranceObjects: tolerance,
      detail: `the positive control created ${createdObjects} objects and the counter rose by only ${controlRoseBy}. `
        + 'A counter that cannot see objects it was just told to make cannot prove the absence of residual ones, '
        + 'so "zero residual" here would be a property of the instrument, not of the editor.',
    };
  }
  if (controlReturnedBy <= 0) {
    return {
      verdict: 'INVALID_BLIND_COUNTER', ok: false, residualObjects: null, controlRoseBy, controlReturnedBy,
      readings, toleranceObjects: tolerance,
      detail: `the counter rose by ${controlRoseBy} for the control objects but never fell again after they were `
        + 'destroyed and GC ran, so it cannot distinguish a released object from a retained one.',
    };
  }
  const residualObjects = /** @type {number} */ (finalCount) - /** @type {number} */ (baselineCount);
  const ok = residualObjects <= tolerance;
  return {
    verdict: ok ? 'PASS' : 'FAIL', ok, residualObjects, controlRoseBy, controlReturnedBy,
    readings, toleranceObjects: tolerance,
    detail: `${residualObjects} residual UObjects after the session cycle and a forced GC `
      + `(tolerance ${tolerance}); the control proved the counter moves (+${controlRoseBy} on create, `
      + `-${controlReturnedBy} on destroy), so zero here is a measurement rather than a blind spot`,
  };
}

/**
 * A claim this run could not measure, restated rather than estimated.
 * @param {{ claim:string, code:string, observable:string }} spec
 */
export function stillBlocked(spec) {
  return { claim: spec.claim, code: spec.code, status: 'STILL BLOCKED', observable: spec.observable };
}
