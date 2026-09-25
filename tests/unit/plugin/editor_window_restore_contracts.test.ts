/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const dir = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/';
const read = (file: string) => readFileSync(resolve(process.cwd(), dir + file), 'utf8');
const windows = read('McpAutomationBridge_ControlEditorScreenshotWindows.cpp');
const dispatch = read('McpAutomationBridge_ControlEditorDispatch.cpp');

describe('editor window restore contracts', () => {
  it('restores a minimized window through its placement without activating it', () => {
    expect(windows).toContain('Placement.showCmd = SW_SHOWNOACTIVATE;');
    expect(windows).toContain('::SetWindowPlacement(Hwnd, &Placement);');
    // SW_RESTORE activates the window and steals focus from the person at the desk.
    expect(windows).not.toMatch(/showCmd = SW_RESTORE|ShowWindow\([^)]*SW_RESTORE/);
  });

  it('routes restore_editor_window to the handler that also lifts the background throttle', () => {
    expect(dispatch).toMatch(/LowerSub == TEXT\("restore_editor_window"\)\)\s*return HandleControlEditorRestoreWindow\(/);
    expect(windows).toContain('Performance->bThrottleCPUWhenNotForeground = false;');
  });
});
