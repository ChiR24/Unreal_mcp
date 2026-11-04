// Wiring contracts of control_editor screenshot renderSize that no unit test can reach by running it (it draws a
// viewport). Behaviour belongs to the integration cases in tests/mcp-tools/core/control-editor.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const EDITOR = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'ControlEditor');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(join(EDITOR, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('screenshot renderSize', () => {
  const render = (): string => read(join('Camera', 'McpViewRender.cpp'));

  it('refuses a renderSize it cannot honour before the capture, and reads every frame through one helper', () => {
    const handler = read('McpEditorScreenshot.cpp');
    expect(handler).toMatch(/if \(RefuseRenderSizeForMcp\(this, Socket, RequestId, Payload, Mode, bEjectedView\)\) \{ return true; \}/u);
    expect(handler).toContain('ReadViewFrameForMcp(Viewport, Payload, Bitmap, FrameSize, FrameCode, FrameError)');
    expect(handler).not.toContain('ReadPixels(');
  });

  it('puts back the engine state it borrows on every path', () => {
    // The high resolution screenshot state is the editor's own dialog's, and the automation flag changes what the
    // engine does on every later draw: a path that skipped the restore would leak both.
    const source = render();
    const exit = source.slice(source.indexOf('ON_SCOPE_EXIT'));
    for (const restored of [
      'Config.CaptureRegion = OldRegion;',
      'Config.UnscaledCaptureRegion = OldUnscaledRegion;',
      'Config.bMaskEnabled = bOldMask;',
      'Config.bCaptureHDR = bOldHdr;',
      'Config.bDumpBufferVisualizationTargets = bOldDump;',
      'Config.FilenameOverride = OldFilename;',
      'GScreenshotResolutionX = OldX;',
      'GScreenshotResolutionY = OldY;',
      'GIsHighResScreenshot = false;',
      'GIsAutomationTesting = bOldAutomation;',
      'FScreenshotRequest::OnScreenshotCaptured().Remove(Listener);',
      'IFileManager::Get().Delete(*Scratch, false, true, true);',
    ]) {
      expect(exit).toContain(restored);
    }
  });

  // The editor resets the viewport's field of view when it restarts: a picture framed like a game camera came out at
  // 90 degrees across. Every capture that places the camera does it through one function, which takes fov too.
  it('places the camera of every capture through one function that takes the field of view', () => {
    const place = read('McpEditorScreenshotWindows.cpp');
    expect(place).toMatch(/if \(Payload->TryGetNumberField\(TEXT\("fov"\), Fov\)\) \{\s*Client->ViewFOV = static_cast<float>\(FMath::Clamp\(Fov, 5\.0, 170\.0\)\);/u);
    expect(read('McpEditorScreenshot.cpp')).toContain('PlaceViewCameraForMcp(CaptureClient, Payload);');
    const inspection = join('..', 'Environment', 'Inspection');
    expect(read(join(inspection, 'McpEnvInspectViewPoints.cpp'))).toContain('const bool bMoved = PlaceViewCameraForMcp(Out.Client, Payload);');
    expect(read(join(inspection, 'McpEnvInspectCapturePasses.cpp'))).toContain('const bool bMoved = PlaceViewCameraForMcp(Client, Payload);');
    for (const file of ['McpEditorScreenshot.cpp', join(inspection, 'McpEnvInspectViewPoints.cpp'), join(inspection, 'McpEnvInspectCapturePasses.cpp')]) {
      expect(read(file), file).not.toContain('SetViewRotation(');
    }
  });

  it('draws the project thumbnail through the same placement and frame helpers, in game view, into the checked writer', () => {
    const thumbnail = read(join('..', 'Ui', 'McpUiProjectThumbnail.cpp'));
    expect(thumbnail).toMatch(/if \(GEditor->PlayWorld\)\s*\{\s*return Refuse\(TEXT\("PIE_RUNNING"\)/u);
    expect(thumbnail).toMatch(/PlaceViewCameraForMcp\(Client, Payload\);\s*const bool bWasGameView = Client->IsInGameView\(\);\s*Client->SetGameView\(true\);/u);
    expect(thumbnail).toMatch(/ReadViewFrameForMcp\(Viewport, Render, Pixels, Size, Code, Error\);\s*Client->SetGameView\(bWasGameView\);/u);
    expect(thumbnail).toContain('McpWriteProjectFile(Path,');
    expect(thumbnail).toContain('FString(FApp::GetProjectName()) + TEXT(".png")');
  });

  // camera is turned into location, rotation and fov (and renderSize) before anything reads them, so the Play refusal,
  // the occlusion settle and the placement see a camera given that way exactly as one given by place; a capture taken
  // again a beat later keeps the view the first pass took.
  it('takes a camera actor\'s view into the placement fields before the capture reads them', () => {
    const handler = read('McpEditorScreenshot.cpp');
    const take = handler.indexOf('if (RefuseOrTakeCameraViewForMcp(this, Socket, RequestId, Payload, Mode)) { return true; }');
    expect(take).toBeGreaterThan(handler.indexOf('Mode = TEXT("editor_viewport");'));
    expect(take).toBeLessThan(handler.indexOf('const bool bCameraPlaced ='));
    expect(take).toBeLessThan(handler.indexOf('RefuseRenderSizeForMcp('));
    const support = read('McpEditorViewportSupport.cpp');
    expect(support).toMatch(/Payload->HasField\(TEXT\("_cameraView"\)\)\) \{\s*return true;/u);
    expect(support).toContain('Camera->GetCameraView(0.f, View);');
    for (const field of ['location', 'rotation', 'fov', 'renderSize', '_cameraView']) {
      expect(support).toContain(`Payload->Set${field === 'fov' ? 'Number' : field === 'renderSize' ? 'String' : field === '_cameraView' ? 'Bool' : 'Object'}Field(TEXT("${field}")`);
    }
    // describe_view, analyze_view and audit_materials read the view through McpCaptureView, which takes the camera the
    // same way (refusals as codes, not replies) before it places the view, and reads a camera's shot in game view.
    const capture = read(join('..', 'Environment', 'Inspection', 'McpEnvInspectViewPoints.cpp'));
    const takeInCapture = capture.indexOf('if (!TakeCameraViewForMcp(Payload, true, CameraCode, OutError))');
    expect(takeInCapture).toBeGreaterThan(0);
    expect(takeInCapture).toBeLessThan(capture.indexOf('const bool bMoved = PlaceViewCameraForMcp(Out.Client, Payload);'));
    expect(capture).toContain('const bool bSwitch = (bGameView || Payload->HasField(TEXT("_cameraView"))) && !Out.Client->IsInGameView();');
    expect(support).toMatch(/if \(TakeCameraViewForMcp\(Payload, Mode == TEXT\("editor_viewport"\), Code, Error\)\) \{\s*return false;\s*\}\s*SendStandardErrorResponse/u);
    // The shot is what a player sees: game view while its frames are drawn and read, the viewport's own setting after.
    expect(handler.indexOf('const FMcpCameraGameViewScope GameView(CaptureClient, Payload);')).toBeLessThan(handler.indexOf('DrawViewportFramesForMcp('));
    expect(support).toMatch(/bWasGameView = Client->IsInGameView\(\);\s*Client->SetGameView\(true\);/u);
    expect(support).toMatch(/~FMcpCameraGameViewScope\(\) \{\s*if \(Client\) \{\s*Client->SetGameView\(bWasGameView\);/u);
  });

  it('sets the automation flag only around the one synchronous draw', () => {
    // UE 5.0 and 5.1 hand the frame to listeners only under it, and it keeps the "screenshot saved" toast away.
    const source = render();
    const set = source.indexOf('GIsAutomationTesting = true;');
    expect(set).toBeGreaterThan(source.indexOf('ON_SCOPE_EXIT'));
    expect(source.slice(set)).toMatch(/^GIsAutomationTesting = true;\s*if \(Viewport->TakeHighResScreenShot\(\)\)\s*\{\s*Viewport->Draw\(\);\s*\}\s*return bCaptured;/u);
  });
});
