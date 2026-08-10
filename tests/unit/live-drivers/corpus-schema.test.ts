// tests/unit/live-drivers/corpus-schema.test.ts
// Task 49 — the corpus schema must REFUSE, and every refusal is proven here by
// feeding it a deliberately bad scenario.
//
// A schema nobody has watched reject anything is a comment. Each case below
// starts from a scenario the schema ACCEPTS, changes exactly one thing, and
// asserts the specific closed reason — so a refusal that fires for the wrong
// cause (or a validator that refuses everything) fails just as loudly as one
// that does not fire at all.

import { describe, expect, it } from 'vitest';

import {
  CORPUS_REASONS,
  CorpusRejection,
  OWNED_ROOT,
  TIER_TIMEOUT_MS,
  indexRecords,
  parseExpectation,
  validateCorpus,
  validateScenario,
} from './live-corpus-schema.mjs';
import { RAW_SCENARIOS, REQUIRED_COVERAGE, buildCorpus } from './live-corpus.mjs';
import { coverageOf } from './live-corpus-runner.mjs';
import { loadRecords } from '../cross-transport/matrix-dimensions.mjs';

const index = indexRecords(loadRecords());

/** A scenario the schema accepts. Every rejection case is this, minus one thing. */
function baseline(): Record<string, unknown> {
  return {
    namespace: 'task49.spec.baseline',
    title: 'baseline mutating scenario used to prove each refusal',
    primitive: 'execute',
    form: 'canonical',
    capability: 'material.create_material',
    ownedPath: `${OWNED_ROOT}/task49-spec-baseline`,
    request: {
      params: { name: 'M_SpecBaseline', path: `${OWNED_ROOT}/task49-spec-baseline` },
      consent: { capability: 'material.create_material', acknowledge: 'explicit' },
    },
    expected: 'success',
    timeoutTier: 'interactive',
    oracle: {
      capability: 'asset.list',
      params: { path: `${OWNED_ROOT}/task49-spec-baseline` },
      expect: 'present',
      needle: 'M_SpecBaseline',
    },
    cleanup: [{
      capability: 'asset.delete_asset',
      params: { assetPath: `${OWNED_ROOT}/task49-spec-baseline/M_SpecBaseline` },
      consent: { capability: 'asset.delete_asset', acknowledge: 'elevated' },
    }],
    requires: {
      unrealMin: '5.0.0',
      plugins: ['EditorScriptingUtilities'],
      editorStates: ['edit'],
      clients: ['stdio', 'native'],
    },
  };
}

/** Apply one mutation and capture the refusal reason, or `null` if it was accepted. */
function reasonFor(mutate: (scenario: Record<string, unknown>) => void): string | null {
  const scenario = baseline();
  mutate(scenario);
  try {
    validateScenario(scenario, { index });
    return null;
  } catch (error) {
    if (error instanceof CorpusRejection) return error.reason;
    throw error;
  }
}

describe('Task 49 corpus schema — the baseline is genuinely accepted', () => {
  // POSITIVE CONTROL. Without it every rejection below would also pass against a
  // validator that refuses literally everything, and the suite would be worthless.
  it('accepts the unmodified baseline', () => {
    const scenario = validateScenario(baseline(), { index });
    expect(scenario.namespace).toBe('task49.spec.baseline');
    expect(scenario.mutates).toBe(true);
    expect(scenario.timeoutMs).toBe(TIER_TIMEOUT_MS.interactive);
  });
});

