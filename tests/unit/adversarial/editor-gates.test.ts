// tests/unit/adversarial/editor-gates.test.ts
// The editor gates' arithmetic, driven offline.
//
// The live orchestrator needs a packaged plugin, a UBT build and a 20-minute
// editor to produce ONE reading. If that were the only thing exercising the
// scoring, the scoring would be unreviewable — and Task 51 has already shipped
// two unfalsifiable gates that looked green (the instantaneous-baseline pilot,
// then D4 at 32-session scale). So every verdict these gates can return,
// including the ones that only appear when the measurement is worthless, is
// produced here from synthetic readings.
//
// BOTH POLARITIES ARE PINNED for each rule. A test that only proves the invalid
// verdict fires would pass against a gate that ALWAYS returns invalid, which is
// just as useless as one that always passes.

import { describe, expect, it } from 'vitest';

import {
  EDITOR_RETAINED_LIMIT_BYTES, isSteadyState, judgeEditorRss, judgeResidualObjects, parseObjectCount, stillBlocked,
} from './editor-gates.mjs';

const MIB = 1024 * 1024;
const sample = (min: number | null, max: number | null, samples = 6) => ({ min, max, samples });

describe('judgeEditorRss — the editor form of D4', () => {
  it('PASSES a real measurement that stayed under the ceiling', () => {
    const verdict = judgeEditorRss({
      baseline: sample(2000 * MIB, 2010 * MIB),
      mid: sample(2020 * MIB, 2040 * MIB),
      final: sample(2008 * MIB, 2050 * MIB),
    });
    expect(verdict.verdict).toBe('PASS');
    expect(verdict.ok).toBe(true);
    expect(verdict.retainedBytes).toBe(8 * MIB);
    expect(verdict.baselineWasAPeak).toBe(false);
  });

  it('FAILS when the editor retained more than 64 MiB — the gate can fail', () => {
    const verdict = judgeEditorRss({
      baseline: sample(2000 * MIB, 2010 * MIB),
      mid: sample(2040 * MIB, 2060 * MIB),
      final: sample(2100 * MIB, 2120 * MIB),
    });
    expect(verdict.verdict).toBe('FAIL');
    expect(verdict.ok).toBe(false);
    expect(verdict.retainedBytes).toBe(100 * MIB);
  });

  it('refuses a vacuous baseline instead of passing it: peak never exceeded baseline', () => {
    // The exact D4 shape. retained is -60 MiB, which is <= 64 MiB, so a naive
    // comparison returns PASS while proving nothing whatsoever.
    const verdict = judgeEditorRss({
      baseline: sample(2100 * MIB, 2100 * MIB),
      mid: sample(2060 * MIB, 2070 * MIB),
      final: sample(2040 * MIB, 2090 * MIB),
    });
    expect(verdict.retainedBytes).toBeLessThan(0);
    expect(verdict.retainedBytes).toBeLessThanOrEqual(EDITOR_RETAINED_LIMIT_BYTES);
    expect(verdict.verdict).toBe('INVALID_VACUOUS_BASELINE');
    expect(verdict.ok).toBe(false);
    expect(verdict.baselineWasAPeak).toBe(true);
  });

  it('does NOT cry vacuous when the workload genuinely moved memory above baseline', () => {
    // Retained is still negative — the OS reclaimed more than the run added — but
    // RSS demonstrably rose above the baseline, so the comparison had a chance to
    // fail and the negative number is a real reading.
    const verdict = judgeEditorRss({
      baseline: sample(2100 * MIB, 2100 * MIB),
      mid: sample(2150 * MIB, 2180 * MIB),
      final: sample(2090 * MIB, 2160 * MIB),
    });
    expect(verdict.baselineWasAPeak).toBe(false);
    expect(verdict.verdict).toBe('PASS');
  });

  it('reports UNMEASURED, not 0, when the pid stopped answering /proc', () => {
    const verdict = judgeEditorRss({
      baseline: sample(null, null, 0), mid: sample(null, null, 0), final: sample(null, null, 0),
    });
    expect(verdict.verdict).toBe('UNMEASURED');
    expect(verdict.ok).toBe(false);
    expect(verdict.retainedBytes).toBeNull();
  });
});

