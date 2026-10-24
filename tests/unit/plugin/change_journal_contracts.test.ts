// The change journal records what changed in the editor and who changed it; inspect changes_since reads it. Wiring and
// hot-path contracts only: the behaviour is covered by the inspect integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('change journal', () => {
  const recorder = code('Core', 'Journal', 'McpChangeJournal.cpp');
  const reader = code('Core', 'Journal', 'McpChangeJournalRead.cpp');

  it('listens for the editor session and brackets every request beside the change watch', () => {
    const lifecycle = code('Core', 'Subsystem', 'EngineWireSubsystemLifecycle.cpp');
    expect(lifecycle).toContain('McpChangeJournal::Start();');
    expect(lifecycle).toContain('McpChangeJournal::Stop();');
    const process = code('Core', 'Requests', 'McpProcessRequest.cpp');
    expect(process.indexOf('McpChangeJournal::BeginRequest(RequestId,')).toBeGreaterThan(process.indexOf('McpUndoLedger::Begin(RequestId);'));
    expect(process).toMatch(/McpUndoLedger::End\(\);\s*McpChangeJournal::EndRequest\(\);/u);
    expect(code('Core', 'Subsystem', 'EngineWireSubsystemResponses.cpp')).toMatch(
      /McpUndoLedger::Describe\(RequestId, EffectiveResult\);\s*McpChangeJournal::AddUserChangeWarning\(RequestId, EffectiveResult\);/u,
    );
    expect(code('Domains', 'Environment', 'Inspection', 'McpEnvInspect.cpp')).toMatch(
      /LowerSubAction\.Equals\(TEXT\("changes_since"\)\)\)\s*\{\s*return McpChangeJournal::HandleChangesSince\(\*this, RequestId, Payload, RequestingSocket\);/u,
    );
  });

  // OnObjectModified fires on every Modify, thousands of times in one Blueprint compile.
  it('rejects an edit on flags before any string work, and judges each package once', () => {
    const edited = recorder.slice(recorder.indexOf('int32 EditedEntry(UObject* Object)'), recorder.indexOf('FJournalState& JournalState()'));
    expect(edited.indexOf('RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject')).toBeGreaterThan(-1);
    expect(edited.indexOf('GIsReinstancing')).toBeLessThan(edited.indexOf('GetOutermost()'));
    const packageEntry = recorder.slice(recorder.indexOf('int32 EditedPackageEntry('), recorder.indexOf('int32 EditedEntry('));
    expect(packageEntry.indexOf('State.ByObject.Find(Key)')).toBeLessThan(packageEntry.indexOf('Package->GetName()'));
  });

  it('calls a change the server\'s inside a request, the user\'s inside a transaction it did not open, else the editor\'s', () => {
    expect(recorder).toMatch(/if \(!JournalState\(\)\.RequestId\.IsEmpty\(\)\)\s*\{\s*return JournalByMcp;\s*\}/u);
    expect(recorder).toContain('return (GUndo || GIsTransacting) ? JournalByUser : JournalByEditor;');
  });

  // A dozen meshes the engine touched at startup read as changed and buried every real change.
  it('keeps no edit the engine made on its own, and asks that before any lookup', () => {
    const edits = recorder.match(/CurrentBy\(\) == JournalByEditor \? INDEX_NONE : EditedEntry\(Object\)/gu) ?? [];
    expect(edits).toHaveLength(2);
    // Nor an actor it added or removed on its own: a new level's world settings, navigation data.
    const actors = recorder.match(/CurrentBy\(\) != JournalByEditor && !GIsReinstancing && IsLevelActor\(Actor\)/gu) ?? [];
    expect(actors).toHaveLength(2);
  });

  const events = code('Core', 'Journal', 'McpChangeJournalEvents.cpp');

  // Opening a level touched four of its actors and added its navigation data, all read as the request's changes.
  it('forgets the actors a level opened with, once the editor\'s last fix-up is done', () => {
    expect(events).toMatch(/void OnMapOpened\([^)]*\)\s*\{\s*ForgetActorsOf\(/u);
    expect(recorder).toContain('!Name.Contains(TEXT("/__External"))');
  });

  it('ignores the registry\'s startup scan and autosaves', () => {
    expect(events).toContain('Module->Get().IsLoadingAssets()');
    expect(events).toContain('(Context.GetSaveFlags() & SAVE_FromAutosave)');
  });

  it('warns a reply once, never inside a batch step', () => {
    expect(reader).toContain('FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId)');
    expect(reader).toContain('State.WarnedThrough = State.WarnUntil;');
  });

  // A row named the property edited, never what it held: an agent could not tell 5000 -> 8000 from a click.
  it('keeps a property\'s first old value from the announcement before an edit, and its latest after', () => {
    const values = code('Core', 'Journal', 'McpChangeJournalValues.cpp');
    expect(events).toMatch(/BindObjectEvents\(\);\s*BindValueEvents\(\);/u);
    expect(events).toMatch(/UnbindObjectEvents\(\);\s*UnbindValueEvents\(\);/u);
    expect(values).toContain('FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddStatic(&OnPreChange)');
    expect(values).toMatch(/if \(bBefore && !Value->bFrom && !Value->bTo\)/u);
    // GetOwnerClass walks past a function to its class: a local variable would be read off the object.
    expect(values).toContain('Member->GetOwner<UClass>()');
    expect(recorder).toContain('NoteValue(Index, Object, Member, false);');
  });

  it('lists where a live actor stood when first kept and where it stands now', () => {
    expect(recorder).toMatch(/Entry\.Start = Actor->GetActorTransform\(\);\s*Entry\.bStart = true;/u);
    expect(reader).toContain('AddTransform(Row, Entry.Start, Actor->GetActorTransform());');
  });
});
