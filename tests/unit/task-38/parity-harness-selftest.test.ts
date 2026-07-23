// tests/unit/task-38/parity-harness-selftest.test.ts
//
// Task 38 lane E — SELF-TESTS. These prove the harness has teeth: that it catches
// each drift class at EXACTLY the expected point and refuses every inadmissible
// fixture with EXACTLY the expected reason. A comparator that could never fail
// (or a validator that rejects everything) would be worthless; every case here
// pairs a positive control (a valid sibling passes) with the drift/rejection.
//
// The final block is the deliverable's headline: the harness REFUSES to treat a
// source-only or native-fake side as a passing parity. There is no argument that
// turns a modelled/grepped native side into ready:true — the real on-disk state
// is RED, and the RED requirement carries the exact producer interface.

import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';

import {
  checkFixture,
  validateRevision,
  validatePointer,
  validateSession,
  validateResult,
  diff,
  compareCaptures,
  assertParityReady,
  isGenuineNativeCapture,
  NATIVE_CAPTURE_REQUIREMENT,
  REASONS,
  driftResultField,
  wrongErrorCode,
  staleRevision,
  falseCapability,
  unboundFallback,
  omitCleanup,
  crossSessionRecord,
  toSourceText,
  broadenMatch,
  addUnknownField,
  injectSchemaDump,
} from './parity-harness.mjs';
import { parseClientCapabilityProfile } from '../../../src/server/mcp-primitives/session-capability-profile.js';

function loadFixture(name: string): Record<string, unknown> {
  return JSON.parse(readFileSync(new URL(`../../fixtures/task-38/${name}`, import.meta.url), 'utf8')) as Record<string, unknown>;
}

// The reason a thunk throws, or a thrown assertion if it did not reject at all.
function reasonOf(thunk: () => unknown): string {
  try {
    thunk();
  } catch (error) {
    const reason = (error as { reason?: string }).reason;
    if (typeof reason === 'string') return reason;
    throw new Error(`threw a non-HarnessRejection: ${String(error)}`);
  }
  throw new Error('expected a HarnessRejection but nothing was thrown');
}

const RESULT = loadFixture('executable-ts-result.json');
const ERROR = loadFixture('executable-ts-error.json');
const REVISION = loadFixture('executable-ts-revision.json');
const SESSION = loadFixture('executable-ts-session.json');
const value = (fixture: Record<string, unknown>): Record<string, unknown> => fixture.value as Record<string, unknown>;

// ===========================================================================
// 1. Each of the six drift classes fails at EXACTLY the expected point.
// ===========================================================================

describe('Task 38 lane E — six drift classes each fail exactly', () => {
  it('one-field result drift is a single pointer mismatch and nothing else', () => {
    const drifted = driftResultField(value(RESULT) as never, 'revision', 2);
    expect(diff(value(RESULT), drifted)).toEqual([{ pointer: '/revision', left: 1, right: 2 }]);
  });

  it('a wrong error code is a single pointer mismatch at /code', () => {
    const drifted = wrongErrorCode(value(ERROR) as never, 'RESOURCE_UNAVAILABLE');
    expect(diff(value(ERROR), drifted)).toEqual([{ pointer: '/code', left: 'RESOURCE_NOT_FOUND', right: 'RESOURCE_UNAVAILABLE' }]);
  });

  it('a false capability is a single pointer mismatch at the over-claimed flag', () => {
    // Grounded honesty: strings never enable a capability, so this profile is all-false.
    const honest = parseClientCapabilityProfile({ resources: 'true', tasks: 'enabled' });
    const drifted = falseCapability(honest as never, 'hasTasks');
    expect(diff(honest, drifted)).toEqual([{ pointer: '/hasTasks', left: false, right: true }]);
  });

  it('a stale revision is rejected at exactly its index', () => {
    expect(validateRevision(value(REVISION))).toBeTruthy(); // positive control: pristine passes
    expect(reasonOf(() => validateRevision(staleRevision(value(REVISION) as never)))).toBe(REASONS.STALE_REVISION);
  });

  it('an unbounded fallback pointer is rejected', () => {
    expect(reasonOf(() => validatePointer(unboundFallback()))).toBe(REASONS.UNBOUNDED_FALLBACK);
  });

  it('a session missing its cleanup receipt is rejected', () => {
    expect(validateSession(value(SESSION))).toBeTruthy(); // positive control: cleaned session passes
    expect(reasonOf(() => validateSession(omitCleanup(value(SESSION) as never)))).toBe(REASONS.MISSING_CLEANUP);
  });
});

