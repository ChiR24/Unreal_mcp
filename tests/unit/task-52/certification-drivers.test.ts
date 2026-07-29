// tests/unit/task-52/certification-drivers.test.ts
// Task 52 — the per-engine subset is a NARROWING, not a quiet loss of coverage.
//
// A certification runs a subset of Task 49's corpus on every engine minor,
// because rerunning the full suite nine times buys repetition rather than
// information. The risk that creates is specific and quiet: a subset that loses
// a dimension — the legacy execute form, the absent-polarity oracle, the cancel
// path — still prints "8 pass / 0 fail" and still reads like a full run in the
// report. Nobody re-derives which dimensions a namespace list covers.
//
// So the subset is measured here by the SAME yardstick the full corpus is
// measured by (`coverageOf`, Task 49's own summariser), every omission has to be
// declared, and the native-only skip rule has to keep pointing at the real defect
// that justifies it. All offline: no editor, no ports, no drivers started.

import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { REQUIRED_COVERAGE, buildCorpus } from '../task-49/live-corpus.mjs';
import { coverageOf } from '../task-49/live-corpus-runner.mjs';
import {
  CERTIFICATION_SUBSET,
  DESTRUCTIVE_CLEANUP_NATIVE_ONLY,
  KNOWN_TRANSPORT_DEFECTS,
  SUBSET_OMISSIONS,
} from './certification-drivers.mjs';

type Scenario = {
  namespace: string;
  capability?: string;
  mutates?: boolean;
  oracle?: { capability: string; expect: string } | null;
  cleanup?: { capability: string }[];
  requires: { clients: string[] };
};

const corpus = buildCorpus() as Scenario[];
const byNamespace = new Map(corpus.map((scenario) => [scenario.namespace, scenario]));
const subset = corpus.filter((scenario) => CERTIFICATION_SUBSET.includes(scenario.namespace));

describe('CERTIFICATION_SUBSET', () => {
  it('names only scenarios that really exist', () => {
    // A typo here does not fail loudly at run time — the filter simply yields
    // fewer cases and the certification reports a smaller, greener run.
    const unresolved = CERTIFICATION_SUBSET.filter((namespace) => !byNamespace.has(namespace));
    expect(unresolved).toEqual([]);
    expect(subset.length).toBe(CERTIFICATION_SUBSET.length);
  });

  it('gives up NO coverage dimension the full corpus declares', () => {
    const coverage = coverageOf(subset);
    expect(coverage.primitives).toEqual([...REQUIRED_COVERAGE.primitives].sort());
    expect(coverage.protocolKinds).toEqual([...REQUIRED_COVERAGE.protocolKinds].sort());
    expect(coverage.executeForms).toEqual([...REQUIRED_COVERAGE.executeForms].sort());
    expect(coverage.oraclePolarities).toEqual([...REQUIRED_COVERAGE.oraclePolarities].sort());
  });

  it('is a real narrowing, and every dropped scenario is declared with a reason', () => {
    expect(subset.length).toBeLessThan(corpus.length);
    for (const omission of SUBSET_OMISSIONS) {
      expect(byNamespace.has(omission.namespace), omission.namespace).toBe(true);
      expect(CERTIFICATION_SUBSET).not.toContain(omission.namespace);
      expect(omission.reason.length).toBeGreaterThan(40);
    }
    // Nothing may leave the corpus without landing in one list or the other.
    expect([...CERTIFICATION_SUBSET, ...SUBSET_OMISSIONS.map((entry) => entry.namespace)].sort())
      .toEqual(corpus.map((scenario) => scenario.namespace).sort());
  });

  it('keeps the independent oracle and the cleanup on every mutating case it kept', () => {
    const mutating = subset.filter((scenario) => scenario.mutates === true);
    expect(mutating.length).toBeGreaterThan(0);
    for (const scenario of mutating) {
      expect(scenario.oracle, scenario.namespace).toBeTruthy();
      // The oracle must not be the action under test reading back its own work.
      expect(scenario.oracle?.capability, scenario.namespace).not.toBe(scenario.capability);
      expect(scenario.cleanup?.length ?? 0, scenario.namespace).toBeGreaterThan(0);
    }
  });

  it('keeps both polarities of the same oracle, so neither reading can be explained away', () => {
    const polarities = subset.filter((scenario) => scenario.oracle).map((scenario) => scenario.oracle?.expect);
    expect(polarities).toContain('present');
    expect(polarities).toContain('absent');
  });
});

