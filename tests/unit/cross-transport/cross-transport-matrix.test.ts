import { describe, expect, it } from 'vitest';

import {
  HarnessRejection,
  MATRIX_DIMENSIONS,
  REASONS,
} from '../mcp-primitives/parity-harness-schema.mjs';
import { checkFixture, compareCaptures, diff } from '../mcp-primitives/parity-harness.mjs';
import { staleRevision, toSourceText } from '../mcp-primitives/parity-harness-drift.mjs';
import { generateMatrix, loadRecords } from './matrix-dimensions.mjs';
import { asCapture, projectCell, unwrap } from './matrix-projection.mjs';

const TRANSCRIPT = {
  mechanism: 'native-http-sse',
  testName: 'Task46.CrossTransportMatrix',
  engineVersion: '5.7.4',
  protocolVersion: '2025-06-18',
  capturedAt: '2026-07-27T12:00:00.000Z',
  transcriptRef: 'cross-transport/native-transcript.jsonl',
  transcriptSha256: 'a'.repeat(64),
  sourceHash: 'b'.repeat(64),
  packageHash: 'c'.repeat(64),
};

const RECEIPT_CASE = {
  id: 'receipt/read-success',
  dimension: 'receipt',
  scenario: 'a successful read returns a canonical receipt',
  extractor: 'receipt',
};

const stdioShape = {
  isError: false,
  structuredContent: { data: { success: true, message: 'ok' } },
};

const nativeShape = {
  jsonrpc: '2.0',
  id: 7,
  result: {
    isError: false,
    structuredContent: { data: { success: true, message: 'ok' } },
    _meta: { 'mcp-session-id': 'e5f7c0aa-1111-2222-3333-444455556666' },
  },
};

const tsCapture = (cell: unknown) =>
  checkFixture(asCapture('receipt/read-success', 'executable-ts', cell, 'stdio driver'));
const nativeCapture = (cell: unknown) =>
  checkFixture(asCapture('receipt/read-success', 'native-protocol', cell, 'native /mcp driver', TRANSCRIPT));

describe('task 46 - generated cross-transport matrix', () => {
  it('generates a case for every one of the 15 gated dimensions from the canonical contract', () => {
    const records = loadRecords();
    const matrix = generateMatrix(records);

    expect(new Set(matrix.map((entry) => entry.dimension))).toEqual(new Set(MATRIX_DIMENSIONS));
    expect(matrix).toHaveLength(MATRIX_DIMENSIONS.length);
    // Contract-derived, not hand-typed: every capability a case names must be a
    // real record id. A typo'd or retired capability fails here rather than
    // becoming a case that quietly never runs.
    const ids = new Set(records.map((record) => record.id));
    for (const entry of matrix.filter((candidate) => candidate.capabilityId !== null)) {
      expect(ids.has(entry.capabilityId as string), `${entry.id} names ${entry.capabilityId}`).toBe(true);
    }
  });

  it('regenerates the identical matrix from the identical contract', () => {
    const first = generateMatrix(loadRecords());
    const second = generateMatrix(loadRecords());

    expect(diff(first, second)).toEqual([]);
  });
});

