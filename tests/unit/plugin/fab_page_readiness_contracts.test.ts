/**
 * Source contracts for the Fab dispatcher's waiting.
 *
 * The first Fab call after the tab auto-opens answered PAGE_NAVIGATING ("retry in a few seconds"), and a
 * call made while another held the page answered ALREADY_IN_FLIGHT. Both now wait on a ticker and run
 * when they can. What must stay true: the wait never blocks the game thread, it has a budget and ends
 * with a typed failure, and a nudge to a stalled page can never run anything for a request.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const fabPrivate = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridgeFab/Private');
const fab = (file: string): string => readFileSync(resolve(fabPrivate, file), 'utf8');
const code = (text: string): string => text.replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('the Fab dispatcher waits instead of failing early', () => {
  const dispatch = code(fab('McpFabBridgeDispatch.cpp'));

  it('never blocks the game thread while it waits', () => {
    for (const file of ['McpFabBridgeDispatch.cpp', 'McpFabPageReadiness.cpp']) {
      const source = code(fab(file));
      expect(source, `${file} must not sleep or wait synchronously`).not.toMatch(
        /FPlatformProcess::Sleep|ConditionalSleep|->Wait\(|WaitForCompletion|\bSleep\(/u,
      );
    }
    expect(dispatch).toContain('FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickWaiters)');
  });

  it('ends a wait that outlasts its budget with a typed failure naming the wait', () => {
    expect(dispatch).toMatch(/constexpr double PageBudgetSeconds = 15\.0;/u);
    expect(dispatch).toMatch(/FailurePayload\(TEXT\("PAGE_NOT_READY"\)/u);
    expect(dispatch).toMatch(/FailurePayload\(TEXT\("PAGE_BUSY"\)/u);
    expect(dispatch).toContain('DescribePageNotReady(PageBudgetSeconds)');
  });

  it('runs a request only against a page that is on fab.com, and only when the callback is free', () => {
    expect(dispatch).toMatch(/GWaiters\.Num\(\) == 0 && !SlotBusy\(\) && Page\.bUrlIsFab && !Page\.bLoading/u);
    expect(dispatch).toMatch(/Page\.bUrlIsFab && \(!Page\.bLoading \|\| Front\.LoadingFor >= LoadingGraceSeconds\)/u);
  });

  it('serves requests in the order they arrived', () => {
    expect(dispatch).toContain('FWaiter& Front = GWaiters[0];');
    expect(dispatch).toContain('GWaiters.Add(MoveTemp(Waiter));');
  });

  it('nudges a stalled page with the origin guard alone, so no request can run unawaited', () => {
    expect(dispatch).toContain('WrapWithOriginGuard(TEXT("repair"), FString())');
    // The nudge is issued at most once per waiting request.
    expect(dispatch).toMatch(/!Front\.bKicked && Front\.PageWaited >= KickAfterSeconds/u);
  });

  it('keeps refusing when there is no Fab tab at all', () => {
    expect(dispatch).toMatch(/if \(!Page\.bTabFound\)\s*\{\s*OutErrorCode = TEXT\("FAB_NOT_READY"\);/u);
  });
});

describe('the page readiness probe', () => {
  const readiness = code(fab('McpFabPageReadiness.cpp'));

  it('compares the host exactly, so fab.com.example cannot pass for Fab', () => {
    expect(readiness).toContain('TEXT("https://www.fab.com")');
    expect(readiness).toContain('TEXT("https://fab.com")');
    expect(readiness).toMatch(/Url\.Len\(\) == Length \|\| Url\[Length\] == TEXT\('\/'\)/u);
  });

  it('asks the browser for its URL and loading state, and runs no script to find out', () => {
    expect(code(fab('McpFabBrowserSessionBridge.cpp'))).toMatch(/OutUrl = Browser->GetUrl\(\);\s*bOutLoading = Browser->IsLoading\(\);/u);
    expect(readiness).not.toMatch(/ExecuteJavascript|RunScriptWithCallback/u);
  });
});

describe('failures the dispatcher words itself reach the caller', () => {
  it('search, add and details use the dispatcher message when there is one', () => {
    expect(code(fab('McpFabSearchOperation.cpp'))).toContain('TryGetStringField(TEXT("message"), PageMessage)');
    expect(code(fab('McpFabAddOperation.cpp'))).toContain('TryGetStringField(TEXT("message"), PageMessage)');
    expect(code(readFileSync(resolve(process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowFabDetails.cpp'), 'utf8')))
      .toContain('TryGetStringField(TEXT("message"), PageMessage)');
  });
});
