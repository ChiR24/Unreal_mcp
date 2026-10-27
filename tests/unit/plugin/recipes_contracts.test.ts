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

  // Search answers on the socket thread (McpNativeTransportGateway.cpp), so the recipes it lists are read from files
  // and JSON alone: no editor or engine header, no UObject lookup, no game-thread state.
  it('lists recipes in search from files and JSON only', () => {
    const scan = code(join(PRIVATE, 'Domains', 'Scripts', 'McpRecipesSearch.cpp'));
    const includes = [...scan.matchAll(/#include "([^"]+)"/gu)].map((match) => String(match[1]));
    expect(includes.sort()).toEqual(['Dom/JsonValue.h', 'Domains/Scripts/McpRecipes.h', 'HAL/FileManager.h',
      'MCP/Gateway/McpNativeGatewaySearchMatch.h', 'Misc/Paths.h']);
    expect(scan).not.toMatch(/\b(GEditor|GWorld|GEngine|FindObject|LoadObject|StaticLoadObject|AsyncTask)\b/u);
    const header = code(join(PRIVATE, 'Domains', 'Scripts', 'McpRecipes.h'));
    expect([...header.matchAll(/#include "([^"]+)"/gu)].map((match) => String(match[1])).sort())
      .toEqual(['CoreMinimal.h', 'Dom/JsonObject.h']);
    expect(code(join(PRIVATE, 'MCP', 'Gateway', 'McpNativeGatewaySearch.cpp'))).toContain('McpRecipes::SearchRows(ContentWords, 3)');
  });

  // Recipes are project data that list_recipes answers only for the read scope: search, which needs no scope for the
  // catalog, lists them only when the session's principal holds Read (or Admin). Write does not imply Read.
  it('lists recipes in search only for a caller that may read the project', () => {
    const transport = code(join(PRIVATE, 'MCP', 'Transport', 'McpNativeTransportGateway.cpp'));
    expect(transport).toContain('DiscoveryQuery.bMayReadProject = GetSessionPrincipal(SessionId).IsScopeAuthorized(EMcpCapabilityScope::Read);');
    expect(code(join(PRIVATE, 'MCP', 'Gateway', 'McpNativeGatewaySearch.cpp')))
      .toMatch(/Recipes = !Input\.bMayReadProject \|\| [^;]*McpRecipes::SearchRows/u);
  });
});