describe('the native-only destructive rule', () => {
  // The rule is now EMPTY, so the old `length > 0` guard would pin the very
  // exclusion that was removed on evidence. What replaces it is strictly
  // stronger: every destructive scenario in the subset must actually RUN on both
  // transports and must carry a cleanup step, which is what the exclusion broke.
  it('leaves no destructive scenario excluded from a transport it is declared for', () => {
    const destructive = CERTIFICATION_SUBSET
      .map((namespace) => byNamespace.get(namespace))
      .filter((scenario) => (scenario?.cleanup ?? []).some((step) => /delete/u.test(step.capability)));
    expect(destructive.length).toBeGreaterThan(0);
    for (const scenario of destructive) {
      expect(scenario?.requires.clients, scenario?.namespace).toContain('native');
      expect(scenario?.requires.clients, scenario?.namespace).toContain('stdio');
      expect(DESTRUCTIVE_CLEANUP_NATIVE_ONLY, scenario?.namespace).not.toContain(scenario?.namespace);
    }
  });

  it('a cleanup grant names the CANONICAL capability, which is the shape both transports accept', () => {
    for (const namespace of CERTIFICATION_SUBSET) {
      for (const step of byNamespace.get(namespace)?.cleanup ?? []) {
        if (!/delete/u.test(step.capability)) continue;
        const grant = /** @type {any} */ (step as unknown as { consent?: { capability?: string } }).consent;
        expect(grant?.capability, `${namespace} cleanup grant`).toBe(step.capability);
        expect(step.capability, `${namespace} cleanup capability`).not.toMatch(/_asset[s]?$/u);
      }
    }
  });

  it('skips only scenarios that are in the subset and really delete something', () => {
    for (const namespace of DESTRUCTIVE_CLEANUP_NATIVE_ONLY) {
      const scenario = byNamespace.get(namespace);
      expect(scenario, namespace).toBeTruthy();
      expect(CERTIFICATION_SUBSET, namespace).toContain(namespace);
      expect(scenario?.cleanup?.some((step) => /delete/u.test(step.capability)), namespace).toBe(true);
    }
  });

  it('never removes the case from native, which is where the deletion still has to work', () => {
    for (const namespace of DESTRUCTIVE_CLEANUP_NATIVE_ONLY) {
      expect(byNamespace.get(namespace)?.requires.clients).toContain('native');
    }
  });

  it('carries the corrected defect record as a SUPERSEDING entry, not a deletion', () => {
    // The record no longer justifies a skip, so it cannot be validated against
    // one. It must still stay legible: name its capability and transport, KEEP
    // the claim it replaces so the earlier conclusion is not quietly erased, and
    // never be the justification for an exclusion that is still active.
    expect(KNOWN_TRANSPORT_DEFECTS.length).toBeGreaterThan(0);
    for (const defect of KNOWN_TRANSPORT_DEFECTS) {
      expect(defect.transport).toBe('stdio');
      expect(defect.source).toMatch(/Task 49/u);
      expect(defect.capability).toBeTruthy();
      expect(defect.supersedes, 'a record that justifies no skip must name what it supersedes').toBeTruthy();
      expect(defect.supersededClaim, 'the replaced claim must stay on the record').toBeTruthy();
      const stillSkipped = new Set(
        DESTRUCTIVE_CLEANUP_NATIVE_ONLY
          .flatMap((namespace) => byNamespace.get(namespace)?.cleanup ?? [])
          .map((step) => step.capability),
      );
      expect(stillSkipped).not.toContain(defect.capability);
    }
  });
});

describe('port steering', () => {
  const source = readFileSync(join(process.cwd(), 'tests/unit/task-52/certification-drivers.mjs'), 'utf8');
  // Comments name the wave-wide defaults deliberately, to explain what this file
  // must NOT do; stripping them is what makes the assertion about the code.
  const code = source.split('\n').filter((line) => !/^\s*(?:\/\/|\*|\/\*)/u.test(line)).join('\n');

  it('points both drivers at THIS run\'s allocated ports', () => {
    expect(code).toContain('port: spec.ports.native');
    expect(code).toContain('String(spec.ports.wsPrimary)');
  });

  it('hardcodes no wave-wide default port anywhere in the code', () => {
    // A driver that silently falls back to 3000/8090/8091 would score this
    // certification against whatever editor another lane happens to be running —
    // the failure that looks most like success.
    expect(code).not.toMatch(/\b(?:3000|8090|8091)\b/u);
  });
});
