// control_editor restore_point rolls the editor back undoing only what calls through this server changed. Wiring
// contracts only: the behaviour is pinned in-editor by EngineWire.ControlEditor.RestorePoint.RollsBackOnlyMcpSteps.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('control_editor restore_point', () => {
  const checkpoint = code('Domains', 'ControlEditor', 'Checkpoint', 'McpEditorCheckpoint.cpp');

  it('is dispatched beside undo and redo', () => {
    expect(code('Domains', 'ControlEditor', 'McpEditorDispatch.cpp'))
      .toMatch(/LowerSub == TEXT\("restore_point"\)\)\s*return McpEditorCheckpoint::Handle\(this, RequestId, Payload, RequestingSocket\);/u);
  });

  it('knows which steps are this server\'s from the undo ledger', () => {
    expect(checkpoint).toContain('McpUndoLedger::IsMcpStep(');
    expect(code('Core', 'Requests', 'McpUndoLedger.cpp')).toMatch(/GOwnedSet\.Add\(Step\.Id\)/u);
  });

  it('refuses before undoing anything', () => {
    const start = checkpoint.indexOf('void Rollback(');
    const rollback = checkpoint.slice(start, checkpoint.indexOf('\n}\n', start));
    const undo = rollback.indexOf('GEditor->UndoTransaction()');
    expect(start).toBeGreaterThan(-1);
    expect(undo).toBeGreaterThan(-1);
    for (const refusal of ['PIE_RUNNING', 'UNDO_HISTORY_CLEARED', 'USER_STEP_SINCE_RESTORE_POINT']) {
      const at = rollback.indexOf(`TEXT("${refusal}")`);
      expect(at, refusal).toBeGreaterThan(-1);
      expect(at, refusal).toBeLessThan(undo);
    }
  });

  it('is pinned in-editor', () => {
    expect(code('Domains', 'ControlEditor', 'Checkpoint', 'McpEditorCheckpointTests.cpp'))
      .toContain('"EngineWire.ControlEditor.RestorePoint.RollsBackOnlyMcpSteps"');
  });
});