describe('Task 49 corpus schema — required refusals', () => {
  it('rejects an unknown capability', () => {
    expect(reasonFor((s) => { s.capability = 'manage_asset.definitely_not_a_capability'; }))
      .toBe(CORPUS_REASONS.UNKNOWN_CAPABILITY);
  });

  it('rejects an unknown parameter on a known capability', () => {
    expect(reasonFor((s) => {
      const request = s.request as Record<string, Record<string, unknown>>;
      request.params.notAParameterOfCreateMaterial = true;
    })).toBe(CORPUS_REASONS.UNKNOWN_PARAM);
  });

  it('rejects the broad success|error mask', () => {
    expect(reasonFor((s) => { s.expected = 'success|error'; })).toBe(CORPUS_REASONS.BROAD_EXPECTATION);
  });

  it('rejects the same mask written with " or "', () => {
    expect(reasonFor((s) => { s.expected = 'success or error'; })).toBe(CORPUS_REASONS.BROAD_EXPECTATION);
  });

  it('rejects timeout placed AFTER error (a timeout passes only as the primary condition)', () => {
    expect(reasonFor((s) => { s.expected = 'error|timeout'; s.expectedErrorCode = 'SOME_CODE'; }))
      .toBe(CORPUS_REASONS.BROAD_EXPECTATION);
  });

  it('rejects an expectation whose first token is not a primary intent', () => {
    expect(reasonFor((s) => { s.expected = 'already exists|success'; })).toBe(CORPUS_REASONS.BROAD_EXPECTATION);
  });

  it('rejects a wildcard alternative', () => {
    expect(reasonFor((s) => { s.expected = 'success|any'; })).toBe(CORPUS_REASONS.BROAD_EXPECTATION);
  });

  it('rejects a mutation with no oracle', () => {
    expect(reasonFor((s) => { delete s.oracle; })).toBe(CORPUS_REASONS.MISSING_ORACLE);
  });

  it('rejects a mutation with no cleanup', () => {
    expect(reasonFor((s) => { s.cleanup = []; })).toBe(CORPUS_REASONS.MISSING_CLEANUP);
  });

  it('rejects a duplicate namespace across the corpus', () => {
    const first = baseline();
    const second = baseline();
    (second as Record<string, unknown>).ownedPath = `${OWNED_ROOT}/task49-spec-baseline-two`;
    const secondRequest = second.request as Record<string, Record<string, unknown>>;
    secondRequest.params.path = `${OWNED_ROOT}/task49-spec-baseline-two`;
    const secondOracle = second.oracle as Record<string, unknown>;
    (secondOracle.params as Record<string, unknown>).path = `${OWNED_ROOT}/task49-spec-baseline-two`;
    const secondCleanup = second.cleanup as Array<Record<string, Record<string, unknown>>>;
    secondCleanup[0].params.assetPath = `${OWNED_ROOT}/task49-spec-baseline-two/M_SpecBaseline`;
    try {
      validateCorpus([first, second], { index });
      throw new Error('validateCorpus accepted two scenarios sharing a namespace');
    } catch (error) {
      expect(error).toBeInstanceOf(CorpusRejection);
      expect((error as CorpusRejection).reason).toBe(CORPUS_REASONS.DUPLICATE_NAMESPACE);
    }
  });

  it('rejects a duplicate owned content path even when namespaces differ', () => {
    const first = baseline();
    const second = baseline();
    second.namespace = 'task49.spec.baseline-two';
    try {
      validateCorpus([first, second], { index });
      throw new Error('validateCorpus accepted two scenarios owning one content path');
    } catch (error) {
      expect((error as CorpusRejection).reason).toBe(CORPUS_REASONS.DUPLICATE_NAMESPACE);
    }
  });
});

describe('Task 49 corpus schema — independence of the oracle', () => {
  it('rejects an oracle that re-runs the capability under test', () => {
    expect(reasonFor((s) => {
      s.oracle = {
        capability: 'material.create_material',
        params: { name: 'M_SpecBaseline', path: `${OWNED_ROOT}/task49-spec-baseline` },
        expect: 'present',
        needle: 'M_SpecBaseline',
      };
    })).toBe(CORPUS_REASONS.DEPENDENT_ORACLE);
  });

  it('rejects an oracle that is itself a mutation', () => {
    expect(reasonFor((s) => {
      s.oracle = {
        capability: 'asset.delete_asset',
        params: { assetPath: `${OWNED_ROOT}/task49-spec-baseline/M_SpecBaseline` },
        expect: 'absent',
        needle: 'M_SpecBaseline',
      };
    })).toBe(CORPUS_REASONS.MUTATING_ORACLE);
  });
});