describe('judgeResidualObjects — a zero that had to be earned', () => {
  const control = { createdObjects: 8, baselineCount: 100_000, controlPeakCount: 100_012, controlReturnCount: 100_000 };

  it('PASSES zero residuals only after the control proved the counter moves both ways', () => {
    const verdict = judgeResidualObjects({ ...control, finalCount: 100_000 });
    expect(verdict.verdict).toBe('PASS');
    expect(verdict.residualObjects).toBe(0);
    expect(verdict.controlRoseBy).toBe(12);
    expect(verdict.controlReturnedBy).toBe(12);
  });

  it('FAILS when objects were retained — the gate can fail', () => {
    const verdict = judgeResidualObjects({ ...control, finalCount: 100_045 });
    expect(verdict.verdict).toBe('FAIL');
    expect(verdict.ok).toBe(false);
    expect(verdict.residualObjects).toBe(45);
  });

  it('refuses a blind counter that never rose for the control objects', () => {
    // Every reading is identical, which is exactly what "zero residual" looks
    // like AND exactly what a stuck counter looks like. Scoring this as a pass is
    // the whole failure mode.
    const verdict = judgeResidualObjects({
      createdObjects: 8, baselineCount: 100_000, controlPeakCount: 100_000,
      controlReturnCount: 100_000, finalCount: 100_000,
    });
    expect(verdict.verdict).toBe('INVALID_BLIND_COUNTER');
    expect(verdict.ok).toBe(false);
    expect(verdict.residualObjects).toBeNull();
  });

  it('refuses a counter that rose but never came back down', () => {
    const verdict = judgeResidualObjects({
      createdObjects: 8, baselineCount: 100_000, controlPeakCount: 100_012,
      controlReturnCount: 100_012, finalCount: 100_000,
    });
    expect(verdict.verdict).toBe('INVALID_BLIND_COUNTER');
    expect(verdict.controlReturnedBy).toBe(0);
  });

  it('reports UNMEASURED when a reading could not be parsed', () => {
    const verdict = judgeResidualObjects({ ...control, finalCount: null });
    expect(verdict.verdict).toBe('UNMEASURED');
    expect(verdict.ok).toBe(false);
  });
});

describe('parseObjectCount — the reading names the line it came from', () => {
  it('reads the UE5 obj list summary and reports which pattern matched', () => {
    const parsed = parseObjectCount('...\n123456 Objects (Total: 1.234M / Max: 2.345M)\n');
    expect(parsed.count).toBe(123_456);
    expect(parsed.matchedPattern).toBe('objects-total');
  });

  it('reads a comma-grouped count', () => {
    expect(parseObjectCount('1,234,567 Objects (Total: 9.9M)').count).toBe(1_234_567);
  });

  it('reads the alternate "Total: N objects" shape', () => {
    const parsed = parseObjectCount('Total: 4242 objects');
    expect(parsed.count).toBe(4242);
    expect(parsed.matchedPattern).toBe('total-objects');
  });

  it('takes the LARGEST summary in a multi-section memreport, not the last', () => {
    // The live failure: a 317KB memreport matched this line 8 times and the LAST
    // was a zero-count group, so the census read 0 objects every single time.
    const parsed = parseObjectCount([
      'Mem FromReport', '111 Objects (Total: 1.0M / Max: 2.0M)',
      'obj list -resourcesizesort', '222333 Objects (Total: 9.0M / Max: 9.5M)',
      'some empty group', '0 Objects (Total: 0.0M / Max: 0.0M)',
    ].join('\n'));
    expect(parsed.count).toBe(222_333);
    expect(parsed.occurrences).toBe(3);
    expect(parsed.allCounts?.[0]).toBe(222_333);
  });

  it('returns null rather than guessing when no known summary line is present', () => {
    const parsed = parseObjectCount('Command executed');
    expect(parsed.count).toBeNull();
    expect(parsed.reason).toBe('NO_KNOWN_SUMMARY_LINE');
  });

  it('does not invent a count from an empty response', () => {
    expect(parseObjectCount('').reason).toBe('EMPTY_RESPONSE');
  });
});

describe('isSteadyState — refusing to baseline on a slope', () => {
  const t = (...mins: number[]) => mins.map((min) => ({ min }));

  it('accepts three troughs that have stopped moving', () => {
    const verdict = isSteadyState(t(900, 902, 901).map((entry) => ({ min: entry.min * MIB })));
    expect(verdict.steady).toBe(true);
    expect(verdict.reason).toBe('STEADY');
  });

  it('REJECTS the live shape: a baseline still shedding the start-up transient', () => {
    // 2677 -> 1800 -> 873 MiB is the real run-4 decay that produced -1798 MiB
    // retained. Baselining anywhere on this slope is unfalsifiable.
    const verdict = isSteadyState(t(2677, 1800, 873).map((entry) => ({ min: entry.min * MIB })));
    expect(verdict.steady).toBe(false);
    expect(verdict.reason).toBe('STILL_DECAYING');
  });

  it('rejects a slow decay that would sneak inside a naive tolerance band', () => {
    const verdict = isSteadyState(t(1000, 995, 991).map((entry) => ({ min: entry.min * MIB })));
    expect(verdict.steady).toBe(false);
  });

  it('does not claim steadiness from too few samples', () => {
    expect(isSteadyState(t(900, 900).map((entry) => ({ min: entry.min * MIB }))).reason)
      .toBe('NOT_ENOUGH_SAMPLES');
  });

  it('tolerates ordinary jitter that is not a trend', () => {
    expect(isSteadyState(t(900, 903, 901).map((entry) => ({ min: entry.min * MIB }))).steady).toBe(true);
  });
});

describe('stillBlocked', () => {
  it('restates the condition instead of estimating a value', () => {
    const entry = stillBlocked({ claim: 'c', code: 'CODE', observable: 'why' });
    expect(entry.status).toBe('STILL BLOCKED');
    expect(entry).not.toHaveProperty('value');
    expect(entry.observable).toBe('why');
  });
});
