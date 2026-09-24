// Source contracts for the PIE clock controls: set_game_speed must not write
// into the edited level, and set_fixed_delta_time must drive the engine's real
// fixed step (there is no r.FixedDeltaTime console variable).
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const DIR = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlEditor');
function code(file: string): string {
  return readFileSync(resolve(DIR, file), 'utf8').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}
function body(file: string, fn: string): string {
  const s = code(file);
  const start = s.indexOf(`::${fn}(`);
  expect(start, `${fn} not found`).toBeGreaterThan(-1);
  const end = s.indexOf('\nbool UMcpAutomationBridgeSubsystem::', start + 1);
  return s.slice(start, end === -1 ? undefined : end);
}

describe('game clock controls', () => {
  it('set_game_speed acts on the PIE world only, never the editor world', () => {
    const s = body('McpAutomationBridge_ControlEditorPlay.cpp', 'HandleControlEditorSetGameSpeed');
    expect(s).toContain('GEditor->PlayWorld.Get()');
    expect(s).not.toContain('GetEditorWorldContext()');
    expect(s).toContain('TEXT("NO_ACTIVE_SESSION")');
  });

  it('set_fixed_delta_time drives FApp, refuses without PIE, and switches off when PIE ends', () => {
    const s = body('McpAutomationBridge_ControlEditorPreferences.cpp', 'HandleControlEditorSetFixedDeltaTime');
    expect(s).not.toContain('r.FixedDeltaTime');
    expect(s).toContain('FApp::SetFixedDeltaTime(DeltaTime)');
    expect(s).toContain('FApp::SetUseFixedTimeStep(bFixed)');
    expect(s).toMatch(/!GEditor->PlayWorld[\s\S]*NO_ACTIVE_SESSION/);
    expect(s).toMatch(/FEditorDelegates::EndPIE\.AddLambda\([\s\S]*FApp::SetUseFixedTimeStep\(false\)/);
  });
});
