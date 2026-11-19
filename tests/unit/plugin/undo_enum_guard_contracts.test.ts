// From UE 5.7 applying an undo step that holds an enum's saved state crashes the editor (restoring the enum asserts in
// UEnum::FNameData::Initialize); the enum editor records one for each of its edits. control_editor undo and redo refuse
// such a step, and a restore point's rollback refuses before it undoes anything, with UNDO_WOULD_CRASH. A step an edit
// through this server made records the enum as a change undo can apply (Safety/McpSafeOperationsEnumUndo.h) and passes;
// tests/mcp-tools/core/enum.test.mjs undoes and redoes those.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const EDITOR = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlEditor');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (...parts: string[]): string =>
  readFileSync(join(EDITOR, ...parts), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('an undo step that holds an enum is refused from UE 5.7', () => {
  it('the check reads the step\'s objects, only where the engine asserts', () => {
    const support = read('McpEditorSupport.h');
    const body = support.slice(support.indexOf('inline bool McpStepRestoresEnum(const FTransaction *Step)'));
    expect(body).toMatch(/^inline bool McpStepRestoresEnum[\s\S]*?#if ENGINE_MAJOR_VERSION > 5 \|\| \(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7\)/u);
    expect(body).toContain('Step->GetTransactionObjects(Objects);');
    expect(body).toContain('Object->IsA<UEnum>()');
  });

  it('a step whose enums this server recorded as a change passes', () => {
    const support = read('McpEditorSupport.h');
    const body = support.slice(support.indexOf('inline bool McpStepRestoresEnum(const FTransaction *Step)'));
    expect(body).toContain('if (Step && !McpSafeOperations::McpEnumChangeSteps().Contains(Step->GetContext().TransactionId))');
    expect(support).toContain('#include "Safety/McpSafeOperationsEnumUndo.h"');
  });

  it('undo and redo refuse the step they would apply, before applying anything', () => {
    const source = read('McpEditorTransactions.cpp');
    const check = source.indexOf('McpStepRestoresEnum(Trans->GetTransaction(Trans->GetQueueLength() - Trans->GetUndoCount() - (bUndo ? 1 : 0)))');
    expect(check).toBeGreaterThan(0);
    expect(source.indexOf('TEXT("UNDO_WOULD_CRASH")')).toBeGreaterThan(check);
    expect(source.indexOf('GEditor->UndoTransaction()')).toBeGreaterThan(source.indexOf('TEXT("UNDO_WOULD_CRASH")'));
  });

  it('a rollback refuses before its first undo', () => {
    const source = read('Checkpoint', 'McpEditorCheckpoint.cpp');
    const refusal = source.indexOf('Steps.FindByPredicate(&McpStepRestoresEnum)');
    expect(refusal).toBeGreaterThan(0);
    expect(source.indexOf('TEXT("UNDO_WOULD_CRASH")', refusal)).toBeGreaterThan(refusal);
    expect(source.indexOf('GEditor->UndoTransaction()')).toBeGreaterThan(refusal);
  });
});
