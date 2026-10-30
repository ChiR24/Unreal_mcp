// read_editor_ui reads the editor's own Slate UI as an outline. It only reads: nothing in it moves the system cursor,
// activates a window, hit-tests the screen or sends input. use_editor_ui, which clicked and typed into that UI, was
// removed: the capabilities do the editor's work.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const EDITOR = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlEditor');
const FOLDER = join(EDITOR, 'EditorUi');
const read = (file: string): string => readFileSync(file, 'utf8').replace(/\r\n/gu, '\n');

describe('editor UI reading', () => {
  it('never moves the cursor, activates a window, hit-tests the screen or sends input', () => {
    for (const file of readdirSync(FOLDER)) {
      expect(read(join(FOLDER, file)), file).not.toMatch(
        /GetPlatformCursor|SetCursorPos|->SetPosition\(|BringToFront|HACK_ForceToFront|ProcessMouseButtonDownEvent|LocateWindowUnderMouse|FindOrCreateVirtualUser|RoutePointer\w*Event|ProcessKey\w*Event|SetUserFocus/u,
      );
    }
  });

  it('control_editor routes read_editor_ui and nothing that uses the UI', () => {
    const dispatch = read(join(EDITOR, 'McpEditorDispatch.cpp'));
    expect(dispatch).toContain('McpEditorUi::HandleRead(');
    expect(dispatch).not.toMatch(/use_editor_ui|HandleUse/u);
  });
});
