// Source contracts for the two Foundation security primitives that exist as a
// TypeScript/C++ MIRRORED PAIR. Neither can be executed here (the native half
// needs UE), so these read the source text and pin the properties that must hold
// on BOTH surfaces or the pair silently diverges.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const read = (relativePath: string): string =>
  readFileSync(resolve(process.cwd(), relativePath), 'utf8');

const FOUNDATION = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation';
const NATIVE_TESTS = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Tests';

// Vectors computed with an external `sha256sum` over the canonical preimage
// bytes, so they are an oracle independent of both implementations:
//   sha256("15:scoped:qawriter18:asset.import_asset15:client-key-0001")
//   sha256("3:p c1:k1:x")   <- principal "p c", capability "k",   key "x"
//   sha256("1:p3:c k1:x")   <- principal "p",   capability "c k", key "x"
// The last two are the boundary-shift pair: under a single-space separator both
// preimages were "p c k" and the two distinct scopes shared ONE ledger slot.
const CROSS_SURFACE_VECTORS = [
  '14190f6efc7cd729e4658414cfa430c59f2c3b8e0e37f3d8ad3571c61dcfa1e5',
  'f270ca82affdbdc185837d23aabcdb5f1e9f8ef00dcd10711b6508a9cd5b1ad6',
  '06bd91f8cd81eb5efe434332021c70925320c056150ee133944238aa994d4986',
] as const;

describe('idempotency slot encoding is identical across the TS and native mirrors', () => {
  it('digests a length-prefixed preimage natively, not a delimiter join', () => {
    const native = read(`${FOUNDATION}/McpIdempotencyLedger.cpp`);

    expect(native).toContain('AppendLengthPrefixedField');
    expect(native).toContain('TEXT("%d:")');
    // The old ambiguous form: three fields glued with one separator token.
    expect(native).not.toContain('FieldSeparator');
  });

  it('length-prefixes each of the three fields in the canonical order', () => {
    const native = read(`${FOUNDATION}/McpIdempotencyLedger.cpp`);
    const appends = [...native.matchAll(/AppendLengthPrefixedField\(\s*Preimage,\s*(\w+)\)/g)].map(
      (match) => match[1],
    );

    expect(appends).toEqual(['PrincipalIdentity', 'CapabilityId', 'IdempotencyKey']);
  });

  it('encodes the length over UTF-8 BYTES on both surfaces, not TCHAR/UTF-16 units', () => {
    const native = read(`${FOUNDATION}/McpIdempotencyLedger.cpp`);
    const ts = read('src/server/gateway/idempotency-ledger.ts');

    expect(native).toContain('FTCHARToUTF8');
    expect(ts).toContain("Buffer.from(field, 'utf8')");
  });

  it('pins the SAME independently computed digest vectors on both surfaces', () => {
    const tsTest = read('src/server/gateway/idempotency-ledger.test.ts');
    const nativeTest = read(`${NATIVE_TESTS}/McpIdempotencyLedgerTests.cpp`);

    for (const vector of CROSS_SURFACE_VECTORS) {
      expect(tsTest, `the TS mirror does not pin ${vector}`).toContain(vector);
      expect(nativeTest, `the native mirror does not pin ${vector}`).toContain(vector);
    }
  });

  it('keeps eviction COMPLETED-only on both surfaces', () => {
    const native = read(`${FOUNDATION}/McpIdempotencyLedger.cpp`);
    const ts = read('src/server/gateway/idempotency-ledger.ts');

    expect(native).toContain('EvictCompletedOverCap');
    expect(ts).toContain('evictCompletedOverCap');
  });
});

describe('constant-time token compare cannot lose the length signal', () => {
  const helper = (): string => read(`${FOUNDATION}/McpSecureTokenCompare.h`);

  it('does not truncate the length difference into a uint8 accumulator', () => {
    // LenA ^ LenB is a nonzero multiple of 256 for e.g. 1 vs 257, so narrowing
    // it to uint8 yields 0 and the length mismatch silently disappears.
    expect(helper()).not.toContain('uint8 Diff = static_cast<uint8>(LenA ^ LenB)');
  });

  it('accumulates the length difference at full width', () => {
    expect(helper()).toContain('uint32 Diff = static_cast<uint32>(LenA ^ LenB)');
  });

  it('still has no data-dependent early exit', () => {
    const source = helper();

    expect(source).toContain('Diff |=');
    expect(source).not.toMatch(/if\s*\(\s*LenA\s*!=\s*LenB\s*\)\s*\{?\s*return/);
    expect(source.slice(source.indexOf('for (int32 i'))).not.toContain('break');
  });
});
