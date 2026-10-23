// The loop guard refuses the same failing call after three failures in a row, on both doors. The TS rule is unit
// tested in gateway-loop-guard.test.ts; these contracts keep the native mirror on the same limit, window, refusal code
// and passing-moment codes, checked before consent and noted where the receipt is final.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { LOOP_LIMIT, LOOP_WINDOW_MS, REPEATED_FAILURE_CODE } from '../../../src/server/gateway/gateway-loop-guard.js';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (text: string): string =>
  text.replace(/\r\n/gu, '\n').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(...segments), 'utf8'));

const guard = read(PRIVATE, 'MCP', 'Execute', 'McpNativeGatewayIdempotency.cpp');
const tsGuard = read('src', 'server', 'gateway', 'gateway-loop-guard.ts');

describe('native loop guard mirrors gateway-loop-guard.ts', () => {
  it('uses the same limit and window', () => {
    expect(guard).toContain(`constexpr int32 LoopLimit = ${String(LOOP_LIMIT)};`);
    expect(guard).toContain(`constexpr double LoopWindowSeconds = ${(LOOP_WINDOW_MS / 1000).toFixed(1)};`);
  });

  it('lets the same codes pass as moments', () => {
    const tsNamed = /\(\?:([A-Z_|]+)\)\$/u.exec(tsGuard)?.[1]?.split('|') ?? [];
    const cppNamed = [...(/TSet<FString> Moments = \{([^}]*)\}/u.exec(guard)?.[1] ?? '').matchAll(/TEXT\("([A-Z_]+)"\)/gu)]
      .map((match) => match[1]);
    expect(tsNamed.length).toBeGreaterThan(0);
    expect([...cppNamed].sort()).toEqual([...tsNamed].sort());
    for (const rule of ['EndsWith(TEXT("TIMEOUT")', 'StartsWith(TEXT("INVALID_")', 'EndsWith(TEXT("_BUSY")', 'StartsWith(TEXT("CONSENT_")']) {
      expect(guard).toContain(rule);
    }
  });

  it('refuses with the same code before the policy gate, so consent is never asked for a refused call', () => {
    const execute = read(PRIVATE, 'MCP', 'Execute', 'McpNativeTransportGatewayExecute.cpp');
    const refusal = execute.indexOf('McpLoopGuardRefusal(Context.PageOwner)');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal).toBeLessThan(execute.indexOf('McpPrequeueGate::Authorize('));
    expect(execute).toContain(`McpValidationError(TEXT("${REPEATED_FAILURE_CODE}"), Repeated)`);
  });

  it('notes the outcome from the final receipt at the completion funnel', () => {
    const pending = read(PRIVATE, 'MCP', 'Transport', 'McpNativeTransportPendingRequests.cpp');
    expect(pending).toMatch(/McpSettleIdempotency\(Conn->IdempotencySlot, bReportedSuccess, ReportedResult\);\s*McpLoopGuardNote\(Conn->PageOwner, bReportedSuccess, ReportedErrorCode\);/u);
  });
});

describe('TS loop guard placement', () => {
  it('checks before consent and notes inside the dispatch', () => {
    const execute = read('src', 'server', 'gateway', 'gateway-execute.ts');
    const check = execute.indexOf('loopStreak(principal, fingerprint)');
    expect(check).toBeGreaterThan(-1);
    expect(check).toBeLessThan(execute.indexOf('checkConsentAuthorization('));
    expect(execute).toMatch(/const receipt = await dispatchAndValidate\([\s\S]*?\);\s*noteOutcome\(principal, fingerprint, receipt\);/u);
  });
});
