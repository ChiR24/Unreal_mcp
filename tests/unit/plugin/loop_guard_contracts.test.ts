// The loop guard refuses the same failing call after three failures in a row (REPEATED_FAILURE). These contracts keep
// the native guard's passing-moment rules, its place before consent, and the note taken where the receipt is final.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const strip = (text: string): string =>
  text.replace(/\r\n/gu, '\n').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const read = (...segments: readonly string[]): string => strip(readFileSync(join(...segments), 'utf8'));

const guard = read(PRIVATE, 'MCP', 'Execute', 'McpNativeGatewayIdempotency.cpp');

describe('native loop guard', () => {
  it('stops at three failures in a minute', () => {
    expect(guard).toContain('constexpr int32 LoopLimit = 3;');
    expect(guard).toContain('constexpr double LoopWindowSeconds = 60.0;');
  });

  it('lets a moment pass: a timeout, a busy editor, consent, a modal or a refused quota', () => {
    const named = [...(/TSet<FString> Moments = \{([^}]*)\}/u.exec(guard)?.[1] ?? '').matchAll(/TEXT\("([A-Z_]+)"\)/gu)]
      .map((match) => match[1]);
    expect(named).toEqual(expect.arrayContaining(['EDITOR_MODAL_ACTIVE', 'NOT_CONNECTED', 'QUOTA_EXCEEDED', 'CANCELLED']));
    for (const rule of ['EndsWith(TEXT("TIMEOUT")', 'StartsWith(TEXT("INVALID_")', 'EndsWith(TEXT("_BUSY")', 'StartsWith(TEXT("CONSENT_")']) {
      expect(guard).toContain(rule);
    }
  });

  it('refuses before the policy gate, so consent is never asked for a refused call', () => {
    const execute = read(PRIVATE, 'MCP', 'Execute', 'McpNativeTransportGatewayExecute.cpp');
    const refusal = execute.indexOf('McpLoopGuardRefusal(Context.PageOwner)');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal).toBeLessThan(execute.indexOf('McpPrequeueGate::Authorize('));
    expect(execute).toContain('McpValidationError(TEXT("REPEATED_FAILURE"), Repeated)');
  });

  it('notes the outcome from the final receipt at the completion funnel', () => {
    const pending = read(PRIVATE, 'MCP', 'Transport', 'McpNativeTransportPendingRequests.cpp');
    expect(pending).toMatch(/McpSettleIdempotency\(Conn->IdempotencySlot, bReportedSuccess, ReportedResult\);\s*McpLoopGuardNote\(Conn->PageOwner, bReportedSuccess, ReportedErrorCode\);/u);
  });
});
