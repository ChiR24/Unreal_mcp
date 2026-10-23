// The undo block of a reply is read from the editor's transaction buffer, not taken from the handler's word: 19
// handlers recorded undo steps without saying so, a transaction that changed nothing was dropped by the buffer after its
// handler had claimed it, and a compile that cleared the history went unreported. Wiring contracts only: the behaviour
// is pinned in-editor by EngineWire.Core.UndoLedger.ReadsTheStack.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('every request records the transactions it closes, beside the change watch', () => {
  const process = code('Core', 'Requests', 'McpProcessRequest.cpp');

  it('begins with the change watch and ends with it', () => {
    expect(process).toMatch(/McpChangeWatch::Begin\(RequestId\);\s*McpUndoLedger::Begin\(RequestId\);/u);
    expect(process).toMatch(/McpChangeWatch::End\(\);\s*McpUndoLedger::End\(\);/u);
  });

  it('describes the reply in the one success funnel both transports pass through', () => {
    const responses = code('Core', 'Subsystem', 'EngineWireSubsystemResponses.cpp');
    expect(responses).toMatch(/McpChangeWatch::AddWarnings\(RequestId, EffectiveResult\);\s*McpUndoLedger::Describe\(RequestId, EffectiveResult\);/u);
  });

  it('holds a reply sent inside the handler\'s open transaction, after a batch step\'s capture and before anything else', () => {
    const responses = code('Core', 'Subsystem', 'EngineWireSubsystemResponses.cpp');
    expect(responses).toMatch(/TryCapture\(RequestId, bSuccess, Message, Result, ErrorCode\)\) \{ return; \}\s*if \(McpUndoLedger::Hold\(RequestId, /u);
  });

  it('sends the held reply first on every exit, while the error capture, change watch and ledger still run', () => {
    expect(process).toMatch(/ON_SCOPE_EXIT \{\s*McpUndoLedger::Flush\(\);/u);
    const flush = process.indexOf('McpUndoLedger::Flush();');
    expect(flush).toBeLessThan(process.indexOf('EndErrorCapture();'));
    expect(flush).toBeLessThan(process.indexOf('McpUndoLedger::End();'));
  });
});

describe('the ledger judges by the buffer', () => {
  const ledger = code('Core', 'Requests', 'McpUndoLedger.cpp');

  it('decides whether the buffer drops a step while it is still on top', () => {
    expect(ledger).toContain('ETransactionStateEventType::TransactionFinalized');
    expect(ledger).toContain('Top->IsTransient()');
  });

  it('binds per request and unbinds at End, so no delegate outlives a request', () => {
    expect(ledger).toContain('OnTransactionStateChanged().AddStatic(&OnTransactionState)');
    expect(ledger).toContain('OnTransactionStateChanged().Remove(GStateHandle)');
  });

  it('says nothing while a transaction is open or after the handler returned', () => {
    expect(ledger).toMatch(/!GRecording \|\| !Trans \|\| !Result\.IsValid\(\) \|\| RequestId != GRequestId \|\| Trans->IsActive\(\)/u);
  });

  it('keeps a handler refusal and corrects only an unbacked claim', () => {
    expect(ledger).toMatch(/!bClaimed \|\| Kept\.Num\(\) > 0/u);
    for (const reason of ['NOTHING_CHANGED', 'UNDO_HISTORY_CLEARED']) {
      expect(ledger).toContain(`TEXT("${reason}")`);
    }
  });

  it('is pinned in-editor', () => {
    expect(code('Core', 'Requests', 'McpUndoLedgerTests.cpp')).toContain('"EngineWire.Core.UndoLedger.ReadsTheStack"');
  });
});
