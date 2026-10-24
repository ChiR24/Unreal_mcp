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

  it('is routed by manage_level', () => {
    expect(code(LEVEL, 'McpLevelHandlers.cpp')).toContain('{TEXT("run_map_check"), McpLevelHandlers::HandleRunMapCheckAction}');
  });

  it('runs the engine\'s Map Check without opening the Message Log, and reads that log back', () => {
    expect(check).toContain('GEditor->Exec(World, TEXT("MAP CHECK DONTDISPLAYDIALOG"), *GLog)');
    expect(check).toContain('GetLogListing(TEXT("MapCheck"))');
    expect(check).toContain('GetFilteredMessages()');
  });

  it('links MessageLog directly and never names the severity UE 5.1 deprecated', () => {
    expect(code(SOURCE, 'EngineWire.Build.cs')).toMatch(/"MessageLog"/u);
    expect(check).not.toContain('CriticalError');
  });
});
