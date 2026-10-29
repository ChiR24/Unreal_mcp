// set_selected_actors changes the editor's actor selection and nothing else. Deselecting BSP surfaces as well would
// modify each level's model and mark the level unsaved, so SelectNone runs with bDeselectBSPSurfs false; and a call
// that leaves the selection as it was opens no undo step.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const FILE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlEditor', 'Selection', 'McpEditorSelection.cpp');
const source = readFileSync(FILE, 'utf8').replace(/\r\n/gu, '\n');

describe('editor selection', () => {
  it('leaves BSP surfaces alone, so the level stays saved', () => {
    expect(source).toContain('GEditor->SelectNone(false, false, false);');
    expect(source).not.toMatch(/SelectNone\([^)]*true/u);
  });

  it('opens its undo step only when the selection changes', () => {
    const guard = source.indexOf('if (bChanges)');
    expect(guard).toBeGreaterThan(-1);
    expect(guard).toBeLessThan(source.indexOf('FMcpScopedEditorTransaction Transaction('));
  });
});
