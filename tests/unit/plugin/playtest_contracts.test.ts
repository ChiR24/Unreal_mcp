// control_editor playtest: drives a pawn of the running game and judges the leg (plugin Domains/Play). Read from the C++
// as text; the live cases are tests/mcp-tools/core/playtest.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const PLAY = join(PRIVATE, 'Domains', 'Play');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (path: string): string => readFileSync(path, 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('playtest', () => {
  const handler = code(join(PLAY, 'McpPlaytest.cpp'));
  const run = code(join(PLAY, 'McpPlaytestRun.cpp'));

  it('drives only a game this server started, never one the user may be playing', () => {
    const started = handler.indexOf('McpPlaySession::StartedByMcp(World)');
    const moved = handler.indexOf('SimpleMoveToLocation(');
    expect(started).toBeGreaterThan(-1);
    expect(started).toBeLessThan(moved);
    expect(code(join(PRIVATE, 'Domains', 'ControlEditor', 'McpEditorPlay.cpp'))).toContain('McpPlaySession::NoteStarted(');
  });

  it('tries no leg without a full nav path: that is no verdict on the game', () => {
    expect(handler).toMatch(/if \(Path\.Reason != TEXT\("ok"\)\)[\s\S]*?TEXT\("inconclusive"\)/u);
  });

  it('passes only with evidence: the goal reached, every check held, no script error', () => {
    expect(run).toMatch(/: bReached && !bCheckUnknown \? TEXT\("pass"\)/u);
    expect(run).toMatch(/bLost \|\| Errors\.Num\(\) > 0 \|\| bCheckFailed \? TEXT\("fail"\)/u);
  });

  it('gives the window back and stops the pawn however the leg ends', () => {
    expect(handler).toMatch(/Driver->StopMovement\(\);\s*\}\s*EndEditorRunForMcp\(Run->Hold\);/u);
  });

  // Found live: a leg in a game its title screen had paused sat out its time limit and answered inconclusive; and a level
  // change inside the session (the title screen reloading the level) made a new world, so a world-keyed session read as
  // the user's.
  it('drives no paused game, and knows its session by the game instance', () => {
    expect(handler).toMatch(/if \(World->IsPaused\(\)\)\s*\{\s*return Refuse\(TEXT\("GAME_PAUSED"\)/u);
    expect(run).toContain('GStarted.Get() == PlayWorld->GetGameInstance()');
  });

  // A move the AI controller refused read as "stuck" two seconds later, though nothing had been driven.
  it('refuses a leg whose move request failed, rather than calling the pawn stuck', () => {
    expect(handler).toMatch(/== EPathFollowingRequestResult::Failed\)\s*\{\s*return Refuse\(TEXT\("MOVE_FAILED"\)/u);
  });

  // Two legs at once would steer the same game: the second is refused until the first has answered.
  it('runs one leg at a time', () => {
    expect(handler).toMatch(/if \(GRunning\)\s*\{\s*return Refuse\(TEXT\("PLAYTEST_RUNNING"\)/u);
    expect(handler).toMatch(/EndEditorRunForMcp\(Run->Hold\);\s*GRunning = false;/u);
  });
});
