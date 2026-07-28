// SECURITY PoC HARNESS — poc-engineer-a (security-research team, Phase 2)
//
// Minimal, SAFE, local-only proofs for candidate findings F9, C1, C2, F3, C3.
// - No live Unreal editor. Every bridge is mocked (isConnected()=true,
//   sendAutomationRequest captures the payload and returns {success:true}).
// - No code execution: the "execute_python" PoC captures the forwarded payload
//   at the bridge boundary; it never runs Python.
// - Toy inputs only. No destructive actions, no filesystem writes, no network.
//
// Run: npx vitest run tests/unit/_poc_security/security-poc.test.ts
//
// Each describe block ends with an explicit VERDICT comment: CONFIRMED /
// PARTIALLY-CONFIRMED (narrower than claimed) / etc.

import { describe, it, expect } from 'vitest';

import { validateArgsSecurity } from '../../../src/tools/handlers/foundation/arguments/handler-argument-validation.js';
import { handleImportAsset } from '../../../src/tools/handlers/asset/asset-basic-actions.js';
import { executeAutomationRequest } from '../../../src/tools/handlers/foundation/dispatch/automation-request-dispatch.js';
import { CommandValidator } from '../../../src/utils/commands/command-validator.js';
import { checkPreDispatchPolicy } from '../../../src/server/gateway/gateway-execute-policy.js';
import type { ExecuteTarget } from '../../../src/server/gateway/gateway-execute-resolve.js';
import {
  IdempotencyKeySchema,
  CorrelationIdSchema,
  RequestIdSchema,
  CatalogRevisionSchema
} from '../../../src/tools/catalog/capabilities/semantic/ids.js';
import {
  buildSuccessReceipt,
  buildErrorReceipt
} from '../../../src/tools/catalog/capabilities/semantic/envelope.js';
import {
  redactText,
  boundArray
} from '../../../src/tools/catalog/capabilities/semantic/receipt-redaction.js';

// A capturing mock of the automation bridge boundary. Nothing crosses to Unreal.
interface Captured { toolName?: string; args?: Record<string, unknown>; }
function mockTools(captured: Captured) {
  return {
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async (toolName: string, args: Record<string, unknown>) => {
        captured.toolName = toolName;
        captured.args = args;
        return { success: true, message: 'mock' };
      }
    }
  } as unknown as Parameters<typeof executeAutomationRequest>[0];
}

// ---------------------------------------------------------------------------
// F9 — import_asset sourcePath path validation
// Claim (hunter): sourcePath is UNVALIDATED -> arbitrary host file read (/etc/passwd).
// Reality: executeAutomationRequest() calls validateArgsSecurity(), and
// 'sourcePath'.includes('path') === true, so the shared allowlist DOES gate it.
// Residual gap: the absolute-path allowlist only fires when value.startsWith('/')
// (handler-argument-validation.ts:102), so Windows drive paths and any
// non-slash-absolute form escape it entirely.
// ---------------------------------------------------------------------------
describe('F9 — import_asset sourcePath validation (prove/disprove)', () => {
  it('DISPROVES "arbitrary /etc/passwd read": POSIX secret paths ARE blocked by shared validation', () => {
    // /etc/ is an explicit blocked pattern.
    expect(() => validateArgsSecurity({ sourcePath: '/etc/passwd', destinationPath: '/Game/x' }))
      .toThrow(/blocked path pattern/i);
    // Non-allowlisted absolute POSIX path -> "unauthorized absolute path".
    expect(() => validateArgsSecurity({ sourcePath: '/home/victim/.ssh/id_rsa', destinationPath: '/Game/x' }))
      .toThrow(/unauthorized absolute path/i);
    // Parent-directory traversal is blocked.
    expect(() => validateArgsSecurity({ sourcePath: '../../etc/passwd', destinationPath: '/Game/x' }))
      .toThrow(/traversal/i);
  });

  it('CONFIRMS residual gap: a Windows drive path bypasses the allowlist (not startsWith "/")', () => {
    // C:\Users\... has no ".." segment, does not match \Windows\ or \Program Files,
    // and does NOT start with "/", so the isAllowedAbsolutePath branch is skipped.
    expect(() => validateArgsSecurity({
      sourcePath: 'C:\\Users\\victim\\Documents\\secret.txt',
      destinationPath: '/Game/Imported/x'
    })).not.toThrow();
  });

  it('CONFIRMS end-to-end: the raw Windows path reaches the bridge unmodified via handleImportAsset', async () => {
    const captured: Captured = {};
    const evilSource = 'C:\\Users\\victim\\Documents\\secret.txt';
    await handleImportAsset({
      args: { sourcePath: evilSource, destinationPath: '/Game/Imported/x' },
      tools: mockTools(captured)
    } as unknown as Parameters<typeof handleImportAsset>[0]);
    // The handler applies NO path normalization/allowlist of its own; the shared
    // dispatch allowlist did not fire for this form, so the raw host path is
    // forwarded verbatim to the bridge.
    expect(captured.toolName).toBe('manage_asset');
    expect(captured.args?.sourcePath).toBe(evilSource);
    expect(captured.args?.subAction).toBe('import');
  });
  // VERDICT: PARTIALLY-CONFIRMED. The "unvalidated -> read /etc/passwd" claim is
  // FALSE (shared allowlist blocks POSIX secret paths + traversal). The real,
  // narrower bug is a Windows-drive / non-slash-absolute allowlist bypass in
  // handler-argument-validation.ts:102 (exposure-gated to LAN). Native read
  // primitive still needs poc-engineer-b's native import confirm.
});