describe('task 46 - the projection compares semantics, not framing', () => {
  it('projects a stdio result and a native JSON-RPC envelope to the identical cell', () => {
    const fromStdio = projectCell(RECEIPT_CASE, { raw: stdioShape });
    const fromNative = projectCell(RECEIPT_CASE, { raw: nativeShape });

    expect(diff(fromStdio, fromNative)).toEqual([]);
    expect(compareCaptures(tsCapture(fromStdio), nativeCapture(fromNative)))
      .toMatchObject({ ready: true, drift: false, mismatches: [] });
  });

  it('projects an SSE-framed native response to the same cell as a bare one', () => {
    const sse = `event: message\ndata: ${JSON.stringify(nativeShape)}\n\n`;

    expect(diff(projectCell(RECEIPT_CASE, { raw: sse }), projectCell(RECEIPT_CASE, { raw: nativeShape }))).toEqual([]);
  });

  it('treats a JSON-RPC error and an isError result as the same refusal outcome', () => {
    const errorCase = { id: 'error/unknown-capability', dimension: 'error', scenario: 'unknown capability', extractor: 'error' };
    const asIsError = projectCell(errorCase, {
      raw: { isError: true, structuredContent: { data: { code: 'CAPABILITY_NOT_FOUND' } } },
      extra: { hasSuggestions: true, hasNextCall: true },
    });
    const asRpcError = projectCell(errorCase, {
      raw: { jsonrpc: '2.0', id: 3, error: { code: -32602, message: 'x', data: { code: 'CAPABILITY_NOT_FOUND' } } },
      extra: { hasSuggestions: true, hasNextCall: true },
    });

    expect(asIsError.outcome).toBe('refusal');
    expect(asRpcError.outcome).toBe('refusal');
    expect(asRpcError.code).toBe('CAPABILITY_NOT_FOUND');
    // The numeric JSON-RPC code is framing and must never become the cell code.
    expect(asIsError.code).toBe('CAPABILITY_NOT_FOUND');
  });

  it('unwraps all three transport shapes to the same result object', () => {
    expect(unwrap(stdioShape).result).toEqual(stdioShape);
    expect(unwrap(nativeShape).result).toMatchObject({ isError: false });
    expect(unwrap(`data: ${JSON.stringify(nativeShape)}`).result).toMatchObject({ isError: false });
  });
});

