/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const read = (path: string) => readFileSync(resolve(process.cwd(), path), 'utf8');
const dir = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor/';
const input = read(`${dir}McpAutomationBridge_ControlEditorInput.cpp`);
const routing = read(`${dir}McpAutomationBridge_ControlEditorInputRouting.cpp`);
const tsHandler = read('src/tools/handlers/editor/editor-input-actions.ts');

describe('simulate_input raw key hold contracts', () => {
  it('releases a held raw key after holdSeconds instead of leaving it down', () => {
    expect(input).toContain('ScheduleKeyReleaseForMcp(Key, HoldSeconds, Payload);');
    expect(input).toContain('SimulateEditorInputForMcp(TEXT("key_up"), Key, Payload');
    // A key_up cancels the pending release; stopping the subsystem clears them all.
    expect(input).toContain('CancelKeyReleaseForMcp(Key);');
    expect(input).toMatch(/void StopAllEnhancedInputHoldsForMcp\(\) \{\s+for \(const TPair<FString, FTSTicker::FDelegateHandle> &Release : McpKeyReleases\(\)\)/);
  });

  it('accepts key_tap (aliases key, tap) on both transports', () => {
    expect(input).toContain('const bool bTap = InputType == TEXT("key_tap");');
    expect(routing).toMatch(/InputType == TEXT\("key"\) \|\| InputType == TEXT\("tap"\)\) \{\s+return TEXT\("key_tap"\);/);
    expect(tsHandler).toContain("key: 'key_tap',");
    expect(tsHandler).toContain("tap: 'key_tap',");
    expect(tsHandler).toContain("new Set(['key_down', 'key_up', 'key_tap',");
  });
});
