// set_current_level makes a loaded level the editor's current one, where spawns land, as the Levels panel's Make Current
// does, and add_sublevel keeps the level that was current. Their guards and the engine calls they avoid are read here.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../support/capability-index.js';

const LEVEL = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'Level');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(LEVEL, ...segments), 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('set_current_level', () => {
  const handler = code('World', 'McpLevelSetCurrent.cpp');

  it('is routed by manage_level, its sub-action mapped onto the route table', () => {
    const handlers = code('McpLevelHandlers.cpp');
    expect(handlers).toContain('{TEXT("set_current_level"), McpLevelHandlers::HandleSetCurrentLevelAction}');
    expect(handlers).toMatch(/LowerSub == TEXT\("set_current_level"\)\) \{\s*EffectiveAction = TEXT\("set_current_level"\);/u);
  });

  // The Levels panel is read only during Play, a Level Instance's level keeps no actor spawned outside its edit session,
  // and the editor never makes a locked level current.
  it('refuses Play, a Level Instance\'s level, an unloaded level and a locked one before the current level changes', () => {
    const change = handler.indexOf('World->SetCurrentLevel(Level)');
    expect(change).toBeGreaterThan(-1);
    for (const guard of ['GEditor->PlayWorld', 'if (!IsUserSublevel(Matches[0]))', 'TEXT("LEVEL_NOT_LOADED")', 'FLevelUtils::IsLevelLocked(Level)']) {
      expect(handler.indexOf(guard), guard).toBeGreaterThan(-1);
      expect(handler.indexOf(guard), guard).toBeLessThan(change);
    }
  });

  // UEditorLevelUtils::MakeLevelCurrent shows the level even when it is visible, which marks the open level unsaved, and
  // answers a locked level with a modal dialog.
  it('makes the level current as the Levels panel does, showing it only when it was hidden', () => {
    expect(handler).not.toContain('MakeLevelCurrent(');
    expect(handler).toMatch(/if \(World->SetCurrentLevel\(Level\)\) \{\s*FEditorDelegates::NewCurrentLevel\.Broadcast\(\);/u);
    expect(handler).toMatch(/if \(bShow\) \{\s*UEditorLevelUtils::SetLevelVisibility\(Level, true, false\);/u);
  });

  // A fold takes its effect and policy from its first member, and the sublevel family's remove and move are destructive.
  it('is a write of its own that needs no consent', () => {
    const record = capabilityIndex().byId.get('manage_level.set_current_level');
    expect(record?.behavior.effect).toBe('write');
    expect(record?.policy.consent).toBe('none');
    expect(record?.legacyIds).toHaveLength(1);
    expect(record?.schemas.input.required).toContain('levelPath');
  });
});

describe('add_sublevel', () => {
  const add = code('World', 'McpLevelAddSublevel.cpp');

  // AddLevelToWorld makes the level it adds current, and spawns meant for the open level landed in the sublevel.
  it('puts the level that was current back after the engine adds the sublevel, and names it in the reply', () => {
    const before = add.indexOf('ULevel* const CurrentBefore = World->GetCurrentLevel();');
    const engine = add.indexOf('UEditorLevelUtils::AddLevelToWorld(');
    const restore = add.indexOf('World->SetCurrentLevel(CurrentBefore)');
    expect(before).toBeGreaterThan(-1);
    expect(engine).toBeGreaterThan(before);
    expect(restore).toBeGreaterThan(engine);
    expect(add.match(/SetStringField\(TEXT\("currentLevel"\), CurrentLevelPath\(\)\)/gu)).toHaveLength(2);
  });
});