// ===========================================================================
// 2. Each inadmissible shape is refused with EXACTLY its reason (via injectors).
// ===========================================================================

describe('Task 38 lane E — the seven rejection rules each fire exactly', () => {
  it('accepts the pristine capture (specificity: rejection is not a blanket refusal)', () => {
    expect(checkFixture(RESULT).executable).toBe(true);
  });

  it('rejects a source-text snapshot with SOURCE_TEXT_CAPTURE', () => {
    expect(reasonOf(() => checkFixture(toSourceText(RESULT)))).toBe(REASONS.SOURCE_TEXT_CAPTURE);
  });

  it('rejects a broad expectation with BROAD_EXPECTATION', () => {
    expect(reasonOf(() => checkFixture(broadenMatch(RESULT)))).toBe(REASONS.BROAD_EXPECTATION);
  });

  it('rejects an unknown field with UNKNOWN_FIELD', () => {
    expect(reasonOf(() => checkFixture(addUnknownField(RESULT, 'capturedAt', 'x')))).toBe(REASONS.UNKNOWN_FIELD);
  });

  it('rejects a schema dump with SCHEMA_DUMP', () => {
    expect(reasonOf(() => validateResult(injectSchemaDump(value(RESULT) as never)))).toBe(REASONS.SCHEMA_DUMP);
  });

  it('rejects a stale revision with STALE_REVISION', () => {
    expect(reasonOf(() => validateRevision(staleRevision(value(REVISION) as never)))).toBe(REASONS.STALE_REVISION);
  });

  it('rejects a cross-session leak with CROSS_SESSION_ID', () => {
    expect(reasonOf(() => validateSession(crossSessionRecord(value(SESSION) as never)))).toBe(REASONS.CROSS_SESSION_ID);
  });

  it('rejects a missing cleanup with MISSING_CLEANUP', () => {
    expect(reasonOf(() => validateSession(omitCleanup(value(SESSION) as never)))).toBe(REASONS.MISSING_CLEANUP);
  });
});

// ===========================================================================
// 3. The on-disk malformed fixtures are each rejected with their manifest reason.
// ===========================================================================

describe('Task 38 lane E — on-disk malformed fixtures reject with their manifest reason', () => {
  const manifest = loadFixture('malformed/_expected.json').expected as Record<string, string>;

  for (const [file, expectedReason] of Object.entries(manifest)) {
    it(`${file} is rejected with ${expectedReason}`, () => {
      const fixture = loadFixture(`malformed/${file}`);
      expect(reasonOf(() => checkFixture(fixture))).toBe(expectedReason);
    });
  }
});

// ===========================================================================
// 4. The comparator is not vacuous: no false positives, and it separates fields.
// ===========================================================================

describe('Task 38 lane E — the comparator is sound both ways', () => {
  it('reports no mismatch for identical values', () => {
    expect(diff(value(RESULT), value(RESULT))).toEqual([]);
  });

  it('reports two mismatches for a two-field drift (does not collapse independent changes)', () => {
    const drifted = { ...value(RESULT), revision: 2, mimeType: 'text/plain' };
    expect(diff(value(RESULT), drifted).map((m: { pointer: string }) => m.pointer).sort()).toEqual(['/mimeType', '/revision']);
  });
});

// ===========================================================================
// 5. RED native-capture blocker — refuses source-only and native-fake claims.
// ===========================================================================

