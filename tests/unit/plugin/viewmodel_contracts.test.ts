// The viewmodel (MVVM) actions reach the ModelViewViewModel plugin by reflection only: it is Beta, so a link to it would
// drop the plugin from the Fab build. These read the C++ as text and hold the traps the engine source showed.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const SOURCE = join('plugins', 'EngineWire', 'Source', 'EngineWire');
const FOLDER = join(SOURCE, 'Private', 'Domains', 'WidgetAuthoring', 'ViewModel');
const read = (file: string): string => readFileSync(join(FOLDER, file), 'utf8').replace(/\r\n/gu, '\n');

describe('viewmodel actions', () => {
  it('link nothing of ModelViewViewModel', () => {
    for (const file of readdirSync(FOLDER)) {
      expect(read(file), file).not.toMatch(/#include\s+"MVVM|#include\s+"Types\/MVVM|UMVVM[A-Za-z]+::/u);
    }
    expect(readFileSync(join(SOURCE, 'EngineWire.Build.cs'), 'utf8')).not.toContain('ModelViewViewModel');
  });

  it('find a binding again by its id, since AddBinding hands back a copy', () => {
    const bind = read('McpWidgetViewModelBind.cpp');
    expect(bind).toContain('Call(TEXT("AddBinding")');
    expect(bind).toContain('IndexOf(Bindings, Id)');
  });

  it('remove a binding from the view\'s array, since the editor\'s RemoveBinding matches by address', () => {
    const edit = read('McpWidgetViewModelEdit.cpp');
    expect(edit).not.toContain('TEXT("RemoveBinding")');
    expect(edit).toContain('RemoveValues(Index, 1)');
  });

  it('compile a binding after its undo step closes, and take a rejected one back without a redo step', () => {
    const bind = read('McpWidgetViewModelBind.cpp');
    expect(bind.indexOf('bRecorded = Transaction.IsRecordingUndo();')).toBeLessThan(bind.indexOf('McpCompileBlueprintWithError(Widget, CompileError)'));
    // Only the editor's last transaction, and only when it is this one: never the user's step.
    expect(bind).toMatch(/bRecorded && Top && Trans->GetUndoCount\(\) == 0 && Top->GetTitle\(\)\.ToString\(\) == TEXT\("Bind Viewmodel"\)/u);
    expect(bind).toContain('GEditor->UndoTransaction(false);');
  });
});
