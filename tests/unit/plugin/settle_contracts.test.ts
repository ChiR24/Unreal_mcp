// control_actor settle_physics lets actors fall in a Simulate session of its own and keeps their resting poses. Wiring
// and structure contracts only: the behaviour is covered by tests/mcp-tools/core/settle-physics.test.mjs.
// (tests/unit/plugin/settle_contracts.test.ts)

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const PLAY = join(PRIVATE, 'Domains', 'Play');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('settle_physics', () => {
  const handler = code(PLAY, 'McpSettle.cpp');
  const run = code(PLAY, 'McpSettleRun.cpp');

  it('is routed by control_actor with the exact-name lookup arrange_by_relations gets', () => {
    expect(code(PRIVATE, 'Domains', 'ControlActor', 'McpActorDispatch.cpp')).toMatch(
      /LowerSub == TEXT\("settle_physics"\)\)\s*return McpSettle::Handle\(\*this, RequestId, Payload, RequestingSocket,\s*\[this\]\(const FString &Name\) \{ return FindActorByName\(Name, true\); \}\);/u,
    );
  });

  // A Simulate session started beside a running Play, or beside another settle, would read or end the wrong session.
  it('refuses while any play session runs or is queued, and while another settle runs', () => {
    expect(handler).toContain('GEditor->IsPlaySessionInProgress() || GSettling');
  });

  // The user's own Play settings (net mode, client count) must survive: the editor copies the settings it is given and
  // Simulate rewrites the copy, so none are passed.
  it('passes no play settings of its own', () => {
    expect(handler).not.toMatch(/EditorPlaySettings/u);
  });

  // The edit lands seconds after the handler returned: outside the request's brackets the reply would carry no undo
  // block or placement checks and the journal would name the user.
  it('keeps the poses only inside the request\'s brackets, opened again', () => {
    expect(handler).toMatch(/RunLatentReply\(RequestId, TEXT\("control_actor settle_physics"\), \[[^\]]*\]\(\) \{\s*FString Code;\s*FString Message;\s*const int32 Kept = Keep\(Run, Code, Message\);/u);
    expect(handler.match(/Keep\(/gu)).toHaveLength(1);
    const latent = code(PRIVATE, 'Core', 'Requests', 'McpProcessRequest.cpp');
    const body = latent.slice(latent.indexOf('void UEngineWireSubsystem::RunLatentReply('));
    expect(body).toMatch(/TGuardValue<bool> UnattendedScriptGuard\(GIsRunningUnattendedScript, true\);\s*bProcessingAutomationRequest = true;\s*BeginErrorCapture\(\);\s*McpChangeWatch::Begin\(RequestId\);\s*McpUndoLedger::Begin\(RequestId\);\s*McpChangeJournal::BeginRequest\(RequestId, Action\);\s*Work\(\);\s*McpUndoLedger::Flush\(\);\s*EndErrorCapture\(\);\s*McpChangeWatch::End\(\);\s*McpUndoLedger::End\(\);\s*McpChangeJournal::EndRequest\(\);\s*bProcessingAutomationRequest = false;/u);
  });

  it('moves the editor actors in one undoable step, as arrange_by_relations does, and only those that did not fall', () => {
    expect(run).toMatch(/FMcpScopedEditorTransaction Transaction\([^;]*;\s*if \(!Transaction\.IsRecordingUndo\(\)\)\s*\{[^}]*return INDEX_NONE;/u);
    expect(run).toMatch(/Editor->Modify\(\);\s*Editor->SetActorTransform\(Body\.End, false, nullptr, ETeleportType::TeleportPhysics\);\s*McpFinishEditorMove\(Editor\);/u);
    expect(run).toContain('Body.bSimulated && !Body.bFell && Body.Editor.IsValid() && !Run.bDryRun');
  });

  it('never moves a floor, a locked or an attached actor: the placement fixer\'s own rule', () => {
    expect(handler).toContain('McpPlacementFix::PinnedReason(Actor)');
  });

  // Simulate left running, or still queued, after the call gave up would keep the editor simulating with no one to end it.
  it('ends or cancels its Simulate session on every way it gives up', () => {
    expect(handler).toMatch(/IsPlaySessionRequestQueued\(\)\)\s*\{\s*GEditor->CancelRequestPlaySession\(\);\s*\}\s*else if \(World\)\s*\{\s*GEditor->RequestEndPlayMap\(\);/u);
    expect(handler).toContain('EndEditorRunForMcp(Run.Hold);');
  });

  it('counts physics time, not game time: physics steps at most MaxPhysicsDeltaTime a frame', () => {
    expect(run).toContain('GetDefault<UPhysicsSettings>()->MaxPhysicsDeltaTime');
  });

  // A body is made static or dynamic from its mobility when it is created: a placed prop is Static, and its body would
  // never move however SetSimulatePhysics is set, so mobility comes first (it re-registers and makes the body again).
  it('makes the simulated copy movable before it simulates, and counts it simulated only then', () => {
    expect(run).toMatch(/Root->SetMobility\(EComponentMobility::Movable\);\s*Root->SetSimulatePhysics\(true\);/u);
    expect(run).toContain('return Root->Mobility == EComponentMobility::Movable && Root->IsSimulatingPhysics();');
  });
});
