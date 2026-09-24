// open_editor_tab advertised "FabTab" for Fab, but Fab registers no tab spawner
// (it builds a fresh "Fab%d" tab per open), so the documented call always failed
// with "the owning plugin may be disabled". Its Window-menu entry is a plain
// FUIAction that TryExecuteToolUIAction ignores, so reflection is the route.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

import { loadCanonicalRegistry } from './gateway/native-discovery-model.js';

const FILE = resolve(process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/McpAutomationBridge_ControlEditorTabs.cpp');
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

describe('open_editor_tab and Fab', () => {
  it('opens Fab through its browser API by reflection, since Fab registers no tab spawner', () => {
    expect(existsSync(FILE)).toBe(true);
    const s = code(readFileSync(FILE, 'utf8'));
    expect(s).toMatch(/FindObject<UClass>\(nullptr, TEXT\("\/Script\/Fab\.FabBrowserApi"\)\)/);
    expect(s).toMatch(/FindFunctionByName\(TEXT\("OpenInNewTab"\)\)/);
    expect(s).toMatch(/InitializeValue_InContainer\(Params\)/);
    expect(s).toMatch(/TabId\.StartsWith\(TEXT\("Fab"\)\)/);
    expect(s).not.toMatch(/TryExecuteToolUIAction/);
    expect(s).not.toMatch(/'FabTab'/);
  });

  it('no longer documents the dead "FabTab" id', () => {
    const text = JSON.stringify(loadCanonicalRegistry().records.filter((r) => r.id.startsWith('control_editor.')));
    expect(text).not.toContain('FabTab');
    expect(text).toContain('\\"Fab\\"');
  });
});
