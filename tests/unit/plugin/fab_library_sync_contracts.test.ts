/**
 * Source contracts for the Fab library read that syncs an empty table itself.
 *
 * The library used to answer "no rows, run Fab.TEDS.MyFolderIntegration" and leave the caller to run a
 * console command. It now runs that sync when, and only when, the table is empty. What must stay true:
 * the only console text ever run is Fab's own fixed command with a fixed page size, a table that already
 * holds rows is never synced over (the sync removes every row first), and the wait never blocks the
 * game thread.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const library = readFileSync(
  resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabLibrary.cpp',
  ),
  'utf8',
);

/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (text: string): string =>
  text.replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');
const source = code(library);

describe('the Fab library sync', () => {
  it('runs one fixed command with one fixed argument, so no caller text reaches a console', () => {
    expect(source).toContain('const TCHAR* const FabSyncCommand = TEXT("Fab.TEDS.MyFolderIntegration");');
    expect(source).toContain('constexpr int32 FabSyncPageSize = 1000;');
    expect(source.match(/Args\.Add\(/gu)?.length).toBe(1);
    expect(source).toContain('Args.Add(FString::FromInt(FabSyncPageSize));');
    // Fab's own registered command object is asked directly: there is no text for a console to parse.
    expect(source).toContain('IConsoleManager::Get().FindConsoleObject(FabSyncCommand)');
    for (const banned of ['GEditor->Exec', 'GEngine->Exec', 'ProcessUserConsoleInput', 'CommandValidator']) {
      expect(source).not.toContain(banned);
    }
  });

  it('never lets the payload name the command or its arguments', () => {
    const start = source.slice(source.indexOf('bool StartFabSync('), source.indexOf('} // namespace'));
    expect(start).not.toMatch(/Payload|TryGetStringField|Filter/u);
  });

  it('syncs only a table that holds no rows, because the sync removes every row first', () => {
    expect(source).toContain('if (Total > 0 || !StartFabSync(SkippedWhy))');
  });

  it('waits on the core ticker, within a budget, and never on the game thread', () => {
    expect(source).toContain('FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(');
    expect(source).toMatch(/constexpr double FabSyncBudgetSeconds = \d+(\.\d+)?;/u);
    expect(source).toMatch(/Now - Wait->Start < FabSyncBudgetSeconds/u);
    for (const banned of ['FPlatformProcess::Sleep', 'FEvent', 'WaitForCompletion']) {
      expect(source).not.toContain(banned);
    }
  });

  it('times the wait by the wall clock, because a delayed ticker is handed the frame delta', () => {
    expect(source).toContain('double Start = FPlatformTime::Seconds();');
    expect(source).toContain('const double Now = FPlatformTime::Seconds();');
    expect(source).not.toMatch(/\+= Delta/u);
  });

  it('answers when the row count has held still, or when the wait is spent', () => {
    expect(source).toMatch(
      /\(RowsNow == 0 \|\| Now - Wait->QuietSince < FabSyncQuietSeconds\) &&\s*Now - Wait->Start < FabSyncBudgetSeconds/u,
    );
  });

  it('gives the registered query back on every path that answers', () => {
    expect(source.match(/UnregisterQuery\(/gu)?.length).toBe(2);
  });

  it('reports that it synced and for how long, and why it could not when it could not', () => {
    for (const field of ['syncTriggered', 'syncWaitedSeconds', 'syncSkipped']) {
      expect(source).toContain(`TEXT("${field}")`);
    }
  });

  it('is built only where the engine ships the data storage API', () => {
    expect(library).toMatch(/#if MCP_HAS_TEDS && __has_include\("DataStorage\/Features\.h"\)/u);
    expect(library).toContain('This engine version has no editor data storage (TEDS) query API');
  });
});
