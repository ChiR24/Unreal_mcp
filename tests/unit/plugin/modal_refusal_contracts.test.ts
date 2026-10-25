// A call made while a modal dialog is up used to wait out the client's timeout on the native door: Slate's modal
// loop never ticks FTSTicker::GetCoreTicker(), which drives the queue drain. The modal-loop tick now answers every
// queued request EDITOR_MODAL_ACTIVE, naming the dialog, without running it. Wiring contracts only: automation runs
// cancel real modals, so the behaviour is covered by calling AnswerQueuedRequestsDuringModal in-editor.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sliceBetween } from './plugin-contract-fixtures.js';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('the modal-loop tick is bound for the subsystem lifetime', () => {
  const lifecycle = read('Core', 'Subsystem', 'EngineWireSubsystemLifecycle.cpp');

  it('binds OnModalLoopTick in Initialize, guarded by IsInitialized', () => {
    const init = sliceBetween(lifecycle, 'void UEngineWireSubsystem::Initialize(', 'void UEngineWireSubsystem::Deinitialize(');
    expect(init).toMatch(/if \(FSlateApplication::IsInitialized\(\)\)\s*\{\s*FSlateApplication::Get\(\)\.GetOnModalLoopTickEvent\(\)\.AddUObject\(this, &UEngineWireSubsystem::OnModalLoopTick\);/u);
  });

  it('removes it first in Deinitialize (Live Coding unloads the module)', () => {
    const deinit = sliceBetween(lifecycle, 'void UEngineWireSubsystem::Deinitialize(', 'bool UEngineWireSubsystem::Tick(');
    const removeAt = deinit.indexOf('GetOnModalLoopTickEvent().RemoveAll(this);');
    expect(removeAt).toBeGreaterThan(-1);
    expect(removeAt).toBeLessThan(deinit.indexOf('FMcpReadinessState::Get().Reset();'));
    expect(removeAt).toBeLessThan(deinit.indexOf('Super::Deinitialize();'));
  });
});

describe('queued requests are answered, never run, while a dialog is up', () => {
  const source = read('Core', 'Requests', 'McpModalRefusal.cpp');

  it('takes the queue under its lock and answers each request EDITOR_MODAL_ACTIVE on its own origin', () => {
    const answer = sliceBetween(source, 'int32 UEngineWireSubsystem::AnswerQueuedRequestsDuringModal(', 'void UEngineWireSubsystem::OnModalLoopTick(');
    expect(answer).toMatch(/FScopeLock Lock\(&PendingAutomationRequestsMutex\);\s*Taken = MoveTemp\(PendingAutomationRequests\);/u);
    expect(answer).toContain('Result->SetStringField(TEXT("modalWindowTitle"), DialogTitle);');
    expect(answer).toContain('Result, TEXT("EDITOR_MODAL_ACTIVE"), Request.Origin);');
    expect(answer, 'answered outside the lock').toMatch(/\}\s*for \(const FPendingAutomationRequest& Request : Taken\)/u);
  });

  it('acts only while a modal window is up and keeps the native heartbeat current', () => {
    const tick = sliceBetween(source, 'void UEngineWireSubsystem::OnModalLoopTick(', '\n}\n');
    expect(tick).toMatch(/GetActiveModalWindow\(\);\s*if \(!Modal\.IsValid\(\)\)\s*\{\s*return;/u);
    expect(tick).toContain('NativeTransport->CleanupStaleRequests();');
    expect(tick).toContain('AnswerQueuedRequestsDuringModal(Modal->GetTitle().ToString());');
  });

  it('never dispatches: no handler, drain or request processing is reached from the file', () => {
    for (const forbidden of ['ProcessAutomationRequest(', 'ProcessPendingAutomationRequests(', 'AutomationHandlers']) {
      expect(source, forbidden).not.toContain(forbidden);
    }
  });
});

describe('the pre-dispatch refusal names the dialog too', () => {
  it('replies EDITOR_MODAL_ACTIVE with modalWindowTitle', () => {
    const source = read('Core', 'Requests', 'McpProcessRequest.cpp');
    expect(source).toContain('ModalResult->SetStringField(TEXT("modalWindowTitle"), DialogTitle);');
    expect(source).toContain('ModalResult, TEXT("EDITOR_MODAL_ACTIVE"));');
  });
});
