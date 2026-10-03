// A MetaSound plays the graph the frontend registered at its first play, and later plays reuse it,
// so edit_metasound saved every change while the editor kept playing the old graph until a restart
// (2026-10-03: nine gain literals set, every sound measured exactly as before). The audio authoring
// dispatcher now re-registers the edited MetaSound after every graph edit. Needs an editor to run,
// so these assert the wiring.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const DISPATCHER = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains',
  'AudioAuthoring', 'McpAutomationBridge_AudioAuthoringHandlers.cpp');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const source = (): string =>
  readFileSync(DISPATCHER, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('a MetaSound graph edit reaches playback without an editor restart', () => {
  it('re-registers through the MetaSound editor, only where the builder and the editor subsystem exist', () => {
    const code = source();
    expect(code).toContain('#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND && __has_include("MetasoundEditorSubsystem.h")');
    // Only a MetaSound source: the editor's register check()s that the object is a MetaSound.
    expect(code).toContain('FindObject<UMetaSoundSource>(nullptr, *AssetPath)');
    expect(code).toContain('GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>()');
    expect(code).toMatch(/if \(MetaSound && MetaSoundEditor\) \{ MetaSoundEditor->RegisterGraphWithFrontend\(\*MetaSound\); \}/u);
  });

  it('runs after every graph edit handler, once per call (a batch registers once, not per step)', () => {
    const code = source();
    const edits = ['HandleMetaSoundBatchAction', 'HandleMetaSoundNodeActions', 'HandleMetaSoundInterfaceActions', 'HandleMetaSoundGraphEditActions'];
    const register = code.indexOf('ReregisterEditedMetaSound(Params);');
    expect(register).toBeGreaterThan(-1);
    for (const handler of edits) {
      const call = code.indexOf(`${handler}(SubAction, Params, Response)`);
      expect(call, handler).toBeGreaterThan(-1);
      expect(call, handler).toBeLessThan(register);
    }
    expect(code).toMatch(/if \(Edited\)\s*\{\s*ReregisterEditedMetaSound\(Params\);\s*return Edited;\s*\}/u);
  });

  it('leaves reads alone: get_metasound_graph is dispatched after the edits', () => {
    const code = source();
    expect(code.indexOf('HandleMetaSoundGraphReadAction(SubAction, Params, Response)')).toBeGreaterThan(code.indexOf('return Edited;'));
  });
});
