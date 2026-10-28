// read_editor_ui and use_editor_ui drive the editor's own Slate UI. Standing rule: automation never moves the system
// cursor and never activates a window. Input goes as a Slate virtual user down the widget's own path, so nothing here
// may touch the platform cursor or hit-test the screen.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const FOLDER = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlEditor', 'EditorUi');
const read = (file: string): string => readFileSync(join(FOLDER, file), 'utf8').replace(/\r\n/gu, '\n');

describe('editor UI automation', () => {
  it('never moves the cursor, activates a window or hit-tests the screen', () => {
    for (const file of readdirSync(FOLDER)) {
      const source = read(file);
      expect(source, file).not.toMatch(/GetPlatformCursor|SetCursorPos|->SetPosition\(|BringToFront|HACK_ForceToFront|ProcessMouseButtonDownEvent|LocateWindowUnderMouse/u);
    }
  });

  it('delivers input as a virtual user down the widget\'s own path', () => {
    const use = read('McpEditorUiUse.cpp');
    expect(use).toContain('FindOrCreateVirtualUser(UiVirtualUser)');
    expect(use).toContain('FindPathToWidget(Widget.ToSharedRef(), Path)');
    // An arranged path has no pointer slot per widget, and routing reads one for each: the editor asserted (index out
    // of bounds) on the first click before the path was rebuilt as a hit-test path.
    expect(use).toContain('Widgets.Add(FWidgetAndPointer(Arranged.Widgets[Index]));');
    expect(use).toContain('const FWidgetPath Path = UiPointerPath(Arranged);');
    for (const route of ['RoutePointerMoveEvent(Path,', 'RoutePointerDownEvent(Path,', 'RoutePointerUpEvent(Path,']) {
      expect(use).toContain(route);
    }
  });
});
