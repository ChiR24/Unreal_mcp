// system_control save_recipe / run_recipe / list_recipes (plugin Domains/Scripts/McpRecipes). Read from the C++ as text;
// the live cases are tests/mcp-tools/core/recipes.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Line comments removed, CRLF normalized, so no assertion can be satisfied by prose. */
const code = (path: string): string => readFileSync(path, 'utf8').replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');

describe('recipes', () => {
  const recipes = code(join(PRIVATE, 'Domains', 'Scripts', 'McpRecipes.cpp'));

  it('is routed by system_control for its three actions', () => {
    const routes = code(join(PRIVATE, 'Domains', 'SystemControl', 'McpSystemHandlers.cpp'));
    for (const [action, handler] of [['save_recipe', 'HandleSave'], ['run_recipe', 'HandleRun'], ['list_recipes', 'HandleList']]) {
      expect(routes).toContain(`Lower == TEXT("${action}")) {\n    return McpRecipes::${handler}(`);
    }
  });

  // One implementation of a script run: the per-step gate, the consent list and the rollback are run_script's own.
  it('runs a recipe through run_script', () => {
    expect(recipes).toContain('McpScript::HandleRunScript(Bridge, RequestId, Run, Socket, &bRan);');
  });

  // A recipe's name is its file name: anything but letters, digits and underscores could leave the folder.
  it('keeps names to letters, digits and underscores', () => {
    expect(recipes).toMatch(/if \(!FChar::IsAlnum\(Char\) && Char != TEXT\('_'\)\)\s*\{\s*return false;/u);
  });

  // Nothing runs at save time: a restore point cannot undo a deletion and leaves the assets a step created.
  it('checks a recipe at save time without running it', () => {
    const save = recipes.slice(recipes.indexOf('bool HandleSave('), recipes.indexOf('bool HandleRun('));
    expect(save).toContain('McpScript::Check(Forms, Allowed, Error, Code)');
    expect(save).not.toContain('HandleRunScript');
  });
});