describe('Task 49 corpus schema — containment, tiers and requirements', () => {
  it('rejects a mutation aimed outside the scenario-owned namespace', () => {
    expect(reasonFor((s) => {
      const request = s.request as Record<string, Record<string, unknown>>;
      request.params.path = '/Game/SomeoneElsesContent';
    })).toBe(CORPUS_REASONS.UNOWNED_TARGET);
  });

  it('rejects a cleanup step aimed outside the scenario-owned namespace', () => {
    expect(reasonFor((s) => {
      const cleanup = s.cleanup as Array<Record<string, Record<string, unknown>>>;
      cleanup[0].params.assetPath = '/Game/RealProject/ImportantMaterial';
    })).toBe(CORPUS_REASONS.UNOWNED_TARGET);
  });

  it('rejects an owned path outside /Game/MCPTest', () => {
    expect(reasonFor((s) => { s.ownedPath = '/Game/Elsewhere/task49'; })).toBe(CORPUS_REASONS.UNOWNED_TARGET);
  });

  it('rejects a timeout tier that contradicts the capability contract', () => {
    // material.create_material is `interactive`; claiming `long-running` would
    // arm a 4-minute deadline and hide a queue stall behind a green result.
    expect(reasonFor((s) => { s.timeoutTier = 'long-running'; })).toBe(CORPUS_REASONS.TIMEOUT_TIER_MISMATCH);
  });

  it('rejects an under-declared plugin requirement', () => {
    expect(reasonFor((s) => {
      (s.requires as Record<string, unknown>).plugins = [];
    })).toBe(CORPUS_REASONS.REQUIREMENT_UNDERDECLARED);
  });

  it('rejects an under-declared engine floor', () => {
    // material.create_material declares a 5.0.0 floor. A scenario claiming it
    // runs on 4.27 asserts coverage on an engine where the capability does not
    // exist — which is how a matrix reports a version it never exercised.
    expect(reasonFor((s) => {
      (s.requires as Record<string, unknown>).unrealMin = '4.27.0';
    })).toBe(CORPUS_REASONS.REQUIREMENT_UNDERDECLARED);
  });

  it('rejects an error-primary scenario that names no typed code', () => {
    expect(reasonFor((s) => { s.expected = 'error'; })).toBe(CORPUS_REASONS.MISSING_ERROR_CODE);
  });

  it('rejects an undeclared top-level field', () => {
    expect(reasonFor((s) => { s.retries = 3; })).toBe(CORPUS_REASONS.UNKNOWN_FIELD);
  });

  it('rejects a schema dump smuggled in as a parameter', () => {
    expect(reasonFor((s) => {
      const request = s.request as Record<string, Record<string, unknown>>;
      request.params.properties = { name: { type: 'string' } };
    })).toBe(CORPUS_REASONS.SCHEMA_DUMP);
  });

  it('rejects a malformed namespace', () => {
    expect(reasonFor((s) => { s.namespace = 'Task 49 Baseline'; })).toBe(CORPUS_REASONS.MALFORMED);
  });
});

describe('Task 49 expectation grammar', () => {
  it('accepts narrow success alternatives', () => {
    const parsed = parseExpectation('success|already exists', '/expected');
    expect(parsed.intent).toBe('success');
    expect(parsed.alternatives).toEqual(['already exists']);
  });

  it('accepts timeout as the primary condition', () => {
    expect(parseExpectation('timeout', '/expected').intent).toBe('timeout');
  });

  for (const mask of ['success|error', 'error|success', 'error|timeout', 'success|timeout', 'success|*']) {
    it(`refuses the mask "${mask}"`, () => {
      expect(() => parseExpectation(mask, '/expected')).toThrow(CorpusRejection);
    });
  }
});

describe('Task 49 corpus — the shipped scenarios', () => {
  const scenarios = buildCorpus();

  it('every shipped scenario passes the schema', () => {
    expect(scenarios.length).toBe(RAW_SCENARIOS.length);
    expect(scenarios.length).toBeGreaterThan(0);
  });

  it('covers every gateway primitive, protocol kind, execute form and oracle polarity', () => {
    const coverage = coverageOf(scenarios);
    expect(coverage.primitives).toEqual([...REQUIRED_COVERAGE.primitives].sort());
    expect(coverage.protocolKinds).toEqual([...REQUIRED_COVERAGE.protocolKinds].sort());
    expect(coverage.executeForms).toEqual([...REQUIRED_COVERAGE.executeForms].sort());
    expect(coverage.oraclePolarities).toEqual([...REQUIRED_COVERAGE.oraclePolarities].sort());
  });

  it('every mutating scenario has an independent read oracle and cleanup', () => {
    for (const scenario of scenarios.filter((entry) => entry.mutates)) {
      expect(scenario.oracle, scenario.namespace).not.toBeNull();
      expect(scenario.oracle?.capability, scenario.namespace).not.toBe(scenario.capability);
      expect(scenario.cleanup.length, scenario.namespace).toBeGreaterThan(0);
    }
  });

  it('ships a POSITIVE CONTROL for the absent-polarity oracle', () => {
    // An `absent` reading only means something if the SAME oracle capability was
    // watched returning `present` in another scenario. Otherwise a permanently
    // blind oracle satisfies every absence assertion in the corpus.
    const absent = scenarios.filter((entry) => entry.oracle?.expect === 'absent');
    expect(absent.length).toBeGreaterThan(0);
    for (const scenario of absent) {
      const control = scenarios.find((entry) => entry.oracle?.expect === 'present'
        && entry.oracle?.capability === scenario.oracle?.capability);
      expect(control, `no present-polarity control for ${scenario.namespace}`).toBeDefined();
    }
  });

  it('every scenario declares at least one client transport', () => {
    for (const scenario of scenarios) {
      expect(scenario.requires.clients.length, scenario.namespace).toBeGreaterThan(0);
    }
  });
});