describe('task 46 - the gate can fail (injected-fault detection)', () => {
  it('detects an injected UNAUTHORIZED ACTION that one transport allowed', () => {
    // The defect shape this plan produced five times: true in the type, green in
    // a test, wrong on the wire. Here the native side ALLOWS a consent-gated
    // capability the stdio side refuses. Both cells are individually valid; only
    // the cross-transport comparison exposes it.
    const consentCase = { id: 'consent/missing-consent-is-refused', dimension: 'consent', scenario: 'consent gate', extractor: 'error' };
    const refused = projectCell(consentCase, {
      raw: { isError: true, structuredContent: { data: { code: 'CONSENT_REQUIRED' } } },
      extra: { hasSuggestions: true, hasNextCall: true },
    });
    const allowed = projectCell(consentCase, {
      raw: { isError: false, structuredContent: { data: { success: true } } },
      extra: { hasSuggestions: false, hasNextCall: false },
    });

    const verdict = compareCaptures(
      checkFixture(asCapture('consent', 'executable-ts', refused, 'stdio')),
      checkFixture(asCapture('consent', 'native-protocol', allowed, 'native', TRANSCRIPT)),
    );
    expect(verdict).toMatchObject({ ready: true, drift: true });
    expect((verdict as { mismatches: { pointer: string }[] }).mismatches.map((entry) => entry.pointer))
      .toContain('/outcome');
  });

  it('detects an injected DUPLICATE MUTATION', () => {
    const idempotencyCase = { id: 'idempotency/replay', dimension: 'idempotency', scenario: 'replay', extractor: 'idempotency' };
    const honest = projectCell(idempotencyCase, { raw: stdioShape, extra: { replayed: true, receiptsIdentical: true, mutationsObserved: 1 } });
    const duplicated = projectCell(idempotencyCase, { raw: stdioShape, extra: { replayed: false, receiptsIdentical: true, mutationsObserved: 2 } });

    const mismatches = diff(honest, duplicated);
    expect(mismatches.length).toBeGreaterThan(0);
    expect(JSON.stringify(mismatches)).toContain('mutationsObserved=2');
  });

  it('detects an injected STALE STATE (a revision that went backwards)', () => {
    const value = { uri: 'unreal://editor/state', revisions: [1, 2, 3] };
    const wrap = (revision: unknown) => ({ id: 'rev', captureKind: 'executable-ts', domain: 'revision', match: 'exact', provenance: 'stdio', value: revision });
    expect(checkFixture(wrap(value)).domain).toBe('revision');

    const rewound = wrap(staleRevision(value));
    expect(() => checkFixture(rewound)).toThrowError(HarnessRejection);
    try {
      checkFixture(rewound);
    } catch (error) {
      expect((error as HarnessRejection).reason).toBe(REASONS.STALE_REVISION);
    }
  });

  it('detects an injected RACE reported through the queue dimension', () => {
    // The native lane oracle catches a race inside the editor; this is the same
    // fault as a client would observe it - one admitted request, two terminal
    // results, or an admitted request that never terminated.
    const queueCase = { id: 'queue/concurrent', dimension: 'queue', scenario: 'concurrency', extractor: 'queue' };
    const clean = projectCell(queueCase, { raw: stdioShape, extra: { completed: 8, duplicates: 0, lost: 0 } });
    const raced = projectCell(queueCase, { raw: stdioShape, extra: { completed: 8, duplicates: 1, lost: 1 } });

    expect(diff(clean, raced).map((entry) => entry.pointer)).toContain('/facts/1');
  });

  it('refuses a MODELLED or SOURCE-TEXT native side instead of passing', () => {
    const cell = projectCell(RECEIPT_CASE, { raw: stdioShape });
    const modelled = { id: 'receipt', captureKind: 'native-model', domain: 'matrix', match: 'exact', provenance: 'hand-authored oracle', value: cell };

    const verdict = compareCaptures(tsCapture(cell), checkFixture(modelled));
    expect(verdict.ready).toBe(false);

    expect(() => checkFixture(toSourceText(modelled))).toThrowError(HarnessRejection);
  });

  it('refuses a VACUOUS cell whose facts were stripped away', () => {
    expect(() => projectCell({ ...RECEIPT_CASE, extractor: 'queue' }, { raw: stdioShape, extra: {} })).not.toThrow();

    const empty = { id: 'v', captureKind: 'executable-ts', domain: 'matrix', match: 'exact', provenance: 'stdio', value: { dimension: 'receipt', scenario: 's', outcome: 'success', code: 'NONE', facts: [] } };
    try {
      checkFixture(empty);
      throw new Error('an empty cell must not be accepted');
    } catch (error) {
      expect((error as HarnessRejection).reason).toBe(REASONS.VACUOUS_CELL);
    }
  });

  it('refuses a cell whose facts still carry transport framing', () => {
    const leaky = {
      id: 'leak/framing',
      dimension: 'receipt',
      scenario: 'framing leak',
      extractor: 'policy',
    };

    expect(() => projectCell(leaky, { raw: stdioShape, extra: { requiredScope: '127.0.0.1:3000', consent: 'none', effect: 'read' } }))
      .toThrowError(HarnessRejection);
    expect(() => projectCell(leaky, { raw: stdioShape, extra: { requiredScope: 'read', consent: '2026-07-27T12:00:00Z', effect: 'read' } }))
      .toThrowError(HarnessRejection);
  });

  it('refuses a matrix cell naming a dimension outside the closed 15', () => {
    const rogue = { id: 'r', captureKind: 'executable-ts', domain: 'matrix', match: 'exact', provenance: 'stdio', value: { dimension: 'telemetry', scenario: 's', outcome: 'success', code: 'NONE', facts: ['a=b'] } };

    expect(() => checkFixture(rogue)).toThrowError(HarnessRejection);
  });

  it('refuses a success cell that smuggles a refusal code, and the reverse', () => {
    const cell = (outcome: string, code: string) => ({ id: 'c', captureKind: 'executable-ts', domain: 'matrix', match: 'exact', provenance: 'stdio', value: { dimension: 'receipt', scenario: 's', outcome, code, facts: ['a=b'] } });

    expect(() => checkFixture(cell('success', 'CONSENT_REQUIRED'))).toThrowError(HarnessRejection);
    expect(() => checkFixture(cell('refusal', 'NONE'))).toThrowError(HarnessRejection);
    expect(() => checkFixture(cell('refusal', '-32602'))).toThrowError(HarnessRejection);
  });
});
