// control_editor screenshot pathTraced (plugin Domains/ControlEditor/Viewport/McpPathTracedShot.cpp): the shot waits on
// the viewport's own sample count, answers inside the client's time limit and puts everything back before it answers.
// Behaviour belongs to the integration cases in tests/mcp-tools/core/control-editor.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const CONTROL_EDITOR = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlEditor');
/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (file: string): string =>
  readFileSync(join(CONTROL_EDITOR, file), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('path-traced screenshot', () => {
  const shot = code(join('Viewport', 'McpPathTracedShot.cpp'));

  it('counts samples on the viewport\'s view state, only where the engine has ray tracing', () => {
    expect(shot).toMatch(/#if RHI_RAYTRACING\s*const FSceneViewStateInterface\* State = Shot\.Client->ViewState\.GetReference\(\);\s*return State \? State->GetPathTracingSampleIndex\(\) : 0;/u);
  });

  it('answers inside the client\'s 30 s limit', () => {
    const deadline = /constexpr double DeadlineSeconds = ([0-9.]+);/u.exec(shot);
    expect(deadline).not.toBeNull();
    expect(Number(deadline?.[1])).toBeLessThan(25);
  });

  // A reply sent first left the viewport in Path Tracing view with the path tracer's settings changed.
  it('puts the view mode, settings, realtime and window back before it answers', () => {
    const capture = shot.slice(shot.indexOf('void Capture('), shot.indexOf('bool Tick('));
    expect(capture.indexOf('Restore(Shot);')).toBeGreaterThan(0);
    expect(capture.indexOf('Restore(Shot);')).toBeLessThan(capture.indexOf('SendScreenshotReceiptForMcp'));
    const restore = shot.slice(shot.indexOf('void Restore('), shot.indexOf('void Reply('));
    for (const step of ['Shot.Samples.Restore();', 'Shot.Denoiser.Restore();', 'Shot.Client->SetViewMode(Shot.OldMode);',
      'Shot.Client->SetGameView(Shot.bWasGameView);', 'Shot.Client->RemoveRealtimeOverride(OverrideText(), false);',
      'EndEditorRunForMcp(Shot.Hold);']) {
      expect(restore).toContain(step);
    }
  });

  // The view state keeps the last shot's count until the path tracer draws again: read first, it passed a moved
  // camera's 1-sample frame off as 64 samples, denoised.
  it('starts every shot on a fresh accumulation, drawn once in Path Tracing and game view before any count is read', () => {
    const handle = shot.slice(shot.indexOf('bool Handle('));
    const invalidate = handle.indexOf('Scene->InvalidatePathTracedOutput();');
    const draw = handle.indexOf('DrawViewportFramesForMcp(Client->Viewport, Client->GetWorld(), 1);');
    expect(invalidate).toBeGreaterThan(0);
    expect(handle.indexOf('Client->SetGameView(true);')).toBeLessThan(draw);
    expect(handle.indexOf('Client->SetViewMode(VMI_PathTracing);')).toBeLessThan(invalidate);
    expect(draw).toBeGreaterThan(invalidate);
    expect(handle.indexOf('FTSTicker::GetCoreTicker().AddTicker(')).toBeGreaterThan(draw);
  });

  it('is taken over by the screenshot handler before it reads the mode', () => {
    const handler = code('McpEditorScreenshot.cpp');
    const hook = handler.indexOf('if (McpPathTracedShot::Handle(this, RequestId, Payload, Socket)) { return true; }');
    expect(hook).toBeGreaterThan(0);
    expect(hook).toBeLessThan(handler.indexOf('Payload->TryGetStringField(TEXT("mode"), Mode);'));
  });
});
