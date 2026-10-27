// remove_sublevel drives the Levels panel's removal, which goes Fatal on a reference still held to the removed world
// when it resets the undo history itself, and opens a modal dialog for a locked level. Its guards can only be read here.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = readFileSync(
  join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Level', 'World', 'McpLevelRemoveSublevel.cpp'),
  'utf8',
).replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('remove_sublevel', () => {
  // An undo step that moved one of the level's actors holds it: with true that is a Fatal, with false the engine clears
  // the history only then (live: a moved actor's level removed, the editor kept running and said UNDO_HISTORY_CLEARED).
  it('never lets the engine reset the undo history first', () => {
    expect(code).toContain('UEditorLevelUtils::RemoveLevelFromWorld(Level, true, false)');
  });

  it('refuses a Level Instance entry, a locked level and unsaved work before it removes anything', () => {
    const remove = code.indexOf('UEditorLevelUtils::RemoveLevelFromWorld(');
    for (const guard of ['if (!IsUserSublevel(Streaming))', 'FLevelUtils::IsLevelLocked(Level)', 'TEXT("DIRTY_PACKAGES")']) {
      expect(code.indexOf(guard), guard).toBeGreaterThan(-1);
      expect(code.indexOf(guard), guard).toBeLessThan(remove);
    }
  });

  // RemoveInvalidLevelFromWorld asserts the level is not loaded.
  it('removes an entry whose level is not loaded only through the engine call made for that', () => {
    expect(code).toContain('const bool bRemoved = Level ? UEditorLevelUtils::RemoveLevelFromWorld(Level, true, false)');
    expect(code).toContain(': UEditorLevelUtils::RemoveInvalidLevelFromWorld(Streaming);');
  });
});