// ---------------------------------------------------------------------------
// C1 — consent/scope declared in records but never enforced pre-dispatch
// ---------------------------------------------------------------------------
describe('C1 — declared consent/scope is not enforced by the pre-dispatch seam', () => {
  it('CONFIRMS: a consent:explicit + requiredScope capability passes checkPreDispatchPolicy ungated', () => {
    const destructiveTarget = {
      record: {
        id: 'manage_asset.delete_asset',
        behavior: { effect: 'write' },
        // Declared policy that a client sees via describe...
        policy: { consent: 'explicit', requiredScope: 'asset:write', dataAccess: 'project' },
        routing: { parentTool: 'manage_asset', dispatchAction: 'delete_asset' }
      },
      legacy: { action: 'delete_asset' }
    } as unknown as ExecuteTarget;

    // ...but the only pre-dispatch seam returns undefined (=allow) regardless of
    // the declared consent/scope. No consent elicitation exists in the path.
    expect(checkPreDispatchPolicy(destructiveTarget, undefined)).toBeUndefined();
    expect(checkPreDispatchPolicy(destructiveTarget, {})).toBeUndefined();
  });
  // VERDICT: CONFIRMED. gateway-execute-policy.ts only checks expectedCatalogRevision;
  // the _target (with policy.consent/requiredScope) is deliberately unused. There
  // is no elicitHighImpactConsent function anywhere in the runtime path.
  // Severity per team threat model: MEDIUM (loopback single-operator; matters for
  // prompt-injected LLM reaching destructive ops with no confirmation).
});

// ---------------------------------------------------------------------------
// C2 — console blocklist asymmetry: execute_python is not command-filtered
// NOTE: on loopback, execute_python is a DOCUMENTED capability (advertised RCE),
// so this is NOT privilege escalation. The security-relevant fact is the
// asymmetry: the console blocklist is not a real containment boundary because
// the same effect is reachable via unfiltered Python.
// ---------------------------------------------------------------------------
describe('C2 — execute_python bypasses the console-command blocklist (asymmetry)', () => {
  const dangerous = 'noop `id`'; // backticks are blocked for console commands

  it('console_command path IS command-validated (blocked)', async () => {
    // Sanity: CommandValidator rejects a backtick command (the generated policy
    // flags it as an unsafe separator before the explicit backtick guard, so the
    // message may be either form).
    expect(() => CommandValidator.validate(dangerous)).toThrow(/[Bb]acktick|Dangerous command blocked/);
    // And the dispatch layer enforces it for the console_command tool.
    await expect(executeAutomationRequest(mockTools({}), 'console_command', { command: dangerous }))
      .rejects.toThrow(/[Bb]acktick|Dangerous command blocked/);
  });

  it('CONFIRMS: execute_python forwards ARBITRARY code with NO command validation', async () => {
    const captured: Captured = {};
    // Harmless payload; the point is that NOTHING inspects it. The same slot
    // accepts os.system(...) identically — it is never passed to CommandValidator.
    const code = 'import os; os.getcwd()';
    const res = await executeAutomationRequest(
      mockTools(captured),
      'system_control',
      { action: 'execute_python', code }
    ) as { success?: boolean };
    expect(res.success).toBe(true);
    expect(captured.toolName).toBe('system_control');
    expect(captured.args?.code).toBe(code); // forwarded verbatim, unfiltered

    // Even a payload carrying the exact token the console path rejects is
    // forwarded untouched when it rides in Python `code`.
    const captured2: Captured = {};
    await executeAutomationRequest(mockTools(captured2), 'system_control', {
      action: 'execute_python',
      code: dangerous
    });
    expect(captured2.args?.code).toBe(dangerous);
  });
  // VERDICT: CONFIRMED asymmetry (validateConsoleCommandPayload only gates
  // console_command/batch_console_commands). Classification: INFO/by-design under
  // loopback threat model — execute_python is advertised RCE, so the console
  // blocklist was never a security boundary. Real note: the blocklist gives a
  // FALSE sense of containment. Native content-filter absence = poc-b's confirm.
});

