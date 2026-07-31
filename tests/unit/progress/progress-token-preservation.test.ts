// Task 44 lane A (RED first) — client progress-token preservation.
//
// The acceptance criterion this suite defends is narrow and easy to fake:
// "no internal ID is invented as progress token". A server-side test that only
// asserts "a token was sent" passes happily while the server substitutes its own
// id and silently breaks every client correlating progress to its own request.
// So every case here asserts the EXACT value AND its EXACT JavaScript type, and
// the absent case asserts that nothing is manufactured.
//
// Pure unit scope: the token reader only reads what the client sent. It never
// allocates, never coerces, and never falls back to a request id.

import { describe, expect, it } from 'vitest';
import {
  readProgressToken,
  type ProgressToken,
} from '../../../src/server/mcp-primitives/progress/progress-token.js';

describe('Task 44 — client progress tokens survive verbatim', () => {
  it('preserves a string token as the identical string (not coerced to a number)', () => {
    const token = readProgressToken({ progressToken: '42' });

    expect(token).toBe('42');
    expect(typeof token).toBe('string');
  });

  it('preserves a number token as the identical number (not stringified)', () => {
    const token = readProgressToken({ progressToken: 42 });

    expect(token).toBe(42);
    expect(typeof token).toBe('number');
  });

  it('keeps the string "42" and the number 42 distinguishable', () => {
    // The decisive pair: a server that canonicalizes tokens to strings (or to
    // its own numeric request id) collapses these two into one value and can no
    // longer answer the right client.
    const asString = readProgressToken({ progressToken: '42' });
    const asNumber = readProgressToken({ progressToken: 42 });

    expect(asString).not.toBe(asNumber);
    expect([typeof asString, typeof asNumber]).toEqual(['string', 'number']);
  });

  it('preserves a zero token rather than treating falsy as absent', () => {
    expect(readProgressToken({ progressToken: 0 })).toBe(0);
    expect(readProgressToken({ progressToken: '' })).toBe('');
  });

  it('preserves an opaque client token byte for byte', () => {
    const opaque = 'urn:client:9f1c-«weird»-\u00e9\u00e8/token';

    expect(readProgressToken({ progressToken: opaque })).toBe(opaque);
  });

  it('INVENTS NOTHING when the client sent no token', () => {
    // The core anti-fabrication assertion. Absent must stay absent: the reader
    // has no request id, no counter and no clock to invent one from, and any
    // future refactor that hands it one must not start emitting a token.
    expect(readProgressToken(undefined)).toBeUndefined();
    expect(readProgressToken({})).toBeUndefined();
    expect(readProgressToken({ progressToken: undefined })).toBeUndefined();
  });

  it('rejects non-conforming token types instead of coercing them', () => {
    // MCP declares progressToken as string | number. Coercing anything else
    // would manufacture a token the client never sent.
    const rejected: readonly unknown[] = [
      null,
      true,
      false,
      {},
      [],
      { toString: () => 'nope' },
    ];

    for (const value of rejected) {
      expect(readProgressToken({ progressToken: value })).toBeUndefined();
    }
  });

  it('rejects non-finite numeric tokens that JSON would silently turn into null', () => {
    // NaN / Infinity serialize to null on the wire, so echoing one back is a
    // corrupted token rather than a preserved one.
    expect(readProgressToken({ progressToken: Number.NaN })).toBeUndefined();
    expect(readProgressToken({ progressToken: Number.POSITIVE_INFINITY })).toBeUndefined();
    expect(readProgressToken({ progressToken: Number.NEGATIVE_INFINITY })).toBeUndefined();
  });

  it('ignores a non-object _meta rather than throwing', () => {
    const notObjects: readonly unknown[] = ['meta', 7, true, null, []];

    for (const value of notObjects) {
      expect(readProgressToken(value)).toBeUndefined();
    }
  });

  it('reads only progressToken and never a neighbouring id field', () => {
    // A meta bag carrying other correlation ids must not be mined for a
    // substitute token.
    const token: ProgressToken | undefined = readProgressToken({
      requestId: 'req-1',
      taskId: 'task-1',
      correlationId: 'corr-1',
    });

    expect(token).toBeUndefined();
  });
});
