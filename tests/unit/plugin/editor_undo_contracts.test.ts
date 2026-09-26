/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const source = readFileSync(
  resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorTransactions.cpp'),
  'utf8',
);

describe('control_editor undo/redo', () => {
  it('drives the transactor instead of an Exec string the editor does not recognise', () => {
    // "Undo" / "Redo" are TRANSACTION subcommands, so Exec("Undo") did nothing
    // while the reply said "Undo executed".
    expect(source).not.toMatch(/->Exec\([^)]*TEXT\("(?:Undo|Redo)"\)\)/);
    expect(source).toContain('GEditor->UndoTransaction()');
    expect(source).toContain('GEditor->RedoTransaction()');
  });

  it('refuses when there is nothing to move and names the transaction when there is', () => {
    expect(source).toContain('Trans->CanUndo(&Reason)');
    expect(source).toContain('Trans->CanRedo(&Reason)');
    for (const code of ['NOTHING_TO_UNDO', 'NOTHING_TO_REDO', 'UNDO_FAILED', 'REDO_FAILED']) expect(source).toContain(`TEXT("${code}")`);
    expect(source).toContain('Resp->SetStringField(TEXT("transaction"), Title);');
  });
});
