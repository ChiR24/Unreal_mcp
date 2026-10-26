// control_editor explore_level: a pawn of the running game explores the level from an archive of cells (plugin
// Domains/Play). Read from the C++ as text; the live cases are tests/mcp-tools/core/explore-level.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const PLAY = join(PRIVATE, 'Domains', 'Play');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (path: string): string => readFileSync(path, 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('explore_level', () => {
  const handler = code(join(PLAY, 'McpExplore.cpp'));
  const run = code(join(PLAY, 'McpExploreRun.cpp'));

  it('is routed by control_editor', () => {
    expect(code(join(PRIVATE, 'Domains', 'ControlEditor', 'McpEditorDispatch.cpp'))).toMatch(
      /LowerSub == TEXT\("explore_level"\)\)\s*return McpExplore::Handle\(\*this, RequestId, Payload, RequestingSocket\);/u,
    );
  });

  it('drives only a game this server started, never one the user may be playing', () => {
    expect(handler.indexOf('McpPlaySession::StartedByMcp(World)')).toBeGreaterThan(-1);
    expect(handler.indexOf('McpPlaySession::StartedByMcp(World)')).toBeLessThan(handler.indexOf('AddTicker('));
  });

  // The cursor is the user's: the pawn moves through its own movement, never through OS or Slate input.
  it('moves the pawn through its own movement, never through OS input', () => {
    expect(run).toContain('Pawn->AddMovementInput(DirOf(S.Move.Dir), 1.0f);');
    expect(`${handler}\n${run}`).not.toMatch(/FSlateApplication|SetCursorPos|ProcessKeyDownEvent|SendInput/u);
  });

  it('keeps a move 19 frames in 20, as Go-Explore\'s exploration does', () => {
    expect(run).toContain('constexpr int32 RepeatOutOf = 20;');
    expect(run).toMatch(/S\.Trail\.Num\(\) == 0 \|\| Next\(S\) % RepeatOutOf == 0/u);
  });

  // Two calls at once would steer the same pawn; the window is given back however a call ends.
  it('runs one call at a time and gives the window back', () => {
    expect(handler).toMatch(/if \(GRunning\)\s*\{\s*return Refuse\(TEXT\("EXPLORE_RUNNING"\)/u);
    expect(handler).toMatch(/EndEditorRunForMcp\(Hold\);\s*GRunning = false;/u);
  });
});
