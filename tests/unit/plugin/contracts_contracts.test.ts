// execute's options.expect and options.require: assertions the plugin judges after and before a call (McpContracts).
// Wiring contracts only: the parser's known answers are the in-editor Pratt test, the verdicts the integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('action contracts', () => {
  const request = code('MCP', 'Execute', 'McpNativeGatewayExecuteRequest.cpp');
  const execute = code('MCP', 'Execute', 'McpNativeTransportGatewayExecute.cpp');
  const process = code('Core', 'Requests', 'McpProcessRequest.cpp');
  const responses = code('Core', 'Subsystem', 'EngineWireSubsystemResponses.cpp');
  const contracts = code('Domains', 'Debug', 'Contracts', 'McpContracts.cpp');

  it('accepts expect and require as execute options, each assertion parsed before anything is queued', () => {
    expect(request).toMatch(/TEXT\("expect"\),\s*TEXT\("require"\),/u);
    expect(request).toContain('McpValidateIdempotencyKeyOption(Options, OutError) && McpExecuteContracts::Validate(Options, OutError)');
    expect(code('MCP', 'Execute', 'Request', 'McpExecuteContractOptions.cpp')).toContain('McpPratt::Parse(Text, Root, Why)');
  });

  // A replay with other expectations must not be served the first call's receipt.
  it('folds the assertions into the idempotency fingerprint and registers them before dispatch', () => {
    expect(execute).toContain('McpCanonicalFingerprint(Plan.CapabilityId + McpExecuteContracts::FingerprintPart(ExecOptions), Plan.Arguments)');
    expect(execute.indexOf('McpContracts::Register(Context.QueueRequestId')).toBeGreaterThan(execute.indexOf('FMcpIdempotencyLedger::Get().Begin('));
    expect(execute.indexOf('McpContracts::Register(Context.QueueRequestId')).toBeLessThan(execute.indexOf('StreamToolCall('));
  });

  it('judges requirements on the game thread just before the handler, inside the request\'s brackets', () => {
    const admit = process.indexOf('if (!McpContracts::Admit(*this, RequestId, RequestingSocket))');
    expect(admit).toBeGreaterThan(process.indexOf('McpChangeJournal::BeginRequest('));
    expect(admit).toBeLessThan(process.indexOf('AutomationHandlers.Find(Action)'));
    expect(contracts).toContain('TEXT("PRECONDITION_FAILED")');
  });

  // Answering failure would hand back a single-use grant for a destructive call that already ran.
  it('judges expectations after the reply\'s own warnings and never turns a success into a failure', () => {
    expect(responses.indexOf('McpContracts::AddExpectations(RequestId, EffectiveResult);')).toBeGreaterThan(
      responses.indexOf('McpValidateDelta::AddWarnings(RequestId, EffectiveResult);'),
    );
    expect(responses).toMatch(/if \(bSuccess\) \{ McpContracts::AddExpectations\(RequestId, EffectiveResult\); \}\s*else \{ McpContracts::Forget\(RequestId\); \}/u);
    const add = contracts.slice(contracts.indexOf('void AddExpectations('), contracts.indexOf('void Forget('));
    expect(add).not.toMatch(/SendAutomationResponse|bSuccess/u);
  });

  it('carries the verdicts onto the receipt, and only there', () => {
    expect(code('MCP', 'Execute', 'McpNativeReceiptEnrichment.cpp')).toContain('RawResult->TryGetArrayField(TEXT("expectations"), Verdicts)');
    expect(code('MCP', 'Protocol', 'McpJsonRpcReplyCompaction.cpp')).toMatch(/ReceiptOutcomeLists\[\] = \{[^}]*TEXT\("expectations"\)/u);
  });

  it('holds a bounded number of calls\' assertions', () => {
    expect(contracts).toContain('constexpr int32 MaxHeld = 256;');
  });
});