describe('Task 38 lane E — parity refuses a source-only, modelled, or transcript-less native side', () => {
  const ts = checkFixture(RESULT); // a genuine executable-ts capture
  // A well-formed transcript provenance block (64-hex digests, a supported version).
  const NATIVE_TRANSCRIPT = {
    mechanism: 'native-automation-inprocess',
    testName: 'McpAutomationBridge.Task38.NativeProtocolCapture',
    engineVersion: '5.7.4-0+UE5',
    protocolVersion: '2025-11-25',
    capturedAt: '2026-07-23T00:00:00.000Z',
    transcriptRef: 'native-transcript.jsonl',
    transcriptSha256: 'a'.repeat(64),
    sourceHash: 'b'.repeat(64),
    packageHash: 'c'.repeat(64),
  };
  const nativeProtocol = (overrides: Record<string, unknown> = {}): Record<string, unknown> => ({ ...RESULT, captureKind: 'native-protocol', transcript: NATIVE_TRANSCRIPT, ...overrides });

  it('every real on-disk fixture is executable-ts — no native-protocol capture exists (RED state)', () => {
    for (const name of ['executable-ts-result.json', 'executable-ts-error.json', 'executable-ts-revision.json', 'executable-ts-profile.json', 'executable-ts-pointer.json', 'executable-ts-session.json']) {
      expect(loadFixture(name).captureKind).toBe('executable-ts');
    }
    expect(isGenuineNativeCapture(null)).toBe(false);
  });

  it('an absent native side is blocked with the RED requirement naming the executable producer', () => {
    const gate = assertParityReady(ts, null);
    expect(gate.ready).toBe(false);
    expect(gate.ready === false && gate.blocker).toBe(NATIVE_CAPTURE_REQUIREMENT);
    expect(NATIVE_CAPTURE_REQUIREMENT.requiredProducer.name).toBe('McpAutomationBridge.Task38.NativeProtocolCapture');
  });

  it('a hand-authored native-model is refused even when value-identical to TS (no fake PASS)', () => {
    const nativeModel = checkFixture({ ...RESULT, captureKind: 'native-model' });
    expect(nativeModel.executable).toBe(false);
    expect(assertParityReady(ts, nativeModel).ready).toBe(false);
    expect(compareCaptures(ts, nativeModel).ready).toBe(false); // the blocker, NOT mismatches:[]
  });

  it('a native-protocol capture with NO transcript is malformed (a native side must be transcript-backed)', () => {
    expect(reasonOf(() => checkFixture({ ...RESULT, captureKind: 'native-protocol' }))).toBe(REASONS.MALFORMED);
  });

  it('an executable-ts capture that smuggles a transcript block is refused (UNKNOWN_FIELD)', () => {
    expect(reasonOf(() => checkFixture({ ...RESULT, transcript: NATIVE_TRANSCRIPT }))).toBe(REASONS.UNKNOWN_FIELD);
  });

  it('POSITIVE CONTROL (SIMULATED): a transcript-backed native-protocol capture opens the gate and the diff catches drift', () => {
    // Constructed here ONLY to prove the gate is not vacuously RED — it now requires a
    // valid transcript block, which no on-disk fixture supplies. NOT project evidence;
    // real parity stays RED until the integration lane runs the C++ producer.
    const nativeMatch = checkFixture(nativeProtocol());
    expect(isGenuineNativeCapture(nativeMatch)).toBe(true);
    expect(compareCaptures(ts, nativeMatch)).toEqual({ ready: true, mismatches: [], drift: false });

    const nativeDrift = checkFixture(nativeProtocol({ value: driftResultField(value(RESULT) as never, 'revision', 2) }));
    const compared = compareCaptures(ts, nativeDrift);
    expect(compared.ready).toBe(true);
    expect(compared.ready === true && compared.mismatches).toEqual([{ pointer: '/revision', left: 1, right: 2 }]);
  });
});
