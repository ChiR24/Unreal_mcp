// manage_level run_map_check answers the editor's own Map Check as rows. Wiring contracts only: the behaviour is
// covered by the manage-level integration cases.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const SOURCE = join('plugins', 'EngineWire', 'Source', 'EngineWire');
const LEVEL = join(SOURCE, 'Private', 'Domains', 'Level');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('run_map_check', () => {
  const check = code(LEVEL, 'Lifecycle', 'McpLevelMapCheck.cpp');

  // The route table alone answered "Unknown manage_level action": the sub-action names above it map onto its keys.
  it('is routed by manage_level, its sub-action mapped onto the route table', () => {
    const handlers = code(LEVEL, 'McpLevelHandlers.cpp');
    expect(handlers).toContain('{TEXT("run_map_check"), McpLevelHandlers::HandleRunMapCheckAction}');
    expect(handlers).toMatch(/LowerSub == TEXT\("run_map_check"\)\) \{\s*EffectiveAction = TEXT\("run_map_check"\);/u);
  });

  it('runs the engine\'s Map Check without opening the Message Log, and reads that log back', () => {
    expect(check).toContain('GEditor->Exec(World, TEXT("MAP CHECK DONTDISPLAYDIALOG"), *GLog)');
    expect(check).toContain('GetLogListing(TEXT("MapCheck"))');
    expect(check).toContain('GetFilteredMessages()');
  });

  // A clean level answered one note: the check's own closing summary.
  it('leaves out the summary the check ends its page with, and counts what the tab\'s filters hide', () => {
    // Every message starts with a severity token, so the summary is that and its text: a one-token test never matched.
    expect(check).toMatch(/Messages\.Last\(\)->GetSeverity\(\) == EMessageSeverity::Info &&\s*!Messages\.Last\(\)->GetMessageTokens\(\)\.ContainsByPredicate\([\s\S]*?Token->GetType\(\) != EMessageToken::Severity && Token->GetType\(\) != EMessageToken::Text;[\s\S]*?--Listed;/u);
    expect(check).toContain('FMessageLog(TEXT("MapCheck")).NumMessages(EMessageSeverity::Info) - 1 - Total');
  });

  // A count named "warnings" was taken over by the engine lines the call logged (an array), and the Map Check's own lines
  // came back a second time there.
  it('names its counts apart from the reply\'s warnings and drops its own lines from them', () => {
    expect(check).toContain('TEXT("warningCount")');
    expect(check).not.toMatch(/TEXT\("warnings"\)/u);
    expect(check).toContain('Subsystem.ForgetCapturedMessages([](const FString& Line) { return Line.StartsWith(TEXT("[MapCheck]")); });');
  });

  it('links MessageLog directly and never names the severity UE 5.1 deprecated', () => {
    expect(code(SOURCE, 'EngineWire.Build.cs')).toMatch(/"MessageLog"/u);
    expect(check).not.toContain('CriticalError');
  });

  // The validator delta: a write that leaves an actor with a Map Check finding says so in its own reply.
  it('runs the engine\'s per-actor Map Check on the actors a call changed, and keeps only its findings', () => {
    const watch = code(SOURCE, 'Private', 'Domains', 'ControlActor', 'Placement', 'McpChangeWatch.cpp');
    expect(watch).toMatch(/return;\s*\}\s*Checked\.Add\(Actor\);/u);
    expect(watch).toMatch(/McpLevelHandlers::MapCheckActors\(Checked, Content\);\s*McpCoplanar::AddCoplanarToRows\(Rows\);/u);
    const delta = check.slice(check.indexOf('void MapCheckActors('));
    expect(delta).toContain('Actor->CheckForErrors();');
    expect(delta).toMatch(/if \(Message\.GetSeverity\(\) == EMessageSeverity::Info\)\s*\{\s*continue;/u);
    // Named by label, as calls address actors: the message's own object token is the internal name.
    expect(delta).toContain('FString::Printf(TEXT("\'%s\': %s: %s"), *Actor, *Kind, *Text.TrimStartAndEnd())');
  });
});