// ---------------------------------------------------------------------------
// F3 — RequestId / CorrelationId / IdempotencyKey charset is not validated
// ---------------------------------------------------------------------------
describe('F3 — id schemas are length-bounded but NOT charset-validated', () => {
  it('CONFIRMS: newline / log-forging / ANSI payloads parse successfully', () => {
    const logForge = 'req1\n2026-01-01 [ERROR] injected fake log line';
    const ansi = 'id\u001b[31mRED\u001b[0m';
    const nul = 'id\u0000trailer';

    // All accepted — only .min(1)/.max(N), no .regex().
    expect(IdempotencyKeySchema.parse(logForge)).toBe(logForge);
    expect(CorrelationIdSchema.parse(ansi)).toBe(ansi);
    expect(RequestIdSchema.parse(logForge)).toBe(logForge);
    expect(RequestIdSchema.parse(nul)).toBe(nul);
  });

  it('contrast: revision ids ARE charset-guarded (regex) — the id schemas should have been too', () => {
    // Proof the codebase knows how to charset-guard a branded string; the
    // correlation/idempotency/request ids simply omit it.
    expect(() => CatalogRevisionSchema.parse('has space')).toThrow();
    expect(() => CatalogRevisionSchema.parse('id\nnewline')).toThrow();
  });
  // VERDICT: CONFIRMED (TS side). These ids are echoed into the receipt
  // (correlationId/requestId/idempotencyId). Elevation to CWE-117 log injection
  // depends on whether the NATIVE side logs the client RequestId unescaped =
  // poc-engineer-b's native log-sink confirm. LOW pending native.
});

// ---------------------------------------------------------------------------
// C3 — receipt redaction is applied asymmetrically
// ---------------------------------------------------------------------------
describe('C3 — receipt redaction only covers warnings/changes, not data/error', () => {
  const secretPhrase = 'token=SUPERSECRET_ABC123';

  it('CONFIRMS: warnings ARE redacted but data survives UNREDACTED', () => {
    const receipt = buildSuccessReceipt({
      capabilityId: 'system_control.get_project_settings' as never,
      data: { leakedFromHandler: secretPhrase } as never,
      warnings: [secretPhrase]
    });
    // warnings go through boundStrings -> redactText
    expect((receipt as unknown as { warnings: string[] }).warnings[0]).toBe('token=[REDACTED]');
    // data is passed through raw -> the secret survives serialization
    expect((receipt as unknown as { data: { leakedFromHandler: string } }).data.leakedFromHandler).toBe(secretPhrase);
  });

  it('CORRECTION: error.message IS redacted in current code (finding sub-claim disproven)', () => {
    // NOTE: the hunter claim "error.message unredacted" is FALSE for the current
    // envelope.ts. buildErrorReceipt -> redactErrorMessage (envelope.ts:104,133)
    // runs redactText on error.message (and boundStrings on suggestions).
    const receipt = buildErrorReceipt({
      capabilityId: 'system_control.execute_python' as never,
      error: {
        kind: 'execution',
        code: 'EXECUTION_ERROR',
        message: `db password=hunter2 for ${secretPhrase}`,
        retryable: false
      } as never
    });
    const msg = (receipt as unknown as { error: { message: string } }).error.message;
    expect(msg).toBe('db password=[REDACTED] for token=[REDACTED]');
    expect(msg).not.toContain('hunter2');
    expect(msg).not.toContain('SUPERSECRET');
  });

  it('CONFIRMS regex gaps: URL creds and bare JWTs are NOT masked by redactText', () => {
    // No token=/secret:/Bearer keyword -> untouched.
    expect(redactText('https://admin:hunter2@internal.example.com/api')).toBe('https://admin:hunter2@internal.example.com/api');
    const jwt = 'eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.abc123signature';
    expect(redactText(jwt)).toBe(jwt);
    // JSON-key form: the quote between the keyword and ':' defeats SECRET_ASSIGNMENT
    // (it requires keyword + optional-ws + [:=]); "token":"..." is never masked.
    expect(redactText('{"token":"aB3xY9zz"}')).toBe('{"token":"aB3xY9zz"}');
    expect(redactText('db=postgres://u:p4ss@h')).toBe('db=postgres://u:p4ss@h');
  });

  it('CONFIRMS: boundArray does not redact its elements', () => {
    const arr = boundArray([{ note: secretPhrase }]);
    expect((arr[0] as { note: string }).note).toBe(secretPhrase);
  });
  // VERDICT: CONFIRMED. redactText (via boundStrings) is wired only to
  // changes+warnings in envelope.ts; data/error/handles/nextCalls are not
  // secret-scrubbed, and the regex misses URL-creds/bare-JWT. Severity LOW
  // (data returns to the same operator; matters only if a handler echoes a
  // host-side secret into data/error).
});
