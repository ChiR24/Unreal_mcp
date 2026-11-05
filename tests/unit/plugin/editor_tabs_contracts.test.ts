// configure_editor open_tab / close_tab: which tab managers they reach (plugin Domains/ControlEditor/McpEditorTabs.cpp).
// Behaviour belongs to the integration cases in tests/integration.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const FILE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlEditor', 'McpEditorTabs.cpp');
/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (): string => readFileSync(FILE, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('editor tabs', () => {
  // The global tab manager holds nomad and floating tabs only. The Output Log the level editor docks lives in the level
  // editor's own manager: a close searching the global one answered "not open", and an open through it brought the
  // tab back as a floating window, which the saved layout then reopened at every start.
  it('close_tab looks in the global manager, then in the level editor\'s', () => {
    expect(code()).toMatch(/TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get\(\)->FindExistingLiveTab\(Id\);\s*const TSharedPtr<FTabManager> LevelTabs = Tab\.IsValid\(\) \? nullptr : LevelEditorTabsForMcp\(\);\s*Tab = LevelTabs\.IsValid\(\) \? LevelTabs->FindExistingLiveTab\(Id\) : Tab;/u);
  });

  it('open_tab invokes through the level editor when it can spawn the tab, else through the global manager', () => {
    const source = code();
    expect(source).toMatch(/const bool bLevel = LevelTabs\.IsValid\(\) && LevelTabs->HasTabSpawner\(TabName\);/u);
    expect(source).toContain('bLevel ? LevelTabs->TryInvokeTab(Id) : FGlobalTabmanager::Get()->TryInvokeTab(Id)');
  });

  it('closes a tab as its own close button does and says when it stays open', () => {
    const source = code();
    expect(source).toContain('const bool bClosed = Tab->RequestCloseTab();');
    expect(source).toContain('TEXT("TAB_NOT_OPEN")');
    expect(source).toContain('TEXT("TAB_CLOSE_REFUSED")');
  });
});
