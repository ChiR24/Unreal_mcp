// Todo 30-31 BB-062/BB-068/BB-036/BB-037
// Screenshot dimensions, viewport UMG capture, PIE diagnostics, and frame-step evidence.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

const uiScreenshot = () => readCpp('Domains/Ui/McpAutomationBridge_UiHandlersScreenshot.cpp');
const controlEditorScreenshot = () => readCpp('Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshot.cpp');
const inspectRuntime = () => readCpp('Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectRuntime.cpp');
const controlEditorPlay = () => readCpp('Domains/ControlEditor/McpAutomationBridge_ControlEditorPlay.cpp');

describe('BB-062 screenshot handlers read resolution from payload', () => {
  it('UiHandlersScreenshot.cpp reads resolution from payload via ResolveScreenshotResolutionForMcp', () => {
    const s = code(uiScreenshot());
    expect(s).toMatch(/ResolveScreenshotResolutionForMcp|TryGetNumberField.*(?:resolution|width|height)/i);
  });
  it('ControlEditorScreenshot.cpp reads resolution from payload via ResolveScreenshotResolutionForMcp', () => {
    const s = code(controlEditorScreenshot());
    expect(s).toMatch(/ResolveScreenshotResolutionForMcp|TryGetNumberField.*(?:resolution|width|height)/i);
  });
});

describe('BB-068 viewport UMG capture uses FScreenshotRequest', () => {
  it('UiHandlersScreenshot.cpp uses FScreenshotRequest or game_viewport capture path', () => {
    const s = code(uiScreenshot());
    expect(s).toMatch(/FScreenshotRequest|game_viewport|RequestScreenshot/i);
  });
});

describe('BB-036 PIE diagnostics emit canonical string identities', () => {
  it('InspectRuntime.cpp emits playerController as string (not object)', () => {
    const s = code(inspectRuntime());
    expect(s).toMatch(/SetStringField\s*\(\s*TEXT\s*\(\s*"playerController"\s*\)/);
    expect(s).not.toMatch(/SetObjectField\s*\(\s*TEXT\s*\(\s*"playerController"\s*\)/);
  });
  it('InspectRuntime.cpp emits pawn as string (not object)', () => {
    const s = code(inspectRuntime());
    expect(s).toMatch(/SetStringField\s*\(\s*TEXT\s*\(\s*"pawn"\s*\)/);
    expect(s).not.toMatch(/SetObjectField\s*\(\s*TEXT\s*\(\s*"pawn"\s*\)/);
  });
  it('InspectRuntime.cpp emits viewTarget as string (not object)', () => {
    const s = code(inspectRuntime());
    expect(s).toMatch(/SetStringField\s*\(\s*TEXT\s*\(\s*"viewTarget"\s*\)/);
    expect(s).not.toMatch(/SetObjectField\s*\(\s*TEXT\s*\(\s*"viewTarget"\s*\)/);
  });
  it('InspectRuntime.cpp emits playerCameraManager as string (not object)', () => {
    const s = code(inspectRuntime());
    expect(s).toMatch(/SetStringField\s*\(\s*TEXT\s*\(\s*"playerCameraManager"\s*\)/);
    expect(s).not.toMatch(/SetObjectField\s*\(\s*TEXT\s*\(\s*"playerCameraManager"\s*\)/);
  });
});

describe('BB-037 step_frame emits steps in response', () => {
  it('ControlEditorPlay.cpp HandleControlEditorStepFrame reads steps from payload', () => {
    const s = code(controlEditorPlay());
    const idx = s.indexOf('HandleControlEditorStepFrame');
    expect(idx).toBeGreaterThan(-1);
    const funcBody = s.slice(idx, idx + 3000);
    expect(funcBody).toMatch(/steps/i);
  });
  it('HandleControlEditorStepFrame emits steps in response object', () => {
    const s = code(controlEditorPlay());
    const idx = s.indexOf('HandleControlEditorStepFrame');
    expect(idx).toBeGreaterThan(-1);
    const funcBody = s.slice(idx, idx + 5000);
    expect(funcBody).toMatch(/SetNumberField.*steps/i);
  });
});
